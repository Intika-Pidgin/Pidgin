;;
;;  polish.nsh
;;
;;  Polish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Version 3
;;  Note: If translating this file, replace '!insertmacro PIDGIN_MACRO_DEFAULT_STRING'
;;  with '!define'.

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Instalator jest ju� uruchomiony."
!define PIDGIN_IS_RUNNING			"Program Pidgin jest obecnie uruchomiony. Prosz� zako�czy� dzia�anie programu Pidgin i spr�bowa� ponownie."
!define GTK_INSTALLER_NEEDED			"Brak biblioteki GTK+ lub wymaga zaktualizowania.$\rProsz� zainstalowa� wersj� ${GTK_MIN_VERSION} lub wy�sz� biblioteki GTK+"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Dalej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"Program $(^Name) jest rozpowszechniany na warunkach Powszechnej Licencji Publicznej GNU (GPL). Licencja jest tu podana wy��cznie w celach informacyjnych. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Klient komunikatora Pidgin (wymagany)"
!define GTK_SECTION_TITLE			"Biblioteka GTK+ (wymagana, je�li nie jest obecna)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Skr�ty"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Pulpit"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu Start"
!define PIDGIN_SECTION_DESCRIPTION		"G��wne pliki programu Pidgin i biblioteki DLL"
!define GTK_SECTION_DESCRIPTION		"Wieloplatformowy zestaw narz�dzi do tworzenia interfejsu graficznego, u�ywany przez program Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Skr�ty do uruchamiania programu Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Utworzenie skr�tu do programu Pidgin na pulpicie"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Utworzenie wpisu w menu Start dla programu Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Odnaleziono star� wersj� biblioteki GTK+. Zaktualizowa� j�?$\rUwaga: program $(^Name) mo�e bez tego nie dzia�a�."
!define GTK_WINDOWS_INCOMPATIBLE		"Systemy Windows 95/98/Me s� niezgodne z bibliotek� GTK+ 2.8.0 lub nowsz�. Biblioteka GTK+ ${GTK_INSTALL_VERSION} nie zostanie zainstalowana.$\rJe�li brak zainstalowanej biblioteki GTK+ ${GTK_MIN_VERSION} lub nowszej, instalacja zostanie przerwana."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Odwied� stron� WWW programu Pidgin"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Nie mo�na odinstalowa� obecnie zainstalowanej wersji programu Pidgin. Nowa wersja zostanie zainstalowana bez usuwania obecnie zainstalowanej wersji."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"B��d podczas instalowania biblioteki GTK+."
!define GTK_BAD_INSTALL_PATH			"Nie mo�na uzyska� dost�pu do podanej �cie�ki lub jej utworzy�."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"Obs�uga adres�w URI"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Instalator nie mo�e odnale�� wpis�w w rejestrze dla programu Pidgin.$\rMo�liwe, �e inny u�ytkownik zainstalowa� ten program."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Brak uprawnie� do odinstalowania tego programu."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Obs�uga sprawdzania pisowni"
!define PIDGIN_SPELLCHECK_ERROR		"B��d podczas instalowania sprawdzania pisowni"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"B��d podczas instalowania s�ownika dla sprawdzania pisowni"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Obs�uga sprawdzania pisowni (do jej instalacji wymagane jest po��czenie z Internetem)."
!define ASPELL_INSTALL_FAILED			"Instalacja nie powiod�a si�"
!define PIDGIN_SPELLCHECK_BRETON		"breto�ski"
!define PIDGIN_SPELLCHECK_CATALAN		"katalo�ski"
!define PIDGIN_SPELLCHECK_CZECH		"czeski"
!define PIDGIN_SPELLCHECK_WELSH		"walijski"
!define PIDGIN_SPELLCHECK_DANISH		"du�ski"
!define PIDGIN_SPELLCHECK_GERMAN		"niemiecki"
!define PIDGIN_SPELLCHECK_GREEK		"grecki"
!define PIDGIN_SPELLCHECK_ENGLISH		"angielski"
!define PIDGIN_SPELLCHECK_ESPERANTO		"esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"hiszpa�ski"
!define PIDGIN_SPELLCHECK_FAROESE		"farerski"
!define PIDGIN_SPELLCHECK_FRENCH		"francuski"
!define PIDGIN_SPELLCHECK_ITALIAN		"w�oski"
!define PIDGIN_SPELLCHECK_DUTCH		"holenderski"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"norweski"
!define PIDGIN_SPELLCHECK_POLISH		"polski"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"portugalski"
!define PIDGIN_SPELLCHECK_ROMANIAN		"rumu�ski"
!define PIDGIN_SPELLCHECK_RUSSIAN		"rosyjski"
!define PIDGIN_SPELLCHECK_SLOVAK		"s�owacki"
!define PIDGIN_SPELLCHECK_SWEDISH		"szwedzki"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"ukrai�ski"

