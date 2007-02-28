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
!define PIDGIN_IS_RUNNING				"Jelenleg fut a Gaim egy p�ld�nya. L�pjen ki a Gaimb�l �s azut�n pr�b�lja �jra."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Tov�bb >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"A $(^Name) a GNU General Public License (GPL) alatt ker�l terjeszt�sre. Az itt olvashat� licenc csak t�j�koztat�si c�lt szolg�l. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Gaim azonnali �zeno kliens (sz�ks�ges)"
!define GTK_SECTION_TITLE			"GTK+ futtat� k�rnyezet (sz�ks�ges)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ t�m�k"
!define GTK_NOTHEME_SECTION_TITLE		"Nincs t�ma"
!define GTK_WIMP_SECTION_TITLE			"Wimp t�ma"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve t�ma"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue t�ma"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Parancsikonok"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Asztal"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Men�"
!define PIDGIN_SECTION_DESCRIPTION		"Gaim f�jlok �s dll-ek"
!define GTK_SECTION_DESCRIPTION			"A Gaim �ltal haszn�lt t�bbplatformos grafikus k�rnyezet"
!define GTK_THEMES_SECTION_DESCRIPTION		"A GTK+ t�m�k megv�ltoztatj�k a GTK+ alkalmaz�sok kin�zet�t."
!define GTK_NO_THEME_DESC			"Ne telep�tse a GTK+ t�m�kat"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows ut�nzat) egy Windows k�rnyezettel harmoniz�l� GTK t�ma."
!define GTK_BLUECURVE_THEME_DESC		"A Bluecurve t�ma."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"A Lighthouseblue t�ma."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Parancsikonok a Gaim ind�t�s�hoz"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Parancsikon l�trehoz�sa a Gaimhoz az asztalon"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Start Men� bejegyz�s l�trehoz�sa a Gaimhoz"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Egy r�gi verzi�j� GTK+ futtat�k�rnyezet van telep�tve. K�v�nja friss�teni?$\rMegjegyz�s: a Gaim nem fog muk�dni, ha nem friss�ti."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"A Windows Gaim weboldal�nak felkeres�se"

; Gaim Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"Gaim (csak elt�vol�t�s)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Hiba a GTK+ futtat�k�rnyezet telep�t�se k�zben."
!define GTK_BAD_INSTALL_PATH			"A megadott el�r�si �t nem �rheto el, vagy nem hozhat� l�tre."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nincs jogosults�ga a GTK+ t�m�k telep�t�s�hez."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Az elt�vol�t� nem tal�lta a Gaim registry bejegyz�seket.$\rVal�sz�n�leg egy m�sik felhaszn�l� telep�tette az alkalmaz�st."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nincs jogosults�ga az alkalmaz�s elt�vol�t�s�hoz."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Helyes�r�sellenorz�s t�mogat�sa"
!define PIDGIN_SPELLCHECK_ERROR			"Hiba a helyes�r�sellenorz�s telep�t�se k�zben"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Hiba a helyes�r�sellenorz�si sz�t�r telep�t�se k�zben"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Helyes�r�sellenorz�s t�mogat�sa. (Internetkapcsolat sz�ks�ges a telep�t�shez)"
!define ASPELL_INSTALL_FAILED			"A telep�t�s sikertelen"
!define PIDGIN_SPELLCHECK_BRETON			"Breton"
!define PIDGIN_SPELLCHECK_CATALAN			"Katal�n"
!define PIDGIN_SPELLCHECK_CZECH			"Cseh"
!define PIDGIN_SPELLCHECK_WELSH			"Walesi"
!define PIDGIN_SPELLCHECK_DANISH			"D�n"
!define PIDGIN_SPELLCHECK_GERMAN			"N�met"
!define PIDGIN_SPELLCHECK_GREEK			"G�r�g"
!define PIDGIN_SPELLCHECK_ENGLISH			"Angol"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Eszperant�"
!define PIDGIN_SPELLCHECK_SPANISH			"Spanyol"
!define PIDGIN_SPELLCHECK_FAROESE			"Far�ai"
!define PIDGIN_SPELLCHECK_FRENCH			"Francia"
!define PIDGIN_SPELLCHECK_ITALIAN			"Olasz"
!define PIDGIN_SPELLCHECK_DUTCH			"Holland"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norv�g"
!define PIDGIN_SPELLCHECK_POLISH			"Lengyel"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portug�l"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rom�n"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Orosz"
!define PIDGIN_SPELLCHECK_SLOVAK			"Szlov�k"
!define PIDGIN_SPELLCHECK_SWEDISH			"Sv�d"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukr�n"

