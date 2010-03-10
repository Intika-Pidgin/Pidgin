;;
;;  norwegian_nynorsk.nsh
;;
;;  Norwegian nynorsk language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installasjonsprogrammet kj�rer allereie."
!define PIDGIN_IS_RUNNING			"Pidgin kj�rer no. Lukk programmet og pr�v igjen."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Neste >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) blir utgjeve med ein GNU General Public License (GPL). Lisensen er berre gjeven her for opplysningsform�l. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin lynmeldingsklient (p�kravd)"
!define GTK_SECTION_TITLE			"GTK+-kj�remilj� (p�kravd om det ikkje er til stades no)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Snarvegar"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Skrivebordet"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmenyen"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin programfiler og DLL-ar"
!define GTK_SECTION_DESCRIPTION		"Ei grafisk brukargrensesnittverkt�ykasse p� fleire plattformer som Pidgin nyttar"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Snarvegar for � starta Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Lag ein snarveg til Pidgin p� skrivebordet"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Lag ein snarveg til Pidgin p� startmenyen"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Bes�k Pidgin si nettside"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Klarte ikkje � avinstallera Pidgin-utg�va som er i bruk. Den nye utg�va kjem til � bli installert utan � ta vekk den gjeldande."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI-referanse"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Avinstallasjonsprogrammet fann ikkje registerpostar for Pidgin.$\rTruleg har ein annan brukar installert denne applikasjonen."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Du har ikkje l�yve til � kunna avinstallera denne applikasjonen."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Stavekontrollhjelp"
!define PIDGIN_SPELLCHECK_ERROR		"Klarte ikkje � installera stavekontrollen"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Klarte ikkje � installera stavekontrollordlista"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Stavekontrollhjelp (treng internettsamband for � installera)."
!define ASPELL_INSTALL_FAILED			"Installasjonen feila"
!define PIDGIN_SPELLCHECK_BRETON		"Bretonsk"
!define PIDGIN_SPELLCHECK_CATALAN		"Katalansk"
!define PIDGIN_SPELLCHECK_CZECH		"Tsjekkisk"
!define PIDGIN_SPELLCHECK_WELSH		"Walisisk"
!define PIDGIN_SPELLCHECK_DANISH		"Dansk"
!define PIDGIN_SPELLCHECK_GERMAN		"Tysk"
!define PIDGIN_SPELLCHECK_GREEK		"Gresk"
!define PIDGIN_SPELLCHECK_ENGLISH		"Engelsk"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"Spansk"
!define PIDGIN_SPELLCHECK_FAROESE		"F�r�ysk"
!define PIDGIN_SPELLCHECK_FRENCH		"Fransk"
!define PIDGIN_SPELLCHECK_ITALIAN		"Italiensk"
!define PIDGIN_SPELLCHECK_DUTCH		"Nederlandsk"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norsk"
!define PIDGIN_SPELLCHECK_POLISH		"Polsk"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugisisk"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumensk"
!define PIDGIN_SPELLCHECK_RUSSIAN		"Russisk"
!define PIDGIN_SPELLCHECK_SLOVAK		"Slovakisk"
!define PIDGIN_SPELLCHECK_SWEDISH		"Svensk"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainsk"

