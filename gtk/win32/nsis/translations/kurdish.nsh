;;
;;  Kurdish.nsh
;;
;;  Kurdish translation if the language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1254
;;
;;  Erdal Ronahi <erdal.ronahi@gmail.com>

; Make sure to update the GAIM_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Sazker jixwe dime�e."
!define GAIM_IS_RUNNING			"Gaim niha jixwe dime�e. Ji Gaim� derkeve � careke din bicerib�ne."
!define GTK_INSTALLER_NEEDED			"Derdora runtime ya GTK+ an tune an rojanekirina w� p�w�st e. $\rJi kerema xwe v${GTK_MIN_VERSION} an bilindtir a GTK+ saz bike."

; License Page
!define GAIM_LICENSE_BUTTON			"P�� >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) bin l�sansa L�sansa Gelempera Gist� ya GNU (GPL) hatiye we�andin. Ji bo agah�, ev l�sans li vir t� xwendin. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (p�w�st)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (p�w�st)"
!define GTK_THEMES_SECTION_TITLE		"Dirb�n GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Dirb tunebe"
!define GTK_WIMP_SECTION_TITLE		"Dirb� Wimp"
!define GTK_BLUECURVE_SECTION_TITLE	"Dirb� Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Dirb� Light House Blue"
!define GAIM_SHORTCUTS_SECTION_TITLE "Riy�n kin"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE "Sermas�"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE "Menuya destp�k�"
!define GAIM_SECTION_DESCRIPTION		"Dosiy�n cevher� ya Gaim � dll"
!define GTK_SECTION_DESCRIPTION		"Paketa am�r�n GUI ji bo gelek platforman, ji h�la Gaim t� bikaran�n."
!define GTK_THEMES_SECTION_DESCRIPTION	"Dirb�n GTK+ dikarin ruy� bernamey�n GTK biguher�nin."
!define GTK_NO_THEME_DESC			"Dirbeke GTK+ saz neke"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) dirbeyke GTK ya wek� derdora sermasey� ya Windows�."
!define GTK_BLUECURVE_THEME_DESC		"Dirb� Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Dirb� Lighthouseblue."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION   "r�ya kin a ji bo destp�kirina Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC   "r�ya kin a Gaim di sermas�y� de ��ke"
!define GAIM_STARTMENU_SHORTCUT_DESC   "Gaim� biniv�se menuya destp�k"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Guhertoyeke kevn a GTK+ hatiye d�tin. Tu dixwaz� bilind bik�?$\rNot: Heke tu nek�, dibe ku Gaim naxebite."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Were Malpera Gaim a Windows�"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (bi ten� rake)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Di sazkirina GTK+ de �ewt� derket."
!define GTK_BAD_INSTALL_PATH			"r�ya te niv�sand nay� gihi�tin an afirandin."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS	"Dest�ra sazkirina dirbek� GTK+ tune."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Raker t�ketiy�n registry y�n Gaim ned�t. $\rQey bikarh�nereke din v� bername saz kir."
!define un.GAIM_UNINSTALL_ERROR_2		"Dest�ra te ji bo rakirina v� bernamey� tune."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Desteka kontrola rastniv�s�"
!define GAIM_SPELLCHECK_ERROR			"Di sazkirina kontrola rastniv�s� de �ewt� derket."
!define GAIM_SPELLCHECK_DICT_ERROR		"Di sazkirina ferhenga rastniv�s� de �ewt� derket."
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Desteka kontrola rastniv�s�.  (Ji bo sazkirin� �nternet p�w�st e)"
!define ASPELL_INSTALL_FAILED			"Sazkirin Serneket"
!define GAIM_SPELLCHECK_BRETON			"Breton�"
!define GAIM_SPELLCHECK_CATALAN			"Catalan"
!define GAIM_SPELLCHECK_CZECH			"�ek�"
!define GAIM_SPELLCHECK_WELSH			"Welsh"
!define GAIM_SPELLCHECK_DANISH			"Danik�"
!define GAIM_SPELLCHECK_GERMAN			"Alman�"
!define GAIM_SPELLCHECK_GREEK			"Yewnan�"
!define GAIM_SPELLCHECK_ENGLISH			"�ngil�z�"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!define GAIM_SPELLCHECK_SPANISH			"Span�"
!define GAIM_SPELLCHECK_FAROESE			"Faroese"
!define GAIM_SPELLCHECK_FRENCH			"Frans�"
!define GAIM_SPELLCHECK_ITALIAN			"�tal�"
!define GAIM_SPELLCHECK_DUTCH			"Dutch"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norwec�"
!define GAIM_SPELLCHECK_POLISH			"Pol�"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portekiz�"
!define GAIM_SPELLCHECK_ROMANIAN			"Roman�"
!define GAIM_SPELLCHECK_RUSSIAN			"Rus�"
!define GAIM_SPELLCHECK_SLOVAK			"Slovak�"
!define GAIM_SPELLCHECK_SWEDISH			"Sw�d�"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukrayn�"

