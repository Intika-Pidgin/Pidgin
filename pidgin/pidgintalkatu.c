/* pidgin
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

#include <pidgin/pidgintalkatu.h>

GtkWidget *
pidgin_talkatu_editor_new_for_connection(PurpleConnection *pc) {
	GtkWidget *editor = NULL;
	GtkWidget *view = NULL;

	g_return_val_if_fail(pc != NULL, NULL);

	editor = talkatu_editor_new();
	view = talkatu_editor_get_view(TALKATU_EDITOR(editor));

	gtk_text_view_set_buffer(
		GTK_TEXT_VIEW(view),
		pidgin_talkatu_buffer_new_for_connection(pc)
	);

	return editor;
}

GtkTextBuffer *
pidgin_talkatu_buffer_new_for_connection(PurpleConnection *pc) {
	PurpleConnectionFlags flags = 0;
	GtkTextBuffer *buffer = NULL;

	g_return_val_if_fail(pc != NULL, NULL);

	flags = purple_connection_get_flags(pc);

	if(flags & PURPLE_CONNECTION_FLAG_HTML) {
		buffer = talkatu_html_buffer_new();
	} else if(flags & PURPLE_CONNECTION_FLAG_FORMATTING_WBFO) {
		buffer = talkatu_whole_buffer_new();
	} else {
		buffer = talkatu_buffer_new(NULL);
	}

	return buffer;
}