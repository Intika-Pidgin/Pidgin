;;
;;  english.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Note: If translating this file, replace '!insertmacro PIDGIN_MACRO_DEFAULT_STRING'
;;  with '!define'.

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!insertmacro PIDGIN_MACRO_DEFAULT_STRING INSTALLER_IS_RUNNING			"The installer is already running."
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_IS_RUNNING			"An instance of Pidgin is currently running.  Please exit Pidgin and try again."
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_INSTALLER_NEEDED			"The GTK+ runtime environment is either missing or needs to be upgraded.$\rPlease install v${GTK_MIN_VERSION} or higher of the GTK+ runtime"

; License Page
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_LICENSE_BUTTON			"Next >"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) is released under the GNU General Public License (GPL). The license is provided here for information purposes only. $_CLICK"

; Components Page
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (required)"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_SECTION_TITLE			"GTK+ Runtime (required if not present)"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SHORTCUTS_SECTION_TITLE		"Shortcuts"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Menu"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SECTION_DESCRIPTION		"Core Pidgin files and dlls"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_SECTION_DESCRIPTION		"A multi-platform GUI toolkit, used by Pidgin"

!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Shortcuts for starting Pidgin"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_DESKTOP_SHORTCUT_DESC		"Create a shortcut to Pidgin on the Desktop"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_STARTMENU_SHORTCUT_DESC		"Create a Start Menu entry for Pidgin"

; GTK+ Directory Page
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_UPGRADE_PROMPT			"An old version of the GTK+ runtime was found. Do you wish to upgrade?$\rNote: $(^Name) may not work unless you do."
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_WINDOWS_INCOMPATIBLE		"Windows 95/98/Me are incompatible with GTK+ 2.8.0 or newer.  GTK+ ${GTK_INSTALL_VERSION} will not be installed.$\rIf you don't have GTK+ ${GTK_MIN_VERSION} or newer already installed, installation will now abort."

; Installer Finish Page
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_FINISH_VISIT_WEB_SITE		"Visit the Pidgin Web Page"

; Pidgin Section Prompts and Texts
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Unable to uninstall the currently installed version of Pidgin. The new version will be installed without removing the currently installed version."

; GTK+ Section Prompts
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_INSTALL_ERROR			"Error installing GTK+ runtime."
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_BAD_INSTALL_PATH			"The path you entered can not be accessed or created."

; URL Handler section
!insertmacro PIDGIN_MACRO_DEFAULT_STRING URI_HANDLERS_SECTION_TITLE		"URI Handlers"

; Uninstall Section Prompts
!insertmacro PIDGIN_MACRO_DEFAULT_STRING un.PIDGIN_UNINSTALL_ERROR_1		"The uninstaller could not find registry entries for Pidgin.$\rIt is likely that another user installed this application."
!insertmacro PIDGIN_MACRO_DEFAULT_STRING un.PIDGIN_UNINSTALL_ERROR_2		"You do not have permission to uninstall this application."

; Spellcheck Section Prompts
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_SECTION_TITLE	"Spellchecking Support"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_ERROR		"Error Installing Spellchecking"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_DICT_ERROR		"Error Installing Spellchecking Dictionary"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Support for Spellchecking.  (Internet connection required for installation)"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING ASPELL_INSTALL_FAILED			"Installation Failed"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_BRETON		"Breton"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_CATALAN		"Catalan"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_CZECH		"Czech"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_WELSH		"Welsh"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_DANISH		"Danish"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_GERMAN		"German"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_GREEK		"Greek"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_ENGLISH		"English"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_SPANISH		"Spanish"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_FAROESE		"Faroese"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_FRENCH		"French"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_ITALIAN		"Italian"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_DUTCH		"Dutch"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_NORWEGIAN		"Norwegian"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_POLISH		"Polish"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_PORTUGUESE		"Portuguese"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_ROMANIAN		"Romanian"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_RUSSIAN		"Russian"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_SLOVAK		"Slovak"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_SWEDISH		"Swedish"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainian"

