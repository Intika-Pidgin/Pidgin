/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#ifndef _PIDGIN_STOCK_H_
#define _PIDGIN_STOCK_H_
/**
 * SECTION:pidginstock
 * @section_id: pidgin-pidginstock
 * @short_description: <filename>pidginstock.h</filename>
 * @title: Stock Resources
 */

#include <gtk/gtk.h>
#include "gtkstatus-icon-theme.h"

/**************************************************************************/
/* Stock images                                                           */
/**************************************************************************/

#define PIDGIN_STOCK_ACTION          "pidgin-action"
#define PIDGIN_STOCK_ALIAS           "pidgin-alias"
#define PIDGIN_STOCK_AWAY            "pidgin-away"
#define PIDGIN_STOCK_CHAT            "pidgin-chat"
#define PIDGIN_STOCK_CLEAR           "pidgin-clear"
#define PIDGIN_STOCK_CLOSE_TABS      "pidgin-close-tab"
#define PIDGIN_STOCK_DEBUG           "pidgin-debug"
#define PIDGIN_STOCK_DISCONNECT      "pidgin-disconnect"
#define PIDGIN_STOCK_DOWNLOAD        "pidgin-download"
#define PIDGIN_STOCK_EDIT            "pidgin-edit"
#define PIDGIN_STOCK_FGCOLOR         "pidgin-fgcolor"
#define PIDGIN_STOCK_FILE_CANCELLED  "pidgin-file-cancelled"
#define PIDGIN_STOCK_FILE_DONE       "pidgin-file-done"
#define PIDGIN_STOCK_IGNORE          "pidgin-ignore"
#define PIDGIN_STOCK_INFO            "pidgin-info"
#define PIDGIN_STOCK_INVITE          "pidgin-invite"
#define PIDGIN_STOCK_MODIFY          "pidgin-modify"
#define PIDGIN_STOCK_ADD             "pidgin-add"
#define PIDGIN_STOCK_OPEN_MAIL       "pidgin-stock-open-mail"
#define PIDGIN_STOCK_PAUSE           "pidgin-pause"
#define PIDGIN_STOCK_POUNCE          "pidgin-pounce"
#define PIDGIN_STOCK_SIGN_OFF        "pidgin-sign-off"
#define PIDGIN_STOCK_SIGN_ON         "pidgin-sign-on"
#define PIDGIN_STOCK_TEXT_NORMAL     "pidgin-text-normal"
#define PIDGIN_STOCK_TYPED           "pidgin-typed"
#define PIDGIN_STOCK_UPLOAD          "pidgin-upload"
#define PIDGIN_STOCK_NEXT            "pidgin-next"

/* Status icons */
#define PIDGIN_STOCK_STATUS_AVAILABLE  "pidgin-status-available"
#define PIDGIN_STOCK_STATUS_AVAILABLE_I "pidgin-status-available-i"
#define PIDGIN_STOCK_STATUS_AWAY       "pidgin-status-away"
#define PIDGIN_STOCK_STATUS_AWAY_I     "pidgin-status-away-i"
#define PIDGIN_STOCK_STATUS_BUSY       "pidgin-status-busy"
#define PIDGIN_STOCK_STATUS_BUSY_I     "pidgin-status-busy-i"
#define PIDGIN_STOCK_STATUS_CHAT       "pidgin-status-chat"
#define PIDGIN_STOCK_STATUS_INVISIBLE  "pidgin-status-invisible"
#define PIDGIN_STOCK_STATUS_XA         "pidgin-status-xa"
#define PIDGIN_STOCK_STATUS_XA_I       "pidgin-status-xa-i"
#define PIDGIN_STOCK_STATUS_LOGIN      "pidgin-status-login"
#define PIDGIN_STOCK_STATUS_LOGOUT     "pidgin-status-logout"
#define PIDGIN_STOCK_STATUS_OFFLINE    "pidgin-status-offline"
#define PIDGIN_STOCK_STATUS_OFFLINE_I  "pidgin-status-offline"
#define PIDGIN_STOCK_STATUS_PERSON     "pidgin-status-person"
#define PIDGIN_STOCK_STATUS_MESSAGE    "pidgin-status-message"

/* Chat room emblems */
#define PIDGIN_STOCK_STATUS_IGNORED    "pidgin-status-ignored"
#define PIDGIN_STOCK_STATUS_FOUNDER    "pidgin-status-founder"
#define PIDGIN_STOCK_STATUS_OPERATOR   "pidgin-status-operator"
#define PIDGIN_STOCK_STATUS_HALFOP     "pidgin-status-halfop"
#define PIDGIN_STOCK_STATUS_VOICE      "pidgin-status-voice"

/* Dialog icons */
#define PIDGIN_STOCK_DIALOG_AUTH	"pidgin-dialog-auth"
#define PIDGIN_STOCK_DIALOG_ERROR	"pidgin-dialog-error"
#define PIDGIN_STOCK_DIALOG_INFO	"pidgin-dialog-info"
#define PIDGIN_STOCK_DIALOG_MAIL	"pidgin-dialog-mail"
#define PIDGIN_STOCK_DIALOG_QUESTION	"pidgin-dialog-question"
#define PIDGIN_STOCK_DIALOG_COOL	"pidgin-dialog-cool"
#define PIDGIN_STOCK_DIALOG_WARNING	"pidgin-dialog-warning"

/* StatusBox Animations */
#define PIDGIN_STOCK_ANIMATION_CONNECT0 "pidgin-anim-connect0"
#define PIDGIN_STOCK_ANIMATION_CONNECT1 "pidgin-anim-connect1"
#define PIDGIN_STOCK_ANIMATION_CONNECT2 "pidgin-anim-connect2"
#define PIDGIN_STOCK_ANIMATION_CONNECT3 "pidgin-anim-connect3"
#define PIDGIN_STOCK_ANIMATION_CONNECT4 "pidgin-anim-connect4"
#define PIDGIN_STOCK_ANIMATION_CONNECT5 "pidgin-anim-connect5"
#define PIDGIN_STOCK_ANIMATION_CONNECT6 "pidgin-anim-connect6"
#define PIDGIN_STOCK_ANIMATION_CONNECT7 "pidgin-anim-connect7"
#define PIDGIN_STOCK_ANIMATION_CONNECT8 "pidgin-anim-connect8"
#define PIDGIN_STOCK_ANIMATION_CONNECT9 "pidgin-anim-connect9"
#define PIDGIN_STOCK_ANIMATION_CONNECT10 "pidgin-anim-connect10"
#define PIDGIN_STOCK_ANIMATION_CONNECT11 "pidgin-anim-connect11"
#define PIDGIN_STOCK_ANIMATION_CONNECT12 "pidgin-anim-connect12"
#define PIDGIN_STOCK_ANIMATION_CONNECT13 "pidgin-anim-connect13"
#define PIDGIN_STOCK_ANIMATION_CONNECT14 "pidgin-anim-connect14"
#define PIDGIN_STOCK_ANIMATION_CONNECT15 "pidgin-anim-connect15"
#define PIDGIN_STOCK_ANIMATION_CONNECT16 "pidgin-anim-connect16"
#define PIDGIN_STOCK_ANIMATION_CONNECT17 "pidgin-anim-connect17"
#define PIDGIN_STOCK_ANIMATION_CONNECT18 "pidgin-anim-connect18"
#define PIDGIN_STOCK_ANIMATION_CONNECT19 "pidgin-anim-connect19"
#define PIDGIN_STOCK_ANIMATION_CONNECT20 "pidgin-anim-connect20"
#define PIDGIN_STOCK_ANIMATION_CONNECT21 "pidgin-anim-connect21"
#define PIDGIN_STOCK_ANIMATION_CONNECT22 "pidgin-anim-connect22"
#define PIDGIN_STOCK_ANIMATION_CONNECT23 "pidgin-anim-connect23"
#define PIDGIN_STOCK_ANIMATION_CONNECT24 "pidgin-anim-connect24"
#define PIDGIN_STOCK_ANIMATION_CONNECT25 "pidgin-anim-connect25"
#define PIDGIN_STOCK_ANIMATION_CONNECT26 "pidgin-anim-connect26"
#define PIDGIN_STOCK_ANIMATION_CONNECT27 "pidgin-anim-connect27"
#define PIDGIN_STOCK_ANIMATION_CONNECT28 "pidgin-anim-connect28"
#define PIDGIN_STOCK_ANIMATION_CONNECT29 "pidgin-anim-connect29"
#define PIDGIN_STOCK_ANIMATION_CONNECT30 "pidgin-anim-connect30"
#define PIDGIN_STOCK_ANIMATION_TYPING0	"pidgin-anim-typing0"
#define PIDGIN_STOCK_ANIMATION_TYPING1	"pidgin-anim-typing1"
#define PIDGIN_STOCK_ANIMATION_TYPING2	"pidgin-anim-typing2"
#define PIDGIN_STOCK_ANIMATION_TYPING3	"pidgin-anim-typing3"
#define PIDGIN_STOCK_ANIMATION_TYPING4	"pidgin-anim-typing4"
#define PIDGIN_STOCK_ANIMATION_TYPING5	"pidgin-anim-typing5"

/* Toolbar (and menu) icons */
#define PIDGIN_STOCK_TOOLBAR_ACCOUNTS     "pidgin-accounts"
#define PIDGIN_STOCK_TOOLBAR_BGCOLOR      "pidgin-bgcolor"
#define PIDGIN_STOCK_TOOLBAR_BLOCK        "pidgin-block"
#define PIDGIN_STOCK_TOOLBAR_FGCOLOR      "pidgin-fgcolor"
#define PIDGIN_STOCK_TOOLBAR_SMILEY       "pidgin-smiley"
#define PIDGIN_STOCK_TOOLBAR_FONT_FACE	  "pidgin-font-face"
#define PIDGIN_STOCK_TOOLBAR_TEXT_SMALLER "pidgin-text-smaller"
#define PIDGIN_STOCK_TOOLBAR_TEXT_LARGER  "pidgin-text-larger"
#define PIDGIN_STOCK_TOOLBAR_INSERT       "pidgin-insert"
#define PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE "pidgin-insert-image"
#define PIDGIN_STOCK_TOOLBAR_INSERT_SCREENSHOT "pidgin-insert-screenshot"
#define PIDGIN_STOCK_TOOLBAR_INSERT_LINK  "pidgin-insert-link"
#define PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW  "pidgin-message-new"
#define PIDGIN_STOCK_TOOLBAR_PENDING      "pidgin-pending"
#define PIDGIN_STOCK_TOOLBAR_PLUGINS      "pidgin-plugins"
#define PIDGIN_STOCK_TOOLBAR_TYPING       "pidgin-typing"
#define PIDGIN_STOCK_TOOLBAR_USER_INFO    "pidgin-info"
#define PIDGIN_STOCK_TOOLBAR_UNBLOCK      "pidgin-unblock"
#define PIDGIN_STOCK_TOOLBAR_SELECT_AVATAR "pidgin-select-avatar"
#define PIDGIN_STOCK_TOOLBAR_SEND_FILE    "pidgin-send-file"
#define PIDGIN_STOCK_TOOLBAR_TRANSFER     "pidgin-transfer"
#ifdef USE_VV
#define PIDGIN_STOCK_TOOLBAR_AUDIO_CALL   "pidgin-audio-call"
#define PIDGIN_STOCK_TOOLBAR_VIDEO_CALL   "pidgin-video-call"
#define PIDGIN_STOCK_TOOLBAR_AUDIO_VIDEO_CALL "pidgin-audio-video-call"
#endif
#define PIDGIN_STOCK_TOOLBAR_SEND_ATTENTION	"pidgin-send-attention"

/* Tray icons */
#define PIDGIN_STOCK_TRAY_AVAILABLE       "pidgin-tray-available"
#define PIDGIN_STOCK_TRAY_INVISIBLE	  "pidgin-tray-invisible"
#define PIDGIN_STOCK_TRAY_AWAY	  	  "pidgin-tray-away"
#define PIDGIN_STOCK_TRAY_BUSY            "pidgin-tray-busy"
#define PIDGIN_STOCK_TRAY_XA              "pidgin-tray-xa"
#define PIDGIN_STOCK_TRAY_OFFLINE         "pidgin-tray-offline"
#define PIDGIN_STOCK_TRAY_CONNECT         "pidgin-tray-connect"
#define PIDGIN_STOCK_TRAY_PENDING         "pidgin-tray-pending"
#define PIDGIN_STOCK_TRAY_EMAIL		  "pidgin-tray-email"


/*
 * For using icons that aren't one of the default GTK_ICON_SIZEs
 */
#define PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC    "pidgin-icon-size-tango-microscopic"
#define PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL    "pidgin-icon-size-tango-extra-small"
#define PIDGIN_ICON_SIZE_TANGO_SMALL          "pidgin-icon-size-tango-small"
#define PIDGIN_ICON_SIZE_TANGO_MEDIUM         "pidgin-icon-size-tango-medium"
#define PIDGIN_ICON_SIZE_TANGO_LARGE          "pidgin-icon-size-tango-large"
#define PIDGIN_ICON_SIZE_TANGO_HUGE           "pidgin-icon-size-tango-huge"

typedef struct _PidginStockIconTheme        PidginStockIconTheme;
typedef struct _PidginStockIconThemeClass   PidginStockIconThemeClass;

#define PIDGIN_TYPE_STOCK_ICON_THEME            (pidgin_stock_icon_theme_get_type ())
#define PIDGIN_STOCK_ICON_THEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_STOCK_ICON_THEME, PidginStockIconTheme))
#define PIDGIN_STOCK_ICON_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_STOCK_ICON_THEME, PidginStockIconThemeClass))
#define PIDGIN_IS_STOCK_ICON_THEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_STOCK_ICON_THEME))
#define PIDGIN_IS_STOCK_ICON_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_STOCK_ICON_THEME))
#define PIDGIN_STOCK_ICON_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_STOCK_ICON_THEME, PidginStockIconThemeClass))

/**
 * PidginStockIconTheme:
 *
 * extends PidginIconTheme (gtkicon-theme.h)
 * A pidgin stock icon theme.
 * This object represents a Pidgin stock icon theme.
 *
 * PidginStockIconTheme is a PidginIconTheme Object.
 */
struct _PidginStockIconTheme
{
	PidginIconTheme parent;
};

struct _PidginStockIconThemeClass
{
	PidginIconThemeClass parent_class;
};

G_BEGIN_DECLS

/**
 * pidgin_stock_icon_theme_get_type:
 *
 * Returns: The #GType for a stock icon theme.
 */
GType pidgin_stock_icon_theme_get_type(void);

/**
 * pidgin_stock_load_status_icon_theme:
 * @theme:		the theme to load, or null to load all the default icons
 *
 * Loades all of the icons from the status icon theme into Pidgin stock
 */
void pidgin_stock_load_status_icon_theme(PidginStatusIconTheme *theme);


void pidgin_stock_load_stock_icon_theme(PidginStockIconTheme *theme);

/**
 * pidgin_stock_init:
 *
 * Sets up the purple stock repository.
 */
void pidgin_stock_init(void);

G_END_DECLS

#endif /* _PIDGIN_STOCK_H_ */
