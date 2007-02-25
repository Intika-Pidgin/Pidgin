/**
 * @file tcl.c Gaim Tcl plugin bindings
 *
 * gaim
 *
 * Copyright (C) 2003 Ethan Blanton <eblanton@cs.purdue.edu>
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
 */

#include "config.h"

#include <tcl.h>

#ifdef HAVE_TK
#include <tk.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "tcl_glib.h"
#include "tcl_gaim.h"

#include "internal.h"
#include "connection.h"
#include "plugin.h"
#include "signals.h"
#include "debug.h"
#include "util.h"
#include "version.h"

struct tcl_plugin_data {
	GaimPlugin *plugin;
	Tcl_Interp *interp;
};

GaimStringref *GaimTclRefAccount;
GaimStringref *GaimTclRefConnection;
GaimStringref *GaimTclRefConversation;
GaimStringref *GaimTclRefPointer;
GaimStringref *GaimTclRefPlugin;
GaimStringref *GaimTclRefPresence;
GaimStringref *GaimTclRefStatus;
GaimStringref *GaimTclRefStatusAttr;
GaimStringref *GaimTclRefStatusType;
GaimStringref *GaimTclRefXfer;

static GHashTable *tcl_plugins = NULL;

GaimPlugin *_tcl_plugin;

static gboolean tcl_loaded = FALSE;

GaimPlugin *tcl_interp_get_plugin(Tcl_Interp *interp)
{
	struct tcl_plugin_data *data;

	if (tcl_plugins == NULL)
		return NULL;

	data = g_hash_table_lookup(tcl_plugins, (gpointer)interp);
	return data != NULL ? data->plugin : NULL;
}

static int tcl_init_interp(Tcl_Interp *interp)
{
	char *rcfile;
	char init[] =
		"namespace eval ::gaim {\n"
		"	namespace export account buddy connection conversation\n"
		"	namespace export core debug notify prefs send_im\n"
		"	namespace export signal unload\n"
		"	namespace eval _callback { }\n"
		"\n"
		"	proc conv_send { account who text } {\n"
		"		set gc [gaim::account connection $account]\n"
		"		set convo [gaim::conversation new $account $who]\n"
		"		set myalias [gaim::account alias $account]\n"
		"\n"
		"		if {![string length $myalias]} {\n"
		"			set myalias [gaim::account username $account]\n"
		"		}\n"
		"\n"
		"		gaim::send_im $gc $who $text\n"
		"		gaim::conversation write $convo	send $myalias $text\n"
		"	}\n"
		"}\n"
		"\n"
		"proc bgerror { message } {\n"
		"	global errorInfo\n"
		"	gaim::notify -error \"Tcl Error\" \"Tcl Error: $message\" \"$errorInfo\"\n"
		"}\n";

	if (Tcl_EvalEx(interp, init, -1, TCL_EVAL_GLOBAL) != TCL_OK) {
		return 1;
	}

	Tcl_SetVar(interp, "argc", "0", TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "argv0", "gaim", TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
	rcfile = g_strdup_printf("%s" G_DIR_SEPARATOR_S "tclrc", gaim_user_dir());
	Tcl_SetVar(interp, "tcl_rcFileName", rcfile, TCL_GLOBAL_ONLY);
	g_free(rcfile);

	Tcl_SetVar(interp, "::gaim::version", VERSION, TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "::gaim::user_dir", gaim_user_dir(), TCL_GLOBAL_ONLY);
#ifdef HAVE_TK
	Tcl_SetVar(interp, "::gaim::tk_available", "1", TCL_GLOBAL_ONLY);
#else
	Tcl_SetVar(interp, "::gaim::tk_available", "0", TCL_GLOBAL_ONLY);
#endif /* HAVE_TK */

	Tcl_CreateObjCommand(interp, "::gaim::account", tcl_cmd_account, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::buddy", tcl_cmd_buddy, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::cmd", tcl_cmd_cmd, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::connection", tcl_cmd_connection, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::conversation", tcl_cmd_conversation, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::core", tcl_cmd_core, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::debug", tcl_cmd_debug, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::notify", tcl_cmd_notify, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::plugins", tcl_cmd_plugins, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::prefs", tcl_cmd_prefs, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::presence", tcl_cmd_presence, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::send_im", tcl_cmd_send_im, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::savedstatus", tcl_cmd_savedstatus, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::signal", tcl_cmd_signal, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::status", tcl_cmd_status, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::status_attr", tcl_cmd_status_attr, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::status_type", tcl_cmd_status_type, (ClientData)NULL, NULL);
	Tcl_CreateObjCommand(interp, "::gaim::unload", tcl_cmd_unload, (ClientData)NULL, NULL);

	return 0;
}

static Tcl_Interp *tcl_create_interp()
{
	Tcl_Interp *interp;

	interp = Tcl_CreateInterp();
	if (Tcl_Init(interp) == TCL_ERROR) {
		Tcl_DeleteInterp(interp);
		return NULL;
	}

	if (tcl_init_interp(interp)) {
		Tcl_DeleteInterp(interp);
		return NULL;
	}
	Tcl_StaticPackage(interp, "gaim", tcl_init_interp, NULL);

	return interp;
}

static gboolean tcl_probe_plugin(GaimPlugin *plugin)
{
	GaimPluginInfo *info;
	Tcl_Interp *interp;
	Tcl_Parse parse;
	Tcl_Obj *result, **listitems;
	struct stat st;
	FILE *fp;
	char *buf, *cur;
	const char *next;
	int len, found = 0, err = 0, nelems;
	gboolean status = FALSE;
	if ((fp = g_fopen(plugin->path, "r")) == NULL)
		return FALSE;
	if (fstat(fileno(fp), &st)) {
		fclose(fp);
		return FALSE;
	}
	len = st.st_size;

	buf = g_malloc(len + 1);

	cur = buf;
	while (fgets(cur, GPOINTER_TO_INT(buf) - (buf - cur), fp)) {
		cur += strlen(cur);
		if (feof(fp))
			break;
	}

	if (ferror(fp)) {
		gaim_debug(GAIM_DEBUG_ERROR, "tcl", "error reading %s (%s)\n", plugin->path, strerror(errno));
		g_free(buf);
		fclose(fp);
		return FALSE;
	}

	fclose(fp);

	if ((interp = tcl_create_interp()) == NULL) {
		return FALSE;
	}

	next = buf;
	do {
		if (Tcl_ParseCommand(interp, next, len, 0, &parse) == TCL_ERROR) {
			gaim_debug(GAIM_DEBUG_ERROR, "tcl", "parse error in %s: %s\n", plugin->path,
				   Tcl_GetString(Tcl_GetObjResult(interp)));
			err = 1;
			break;
		}
		if (parse.tokenPtr[0].type == TCL_TOKEN_SIMPLE_WORD
		    && !strncmp(parse.tokenPtr[0].start, "proc", parse.tokenPtr[0].size)) {
			if (!strncmp(parse.tokenPtr[2].start, "plugin_init", parse.tokenPtr[2].size)) {
				if (Tcl_EvalEx(interp, parse.commandStart, parse.commandSize, TCL_EVAL_GLOBAL) != TCL_OK) {
					Tcl_FreeParse(&parse);
					break;
				}
				found = 1;
				/* We'll continue parsing the file, just in case */
			}
		}
		len -= (parse.commandStart + parse.commandSize) - next;
		next = parse.commandStart + parse.commandSize;
		Tcl_FreeParse(&parse);
	} while (len);

	if (found && !err) {
		if (Tcl_EvalEx(interp, "plugin_init", -1, TCL_EVAL_GLOBAL) == TCL_OK) {
			result = Tcl_GetObjResult(interp);
			if (Tcl_ListObjGetElements(interp, result, &nelems, &listitems) == TCL_OK) {
				if ((nelems == 6) || (nelems == 7)) {
					info = g_new0(GaimPluginInfo, 1);

					info->magic = GAIM_PLUGIN_MAGIC;
					info->major_version = GAIM_MAJOR_VERSION;
					info->minor_version = GAIM_MINOR_VERSION;
					info->type = GAIM_PLUGIN_STANDARD;
					info->dependencies = g_list_append(info->dependencies, "core-tcl");

					info->name = g_strdup(Tcl_GetString(listitems[0]));
					info->version = g_strdup(Tcl_GetString(listitems[1]));
					info->summary = g_strdup(Tcl_GetString(listitems[2]));
					info->description = g_strdup(Tcl_GetString(listitems[3]));
					info->author = g_strdup(Tcl_GetString(listitems[4]));
					info->homepage = g_strdup(Tcl_GetString(listitems[5]));

					if (nelems == 6)
						info->id = g_strdup_printf("tcl-%s", Tcl_GetString(listitems[0]));
					else if (nelems == 7)
						info->id = g_strdup_printf("tcl-%s", Tcl_GetString(listitems[6]));

					plugin->info = info;

					if (gaim_plugin_register(plugin))
						status = TRUE;
				}
			}
		}
	}

	Tcl_DeleteInterp(interp);
	g_free(buf);
	return status;
}

static gboolean tcl_load_plugin(GaimPlugin *plugin)
{
	struct tcl_plugin_data *data;
	Tcl_Interp *interp;
	Tcl_Obj *result;

	plugin->extra = NULL;

	if ((interp = tcl_create_interp()) == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "tcl", "Could not initialize Tcl interpreter\n");
		return FALSE;
	}

	Tcl_SourceRCFile(interp);

	if (Tcl_EvalFile(interp, plugin->path) != TCL_OK) {
		result = Tcl_GetObjResult(interp);
		gaim_debug(GAIM_DEBUG_ERROR, "tcl",
		           "Error evaluating %s: %s\n", plugin->path,
		           Tcl_GetString(result));
		Tcl_DeleteInterp(interp);
		return FALSE;
	}

	Tcl_Preserve((ClientData)interp);

	data = g_new0(struct tcl_plugin_data, 1);
	data->plugin = plugin;
	data->interp = interp;
	plugin->extra = data;

	g_hash_table_insert(tcl_plugins, (gpointer)interp, (gpointer)data);

	return TRUE;
}

static gboolean tcl_unload_plugin(GaimPlugin *plugin)
{
	struct tcl_plugin_data *data;

	if (plugin == NULL)
		return TRUE;

	data = plugin->extra;

	if (data != NULL) {
		g_hash_table_remove(tcl_plugins, (gpointer)(data->interp));
		gaim_signals_disconnect_by_handle(data->interp);
		tcl_cmd_cleanup(data->interp);
		tcl_signal_cleanup(data->interp);
		Tcl_Release((ClientData)data->interp);
		Tcl_DeleteInterp(data->interp);
		g_free(data);
	}

	return TRUE;
}

static void tcl_destroy_plugin(GaimPlugin *plugin)
{
	if (plugin->info != NULL) {
		g_free(plugin->info->id);
		g_free(plugin->info->name);
		g_free(plugin->info->version);
		g_free(plugin->info->description);
		g_free(plugin->info->author);
		g_free(plugin->info->homepage);
	}

	return;
}

static gboolean tcl_load(GaimPlugin *plugin)
{
	if(!tcl_loaded)
		return FALSE;
	tcl_glib_init();
	tcl_cmd_init();
	tcl_signal_init();
	gaim_tcl_ref_init();

	GaimTclRefAccount = gaim_stringref_new("Account");
	GaimTclRefConnection = gaim_stringref_new("Connection");
	GaimTclRefConversation = gaim_stringref_new("Conversation");
	GaimTclRefPointer = gaim_stringref_new("Pointer");
	GaimTclRefPlugin = gaim_stringref_new("Plugin");
	GaimTclRefPresence = gaim_stringref_new("Presence");
	GaimTclRefStatus = gaim_stringref_new("Status");
	GaimTclRefStatusAttr = gaim_stringref_new("StatusAttr");
	GaimTclRefStatusType = gaim_stringref_new("StatusType");
	GaimTclRefXfer = gaim_stringref_new("Xfer");

	tcl_plugins = g_hash_table_new(g_direct_hash, g_direct_equal);

#ifdef HAVE_TK
	Tcl_StaticPackage(NULL, "Tk", Tk_Init, Tk_SafeInit);
#endif /* HAVE_TK */

	return TRUE;
}

static gboolean tcl_unload(GaimPlugin *plugin)
{
	g_hash_table_destroy(tcl_plugins);
	tcl_plugins = NULL;

	gaim_stringref_unref(GaimTclRefAccount);
	gaim_stringref_unref(GaimTclRefConnection);
	gaim_stringref_unref(GaimTclRefConversation);
	gaim_stringref_unref(GaimTclRefPointer);
	gaim_stringref_unref(GaimTclRefPlugin);
	gaim_stringref_unref(GaimTclRefPresence);
	gaim_stringref_unref(GaimTclRefStatus);
	gaim_stringref_unref(GaimTclRefStatusAttr);
	gaim_stringref_unref(GaimTclRefStatusType);
	gaim_stringref_unref(GaimTclRefXfer);

	return TRUE;
}

static GaimPluginLoaderInfo tcl_loader_info =
{
	NULL,
	tcl_probe_plugin,
	tcl_load_plugin,
	tcl_unload_plugin,
	tcl_destroy_plugin,
};

static GaimPluginInfo tcl_info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_LOADER,
	NULL,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	"core-tcl",
	N_("Tcl Plugin Loader"),
	VERSION,
	N_("Provides support for loading Tcl plugins"),
	N_("Provides support for loading Tcl plugins"),
	"Ethan Blanton <eblanton@cs.purdue.edu>",
	GAIM_WEBSITE,
	tcl_load,
	tcl_unload,
	NULL,
	NULL,
	&tcl_loader_info,
	NULL,
	NULL
};

#ifdef _WIN32
typedef Tcl_Interp* (CALLBACK* LPFNTCLCREATEINTERP)(void);
typedef void        (CALLBACK* LPFNTKINIT)(Tcl_Interp*);

LPFNTCLCREATEINTERP wtcl_CreateInterp = NULL;
LPFNTKINIT wtk_Init = NULL;
#undef Tcl_CreateInterp
#define Tcl_CreateInterp wtcl_CreateInterp
#undef Tk_Init
#define Tk_Init wtk_Init

static gboolean tcl_win32_init() {
	const char regkey[] = "SOFTWARE\\ActiveState\\ActiveTcl\\";
	char *version = NULL;
	gboolean retval = FALSE;

	if ((version = wgaim_read_reg_string(HKEY_LOCAL_MACHINE, regkey, "CurrentVersion"))
			|| (version = wgaim_read_reg_string(HKEY_CURRENT_USER, regkey, "CurrentVersion"))) {
		char *path;
		char *regkey2;

		regkey2 = g_strdup_printf("%s%s\\", regkey, version);
		if ((path = wgaim_read_reg_string(HKEY_LOCAL_MACHINE, regkey2, NULL)) || (path = wgaim_read_reg_string(HKEY_CURRENT_USER, regkey2, NULL))) {
			char *tclpath;
			char *tkpath;

			gaim_debug(GAIM_DEBUG_INFO, "tcl", "Loading ActiveTCL version %s from \"%s\"\n", version, path);

			tclpath = g_build_filename(path, "bin", "tcl84.dll", NULL);
			tkpath = g_build_filename(path, "bin", "tk84.dll", NULL);

			if(!(wtcl_CreateInterp = (LPFNTCLCREATEINTERP) wgaim_find_and_loadproc(tclpath, "Tcl_CreateInterp"))) {
				gaim_debug(GAIM_DEBUG_INFO, "tcl", "tcl_win32_init error loading Tcl_CreateInterp\n");
			} else {
				if(!(wtk_Init = (LPFNTKINIT) wgaim_find_and_loadproc(tkpath, "Tk_Init"))) {
					HMODULE mod;
					gaim_debug(GAIM_DEBUG_INFO, "tcl", "tcl_win32_init error loading Tk_Init\n");
					if((mod = GetModuleHandle("tcl84.dll")))
						FreeLibrary(mod);
				} else {
					retval = TRUE;
				}
			}
			g_free(tclpath);
			g_free(tkpath);
		}
		g_free(path);
		g_free(regkey2);
	}

	g_free(version);

	if (!retval)
		gaim_debug(GAIM_DEBUG_INFO, "tcl", _("Unable to detect ActiveTCL installation. If you wish to use TCL plugins, install ActiveTCL from http://www.activestate.com\n"));

	return retval;
}

#endif /* _WIN32 */

static void tcl_init_plugin(GaimPlugin *plugin)
{
#ifdef USE_TCL_STUBS
	Tcl_Interp *interp = NULL;
#endif
	_tcl_plugin = plugin;

#ifdef USE_TCL_STUBS
#ifdef _WIN32
	if(!tcl_win32_init())
		return;
#endif
	if(!(interp = Tcl_CreateInterp()))
		return;

	if(!Tcl_InitStubs(interp, TCL_VERSION, 0)) {
		gaim_debug(GAIM_DEBUG_ERROR, "tcl", "Tcl_InitStubs: %s\n", interp->result);
		return;
	}
#endif

	Tcl_FindExecutable("gaim");

#if defined(USE_TK_STUBS) && defined(HAVE_TK)
	Tk_Init(interp);

	if(!Tk_InitStubs(interp, TK_VERSION, 0)) {
		gaim_debug(GAIM_DEBUG_ERROR, "tcl", "Error Tk_InitStubs: %s\n", interp->result);
		Tcl_DeleteInterp(interp);
		return;
	}
#endif
	tcl_loaded = TRUE;
#ifdef USE_TCL_STUBS
	Tcl_DeleteInterp(interp);
#endif
	tcl_loader_info.exts = g_list_append(tcl_loader_info.exts, "tcl");
}

GAIM_INIT_PLUGIN(tcl, tcl_init_plugin, tcl_info)
