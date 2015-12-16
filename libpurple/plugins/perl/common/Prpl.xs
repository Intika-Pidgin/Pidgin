#include "module.h"

MODULE = Purple::Prpl  PACKAGE = Purple::Find  PREFIX = purple_protocols_
PROTOTYPES: ENABLE

Purple::Plugin
purple_protocols_find(id)
	const char *id

MODULE = Purple::Prpl  PACKAGE = Purple::Prpl  PREFIX = purple_protocol_
PROTOTYPES: ENABLE

void
purple_protocol_change_account_status(account, old_status, new_status)
	Purple::Account account
	Purple::Status old_status
	Purple::Status new_status

void
purple_protocol_get_statuses(account, presence)
	Purple::Account account
	Purple::Presence presence
PREINIT:
	GList *l, *ll;
PPCODE:
	ll = purple_protocol_get_statuses(account,presence);
	for (l = ll; l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Status")));
	}
	/* We can free the list here but the script needs to free the
	 * Purple::Status 'objects' itself. */
	g_list_free(ll);

void
purple_protocol_got_account_idle(account, idle, idle_time)
	Purple::Account account
	gboolean idle
	time_t idle_time

void
purple_protocol_got_account_login_time(account, login_time)
	Purple::Account account
	time_t login_time

void
purple_protocol_got_user_idle(account, name, idle, idle_time)
	Purple::Account account
	const char *name
	gboolean idle
	time_t idle_time

void
purple_protocol_got_user_login_time(account, name, login_time)
	Purple::Account account
	const char *name
	time_t login_time

int
purple_protocol_send_raw(gc, str)
	Purple::Connection gc
	const char *str
PREINIT:
	PurpleProtocol *protocol;
CODE:
	if (!gc)
		RETVAL = 0;
	else {
		protocol = purple_connection_get_protocol(gc);
		if (protocol)
			RETVAL = purple_protocol_iface_send_raw(protocol, gc, str, strlen(str));
		else
			RETVAL = 0;
	}
OUTPUT:
	RETVAL

