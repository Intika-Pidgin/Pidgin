/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "gntbindable.h"
#include "gntstyle.h"
#include "gnt.h"
#include "gntutils.h"

static GObjectClass *parent_class = NULL;

static void
gnt_bindable_class_init(GntBindableClass *klass)
{
	parent_class = g_type_class_peek_parent(klass);

	klass->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				(GDestroyNotify)gnt_bindable_action_free);
	klass->bindings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				(GDestroyNotify)gnt_bindable_action_param_free);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
	GNTDEBUG;
}

static gpointer
bindable_clone(GntBindableAction *action)
{
	GntBindableAction *ret = g_new0(GntBindableAction, 1);
	ret->name = g_strdup(action->name);
	ret->u = action->u;
	return ret;
}

static gpointer
binding_clone(GntBindableActionParam *param)
{
	GntBindableActionParam *p = g_new0(GntBindableActionParam, 1);
	p->list = g_list_copy(param->list);
	p->action = param->action;
	return p;
}

static void
duplicate_hashes(GntBindableClass *klass)
{
	/* Duplicate the bindings from parent class */
	if (klass->actions) {
		klass->actions = g_hash_table_duplicate(klass->actions, g_str_hash,
					g_str_equal, g_free, (GDestroyNotify)gnt_bindable_action_free,
					(GDupFunc)g_strdup, (GDupFunc)bindable_clone);
		klass->bindings = g_hash_table_duplicate(klass->bindings, g_str_hash,
					g_str_equal, g_free, (GDestroyNotify)gnt_bindable_action_param_free,
					(GDupFunc)g_strdup, (GDupFunc)binding_clone);
	} else {
		klass->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
					(GDestroyNotify)gnt_bindable_action_free);
		klass->bindings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
					(GDestroyNotify)gnt_bindable_action_param_free);
	}

	GNTDEBUG;
}

/******************************************************************************
 * GntBindable API
 *****************************************************************************/
GType
gnt_bindable_get_gtype(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(GntBindableClass),
			(GBaseInitFunc)duplicate_hashes,	/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_bindable_class_init,
			NULL,
			NULL,					/* class_data		*/
			sizeof(GntBindable),
			0,						/* n_preallocs		*/
			NULL,					/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "GntBindable",
									  &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**
 * Key Remaps
 */
const char *
gnt_bindable_remap_keys(GntBindable *bindable, const char *text)
{
	const char *remap = NULL;
	GType type = G_OBJECT_TYPE(bindable);
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bindable));

	if (klass->remaps == NULL)
	{
		klass->remaps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		gnt_styles_get_keyremaps(type, klass->remaps);
	}

	remap = g_hash_table_lookup(klass->remaps, text);

	return (remap ? remap : text);
}

/**
 * Actions and Bindings
 */
gboolean
gnt_bindable_perform_action_named(GntBindable *bindable, const char *name, ...)
{
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bindable));
	GList *list = NULL;
	va_list args;
	GntBindableAction *action;
	void *p;

	va_start(args, name);
	while ((p = va_arg(args, void *)) != NULL)
		list = g_list_append(list, p);
	va_end(args);
	
	action = g_hash_table_lookup(klass->actions, name);
	if (action && action->u.action) {
		return action->u.action(bindable, list);
	}
	return FALSE;
}

gboolean
gnt_bindable_perform_action_key(GntBindable *bindable, const char *keys)
{
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bindable));
	GntBindableActionParam *param = g_hash_table_lookup(klass->bindings, keys);

	if (param && param->action) {
		if (param->list)
			return param->action->u.action(bindable, param->list);
		else
			return param->action->u.action_noparam(bindable);
	}
	return FALSE;
}

static void
register_binding(GntBindableClass *klass, const char *name, const char *trigger, GList *list)
{
	GntBindableActionParam *param;
	GntBindableAction *action;

	if (name == NULL || *name == '\0') {
		g_hash_table_remove(klass->bindings, (char*)trigger);
		gnt_keys_del_combination(trigger);
		return;
	}

	action = g_hash_table_lookup(klass->actions, name);
	if (!action) {
		g_printerr("GntWidget: Invalid action name %s for %s\n",
				name, g_type_name(G_OBJECT_CLASS_TYPE(klass)));
		if (list)
			g_list_free(list);
		return;
	}

	param = g_new0(GntBindableActionParam, 1);
	param->action = action;
	param->list = list;
	g_hash_table_replace(klass->bindings, g_strdup(trigger), param);
	gnt_keys_add_combination(trigger);
}

void gnt_bindable_register_binding(GntBindableClass *klass, const char *name,
			const char *trigger, ...)
{
	GList *list = NULL;
	va_list args;
	void *data;

	va_start(args, trigger);
	while ((data = va_arg(args, void *))) {
		list = g_list_append(list, data);
	}
	va_end(args);

	register_binding(klass, name, trigger, list);
}

void gnt_bindable_class_register_action(GntBindableClass *klass, const char *name,
			GntBindableActionCallback callback, const char *trigger, ...)
{
	void *data;
	va_list args;
	GntBindableAction *action = g_new0(GntBindableAction, 1);
	GList *list;

	action->name = g_strdup(name);
	action->u.action = callback;

	g_hash_table_replace(klass->actions, g_strdup(name), action);

	if (trigger && *trigger) {
		list = NULL;
		va_start(args, trigger);
		while ((data = va_arg(args, void *))) {
			list = g_list_append(list, data);
		}
		va_end(args);

		register_binding(klass, name, trigger, list);
	}
}

void gnt_bindable_action_free(GntBindableAction *action)
{
	g_free(action->name);
	g_free(action);
}

void gnt_bindable_action_param_free(GntBindableActionParam *param)
{
	g_list_free(param->list);   /* XXX: There may be a leak here for string parameters */
	g_free(param);
}


