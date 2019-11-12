/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "glibcompat.h"

#if !GLIB_CHECK_VERSION(2,32,0)

gboolean
g_hash_table_contains(GHashTable *hash_table, gconstpointer key) {
	return g_hash_table_lookup_extended(hash_table, key, NULL, NULL);
}

void
g_queue_free_full(GQueue *queue, GDestroyNotify free_func) {
	GList *l = NULL;

	for(l = queue->head; l != NULL; l = l->next) {
		free_func(l->data);
	}

	g_queue_free(queue);
}

#endif /* !GLIB_CHECK_VERSION(2,32,0) */

