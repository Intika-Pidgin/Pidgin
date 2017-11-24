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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_SOUND_H_
#define _PURPLE_SOUND_H_
/**
 * SECTION:sound
 * @section_id: libpurple-sound
 * @short_description: Sound subsystem definition.
 * @title: Sound API
 * @see_also: <link linkend="chapter-signals-sound">Sound signals</link>
 */

#include "account.h"

#define PURPLE_TYPE_SOUND_UI_OPS (purple_sound_ui_ops_get_type())

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/
typedef struct _PurpleSoundUiOps PurpleSoundUiOps;

/**
 * PurpleSoundStatus:
 * @PURPLE_SOUND_STATUS_AVAILABLE:	Only play sound when Status is Available.
 * @PURPLE_SOUND_STATUS_AWAY:		Only play sound when Status is Not Available.
 * @PURPLE_SOUND_STATUS_ALWAYS:		Always play sound.
 *
 * A preference option on when to play sounds depending on the current status
 */
typedef enum {
	PURPLE_SOUND_STATUS_AVAILABLE = 1,
	PURPLE_SOUND_STATUS_AWAY,
	PURPLE_SOUND_STATUS_ALWAYS,
} PurpleSoundStatus;

/**
 * PurpleSoundEventID:
 * @PURPLE_SOUND_BUDDY_ARRIVE:   Buddy signs on.
 * @PURPLE_SOUND_BUDDY_LEAVE:    Buddy signs off.
 * @PURPLE_SOUND_RECEIVE:        Receive an IM.
 * @PURPLE_SOUND_FIRST_RECEIVE:  Receive an IM that starts a conv.
 * @PURPLE_SOUND_SEND:           Send an IM.
 * @PURPLE_SOUND_CHAT_JOIN:      Someone joins a chat.
 * @PURPLE_SOUND_CHAT_LEAVE:     Someone leaves a chat.
 * @PURPLE_SOUND_CHAT_YOU_SAY:   You say something in a chat.
 * @PURPLE_SOUND_CHAT_SAY:       Someone else says somthing in a chat.
 * @PURPLE_SOUND_POUNCE_DEFAULT: Default sound for a buddy pounce.
 * @PURPLE_SOUND_CHAT_NICK:      Someone says your name in a chat.
 * @PURPLE_SOUND_GOT_ATTENTION:  Got an attention.
 * @PURPLE_NUM_SOUNDS:           Total number of sounds.
 *
 * A type of sound.
 */
typedef enum
{
	PURPLE_SOUND_BUDDY_ARRIVE = 0,
	PURPLE_SOUND_BUDDY_LEAVE,
	PURPLE_SOUND_RECEIVE,
	PURPLE_SOUND_FIRST_RECEIVE,
	PURPLE_SOUND_SEND,
	PURPLE_SOUND_CHAT_JOIN,
	PURPLE_SOUND_CHAT_LEAVE,
	PURPLE_SOUND_CHAT_YOU_SAY,
	PURPLE_SOUND_CHAT_SAY,
	PURPLE_SOUND_POUNCE_DEFAULT,
	PURPLE_SOUND_CHAT_NICK,
	PURPLE_SOUND_GOT_ATTENTION,
	PURPLE_NUM_SOUNDS

} PurpleSoundEventID;

/**
 * PurpleSoundUiOps:
 * @init: Called when the UI should initialize sound
 * @uninit: Called when the UI should teardown sound
 * @play_file: Called when a file should be played.
 *             <sbr/>@filename: The filename to play
 * @play_event: Called when a sound event should be played.
 *              <sbr/>@event: The #@PurpleSoundEventID to play
 *
 * Operations used by the core to request that particular sound files, or the
 * sound associated with a particular event, should be played.
 */
struct _PurpleSoundUiOps
{
	void (*init)(void);
	void (*uninit)(void);
	void (*play_file)(const char *filename);
	void (*play_event)(PurpleSoundEventID event);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* Sound API                                                              */
/**************************************************************************/

/**
 * purple_sound_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleSoundUiOps boxed structure.
 */
GType purple_sound_ui_ops_get_type(void);

/**
 * purple_sound_play_file:
 * @filename: The file to play.
 * @account: (nullable): The account that this sound is associated with, or
 *        NULL if the sound is not associated with any specific
 *        account.  This is needed for the "sounds while away?"
 *        preference to work correctly.
 *
 * Plays the specified sound file.
 */
void purple_sound_play_file(const char *filename, const PurpleAccount *account);

/**
 * purple_sound_play_event:
 * @event: The event.
 * @account: (nullable): The account that this sound is associated with, or
 *        NULL if the sound is not associated with any specific
 *        account.  This is needed for the "sounds while away?"
 *        preference to work correctly.
 *
 * Plays the sound associated with the specified event.
 */
void purple_sound_play_event(PurpleSoundEventID event, const PurpleAccount *account);

/**
 * purple_sound_set_ui_ops:
 * @ops: The UI sound operations structure.
 *
 * Sets the UI sound operations
 */
void purple_sound_set_ui_ops(PurpleSoundUiOps *ops);

/**
 * purple_sound_get_ui_ops:
 *
 * Gets the UI sound operations
 *
 * Returns: The UI sound operations structure.
 */
PurpleSoundUiOps *purple_sound_get_ui_ops(void);

/**
 * purple_sound_init:
 *
 * Initializes the sound subsystem
 */
void purple_sound_init(void);

/**
 * purple_sound_uninit:
 *
 * Shuts down the sound subsystem
 */
void purple_sound_uninit(void);

/**
 * purple_sounds_get_handle:
 *
 * Returns the sound subsystem handle.
 *
 * Returns: The sound subsystem handle.
 */
void *purple_sounds_get_handle(void);

G_END_DECLS

#endif /* _PURPLE_SOUND_H_ */
