/*
 *					MXit Protocol libPurple Plugin
 *
 *			-- user roster management (mxit contacts) --
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

#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>

#include	"purple.h"

#include	"protocol.h"
#include	"mxit.h"
#include	"roster.h"


struct contact_invite {
	struct MXitSession*		session;		/* MXit session object */
	struct contact*			contact;		/* The contact performing the invite */
};


/*========================================================================================================================
 * Presence / Status
 */

/* statuses (reference: libpurple/status.h) */
static struct status
{
	PurpleStatusPrimitive	primative;
	int						mxit;
	const char*				id;
	const char*				name;
} const mxit_statuses[] = {
		/*	primative,						no,							id,			name					*/
		{	PURPLE_STATUS_OFFLINE,			MXIT_PRESENCE_OFFLINE,		"offline",	N_( "Offline" )			},	/* 0 */
		{	PURPLE_STATUS_AVAILABLE,		MXIT_PRESENCE_ONLINE,		"online",	N_( "Available" )		},	/* 1 */
		{	PURPLE_STATUS_AWAY,				MXIT_PRESENCE_AWAY,			"away",		N_( "Away" )			},	/* 2 */
		{	PURPLE_STATUS_AVAILABLE,		MXIT_PRESENCE_AVAILABLE,	"chat",		N_( "Chatty" )			},	/* 3 */
		{	PURPLE_STATUS_UNAVAILABLE,		MXIT_PRESENCE_DND,			"dnd",		N_( "Do Not Disturb" )	}	/* 4 */
};


/*------------------------------------------------------------------------
 * Return list of supported statuses. (see status.h)
 *
 *  @param account	The MXit account object
 *  @return			List of PurpleStatusType
 */
GList* mxit_status_types( PurpleAccount* account )
{
	GList*				statuslist	= NULL;
	PurpleStatusType*	type;
	unsigned int		i;

	for ( i = 0; i < ARRAY_SIZE( mxit_statuses ); i++ ) {
		const struct status* status = &mxit_statuses[i];

		/* add mxit status (reference: "libpurple/status.h") */
		type = purple_status_type_new_with_attrs( status->primative, status->id, _( status->name ), TRUE, TRUE, FALSE,
					"message", _( "Message" ), purple_value_new( PURPLE_TYPE_STRING ),
					NULL );

		statuslist = g_list_append( statuslist, type );
	}

	return statuslist;
}


/*------------------------------------------------------------------------
 * Returns the MXit presence code, given the unique status ID.
 *
 *  @param id		The status ID
 *  @return			The MXit presence code
 */
int mxit_convert_presence( const char* id )
{
	unsigned int	i;

	for ( i = 0; i < ARRAY_SIZE( mxit_statuses ); i++ ) {
		if ( strcmp( mxit_statuses[i].id, id ) == 0 )	/* status found! */
			return mxit_statuses[i].mxit;
	}

	return -1;
}


/*------------------------------------------------------------------------
 * Returns the MXit presence as a string, given the MXit presence ID.
 *
 *  @param no		The MXit presence I (see above)
 *  @return			The presence as a text string
 */
const char* mxit_convert_presence_to_name( short no )
{
	unsigned int	i;

	for ( i = 0; i < ARRAY_SIZE( mxit_statuses ); i++ ) {
		if ( mxit_statuses[i].mxit == no )				/* status found! */
			return _( mxit_statuses[i].name );
	}

	return "";
}


/*========================================================================================================================
 * Moods
 */

/*------------------------------------------------------------------------
 * Returns the MXit mood as a string, given the MXit mood's ID.
 *
 *  @param id		The MXit mood ID (see roster.h)
 *  @return			The mood as a text string
 */
const char* mxit_convert_mood_to_name( short id )
{
	switch ( id ) {
		case MXIT_MOOD_ANGRY :
				return _( "Angry" );
		case MXIT_MOOD_EXCITED :
				return _( "Excited" );
		case MXIT_MOOD_GRUMPY :
				return _( "Grumpy" );
		case MXIT_MOOD_HAPPY :
				return _( "Happy" );
		case MXIT_MOOD_INLOVE :
				return _( "In Love" );
		case MXIT_MOOD_INVINCIBLE :
				return _( "Invincible" );
		case MXIT_MOOD_SAD :
				return _( "Sad" );
		case MXIT_MOOD_HOT :
				return _( "Hot" );
		case MXIT_MOOD_SICK :
				return _( "Sick" );
		case MXIT_MOOD_SLEEPY :
				return _( "Sleepy" );
		case MXIT_MOOD_NONE :
		default :
				return "";
	}
}


/*========================================================================================================================
 * Subscription Types
 */
 
/*------------------------------------------------------------------------
 * Returns a Contact subscription type as a string.
 *
 *  @param subtype	The subscription type
 *  @return			The subscription type as a text string
 */
const char* mxit_convert_subtype_to_name( short subtype )
{
	switch ( subtype ) {
		case MXIT_SUBTYPE_BOTH :
				return _( "Both" );
		case MXIT_SUBTYPE_PENDING :
				return _( "Pending" );
		case MXIT_SUBTYPE_ASK :
				return _( "Invited" );
		case MXIT_SUBTYPE_REJECTED :
				return _( "Rejected" );
		case MXIT_SUBTYPE_DELETED :
				return _( "Deleted" );
		case MXIT_SUBTYPE_NONE :
				return _( "None" );
		default :
				return "";
	}
}


/*========================================================================================================================
 * Calls from the MXit Protocol layer
 */

#if	0
/*------------------------------------------------------------------------
 * Dump a contact's info the the debug console.
 *
 *  @param contact		The contact
 */
static void dump_contact( struct contact* contact )
{
	purple_debug_info( MXIT_PLUGIN_ID, "CONTACT: name='%s', alias='%s', group='%s', type='%i', presence='%i', mood='%i'\n",
						contact->username, contact->alias, contact->groupname, contact->type, contact->presence, contact->mood );
}
#endif


#if	0
/*------------------------------------------------------------------------
 * Move a buddy from one group to another
 *
 * @param buddy		the buddy to move between groups
 * @param group		the new group to move the buddy to
 */
static PurpleBuddy* mxit_update_buddy_group( struct MXitSession* session, PurpleBuddy* buddy, PurpleGroup* group )
{
	struct contact*		contact			= NULL;
	PurpleGroup*		current_group	= purple_buddy_get_group( buddy );
	PurpleBuddy*		newbuddy		= NULL;

	/* make sure the groups actually differs */
	if ( strcmp( current_group->name, group->name ) != 0 ) {
		/* groupnames does not match, so we need to make the update */

		purple_debug_info( MXIT_PLUGIN_ID, "Moving '%s' from group '%s' to '%s'\n", buddy->alias, current_group->name, group->name );

		/*
		 * XXX: libPurple does not currently provide an API to change or rename the group name
		 * for a specific buddy. One option is to remove the buddy from the list and re-adding
		 * him in the new group, but by doing that makes the buddy go offline and then online
		 * again. This is really not ideal and very iretating, but how else then?
		 */

		/* create new buddy */
		newbuddy = purple_buddy_new( session->acc, buddy->name, buddy->alias );
		newbuddy->proto_data = buddy->proto_data;
		buddy->proto_data = NULL;

		/* remove the buddy */
		purple_blist_remove_buddy( buddy );

		/* add buddy */
		purple_blist_add_buddy( newbuddy, NULL, group, NULL );

		/* now re-instate his presence again */
		contact = newbuddy->proto_data;
		if ( contact ) {

			/* update the buddy's status (reference: "libpurple/prpl.h") */
			if ( contact->statusMsg )
				purple_prpl_got_user_status( session->acc, newbuddy->name, mxit_statuses[contact->presence].id, "message", contact->statusMsg, NULL );
			else
				purple_prpl_got_user_status( session->acc, newbuddy->name, mxit_statuses[contact->presence].id, NULL );

			/* update avatar */
			if ( contact->avatarId ) {
				mxit_get_avatar( session, newbuddy->name, contact->avatarId );
				g_free( contact->avatarId );
				contact->avatarId = NULL;
			}
		}

		return newbuddy;
	}
	else
		return buddy;
}
#endif


/*------------------------------------------------------------------------
 * A contact update packet was received from the MXit server, so update the buddy's
 * information.
 *
 *  @param session		The MXit session object
 *  @param contact		The contact
 */
void mxit_update_contact( struct MXitSession* session, struct contact* contact )
{
	PurpleBuddy*		buddy	= NULL;
	PurpleGroup*		group	= NULL;
	const char*			id		= NULL;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_update_contact: user='%s' alias='%s' group='%s'\n", contact->username, contact->alias, contact->groupname );

	/*
	 * libPurple requires all contacts to be in a group.
	 * So if this MXit contact isn't in a group, pretend it is.
	 */
	if ( *contact->groupname == '\0' ) {
		strcpy( contact->groupname, MXIT_DEFAULT_GROUP );
	}

	/* find or create a group for this contact */
	group = purple_find_group( contact->groupname );
	if ( !group )
		group = purple_group_new( contact->groupname );

	/* see if the buddy is not in the group already */
	buddy = purple_find_buddy_in_group( session->acc, contact->username, group );
	if ( !buddy ) {
		/* buddy not found in the group */

		/* lets try finding him in all groups */
		buddy = purple_find_buddy( session->acc, contact->username );
		if ( buddy ) {
			/* ok, so we found him in another group. to switch him between groups we must delete him and add him again. */
			purple_blist_remove_buddy( buddy );
			buddy = NULL;
		}

		/* create new buddy */
		buddy = purple_buddy_new( session->acc, contact->username, contact->alias );
		buddy->proto_data = contact;

		/* add new buddy to list */
		purple_blist_add_buddy( buddy, NULL, group, NULL );
	}
	else {
		/* buddy was found in the group */

		/* now update the buddy's alias */
		purple_blist_alias_buddy( buddy, contact->alias );

		/* replace the buddy's contact struct */
		if ( buddy->proto_data )
			free( buddy->proto_data );
		buddy->proto_data = contact;
	}

	/* load buddy's avatar id */
	id = purple_buddy_icons_get_checksum_for_user( buddy );
	if ( id )
		contact->avatarId = g_strdup( id );
	else
		contact->avatarId = NULL;

	/* update the buddy's status (reference: "libpurple/prpl.h") */
	purple_prpl_got_user_status( session->acc, contact->username, mxit_statuses[contact->presence].id, NULL );
}


/*------------------------------------------------------------------------
 * A presence update packet was received from the MXit server, so update the buddy's
 * information.
 *
 *  @param session		The MXit session object
 *  @param username		The contact which presence to update
 *  @param presence		The new presence state for the contact
 *  @param mood			The new mood for the contact
 *  @param customMood	The custom mood identifier
 *  @param statusMsg	This is the contact's status message
 *  @param avatarId		This is the contact's avatar id
 */
void mxit_update_buddy_presence( struct MXitSession* session, const char* username, short presence, short mood, const char* customMood, const char* statusMsg, const char* avatarId )
{
	PurpleBuddy*		buddy	= NULL;
	struct contact*		contact	= NULL;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_update_buddy_presence: user='%s' presence=%i mood=%i customMood='%s' statusMsg='%s' avatar='%s'\n",
		username, presence, mood, customMood, statusMsg, avatarId );

	if ( ( presence < MXIT_PRESENCE_OFFLINE ) || ( presence > MXIT_PRESENCE_DND ) ) {
		purple_debug_info( MXIT_PLUGIN_ID, "mxit_update_buddy_presence: invalid presence state %i\n", presence );
		return;		/* ignore packet */
	}

	/* find the buddy information for this contact (reference: "libpurple/blist.h") */
	buddy = purple_find_buddy( session->acc, username );
	if ( !buddy ) {
		purple_debug_warning( MXIT_PLUGIN_ID, "mxit_update_buddy_presence: unable to find the buddy '%s'\n", username );
		return;
	}

	contact = buddy->proto_data;
	if ( !contact )
		return;

	contact->presence = presence;	
	contact->mood = mood;

	g_strlcpy( contact->customMood, customMood, sizeof( contact->customMood ) );
	// TODO: Download custom mood frame.

	/* update status message */
	if ( contact->statusMsg ) {
		g_free( contact->statusMsg );
		contact->statusMsg = NULL;
	}
	if ( statusMsg[0] != '\0' )
		contact->statusMsg = g_markup_escape_text( statusMsg, -1 );

	/* update avatarId */
	if ( ( contact->avatarId ) && ( g_ascii_strcasecmp( contact->avatarId, avatarId ) == 0 ) ) {
		/*  avatar has not changed - do nothing */
	}
	else if ( avatarId[0] != '\0' ) {		/* avatar has changed */
		if ( contact->avatarId )
			g_free( contact->avatarId );
		contact->avatarId = g_strdup( avatarId );

		/* Send request to download new avatar image */
		mxit_get_avatar( session, username, avatarId );
	}
	else		/* clear current avatar */
		purple_buddy_icons_set_for_user( session->acc, username, NULL, 0, NULL );

	/* update the buddy's status (reference: "libpurple/prpl.h") */
	if ( contact->statusMsg )
		purple_prpl_got_user_status( session->acc, username, mxit_statuses[contact->presence].id, "message", contact->statusMsg, NULL );
	else
		purple_prpl_got_user_status( session->acc, username, mxit_statuses[contact->presence].id, NULL );
}


/*------------------------------------------------------------------------
 * update the blist cached by libPurple. We need to do this to keep
 * libPurple and MXit's rosters in sync with each other.
 *
 * @param session		The MXit session object
 */
void mxit_update_blist( struct MXitSession* session )
{
	PurpleBuddy*	buddy	= NULL;
	GSList*			list	= NULL;
	unsigned int	i;

	/* remove all buddies we did not receive a roster update for.
	 * these contacts must have been removed from another client */
	list = purple_find_buddies( session->acc, NULL );

	for ( i = 0; i < g_slist_length( list ); i++ ) {
		buddy = g_slist_nth_data( list, i );

		if ( !buddy->proto_data ) {
			/* this buddy should be removed, because we did not receive him in our roster update from MXit */
			purple_debug_info( MXIT_PLUGIN_ID, "Removed 'old' buddy from the blist '%s' (%s)\n", buddy->alias, buddy->name );
			purple_blist_remove_buddy( buddy );
		}
	}

	/* tell the UI to update the blist */
	purple_blist_add_account( session->acc );
}


/*------------------------------------------------------------------------
 * The user authorized an invite (subscription request).
 *
 *  @param user_data	Object associated with the invite
 */
static void mxit_cb_buddy_auth( gpointer user_data )
{
	struct contact_invite*	invite	= (struct contact_invite*) user_data;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_cb_buddy_auth '%s'\n", invite->contact->username );

	/* send a allow subscription packet to MXit */
	mxit_send_allow_sub( invite->session, invite->contact->username, invite->contact->alias );

	/* freeup invite object */
	if ( invite->contact->msg )
		g_free( invite->contact->msg );
	g_free( invite->contact );
	g_free( invite );
}


/*------------------------------------------------------------------------
 * The user rejected an invite (subscription request).
 *
 *  @param user_data	Object associated with the invite
 */
static void mxit_cb_buddy_deny( gpointer user_data )
{
	struct contact_invite*	invite	= (struct contact_invite*) user_data;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_cb_buddy_deny '%s'\n", invite->contact->username );

	/* send a deny subscription packet to MXit */
	mxit_send_deny_sub( invite->session, invite->contact->username );

	/* freeup invite object */
	if ( invite->contact->msg )
		g_free( invite->contact->msg );
	g_free( invite->contact );
	g_free( invite );
}


/*------------------------------------------------------------------------
 * A new subscription request packet was received from the MXit server.
 * Prompt user to accept or reject it.
 *
 *  @param session		The MXit session object
 *  @param contact		The contact performing the invite
 */
void mxit_new_subscription( struct MXitSession* session, struct contact* contact )
{
	struct contact_invite*	invite;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_new_subscription from '%s' (%s)\n", contact->username, contact->alias );

	invite = g_new0( struct contact_invite, 1 );
	invite->session = session;
	invite->contact = contact;

	/* (reference: "libpurple/account.h") */
	purple_account_request_authorization( session->acc, contact->username, NULL, contact->alias, contact->msg, FALSE, mxit_cb_buddy_auth, mxit_cb_buddy_deny, invite );
}


/*------------------------------------------------------------------------
 * Return TRUE if this is a MXit Chatroom contact.
 *
 *  @param session		The MXit session object
 *  @param username		The username of the contact
 */
gboolean is_mxit_chatroom_contact( struct MXitSession* session, const char* username )
{
	PurpleBuddy*		buddy;
	struct contact*		contact	= NULL;

	/* find the buddy */
	buddy = purple_find_buddy( session->acc, username );
	if ( !buddy ) {
		purple_debug_warning( MXIT_PLUGIN_ID, "is_mxit_chatroom_contact: unable to find the buddy '%s'\n", username );
		return FALSE;
	}

	contact = buddy->proto_data;
	if ( !contact )
		return FALSE;

	return ( contact->type == MXIT_TYPE_CHATROOM );
}


/*========================================================================================================================
 * Callbacks from libpurple
 */

/*------------------------------------------------------------------------
 * The user has added a buddy to the list, so send an invite request.
 *
 *  @param gc		The connection object
 *  @param buddy	The new buddy
 *  @param group	The group of the new buddy
 */
void mxit_add_buddy( PurpleConnection* gc, PurpleBuddy* buddy, PurpleGroup* group )
{
	struct MXitSession*	session	= (struct MXitSession*) gc->proto_data;
	GSList*				list	= NULL;
	PurpleBuddy*		mxbuddy	= NULL;
	unsigned int		i;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_add_buddy '%s' (group='%s')\n", buddy->name, group->name );

	list = purple_find_buddies( session->acc, buddy->name );
	if ( g_slist_length( list ) == 1 ) {
		purple_debug_info( MXIT_PLUGIN_ID, "mxit_add_buddy (scenario 1) (list:%i)\n", g_slist_length( list ) );
		/*
		 * we only send an invite to MXit when the user is not already inside our
		 * blist.  this is done because purple does an add_buddy() call when
		 * you accept an invite.  so in that case the user is already
		 * in our blist and ready to be chatted to.
		 */
		mxit_send_invite( session, buddy->name, buddy->alias, group->name );
	}
	else {
		purple_debug_info( MXIT_PLUGIN_ID, "mxit_add_buddy (scenario 2) (list:%i)\n", g_slist_length( list ) );
		/*
		 * we already have the buddy in our list, so we will only update
		 * his information here and not send another invite message
		 */

		/* find the correct buddy */
		for ( i = 0; i < g_slist_length( list ); i++ ) {
			mxbuddy = g_slist_nth_data( list, i );

			if ( mxbuddy->proto_data != NULL ) {
				/* this is our REAL MXit buddy! */

				/* now update the buddy's alias */
				purple_blist_alias_buddy( mxbuddy, buddy->alias );

				/* now update the buddy's group */
//				mxbuddy = mxit_update_buddy_group( session, mxbuddy, group );

				/* send the update to the MXit server */
				mxit_send_update_contact( session, mxbuddy->name, mxbuddy->alias, group->name );
			}
		}
	}

	/*
	 * we remove the buddy here from the buddy list because the MXit server
	 * will send us a proper contact update packet if this succeeds.  now
	 * we do not have to worry about error handling in case of adding an
	 * invalid contact.  so the user will still see the contact as offline
	 * until he eventually accepts the invite.
	 */
	purple_blist_remove_buddy( buddy );

	g_slist_free( list );
}


/*------------------------------------------------------------------------
 * The user has removed a buddy from the list.
 *
 *  @param gc		The connection object
 *  @param buddy	The buddy being removed
 *  @param group	The group the buddy was in
 */
void mxit_remove_buddy( PurpleConnection* gc, PurpleBuddy* buddy, PurpleGroup* group )
{
	struct MXitSession*	session	= (struct MXitSession*) gc->proto_data;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_remove_buddy '%s'\n", buddy->name );

	mxit_send_remove( session, buddy->name );
}


/*------------------------------------------------------------------------
 * The user changed the buddy's alias.
 *
 *  @param gc		The connection object
 *  @param who		The username of the buddy
 *  @param alias	The new alias
 */
void mxit_buddy_alias( PurpleConnection* gc, const char* who, const char* alias )
{
	struct MXitSession*	session	= (struct MXitSession*) gc->proto_data;
	PurpleBuddy*		buddy	= NULL;
	PurpleGroup*		group	= NULL;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_buddy_alias '%s' to '%s\n", who, alias );

	/* find the buddy */
	buddy = purple_find_buddy( session->acc, who );
	if ( !buddy ) {
		purple_debug_warning( MXIT_PLUGIN_ID, "mxit_buddy_alias: unable to find the buddy '%s'\n", who );
		return;
	}

	/* find buddy group */
	group = purple_buddy_get_group( buddy );
	if ( !group ) {
		purple_debug_warning( MXIT_PLUGIN_ID, "mxit_buddy_alias: unable to find the group for buddy '%s'\n", who );
		return;
	}

	mxit_send_update_contact( session, who, alias, group->name );
}


/*------------------------------------------------------------------------
 * The user changed the group for a single buddy.
 *
 *  @param gc			The connection object
 *  @param who			The username of the buddy
 *  @param old_group	The old group's name
 *  @param new_group	The new group's name
 */
void mxit_buddy_group( PurpleConnection* gc, const char* who, const char* old_group, const char* new_group )
{
	struct MXitSession*	session	= (struct MXitSession*) gc->proto_data;
	PurpleBuddy*		buddy	= NULL;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_buddy_group from '%s' to '%s'\n", old_group, new_group );

	/* find the buddy */
	buddy = purple_find_buddy( session->acc, who );
	if ( !buddy ) {
		purple_debug_warning( MXIT_PLUGIN_ID, "mxit_buddy_group: unable to find the buddy '%s'\n", who );
		return;
	}

	mxit_send_update_contact( session, who, buddy->alias, new_group );
}


/*------------------------------------------------------------------------
 * The user has selected to rename a group, so update all contacts in that
 * group.
 *
 *  @param gc				The connection object
 *  @param old_name			The old group name
 *  @param group			The updated group object
 *  @param moved_buddies	The buddies affected by the rename
 */
void mxit_rename_group( PurpleConnection* gc, const char* old_name, PurpleGroup* group, GList* moved_buddies )
{
	struct MXitSession*	session	= (struct MXitSession*) gc->proto_data;
	PurpleBuddy*		buddy	= NULL;
	GList*				item	= NULL;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_rename_group from '%s' to '%s\n", old_name, group->name );

	//  TODO: Might be more efficient to use the "rename group" command (cmd=29).

	/* loop through all the contacts in the group and send updates */
	item = moved_buddies;
	while ( item ) {
		buddy = item->data;
		mxit_send_update_contact( session, buddy->name, buddy->alias, group->name );
		item = g_list_next( item );
	}
}

