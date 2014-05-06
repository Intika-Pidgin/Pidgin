#include "module.h"
#include "../perl-handlers.h"

/* Prototypes for the BOOT section below. */
PURPLE_PERL_BOOT_PROTO(Account);
PURPLE_PERL_BOOT_PROTO(Account__Option);
PURPLE_PERL_BOOT_PROTO(Buddy__Icon);
PURPLE_PERL_BOOT_PROTO(BuddyList);
PURPLE_PERL_BOOT_PROTO(Certificate);
PURPLE_PERL_BOOT_PROTO(Cmd);
PURPLE_PERL_BOOT_PROTO(Connection);
PURPLE_PERL_BOOT_PROTO(Conversation);
PURPLE_PERL_BOOT_PROTO(Core);
PURPLE_PERL_BOOT_PROTO(Debug);
PURPLE_PERL_BOOT_PROTO(Hash);
PURPLE_PERL_BOOT_PROTO(Xfer);
PURPLE_PERL_BOOT_PROTO(Idle);
PURPLE_PERL_BOOT_PROTO(Log);
PURPLE_PERL_BOOT_PROTO(Network);
PURPLE_PERL_BOOT_PROTO(Notify);
PURPLE_PERL_BOOT_PROTO(Plugin);
PURPLE_PERL_BOOT_PROTO(PluginPref);
PURPLE_PERL_BOOT_PROTO(Pounce);
PURPLE_PERL_BOOT_PROTO(Prefs);
PURPLE_PERL_BOOT_PROTO(Proxy);
PURPLE_PERL_BOOT_PROTO(Prpl);
PURPLE_PERL_BOOT_PROTO(Request);
PURPLE_PERL_BOOT_PROTO(Roomlist);
PURPLE_PERL_BOOT_PROTO(SSL);
PURPLE_PERL_BOOT_PROTO(SavedStatus);
PURPLE_PERL_BOOT_PROTO(Serv);
PURPLE_PERL_BOOT_PROTO(Signal);
PURPLE_PERL_BOOT_PROTO(Sound);
PURPLE_PERL_BOOT_PROTO(Status);
PURPLE_PERL_BOOT_PROTO(Stringref);
PURPLE_PERL_BOOT_PROTO(Util);
PURPLE_PERL_BOOT_PROTO(Whiteboard);
PURPLE_PERL_BOOT_PROTO(XMLNode);

MODULE = Purple PACKAGE = Purple PREFIX = purple_
PROTOTYPES: ENABLE

BOOT:
	PURPLE_PERL_BOOT(Account);
	PURPLE_PERL_BOOT(Account__Option);
	PURPLE_PERL_BOOT(Buddy__Icon);
	PURPLE_PERL_BOOT(BuddyList);
	PURPLE_PERL_BOOT(Certificate);
	PURPLE_PERL_BOOT(Cmd);
	PURPLE_PERL_BOOT(Connection);
	PURPLE_PERL_BOOT(Conversation);
	PURPLE_PERL_BOOT(Core);
	PURPLE_PERL_BOOT(Debug);
	PURPLE_PERL_BOOT(Hash);
	PURPLE_PERL_BOOT(Xfer);
	PURPLE_PERL_BOOT(Idle);
	PURPLE_PERL_BOOT(Log);
	PURPLE_PERL_BOOT(Network);
	PURPLE_PERL_BOOT(Notify);
	PURPLE_PERL_BOOT(Plugin);
	PURPLE_PERL_BOOT(PluginPref);
	PURPLE_PERL_BOOT(Pounce);
	PURPLE_PERL_BOOT(Prefs);
	PURPLE_PERL_BOOT(Proxy);
	PURPLE_PERL_BOOT(Prpl);
	PURPLE_PERL_BOOT(Request);
	PURPLE_PERL_BOOT(Roomlist);
	PURPLE_PERL_BOOT(SSL);
	PURPLE_PERL_BOOT(SavedStatus);
	PURPLE_PERL_BOOT(Serv);
	PURPLE_PERL_BOOT(Signal);
	PURPLE_PERL_BOOT(Sound);
	PURPLE_PERL_BOOT(Status);
	PURPLE_PERL_BOOT(Stringref);
	PURPLE_PERL_BOOT(Util);
	PURPLE_PERL_BOOT(Whiteboard);
	PURPLE_PERL_BOOT(XMLNode);

guint
timeout_add(plugin, seconds, callback, data = 0)
	Purple::Plugin plugin
	int seconds
	SV *callback
	SV *data
CODE:
	RETVAL = purple_perl_timeout_add(plugin, seconds, callback, data);
OUTPUT:
	RETVAL

gboolean
timeout_remove(handle)
	guint handle
CODE:
	RETVAL = purple_perl_timeout_remove(handle);
OUTPUT:
	RETVAL

void
deinit()
CODE:
	purple_perl_timeout_clear();


MODULE = Purple PACKAGE = Purple PREFIX = purple_
PROTOTYPES: ENABLE

Purple::Core
purple_get_core()
