/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include <glib.h>
#include <string.h>

#include <purple.h>

/******************************************************************************
 * PurpleProtcolXfer Implementations
 *****************************************************************************/
static GType test_purple_protocol_xfer_get_type(void);

typedef struct {
	GObject parent;

	gboolean can_send;
} TestPurpleProtocolXfer;

typedef struct {
	GObjectClass parent;
} TestPurpleProtocolXferClass;


static gboolean
test_purple_protocol_xfer_can_receive(PurpleProtocolXferInterface *iface, PurpleConnection *c, const gchar *who) {
	TestPurpleProtocolXfer *xfer = (TestPurpleProtocolXfer *)iface;

	return xfer->can_send;
}

static void
test_purple_protocol_xfer_iface_init(PurpleProtocolXferInterface *iface) {
	iface->can_receive = test_purple_protocol_xfer_can_receive;
	iface->send = NULL;
	iface->new_xfer = NULL;
}

G_DEFINE_TYPE_WITH_CODE(
	TestPurpleProtocolXfer,
	test_purple_protocol_xfer,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(
		PURPLE_TYPE_PROTOCOL_XFER,
		test_purple_protocol_xfer_iface_init
	)
);

static void
test_purple_protocol_xfer_init(TestPurpleProtocolXfer *self) {
}

static void
test_purple_protocol_xfer_class_init(TestPurpleProtocolXferClass *klass){
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_protocol_xfer_can_receive_func(void) {
	TestPurpleProtocolXfer *xfer = g_object_new(test_purple_protocol_xfer_get_type(), NULL);
	PurpleConnection *c = g_object_new(PURPLE_TYPE_CONNECTION, NULL);	
	gboolean actual = FALSE;

	xfer->can_send = FALSE;
	actual = purple_protocol_xfer_can_receive(
		PURPLE_PROTOCOL_XFER(xfer),
		c, 
		"foo"
	);
	g_assert_false(actual);

	xfer->can_send = TRUE;
	actual = purple_protocol_xfer_can_receive(
		PURPLE_PROTOCOL_XFER(xfer),
		c,
		"foo"
	);
	g_assert_true(actual);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	#if GLIB_CHECK_VERSION(2, 38, 0)
	g_test_set_nonfatal_assertions();
	#endif /* GLIB_CHECK_VERSION(2, 38, 0) */

	g_test_add_func("/protocol-xfer/can-receive", test_purple_protocol_xfer_can_receive_func);

	return g_test_run();
}
