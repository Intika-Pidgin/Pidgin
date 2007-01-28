/**
 * @file gaimstock.c GTK+ Stock resources
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "gtkgaim.h"

#include "gaimstock.h"

static struct StockIcon
{
	const char *name;
	const char *dir;
	const char *filename;

} const stock_icons[] =
{
	{ GAIM_STOCK_ABOUT,           "buttons", "about_menu.png"           },
	{ GAIM_STOCK_ACCOUNTS,        "buttons", "accounts.png"             },
	{ GAIM_STOCK_ACTION,          NULL,      GTK_STOCK_EXECUTE          },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_ALIAS,           NULL,      GTK_STOCK_EDIT             },
#else
	{ GAIM_STOCK_ALIAS,           "buttons", "edit.png"                 },
#endif
	{ GAIM_STOCK_BGCOLOR,         "buttons", "change-bgcolor-small.png" },
	{ GAIM_STOCK_BLOCK,           NULL,      GTK_STOCK_STOP             },
	{ GAIM_STOCK_UNBLOCK,         NULL,      GTK_STOCK_STOP /* XXX: */  },
	{ GAIM_STOCK_CHAT,            NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_CLEAR,           NULL,      GTK_STOCK_CLEAR            },
	{ GAIM_STOCK_CLOSE_TABS,      NULL,      GTK_STOCK_CLOSE            },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_CONNECT,         NULL,      GTK_STOCK_CONNECT          },
#else
	{ GAIM_STOCK_CONNECT,         "icons",   "stock_connect_16.png"     },
#endif
	{ GAIM_STOCK_DEBUG,           NULL,      GTK_STOCK_PROPERTIES       },
	{ GAIM_STOCK_DOWNLOAD,        NULL,      GTK_STOCK_GO_DOWN          },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_DISCONNECT,      NULL,      GTK_STOCK_DISCONNECT       },
#else
	{ GAIM_STOCK_DISCONNECT,      "icons",   "stock_disconnect_16.png"  },
#endif
	{ GAIM_STOCK_FGCOLOR,         "buttons", "change-fgcolor-small.png" },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_EDIT,            NULL,      GTK_STOCK_EDIT             },
#else
	{ GAIM_STOCK_EDIT,            "buttons", "edit.png"                 },
#endif
	{ GAIM_STOCK_FILE_CANCELED,   NULL,      GTK_STOCK_CANCEL           },
	{ GAIM_STOCK_FILE_DONE,       NULL,      GTK_STOCK_APPLY            },
	{ GAIM_STOCK_FILE_TRANSFER,   NULL,      GTK_STOCK_REVERT_TO_SAVED  },
	{ GAIM_STOCK_ICON_AWAY,       "icons",   "away.png"                 },
	{ GAIM_STOCK_ICON_AWAY_MSG,   "icons",   "msgpend.png"              },
	{ GAIM_STOCK_ICON_CONNECT,    "icons",   "connect.png"              },
	{ GAIM_STOCK_ICON_OFFLINE,    "icons",   "offline.png"              },
	{ GAIM_STOCK_ICON_ONLINE,     "icons",   "online.png"               },
	{ GAIM_STOCK_ICON_ONLINE_MSG, "icons",   "msgunread.png"            },
	{ GAIM_STOCK_IGNORE,          NULL,      GTK_STOCK_DIALOG_ERROR     },
	{ GAIM_STOCK_IM,              "buttons", "send-im.png"		    },
	{ GAIM_STOCK_IMAGE,           "buttons", "insert-image-small.png"   },
#if GTK_CHECK_VERSION(2,8,0)
	{ GAIM_STOCK_INFO,            NULL,      GTK_STOCK_INFO             },
#else
	{ GAIM_STOCK_INFO,            "buttons", "info.png"                 },
#endif
	{ GAIM_STOCK_INVITE,          NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_LINK,            "buttons", "insert-link-small.png"    },
	{ GAIM_STOCK_LOG,             NULL,      GTK_STOCK_DND_MULTIPLE     },
	{ GAIM_STOCK_MODIFY,          NULL,      GTK_STOCK_PREFERENCES      },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_PAUSE,           NULL,      GTK_STOCK_MEDIA_PAUSE      },
#else
	{ GAIM_STOCK_PAUSE,           "buttons", "pause.png"                },
#endif
	{ GAIM_STOCK_PENDING,         "buttons", "send-im.png"              },
#if GTK_CHECK_VERSION(2,6,0)
	{ GAIM_STOCK_PLUGIN,          NULL,      GTK_STOCK_DISCONNECT       },
#else
	{ GAIM_STOCK_PLUGIN,          "icons",   "stock_disconnect_16.png"  },
#endif
	{ GAIM_STOCK_POUNCE,          NULL,      GTK_STOCK_REDO             },
	{ GAIM_STOCK_OPEN_MAIL,       NULL,      GTK_STOCK_JUMP_TO          },
	{ GAIM_STOCK_SEND,            "buttons", "send-im.png"              },
	{ GAIM_STOCK_SIGN_ON,         NULL,      GTK_STOCK_EXECUTE          },
	{ GAIM_STOCK_SIGN_OFF,        NULL,      GTK_STOCK_CLOSE            },
	{ GAIM_STOCK_SMILEY,          "buttons", "insert-smiley-small.png"  },
	{ GAIM_STOCK_TEXT_BIGGER,     "buttons", "text_bigger.png"          },
	{ GAIM_STOCK_TEXT_NORMAL,     "buttons", "text_normal.png"          },
	{ GAIM_STOCK_TEXT_SMALLER,    "buttons", "text_smaller.png"         },
	{ GAIM_STOCK_TYPED,           "gaim",    "typed.png"                },
	{ GAIM_STOCK_TYPING,          "gaim",    "typing.png"               },
	{ GAIM_STOCK_VOICE_CHAT,      "gaim",    "phone.png"                },
	{ GAIM_STOCK_STATUS_INVISIBLE,"gaim",    "status-invisible.png"     },
	{ GAIM_STOCK_STATUS_TYPING0,  "gaim",    "status-typing0.png"       },
	{ GAIM_STOCK_STATUS_TYPING1,  "gaim",    "status-typing1.png"       },
	{ GAIM_STOCK_STATUS_TYPING2,  "gaim",    "status-typing2.png"       },
	{ GAIM_STOCK_STATUS_TYPING3,  "gaim",    "status-typing3.png"       },
	{ GAIM_STOCK_STATUS_CONNECT0, "gaim",    "status-connect0.png"      },
	{ GAIM_STOCK_STATUS_CONNECT1, "gaim",    "status-connect1.png"      },
	{ GAIM_STOCK_STATUS_CONNECT2, "gaim",    "status-connect2.png"      },
	{ GAIM_STOCK_STATUS_CONNECT3, "gaim",    "status-connect3.png"      },
	{ GAIM_STOCK_UPLOAD,          NULL,      GTK_STOCK_GO_UP            },
};

static const GtkStockItem stock_items[] =
{
	{ GAIM_STOCK_ALIAS,      N_("_Alias"),      0, 0, NULL },
	{ GAIM_STOCK_CHAT,       N_("_Join"),       0, 0, NULL },
	{ GAIM_STOCK_CLOSE_TABS, N_("Close _tabs"), 0, 0, NULL },
	{ GAIM_STOCK_IM,         N_("I_M"),         0, 0, NULL },
	{ GAIM_STOCK_INFO,       N_("_Get Info"),   0, 0, NULL },
	{ GAIM_STOCK_INVITE,     N_("_Invite"),     0, 0, NULL },
	{ GAIM_STOCK_MODIFY,     N_("_Modify"),     0, 0, NULL },
	{ GAIM_STOCK_OPEN_MAIL,  N_("_Open Mail"),  0, 0, NULL },
	{ GAIM_STOCK_PAUSE,      N_("_Pause"),      0, 0, NULL },
};

static struct SizedStockIcon {
  const char *name;
  const char *dir;
  const char *filename;
  gboolean extra_small;
  gboolean small;
  gboolean medium;
  gboolean huge;
} const sized_stock_icons [] = {
	{ PIDGIN_STOCK_STATUS_AVAILABLE,"status", "available.png", 	TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_AWAY, 	"status", "away.png", 		TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_BUSY, 	"status", "busy.png", 		TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_CHAT, 	"status", "chat.png",		TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_XA, 	"status", "extended-away.png",	TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_LOGIN, 	"status", "log-in.png",		TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_LOGOUT, 	"status", "log-out.png",	TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_OFFLINE, 	"status", "offline.png",	TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_PERSON, 	"status", "person.png",		TRUE, TRUE, TRUE, FALSE },
	{ PIDGIN_STOCK_STATUS_OPERATOR,	"status", "operator.png",	TRUE, FALSE, FALSE, FALSE },
	{ PIDGIN_STOCK_STATUS_HALFOP, 	"status", "half-operator.png",	TRUE, FALSE, FALSE, FALSE },

	{ PIDGIN_STOCK_DIALOG_AUTH,	"dialogs", "auth.png",		TRUE, FALSE, FALSE, TRUE },
	{ PIDGIN_STOCK_DIALOG_COOL,	"dialogs", "cool.png", 		FALSE, FALSE, FALSE, TRUE },
	{ PIDGIN_STOCK_DIALOG_ERROR,	"dialogs", "error.png",		TRUE, FALSE, FALSE, TRUE },
	{ PIDGIN_STOCK_DIALOG_INFO,	"dialogs", "info.png",		TRUE, FALSE, FALSE, TRUE },
	{ PIDGIN_STOCK_DIALOG_MAIL,	"dialogs", "mail.png",		TRUE, FALSE, FALSE, TRUE },
	{ PIDGIN_STOCK_DIALOG_QUESTION,	"dialogs", "question.png",	TRUE, FALSE, FALSE, TRUE },
	{ PIDGIN_STOCK_DIALOG_WARNING,	"dialogs", "warning.png",	FALSE, FALSE, FALSE, TRUE },
};

static gchar *
find_file(const char *dir, const char *base)
{
	char *filename;

	if (base == NULL)
		return NULL;

	if (!strcmp(dir, "gaim"))
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", base, NULL);
	else
	{
		filename = g_build_filename(DATADIR, "pixmaps", "gaim", dir,
									base, NULL);
	}

	return filename;
}

static void
add_sized_icon(GtkIconSet *iconset, GtkIconSize sizeid, const char *dir, 
	       const char *size, const char *file)
{
	char *filename;
	GtkIconSource *source;	

	filename = g_build_filename(DATADIR, "pixmaps", "pidgin", dir, size, file, NULL);
	source = gtk_icon_source_new();
        gtk_icon_source_set_filename(source, filename);
        gtk_icon_source_set_direction_wildcarded(source, TRUE);
	gtk_icon_source_set_size(source, sizeid);
        gtk_icon_source_set_size_wildcarded(source, FALSE);
        gtk_icon_source_set_state_wildcarded(source, TRUE);
        gtk_icon_set_add_source(iconset, source);
	gtk_icon_source_free(source);
        g_free(filename);
}

void
gaim_gtk_stock_init(void)
{
	static gboolean stock_initted = FALSE;
	GtkIconFactory *icon_factory;
	size_t i;
	GtkWidget *win;
	GtkIconSize extra_small, small, medium, huge;
	
	if (stock_initted)
		return;

	stock_initted = TRUE;

	/* Setup the icon factory. */
	icon_factory = gtk_icon_factory_new();

	gtk_icon_factory_add_default(icon_factory);

	/* Er, yeah, a hack, but it works. :) */
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(win);

	for (i = 0; i < G_N_ELEMENTS(stock_icons); i++)
	{
		GtkIconSource *source;
		GtkIconSet *iconset;
		gchar *filename;

		if (stock_icons[i].dir == NULL)
		{
			/* GTK+ Stock icon */
			iconset = gtk_style_lookup_icon_set(gtk_widget_get_style(win),
												stock_icons[i].filename);
		}
		else
		{
			filename = find_file(stock_icons[i].dir, stock_icons[i].filename);

			if (filename == NULL)
				continue;

			source = gtk_icon_source_new();
			gtk_icon_source_set_filename(source, filename);
			gtk_icon_source_set_direction_wildcarded(source, TRUE);
			gtk_icon_source_set_size_wildcarded(source, TRUE);
			gtk_icon_source_set_state_wildcarded(source, TRUE);
			

			iconset = gtk_icon_set_new();
			gtk_icon_set_add_source(iconset, source);
			
			gtk_icon_source_free(source);
			g_free(filename);
		}

		gtk_icon_factory_add(icon_factory, stock_icons[i].name, iconset);

		gtk_icon_set_unref(iconset);
	}

	/* register custom icon sizes */
	extra_small =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL, 16, 16);
	small =        gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_SMALL, 22, 22);
	medium =       gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_MEDIUM, 32, 32);
	huge =         gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_HUGE, 64, 64);

	for (i = 0; i < G_N_ELEMENTS(sized_stock_icons); i++)
	{
		GtkIconSet *iconset;

		iconset = gtk_icon_set_new();
		if (sized_stock_icons[i].extra_small)
			add_sized_icon(iconset, extra_small,
				       sized_stock_icons[i].dir,
				       "16", sized_stock_icons[i].filename);
               if (sized_stock_icons[i].small)
                        add_sized_icon(iconset, small,
				       sized_stock_icons[i].dir, 
                                       "22", sized_stock_icons[i].filename);
               if (sized_stock_icons[i].medium)
                        add_sized_icon(iconset, medium,
			               sized_stock_icons[i].dir, 
                                       "32", sized_stock_icons[i].filename);
               if (sized_stock_icons[i].huge)
                        add_sized_icon(iconset, huge,
	                               sized_stock_icons[i].dir, 
                                       "64", sized_stock_icons[i].filename);

		gtk_icon_factory_add(icon_factory, sized_stock_icons[i].name, iconset);
		gtk_icon_set_unref(iconset);
	}

	gtk_widget_destroy(win);
	g_object_unref(G_OBJECT(icon_factory));

	/* Register the stock items. */
	gtk_stock_add_static(stock_items, G_N_ELEMENTS(stock_items));
}
