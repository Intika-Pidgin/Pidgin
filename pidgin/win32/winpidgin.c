/*
 *  winpidgin.c
 *
 *  Date: June, 2002
 *  Description: Entry point for win32 pidgin, and various win32 dependant
 *  routines.
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
 *
 */

#include "config.h"

#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef IS_WIN32_CROSS_COMPILED
typedef int (__cdecl* LPFNPIDGINMAIN)(HINSTANCE, int, char**);
#endif
typedef void (WINAPI* LPFNSETDLLDIRECTORY)(LPCWSTR);
typedef BOOL (WINAPI* LPFNATTACHCONSOLE)(DWORD);
typedef BOOL (WINAPI* LPFNSETPROCESSDEPPOLICY)(DWORD);

static BOOL portable_mode = FALSE;

/*
 *  PROTOTYPES
 */
#ifdef IS_WIN32_CROSS_COMPILED
int __cdecl pidgin_main(HINSTANCE hint, int argc, char *argv[]);
#else
static LPFNPIDGINMAIN pidgin_main = NULL;
#endif
static LPFNSETDLLDIRECTORY MySetDllDirectory = NULL;

static const wchar_t *get_win32_error_message(DWORD err) {
	static wchar_t err_msg[512];

	FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR) &err_msg, sizeof(err_msg) / sizeof(wchar_t), NULL);

	return err_msg;
}

static BOOL reg_value_exists(HKEY key, wchar_t *sub_key, wchar_t *val_name) {
	HKEY hkey;
	LONG retv;
	DWORD index;
	wchar_t name_buffer[100];
	BOOL exists = FALSE;

	if (sub_key == NULL || val_name == NULL)
		return FALSE;

	retv = RegOpenKeyExW(key, sub_key, 0, KEY_ENUMERATE_SUB_KEYS, &hkey);
	if (retv != ERROR_SUCCESS)
		return FALSE;

	if (val_name[0] == L'\0') {
		RegCloseKey(hkey);
		return TRUE;
	}

	index = 0;
	while (TRUE)
	{
		DWORD name_size = sizeof(name_buffer);
		retv = RegEnumValueW(hkey, index++, name_buffer, &name_size,
			NULL, NULL, NULL, NULL);
		if (retv != ERROR_SUCCESS)
			break;
		name_size /= sizeof(wchar_t);
		if (wcsncmp(name_buffer, val_name, name_size) == 0) {
			exists = TRUE;
			break;
		}
	}

	RegCloseKey(hkey);
	return exists;
}

static BOOL read_reg_string(HKEY key, wchar_t *sub_key, wchar_t *val_name, LPBYTE data, LPDWORD data_len) {
	HKEY hkey;
	BOOL ret = FALSE;
	LONG retv;

	if (ERROR_SUCCESS == (retv = RegOpenKeyExW(key, sub_key, 0,
					KEY_QUERY_VALUE, &hkey))) {
		if (ERROR_SUCCESS == (retv = RegQueryValueExW(hkey, val_name,
						NULL, NULL, data, data_len)))
			ret = TRUE;
		else {
			const wchar_t *err_msg = get_win32_error_message(retv);

			wprintf(L"Could not read reg key '%s' subkey '%s' value: '%s'.\nMessage: (%ld) %s\n",
					(key == HKEY_LOCAL_MACHINE) ? L"HKLM"
					 : ((key == HKEY_CURRENT_USER) ? L"HKCU" : L"???"),
					sub_key, val_name, retv, err_msg);
		}
		RegCloseKey(hkey);
	}
	else {
		wchar_t szBuf[80];

		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, retv, 0,
				(LPWSTR) &szBuf, sizeof(szBuf) / sizeof(wchar_t), NULL);
		wprintf(L"Could not open reg subkey: %s\nError: (%ld) %s\n",
				sub_key, retv, szBuf);
	}

	return ret;
}

static BOOL check_for_gtk(const wchar_t *path) {
	struct _stat stat_buf;
	wchar_t test_path[MAX_PATH + 1];

	_snwprintf(test_path, sizeof(test_path) / sizeof(wchar_t),
		L"%s\\libgtk-win32-2.0-0.dll", path);
	test_path[sizeof(test_path) / sizeof(wchar_t) - 1] = L'\0';

	return (_wstat(test_path, &stat_buf) == 0);
}

static void common_dll_prep(const wchar_t *path) {
	HMODULE hmod;
	HKEY hkey;
	wchar_t alt_path_buff[MAX_PATH + 1];
	wchar_t tmp_path[MAX_PATH + 1];
	/* Hold strlen("FS_PLUGIN_PATH=" or "GST_PLUGIN_SYSTEM_PATH") +
	 * MAX_PATH + 1
	 */
	wchar_t set_path[MAX_PATH + 24];
	wchar_t *fslash, *bslash;

	if (!check_for_gtk(path)) {
		const wchar_t *winpath = _wgetenv(L"PATH");
		wchar_t *delim;

		if (winpath == NULL) {
			printf("Unable to determine GTK+ path (and PATH is not set).\n");
			exit(-1);
		}

		path = NULL;
		do
		{
			wcsncpy(alt_path_buff, winpath, MAX_PATH);
			alt_path_buff[MAX_PATH] = L'\0';
			delim = wcschr(alt_path_buff, L';');
			if (delim != NULL) {
				delim[0] = L'\0';
				winpath = wcschr(winpath, L';') + 1;
			}
			if (check_for_gtk(alt_path_buff)) {
				path = alt_path_buff;
				break;
			}
		}
		while (delim != NULL);

		if (path == NULL) {
			printf("Unable to determine GTK+ path.\n");
			exit(-1);
		}
	}

	wprintf(L"GTK+ path found: %s\n", path);

	wcsncpy(tmp_path, path, MAX_PATH);
	tmp_path[MAX_PATH] = L'\0';
	bslash = wcsrchr(tmp_path, L'\\');
	fslash = wcsrchr(tmp_path, L'/');
	if (bslash && bslash > fslash)
		bslash[0] = L'\0';
	else if (fslash && fslash > bslash)
		fslash[0] = L'\0';
	/* tmp_path now contains \path\to\Pidgin\Gtk */

	_snwprintf(set_path, sizeof(set_path) / sizeof(wchar_t),
		L"FS_PLUGIN_PATH=%s\\lib\\farstream-0.1", tmp_path);
	set_path[sizeof(set_path) / sizeof(wchar_t) - 1] = L'\0';
	_wputenv(set_path);

	_snwprintf(set_path, sizeof(set_path) / sizeof(wchar_t),
		L"GST_PLUGIN_SYSTEM_PATH=%s\\lib\\gstreamer-0.10", tmp_path);
	set_path[sizeof(set_path) / sizeof(wchar_t) - 1] = L'\0';
	_wputenv(set_path);

	_snwprintf(set_path, sizeof(set_path) / sizeof(wchar_t),
		L"GST_PLUGIN_PATH=%s\\lib\\gstreamer-0.10", tmp_path);
	set_path[sizeof(set_path) / sizeof(wchar_t) - 1] = L'\0';
	_wputenv(set_path);

	if ((hmod = GetModuleHandleW(L"kernel32.dll"))) {
		MySetDllDirectory = (LPFNSETDLLDIRECTORY) GetProcAddress(
			hmod, "SetDllDirectoryW");
		if (!MySetDllDirectory)
			printf("SetDllDirectory not supported\n");
	} else
		printf("Error getting kernel32.dll module handle\n");

	/* For Windows XP SP1+ / Server 2003 we use SetDllDirectory to avoid dll hell */
	if (MySetDllDirectory) {
		MySetDllDirectory(path);
	}

	/* For the rest, we set the current directory and make sure
	 * SafeDllSearch is set to 0 where needed. */
	else {
		OSVERSIONINFOW osinfo;

		printf("Setting current directory to GTK+ dll directory\n");
		SetCurrentDirectoryW(path);
		/* For Windows 2000 (SP3+) / WinXP (No SP):
		 * If SafeDllSearchMode is set to 1, Windows system directories are
		 * searched for dlls before the current directory. Therefore we set it
		 * to 0.
		 */
		osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
		GetVersionExW(&osinfo);
		if ((osinfo.dwMajorVersion == 5
				&& osinfo.dwMinorVersion == 0
				&& wcscmp(osinfo.szCSDVersion, L"Service Pack 3") >= 0)
			||
			(osinfo.dwMajorVersion == 5
				&& osinfo.dwMinorVersion == 1
				&& wcscmp(osinfo.szCSDVersion, L"") >= 0)
		) {
			DWORD regval = 1;
			DWORD reglen = sizeof(DWORD);

			printf("Using Win2k (SP3+) / WinXP (No SP)... Checking SafeDllSearch\n");
			read_reg_string(HKEY_LOCAL_MACHINE,
				L"System\\CurrentControlSet\\Control\\Session Manager",
				L"SafeDllSearchMode",
				(LPBYTE) &regval,
				&reglen);

			if (regval != 0) {
				printf("Trying to set SafeDllSearchMode to 0\n");
				regval = 0;
				if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
					L"System\\CurrentControlSet\\Control\\Session Manager",
					0,  KEY_SET_VALUE, &hkey
				) == ERROR_SUCCESS) {
					if (RegSetValueExW(hkey,
						L"SafeDllSearchMode", 0,
						REG_DWORD, (LPBYTE) &regval,
						sizeof(DWORD)
					) != ERROR_SUCCESS)
						printf("Error writing SafeDllSearchMode. Error: %u\n",
							(UINT) GetLastError());
					RegCloseKey(hkey);
				} else
					printf("Error opening Session Manager key for writing. Error: %u\n",
						(UINT) GetLastError());
			} else
				printf("SafeDllSearchMode is set to 0\n");
		}/*end else*/
	}
}

#ifndef IS_WIN32_CROSS_COMPILED
static void dll_prep(const wchar_t *pidgin_dir) {
	wchar_t path[MAX_PATH + 1];
	path[0] = L'\0';

	if (*pidgin_dir) {
		_snwprintf(path, sizeof(path) / sizeof(wchar_t), L"%s\\Gtk\\bin", pidgin_dir);
		path[sizeof(path) / sizeof(wchar_t) - 1] = L'\0';
	}

	common_dll_prep(path);
}
#endif

static void portable_mode_dll_prep(const wchar_t *pidgin_dir) {
	/* need to be able to fit MAX_PATH + "PURPLEHOME=" in path2 */
	wchar_t path[MAX_PATH + 1];
	wchar_t path2[MAX_PATH + 12];
	const wchar_t *prev = NULL;

	/* We assume that GTK+ is installed under \\path\to\Pidgin\..\GTK
	 * First we find \\path\to
	 */
	if (*pidgin_dir)
		/* pidgin_dir points to \\path\to\Pidgin */
		prev = wcsrchr(pidgin_dir, L'\\');

	if (prev) {
		int cnt = (prev - pidgin_dir);
		wcsncpy(path, pidgin_dir, cnt);
		path[cnt] = L'\0';
	} else {
		printf("Unable to determine current executable path. \n"
			"This will prevent the settings dir from being set.\n");
		common_dll_prep(L'\0');
		return;
	}

	/* Set $HOME so that the GTK+ settings get stored in the right place */
	_snwprintf(path2, sizeof(path2) / sizeof(wchar_t), L"HOME=%s", path);
	_wputenv(path2);

	/* Set up the settings dir base to be \\path\to
	 * The actual settings dir will be \\path\to\.purple */
	_snwprintf(path2, sizeof(path2) / sizeof(wchar_t), L"PURPLEHOME=%s", path);
	wprintf(L"Setting settings dir: %s\n", path2);
	_wputenv(path2);

	_snwprintf(path2, sizeof(path2) / sizeof(wchar_t), L"%s\\Gtk\\bin",
		pidgin_dir);
	path2[sizeof(path2) / sizeof(wchar_t) - 1] = L'\0';
	if (check_for_gtk(path2))
		common_dll_prep(path2);
	else {
		/* set the GTK+ path to be \\path\to\GTK\bin */
		wcscat(path, L"\\GTK\\bin");
		common_dll_prep(path);
	}
}

static wchar_t* winpidgin_lcid_to_posix(LCID lcid) {
	wchar_t *posix = NULL;
	int lang_id = PRIMARYLANGID(lcid);
	int sub_id = SUBLANGID(lcid);

	switch (lang_id) {
		case LANG_AFRIKAANS: posix = L"af"; break;
		case LANG_ARABIC: posix = L"ar"; break;
		case LANG_AZERI: posix = L"az"; break;
		case LANG_BENGALI: posix = L"bn"; break;
		case LANG_BULGARIAN: posix = L"bg"; break;
		case LANG_CATALAN: posix = L"ca"; break;
		case LANG_CZECH: posix = L"cs"; break;
		case LANG_DANISH: posix = L"da"; break;
		case LANG_ESTONIAN: posix = L"et"; break;
		case LANG_PERSIAN: posix = L"fa"; break;
		case LANG_GERMAN: posix = L"de"; break;
		case LANG_GREEK: posix = L"el"; break;
		case LANG_ENGLISH:
			switch (sub_id) {
				case SUBLANG_ENGLISH_UK:
					posix = L"en_GB"; break;
				case SUBLANG_ENGLISH_AUS:
					posix = L"en_AU"; break;
				case SUBLANG_ENGLISH_CAN:
					posix = L"en_CA"; break;
				default:
					posix = L"en"; break;
			}
			break;
		case LANG_SPANISH: posix = L"es"; break;
		case LANG_BASQUE: posix = L"eu"; break;
		case LANG_FINNISH: posix = L"fi"; break;
		case LANG_FRENCH: posix = L"fr"; break;
		case LANG_GALICIAN: posix = L"gl"; break;
		case LANG_GUJARATI: posix = L"gu"; break;
		case LANG_HEBREW: posix = L"he"; break;
		case LANG_HINDI: posix = L"hi"; break;
		case LANG_HUNGARIAN: posix = L"hu"; break;
		case LANG_ICELANDIC: break;
		case LANG_INDONESIAN: posix = L"id"; break;
		case LANG_ITALIAN: posix = L"it"; break;
		case LANG_JAPANESE: posix = L"ja"; break;
		case LANG_GEORGIAN: posix = L"ka"; break;
		case LANG_KANNADA: posix = L"kn"; break;
		case LANG_KOREAN: posix = L"ko"; break;
		case LANG_LITHUANIAN: posix = L"lt"; break;
		case LANG_MACEDONIAN: posix = L"mk"; break;
		case LANG_DUTCH: posix = L"nl"; break;
		case LANG_NEPALI: posix = L"ne"; break;
		case LANG_NORWEGIAN:
			switch (sub_id) {
				case SUBLANG_NORWEGIAN_BOKMAL:
					posix = L"nb"; break;
				case SUBLANG_NORWEGIAN_NYNORSK:
					posix = L"nn"; break;
			}
			break;
		case LANG_PUNJABI: posix = L"pa"; break;
		case LANG_POLISH: posix = L"pl"; break;
		case LANG_PASHTO: posix = L"ps"; break;
		case LANG_PORTUGUESE:
			switch (sub_id) {
				case SUBLANG_PORTUGUESE_BRAZILIAN:
					posix = L"pt_BR"; break;
				default:
				posix = L"pt"; break;
			}
			break;
		case LANG_ROMANIAN: posix = L"ro"; break;
		case LANG_RUSSIAN: posix = L"ru"; break;
		case LANG_SLOVAK: posix = L"sk"; break;
		case LANG_SLOVENIAN: posix = L"sl"; break;
		case LANG_ALBANIAN: posix = L"sq"; break;
		/* LANG_CROATIAN == LANG_SERBIAN == LANG_BOSNIAN */
		case LANG_SERBIAN:
			switch (sub_id) {
				case SUBLANG_SERBIAN_LATIN:
					posix = L"sr@Latn"; break;
				case SUBLANG_SERBIAN_CYRILLIC:
					posix = L"sr"; break;
				case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC:
				case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN:
					posix = L"bs"; break;
				case SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN:
					posix = L"hr"; break;
			}
			break;
		case LANG_SWEDISH: posix = L"sv"; break;
		case LANG_TAMIL: posix = L"ta"; break;
		case LANG_TELUGU: posix = L"te"; break;
		case LANG_THAI: posix = L"th"; break;
		case LANG_TURKISH: posix = L"tr"; break;
		case LANG_UKRAINIAN: posix = L"uk"; break;
		case LANG_VIETNAMESE: posix = L"vi"; break;
		case LANG_XHOSA: posix = L"xh"; break;
		case LANG_CHINESE:
			switch (sub_id) {
				case SUBLANG_CHINESE_SIMPLIFIED:
					posix = L"zh_CN"; break;
				case SUBLANG_CHINESE_TRADITIONAL:
					posix = L"zh_TW"; break;
				default:
					posix = L"zh"; break;
			}
			break;
		case LANG_URDU: break;
		case LANG_BELARUSIAN: break;
		case LANG_LATVIAN: break;
		case LANG_ARMENIAN: break;
		case LANG_FAEROESE: break;
		case LANG_MALAY: break;
		case LANG_KAZAK: break;
		case LANG_KYRGYZ: break;
		case LANG_SWAHILI: break;
		case LANG_UZBEK: break;
		case LANG_TATAR: break;
		case LANG_ORIYA: break;
		case LANG_MALAYALAM: break;
		case LANG_ASSAMESE: break;
		case LANG_MARATHI: break;
		case LANG_SANSKRIT: break;
		case LANG_MONGOLIAN: break;
		case LANG_KONKANI: break;
		case LANG_MANIPURI: break;
		case LANG_SINDHI: break;
		case LANG_SYRIAC: break;
		case LANG_KASHMIRI: break;
		case LANG_DIVEHI: break;
	}

	/* Deal with exceptions */
	if (posix == NULL) {
		switch (lcid) {
			case 0x0455: posix = L"my_MM"; break; /* Myanmar (Burmese) */
			case 9999: posix = L"ku"; break; /* Kurdish (from NSIS) */
		}
	}

	return posix;
}

/* Determine and set Pidgin locale as follows (in order of priority):
   - Check PIDGINLANG env var
   - Check NSIS Installer Language reg value
   - Use default user locale
*/
static const wchar_t *winpidgin_get_locale() {
	const wchar_t *locale = NULL;
	LCID lcid;
	wchar_t data[10];
	DWORD datalen = sizeof(data) / sizeof(wchar_t);

	/* Check if user set PIDGINLANG env var */
	if ((locale = _wgetenv(L"PIDGINLANG")))
		return locale;

	if (!portable_mode && read_reg_string(HKEY_CURRENT_USER, L"SOFTWARE\\pidgin",
			L"Installer Language", (LPBYTE) &data, &datalen)) {
		if ((locale = winpidgin_lcid_to_posix(_wtoi(data))))
			return locale;
	}

	lcid = GetUserDefaultLCID();
	if ((locale = winpidgin_lcid_to_posix(lcid)))
		return locale;

	return L"en";
}

static void winpidgin_set_locale() {
	const wchar_t *locale;
	wchar_t envstr[25];

	locale = winpidgin_get_locale();

	_snwprintf(envstr, sizeof(envstr) / sizeof(wchar_t), L"LANG=%s", locale);
	wprintf(L"Setting locale: %s\n", envstr);
	_wputenv(envstr);
}

#define PIDGIN_WM_FOCUS_REQUEST (WM_APP + 13)
#define PIDGIN_WM_PROTOCOL_HANDLE (WM_APP + 14)

static BOOL winpidgin_set_running(BOOL fail_if_running) {
	HANDLE h;

	if ((h = CreateMutexW(NULL, FALSE, L"pidgin_is_running"))) {
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS) {
			if (fail_if_running) {
				HWND msg_win;

				printf("An instance of Pidgin is already running.\n");

				if((msg_win = FindWindowExW(NULL, NULL, L"WinpidginMsgWinCls", NULL)))
					if(SendMessage(msg_win, PIDGIN_WM_FOCUS_REQUEST, (WPARAM) NULL, (LPARAM) NULL))
						return FALSE;

				/* If we get here, the focus request wasn't successful */

				MessageBoxW(NULL,
					L"An instance of Pidgin is already running",
					NULL, MB_OK | MB_TOPMOST);

				return FALSE;
			}
		} else if (err != ERROR_SUCCESS)
			printf("Error (%u) accessing \"pidgin_is_running\" mutex.\n", (UINT) err);
	}
	return TRUE;
}

#define PROTO_HANDLER_SWITCH L"--protocolhandler="

static void handle_protocol(wchar_t *cmd) {
	char *remote_msg, *utf8msg;
	wchar_t *tmp1, *tmp2;
	int len, wlen;
	SIZE_T len_written;
	HWND msg_win;
	DWORD pid;
	HANDLE process;

	/* The start of the message */
	tmp1 = cmd + wcslen(PROTO_HANDLER_SWITCH);

	/* The end of the message */
	if ((tmp2 = wcschr(tmp1, L' ')))
		wlen = (tmp2 - tmp1);
	else
		wlen = wcslen(tmp1);

	if (wlen == 0) {
		printf("No protocol message specified.\n");
		return;
	}

	if (!(msg_win = FindWindowExW(NULL, NULL, L"WinpidginMsgWinCls", NULL))) {
		printf("Unable to find an instance of Pidgin to handle protocol message.\n");
		return;
	}

	len = WideCharToMultiByte(CP_UTF8, 0, tmp1,
			wlen, NULL, 0, NULL, NULL);
	if (len) {
		utf8msg = malloc(len);
		len = WideCharToMultiByte(CP_UTF8, 0, tmp1,
			wlen, utf8msg, len, NULL, NULL);
	}

	if (len == 0) {
		printf("No protocol message specified.\n");
		return;
	}

	GetWindowThreadProcessId(msg_win, &pid);
	if (!(process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid))) {
		DWORD dw = GetLastError();
		const wchar_t *err_msg = get_win32_error_message(dw);
		wprintf(L"Unable to open Pidgin process. (%u) %s\n", (UINT) dw, err_msg);
		return;
	}

	wprintf(L"Trying to handle protocol message:\n'%.*s'\n", wlen, tmp1);

	/* MEM_COMMIT initializes the memory to zero
	 * so we don't need to worry that our section of utf8msg isn't nul-terminated */
	if ((remote_msg = (char*) VirtualAllocEx(process, NULL, len + 1, MEM_COMMIT, PAGE_READWRITE))) {
		if (WriteProcessMemory(process, remote_msg, utf8msg, len, &len_written)) {
			if (!SendMessageA(msg_win, PIDGIN_WM_PROTOCOL_HANDLE, len_written, (LPARAM) remote_msg))
				printf("Unable to send protocol message to Pidgin instance.\n");
		} else {
			DWORD dw = GetLastError();
			const wchar_t *err_msg = get_win32_error_message(dw);
			wprintf(L"Unable to write to remote memory. (%u) %s\n", (UINT) dw, err_msg);
		}

		VirtualFreeEx(process, remote_msg, 0, MEM_RELEASE);
	} else {
		DWORD dw = GetLastError();
		const wchar_t *err_msg = get_win32_error_message(dw);
		wprintf(L"Unable to allocate remote memory. (%u) %s\n", (UINT) dw, err_msg);
	}

	CloseHandle(process);
	free(utf8msg);
}


int _stdcall
WinMain (struct HINSTANCE__ *hInstance, struct HINSTANCE__ *hPrevInstance,
		char *lpszCmdLine, int nCmdShow) {
	wchar_t errbuf[512];
	wchar_t pidgin_dir[MAX_PATH];
	wchar_t *pidgin_dir_start = NULL;
	wchar_t exe_name[MAX_PATH];
	HMODULE hmod;
	wchar_t *wtmp;
	int pidgin_argc;
	char **pidgin_argv; /* This is in utf-8 */
	int i, j, k;
	BOOL debug = FALSE, help = FALSE, version = FALSE, multiple = FALSE, success;
	LPWSTR *szArglist;
	LPWSTR cmdLine;

	/* If debug or help or version flag used, create console for output */
	for (i = 1; i < __argc; i++) {
		if (strlen(__argv[i]) > 1 && __argv[i][0] == '-') {
			/* check if we're looking at -- or - option */
			if (__argv[i][1] == '-') {
				if (strstr(__argv[i], "--debug") == __argv[i])
					debug = TRUE;
				else if (strstr(__argv[i], "--help") == __argv[i])
					help = TRUE;
				else if (strstr(__argv[i], "--version") == __argv[i])
					version = TRUE;
				else if (strstr(__argv[i], "--multiple") == __argv[i])
					multiple = TRUE;
			} else {
				if (strchr(__argv[i], 'd'))
					debug = TRUE;
				if (strchr(__argv[i], 'h'))
					help = TRUE;
				if (strchr(__argv[i], 'v'))
					version = TRUE;
				if (strchr(__argv[i], 'm'))
					multiple = TRUE;
			}
		}
	}

	/* Permanently enable DEP if the OS supports it */
	if ((hmod = GetModuleHandleW(L"kernel32.dll"))) {
		LPFNSETPROCESSDEPPOLICY MySetProcessDEPPolicy =
			(LPFNSETPROCESSDEPPOLICY)
			GetProcAddress(hmod, "SetProcessDEPPolicy");
		if (MySetProcessDEPPolicy)
			MySetProcessDEPPolicy(1); //PROCESS_DEP_ENABLE
	}

	if (debug || help || version) {
		/* If stdout hasn't been redirected to a file, alloc a console
		 *  (_istty() doesn't work for stuff using the GUI subsystem) */
		if (_fileno(stdout) == -1 || _fileno(stdout) == -2) {
			LPFNATTACHCONSOLE MyAttachConsole = NULL;
			if (hmod)
				MyAttachConsole =
					(LPFNATTACHCONSOLE)
					GetProcAddress(hmod, "AttachConsole");
			if ((MyAttachConsole && MyAttachConsole(ATTACH_PARENT_PROCESS))
					|| AllocConsole()) {
				freopen("CONOUT$", "w", stdout);
				freopen("CONOUT$", "w", stderr);
			}
		}
	}

	cmdLine = GetCommandLineW();

	/* If this is a protocol handler invocation, deal with it accordingly */
	if ((wtmp = wcsstr(cmdLine, PROTO_HANDLER_SWITCH)) != NULL) {
		handle_protocol(wtmp);
		return 0;
	}

	/* Load exception handler if we have it */
	if (GetModuleFileNameW(NULL, pidgin_dir, MAX_PATH) != 0) {

		/* primitive dirname() */
		pidgin_dir_start = wcsrchr(pidgin_dir, L'\\');

		if (pidgin_dir_start) {
			HMODULE hmod;
			pidgin_dir_start[0] = L'\0';

			/* tmp++ will now point to the executable file name */
			wcscpy(exe_name, pidgin_dir_start + 1);

			wcscat(pidgin_dir, L"\\exchndl.dll");
			if ((hmod = LoadLibraryW(pidgin_dir))) {
				typedef void (__cdecl* LPFNSETLOGFILE)(const LPCSTR);
				LPFNSETLOGFILE MySetLogFile;
				/* exchndl.dll is built without UNICODE */
				char debug_dir[MAX_PATH];
				printf("Loaded exchndl.dll\n");
				/* Temporarily override exchndl.dll's logfile
				 * to something sane (Pidgin will override it
				 * again when it initializes) */
				MySetLogFile = (LPFNSETLOGFILE) GetProcAddress(hmod, "SetLogFile");
				if (MySetLogFile) {
					if (GetTempPathA(sizeof(debug_dir), debug_dir) != 0) {
						strcat(debug_dir, "pidgin.RPT");
						printf(" Setting exchndl.dll LogFile to %s\n",
							debug_dir);
						MySetLogFile(debug_dir);
					}
				}
				/* The function signature for SetDebugInfoDir is the same as SetLogFile,
				 * so we can reuse the variable */
				MySetLogFile = (LPFNSETLOGFILE) GetProcAddress(hmod, "SetDebugInfoDir");
				if (MySetLogFile) {
					char *pidgin_dir_ansi = NULL;
					/* Restore pidgin_dir to point to where the executable is */
					pidgin_dir_start[0] = L'\0';
					i = WideCharToMultiByte(CP_ACP, 0, pidgin_dir,
						-1, NULL, 0, NULL, NULL);
					if (i != 0) {
						pidgin_dir_ansi = malloc(i);
						i = WideCharToMultiByte(CP_ACP, 0, pidgin_dir,
							-1, pidgin_dir_ansi, i, NULL, NULL);
						if (i == 0) {
							free(pidgin_dir_ansi);
							pidgin_dir_ansi = NULL;
						}
					}
					if (pidgin_dir_ansi != NULL) {
						_snprintf(debug_dir, sizeof(debug_dir),
							"%s\\pidgin-%s-dbgsym",
							pidgin_dir_ansi,  VERSION);
						debug_dir[sizeof(debug_dir) - 1] = '\0';
						printf(" Setting exchndl.dll DebugInfoDir to %s\n",
							debug_dir);
						MySetLogFile(debug_dir);
						free(pidgin_dir_ansi);
					}
				}

			}

			/* Restore pidgin_dir to point to where the executable is */
			pidgin_dir_start[0] = L'\0';
		}
	} else {
		DWORD dw = GetLastError();
		const wchar_t *err_msg = get_win32_error_message(dw);
		_snwprintf(errbuf, 512,
			L"Error getting module filename.\nError: (%u) %s",
			(UINT) dw, err_msg);
		wprintf(L"%s\n", errbuf);
		MessageBoxW(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
		pidgin_dir[0] = L'\0';
	}

	/* Determine if we're running in portable mode */
	if (wcsstr(cmdLine, L"--portable-mode")
			|| (exe_name != NULL && wcsstr(exe_name, L"-portable.exe"))) {
		printf("Running in PORTABLE mode.\n");
		portable_mode = TRUE;
	}

	if (portable_mode)
		portable_mode_dll_prep(pidgin_dir);
#ifndef IS_WIN32_CROSS_COMPILED
	else if (!getenv("PIDGIN_NO_DLL_CHECK"))
		dll_prep(pidgin_dir);
#endif

	winpidgin_set_locale();

	/* If help, version or multiple flag used, do not check Mutex */
	if (!help && !version)
		if (!winpidgin_set_running(getenv("PIDGIN_MULTI_INST") == NULL && !multiple))
			return 0;

#ifndef IS_WIN32_CROSS_COMPILED
	/* Now we are ready for Pidgin .. */
	wcscat(pidgin_dir, L"\\pidgin.dll");
	if ((hmod = LoadLibraryW(pidgin_dir)))
		pidgin_main = (LPFNPIDGINMAIN) GetProcAddress(hmod, "pidgin_main");
#endif

	/* Restore pidgin_dir to point to where the executable is */
	if (pidgin_dir_start)
		pidgin_dir_start[0] = L'\0';

#ifndef IS_WIN32_CROSS_COMPILED
	if (!pidgin_main) {
		DWORD dw = GetLastError();
		BOOL mod_not_found = (dw == ERROR_MOD_NOT_FOUND || dw == ERROR_DLL_NOT_FOUND);
		const wchar_t *err_msg = get_win32_error_message(dw);

		_snwprintf(errbuf, 512, L"Error loading pidgin.dll.\nError: (%u) %s%s%s",
			(UINT) dw, err_msg,
			mod_not_found ? L"\n" : L"",
			mod_not_found ? L"This probably means that GTK+ can't be found." : L"");
		wprintf(L"%s\n", errbuf);
		MessageBoxW(NULL, errbuf, L"Error", MB_OK | MB_TOPMOST);

		return 0;
	}
#endif

	/* Convert argv to utf-8*/
	szArglist = CommandLineToArgvW(cmdLine, &j);
	pidgin_argc = j;
	pidgin_argv = malloc(pidgin_argc* sizeof(char*));
	k = 0;
	for (i = 0; i < j; i++) {
		success = FALSE;
		/* Remove the --portable-mode arg from the args passed to pidgin so it doesn't choke */
		if (wcsstr(szArglist[i], L"--portable-mode") == NULL) {
			int len = WideCharToMultiByte(CP_UTF8, 0, szArglist[i],
				-1, NULL, 0, NULL, NULL);
			if (len != 0) {
				char *arg = malloc(len);
				len = WideCharToMultiByte(CP_UTF8, 0, szArglist[i],
					-1, arg, len, NULL, NULL);
				if (len != 0) {
					pidgin_argv[k++] = arg;
					success = TRUE;
				}
			}
			if (!success)
				wprintf(L"Error converting argument '%s' to UTF-8\n",
					szArglist[i]);
		}
		if (!success)
			pidgin_argc--;
	}
	LocalFree(szArglist);


	return pidgin_main(hInstance, pidgin_argc, pidgin_argv);
}
