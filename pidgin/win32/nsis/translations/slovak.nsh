;;  vim:syn=winbatch:encoding=cp1250:
;;
;;  slovak.nsh
;;
;;  Slovak language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: dominik@internetkosice.sk
;;  Version 2

; Startup Checks
!define INSTALLER_IS_RUNNING			"In�tal�cia je u� spusten�"
!define GAIM_IS_RUNNING				"Gaim je pr�ve spusten�. Vypnite ho a sk�ste znova."
!define GTK_INSTALLER_NEEDED			"GTK+ runtime prostredie ch�ba alebo mus� by� upgradovan�.$\rNain�talujte, pros�m, GTK+ runtime verziu v${GTK_MIN_VERSION}, alebo nov�iu"

; License Page
!define GAIM_LICENSE_BUTTON			"�alej >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) je vydan� pod GPL licenciou. T�to licencia je len pre informa�n� ��ely. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Klient (nevyhnutn�)"
!define GTK_SECTION_TITLE			"GTK+ Runtime prostredie (nevyhnutn�)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ t�my"
!define GTK_NOTHEME_SECTION_TITLE		"�iadna grafick� t�ma"
!define GTK_WIMP_SECTION_TITLE			"Wimp grafick� t�ma"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve grafick� t�ma"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue grafick� t�ma"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Z�stupcovia"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Plocha"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"�tart Menu"
!define GAIM_SECTION_DESCRIPTION		"Jadro Gaim-u a nevyhnutn� DLL s�bory"
!define GTK_SECTION_DESCRIPTION			"Multiplatformov� GUI n�stroje, pou��van� Gaim-om"
!define GTK_THEMES_SECTION_DESCRIPTION		"Pomocou GTK+ grafick�ch t�m m��ete zmeni� vzh�ad GTK+ aplik�ci�."
!define GTK_NO_THEME_DESC			"Nein�talova� GTK+ grafick� t�mu"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) je GTK grafick� t�ma, ktor� pekne lad� s prostred�m Windows."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve grafick� t�ma."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblue grafick� t�ma"
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Z�stupcovia pre Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Vytvori� z�stupcu pre Gaim na pracovnej ploche"
!define GAIM_STARTMENU_SHORTCUT_DESC		"Vytvori� odkaz na Gaim v �tart Menu"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Bola n�jden� star�ia verzia GTK+ runtime. Prajete si upgradova� s��asn� verziu?$\rPozn�mka: Gaim nemus� po upgradovan� fungova� spr�vne."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Nav�t�vi� webstr�nku Windows Gaim"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (len odstr�ni�)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Chyba pri in�tal�cii GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Zadan� cesta nie je pr�stupn� alebo ju nie je mo�n� vytvori�."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nem�te opr�vnenie na in�tal�ciu GTK+ grafickej t�my."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"In�tal�toru sa nepodarilo n�js� polo�ky v registri pre Gaim.$\rJe mo�n�, �e t�to aplik�ciu nain�taloval in� pou��vate�."
!define un.GAIM_UNINSTALL_ERROR_2		"Nem�te opr�vnenie na odin�tal�ciu tejto aplik�cie."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Podpora kontroly pravopisu"
!define GAIM_SPELLCHECK_ERROR			"Chyba pri in�tal�cii kontroly pravopisu"
!define GAIM_SPELLCHECK_DICT_ERROR		"Chyba pri in�tal�cii slovn�ka kontroly pravopisu"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Podpora kontroly pravopisu (Nutn� pripojenie k Internetu)"
!define ASPELL_INSTALL_FAILED			"In�tal�cia zlyhala"
!define GAIM_SPELLCHECK_BRETON			"Bret�nsky"
!define GAIM_SPELLCHECK_CATALAN			"Katal�nsky"
!define GAIM_SPELLCHECK_CZECH			"�esk�"
!define GAIM_SPELLCHECK_WELSH			"Welshsk�"
!define GAIM_SPELLCHECK_DANISH			"D�nsky"
!define GAIM_SPELLCHECK_GERMAN			"Nemeck�"
!define GAIM_SPELLCHECK_GREEK			"Gr�cky"
!define GAIM_SPELLCHECK_ENGLISH			"Anglick�"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperantsk�"
!define GAIM_SPELLCHECK_SPANISH			"�panielsk�"
!define GAIM_SPELLCHECK_FAROESE			"Faroesk�"
!define GAIM_SPELLCHECK_FRENCH			"Franc�zsky"
!define GAIM_SPELLCHECK_ITALIAN			"Taliansk�"
!define GAIM_SPELLCHECK_DUTCH			"Holandsk�"
!define GAIM_SPELLCHECK_NORWEGIAN		"N�rsky"
!define GAIM_SPELLCHECK_POLISH			"Po�sk�"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugalsk�"
!define GAIM_SPELLCHECK_ROMANIAN		"Rumunsk�"
!define GAIM_SPELLCHECK_RUSSIAN			"Rusk�"
!define GAIM_SPELLCHECK_SLOVAK			"Slovensk�"
!define GAIM_SPELLCHECK_SWEDISH			"�v�dsky"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukrajinsk�"

