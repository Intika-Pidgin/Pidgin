#ifndef DBUS_API_SUBJECT_TO_CHANGE
#define DBUS_API_SUBJECT_TO_CHANGE
#endif

#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbus-purple.h"
#include "purple-client.h"

static DBusGConnection *bus;
static DBusGProxy *purple_proxy;

static GList *garray_int_to_glist(GArray *array)
{
	GList *list = NULL;
	gsize i;

	for (i = 0; i < array->len; i++)
		list = g_list_append(list, GINT_TO_POINTER(g_array_index(array,gint,i)));

	g_array_free(array, TRUE);
	return list;
}

static GSList *garray_int_to_gslist(GArray *array)
{
	GSList *list = NULL;
	gsize i;

	for (i = 0; i < array->len; i++)
		list = g_slist_append(list, GINT_TO_POINTER(g_array_index(array,gint,i)));

	g_array_free(array, TRUE);
	return list;
}

#include "purple-client-bindings.c"

static void lose(const char *fmt, ...) G_GNUC_NORETURN G_GNUC_PRINTF (1, 2);
static void lose_gerror(const char *prefix, GError *error) G_GNUC_NORETURN;

static void
lose(const char *str, ...)
{
	va_list args;

	va_start(args, str);

	vfprintf(stderr, str, args);
	fputc('\n', stderr);

	va_end(args);

	exit(1);
}

static void
lose_gerror(const char *prefix, GError *error)
{
	lose("%s: %s", prefix, error->message);
}

void purple_init(void)
{
	GError *error = NULL;

#if !GLIB_CHECK_VERSION(2, 36, 0)
	/* GLib type system is automaticaly initialized since 2.36. */
	g_type_init();
#endif

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus)
		lose_gerror ("Couldn't connect to session bus", error);

	purple_proxy = dbus_g_proxy_new_for_name (bus,
					PURPLE_DBUS_SERVICE,
					PURPLE_DBUS_PATH,
					PURPLE_DBUS_INTERFACE);

	if (!purple_proxy)
		lose_gerror ("Couldn't connect to the Purple Service", error);
}

