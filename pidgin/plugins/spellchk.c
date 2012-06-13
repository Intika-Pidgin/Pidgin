/*
 * Purple - Replace certain misspelled words with their correct form.
 *
 * Signification changes were made by Benjamin Kahn ("xkahn") and
 * Richard Laager ("rlaager") in April 2005--you may want to contact
 * them if you have questions.
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

/*
 * A lot of this code (especially the config code) was taken directly
 * or nearly directly from xchat, version 1.4.2 by Peter Zelezny and others.
 */

#include "internal.h"
#include "pidgin.h"

#include "debug.h"
#include "notify.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtkutils.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SPELLCHECK_PLUGIN_ID "gtk-spellcheck"
#define SPELLCHK_OBJECT_KEY "spellchk"

enum {
	BAD_COLUMN,
	GOOD_COLUMN,
	WORD_ONLY_COLUMN,
	CASE_SENSITIVE_COLUMN,
	N_COLUMNS
};

struct _spellchk {
	GtkTextView *view;
	GtkTextMark *mark_insert_start;
	GtkTextMark *mark_insert_end;

	gchar *word;
	gboolean inserting;
	gboolean ignore_correction;
	gboolean ignore_correction_on_send;
	gint pos;
};

typedef struct _spellchk spellchk;

static GtkListStore *model;

static gboolean
is_word_uppercase(const gchar *word)
{
	for (; word[0] != '\0'; word = g_utf8_find_next_char (word, NULL)) {
		gunichar c = g_utf8_get_char(word);

		if (!(g_unichar_isupper(c) ||
		      g_unichar_ispunct(c) ||
		      g_unichar_isspace(c)))
			return FALSE;
	}

	return TRUE;
}

static gboolean
is_word_lowercase(const gchar *word)
{
	for (; word[0] != '\0'; word = g_utf8_find_next_char(word, NULL)) {
		gunichar c = g_utf8_get_char(word);

		if (!(g_unichar_islower(c) ||
		      g_unichar_ispunct(c) ||
		      g_unichar_isspace(c)))
			return FALSE;
	}

	return TRUE;
}

static gboolean
is_word_proper(const gchar *word)
{
	if (word[0] == '\0')
		return FALSE;

	if (!g_unichar_isupper(g_utf8_get_char_validated(word, -1)))
		return FALSE;

	return is_word_lowercase(g_utf8_offset_to_pointer(word, 1));
}

static gchar *
make_word_proper(const gchar *word)
{
	char buf[7];
	gchar *lower = g_utf8_strdown(word, -1);
	gint bytes;
	gchar *ret;

	bytes = g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(word)), buf);
	buf[MIN(bytes, sizeof(buf) - 1)] = '\0';

	ret = g_strconcat(buf, g_utf8_offset_to_pointer(lower, 1), NULL);
	g_free(lower);

	return ret;
}

static gboolean
substitute_simple_buffer(GtkTextBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;
	GtkTreeIter treeiter;
	gchar *text = NULL;

	gtk_text_buffer_get_iter_at_offset(buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset(buffer, &end, 0);
	gtk_text_iter_forward_to_end(&end);

	text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &treeiter) && text) {
		do {
			GValue val1;
			const gchar *bad;
			gchar *cursor;
			glong char_pos;

			val1.g_type = 0;
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &treeiter, WORD_ONLY_COLUMN, &val1);
			if (g_value_get_boolean(&val1))
			{
				g_value_unset(&val1);
				continue;
			}
			g_value_unset(&val1);

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &treeiter, BAD_COLUMN, &val1);
			bad = g_value_get_string(&val1);

			/* using g_utf8_* to get /character/ offsets instead of byte offsets for buffer */
			if ((cursor = g_strrstr(text, bad)))
			{
				GValue val2;
				const gchar *good;

				val2.g_type = 0;
				gtk_tree_model_get_value(GTK_TREE_MODEL(model), &treeiter, GOOD_COLUMN, &val2);
				good = g_value_get_string(&val2);

				char_pos = g_utf8_pointer_to_offset(text, cursor);
				gtk_text_buffer_get_iter_at_offset(buffer, &start, char_pos);
				gtk_text_buffer_get_iter_at_offset(buffer, &end, char_pos + g_utf8_strlen(bad, -1));
				gtk_text_buffer_delete(buffer, &start, &end);

				gtk_text_buffer_get_iter_at_offset(buffer, &start, char_pos);
				gtk_text_buffer_insert(buffer, &start, good, -1);

				g_value_unset(&val2);
				g_free(text);

				g_value_unset(&val1);
				return TRUE;
			}

			g_value_unset(&val1);
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &treeiter));
	}

	g_free(text);
	return FALSE;
}

static gchar *
substitute_word(gchar *word)
{
	GtkTreeIter iter;
	gchar *outword;
	gchar *lowerword;
	gchar *foldedword;

	if (word == NULL)
		return NULL;

	lowerword = g_utf8_strdown(word, -1);
	foldedword = g_utf8_casefold(word, -1);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
		do {
			GValue val1;
			gboolean case_sensitive;
			const char *bad;
			gchar *tmpbad = NULL;

			val1.g_type = 0;
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, WORD_ONLY_COLUMN, &val1);
			if (!g_value_get_boolean(&val1)) {
				g_value_unset(&val1);
				continue;
			}
			g_value_unset(&val1);

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CASE_SENSITIVE_COLUMN, &val1);
			case_sensitive = g_value_get_boolean(&val1);
			g_value_unset(&val1);

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, BAD_COLUMN, &val1);
			bad = g_value_get_string(&val1);

			if ((case_sensitive && !strcmp(bad, word)) ||
			    (!case_sensitive && (!strcmp(bad, lowerword) ||
			                        (!is_word_lowercase(bad) &&
			                         !strcmp((tmpbad = g_utf8_casefold(bad, -1)), foldedword)))))
			{
				GValue val2;
				const char *good;

				g_free(tmpbad);

				val2.g_type = 0;
				gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, GOOD_COLUMN, &val2);
				good = g_value_get_string(&val2);

				if (!case_sensitive && is_word_lowercase(bad) && is_word_lowercase(good))
				{
					if (is_word_uppercase(word))
						outword = g_utf8_strup(good, -1);
					else if (is_word_proper(word))
						outword = make_word_proper(good);
					else
						outword = g_strdup(good);
				}
				else
					outword = g_strdup(good);

				g_value_unset(&val1);
				g_value_unset(&val2);

				g_free(lowerword);
				g_free(foldedword);
				return outword;
			}

			g_value_unset(&val1);
			g_free(tmpbad);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
	}
	g_free(lowerword);
	g_free(foldedword);

	return NULL;
}

static void
spellchk_free(spellchk *spell)
{
	GtkTextBuffer *buffer;

	g_return_if_fail(spell != NULL);

	buffer = gtk_text_view_get_buffer(spell->view);

	g_signal_handlers_disconnect_matched(buffer,
			G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL,
			spell);
	g_free(spell->word);
	g_free(spell);
}

/* Pango doesn't know about the "'" character.  Let's fix that. */
static gboolean
spellchk_inside_word(GtkTextIter *iter)
{
	gunichar ucs4_char = gtk_text_iter_get_char(iter);
	gchar *utf8_str;
	gchar c = 0;

	utf8_str = g_ucs4_to_utf8(&ucs4_char, 1, NULL, NULL, NULL);
	if (utf8_str != NULL)
	{
		c = utf8_str[0];
		g_free(utf8_str);
	}

	/* Hack because otherwise typing things like U.S. gets difficult
	 * if you have 'u' -> 'you' set as a correction...
	 *
	 * Part 1 of 2: This marks . as being an inside-word character. */
	if (c == '.')
		return TRUE;

	/* Avoid problems with \r, for example (SF #1289031). */
	if (c == '\\')
		return TRUE;

	if (gtk_text_iter_inside_word (iter) == TRUE)
		return TRUE;

	if (c == '\'') {
		gboolean result = gtk_text_iter_backward_char(iter);
		gboolean output = gtk_text_iter_inside_word(iter);

		if (result)
		{
			/*
			 * Hack so that "u'll" will correct correctly.
			 */
			ucs4_char = gtk_text_iter_get_char(iter);
			utf8_str = g_ucs4_to_utf8(&ucs4_char, 1, NULL, NULL, NULL);
			if (utf8_str != NULL)
			{
				c = utf8_str[0];
				g_free(utf8_str);

				if (c == 'u' || c == 'U')
				{
					gtk_text_iter_forward_char(iter);
					return FALSE;
				}
			}

			gtk_text_iter_forward_char(iter);
		}

		return output;
	}
	else if (c == '&')
		return TRUE;

	return FALSE;
}

static gboolean
spellchk_backward_word_start(GtkTextIter *iter)
{
	int output;
	int result;

	output = gtk_text_iter_backward_word_start(iter);

	/* It didn't work...  */
	if (!output)
		return FALSE;

	while (spellchk_inside_word(iter)) {
		result = gtk_text_iter_backward_char(iter);

		/* We can't go backwards anymore?  We're at the beginning of the word. */
		if (!result)
			return TRUE;

		if (!spellchk_inside_word(iter)) {
			gtk_text_iter_forward_char(iter);
			return TRUE;
		}

		output = gtk_text_iter_backward_word_start(iter);
		if (!output)
			return FALSE;
	}

	return TRUE;
}

static gboolean
check_range(spellchk *spell, GtkTextBuffer *buffer,
				GtkTextIter start, GtkTextIter end, gboolean sending)
{
	gboolean replaced;
	gboolean result;
	gchar *tmp;
	int period_count = 0;
	gchar *word;
	GtkTextMark *mark;
	GtkTextIter pos;

	if ((replaced = substitute_simple_buffer(buffer)))
	{
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &pos, mark);
		spell->pos = gtk_text_iter_get_offset(&pos);

		gtk_text_buffer_get_iter_at_mark(buffer, &start, mark);
		gtk_text_buffer_get_iter_at_mark(buffer, &end, mark);
	}

	if (!sending)
	{
		/* We need to go backwords to find out if we are inside a word or not. */
		gtk_text_iter_backward_char(&end);

		if (spellchk_inside_word(&end))
		{
			gtk_text_iter_forward_char(&end);
			return replaced;  /* We only pay attention to whole words. */
		}
	}

	/* We could be in the middle of a whitespace block.  Check for that. */
	result = gtk_text_iter_backward_char(&end);

	if (!spellchk_inside_word(&end))
	{
		if (result)
			gtk_text_iter_forward_char(&end);
		return replaced;
	}

	if (result)
		gtk_text_iter_forward_char(&end);

	/* Move backwards to the beginning of the word. */
	spellchk_backward_word_start(&start);

	g_free(spell->word);
	spell->word = gtk_text_iter_get_text(&start, &end);

	/* Hack because otherwise typing things like U.S. gets difficult
	 * if you have 'u' -> 'you' set as a correction...
	 *
	 * Part 2 of 2: This chops periods off the end of the word so
	 * the right substitution entry is found. */
	tmp = g_strdup(spell->word);
	if (tmp != NULL && *tmp != '\0') {
		gchar *c;
		for (c = tmp + strlen(tmp) - 1 ; c != tmp ; c--) {
			if (*c == '.') {
				*c = '\0';
				period_count++;
			} else
				break;
		}
	}

	if ((word = substitute_word(tmp))) {
		GtkTextMark *mark;
		GtkTextIter pos;
		gchar *tmp2;
		int i;

		for (i = 1 ; i <= period_count ; i++) {
			tmp2 = g_strconcat(word, ".", NULL);
			g_free(word);
			word = tmp2;
		}

		gtk_text_buffer_delete(buffer, &start, &end);
		gtk_text_buffer_insert(buffer, &start, word, -1);

		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &pos, mark);
		spell->pos = gtk_text_iter_get_offset(&pos);

		g_free(word);
		g_free(tmp);
		return TRUE;
	}
	g_free(tmp);

	g_free(spell->word);
	spell->word = NULL;

	return replaced;
}

/* insertion works like this:
 *  - before the text is inserted, we mark the position in the buffer.
 *  - after the text is inserted, we see where our mark is and use that and
 *    the current position to check the entire range of inserted text.
 *
 * this may be overkill for the common case (inserting one character). */
static void
insert_text_before(GtkTextBuffer *buffer, GtkTextIter *iter,
					gchar *text, gint len, spellchk *spell)
{
	if (spell->inserting == TRUE)
		return;

	spell->inserting = TRUE;

	g_free(spell->word);
	spell->word = NULL;

	gtk_text_buffer_move_mark(buffer, spell->mark_insert_start, iter);
}

static void
insert_text_after(GtkTextBuffer *buffer, GtkTextIter *iter,
					gchar *text, gint len, spellchk *spell)
{
	GtkTextIter start, end;
	GtkTextMark *mark;

	spell->ignore_correction_on_send = FALSE;

	if (spell->ignore_correction) {
		spell->ignore_correction = FALSE;
		return;
	}

	/* we need to check a range of text. */
	gtk_text_buffer_get_iter_at_mark(buffer, &start, spell->mark_insert_start);

	if (len == 1)
		check_range(spell, buffer, start, *iter, FALSE);

	/* if check_range modified the buffer, iter has been invalidated */
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &end, mark);
	gtk_text_buffer_move_mark(buffer, spell->mark_insert_end, &end);

	spell->inserting = FALSE;
}

static void
delete_range_after(GtkTextBuffer *buffer,
					GtkTextIter *start, GtkTextIter *end, spellchk *spell)
{
	GtkTextIter start2, end2;
	GtkTextMark *mark;
	GtkTextIter pos;
	gint place;

	spell->ignore_correction_on_send = FALSE;

	if (!spell->word)
		return;

	if (spell->inserting == TRUE)
		return;

	spell->inserting = TRUE;

	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &pos, mark);
	place = gtk_text_iter_get_offset(&pos);

	if ((place + 1) != spell->pos) {
		g_free(spell->word);
		spell->word = NULL;
		return;
	}

	gtk_text_buffer_get_iter_at_mark(buffer, &start2, spell->mark_insert_start);
	gtk_text_buffer_get_iter_at_mark(buffer, &end2, spell->mark_insert_end);

	gtk_text_buffer_delete(buffer, &start2, &end2);
	gtk_text_buffer_insert(buffer, &start2, spell->word, -1);
	spell->ignore_correction = TRUE;
	spell->ignore_correction_on_send = TRUE;

	spell->inserting = FALSE;
	g_free(spell->word);
	spell->word = NULL;
}

static void
message_send_cb(GtkWidget *widget, spellchk *spell)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	GtkTextMark *mark;
	gboolean replaced;

	if (spell->ignore_correction_on_send)
	{
		spell->ignore_correction_on_send = FALSE;
		return;
	}

#if 0
	if (!purple_prefs_get_bool("/plugins/gtk/spellchk/last_word_replace"))
		return;
#endif

	buffer = gtk_text_view_get_buffer(spell->view);

	gtk_text_buffer_get_end_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	spell->inserting = TRUE;
	replaced = check_range(spell, buffer, start, end, TRUE);
	spell->inserting = FALSE;

	/* if check_range modified the buffer, iter has been invalidated */
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &end, mark);
	gtk_text_buffer_move_mark(buffer, spell->mark_insert_end, &end);

	if (replaced)
	{
		g_signal_stop_emission_by_name(widget, "message_send");
		spell->ignore_correction_on_send = TRUE;
	}
}

static void
spellchk_new_attach(PurpleConversation *conv)
{
	spellchk *spell;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	PidginConversation *gtkconv;
	GtkTextView *view;

	gtkconv = PIDGIN_CONVERSATION(conv);

	view = GTK_TEXT_VIEW(gtkconv->entry);

	spell = g_object_get_data(G_OBJECT(view), SPELLCHK_OBJECT_KEY);
	if (spell != NULL)
		return;

	/* attach to the widget */
	spell = g_new0(spellchk, 1);
	spell->view = view;

	g_object_set_data_full(G_OBJECT(view), SPELLCHK_OBJECT_KEY, spell,
			(GDestroyNotify)spellchk_free);

	buffer = gtk_text_view_get_buffer(view);

	/* we create the mark here, but we don't use it until text is
	 * inserted, so we don't really care where iter points.  */
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	spell->mark_insert_start = gtk_text_buffer_create_mark(buffer,
			"spellchk-insert-start",
			&start, TRUE);
	spell->mark_insert_end = gtk_text_buffer_create_mark(buffer,
			"spellchk-insert-end",
			&start, TRUE);

	g_signal_connect_after(G_OBJECT(buffer),
			"delete-range",
			G_CALLBACK(delete_range_after), spell);
	g_signal_connect(G_OBJECT(buffer),
			"insert-text",
			G_CALLBACK(insert_text_before), spell);
	g_signal_connect_after(G_OBJECT(buffer),
			"insert-text",
			G_CALLBACK(insert_text_after), spell);

	g_signal_connect(G_OBJECT(gtkconv->entry), "message_send",
	                 G_CALLBACK(message_send_cb), spell);
	return;
}

static int buf_get_line(char *ibuf, char **buf, int *position, gsize len)
{
	int pos = *position;
	int spos = pos;

	if (pos == len)
		return 0;

	while (!(ibuf[pos] == '\n' ||
	         (ibuf[pos] == '\r' && ibuf[pos + 1] != '\n')))
	{
		pos++;
		if (pos == len)
			return 0;
	}

	if (pos != 0 && ibuf[pos] == '\n' && ibuf[pos - 1] == '\r')
		ibuf[pos - 1] = '\0';

	ibuf[pos] = '\0';
	*buf = &ibuf[spos];

	pos++;
	*position = pos;

	return 1;
}

static void load_conf(void)
{
	/* Corrections to change "...", "(c)", "(r)", and "(tm)" to their
	 * Unicode character equivalents were not added here even though
	 * they existed in the source list(s). I think these corrections
	 * would be more trouble than they're worth.
	 */
	const char * const defaultconf =
			"BAD abbout\nGOOD about\n"
			"BAD abotu\nGOOD about\n"
			"BAD abouta\nGOOD about a\n"
			"BAD aboutit\nGOOD about it\n"
			"BAD aboutthe\nGOOD about the\n"
			"BAD abscence\nGOOD absence\n"
			"BAD accesories\nGOOD accessories\n"
			"BAD accidant\nGOOD accident\n"
			"BAD accomodate\nGOOD accommodate\n"
			"BAD accordingto\nGOOD according to\n"
			"BAD accross\nGOOD across\n"
			"BAD acheive\nGOOD achieve\n"
			"BAD acheived\nGOOD achieved\n"
			"BAD acheiving\nGOOD achieving\n"
			"BAD acn\nGOOD can\n"
			"BAD acommodate\nGOOD accommodate\n"
			"BAD acomodate\nGOOD accommodate\n"
			"BAD actualyl\nGOOD actually\n"
			"BAD additinal\nGOOD additional\n"
			"BAD addtional\nGOOD additional\n"
			"BAD adequit\nGOOD adequate\n"
			"BAD adequite\nGOOD adequate\n"
			"BAD adn\nGOOD and\n"
			"BAD advanage\nGOOD advantage\n"
			"BAD affraid\nGOOD afraid\n"
			"BAD afterthe\nGOOD after the\n"
			"COMPLETE 0\nBAD againstt he \nGOOD against the \n"
			"BAD aganist\nGOOD against\n"
			"BAD aggresive\nGOOD aggressive\n"
			"BAD agian\nGOOD again\n"
			"BAD agreemeent\nGOOD agreement\n"
			"BAD agreemeents\nGOOD agreements\n"
			"BAD agreemnet\nGOOD agreement\n"
			"BAD agreemnets\nGOOD agreements\n"
			"BAD agressive\nGOOD aggressive\n"
			"BAD agressiveness\nGOOD aggressiveness\n"
			"BAD ahd\nGOOD had\n"
			"BAD ahold\nGOOD a hold\n"
			"BAD ahppen\nGOOD happen\n"
			"BAD ahve\nGOOD have\n"
			"BAD allready\nGOOD already\n"
			"BAD allwasy\nGOOD always\n"
			"BAD allwyas\nGOOD always\n"
			"BAD almots\nGOOD almost\n"
			"BAD almsot\nGOOD almost\n"
			"BAD alomst\nGOOD almost\n"
			"BAD alot\nGOOD a lot\n"
			"BAD alraedy\nGOOD already\n"
			"BAD alreayd\nGOOD already\n"
			"BAD alreday\nGOOD already\n"
			"BAD alwasy\nGOOD always\n"
			"BAD alwats\nGOOD always\n"
			"BAD alway\nGOOD always\n"
			"BAD alwyas\nGOOD always\n"
			"BAD amde\nGOOD made\n"
			"BAD Ameria\nGOOD America\n"
			"BAD amke\nGOOD make\n"
			"BAD amkes\nGOOD makes\n"
			"BAD anbd\nGOOD and\n"
			"BAD andone\nGOOD and one\n"
			"BAD andteh\nGOOD and the\n"
			"BAD andthe\nGOOD and the\n"
			"COMPLETE 0\nBAD andt he \nGOOD and the \n"
			"BAD anothe\nGOOD another\n"
			"BAD anual\nGOOD annual\n"
			"BAD any1\nGOOD anyone\n"
			"BAD apparant\nGOOD apparent\n"
			"BAD apparrent\nGOOD apparent\n"
			"BAD appearence\nGOOD appearance\n"
			"BAD appeares\nGOOD appears\n"
			"BAD applicaiton\nGOOD application\n"
			"BAD applicaitons\nGOOD applications\n"
			"BAD applyed\nGOOD applied\n"
			"BAD appointiment\nGOOD appointment\n"
			"BAD approrpiate\nGOOD appropriate\n"
			"BAD approrpriate\nGOOD appropriate\n"
			"BAD aquisition\nGOOD acquisition\n"
			"BAD aquisitions\nGOOD acquisitions\n"
			"BAD arent\nGOOD aren't\n"
			"COMPLETE 0\nBAD aren;t \nGOOD aren't \n"
			"BAD arguement\nGOOD argument\n"
			"BAD arguements\nGOOD arguments\n"
			"COMPLETE 0\nBAD arn't \nGOOD aren't \n"
			"BAD arond\nGOOD around\n"
			"BAD artical\nGOOD article\n"
			"BAD articel\nGOOD article\n"
			"BAD asdvertising\nGOOD advertising\n"
			"COMPLETE 0\nBAD askt he \nGOOD ask the \n"
			"BAD assistent\nGOOD assistant\n"
			"BAD asthe\nGOOD as the\n"
			"BAD atention\nGOOD attention\n"
			"BAD atmospher\nGOOD atmosphere\n"
			"BAD attentioin\nGOOD attention\n"
			"BAD atthe\nGOOD at the\n"
			"BAD audeince\nGOOD audience\n"
			"BAD audiance\nGOOD audience\n"
			"BAD authentification\nGOOD authentication\n"
			"BAD availalbe\nGOOD available\n"
			"BAD awya\nGOOD away\n"
			"BAD aywa\nGOOD away\n"
			"BAD b4\nGOOD before\n"
			"BAD bakc\nGOOD back\n"
			"BAD balence\nGOOD balance\n"
			"BAD ballance\nGOOD balance\n"
			"BAD baout\nGOOD about\n"
			"BAD bcak\nGOOD back\n"
			"BAD bcuz\nGOOD because\n"
			"BAD beacuse\nGOOD because\n"
			"BAD becasue\nGOOD because\n"
			"BAD becaus\nGOOD because\n"
			"BAD becausea\nGOOD because a\n"
			"BAD becauseof\nGOOD because of\n"
			"BAD becausethe\nGOOD because the\n"
			"BAD becauseyou\nGOOD because you\n"
			"COMPLETE 0\nBAD beckon call\nGOOD beck and call\n"
			"BAD becomeing\nGOOD becoming\n"
			"BAD becomming\nGOOD becoming\n"
			"BAD becuase\nGOOD because\n"
			"BAD becuse\nGOOD because\n"
			"BAD befoer\nGOOD before\n"
			"BAD beggining\nGOOD beginning\n"
			"BAD begining\nGOOD beginning\n"
			"BAD beginining\nGOOD beginning\n"
			"BAD beleiev\nGOOD believe\n"
			"BAD beleieve\nGOOD believe\n"
			"BAD beleif\nGOOD belief\n"
			"BAD beleive\nGOOD believe\n"
			"BAD beleived\nGOOD believed\n"
			"BAD beleives\nGOOD believes\n"
			"BAD belive\nGOOD believe\n"
			"BAD belived\nGOOD believed\n"
			"BAD belives\nGOOD believes\n"
			"BAD benifit\nGOOD benefit\n"
			"BAD benifits\nGOOD benefits\n"
			"BAD betwen\nGOOD between\n"
			"BAD beutiful\nGOOD beautiful\n"
			"BAD blase\nGOOD blas\303\251\n"
			"BAD boxs\nGOOD boxes\n"
			"BAD brodcast\nGOOD broadcast\n"
			"BAD butthe\nGOOD but the\n"
			"BAD bve\nGOOD be\n"
			"COMPLETE 0\nBAD byt he \nGOOD by the \n"
			"BAD cafe\nGOOD caf\303\251\n"
			"BAD caharcter\nGOOD character\n"
			"BAD calcullated\nGOOD calculated\n"
			"BAD calulated\nGOOD calculated\n"
			"BAD candidtae\nGOOD candidate\n"
			"BAD candidtaes\nGOOD candidates\n"
			"COMPLETE 0\nBAD case and point\nGOOD case in point\n"
			"BAD cant\nGOOD can't\n"
			"COMPLETE 0\nBAD can;t \nGOOD can't \n"
			"COMPLETE 0\nBAD can't of been\nGOOD can't have been\n"
			"BAD catagory\nGOOD category\n"
			"BAD categiory\nGOOD category\n"
			"BAD certian\nGOOD certain\n"
			"BAD challange\nGOOD challenge\n"
			"BAD challanges\nGOOD challenges\n"
			"BAD chaneg\nGOOD change\n"
			"BAD chanegs\nGOOD changes\n"
			"BAD changable\nGOOD changeable\n"
			"BAD changeing\nGOOD changing\n"
			"BAD changng\nGOOD changing\n"
			"BAD charachter\nGOOD character\n"
			"BAD charachters\nGOOD characters\n"
			"BAD charactor\nGOOD character\n"
			"BAD charecter\nGOOD character\n"
			"BAD charector\nGOOD character\n"
			"BAD cheif\nGOOD chief\n"
			"BAD chekc\nGOOD check\n"
			"BAD chnage\nGOOD change\n"
			"BAD cieling\nGOOD ceiling\n"
			"BAD circut\nGOOD circuit\n"
			"BAD claer\nGOOD clear\n"
			"BAD claered\nGOOD cleared\n"
			"BAD claerly\nGOOD clearly\n"
			"BAD cliant\nGOOD client\n"
			"BAD cliche\nGOOD clich\303\251\n"
			"BAD cna\nGOOD can\n"
			"BAD colection\nGOOD collection\n"
			"BAD comanies\nGOOD companies\n"
			"BAD comany\nGOOD company\n"
			"BAD comapnies\nGOOD companies\n"
			"BAD comapny\nGOOD company\n"
			"BAD combintation\nGOOD combination\n"
			"BAD comited\nGOOD committed\n"
			"BAD comittee\nGOOD committee\n"
			"BAD commadn\nGOOD command\n"
			"BAD comming\nGOOD coming\n"
			"BAD commitee\nGOOD committee\n"
			"BAD committe\nGOOD committee\n"
			"BAD committment\nGOOD commitment\n"
			"BAD committments\nGOOD commitments\n"
			"BAD committy\nGOOD committee\n"
			"BAD comntain\nGOOD contain\n"
			"BAD comntains\nGOOD contains\n"
			"BAD compair\nGOOD compare\n"
			"COMPLETE 0\nBAD company;s \nGOOD company's \n"
			"BAD competetive\nGOOD competitive\n"
			"BAD compleated\nGOOD completed\n"
			"BAD compleatly\nGOOD completely\n"
			"BAD compleatness\nGOOD completeness\n"
			"BAD completly\nGOOD completely\n"
			"BAD completness\nGOOD completeness\n"
			"BAD composate\nGOOD composite\n"
			"BAD comtain\nGOOD contain\n"
			"BAD comtains\nGOOD contains\n"
			"BAD comunicate\nGOOD communicate\n"
			"BAD comunity\nGOOD community\n"
			"BAD condolances\nGOOD condolences\n"
			"BAD conected\nGOOD connected\n"
			"BAD conferance\nGOOD conference\n"
			"BAD confirmmation\nGOOD confirmation\n"
			"BAD congradulations\nGOOD congratulations\n"
			"BAD considerit\nGOOD considerate\n"
			"BAD considerite\nGOOD considerate\n"
			"BAD consonent\nGOOD consonant\n"
			"BAD conspiricy\nGOOD conspiracy\n"
			"BAD consultent\nGOOD consultant\n"
			"BAD convertable\nGOOD convertible\n"
			"BAD cooparate\nGOOD cooperate\n"
			"BAD cooporate\nGOOD cooperate\n"
			"BAD corproation\nGOOD corporation\n"
			"BAD corproations\nGOOD corporations\n"
			"BAD corruptable\nGOOD corruptible\n"
			"BAD cotten\nGOOD cotton\n"
			"BAD coudl\nGOOD could\n"
			"COMPLETE 0\nBAD coudln't \nGOOD couldn't \n"
			"COMPLETE 0\nBAD coudn't \nGOOD couldn't \n"
			"BAD couldnt\nGOOD couldn't\n"
			"COMPLETE 0\nBAD couldn;t \nGOOD couldn't \n"
			"COMPLETE 0\nBAD could of been\nGOOD could have been\n"
			"COMPLETE 0\nBAD could of had\nGOOD could have had\n"
			"BAD couldthe\nGOOD could the\n"
			"BAD couldve\nGOOD could've\n"
			"BAD cpoy\nGOOD copy\n"
			"BAD creme\nGOOD cr\303\250me\n"
			"BAD ctaegory\nGOOD category\n"
			"BAD cu\nGOOD see you\n"
			"BAD cusotmer\nGOOD customer\n"
			"BAD cusotmers\nGOOD customers\n"
			"BAD cutsomer\nGOOD customer\n"
			"BAD cutsomers\nGOOD customer\n"
			"BAD cuz\nGOOD because\n"
			"BAD cxan\nGOOD can\n"
			"BAD danceing\nGOOD dancing\n"
			"BAD dcument\nGOOD document\n"
			"BAD deatils\nGOOD details\n"
			"BAD decison\nGOOD decision\n"
			"BAD decisons\nGOOD decisions\n"
			"BAD decor\nGOOD d\303\251cor\n"
			"BAD defendent\nGOOD defendant\n"
			"BAD definately\nGOOD definitely\n"
			"COMPLETE 0\nBAD deja vu\nGOOD d\303\251j\303\240 vu\n"
			"BAD deptartment\nGOOD department\n"
			"BAD desicion\nGOOD decision\n"
			"BAD desicions\nGOOD decisions\n"
			"BAD desision\nGOOD decision\n"
			"BAD desisions\nGOOD decisions\n"
			"BAD detente\nGOOD d\303\251tente\n"
			"BAD develeoprs\nGOOD developers\n"
			"BAD devellop\nGOOD develop\n"
			"BAD develloped\nGOOD developed\n"
			"BAD develloper\nGOOD developer\n"
			"BAD devellopers\nGOOD developers\n"
			"BAD develloping\nGOOD developing\n"
			"BAD devellopment\nGOOD development\n"
			"BAD devellopments\nGOOD developments\n"
			"BAD devellops\nGOOD develop\n"
			"BAD develope\nGOOD develop\n"
			"BAD developement\nGOOD development\n"
			"BAD developements\nGOOD developments\n"
			"BAD developor\nGOOD developer\n"
			"BAD developors\nGOOD developers\n"
			"BAD develpment\nGOOD development\n"
			"BAD diaplay\nGOOD display\n"
			"BAD didint\nGOOD didn't\n"
			"BAD didnot\nGOOD did not\n"
			"BAD didnt\nGOOD didn't\n"
			"COMPLETE 0\nBAD didn;t \nGOOD didn't \n"
			"BAD difefrent\nGOOD different\n"
			"BAD diferences\nGOOD differences\n"
			"BAD differance\nGOOD difference\n"
			"BAD differances\nGOOD differences\n"
			"BAD differant\nGOOD different\n"
			"BAD differemt\nGOOD different\n"
			"BAD differnt\nGOOD different\n"
			"BAD diffrent\nGOOD different\n"
			"BAD directer\nGOOD director\n"
			"BAD directers\nGOOD directors\n"
			"BAD directiosn\nGOOD direction\n"
			"BAD disatisfied\nGOOD dissatisfied\n"
			"BAD discoverd\nGOOD discovered\n"
			"BAD disign\nGOOD design\n"
			"BAD dispaly\nGOOD display\n"
			"BAD dissonent\nGOOD dissonant\n"
			"BAD distribusion\nGOOD distribution\n"
			"BAD divsion\nGOOD division\n"
			"BAD docuement\nGOOD documents\n"
			"BAD docuemnt\nGOOD document\n"
			"BAD documetn\nGOOD document\n"
			"BAD documnet\nGOOD document\n"
			"BAD documnets\nGOOD documents\n"
			"COMPLETE 0\nBAD doens't \nGOOD doesn't \n"
			"BAD doese\nGOOD does\n"
			"COMPLETE 0\nBAD doe snot \nGOOD does not \n"
			"BAD doesnt\nGOOD doesn't\n"
			"COMPLETE 0\nBAD doesn;t \nGOOD doesn't \n"
			"BAD doign\nGOOD doing\n"
			"BAD doimg\nGOOD doing\n"
			"BAD doind\nGOOD doing\n"
			"BAD dollers\nGOOD dollars\n"
			"BAD donig\nGOOD doing\n"
			"BAD donno\nGOOD don't know\n"
			"BAD dont\nGOOD don't\n"
			"COMPLETE 0\nBAD do'nt \nGOOD don't \n"
			"COMPLETE 0\nBAD don;t \nGOOD don't \n"
			"COMPLETE 0\nBAD don't no \nGOOD don't know \n"
			"COMPLETE 0\nBAD dosn't \nGOOD doesn't \n"
			"BAD driveing\nGOOD driving\n"
			"BAD drnik\nGOOD drink\n"
			"BAD dunno\nGOOD don't know\n"
			"BAD eclair\nGOOD \303\251clair\n"
			"BAD efel\nGOOD feel\n"
			"BAD effecient\nGOOD efficient\n"
			"BAD efort\nGOOD effort\n"
			"BAD eforts\nGOOD efforts\n"
			"BAD ehr\nGOOD her\n"
			"BAD eligable\nGOOD eligible\n"
			"BAD embarass\nGOOD embarrass\n"
			"BAD emigre\nGOOD \303\251migr\303\251\n"
			"BAD enought\nGOOD enough\n"
			"BAD entree\nGOOD entr\303\251e\n"
			"BAD enuf\nGOOD enough\n"
			"BAD equippment\nGOOD equipment\n"
			"BAD equivalant\nGOOD equivalent\n"
			"BAD esle\nGOOD else\n"
			"BAD especally\nGOOD especially\n"
			"BAD especialyl\nGOOD especially\n"
			"BAD espesially\nGOOD especially\n"
			"BAD essense\nGOOD essence\n"
			"BAD excellance\nGOOD excellence\n"
			"BAD excellant\nGOOD excellent\n"
			"BAD excercise\nGOOD exercise\n"
			"BAD exchagne\nGOOD exchange\n"
			"BAD exchagnes\nGOOD exchanges\n"
			"BAD excitment\nGOOD excitement\n"
			"BAD exhcange\nGOOD exchange\n"
			"BAD exhcanges\nGOOD exchanges\n"
			"BAD experiance\nGOOD experience\n"
			"BAD experienc\nGOOD experience\n"
			"BAD exprience\nGOOD experience\n"
			"BAD exprienced\nGOOD experienced\n"
			"BAD eyt\nGOOD yet\n"
			"BAD facade\nGOOD fa\303\247ade\n"
			"BAD faeture\nGOOD feature\n"
			"BAD faetures\nGOOD feature\n"
			"BAD familair\nGOOD familiar\n"
			"BAD familar\nGOOD familiar\n"
			"BAD familliar\nGOOD familiar\n"
			"BAD fammiliar\nGOOD familiar\n"
			"BAD feild\nGOOD field\n"
			"BAD feilds\nGOOD fields\n"
			"BAD fianlly\nGOOD finally\n"
			"BAD fidn\nGOOD find\n"
			"BAD finalyl\nGOOD finally\n"
			"BAD firends\nGOOD friends\n"
			"BAD firts\nGOOD first\n"
			"BAD follwo\nGOOD follow\n"
			"BAD follwoing\nGOOD following\n"
			"BAD fora\nGOOD for a\n"
			"COMPLETE 0\nBAD for all intensive purposes\nfor all intents and purposes\n"
			"BAD foriegn\nGOOD foreign\n"
			"BAD forthe\nGOOD for the\n"
			"BAD forwrd\nGOOD forward\n"
			"BAD forwrds\nGOOD forwards\n"
			"BAD foudn\nGOOD found\n"
			"BAD foward\nGOOD forward\n"
			"BAD fowards\nGOOD forwards\n"
			"BAD freind\nGOOD friend\n"
			"BAD freindly\nGOOD friendly\n"
			"BAD freinds\nGOOD friends\n"
			"BAD friday\nGOOD Friday\n"
			"BAD frmo\nGOOD from\n"
			"BAD fromthe\nGOOD from the\n"
			"COMPLETE 0\nBAD fromt he \nGOOD from the \n"
			"BAD furneral\nGOOD funeral\n"
			"BAD fwe\nGOOD few\n"
			"BAD garantee\nGOOD guarantee\n"
			"BAD gaurd\nGOOD guard\n"
			"BAD gemeral\nGOOD general\n"
			"BAD gerat\nGOOD great\n"
			"BAD geting\nGOOD getting\n"
			"BAD gettin\nGOOD getting\n"
			"BAD gievn\nGOOD given\n"
			"BAD giveing\nGOOD giving\n"
			"BAD gloabl\nGOOD global\n"
			"BAD goign\nGOOD going\n"
			"BAD gonig\nGOOD going\n"
			"BAD govenment\nGOOD government\n"
			"BAD goverment\nGOOD government\n"
			"BAD gruop\nGOOD group\n"
			"BAD gruops\nGOOD groups\n"
			"BAD grwo\nGOOD grow\n"
			"BAD guidlines\nGOOD guidelines\n"
			"BAD hadbeen\nGOOD had been\n"
			"BAD hadnt\nGOOD hadn't\n"
			"COMPLETE 0\nBAD hadn;t \nGOOD hadn't \n"
			"BAD haev\nGOOD have\n"
			"BAD hapen\nGOOD happen\n"
			"BAD hapened\nGOOD happened\n"
			"BAD hapening\nGOOD happening\n"
			"BAD hapens\nGOOD happens\n"
			"BAD happend\nGOOD happened\n"
			"BAD hasbeen\nGOOD has been\n"
			"BAD hasnt\nGOOD hasn't\n"
			"COMPLETE 0\nBAD hasn;t \nGOOD hasn't \n"
			"BAD havebeen\nGOOD have been\n"
			"BAD haveing\nGOOD having\n"
			"BAD havent\nGOOD haven't\n"
			"COMPLETE 0\nBAD haven;t \nGOOD haven't \n"
			"BAD hda\nGOOD had\n"
			"BAD hearign\nGOOD hearing\n"
			"COMPLETE 0\nBAD he;d \nGOOD he'd \n"
			"BAD heirarchy\nGOOD hierarchy\n"
			"BAD hel\nGOOD he'll\n"
			"COMPLETE 0\nBAD he;ll \nGOOD he'll \n"
			"BAD helpfull\nGOOD helpful\n"
			"BAD herat\nGOOD heart\n"
			"BAD heres\nGOOD here's\n"
			"COMPLETE 0\nBAD here;s \nGOOD here's \n"
			"BAD hes\nGOOD he's\n"
			"COMPLETE 0\nBAD he;s \nGOOD he's \n"
			"BAD hesaid\nGOOD he said\n"
			"BAD hewas\nGOOD he was\n"
			"BAD hge\nGOOD he\n"
			"BAD hismelf\nGOOD himself\n"
			"BAD hlep\nGOOD help\n"
			"BAD hott\nGOOD hot\n"
			"BAD hows\nGOOD how's\n"
			"BAD hsa\nGOOD has\n"
			"BAD hse\nGOOD she\n"
			"BAD hsi\nGOOD his\n"
			"BAD hte\nGOOD the\n"
			"BAD htere\nGOOD there\n"
			"BAD htese\nGOOD these\n"
			"BAD htey\nGOOD they\n"
			"BAD hting\nGOOD thing\n"
			"BAD htink\nGOOD think\n"
			"BAD htis\nGOOD this\n"
			"COMPLETE 0\nBAD htp:\nGOOD http:\n"
			"COMPLETE 0\nBAD http:\\\\nGOOD http://\n"
			"BAD httpL\nGOOD http:\n"
			"BAD hvae\nGOOD have\n"
			"BAD hvaing\nGOOD having\n"
			"BAD hwich\nGOOD which\n"
			"BAD i\nGOOD I\n"
			"COMPLETE 0\nBAD i c \nGOOD I see \n"
			"COMPLETE 0\nBAD i;d \nGOOD I'd \n"
			"COMPLETE 0\nBAD i'd \nGOOD I'd \n"
			"COMPLETE 0\nBAD I;d \nGOOD I'd \n"
			"BAD idae\nGOOD idea\n"
			"BAD idaes\nGOOD ideas\n"
			"BAD identofy\nGOOD identify\n"
			"BAD ihs\nGOOD his\n"
			"BAD iits the\nGOOD it's the\n"
			"COMPLETE 0\nBAD i'll \nGOOD I'll \n"
			"COMPLETE 0\nBAD I;ll \nGOOD I'll \n"
			"COMPLETE 0\nBAD i;m \nGOOD I'm \n"
			"COMPLETE 0\nBAD i'm \nGOOD I'm \n"
			"COMPLETE 0\nBAD I\"m \nGOOD I'm \n"
			"BAD imediate\nGOOD immediate\n"
			"BAD imediatly\nGOOD immediately\n"
			"BAD immediatly\nGOOD immediately\n"
			"BAD importent\nGOOD important\n"
			"BAD importnat\nGOOD important\n"
			"BAD impossable\nGOOD impossible\n"
			"BAD improvemnt\nGOOD improvement\n"
			"BAD improvment\nGOOD improvement\n"
			"BAD includ\nGOOD include\n"
			"BAD indecate\nGOOD indicate\n"
			"BAD indenpendence\nGOOD independence\n"
			"BAD indenpendent\nGOOD independent\n"
			"BAD indepedent\nGOOD independent\n"
			"BAD independance\nGOOD independence\n"
			"BAD independant\nGOOD independent\n"
			"BAD influance\nGOOD influence\n"
			"BAD infomation\nGOOD information\n"
			"BAD informatoin\nGOOD information\n"
			"BAD inital\nGOOD initial\n"
			"BAD instaleld\nGOOD installed\n"
			"BAD insted\nGOOD instead\n"
			"BAD insurence\nGOOD insurance\n"
			"BAD inteh\nGOOD in the\n"
			"BAD interum\nGOOD interim\n"
			"BAD inthe\nGOOD in the\n"
			"COMPLETE 0\nBAD int he \nGOOD in the \n"
			"BAD inturn\nGOOD intern\n"
			"BAD inwhich\nGOOD in which\n"
			"COMPLETE 0\nBAD i snot \nGOOD is not \n"
			"BAD isnt\nGOOD isn't\n"
			"COMPLETE 0\nBAD isn;t \nGOOD isn't \n"
			"BAD isthe\nGOOD is the\n"
			"BAD itd\nGOOD it'd\n"
			"COMPLETE 0\nBAD it;d \nGOOD it'd \n"
			"BAD itis\nGOOD it is\n"
			"BAD ititial\nGOOD initial\n"
			"BAD itll\nGOOD it'll\n"
			"COMPLETE 0\nBAD it;ll \nGOOD it'll \n"
			"BAD itnerest\nGOOD interest\n"
			"BAD itnerested\nGOOD interested\n"
			"BAD itneresting\nGOOD interesting\n"
			"BAD itnerests\nGOOD interests\n"
			"COMPLETE 0\nBAD it;s \nGOOD it's \n"
			"BAD itsa\nGOOD it's a\n"
			"COMPLETE 0\nBAD  its a \nGOOD  it's a \n"
			"COMPLETE 0\nBAD  it snot \nGOOD  it's not \n"
			"COMPLETE 0\nBAD  it' snot \nGOOD  it's not \n"
			"COMPLETE 0\nBAD  its the \nGOOD  it's the \n"
			"BAD itwas\nGOOD it was\n"
			"BAD ive\nGOOD I've\n"
			"COMPLETE 0\nBAD i;ve \nGOOD I've \n"
			"COMPLETE 0\nBAD i've \nGOOD I've \n"
			"BAD iwll\nGOOD will\n"
			"BAD iwth\nGOOD with\n"
			"BAD jsut\nGOOD just\n"
			"BAD jugment\nGOOD judgment\n"
			"BAD kno\nGOOD know\n"
			"BAD knowldge\nGOOD knowledge\n"
			"BAD knowlege\nGOOD knowledge\n"
			"BAD knwo\nGOOD know\n"
			"BAD knwon\nGOOD known\n"
			"BAD knwos\nGOOD knows\n"
			"BAD konw\nGOOD know\n"
			"BAD konwn\nGOOD known\n"
			"BAD konws\nGOOD knows\n"
			"BAD labratory\nGOOD laboratory\n"
			"BAD labtop\nGOOD laptop\n"
			"BAD lastyear\nGOOD last year\n"
			"BAD laterz\nGOOD later\n"
			"BAD learnign\nGOOD learning\n"
			"BAD lenght\nGOOD length\n"
			"COMPLETE 0\nBAD let;s \nGOOD let's \n"
			"COMPLETE 0\nBAD let's him \nGOOD lets him \n"
			"COMPLETE 0\nBAD let's it \nGOOD lets it \n"
			"BAD levle\nGOOD level\n"
			"BAD libary\nGOOD library\n"
			"BAD librarry\nGOOD library\n"
			"BAD librery\nGOOD library\n"
			"BAD liek\nGOOD like\n"
			"BAD liekd\nGOOD liked\n"
			"BAD lieutenent\nGOOD lieutenant\n"
			"BAD liev\nGOOD live\n"
			"BAD likly\nGOOD likely\n"
			"BAD lisense\nGOOD license\n"
			"BAD littel\nGOOD little\n"
			"BAD litttle\nGOOD little\n"
			"BAD liuke\nGOOD like\n"
			"BAD liveing\nGOOD living\n"
			"BAD loev\nGOOD love\n"
			"BAD lonly\nGOOD lonely\n"
			"BAD lookign\nGOOD looking\n"
			"BAD m\nGOOD am\n"
			"BAD maintainence\nGOOD maintenance\n"
			"BAD maintenence\nGOOD maintenance\n"
			"BAD makeing\nGOOD making\n"
			"BAD managment\nGOOD management\n"
			"BAD mantain\nGOOD maintain\n"
			"BAD marraige\nGOOD marriage\n"
			"COMPLETE 0\nBAD may of been\nGOOD may have been\n"
			"COMPLETE 0\nBAD may of had\nGOOD may have had\n"
			"BAD memeber\nGOOD member\n"
			"BAD merchent\nGOOD merchant\n"
			"BAD mesage\nGOOD message\n"
			"BAD mesages\nGOOD messages\n"
			"COMPLETE 0\nBAD might of been\nGOOD might have been\n"
			"COMPLETE 0\nBAD might of had\nGOOD might have had\n"
			"BAD mispell\nGOOD misspell\n"
			"BAD mispelling\nGOOD misspelling\n"
			"BAD mispellings\nGOOD misspellings\n"
			"BAD mkae\nGOOD make\n"
			"BAD mkaes\nGOOD makes\n"
			"BAD mkaing\nGOOD making\n"
			"BAD moeny\nGOOD money\n"
			"BAD monday\nGOOD Monday\n"
			"BAD morgage\nGOOD mortgage\n"
			"BAD mroe\nGOOD more\n"
			"COMPLETE 0\nBAD must of been\nGOOD must have been\n"
			"COMPLETE 0\nBAD must of had\nGOOD must have had\n"
			"COMPLETE 0\nBAD mute point\nGOOD moot point\n"
			"BAD mysefl\nGOOD myself\n"
			"BAD myu\nGOOD my\n"
			"BAD naive\nGOOD naïve\n"
			"BAD ne1\nGOOD anyone\n"
			"BAD neway\nGOOD anyway\n"
			"BAD neways\nGOOD anyways\n"
			"BAD necassarily\nGOOD necessarily\n"
			"BAD necassary\nGOOD necessary\n"
			"BAD neccessarily\nGOOD necessarily\n"
			"BAD neccessary\nGOOD necessary\n"
			"BAD necesarily\nGOOD necessarily\n"
			"BAD necesary\nGOOD necessary\n"
			"BAD negotiaing\nGOOD negotiating\n"
			"BAD nkow\nGOOD know\n"
			"BAD nothign\nGOOD nothing\n"
			"BAD nto\nGOOD not\n"
			"BAD nver\nGOOD never\n"
			"BAD nwe\nGOOD new\n"
			"BAD nwo\nGOOD now\n"
			"BAD obediant\nGOOD obedient\n"
			"BAD ocasion\nGOOD occasion\n"
			"BAD occassion\nGOOD occasion\n"
			"BAD occurance\nGOOD occurrence\n"
			"BAD occured\nGOOD occurred\n"
			"BAD occurence\nGOOD occurrence\n"
			"BAD occurrance\nGOOD occurrence\n"
			"BAD oclock\nGOOD o'clock\n"
			"BAD oculd\nGOOD could\n"
			"BAD ocur\nGOOD occur\n"
			"BAD oeprator\nGOOD operator\n"
			"BAD ofits\nGOOD of its\n"
			"BAD ofthe\nGOOD of the\n"
			"BAD oft he\nGOOD of the\n"
			"BAD oging\nGOOD going\n"
			"BAD ohter\nGOOD other\n"
			"BAD omre\nGOOD more\n"
			"BAD oneof\nGOOD one of\n"
			"BAD onepoint\nGOOD one point\n"
			"BAD onthe\nGOOD on the\n"
			"COMPLETE 0\nBAD ont he \nGOOD on the \n"
			"BAD onyl\nGOOD only\n"
			"BAD oppasite\nGOOD opposite\n"
			"BAD opperation\nGOOD operation\n"
			"BAD oppertunity\nGOOD opportunity\n"
			"BAD opposate\nGOOD opposite\n"
			"BAD opposible\nGOOD opposable\n"
			"BAD opposit\nGOOD opposite\n"
			"BAD oppotunities\nGOOD opportunities\n"
			"BAD oppotunity\nGOOD opportunity\n"
			"BAD orginization\nGOOD organization\n"
			"BAD orginized\nGOOD organized\n"
			"BAD otehr\nGOOD other\n"
			"BAD otu\nGOOD out\n"
			"BAD outof\nGOOD out of\n"
			"BAD overthe\nGOOD over the\n"
			"BAD owrk\nGOOD work\n"
			"BAD owuld\nGOOD would\n"
			"BAD oxident\nGOOD oxidant\n"
			"BAD papaer\nGOOD paper\n"
			"BAD passe\nGOOD pass\303\251\n"
			"BAD parliment\nGOOD parliament\n"
			"BAD partof\nGOOD part of\n"
			"BAD paymetn\nGOOD payment\n"
			"BAD paymetns\nGOOD payments\n"
			"BAD pciture\nGOOD picture\n"
			"BAD peice\nGOOD piece\n"
			"BAD peices\nGOOD pieces\n"
			"BAD peolpe\nGOOD people\n"
			"BAD peopel\nGOOD people\n"
			"BAD percentof\nGOOD percent of\n"
			"BAD percentto\nGOOD percent to\n"
			"BAD performence\nGOOD performance\n"
			"BAD perhasp\nGOOD perhaps\n"
			"BAD perhpas\nGOOD perhaps\n"
			"BAD permanant\nGOOD permanent\n"
			"BAD perminent\nGOOD permanent\n"
			"BAD personalyl\nGOOD personally\n"
			"BAD pleasent\nGOOD pleasant\n"
			"BAD pls\nGOOD please\n"
			"BAD plz\nGOOD please\n"
			"BAD poeple\nGOOD people\n"
			"BAD porblem\nGOOD problem\n"
			"BAD porblems\nGOOD problems\n"
			"BAD porvide\nGOOD provide\n"
			"BAD possable\nGOOD possible\n"
			"BAD postition\nGOOD position\n"
			"BAD potatoe\nGOOD potato\n"
			"BAD potatos\nGOOD potatoes\n"
			"BAD potentialy\nGOOD potentially\n"
			"BAD ppl\nGOOD people\n"
			"BAD pregnent\nGOOD pregnant\n"
			"BAD presance\nGOOD presence\n"
			"BAD primative\nGOOD primitive\n"
			"BAD probally\nGOOD probably\n"
			"BAD probelm\nGOOD problem\n"
			"BAD probelms\nGOOD problems\n"
			"BAD probly\nGOOD probably\n"
			"BAD prolly\nGOOD probably\n"
			"BAD proly\nGOOD probably\n"
			"BAD prominant\nGOOD prominent\n"
			"BAD proposterous\nGOOD preposterous\n"
			"BAD protege\nGOOD prot\303\251g\303\251\n"
			"BAD protoge\nGOOD prot\303\251g\303\251\n"
			"BAD psoition\nGOOD position\n"
			"BAD ptogress\nGOOD progress\n"
			"BAD pursuade\nGOOD persuade\n"
			"BAD puting\nGOOD putting\n"
			"BAD pwoer\nGOOD power\n"
			"BAD quater\nGOOD quarter\n"
			"BAD quaters\nGOOD quarters\n"
			"BAD quesion\nGOOD question\n"
			"BAD quesions\nGOOD questions\n"
			"BAD questioms\nGOOD questions\n"
			"BAD questiosn\nGOOD questions\n"
			"BAD questoin\nGOOD question\n"
			"BAD quetion\nGOOD question\n"
			"BAD quetions\nGOOD questions\n"
			"BAD r\nGOOD are\n"
			"BAD raeson\nGOOD reason\n"
			"BAD realyl\nGOOD really\n"
			"BAD reccomend\nGOOD recommend\n"
			"BAD reccommend\nGOOD recommend\n"
			"BAD receieve\nGOOD receive\n"
			"BAD recieve\nGOOD receive\n"
			"BAD recieved\nGOOD received\n"
			"BAD recieving\nGOOD receiving\n"
			"BAD recomend\nGOOD recommend\n"
			"BAD recomendation\nGOOD recommendation\n"
			"BAD recomendations\nGOOD recommendations\n"
			"BAD recomended\nGOOD recommended\n"
			"BAD reconize\nGOOD recognize\n"
			"BAD recrod\nGOOD record\n"
			"BAD rediculous\nGOOD ridiculous\n"
			"BAD rediculus\nGOOD ridiculous\n"
			"BAD reguard\nGOOD regard\n"
			"BAD religous\nGOOD religious\n"
			"BAD reluctent\nGOOD reluctant\n"
			"BAD remeber\nGOOD remember\n"
			"BAD reommend\nGOOD recommend\n"
			"BAD representativs\nGOOD representatives\n"
			"BAD representives\nGOOD representatives\n"
			"BAD represetned\nGOOD represented\n"
			"BAD represnt\nGOOD represent\n"
			"BAD reserach\nGOOD research\n"
			"BAD resollution\nGOOD resolution\n"
			"BAD resorces\nGOOD resources\n"
			"BAD respomd\nGOOD respond\n"
			"BAD respomse\nGOOD response\n"
			"BAD responce\nGOOD response\n"
			"BAD responsability\nGOOD responsibility\n"
			"BAD responsable\nGOOD responsible\n"
			"BAD responsibile\nGOOD responsible\n"
			"BAD responsiblity\nGOOD responsibility\n"
			"BAD restaraunt\nGOOD restaurant\n"
			"BAD restuarant\nGOOD restaurant\n"
			"BAD reult\nGOOD result\n"
			"BAD reveiw\nGOOD review\n"
			"BAD reveiwing\nGOOD reviewing\n"
			"BAD rumers\nGOOD rumors\n"
			"BAD rwite\nGOOD write\n"
			"BAD rythm\nGOOD rhythm\n"
			"BAD saidhe\nGOOD said he\n"
			"BAD saidit\nGOOD said it\n"
			"BAD saidthat\nGOOD said that\n"
			"BAD saidthe\nGOOD said the\n"
			"COMPLETE 0\nBAD saidt he \nGOOD said the \n"
			"BAD sandwhich\nGOOD sandwich\n"
			"BAD sandwitch\nGOOD sandwich\n"
			"BAD saturday\nGOOD Saturday\n"
			"BAD scedule\nGOOD schedule\n"
			"BAD sceduled\nGOOD scheduled\n"
			"BAD seance\nGOOD s\303\251ance\n"
			"BAD secratary\nGOOD secretary\n"
			"BAD sectino\nGOOD section\n"
			"BAD seh\nGOOD she\n"
			"BAD selectoin\nGOOD selection\n"
			"BAD sence\nGOOD sense\n"
			"BAD sentance\nGOOD sentence\n"
			"BAD separeate\nGOOD separate\n"
			"BAD seperate\nGOOD separate\n"
			"BAD sercumstances\nGOOD circumstances\n"
			"BAD shcool\nGOOD school\n"
			"COMPLETE 0\nBAD she;d \nGOOD she'd \n"
			"COMPLETE 0\nBAD she;ll \nGOOD she'll \n"
			"BAD shes\nGOOD she's\n"
			"COMPLETE 0\nBAD she;s \nGOOD she's \n"
			"BAD shesaid\nGOOD she said\n"
			"BAD shineing\nGOOD shining\n"
			"BAD shiped\nGOOD shipped\n"
			"BAD shoudl\nGOOD should\n"
			"COMPLETE 0\nBAD shoudln't \nGOOD shouldn't \n"
			"BAD shouldent\nGOOD shouldn't\n"
			"BAD shouldnt\nGOOD shouldn't\n"
			"COMPLETE 0\nBAD shouldn;t \nGOOD shouldn't \n"
			"COMPLETE 0\nBAD should of been\nGOOD should have been\n"
			"COMPLETE 0\nBAD should of had\nGOOD should have had\n"
			"BAD shouldve\nGOOD should've\n"
			"BAD showinf\nGOOD showing\n"
			"BAD signifacnt\nGOOD significant\n"
			"BAD simalar\nGOOD similar\n"
			"BAD similiar\nGOOD similar\n"
			"BAD simpyl\nGOOD simply\n"
			"BAD sincerly\nGOOD sincerely\n"
			"BAD sitll\nGOOD still\n"
			"BAD smae\nGOOD same\n"
			"BAD smoe\nGOOD some\n"
			"BAD soem\nGOOD some\n"
			"BAD sohw\nGOOD show\n"
			"BAD soical\nGOOD social\n"
			"BAD some1\nGOOD someone\n"
			"BAD somethign\nGOOD something\n"
			"BAD someting\nGOOD something\n"
			"BAD somewaht\nGOOD somewhat\n"
			"BAD somthing\nGOOD something\n"
			"BAD somtimes\nGOOD sometimes\n"
			"COMPLETE 0\nBAD sot hat \nGOOD so that \n"
			"BAD soudn\nGOOD sound\n"
			"BAD soudns\nGOOD sounds\n"
			"BAD speach\nGOOD speech\n"
			"BAD specificaly\nGOOD specifically\n"
			"BAD specificalyl\nGOOD specifically\n"
			"BAD spelt\nGOOD spelled\n"
			"BAD sry\nGOOD sorry\n"
			"COMPLETE 0\nBAD state of the ark\nGOOD state of the art\n"
			"BAD statment\nGOOD statement\n"
			"BAD statments\nGOOD statements\n"
			"BAD stnad\nGOOD stand\n"
			"BAD stopry\nGOOD story\n"
			"BAD stoyr\nGOOD story\n"
			"BAD stpo\nGOOD stop\n"
			"BAD strentgh\nGOOD strength\n"
			"BAD stroy\nGOOD story\n"
			"BAD struggel\nGOOD struggle\n"
			"BAD strugle\nGOOD struggle\n"
			"BAD studnet\nGOOD student\n"
			"BAD successfull\nGOOD successful\n"
			"BAD successfuly\nGOOD successfully\n"
			"BAD successfulyl\nGOOD successfully\n"
			"BAD sucess\nGOOD success\n"
			"BAD sucessfull\nGOOD successful\n"
			"BAD sufficiant\nGOOD sufficient\n"
			"BAD sum1\nGOOD someone\n"
			"BAD sunday\nGOOD Sunday\n"
			"BAD suposed\nGOOD supposed\n"
			"BAD supposably\nGOOD supposedly\n"
			"BAD suppossed\nGOOD supposed\n"
			"BAD suprise\nGOOD surprise\n"
			"BAD suprised\nGOOD surprised\n"
			"BAD sux\nGOOD sucks\n"
			"BAD swiming\nGOOD swimming\n"
			"BAD tahn\nGOOD than\n"
			"BAD taht\nGOOD that\n"
			"COMPLETE 0\nBAD take it for granite\nGOOD take it for granted\n"
			"COMPLETE 0\nBAD taken for granite\nGOOD taken for granted\n"
			"BAD talekd\nGOOD talked\n"
			"BAD talkign\nGOOD talking\n"
			"BAD tath\nGOOD that\n"
			"BAD tecnical\nGOOD technical\n"
			"BAD teh\nGOOD the\n"
			"BAD tehy\nGOOD they\n"
			"COMPLETE 0\nBAD tellt he \nGOOD tell the \n"
			"BAD termoil\nGOOD turmoil\n"
			"BAD tets\nGOOD test\n"
			"BAD tghe\nGOOD the\n"
			"BAD tghis\nGOOD this\n"
			"BAD thansk\nGOOD thanks\n"
			"BAD thanx\nGOOD thanks\n"
			"BAD thats\nGOOD that's\n"
			"BAD thatthe\nGOOD that the\n"
			"COMPLETE 0\nBAD thatt he \nGOOD that the \n"
			"BAD thecompany\nGOOD the company\n"
			"BAD thefirst\nGOOD the first\n"
			"BAD thegovernment\nGOOD the government\n"
			"COMPLETE 0\nBAD their are \nGOOD there are \n"
			"COMPLETE 0\nBAD their aren't \nGOOD there aren't \n"
			"COMPLETE 0\nBAD their is \nGOOD there is \n"
			"BAD themself\nGOOD themselves\n"
			"BAD themselfs\nGOOD themselves\n"
			"BAD thenew\nGOOD the new\n"
			"BAD theres\nGOOD there's\n"
			"COMPLETE 0\nBAD there's is \nGOOD theirs is \n"
			"COMPLETE 0\nBAD there's isn't \nGOOD theirs isn't \n"
			"BAD theri\nGOOD their\n"
			"BAD thesame\nGOOD the same\n"
			"BAD thetwo\nGOOD the two\n"
			"BAD theyd\nGOOD they'd\n"
			"COMPLETE 0\nBAD they;d \nGOOD they'd \n"
			"COMPLETE 0\nBAD they;l \nGOOD they'll \n"
			"BAD theyll\nGOOD they'll\n"
			"COMPLETE 0\nBAD they;ll \nGOOD they'll \n"
			"COMPLETE 0\nBAD they;r \nGOOD they're \n"
			"COMPLETE 0\nBAD theyre \nGOOD they're \n"
			"COMPLETE 0\nBAD they;re \nGOOD they're \n"
			"COMPLETE 0\nBAD they're are \nGOOD there are \n"
			"COMPLETE 0\nBAD they're is \nGOOD there is \n"
			"COMPLETE 0\nBAD they;v \nGOOD they've \n"
			"BAD theyve\nGOOD they've\n"
			"COMPLETE 0\nBAD they;ve \nGOOD they've \n"
			"BAD thgat\nGOOD that\n"
			"BAD thge\nGOOD the\n"
			"BAD thier\nGOOD their \n"
			"BAD thigsn\nGOOD things\n"
			"BAD thisyear\nGOOD this year\n"
			"BAD thme\nGOOD them\n"
			"BAD thna\nGOOD than\n"
			"BAD thne\nGOOD then\n"
			"BAD thnig\nGOOD thing\n"
			"BAD thnigs\nGOOD things\n"
			"BAD tho\nGOOD though\n"
			"BAD threatend\nGOOD threatened\n"
			"BAD thsi\nGOOD this\n"
			"BAD thsoe\nGOOD those\n"
			"BAD thta\nGOOD that\n"
			"BAD thursday\nGOOD Thursday\n"
			"BAD thx\nGOOD thanks\n"
			"BAD tihs\nGOOD this\n"
			"BAD timne\nGOOD time\n"
			"BAD tiogether\nGOOD together\n"
			"BAD tje\nGOOD the\n"
			"BAD tjhe\nGOOD the\n"
			"BAD tkae\nGOOD take\n"
			"BAD tkaes\nGOOD takes\n"
			"BAD tkaing\nGOOD taking\n"
			"BAD tlaking\nGOOD talking\n"
			"BAD tnx\nGOOD thanks\n"
			"BAD todya\nGOOD today\n"
			"BAD togehter\nGOOD together\n"
			"COMPLETE 0\nBAD toldt he \nGOOD told the \n"
			"BAD tomorow\nGOOD tomorrow\n"
			"BAD tongiht\nGOOD tonight\n"
			"BAD tonihgt\nGOOD tonight\n"
			"BAD tonite\nGOOD tonight\n"
			"BAD totaly\nGOOD totally\n"
			"BAD totalyl\nGOOD totally\n"
			"BAD tothe\nGOOD to the\n"
			"COMPLETE 0\nBAD tot he \nGOOD to the \n"
			"BAD touche\nGOOD touch\303\251\n"
			"BAD towrad\nGOOD toward\n"
			"BAD traditionalyl\nGOOD traditionally\n"
			"BAD transfered\nGOOD transferred\n"
			"BAD truely\nGOOD truly\n"
			"BAD truley\nGOOD truly\n"
			"BAD tryed\nGOOD tried\n"
			"BAD tthe\nGOOD the\n"
			"BAD tuesday\nGOOD Tuesday\n"
			"BAD tyhat\nGOOD that\n"
			"BAD tyhe\nGOOD the\n"
			"BAD u\nGOOD you\n"
			"BAD udnerstand\nGOOD understand\n"
			"BAD understnad\nGOOD understand\n"
			"COMPLETE 0\nBAD undert he \nGOOD under the \n"
			"BAD unforseen\nGOOD unforeseen\n"
			"BAD UnitedStates\nGOOD United States\n"
			"BAD unliek\nGOOD unlike\n"
			"BAD unpleasently\nGOOD unpleasantly\n"
			"BAD untill\nGOOD until\n"
			"BAD untilll\nGOOD until\n"
			"BAD ur\nGOOD you are\n"
			"BAD useing\nGOOD using\n"
			"BAD usualyl\nGOOD usually\n"
			"BAD veyr\nGOOD very\n"
			"BAD virtualyl\nGOOD virtually\n"
			"BAD visavis\nGOOD vis-a-vis\n"
			"COMPLETE 0\nBAD vis-a-vis\nGOOD vis-\303\240-vis\n"
			"BAD vrey\nGOOD very\n"
			"BAD vulnerible\nGOOD vulnerable\n"
			"BAD waht\nGOOD what\n"
			"BAD warrent\nGOOD warrant\n"
			"COMPLETE 0\nBAD wa snot \nGOOD was not \n"
			"COMPLETE 0\nBAD wasnt \nGOOD wasn't \n"
			"COMPLETE 0\nBAD wasn;t \nGOOD wasn't \n"
			"BAD wat\nGOOD what\n"
			"BAD watn\nGOOD want\n"
			"COMPLETE 0\nBAD we;d \nGOOD we'd \n"
			"BAD wednesday\nGOOD Wednesday\n"
			"BAD wehn\nGOOD when\n"
			"COMPLETE 0\nBAD we'l \nGOOD we'll \n"
			"COMPLETE 0\nBAD we;ll \nGOOD we'll \n"
			"COMPLETE 0\nBAD we;re \nGOOD we're \n"
			"BAD werent\nGOOD weren't\n"
			"COMPLETE 0\nBAD weren;t \nGOOD weren't \n"
			"COMPLETE 0\nBAD wern't \nGOOD weren't \n"
			"BAD werre\nGOOD were\n"
			"BAD weve\nGOOD we've\n"
			"COMPLETE 0\nBAD we;ve \nGOOD we've \n"
			"BAD whats\nGOOD what's\n"
			"COMPLETE 0\nBAD what;s \nGOOD what's \n"
			"BAD whcih\nGOOD which\n"
			"COMPLETE 0\nBAD whent he \nGOOD when the \n"
			"BAD wheres\nGOOD where's\n"
			"COMPLETE 0\nBAD where;s \nGOOD where's \n"
			"BAD wherre\nGOOD where\n"
			"BAD whic\nGOOD which\n"
			"COMPLETE 0\nBAD whicht he \nGOOD which the \n"
			"BAD whihc\nGOOD which\n"
			"BAD wholl\nGOOD who'll\n"
			"BAD whos\nGOOD who's\n"
			"COMPLETE 0\nBAD who;s \nGOOD who's \n"
			"BAD whove\nGOOD who've\n"
			"COMPLETE 0\nBAD who;ve \nGOOD who've \n"
			"BAD whta\nGOOD what\n"
			"BAD whys\nGOOD why's\n"
			"BAD wief\nGOOD wife\n"
			"BAD wierd\nGOOD weird\n"
			"BAD wihch\nGOOD which\n"
			"BAD wiht\nGOOD with\n"
			"BAD willbe\nGOOD will be\n"
			"COMPLETE 0\nBAD will of been\nGOOD will have been\n"
			"COMPLETE 0\nBAD will of had\nGOOD will have had\n"
			"BAD windoes\nGOOD windows\n"
			"BAD witha\nGOOD with a\n"
			"BAD withdrawl\nGOOD withdrawal\n"
			"BAD withe\nGOOD with\n"
			"COMPLETE 0\nBAD withthe \nGOOD with the \n"
			"BAD witht he\nGOOD with the\n"
			"BAD wiull\nGOOD will\n"
			"BAD wnat\nGOOD want\n"
			"BAD wnated\nGOOD wanted\n"
			"BAD wnats\nGOOD wants\n"
			"BAD woh\nGOOD who\n"
			"BAD wohle\nGOOD whole\n"
			"BAD wokr\nGOOD work\n"
			"BAD wont\nGOOD won't\n"
			"COMPLETE 0\nBAD wo'nt \nGOOD won't \n"
			"COMPLETE 0\nBAD won;t \nGOOD won't \n"
			"BAD woudl\nGOOD would\n"
			"COMPLETE 0\nBAD woudln't \nGOOD wouldn't \n"
			"BAD wouldbe\nGOOD would be\n"
			"BAD wouldnt\nGOOD wouldn't\n"
			"COMPLETE 0\nBAD wouldn;t \nGOOD wouldn't \n"
			"COMPLETE 0\nBAD would of been\nGOOD would have been\n"
			"COMPLETE 0\nBAD would of had\nGOOD would have had\n"
			"BAD wouldve\nGOOD would've\n"
			"BAD wriet\nGOOD write\n"
			"BAD writting\nGOOD writing\n"
			"BAD wrod\nGOOD word\n"
			"BAD wroet\nGOOD wrote\n"
			"BAD wroking\nGOOD working\n"
			"BAD wtih\nGOOD with\n"
			"BAD wuould\nGOOD would\n"
			"BAD wud\nGOOD would\n"
			"BAD wut\nGOOD what\n"
			"BAD wya\nGOOD way\n"
			"BAD y\nGOOD why\n"
			"BAD yeh\nGOOD yeah\n"
			"BAD yera\nGOOD year\n"
			"BAD yeras\nGOOD years\n"
			"BAD yersa\nGOOD years\n"
			"BAD yoiu\nGOOD you\n"
			"BAD youare\nGOOD you are\n"
			"BAD youd\nGOOD you'd\n"
			"COMPLETE 0\nBAD you;d \nGOOD you'd \n"
			"BAD youll\nGOOD you'll\n"
			"COMPLETE 0\nBAD your a \nGOOD you're a \n"
			"COMPLETE 0\nBAD your an \nGOOD you're an \n"
			"BAD youre\nGOOD you're\n"
			"COMPLETE 0\nBAD you;re \nGOOD you're \n"
			"COMPLETE 0\nBAD you're own \nGOOD your own \n"
			"COMPLETE 0\nBAD your her \nGOOD you're her \n"
			"COMPLETE 0\nBAD your here \nGOOD you're here \n"
			"COMPLETE 0\nBAD your his \nGOOD you're his \n"
			"COMPLETE 0\nBAD your my \nGOOD you're my \n"
			"COMPLETE 0\nBAD your the \nGOOD you're the \n"
			"COMPLETE 0\nBAD your their \nGOOD you're their \n"
			"COMPLETE 0\nBAD your your \nGOOD you're your \n"
			"BAD youve\nGOOD you've\n"
			"COMPLETE 0\nBAD you;ve \nGOOD you've \n"
			"BAD ytou\nGOOD you\n"
			"BAD yuo\nGOOD you\n"
			"BAD yuor\nGOOD your\n";
	gchar *buf;
	gchar *ibuf;
	GHashTable *hashes;
	char bad[82] = "";
	char good[256] = "";
	int pnt = 0;
	gsize size;
	gboolean complete = TRUE;
	gboolean case_sensitive = FALSE;

	buf = g_build_filename(purple_user_dir(), "dict", NULL);
	g_file_get_contents(buf, &ibuf, &size, NULL);
	g_free(buf);
	if (!ibuf) {
		ibuf = g_strdup(defaultconf);
		size = strlen(defaultconf);
	}

	model = gtk_list_store_new((gint)N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	hashes = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	while (buf_get_line(ibuf, &buf, &pnt, size)) {
		if (*buf != '#') {
			if (!g_ascii_strncasecmp(buf, "BAD ", 4))
			{
				strncpy(bad, buf + 4, 81);
			}
			else if(!g_ascii_strncasecmp(buf, "CASE ", 5))
			{
				case_sensitive = *(buf+5) == '0' ? FALSE : TRUE;
			}
			else if(!g_ascii_strncasecmp(buf, "COMPLETE ", 9))
			{
				complete = *(buf+9) == '0' ? FALSE : TRUE;
			}
			else if (!g_ascii_strncasecmp(buf, "GOOD ", 5))
			{
				strncpy(good, buf + 5, 255);

				if (*bad && *good && g_hash_table_lookup(hashes, bad) == NULL) {
					GtkTreeIter iter;

					/* We don't actually need to store the good string, since this
					 * hash is just being used to eliminate duplicate bad strings.
					 * The value has to be non-NULL so the lookup above will work.
					 */
					g_hash_table_insert(hashes, g_strdup(bad), GINT_TO_POINTER(1));

					if (!complete)
						case_sensitive = TRUE;

					gtk_list_store_append(model, &iter);
					gtk_list_store_set(model, &iter,
						BAD_COLUMN, bad,
						GOOD_COLUMN, good,
						WORD_ONLY_COLUMN, complete,
						CASE_SENSITIVE_COLUMN, case_sensitive,
						-1);
				}
				bad[0] = '\0';
				complete = TRUE;
				case_sensitive = FALSE;
			}
		}
	}
	g_free(ibuf);
	g_hash_table_destroy(hashes);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
	                                     0, GTK_SORT_ASCENDING);
}

static GtkWidget *tree;
static GtkWidget *bad_entry;
static GtkWidget *good_entry;
static GtkWidget *complete_toggle;
static GtkWidget *case_toggle;

static void save_list(void);

static void on_edited(GtkCellRendererText *cellrenderertext,
					  gchar *path, gchar *arg2, gpointer data)
{
	GtkTreeIter iter;
	GValue val;

	if (arg2[0] == '\0') {
		gdk_beep();
		return;
	}

	g_return_if_fail(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path));
	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, GPOINTER_TO_INT(data), &val);

	if (strcmp(arg2, g_value_get_string(&val))) {
		gtk_list_store_set(model, &iter, GPOINTER_TO_INT(data), arg2, -1);
		save_list();
	}
	g_value_unset(&val);
}


static void word_only_toggled(GtkCellRendererToggle *cellrenderertoggle,
						gchar *path, gpointer data){
	GtkTreeIter iter;
	gboolean enabled;

	g_return_if_fail(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path));
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
					   WORD_ONLY_COLUMN, &enabled,
					   -1);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   WORD_ONLY_COLUMN, !enabled,
					   -1);

	/* I want to be sure that the above change has happened to the GtkTreeView first. */
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   CASE_SENSITIVE_COLUMN, enabled,
					   -1);

	save_list();
}

static void case_sensitive_toggled(GtkCellRendererToggle *cellrenderertoggle,
						gchar *path, gpointer data){
	GtkTreeIter iter;
	gboolean enabled;

	g_return_if_fail(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path));

	/* Prevent the case sensitive column from changing on non-whole word replacements.
	 * Ideally, the column would be set insensitive in the word_only_toggled callback. */
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
					   WORD_ONLY_COLUMN, &enabled,
					   -1);
	if (!enabled)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
					   CASE_SENSITIVE_COLUMN, &enabled,
					   -1);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   CASE_SENSITIVE_COLUMN, !enabled,
					   -1);

	save_list();
}

static void list_add_new(void)
{
	GtkTreeIter iter;
	const char *word = gtk_entry_get_text(GTK_ENTRY(bad_entry));
	gboolean case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_toggle));

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
		char *tmpword = g_utf8_casefold(word, -1);

		do {
			GValue bad_val;
			gboolean match;

			bad_val.g_type = 0;
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, BAD_COLUMN, &bad_val);

			if (case_sensitive)
			{
				GValue case_sensitive_val;
				case_sensitive_val.g_type = 0;
				gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CASE_SENSITIVE_COLUMN, &case_sensitive_val);

				/* If they're both case-sensitive, then compare directly.
				 * Otherwise, they overlap. */
				if (g_value_get_boolean(&case_sensitive_val))
				{
					match = !strcmp(g_value_get_string(&bad_val), word);
				}
				else
				{
					char *bad = g_utf8_casefold(g_value_get_string(&bad_val), -1);
					match = !strcmp(bad, tmpword);
					g_free(bad);
				}
				g_value_unset(&case_sensitive_val);
			}
			else
			{
				char *bad = g_utf8_casefold(g_value_get_string(&bad_val), -1);
				match = !strcmp(bad, tmpword);
				g_free(bad);
			}

			if (match) {
				g_value_unset(&bad_val);
				g_free(tmpword);

				purple_notify_error(NULL, _("Duplicate Correction"),
					_("The specified word already exists in the correction list."),
					gtk_entry_get_text(GTK_ENTRY(bad_entry)));
				return;
			}

			g_value_unset(&bad_val);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));

		g_free(tmpword);
	}


	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
		BAD_COLUMN, word,
		GOOD_COLUMN, gtk_entry_get_text(GTK_ENTRY(good_entry)),
		WORD_ONLY_COLUMN, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(complete_toggle)),
		CASE_SENSITIVE_COLUMN, case_sensitive,
		-1);

	gtk_editable_delete_text(GTK_EDITABLE(bad_entry), 0, -1);
	gtk_editable_delete_text(GTK_EDITABLE(good_entry), 0, -1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(complete_toggle), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_toggle), FALSE);
	gtk_widget_grab_focus(bad_entry);

	save_list();
}

static void add_selected_row_to_list(GtkTreeModel *model, GtkTreePath *path,
	GtkTreeIter *iter, gpointer data)
{
	GtkTreeRowReference *row_reference;
	GSList **list = (GSList **)data;
	row_reference = gtk_tree_row_reference_new(model, path);
	*list = g_slist_prepend(*list, row_reference);
}

static void remove_row(void *data1, gpointer data2)
{
	GtkTreeRowReference *row_reference;
	GtkTreePath *path;
	GtkTreeIter iter;

	row_reference = (GtkTreeRowReference *)data1;
	path = gtk_tree_row_reference_get_path(row_reference);

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
		gtk_list_store_remove(model, &iter);

	gtk_tree_path_free(path);
	gtk_tree_row_reference_free(row_reference);
}

static void list_delete(void)
{
	GtkTreeSelection *sel;
	GSList *list = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_selected_foreach(sel, add_selected_row_to_list, &list);

	g_slist_foreach(list, remove_row, NULL);
	g_slist_free(list);

	save_list();
}

static void save_list()
{
	GString *data;
	GtkTreeIter iter;

	data = g_string_new("");

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
		do {
			GValue val0;
			GValue val1;
			GValue val2;
			GValue val3;

			val0.g_type = 0;
			val1.g_type = 0;
			val2.g_type = 0;
			val3.g_type = 0;

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, BAD_COLUMN, &val0);
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, GOOD_COLUMN, &val1);
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, WORD_ONLY_COLUMN, &val2);
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CASE_SENSITIVE_COLUMN, &val3);

			g_string_append_printf(data, "COMPLETE %d\nCASE %d\nBAD %s\nGOOD %s\n\n",
								   g_value_get_boolean(&val2),
								   g_value_get_boolean(&val3),
								   g_value_get_string(&val0),
								   g_value_get_string(&val1));

			g_value_unset(&val0);
			g_value_unset(&val1);
			g_value_unset(&val2);
			g_value_unset(&val3);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
	}

	purple_util_write_data_to_file("dict", data->str, -1);

	g_string_free(data, TRUE);
}

static void on_selection_changed(GtkTreeSelection *sel,
	gpointer data)
{
	gint num_selected;
	num_selected = gtk_tree_selection_count_selected_rows(sel);
	gtk_widget_set_sensitive((GtkWidget*)data, (num_selected > 0));
}

static gboolean non_empty(const char *s)
{
	while (*s && g_ascii_isspace(*s))
		s++;
	return *s;
}

static void on_entry_changed(GtkEditable *editable, gpointer data)
{
	gtk_widget_set_sensitive((GtkWidget*)data,
		non_empty(gtk_entry_get_text(GTK_ENTRY(bad_entry))) &&
		non_empty(gtk_entry_get_text(GTK_ENTRY(good_entry))));
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *conv_handle = purple_conversations_get_handle();
	GList *convs;

	load_conf();

	/* Attach to existing conversations */
	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next)
	{
		spellchk_new_attach((PurpleConversation *)convs->data);
	}

	purple_signal_connect(conv_handle, "conversation-created",
			    plugin, PURPLE_CALLBACK(spellchk_new_attach), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	GList *convs;

	/* Detach from existing conversations */
	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next)
	{
		PidginConversation *gtkconv = PIDGIN_CONVERSATION((PurpleConversation *)convs->data);
		spellchk *spell = g_object_get_data(G_OBJECT(gtkconv->entry), SPELLCHK_OBJECT_KEY);

		g_signal_handlers_disconnect_by_func(gtkconv->entry, message_send_cb, spell);
		g_object_set_data(G_OBJECT(gtkconv->entry), SPELLCHK_OBJECT_KEY, NULL);
	}

	return TRUE;
}

static void whole_words_button_toggled(GtkToggleButton *complete_toggle, GtkToggleButton *case_toggle)
{
	gboolean enabled = gtk_toggle_button_get_active(complete_toggle);

	gtk_toggle_button_set_active(case_toggle, !enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(case_toggle), enabled);
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret, *vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkSizeGroup *sg;
	GtkSizeGroup *sg2;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *vbox2;
	GtkWidget *vbox3;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Text Replacements"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_widget_show(vbox);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_widget_set_size_request(tree, -1, 200);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer),
		"editable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
		G_CALLBACK(on_edited), GINT_TO_POINTER(0));
	column = gtk_tree_view_column_new_with_attributes(_("You type"), renderer,
													  "text", BAD_COLUMN,
													  NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer),
		"editable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
		G_CALLBACK(on_edited), GINT_TO_POINTER(1));
	column = gtk_tree_view_column_new_with_attributes(_("You send"), renderer,
													  "text", GOOD_COLUMN,
													  NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(G_OBJECT(renderer),
		"activatable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "toggled",
		G_CALLBACK(word_only_toggled), NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Whole words only"), renderer,
													  "active", WORD_ONLY_COLUMN,
													  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(G_OBJECT(renderer),
		"activatable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "toggled",
		G_CALLBACK(case_sensitive_toggled), NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Case sensitive"), renderer,
													  "active", CASE_SENSITIVE_COLUMN,
													  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)),
		 GTK_SELECTION_MULTIPLE);
	gtk_box_pack_start(GTK_BOX(vbox), 
		pidgin_make_scrollable(tree, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, -1, -1), 
		TRUE, TRUE, 0);
	gtk_widget_show(tree);

	hbox = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK(list_delete), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);

	g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree))),
		"changed", G_CALLBACK(on_selection_changed), button);

	gtk_widget_show(button);

	vbox = pidgin_make_frame(ret, _("Add a new text replacement"));

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);
	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	sg2 = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	bad_entry = gtk_entry_new();
	/* Set a minimum size. Since they're in a size group, the other entry will match up. */
	gtk_widget_set_size_request(bad_entry, 350, -1);
	gtk_size_group_add_widget(sg2, bad_entry);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox2), _("You _type:"), sg, bad_entry, FALSE, NULL);

	good_entry = gtk_entry_new();
	gtk_size_group_add_widget(sg2, good_entry);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox2), _("You _send:"), sg, good_entry, FALSE, NULL);

	/* Created here so it can be passed to whole_words_button_toggled. */
	case_toggle = gtk_check_button_new_with_mnemonic(_("_Exact case match (uncheck for automatic case handling)"));

	complete_toggle = gtk_check_button_new_with_mnemonic(_("Only replace _whole words"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(complete_toggle), TRUE);
	g_signal_connect(G_OBJECT(complete_toggle), "clicked",
                         G_CALLBACK(whole_words_button_toggled), case_toggle);
	gtk_widget_show(complete_toggle);
	gtk_box_pack_start(GTK_BOX(vbox2), complete_toggle, FALSE, FALSE, 0);

	/* The button is created above so it can be passed to whole_words_button_toggled. */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_toggle), FALSE);
	gtk_widget_show(case_toggle);
	gtk_box_pack_start(GTK_BOX(vbox2), case_toggle, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK(list_add_new), NULL);
	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	gtk_box_pack_end(GTK_BOX(vbox3), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(bad_entry), "changed", G_CALLBACK(on_entry_changed), button);
	g_signal_connect(G_OBJECT(good_entry), "changed", G_CALLBACK(on_entry_changed), button);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

#if 0
	vbox = pidgin_make_frame(ret, _("General Text Replacement Options"));
	pidgin_prefs_checkbox(_("Enable replacement of last word on send"),
	                        "/plugins/gtk/spellchk/last_word_replace", vbox);
#endif

	gtk_widget_show_all(ret);
	g_object_unref(sg);
	g_object_unref(sg2);
	return ret;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,
	0, /* page_num (Reserved) */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	SPELLCHECK_PLUGIN_ID,
	N_("Text replacement"),
	DISPLAY_VERSION,
	N_("Replaces text in outgoing messages according to user-defined rules."),
	N_("Replaces text in outgoing messages according to user-defined rules."),
	"Eric Warmenhoven <eric@warmenhoven.org>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	&ui_info,
	NULL,
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
#if 0
	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/spellchk");
	purple_prefs_add_bool("/plugins/gtk/spellchk/last_word_replace", TRUE);
#endif
}

PURPLE_INIT_PLUGIN(spellcheck, init_plugin, info)
