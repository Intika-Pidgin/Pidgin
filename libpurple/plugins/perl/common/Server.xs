#include "module.h"

MODULE = Purple::Serv  PACKAGE = Purple::Serv  PREFIX = serv_
PROTOTYPES: ENABLE


void 
purple_serv_add_deny(con, a)
	Purple::Connection con
	const char * a

void 
purple_serv_add_permit(a, b)
	Purple::Connection a
	const char * b

void 
purple_serv_alias_buddy(buddy)
	Purple::BuddyList::Buddy buddy 

void 
purple_serv_chat_invite(con, a, b, c)
	Purple::Connection con
	int a
	const char * b
	const char * c

void 
purple_serv_chat_leave(a, b)
	Purple::Connection a
	int b

void 
purple_serv_get_info(con, a)
	Purple::Connection con 
	const char * a

void 
purple_serv_got_alias(gc, who, alias)
	Purple::Connection gc
	const char *who
	const char *alias

void 
purple_serv_got_chat_in(g, id, who, chatflags, message, mtime)
	Purple::Connection g
	int id
	const char *who
	Purple::MessageFlags chatflags
	const char *message
	time_t mtime

void 
purple_serv_got_chat_invite(gc, name, who, message, components)
	Purple::Connection gc
	const char *name
	const char *who
	const char *message
	SV * components
INIT:
	HV * t_HV;
	HE * t_HE;
	SV * t_SV;
	GHashTable * t_GHash;
	I32 len;
	char *t_key, *t_value;
CODE:
	t_HV =  (HV *)SvRV(components);
	t_GHash = g_hash_table_new(g_str_hash, g_str_equal);

	for (t_HE = hv_iternext(t_HV); t_HE != NULL; t_HE = hv_iternext(t_HV) ) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(t_HV, t_key, len, 0);
 		t_value = SvPVutf8_nolen(t_SV);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}
	purple_serv_got_chat_invite(gc, name, who, message, t_GHash);

void 
purple_serv_got_chat_left(g, id)
	Purple::Connection g
	int id

void 
purple_serv_got_im(gc, who, msg, imflags, mtime)
	Purple::Connection gc
	const char *who
	const char *msg
	Purple::MessageFlags imflags
	time_t mtime

Purple::ChatConversation
purple_serv_got_joined_chat(gc, id, name)
	Purple::Connection gc
	int id
	const char *name

void 
purple_serv_got_typing(gc, name, timeout, state)
	Purple::Connection gc
	const char *name
	int timeout
	Purple::IMTypingState state

void 
purple_serv_got_typing_stopped(gc, name)
	Purple::Connection gc
	const char *name

void
purple_serv_join_chat(conn, components)
	Purple::Connection conn
	HV * components
PREINIT:
	HE *t_HE;
	SV *t_SV;
	I32 len;
	GHashTable *t_GHash;
	char *t_key, *t_value;
CODE:
	t_GHash = g_hash_table_new(g_str_hash, g_str_equal);

	for (t_HE = hv_iternext(components); t_HE != NULL;
	     t_HE = hv_iternext(components)) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(components, t_key, len, 0);
		t_value = SvPVutf8_nolen(t_SV);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}
	purple_serv_join_chat(conn, t_GHash);
	g_hash_table_destroy(t_GHash);

void 
purple_serv_move_buddy(buddy, group1, group2)
	Purple::BuddyList::Buddy buddy
	Purple::BuddyList::Group group1
	Purple::BuddyList::Group group2

void 
purple_serv_reject_chat(con, components)
	Purple::Connection con 
	SV * components
INIT:
	HV * t_HV;
	HE * t_HE;
	SV * t_SV;
	GHashTable * t_GHash;
	I32 len;
	char *t_key, *t_value;
CODE:
	t_HV =  (HV *)SvRV(components);
	t_GHash = g_hash_table_new(g_str_hash, g_str_equal);

	for (t_HE = hv_iternext(t_HV); t_HE != NULL; t_HE = hv_iternext(t_HV) ) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(t_HV, t_key, len, 0);
 		t_value = SvPVutf8_nolen(t_SV);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}
	purple_serv_reject_chat(con, t_GHash);

void 
purple_serv_rem_deny(con, a)
	Purple::Connection con
	const char * 	a

void 
purple_serv_rem_permit(con, a)
	Purple::Connection con
	const char *	a

void 
purple_serv_send_file(gc, who, file)
	Purple::Connection gc
	const char *who
	const char *file

int  
purple_serv_send_typing(con, a, state)
	Purple::Connection con
	const char * a
	Purple::IMTypingState state

void 
purple_serv_set_info(con, a)
	Purple::Connection con 
	const char * a

void 
purple_serv_set_permit_deny(con)
	Purple::Connection con  

