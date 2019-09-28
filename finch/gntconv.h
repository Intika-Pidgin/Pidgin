/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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

#ifndef _GNT_CONV_H
#define _GNT_CONV_H
/**
 * SECTION:gntconv
 * @section_id: finch-gntconv
 * @short_description: <filename>gntconv.h</filename>
 * @title: Conversation API
 */

#include <gnt.h>
#include <gntwidget.h>
#include <gntmenuitem.h>

#include "conversation.h"

/* Grabs the conv out of a PurpleConverstation */
#define FINCH_CONV(conv) ((FinchConv *)purple_conversation_get_ui_data(conv))

/***************************************************************************
 * GNT Conversations API
 ***************************************************************************/

typedef struct _FinchConv FinchConv;
typedef struct _FinchConvChat FinchConvChat;
typedef struct _FinchConvIm FinchConvIm;

/**
 * FinchConversationFlag:
 * @FINCH_CONV_NO_SOUND: A flag to mute a conversation.
 *
 * Flags that can be set for each conversation.
 */
typedef enum
{
	FINCH_CONV_NO_SOUND     = 1 << 0,
} FinchConversationFlag;

/**
 * FinchConv:
 * @list: A list of conversations being displayed in this window.
 * @active_conv: The active conversation.
 * @window: The #GntWindow for the conversation.
 * @entry: The #GntEntry for input.
 * @tv: The #GntTextView that displays the history.
 * @menu: The menu for the conversation.
 * @info: The info widget that shows the information about the conversation.
 * @plugins: The #GntMenuItem for plugins.
 * @flags: The flags for the conversation.
 *
 * A Finch conversation.
 */
struct _FinchConv
{
	GList *list;
	PurpleConversation *active_conv;

	GntWidget *window;        /* the container */
	GntWidget *entry;         /* entry */
	GntWidget *tv;            /* text-view */
	GntWidget *menu;
	GntWidget *info;
	GntMenuItem *plugins;
	FinchConversationFlag flags;

	union
	{
		FinchConvChat *chat;
		FinchConvIm *im;
	} u;
};

/**
 * FinchConvChat:
 * @userlist: The widget that displays the users in the chat.
 *
 * The chat specific implementation for a conversation.
 */
struct _FinchConvChat
{
	GntWidget *userlist;       /* the userlist */

	/*< private >*/
	void *finch_reserved1;
	void *finch_reserved2;
	void *finch_reserved3;
	void *finch_reserved4;
};

/**
 * FinchConvIm:
 * @sendto: The sendto widget which allows the user to select who they're
 *          messaging.
 * @e2ee_menu: The end-to-end-encryption widget which lets the user configure
 *             the encryption.
 *
 * The instant message implementation for a conversation.
 */
struct _FinchConvIm
{
	GntMenuItem *sendto;
	GntMenuItem *e2ee_menu;

	/*< private >*/
	void *finch_reserved1;
	void *finch_reserved2;
	void *finch_reserved3;
	void *finch_reserved4;
};

/**
 * finch_conv_get_ui_ops:
 *
 * Get the ui-functions.
 *
 * Returns: The PurpleConversationUiOps populated with the appropriate functions.
 */
PurpleConversationUiOps *finch_conv_get_ui_ops(void);

/**
 * finch_conversation_init:
 *
 * Perform the necessary initializations.
 */
void finch_conversation_init(void);

/**
 * finch_conversation_uninit:
 *
 * Perform the necessary uninitializations.
 */
void finch_conversation_uninit(void);

/**
 * finch_conversation_set_active:
 * @conv: The conversation to make active.
 *
 * Set a conversation as active in a contactized conversation
 */
void finch_conversation_set_active(PurpleConversation *conv);

/**
 * finch_conversation_set_info_widget:
 * @conv:   The conversation.
 * @widget: The widget containing the information. If %NULL,
 *               the current information widget is removed.
 *
 * Sets the information widget for the conversation window.
 */
void finch_conversation_set_info_widget(PurpleConversation *conv, GntWidget *widget);

#endif
