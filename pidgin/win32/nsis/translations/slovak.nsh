;;  vim:syn=winbatch:encoding=cp1250:
;;
;;  slovak.nsh
;;
;;  Slovak language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: dominik@internetkosice.sk
;;  Version 2

; Startup Checks
!define INSTALLER_IS_RUNNING			"In�tal�cia je u� spusten�"
!define PIDGIN_IS_RUNNING				"Pidgin je pr�ve spusten�. Vypnite ho a sk�ste znova."
!define GTK_INSTALLER_NEEDED			"GTK+ runtime prostredie ch�ba alebo mus� by� upgradovan�.$\rNain�talujte, pros�m, GTK+ runtime verziu v${GTK_MIN_VERSION}, alebo nov�iu"

; License Page
!define PIDGIN_LICENSE_BUTTON			"�alej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) je vydan� pod GPL licenciou. T�to licencia je len pre informa�n� ��ely. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Klient (nevyhnutn�)"
!define GTK_SECTION_TITLE			"GTK+ Runtime prostredie (nevyhnutn�)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Z�stupcovia"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Plocha"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"�tart Menu"
!define PIDGIN_SECTION_DESCRIPTION		"Jadro Pidgin-u a nevyhnutn� DLL s�bory"
!define GTK_SECTION_DESCRIPTION			"Multiplatformov� GUI n�stroje, pou��van� Pidgin-om"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Z�stupcovia pre Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Vytvori� z�stupcu pre Pidgin na pracovnej ploche"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Vytvori� odkaz na Pidgin v �tart Menu"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Bola n�jden� star�ia verzia GTK+ runtime. Prajete si upgradova� s��asn� verziu?$\rPozn�mka: $(^Name) nemus� po upgradovan� fungova� spr�vne."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Nav�t�vi� webstr�nku Windows Pidgin"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Chyba pri in�tal�cii GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Zadan� cesta nie je pr�stupn� alebo ju nie je mo�n� vytvori�."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"In�tal�toru sa nepodarilo n�js� polo�ky v registri pre Pidgin.$\rJe mo�n�, �e t�to aplik�ciu nain�taloval in� pou��vate�."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nem�te opr�vnenie na odin�tal�ciu tejto aplik�cie."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Podpora kontroly pravopisu"
!define PIDGIN_SPELLCHECK_ERROR			"Chyba pri in�tal�cii kontroly pravopisu"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Chyba pri in�tal�cii slovn�ka kontroly pravopisu"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Podpora kontroly pravopisu (Nutn� pripojenie k Internetu)"
!define ASPELL_INSTALL_FAILED			"In�tal�cia zlyhala"
!define PIDGIN_SPELLCHECK_BRETON			"Bret�nsky"
!define PIDGIN_SPELLCHECK_CATALAN			"Katal�nsky"
!define PIDGIN_SPELLCHECK_CZECH			"�esk�"
!define PIDGIN_SPELLCHECK_WELSH			"Welshsk�"
!define PIDGIN_SPELLCHECK_DANISH			"D�nsky"
!define PIDGIN_SPELLCHECK_GERMAN			"Nemeck�"
!define PIDGIN_SPELLCHECK_GREEK			"Gr�cky"
!define PIDGIN_SPELLCHECK_ENGLISH			"Anglick�"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperantsk�"
!define PIDGIN_SPELLCHECK_SPANISH			"�panielsk�"
!define PIDGIN_SPELLCHECK_FAROESE			"Faroesk�"
!define PIDGIN_SPELLCHECK_FRENCH			"Franc�zsky"
!define PIDGIN_SPELLCHECK_ITALIAN			"Taliansk�"
!define PIDGIN_SPELLCHECK_DUTCH			"Holandsk�"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"N�rsky"
!define PIDGIN_SPELLCHECK_POLISH			"Po�sk�"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugalsk�"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumunsk�"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Rusk�"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slovensk�"
!define PIDGIN_SPELLCHECK_SWEDISH			"�v�dsky"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrajinsk�"

