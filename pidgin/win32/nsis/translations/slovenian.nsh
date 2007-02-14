;;
;;  slovenian.nsh
;;
;;  Slovenian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Martin Srebotnjak <miles@filmsi.net>
;;  Version 3
;;

; Startup GTK+ check
!define INSTALLER_IS_RUNNING			"Name��anje �e poteka."
!define GAIM_IS_RUNNING				"Trenutno �e te�e razli�ica Gaima. Prosimo zaprite Gaim in poskusite znova."
!define GTK_INSTALLER_NEEDED			"Izvajalno okolje GTK+ manjka ali pa ga je potrebno nadgraditi.$\rProsimo namestite v${GTK_MIN_VERSION} ali vi�jo razli�ico izvajalnega okolja GTK+"

; License Page
!define GAIM_LICENSE_BUTTON			"Naprej >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) je na voljo pod licenco GPL. Ta licenca je tu na voljo le v informativne namene. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim - odjemalec za klepet (zahtevano)"
!define GTK_SECTION_TITLE			"GTK+ izvajalno okolje (zahtevano)"
!define GTK_THEMES_SECTION_TITLE		"Teme GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Brez teme"
!define GTK_WIMP_SECTION_TITLE			"Tema Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Bli�njice"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Namizje"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Za�etni meni"
!define GAIM_SECTION_DESCRIPTION		"Temeljne datoteke Gaima"
!define GTK_SECTION_DESCRIPTION			"Ve�platformna orodjarna GUI, ki jo uporablja Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION		"Teme GTK+ lahko spremenijo izgled programov GTK+."
!define GTK_NO_THEME_DESC			"Brez namestitve teme GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (opona�evalec Oken) je tema GTK, ki se lepo vklaplja v namizno okolje Windows."
!define GTK_BLUECURVE_THEME_DESC		"Tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Tema Lighthouseblue."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Bli�njice za zagon Gaima"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Ustvari bli�njico za Gaim na namizju"
!define GAIM_STARTMENU_SHORTCUT_DESC		"Ustvari vnos Gaim v meniju Start"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Na�el sem starej�o razli�ico izvajalnega okolja GTK+. Jo �elite nadgraditi?$\rOpomba: �e je ne boste nadgradili, Gaim morda ne bo deloval."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Obi��ite spletno stran Windows Gaim"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (samo odstrani)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Napaka pri namestitvi izvajalnega okolja GTK+."
!define GTK_BAD_INSTALL_PATH			"Pot, ki ste jo vnesli, ni dosegljiva ali je ni mogo�e ustvariti."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Za namestitev teme GTK+ nimate ustreznih pravic."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Ne morem najti vnosov v registru za Gaim.$\rNajverjetneje je ta program namestil drug uporabnik."
!define un.GAIM_UNINSTALL_ERROR_2		"Za odstranitev programa nimate ustreznih pravic."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Podpora preverjanja �rkovanja"
!define GAIM_SPELLCHECK_ERROR			"Napaka pri name��anju preverjanja �rkovanja"
!define GAIM_SPELLCHECK_DICT_ERROR		"Napaka pri name��anju slovarja za preverjanje �rkovanja"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Podpora preverjanja �rkovanja.  (Za namestitev je potrebna spletna povezava)"
!define ASPELL_INSTALL_FAILED			"Namestitev ni uspela."
!define GAIM_SPELLCHECK_BRETON			"bretonski"
!define GAIM_SPELLCHECK_CATALAN			"katalonski"
!define GAIM_SPELLCHECK_CZECH			"�e�ki"
!define GAIM_SPELLCHECK_WELSH			"vel�ki"
!define GAIM_SPELLCHECK_DANISH			"danski"
!define GAIM_SPELLCHECK_GERMAN			"nem�ki"
!define GAIM_SPELLCHECK_GREEK			"gr�ki"
!define GAIM_SPELLCHECK_ENGLISH			"angle�ki"
!define GAIM_SPELLCHECK_ESPERANTO		"esperantski"
!define GAIM_SPELLCHECK_SPANISH			"�panski"
!define GAIM_SPELLCHECK_FAROESE			"farojski"
!define GAIM_SPELLCHECK_FRENCH			"francoski"
!define GAIM_SPELLCHECK_ITALIAN			"italijanski"
!define GAIM_SPELLCHECK_DUTCH			"nizozemski"
!define GAIM_SPELLCHECK_NORWEGIAN		"norve�ki"
!define GAIM_SPELLCHECK_POLISH			"poljski"
!define GAIM_SPELLCHECK_PORTUGUESE		"portugalski"
!define GAIM_SPELLCHECK_ROMANIAN		"romunski"
!define GAIM_SPELLCHECK_RUSSIAN			"ruski"
!define GAIM_SPELLCHECK_SLOVAK			"slova�ki"
!define GAIM_SPELLCHECK_SLOVENIAN		"slovenski"
!define GAIM_SPELLCHECK_SWEDISH			"�vedski"
!define GAIM_SPELLCHECK_UKRAINIAN		"ukrajinski"
