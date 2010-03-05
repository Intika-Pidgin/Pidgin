;;
;;  Kurdish.nsh
;;
;;  Kurdish translation if the language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1254
;;
;;  Erdal Ronahi <erdal.ronahi@gmail.com>

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Sazker jixwe dime�e."
!define PIDGIN_IS_RUNNING			"Pidgin niha jixwe dime�e. Ji Pidgin � derkeve � careke din bicerib�ne."

; License Page
!define PIDGIN_LICENSE_BUTTON			"P�� >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) bin l�sansa L�sansa Gelempera Gist� ya GNU (GPL) hatiye we�andin. Ji bo agah�, ev l�sans li vir t� xwendin. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (p�w�st)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (p�w�st)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Riy�n kin"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Sermas�"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Menuya destp�k�"
!define PIDGIN_SECTION_DESCRIPTION		"Dosiy�n cevher� ya Pidgin � dll"
!define GTK_SECTION_DESCRIPTION		"Paketa am�r�n GUI ji bo gelek platforman, ji h�la Pidgin t� bikaran�n."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "r�ya kin a ji bo destp�kirina Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "r�ya kin a Pidgin di sermas�y� de ��ke"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Pidgin � biniv�se menuya destp�k"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Were Malpera Pidgin a Windows�"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Raker t�ketiy�n registry y�n Pidgin ned�t. $\rQey bikarh�nereke din v� bername saz kir."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Dest�ra te ji bo rakirina v� bernamey� tune."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Desteka kontrola rastniv�s�"
!define PIDGIN_SPELLCHECK_ERROR			"Di sazkirina kontrola rastniv�s� de �ewt� derket."
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Desteka kontrola rastniv�s�.  (Ji bo sazkirin� �nternet p�w�st e)"

