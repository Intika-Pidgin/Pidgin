;; vim:syn=winbatch:encoding=cp1252:
;;
;;  german.nsh
;;
;;  German language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Bjoern Voigt <bjoern@cs.tu-berlin.de>, 2008.
;;  Version 3
;;
 
; Startup checks
!define INSTALLER_IS_RUNNING			"Der Installer l�uft schon."
!define PIDGIN_IS_RUNNING			"Eine Instanz von Pidgin l�uft momentan schon. Beenden Sie Pidgin und versuchen Sie es nochmal."
!define GTK_INSTALLER_NEEDED			"Die GTK+ Runtime Umgebung fehlt entweder oder muss aktualisiert werden.$\rBitte installieren Sie v${GTK_MIN_VERSION} oder h�her der GTK+ Runtime"
 
; License Page
!define PIDGIN_LICENSE_BUTTON			"Weiter >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) wird unter der GNU General Public License (GPL) ver�ffentlicht. Die Lizenz dient hier nur der Information. $_CLICK"
 
; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (erforderlich)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Umgebung (erforderlich)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE	"Verkn�pfungen"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmen�"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin-Basisdateien und -DLLs"
!define GTK_SECTION_DESCRIPTION		"Ein Multi-Plattform-GUI-Toolkit, verwendet von Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Verkn�pfungen zum Starten von Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC	"Erstellt eine Verkn�pfung zu Pidgin auf dem Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_DESC	"Erstellt einen Eintrag f�r Pidgin im Startmen�"
 
; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Eine alte Version der GTK+ Runtime wurde gefunden. M�chten Sie aktualisieren?$\rHinweis: $(^Name) funktioniert evtl. nicht, wenn Sie nicht aktualisieren."
!define GTK_WINDOWS_INCOMPATIBLE		"Windows 95/98/Me sind inkompatibel zu GTK+ 2.8.0 oder neuer.  GTK+ ${GTK_INSTALL_VERSION} wird nicht installiert.$\rWenn Sie nicht GTK+ ${GTK_MIN_VERSION} oder neuer installiert haben, wird die Installation jetzt abgebrochen."
 
; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE	"Besuchen Sie die Pidgin Webseite"
 
; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Die aktuell installierte Version von Pidgin kann nicht deinstalliert werden. Die neue Version wird installiert, ohne dass die aktuell installierte Version gel�scht wird."
 
; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fehler beim Installieren der GTK+ Runtime."
!define GTK_BAD_INSTALL_PATH			"Der Pfad, den Sie eingegeben haben, existiert nicht und kann nicht erstellt werden."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI-Behandlung"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Der Deinstaller konnte keine Registrierungsschl�ssel f�r Pidgin finden.$\rEs ist wahrscheinlich, da� ein anderer Benutzer diese Anwendung installiert hat."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Sie haben keine Berechtigung, diese Anwendung zu deinstallieren."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Unterst�tzung f�r Rechtschreibkontrolle"
!define PIDGIN_SPELLCHECK_ERROR		"Fehler bei der Installation der Rechtschreibkontrolle"
!define PIDGIN_SPELLCHECK_DICT_ERROR	"Fehler bei der Installation des W�rterbuches f�r die Rechtschreibkontrolle"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Unterst�tzung f�r Rechtschreibkontrolle.  (F�r die Installation ist eine Internet-Verbindung n�tig)"
!define ASPELL_INSTALL_FAILED			"Installation gescheitert"
!define PIDGIN_SPELLCHECK_BRETON		"Bretonisch"
!define PIDGIN_SPELLCHECK_CATALAN		"Katalanisch"
!define PIDGIN_SPELLCHECK_CZECH		"Tschechisch"
!define PIDGIN_SPELLCHECK_WELSH		"Walisisch"
!define PIDGIN_SPELLCHECK_DANISH		"D�nisch"
!define PIDGIN_SPELLCHECK_GERMAN		"Deutsch"
!define PIDGIN_SPELLCHECK_GREEK		"Griechisch"
!define PIDGIN_SPELLCHECK_ENGLISH		"Englisch"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"Spanisch"
!define PIDGIN_SPELLCHECK_FAROESE		"Far�ersprache"
!define PIDGIN_SPELLCHECK_FRENCH		"Franz�sisch"
!define PIDGIN_SPELLCHECK_ITALIAN		"Italienisch"
!define PIDGIN_SPELLCHECK_DUTCH		"Holl�ndisch"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norwegisch"
!define PIDGIN_SPELLCHECK_POLISH		"Polnisch"
!define PIDGIN_SPELLCHECK_PORTUGUESE	"Portugiesisch"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rum�nisch"
!define PIDGIN_SPELLCHECK_RUSSIAN		"Russisch"
!define PIDGIN_SPELLCHECK_SLOVAK		"Slowakisch"
!define PIDGIN_SPELLCHECK_SWEDISH		"Schwedisch"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainisch"
