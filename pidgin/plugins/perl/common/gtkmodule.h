/* Allow the Perl code to see deprecated functions, so we can continue to
 * export them to Perl plugins. */
#undef PIDGIN_DISABLE_DEPRECATED

typedef struct group *Pidgin__Group;

#define group perl_group

#include <glib.h>
#include <gtk/gtk.h>
#ifdef _WIN32
#undef pipe
#endif

#define SILENT_NO_TAINT_SUPPORT 0
#define NO_TAINT_SUPPORT 0

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#undef group

#include <plugins/perl/common/module.h>

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkconv.h"
#include "gtkconvwin.h"
#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkxfer.h"
#include "gtklog.h"
#include "gtkmenutray.h"
#include "gtkplugin.h"
#include "gtkpluginpref.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkroomlist.h"
#include "gtksavedstatuses.h"
#include "gtksession.h"
#include "gtksound.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"

/* gtkaccount.h */
typedef PidginAccountDialogType		Pidgin__Account__Dialog__Type;

/* gtkblist.h */
typedef PidginBuddyList *		Pidgin__BuddyList;
typedef pidgin_blist_sort_function	Pidgin__BuddyList__SortFunction;

/* gtkconv.h */
typedef PidginConversation *		Pidgin__Conversation;
typedef PidginUnseenState		Pidgin__UnseenState;

/* gtkconvwin.h */
typedef PidginConvWindow *			Pidgin__Conversation__Window;
typedef PidginConvPlacementFunc		Pidgin__Conversation__PlacementFunc;

/* gtkxfer.h */
typedef PidginXferDialog *		Pidgin__Xfer__Dialog;

/* gtkmenutray.h */
typedef PidginMenuTray *		Pidgin__MenuTray;

/* gtkstatusbox.h */
typedef PidginStatusBox *		Pidgin__StatusBox;
