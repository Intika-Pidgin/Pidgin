#include "gntutils.h"
#include "gnttree.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"

void gnt_util_get_text_bound(const char *text, int *width, int *height)
{
	const char *s = text, *last;
	int count = 1, max = 0;
	int len;

	/* XXX: ew ... everyone look away */
	last = s;
	if (s)
	{
		while (*s)
		{
			if (*s == '\n' || *s == '\r')
			{
				count++;
				len = gnt_util_onscreen_width(last, s);
				if (max < len)
					max = len;
				last = s + 1;
			}
			s = g_utf8_next_char(s);
		}

		len = gnt_util_onscreen_width(last, s);
		if (max < len)
			max = len;
	}

	if (height)
		*height = count;
	if (width)
		*width = max + (count > 1);
}

int gnt_util_onscreen_width(const char *start, const char *end)
{
	int width = 0;

	if (end == NULL)
		end = start + strlen(start);

	while (start < end) {
		width += g_unichar_iswide(g_utf8_get_char(start)) ? 2 : 1;
		start = g_utf8_next_char(start);
	}
	return width;
}

const char *gnt_util_onscreen_width_to_pointer(const char *string, int len, int *w)
{
	int size;
	int width = 0;
	const char *str = string;

	if (len <= 0) {
		len = gnt_util_onscreen_width(string, NULL);
	}

	while (width < len && *str) {
		size = g_unichar_iswide(g_utf8_get_char(str)) ? 2 : 1;
		if (width + size > len)
			break;
		str = g_utf8_next_char(str);
		width += size;
	}
	if (w)
		*w = width;
	return str;
}

char *gnt_util_onscreen_fit_string(const char *string, int maxw)
{
	const char *start, *end;
	GString *str;

	if (maxw <= 0)
		maxw = getmaxx(stdscr) - 4;

	start = string;
	str = g_string_new(NULL);

	while (*start) {
		if ((end = strchr(start, '\n')) != NULL ||
			(end = strchr(start, '\r')) != NULL) {
			if (gnt_util_onscreen_width(start, end) > maxw)
				end = NULL;
		}
		if (end == NULL)
			end = gnt_util_onscreen_width_to_pointer(start, maxw, NULL);
		str = g_string_append_len(str, start, end - start);
		if (*end) {
			str = g_string_append_c(str, '\n');
			if (*end == '\n' || *end == '\r')
				end++;
		}
		start = end;
	}
	return g_string_free(str, FALSE);
}

struct duplicate_fns
{
	GDupFunc key_dup;
	GDupFunc value_dup;
	GHashTable *table;
};

static void
duplicate_values(gpointer key, gpointer value, gpointer data)
{
	struct duplicate_fns *fns = data;
	g_hash_table_insert(fns->table, fns->key_dup ? fns->key_dup(key) : key,
			fns->value_dup ? fns->value_dup(value) : value);
}

GHashTable *g_hash_table_duplicate(GHashTable *src, GHashFunc hash,
		GEqualFunc equal, GDestroyNotify key_d, GDestroyNotify value_d,
		GDupFunc key_dup, GDupFunc value_dup)
{
	GHashTable *dest = g_hash_table_new_full(hash, equal, key_d, value_d);
	struct duplicate_fns fns = {key_dup, value_dup, dest};
	g_hash_table_foreach(src, duplicate_values, &fns);
	return dest;
}

gboolean gnt_boolean_handled_accumulator(GSignalInvocationHint *ihint,
				  GValue                *return_accu,
				  const GValue          *handler_return,
				  gpointer               dummy)
{
	gboolean continue_emission;
	gboolean signal_handled;

	signal_handled = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, signal_handled);
	continue_emission = !signal_handled;

	return continue_emission;
}

typedef struct {
	GHashTable *hash;
	GntTree *tree;
} BindingView;

static void
add_binding(gpointer key, gpointer value, gpointer data)
{
	BindingView *bv = data;
	GntBindableActionParam *act = value;
	const char *name = g_hash_table_lookup(bv->hash, act->action);
	if (name && *name) {
		const char *k = gnt_key_lookup(key);
		if (!k)
			k = key;
		gnt_tree_add_row_after(bv->tree, (gpointer)k,
				gnt_tree_create_row(bv->tree, k, name), NULL, NULL);
	}
}

static void
add_action(gpointer key, gpointer value, gpointer data)
{
	BindingView *bv = data;
	g_hash_table_insert(bv->hash, value, key);
}

GntWidget *gnt_widget_bindings_view(GntWidget *widget)
{
	GntBindable *bind = GNT_BINDABLE(widget);
	GntWidget *tree = gnt_tree_new_with_columns(2);
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bind));
	GHashTable *hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	BindingView bv = {hash, GNT_TREE(tree)};

	gnt_tree_set_compare_func(bv.tree, (GCompareFunc)g_utf8_collate);
	g_hash_table_foreach(klass->actions, add_action, &bv);
	g_hash_table_foreach(klass->bindings, add_binding, &bv);
	gnt_tree_adjust_columns(bv.tree);
	g_hash_table_destroy(hash);

	return tree;
}

