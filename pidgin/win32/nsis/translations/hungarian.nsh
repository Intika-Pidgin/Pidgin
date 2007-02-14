;;
;;  hungarian.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Authors: Sutto Zoltan <suttozoltan@chello.hu>, 2003
;;           Gabor Kelemen <kelemeng@gnome.hu>, 2005
;;

; Startup Checks
!define GTK_INSTALLER_NEEDED			"A GTK+ futtat� k�rnyezet hi�nyzik vagy friss�t�se sz�ks�ges.$\rK�rem telep�tse a v${GTK_MIN_VERSION} vagy magasabb verzi�j� GTK+ futtat� k�rnyezetet."
!define INSTALLER_IS_RUNNING			"A telep�to m�r fut."
!define GAIM_IS_RUNNING				"Jelenleg fut a Gaim egy p�ld�nya. L�pjen ki a Gaimb�l �s azut�n pr�b�lja �jra."

; License Page
!define GAIM_LICENSE_BUTTON			"Tov�bb >"
!define GAIM_LICENSE_BOTTOM_TEXT		"A $(^Name) a GNU General Public License (GPL) alatt ker�l terjeszt�sre. Az itt olvashat� licenc csak t�j�koztat�si c�lt szolg�l. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim azonnali �zeno kliens (sz�ks�ges)"
!define GTK_SECTION_TITLE			"GTK+ futtat� k�rnyezet (sz�ks�ges)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ t�m�k"
!define GTK_NOTHEME_SECTION_TITLE		"Nincs t�ma"
!define GTK_WIMP_SECTION_TITLE			"Wimp t�ma"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve t�ma"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue t�ma"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Parancsikonok"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Asztal"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Men�"
!define GAIM_SECTION_DESCRIPTION		"Gaim f�jlok �s dll-ek"
!define GTK_SECTION_DESCRIPTION			"A Gaim �ltal haszn�lt t�bbplatformos grafikus k�rnyezet"
!define GTK_THEMES_SECTION_DESCRIPTION		"A GTK+ t�m�k megv�ltoztatj�k a GTK+ alkalmaz�sok kin�zet�t."
!define GTK_NO_THEME_DESC			"Ne telep�tse a GTK+ t�m�kat"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows ut�nzat) egy Windows k�rnyezettel harmoniz�l� GTK t�ma."
!define GTK_BLUECURVE_THEME_DESC		"A Bluecurve t�ma."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"A Lighthouseblue t�ma."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Parancsikonok a Gaim ind�t�s�hoz"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Parancsikon l�trehoz�sa a Gaimhoz az asztalon"
!define GAIM_STARTMENU_SHORTCUT_DESC		"Start Men� bejegyz�s l�trehoz�sa a Gaimhoz"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Egy r�gi verzi�j� GTK+ futtat�k�rnyezet van telep�tve. K�v�nja friss�teni?$\rMegjegyz�s: a Gaim nem fog muk�dni, ha nem friss�ti."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"A Windows Gaim weboldal�nak felkeres�se"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (csak elt�vol�t�s)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Hiba a GTK+ futtat�k�rnyezet telep�t�se k�zben."
!define GTK_BAD_INSTALL_PATH			"A megadott el�r�si �t nem �rheto el, vagy nem hozhat� l�tre."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nincs jogosults�ga a GTK+ t�m�k telep�t�s�hez."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Az elt�vol�t� nem tal�lta a Gaim registry bejegyz�seket.$\rVal�sz�n�leg egy m�sik felhaszn�l� telep�tette az alkalmaz�st."
!define un.GAIM_UNINSTALL_ERROR_2		"Nincs jogosults�ga az alkalmaz�s elt�vol�t�s�hoz."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Helyes�r�sellenorz�s t�mogat�sa"
!define GAIM_SPELLCHECK_ERROR			"Hiba a helyes�r�sellenorz�s telep�t�se k�zben"
!define GAIM_SPELLCHECK_DICT_ERROR		"Hiba a helyes�r�sellenorz�si sz�t�r telep�t�se k�zben"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Helyes�r�sellenorz�s t�mogat�sa. (Internetkapcsolat sz�ks�ges a telep�t�shez)"
!define ASPELL_INSTALL_FAILED			"A telep�t�s sikertelen"
!define GAIM_SPELLCHECK_BRETON			"Breton"
!define GAIM_SPELLCHECK_CATALAN			"Katal�n"
!define GAIM_SPELLCHECK_CZECH			"Cseh"
!define GAIM_SPELLCHECK_WELSH			"Walesi"
!define GAIM_SPELLCHECK_DANISH			"D�n"
!define GAIM_SPELLCHECK_GERMAN			"N�met"
!define GAIM_SPELLCHECK_GREEK			"G�r�g"
!define GAIM_SPELLCHECK_ENGLISH			"Angol"
!define GAIM_SPELLCHECK_ESPERANTO		"Eszperant�"
!define GAIM_SPELLCHECK_SPANISH			"Spanyol"
!define GAIM_SPELLCHECK_FAROESE			"Far�ai"
!define GAIM_SPELLCHECK_FRENCH			"Francia"
!define GAIM_SPELLCHECK_ITALIAN			"Olasz"
!define GAIM_SPELLCHECK_DUTCH			"Holland"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norv�g"
!define GAIM_SPELLCHECK_POLISH			"Lengyel"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portug�l"
!define GAIM_SPELLCHECK_ROMANIAN		"Rom�n"
!define GAIM_SPELLCHECK_RUSSIAN			"Orosz"
!define GAIM_SPELLCHECK_SLOVAK			"Szlov�k"
!define GAIM_SPELLCHECK_SWEDISH			"Sv�d"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukr�n"

