;; vim:syn=winbatch:encoding=cp1252:
;;
;;  german.nsh
;;
;;  German language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Bjoern Voigt <bjoern@cs.tu-berlin.de>, 2006.
;;  Version 3
;;
 
; Startup checks
!define INSTALLER_IS_RUNNING			"Der Installer l�uft schon."
!define PIDGIN_IS_RUNNING				"Eine Instanz von Gaim l�uft momentan schon. Beenden Sie Gaim und versuchen Sie es nochmal."
!define GTK_INSTALLER_NEEDED			"Die GTK+ Runtime Umgebung fehlt entweder oder mu� aktualisiert werden.$\rBitte installieren Sie v${GTK_MIN_VERSION} oder h�her der GTK+ Runtime"
 
; License Page
!define PIDGIN_LICENSE_BUTTON			"Weiter >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) wird unter der GNU General Public License (GPL) ver�ffentlicht. Die Lizenz dient hier nur der Information. $_CLICK"
 
; Components Page
!define PIDGIN_SECTION_TITLE			"Gaim Instant Messaging Client (erforderlich)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Umgebung (erforderlich)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ Themen"
!define GTK_NOTHEME_SECTION_TITLE		"Kein Thema"
!define GTK_WIMP_SECTION_TITLE		"Wimp Thema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve Thema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue Thema"
!define PIDGIN_SHORTCUTS_SECTION_TITLE	"Verkn�pfungen"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmen�"
!define PIDGIN_SECTION_DESCRIPTION		"Gaim-Basisdateien und -DLLs"
!define GTK_SECTION_DESCRIPTION		"Ein Multi-Plattform-GUI-Toolkit, verwendet von Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ Themen k�nnen Aussehen und Bedienung von GTK+ Anwendungen ver�ndern."
!define GTK_NO_THEME_DESC			"Installiere kein GTK+ Thema"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows Imitator) ist ein GTK Theme, das sich besonders gut in den Windows Desktop integriert."
!define GTK_BLUECURVE_THEME_DESC		"Das Bluecurve Thema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Das Lighthouseblue Thema."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Verkn�pfungen zum Starten von Gaim"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Erstellt eine Verkn�pfung zu Gaim auf dem Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Erstellt einen Eintrag f�r Gaim im Startmen�"
 
; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Eine alte Version der GTK+ Runtime wurde gefunden. M�chten Sie aktualisieren?$\rHinweis: Gaim funktioniert evtl. nicht, wenn Sie nicht aktualisieren."
 
; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Besuchen Sie die Windows Gaim Webseite"
 
; Gaim Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"Gaim (nur entfernen)"
 
; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fehler beim Installieren der GTK+ Runtime."
!define GTK_BAD_INSTALL_PATH			"Der Pfad, den Sie eingegeben haben, existiert nicht und kann nicht erstellt werden."
 
; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Sie haben keine Berechtigung, um ein GTK+ Theme zu installieren."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Der Deinstaller konnte keine Registrierungsschl�ssel f�r Gaim finden.$\rEs ist wahrscheinlich, da� ein anderer Benutzer diese Anwendunng installiert hat."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Sie haben keine Berechtigung, diese Anwendung zu deinstallieren."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Unterst�tzung f�r Rechtschreibkontrolle"
!define PIDGIN_SPELLCHECK_ERROR			"Fehler bei der Installation der Rechtschreibkontrolle"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Fehler bei der Installation des W�rterbuches f�r die Rechtschreibkontrolle"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Unterst�tzung f�r Rechtschreibkontrolle.  (F�r die Installation ist eine Internet-Verbindung n�tig)"
!define ASPELL_INSTALL_FAILED			"Installation gescheitert"
!define PIDGIN_SPELLCHECK_BRETON			"Bretonisch"
!define PIDGIN_SPELLCHECK_CATALAN			"Katalanisch"
!define PIDGIN_SPELLCHECK_CZECH			"Tschechisch"
!define PIDGIN_SPELLCHECK_WELSH			"Walisisch"
!define PIDGIN_SPELLCHECK_DANISH			"D�nisch"
!define PIDGIN_SPELLCHECK_GERMAN			"Deutsch"
!define PIDGIN_SPELLCHECK_GREEK			"Griechisch"
!define PIDGIN_SPELLCHECK_ENGLISH			"Englisch"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH			"Spanisch"
!define PIDGIN_SPELLCHECK_FAROESE			"Far�ersprache"
!define PIDGIN_SPELLCHECK_FRENCH			"Franz�sisch"
!define PIDGIN_SPELLCHECK_ITALIAN			"Italienisch"
!define PIDGIN_SPELLCHECK_DUTCH			"Holl�ndisch"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norwegisch"
!define PIDGIN_SPELLCHECK_POLISH			"Polnisch"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugiesisch"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rum�nisch"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Russisch"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slowakisch"
!define PIDGIN_SPELLCHECK_SWEDISH			"Schwedisch"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainisch"
