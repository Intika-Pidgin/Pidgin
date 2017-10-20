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

/* This file must remain without any immediate dependencies aka don't link
 * directly to Pidgin, libpidgin, GLib, etc. WinPidgin adds a DLL directory
 * at runtime if needed and dynamically loads libpidgin via LoadLibrary().
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

typedef int (__cdecl* LPFNPIDGINMAIN)(HINSTANCE, int, char**);
typedef BOOL (WINAPI* LPFNSETDLLDIRECTORY)(LPCWSTR);
typedef BOOL (WINAPI* LPFNATTACHCONSOLE)(DWORD);
typedef BOOL (WINAPI* LPFNSETPROCESSDEPPOLICY)(DWORD);

/*
 *  PROTOTYPES
 */
static LPFNPIDGINMAIN pidgin_main = NULL;

static const wchar_t *get_win32_error_message(DWORD err) {
	static wchar_t err_msg[512];

	FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR) &err_msg, sizeof(err_msg) / sizeof(wchar_t), NULL);

	return err_msg;
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
		wprintf(L"Unable to open Pidgin process. (%u) %ls\n",
				(UINT)dw, err_msg);
		return;
	}

	wprintf(L"Trying to handle protocol message:\n'%.*ls'\n", wlen, tmp1);

	/* MEM_COMMIT initializes the memory to zero
	 * so we don't need to worry that our section of utf8msg isn't nul-terminated */
	if ((remote_msg = (char*) VirtualAllocEx(process, NULL, len + 1, MEM_COMMIT, PAGE_READWRITE))) {
		if (WriteProcessMemory(process, remote_msg, utf8msg, len, &len_written)) {
			if (!SendMessageA(msg_win, PIDGIN_WM_PROTOCOL_HANDLE, len_written, (LPARAM) remote_msg))
				printf("Unable to send protocol message to Pidgin instance.\n");
		} else {
			DWORD dw = GetLastError();
			const wchar_t *err_msg = get_win32_error_message(dw);
			wprintf(L"Unable to write to remote memory. (%u) %ls\n",
				       (UINT)dw, err_msg);
		}

		VirtualFreeEx(process, remote_msg, 0, MEM_RELEASE);
	} else {
		DWORD dw = GetLastError();
		const wchar_t *err_msg = get_win32_error_message(dw);
		wprintf(L"Unable to allocate remote memory. (%u) %ls\n",
				(UINT)dw, err_msg);
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

		/* Find parent directory to see if it's bin/ */
		pidgin_dir_start = wcsrchr(pidgin_dir, L'\\');

		/* Add bin/ subdirectory to DLL path if not already in bin/ */
		if (pidgin_dir_start == NULL ||
				wcscmp(pidgin_dir_start + 1, L"bin") != 0) {
			LPFNSETDLLDIRECTORY MySetDllDirectory = NULL;

			hmod = GetModuleHandleW(L"kernel32.dll");

			if (hmod != NULL) {
				MySetDllDirectory = (LPFNSETDLLDIRECTORY) GetProcAddress(hmod, "SetDllDirectoryW");
				if (MySetDllDirectory == NULL) {
					DWORD dw = GetLastError();
					const wchar_t *err_msg = get_win32_error_message(dw);
					wprintf(L"Error loading SetDllDirectory(): (%u) %ls\n", dw, err_msg);
				}
			} else {
				printf("Error getting kernel32.dll handle\n");
			}

			if (MySetDllDirectory) {
				wcscat(pidgin_dir, L"\\bin");
				if (MySetDllDirectory(pidgin_dir)) {
					wprintf(L"Added DLL directory to search path: %ls\n",
							pidgin_dir);
				} else {
					DWORD dw = GetLastError();
					const wchar_t *err_msg = get_win32_error_message(dw);
					wprintf(L"Error calling SetDllDirectory(): (%u) %ls\n", dw, err_msg);
				}
			}
		}
	} else {
		DWORD dw = GetLastError();
		const wchar_t *err_msg = get_win32_error_message(dw);
		_snwprintf(errbuf, sizeof(errbuf) / sizeof(wchar_t),
			L"Error getting module filename.\nError: (%u) %ls",
			(UINT) dw, err_msg);
		wprintf(L"%ls\n", errbuf);
		MessageBoxW(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
		pidgin_dir[0] = L'\0';
	}

	/* If help, version or multiple flag used, do not check Mutex */
	if (!help && !version)
		if (!winpidgin_set_running(getenv("PIDGIN_MULTI_INST") == NULL && !multiple))
			return 0;

	/* Now we are ready for Pidgin .. */
	if ((hmod = LoadLibraryW(LIBPIDGIN_DLL_NAMEW)))
		pidgin_main = (LPFNPIDGINMAIN) GetProcAddress(hmod, "pidgin_main");

	if (!pidgin_main) {
		DWORD dw = GetLastError();
		const wchar_t *err_msg = get_win32_error_message(dw);

		_snwprintf(errbuf, sizeof(errbuf) / sizeof(wchar_t),
				L"Error loading %ls.\nError: (%u) %ls",
				LIBPIDGIN_DLL_NAMEW, (UINT) dw, err_msg);
		wprintf(L"%ls\n", errbuf);
		MessageBoxW(NULL, errbuf, L"Error", MB_OK | MB_TOPMOST);

		return 0;
	}

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
				wprintf(L"Error converting argument '%ls' to UTF-8\n",
					szArglist[i]);
		}
		if (!success)
			pidgin_argc--;
	}
	LocalFree(szArglist);


	return pidgin_main(hInstance, pidgin_argc, pidgin_argv);
}
