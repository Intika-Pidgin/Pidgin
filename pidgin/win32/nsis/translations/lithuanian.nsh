;;
;;  lithuanian.nsh
;;
;;  Lithuanian translation strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1257
;;
;;  Version 1

; Startup Checks
!define INSTALLER_IS_RUNNING			"Diegimo programa jau paleista."
!define GAIM_IS_RUNNING				"�iuo metu Gaim yra paleistas. U�darykite �i� program� ir pabandykite i� naujo."
!define GTK_INSTALLER_NEEDED			"GTK+ vykdymo meto aplinkos n�ra arba ji turi b�ti atnaujinta.$\r�diekite v${GTK_MIN_VERSION} arba naujesn� GTK+ vykdymo meto aplinkos versij�"

; License Page
!define GAIM_LICENSE_BUTTON			"Toliau >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) yra i�leistas GNU bendrosios vie�osios licenzijos (GPL) s�lygomis.  Licenzija �ia yra pateikta tik susipa�inimo tikslams. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim pokalbi� klientas (b�tinas)"
!define GTK_SECTION_TITLE			"GTK+ vykdymo meto aplinka (b�tina)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ apipavidalinimai"
!define GTK_NOTHEME_SECTION_TITLE		"Jokio apipavidalinimo"
!define GTK_WIMP_SECTION_TITLE			"Wimp apipavidalinimas"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve apipavidalinimas"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue apipavidalinimas"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Nuorodos"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Darbalaukyje"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Pradiniame meniu"
!define GAIM_SECTION_DESCRIPTION		"Pagrindiniai Gaim failai"
!define GTK_SECTION_DESCRIPTION			"Daugiaplatforminis vartotojo s�sajos priemoni� komplektas, naudojamas Gaim."
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+ apipavidalinimai gali pakeisti GTK+ program� i�vaizd�."
!define GTK_NO_THEME_DESC			"Ne�diegti GTK+ apipavidalinimo."
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows imitatorius) yra gerai Windows aplinkoje derantis GTK apipavidalinimas."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve apipavidalinimas."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblue apipavidalinimas."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Gaim paleidimo nuorodos"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Sukurti nuorod� � Gaim darbastalyje."
!define GAIM_STARTMENU_SHORTCUT_DESC		"Sukurti pradinio meniu �ra��, skirt� Gaim."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Rasta sena GTK+ vykdymo meto aplinkos versija. Ar norite j� atnaujinti?$\rPastaba: skland�iam Gaim darbui atnaujinimas gali b�ti reikalingas."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Aplankyti Windows Gaim tinklalap�"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (tik pa�alinti)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ vykdymo meto aplinkos diegimo klaida."
!define GTK_BAD_INSTALL_PATH			"J�s� nurodytas katalogas neprieinamas ar negali b�ti sukurtas."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Neturite teisi� �diegti GTK+ apipavidalinim�."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"�alinimo programa nerado Gaim registro �ra��.$\rGalb�t kitas vartotojas instaliavo �i� program�."
!define un.GAIM_UNINSTALL_ERROR_2		"Neturite teisi� pa�alinti �ios programos."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Ra�ybos tikrinimo palaikymas"
!define GAIM_SPELLCHECK_ERROR			"Ra�ybos tikrinimo diegimo klaida"
!define GAIM_SPELLCHECK_DICT_ERROR		"Ra�ybos tikrinimo �odyno diegimo klaida"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Ra�ybos tikrinimo palaikymas.  �diegimui b�tina interneto jungtis."
!define ASPELL_INSTALL_FAILED			"Diegimas nepavyko"
!define GAIM_SPELLCHECK_BRETON			"Breton� kalba"
!define GAIM_SPELLCHECK_CATALAN			"Katalon� kalba"
!define GAIM_SPELLCHECK_CZECH			"�ek� kalba"
!define GAIM_SPELLCHECK_WELSH			"Val� kalba"
!define GAIM_SPELLCHECK_DANISH			"Dan� kalba"
!define GAIM_SPELLCHECK_GERMAN			"Vokie�i� kalba"
!define GAIM_SPELLCHECK_GREEK			"Graik� kalba"
!define GAIM_SPELLCHECK_ENGLISH			"Angl� kalba"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto kalba"
!define GAIM_SPELLCHECK_SPANISH			"Ispan� kalba"
!define GAIM_SPELLCHECK_FAROESE			"Farer� kalba"
!define GAIM_SPELLCHECK_FRENCH			"Pranc�z� kalba"
!define GAIM_SPELLCHECK_ITALIAN			"Ital� kalba"
!define GAIM_SPELLCHECK_DUTCH			"Oland� kalba"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norveg� kalba"
!define GAIM_SPELLCHECK_POLISH			"Lenk� kalba"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugal� kalba"
!define GAIM_SPELLCHECK_ROMANIAN		"Rumun� kalba"
!define GAIM_SPELLCHECK_RUSSIAN			"Rus� kalba"
!define GAIM_SPELLCHECK_SLOVAK			"Slovak� kalba"
!define GAIM_SPELLCHECK_SWEDISH			"�ved� kalba"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukrainie�i� kalba"
