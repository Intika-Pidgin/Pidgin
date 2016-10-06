/*
 *					MXit Protocol libPurple Plugin
 *
 *					--  MXit libPurple plugin API --
 *
 *				Pieter Loubser	<libpurple@mxit.com>
 *
 *			(C) Copyright 2009	MXit Lifestyle (Pty) Ltd.
 *				<http://www.mxitlifestyle.com>
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
 */

#include	"internal.h"
#include	"debug.h"
#include	"accountopt.h"
#include	"plugins.h"
#include	"version.h"

#include	"mxit.h"
#include	"client.h"
#include	"login.h"
#include	"roster.h"
#include	"chunk.h"
#include	"filexfer.h"
#include	"actions.h"
#include	"multimx.h"


#ifdef	MXIT_LINK_CLICK


/* pidgin callback function pointers for URI click interception */
static void *(*mxit_pidgin_uri_cb)(const char *uri);
static PurpleNotifyUiOps* mxit_nots_override_original;
static PurpleNotifyUiOps mxit_nots_override;
static int not_link_ref_count = 0;
static PurpleProtocol *my_protocol = NULL;


/*------------------------------------------------------------------------
 * Handle an URI clicked on the UI
 *
 * @param link	the link name which has been clicked
 */
static void* mxit_link_click( const char* link64 )
{
	PurpleAccount*		account;
	PurpleConnection*	gc;
	gchar**				parts		= NULL;
	gchar*				link		= NULL;
	gsize				len;
	gboolean			is_command	= FALSE;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_link_click (%s)\n", link64 );

	if ( g_ascii_strncasecmp( link64, MXIT_LINK_PREFIX, strlen( MXIT_LINK_PREFIX ) ) != 0 ) {
		/* this is not for us */
		goto skip;
	}

	/* decode the base64 payload */
	link = (gchar*) purple_base64_decode( link64 + strlen( MXIT_LINK_PREFIX ), &len );
	purple_debug_info( MXIT_PLUGIN_ID, "Clicked Link: '%s'\n", link );

	parts = g_strsplit( link, "|", 6 );

	/* check if this is a valid mxit link */
	if ( ( !parts ) || ( !parts[0] ) || ( !parts[1] ) || ( !parts[2] ) || ( !parts[3] ) || ( !parts[4] ) || ( !parts[5] ) ) {
		/* this is not for us */
		goto skip;
	}
	else if ( g_ascii_strcasecmp( parts[0], MXIT_LINK_KEY ) != 0 ) {
		/* this is not for us */
		goto skip;
	}

	/* find the account */
	account = purple_accounts_find( parts[1], parts[2] );
	if ( !account )
		goto skip;
	gc = purple_account_get_connection( account );
	if ( !gc )
		goto skip;

	/* determine if it's a command-response to send */
	is_command = ( atoi( parts[4] ) == 1 );

	/* send click message back to MXit */
	mxit_send_message( purple_connection_get_protocol_data( gc ), parts[3], parts[5], FALSE, is_command );

	g_free( link );
	link = NULL;
	g_strfreev( parts );
	parts = NULL;

	return (void*) link64;

skip:
	/* this is not an internal mxit link */

	g_free(link);
	link = NULL;

	if ( parts )
		g_strfreev( parts );
	parts = NULL;

	if ( mxit_pidgin_uri_cb )
		return mxit_pidgin_uri_cb( link64 );
	else
		return (void*) link64;
}


/*------------------------------------------------------------------------
 * Register MXit to receive URI click notifications from the UI
 */
void mxit_register_uri_handler( void )
{
	not_link_ref_count++;
	if ( not_link_ref_count == 1 ) {
		/* make copy of notifications */
		mxit_nots_override_original = purple_notify_get_ui_ops();
		memcpy( &mxit_nots_override, mxit_nots_override_original, sizeof( PurpleNotifyUiOps ) );

		/* save previously configured callback function pointer */
		mxit_pidgin_uri_cb = mxit_nots_override.notify_uri;

		/* override the URI function call with MXit's own one */
		mxit_nots_override.notify_uri = mxit_link_click;
		purple_notify_set_ui_ops( &mxit_nots_override );
	}
}


/*------------------------------------------------------------------------
 * Unregister MXit from receiving URI click notifications from the UI
 */
static void mxit_unregister_uri_handler()
{
	not_link_ref_count--;
	if ( not_link_ref_count == 0 ) {
		/* restore the notifications to its original state */
		purple_notify_set_ui_ops( mxit_nots_override_original );
	}
}

#endif


/*------------------------------------------------------------------------
 * This gets called when a new chat conversation is opened by the user
 *
 *  @param conv				The conversation object
 *  @param session			The MXit session object
 */
static void mxit_cb_chat_created( PurpleConversation* conv, struct MXitSession* session )
{
	PurpleConnection*	gc;
	struct contact*		contact;
	PurpleBuddy*		buddy;
	const char*			who;
	char*				tmp;

	gc = purple_conversation_get_connection( conv );
	if ( session->con != gc ) {
		/* not our conversation */
		return;
	}
	else if ( !PURPLE_IS_IM_CONVERSATION( conv ) ) {
		/* wrong type of conversation */
		return;
	}

	/* get the contact name */
	who = purple_conversation_get_name( conv );
	if ( !who )
		return;

	purple_debug_info( MXIT_PLUGIN_ID, "Conversation started with '%s'\n", who );

	/* find the buddy object */
	buddy = purple_blist_find_buddy( session->acc, who );
	if ( !buddy )
		return;

	contact = purple_buddy_get_protocol_data( buddy );
	if ( !contact )
		return;

	/* we ignore all conversations with which we have chatted with in this session */
	if ( find_active_chat( session->active_chats, who ) )
		return;

	/* determine if this buddy is a MXit service */
	switch ( contact->type ) {
		case MXIT_TYPE_BOT :
		case MXIT_TYPE_CHATROOM :
		case MXIT_TYPE_GALLERY :
		case MXIT_TYPE_INFO :
				tmp = g_strdup_printf("<font color=\"#999999\">%s</font>\n", _( "Loading menu..." ));
				purple_serv_got_im( session->con, who, tmp, PURPLE_MESSAGE_NOTIFY, time( NULL ) );
				g_free( tmp );
				mxit_send_message( session, who, " ", FALSE, FALSE );
		default :
				break;
	}
}


/*------------------------------------------------------------------------
 * Enable some signals to handled by our plugin
 *
 *  @param session			The MXit session object
 */
void mxit_enable_signals( struct MXitSession* session )
{
	/* enable the signal when a new conversation is opened by the user */
	purple_signal_connect_priority( purple_conversations_get_handle(), "conversation-created", session, PURPLE_CALLBACK( mxit_cb_chat_created ),
			session, PURPLE_SIGNAL_PRIORITY_HIGHEST );
}


/*------------------------------------------------------------------------
 * Disable some signals handled by our plugin
 *
 *  @param session			The MXit session object
 */
static void mxit_disable_signals( struct MXitSession* session )
{
	/* disable the signal when a new conversation is opened by the user */
	purple_signal_disconnect( purple_conversations_get_handle(), "conversation-created", session, PURPLE_CALLBACK( mxit_cb_chat_created ) );
}


/*------------------------------------------------------------------------
 * Return the base icon name.
 *
 *  @param account	The MXit account object
 *  @param buddy	The buddy
 *  @return			The icon name (excluding extension)
 */
static const char* mxit_list_icon( PurpleAccount* account, PurpleBuddy* buddy )
{
	return "mxit";
}


/*------------------------------------------------------------------------
 * Return the emblem icon name.
 *
 *  @param buddy	The buddy
 *  @return			The icon name (excluding extension)
 */
static const char* mxit_list_emblem( PurpleBuddy* buddy )
{
	struct contact*	contact = purple_buddy_get_protocol_data( buddy );

	if ( !contact )
		return NULL;

	/* subscription state is Pending, Rejected or Deleted */
	if ( contact->subtype != MXIT_SUBTYPE_BOTH )
		return "not-authorized";

	switch ( contact-> type ) {
		case MXIT_TYPE_JABBER :			/* external contacts via MXit */
		case MXIT_TYPE_MSN :
		case MXIT_TYPE_YAHOO :
		case MXIT_TYPE_ICQ :
		case MXIT_TYPE_AIM :
		case MXIT_TYPE_QQ :
		case MXIT_TYPE_WV :
			return "external";

		case MXIT_TYPE_BOT :			/* MXit services */
		case MXIT_TYPE_GALLERY :
		case MXIT_TYPE_INFO :
			return "bot";

		case MXIT_TYPE_CHATROOM :		/* MXit group chat services */
		case MXIT_TYPE_MULTIMX :
		default:
			return NULL;
	}
}


/*------------------------------------------------------------------------
 * Return short string representing buddy's status for display on buddy list.
 * Returns status message (if one is set), or otherwise the mood.
 *
 *  @param buddy	The buddy.
 *  @return			The status text
 */
char* mxit_status_text( PurpleBuddy* buddy )
{
	char* text = NULL;
	struct contact*	contact = purple_buddy_get_protocol_data( buddy );

	if ( !contact )
		return NULL;

	if ( contact->statusMsg )							/* status message */
		text = g_strdup( contact-> statusMsg );
	else if ( contact->mood != MXIT_MOOD_NONE )			/* mood */
		text = g_strdup( mxit_convert_mood_to_name( contact->mood ) );

	return text;
}


/*------------------------------------------------------------------------
 * Return UI tooltip information for a buddy when hovering in buddy list.
 *
 *  @param buddy	The buddy
 *  @param info		The tooltip info being returned
 *  @param full		Return full or summarized information
 */
static void mxit_tooltip( PurpleBuddy* buddy, PurpleNotifyUserInfo* info, gboolean full )
{
	struct contact*	contact = purple_buddy_get_protocol_data( buddy );

	if ( !contact )
		return;

	/* status (reference: "libpurple/notify.h") */
	if ( contact->presence != MXIT_PRESENCE_OFFLINE )
		purple_notify_user_info_add_pair_plaintext( info, _( "Status" ), mxit_convert_presence_to_name( contact->presence ) );

	/* status message */
	if ( contact->statusMsg ) {
		/* TODO: Check whether it's correct to call add_pair_html,
		         or if we should be using add_pair_plaintext */
		purple_notify_user_info_add_pair_html( info, _( "Status Message" ), contact->statusMsg );
	}

	/* mood */
	if ( contact->mood != MXIT_MOOD_NONE )
		purple_notify_user_info_add_pair_plaintext( info, _( "Mood" ), mxit_convert_mood_to_name( contact->mood ) );

	/* subscription type */
	if ( contact->subtype != 0 )
		purple_notify_user_info_add_pair_plaintext( info, _( "Subscription" ), mxit_convert_subtype_to_name( contact->subtype ) );

	/* rejection message */
	if ( ( contact->subtype == MXIT_SUBTYPE_REJECTED ) && ( contact->msg != NULL ) )
		purple_notify_user_info_add_pair_plaintext( info, _( "Rejection Message" ), contact->msg );
}


/*------------------------------------------------------------------------
 * Initiate the logout sequence, close the connection and clear the session data.
 *
 *  @param gc	The connection object
 */
static void mxit_close( PurpleConnection* gc )
{
	struct MXitSession*	session	= purple_connection_get_protocol_data( gc );

	/* disable signals */
	mxit_disable_signals( session );

	/* close the connection */
	mxit_close_connection( session );

#ifdef		MXIT_LINK_CLICK
	/* unregister for uri click notification */
	mxit_unregister_uri_handler();
#endif

	purple_debug_info( MXIT_PLUGIN_ID, "Releasing the session object..\n" );

	/* free the session memory */
	g_free( session );
	session = NULL;
}


/*------------------------------------------------------------------------
 * Send a message to a contact
 *
 *  @param gc		The connection object
 *  @param who		The username of the recipient
 *  @param message	The message text
 *  @param flags	Message flags (defined in conversation.h)
 *  @return			Positive value (success, and echo to conversation window)
					Zero (success, no echo)
					Negative value (error)
 */
static int mxit_send_im(PurpleConnection* gc, PurpleMessage *msg)
{
	mxit_send_message(purple_connection_get_protocol_data(gc),
		purple_message_get_recipient(msg), purple_message_get_contents(msg),
		TRUE, FALSE);

	return 1;		/* echo to conversation window */
}


/*------------------------------------------------------------------------
 * The user changed their current presence state.
 *
 *  @param account	The MXit account object
 *  @param status	The new status (libPurple status type)
 */
static void mxit_set_status( PurpleAccount* account, PurpleStatus* status )
{
	struct MXitSession*		session =	purple_connection_get_protocol_data( purple_account_get_connection( account ) );
	const char*				statusid;
	int						presence;
	char*					statusmsg1;
	char*					statusmsg2;

	/* Handle mood changes */
	if ( purple_status_type_get_primitive( purple_status_get_status_type( status ) ) == PURPLE_STATUS_MOOD ) {
		const char* moodid = purple_status_get_attr_string( status, PURPLE_MOOD_NAME );
		int mood;

		/* convert the purple mood to a mxit mood */
		mood = mxit_convert_mood( moodid );
		if ( mood < 0 ) {
			/* error, mood not found */
			purple_debug_info( MXIT_PLUGIN_ID, "Mood status NOT found! (id = %s)\n", moodid );
			return;
		}

		/* update mood state */
		mxit_send_mood( session, mood );
		return;
	}

	/* get the status id (reference: "libpurple/status.h") */
	statusid = purple_status_get_id( status );

	/* convert the purple status to a mxit status */
	presence = mxit_convert_presence( statusid );
	if ( presence < 0 ) {
		/* error, status not found */
		purple_debug_info( MXIT_PLUGIN_ID, "Presence status NOT found! (id = %s)\n", statusid );
		return;
	}

	statusmsg1 = purple_markup_strip_html( purple_status_get_attr_string( status, "message" ) );
	statusmsg2 = g_strndup( statusmsg1, CP_MAX_STATUS_MSG );

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_set_status: '%s'\n", statusmsg2 );

	/* update presence state */
	mxit_send_presence( session, presence, statusmsg2 );

	g_free( statusmsg1 );
	g_free( statusmsg2 );
}


/*------------------------------------------------------------------------
 * MXit supports messages to offline contacts.
 *
 *  @param buddy	The buddy
 */
static gboolean mxit_offline_message( const PurpleBuddy *buddy )
{
	return TRUE;
}


/*------------------------------------------------------------------------
 * Free the resources used to store a buddy.
 *
 *  @param buddy	The buddy
 */
static void mxit_free_buddy( PurpleBuddy* buddy )
{
	struct contact*		contact;

	contact = purple_buddy_get_protocol_data( buddy );
	if ( contact ) {
		g_free(contact->statusMsg);
		g_free(contact->avatarId);
		g_free(contact->msg);
		if (contact->image)
			g_object_unref(contact->image);
		g_free( contact );
	}

	purple_buddy_set_protocol_data( buddy, NULL );
}


/*------------------------------------------------------------------------
 * Periodic task called every KEEPALIVE_INTERVAL (30 sec) to to maintain
 * idle connections, timeouts and the transmission queue to the MXit server.
 *
 *  @param gc		The connection object
 */
static void mxit_keepalive( PurpleConnection *gc )
{
	struct MXitSession*	session	= purple_connection_get_protocol_data( gc );

	/* if not logged in, there is nothing to do */
	if ( !( session->flags & MXIT_FLAG_LOGGEDIN ) )
		return;

	/* pinging is only for socket connections (HTTP does polling) */
	if ( session->http )
		return;

	if ( session->last_tx <= ( mxit_now_milli() - ( MXIT_PING_INTERVAL * 1000 ) ) ) {
		/*
		 * this connection has been idle for too long, better ping
		 * the server before it kills our connection.
		 */
		mxit_send_ping( session );
	}
}


/*------------------------------------------------------------------------
 * Set or clear our Buddy icon.
 *
 *  @param gc		The connection object
 *  @param img		The buddy icon data
 */
static void
mxit_set_buddy_icon(PurpleConnection *gc, PurpleImage *img)
{
	struct MXitSession* session = purple_connection_get_protocol_data(gc);

	if (img == NULL)
		mxit_set_avatar(session, NULL, 0);
	else {
		mxit_set_avatar(session, purple_image_get_data(img),
			purple_image_get_size(img));
	}
}


/*------------------------------------------------------------------------
 * Request profile information for another MXit contact.
 *
 *  @param gc		The connection object
 *  @param who		The username of the contact.
 */
static void mxit_get_info( PurpleConnection *gc, const char *who )
{
	PurpleBuddy*			buddy;
	struct contact*			contact;
	struct MXitSession*		session			= purple_connection_get_protocol_data( gc );
	const char*				profilelist[]	= { CP_PROFILE_BIRTHDATE, CP_PROFILE_GENDER, CP_PROFILE_FULLNAME,
												CP_PROFILE_FIRSTNAME, CP_PROFILE_LASTNAME, CP_PROFILE_REGCOUNTRY, CP_PROFILE_LASTSEEN,
												CP_PROFILE_STATUS, CP_PROFILE_AVATAR, CP_PROFILE_WHEREAMI, CP_PROFILE_ABOUTME, CP_PROFILE_RELATIONSHIP };

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_get_info: '%s'\n", who );

	/* find the buddy information for this contact (reference: "libpurple/buddylist.h") */
	buddy = purple_blist_find_buddy( session->acc, who );
	if ( buddy ) {
		/* user is in our contact-list, so it's not an invite */
		contact = purple_buddy_get_protocol_data( buddy );
		if ( !contact )
			return;

		/* only MXit users have profiles */
		if ( contact->type != MXIT_TYPE_MXIT ) {
			mxit_popup( PURPLE_NOTIFY_MSG_WARNING, _( "No profile available" ), _( "This contact does not have a profile." ) );
			return;
		}
	}

	/* send profile request */
	mxit_send_extprofile_request( session, who, ARRAY_SIZE( profilelist ), profilelist );
}


/*------------------------------------------------------------------------
 * Return a list of labels to be used by Pidgin for assisting the user.
 */
static GHashTable* mxit_get_text_table( PurpleAccount* acc )
{
	GHashTable* table;

	table = g_hash_table_new( g_str_hash, g_str_equal );

	g_hash_table_insert( table, "login_label", (gpointer)_( "Your MXit ID..." ) );

	return table;
}


/*------------------------------------------------------------------------
 * Re-Invite was selected from the buddy-list menu.
 *
 *  @param node		The entry in the buddy list.
 *  @param ignored	(not used)
 */
static void mxit_reinvite( PurpleBlistNode *node, gpointer ignored )
{
	PurpleBuddy*		buddy		= (PurpleBuddy *) node;
	PurpleConnection*	gc			= purple_account_get_connection( purple_buddy_get_account( buddy ) );
	struct MXitSession*	session		= purple_connection_get_protocol_data( gc );
	struct contact*		contact;

	contact = purple_buddy_get_protocol_data( (PurpleBuddy*) node );
	if ( !contact )
		return;

	/* send a new invite */
	mxit_send_invite( session, contact->username, TRUE, contact->alias, contact->groupname, NULL );
}


/*------------------------------------------------------------------------
 * Buddy-list menu.
 *
 *  @param node		The entry in the buddy list.
 */
static GList* mxit_blist_menu( PurpleBlistNode *node )
{
	PurpleBuddy*		buddy;
	struct contact*		contact;
	GList*				m = NULL;
	PurpleMenuAction*	act;

	if ( !PURPLE_IS_BUDDY( node ) )
		return NULL;

	buddy = (PurpleBuddy *) node;
	contact = purple_buddy_get_protocol_data( buddy );
	if ( !contact )
		return NULL;

	if ( ( contact->subtype == MXIT_SUBTYPE_DELETED ) || ( contact->subtype == MXIT_SUBTYPE_REJECTED ) || ( contact->subtype == MXIT_SUBTYPE_NONE ) ) {
		/* contact is in Deleted, Rejected or None state */
		act = purple_menu_action_new( _( "Re-Invite" ), PURPLE_CALLBACK( mxit_reinvite ), NULL, NULL );
		m = g_list_append( m, act );
	}

	return m;
}


/*------------------------------------------------------------------------
 * Return Chat-room default settings.
 *
 *  @return		Chat defaults list
 */
static GHashTable *mxit_chat_info_defaults( PurpleConnection *gc, const char *chat_name )
{
    return g_hash_table_new_full( g_str_hash, g_str_equal, NULL, g_free );
}


/*------------------------------------------------------------------------
 * Send a typing indicator event.
 *
 *  @param gc		The connection object
 *  @param name		The username of the contact
 *  @param state	The typing state to be reported.
 */
static unsigned int mxit_send_typing( PurpleConnection *gc, const char *name, PurpleIMTypingState state )
{
	PurpleAccount*		account		= purple_connection_get_account( gc );
	struct MXitSession*	session		= purple_connection_get_protocol_data( gc );
	PurpleBuddy*		buddy;
	struct contact*		contact;
	gchar*				messageId	= NULL;

	/* find the buddy information for this contact (reference: "libpurple/buddylist.h") */
	buddy = purple_blist_find_buddy( account, name );
	if ( !buddy ) {
		purple_debug_warning( MXIT_PLUGIN_ID, "mxit_send_typing: unable to find the buddy '%s'\n", name );
		return 0;
	}

	contact = purple_buddy_get_protocol_data( buddy );
	if ( !contact )
		return 0;

	/* does this contact support and want typing notification? */
	if ( ! ( contact->capabilities & MXIT_PFLAG_TYPING ) )
		return 0;

	messageId = purple_uuid_random();		/* generate a unique message id */

	switch ( state ) {
		case PURPLE_IM_TYPING :		/* currently typing */
			mxit_send_msgevent( session, name, messageId, CP_MSGEVENT_TYPING );
			break;

		case PURPLE_IM_TYPED :			/* stopped typing */
		case PURPLE_IM_NOT_TYPING :	/* not typing / erased all text */
			mxit_send_msgevent( session, name, messageId, CP_MSGEVENT_STOPPED );
			break;

		default:
			break;
	}

	g_free( messageId );

	return 0;
}


/*========================================================================================================================*/

/*------------------------------------------------------------------------
 * Initializing the MXit protocol instance.
 *
 *  @param protocol	The MXit protocol
 */
static void
mxit_protocol_init( PurpleProtocol *protocol )
{
	PurpleAccountOption *option;

	protocol->id        = MXIT_PROTOCOL_ID;
	protocol->name      = MXIT_PROTOCOL_NAME;
	protocol->options   = OPT_PROTO_REGISTER_NOSCREENNAME |
	                      OPT_PROTO_UNIQUE_CHATNAME |
	                      OPT_PROTO_INVITE_MESSAGE |
	                      OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE;
	protocol->icon_spec = purple_buddy_icon_spec_new(
		"png,jpeg,bmp",										/* supported formats */
		32, 32,												/* min width & height */
		800, 800,											/* max width & height */
		CP_MAX_FILESIZE,									/* max filesize */
		PURPLE_ICON_SCALE_SEND | PURPLE_ICON_SCALE_DISPLAY	/* scaling rules */
	);

	/* Configuration options */

	/* WAP server (reference: "libpurple/accountopt.h") */
	option = purple_account_option_string_new( _( "WAP Server" ), MXIT_CONFIG_WAPSERVER, DEFAULT_WAPSITE );
	protocol->account_options = g_list_append( protocol->account_options, option );

	option = purple_account_option_bool_new( _( "Connect via HTTP" ), MXIT_CONFIG_USE_HTTP, FALSE );
	protocol->account_options = g_list_append( protocol->account_options, option );

	option = purple_account_option_bool_new( _( "Enable splash-screen popup" ), MXIT_CONFIG_SPLASHPOPUP, FALSE );
	protocol->account_options = g_list_append( protocol->account_options, option );
}


/*------------------------------------------------------------------------
 * Initializing the MXit class and interfaces.                          */


static void
mxit_protocol_class_init( PurpleProtocolClass *klass )
{
	klass->login        = mxit_login;			/* [login.c] */
	klass->close        = mxit_close;
	klass->status_types = mxit_status_types;	/* [roster.c] */
	klass->list_icon    = mxit_list_icon;
}


static void
mxit_protocol_client_iface_init( PurpleProtocolClientIface *client_iface )
{
	client_iface->get_actions            = mxit_get_actions;	/* [actions.c] */
	client_iface->list_emblem            = mxit_list_emblem;
	client_iface->status_text            = mxit_status_text;
	client_iface->tooltip_text           = mxit_tooltip;
	client_iface->blist_node_menu        = mxit_blist_menu;
	client_iface->buddy_free             = mxit_free_buddy;
	client_iface->offline_message        = mxit_offline_message;
	client_iface->get_account_text_table = mxit_get_text_table;
	client_iface->get_moods              = mxit_get_moods;
}


static void
mxit_protocol_server_iface_init( PurpleProtocolServerIface *server_iface )
{
	server_iface->register_user  = mxit_register;
	server_iface->get_info       = mxit_get_info;
	server_iface->set_status     = mxit_set_status;
	server_iface->add_buddy      = mxit_add_buddy;		/* [roster.c] */
	server_iface->remove_buddy   = mxit_remove_buddy;	/* [roster.c] */
	server_iface->keepalive      = mxit_keepalive;
	server_iface->alias_buddy    = mxit_buddy_alias;	/* [roster.c] */
	server_iface->group_buddy    = mxit_buddy_group;	/* [roster.c] */
	server_iface->rename_group   = mxit_rename_group;	/* [roster.c] */
	server_iface->set_buddy_icon = mxit_set_buddy_icon;

	/* TODO: Add function to move all contacts out of this group (cmd=30 - remove group)? */
	server_iface->remove_group       = NULL;
}


static void
mxit_protocol_im_iface_init( PurpleProtocolIMIface *im_iface )
{
	im_iface->send        = mxit_send_im;
	im_iface->send_typing = mxit_send_typing;
}


static void
mxit_protocol_chat_iface_init( PurpleProtocolChatIface *chat_iface )
{
	chat_iface->info          = mxit_chat_info;		/* [multimx.c] */
	chat_iface->info_defaults = mxit_chat_info_defaults;
	chat_iface->join          = mxit_chat_join;		/* [multimx.c] */
	chat_iface->reject        = mxit_chat_reject;	/* [multimx.c] */
	chat_iface->get_name      = mxit_chat_name;		/* [multimx.c] */
	chat_iface->invite        = mxit_chat_invite;	/* [multimx.c] */
	chat_iface->leave         = mxit_chat_leave;	/* [multimx.c] */
	chat_iface->send          = mxit_chat_send;		/* [multimx.c] */
}


static void
mxit_protocol_xfer_iface_init( PurpleProtocolXferIface *xfer_iface )
{
	xfer_iface->can_receive = mxit_xfer_enabled;	/* [filexfer.c] */
	xfer_iface->send        = mxit_xfer_tx;		/* [filexfer.c] */
	xfer_iface->new_xfer    = mxit_xfer_new;		/* [filexfer.c] */
}


PURPLE_DEFINE_TYPE_EXTENDED(
	MXitProtocol, mxit_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC( PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                   mxit_protocol_client_iface_init )

	PURPLE_IMPLEMENT_INTERFACE_STATIC( PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                   mxit_protocol_server_iface_init )

	PURPLE_IMPLEMENT_INTERFACE_STATIC( PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                   mxit_protocol_im_iface_init )

	PURPLE_IMPLEMENT_INTERFACE_STATIC( PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                   mxit_protocol_chat_iface_init )

	PURPLE_IMPLEMENT_INTERFACE_STATIC( PURPLE_TYPE_PROTOCOL_XFER_IFACE,
	                                   mxit_protocol_xfer_iface_init )
);


/*------------------------------------------------------------------------
 * Querying the MXit plugin.
 *
 *  @param error	Query error (if any)
 */
static PurplePluginInfo *
plugin_query( GError **error )
{
	const gchar * const authors[] = MXIT_PLUGIN_AUTHORS;

	return purple_plugin_info_new(
		"id",			MXIT_PLUGIN_ID,			/* plugin id (must be unique) */
		"name",			MXIT_PLUGIN_NAME,		/* plugin name (this will be displayed in the UI) */
		"version",		DISPLAY_VERSION,		/* version of the plugin */
		"category",		MXIT_PLUGIN_CATEGORY,	/* category of the plugin */
		"summary",		MXIT_PLUGIN_SUMMARY,	/* short summary of the plugin */
		"description",	MXIT_PLUGIN_DESC,		/* description of the plugin (can be long) */
		"authors",		authors,				/* plugin authors' name and email addresses */
		"website",		MXIT_PLUGIN_WWW,		/* plugin website (to find new versions and reporting of bugs) */
		"abi-version",	PURPLE_ABI_VERSION,		/* ABI version required by the plugin */
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}


/*------------------------------------------------------------------------
 * Loading the MXit plugin.
 *
 *  @param plugin	The plugin object
 *  @param error	Load error (if any)
 */
static gboolean
plugin_load( PurplePlugin *plugin, GError **error )
{
	mxit_protocol_register_type(plugin);

	my_protocol = purple_protocols_add(MXIT_TYPE_PROTOCOL, error);
	if (!my_protocol)
		return FALSE;

	return TRUE;
}


/*------------------------------------------------------------------------
 * Unloading the MXit plugin.
 *
 *  @param plugin	The plugin object
 *  @param error	Unload error (if any)
 */
static gboolean
plugin_unload( PurplePlugin *plugin, GError **error )
{
	if (!purple_protocols_remove(my_protocol, error))
		return FALSE;

	return TRUE;
}


PURPLE_PLUGIN_INIT( mxit, plugin_query, plugin_load, plugin_unload );
