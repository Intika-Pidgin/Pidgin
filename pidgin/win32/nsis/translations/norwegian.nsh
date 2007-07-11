;;
;;  norwegian.nsh
;;
;;  Norwegian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  J�rgen_Vinne_Iversen <jorgenvi@tihlde.org>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installeren kj�rer allerede."
!define PIDGIN_IS_RUNNING				"En instans av Pidgin kj�rer fra f�r. Avslutt Pidgin og pr�v igjen."
!define GTK_INSTALLER_NEEDED			"GTK+ runtime environment mangler eller trenger en oppgradering.$\rVennligst install�r GTK+ v${GTK_MIN_VERSION} eller h�yere"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Neste >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) er utgitt under GPL (GNU General Public License). Lisensen er oppgitt her kun med henblikk p� informasjon. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Hurtigmeldingsklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Snarveier"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Skrivebord"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmeny"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgins kjernefiler og dll'er"
!define GTK_SECTION_DESCRIPTION			"Et GUI-verkt�y for flere ulike plattformer, brukes av Pidgin."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Snarveier for � starte Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Lag en snarvei til Pidgin p� Skrivebordet"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Legg til Pidgin i Startmenyen"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"En eldre versjon av GTK+ runtime ble funnet. �nsker du � oppgradere?$\rMerk: $(^Name) vil kanskje ikke virke hvis du ikke oppgraderer."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Bes�k Pidgin for Windows' Nettside"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"En feil oppstod ved installering av GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Stien du oppga kan ikke aksesseres eller lages."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Avinstalleringsprogrammet kunne ikke finne noen registeroppf�ring for Pidgin.$\rTrolig har en annen bruker avinstallert denne applikasjonen."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Du har ikke rettigheter til � avinstallere denne applikasjonen."



; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"St�tte for stavekontroll"
!define PIDGIN_SPELLCHECK_ERROR			"Det oppstod en feil ved installering av stavekontroll"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Det oppstod en feil ved installering av ordboken for stavekontroll"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"St�tte for stavekontroll. (Internettoppkobling p�krevd for installasjon)"
!define ASPELL_INSTALL_FAILED			"Installasjonen mislyktes."
!define PIDGIN_SPELLCHECK_BRETON			"Bretagnsk"
!define PIDGIN_SPELLCHECK_CATALAN			"Katalansk"
!define PIDGIN_SPELLCHECK_CZECH			"Tsjekkisk"
!define PIDGIN_SPELLCHECK_WELSH			"Walisisk"
!define PIDGIN_SPELLCHECK_DANISH			"Dansk"
!define PIDGIN_SPELLCHECK_GERMAN			"Tysk"
!define PIDGIN_SPELLCHECK_GREEK			"Gresk"
!define PIDGIN_SPELLCHECK_ENGLISH			"Engelsk"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH			"Spansk"
!define PIDGIN_SPELLCHECK_FAROESE			"F�r�ysk"
!define PIDGIN_SPELLCHECK_FRENCH			"Fransk"
!define PIDGIN_SPELLCHECK_ITALIAN			"Italiensk"
!define PIDGIN_SPELLCHECK_DUTCH			"Nederlandsk"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norsk"
!define PIDGIN_SPELLCHECK_POLISH			"Polsk"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugisisk"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumensk"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Russisk"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slovakisk"
!define PIDGIN_SPELLCHECK_SWEDISH			"Svensk"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainsk"
