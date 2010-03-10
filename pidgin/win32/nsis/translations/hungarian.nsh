;;
;;  hungarian.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Authors: Sutto Zoltan <suttozoltan@chello.hu>, 2003
;;           Gabor Kelemen <kelemeng@gnome.hu>, 2005
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"A telep�t� m�r fut."
!define PIDGIN_IS_RUNNING				"Jelenleg fut a Pidgin egy p�ld�nya. L�pjen ki a Pidginb�l �s azut�n pr�b�lja �jra."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Tov�bb >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"A $(^Name) a GNU General Public License (GPL) alatt ker�l terjeszt�sre. Az itt olvashat� licenc csak t�j�koztat�si c�lt szolg�l. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin azonnali �zen� kliens (sz�ks�ges)"
!define GTK_SECTION_TITLE			"GTK+ futtat� k�rnyezet (sz�ks�ges)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Parancsikonok"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Asztal"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Men�"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin f�jlok �s dll-ek"
!define GTK_SECTION_DESCRIPTION			"A Pidgin �ltal haszn�lt t�bbplatformos grafikus eszk�zk�szlet"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Parancsikonok a Pidgin ind�t�s�hoz"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Parancsikon l�trehoz�sa a Pidginhez az asztalon"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Start Men� bejegyz�s l�trehoz�sa a Pidginhez"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"A Windows Pidgin weboldal�nak felkeres�se"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"A Pidgin jelenleg telep�tett v�ltozata nem t�vol�that� el. Az �j verzi� a jelenleg telep�tett verzi� elt�vol�t�sa n�lk�l ker�l telep�t�sre. "


; GTK+ Section Prompts

!define URI_HANDLERS_SECTION_TITLE		"URI kezel�k"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Az elt�vol�t� nem tal�lta a Pidgin registry bejegyz�seket.$\rVal�sz�n�leg egy m�sik felhaszn�l� telep�tette az alkalmaz�st."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nincs jogosults�ga az alkalmaz�s elt�vol�t�s�hoz."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Helyes�r�s-ellen�rz�s t�mogat�sa"
!define PIDGIN_SPELLCHECK_ERROR			"Hiba a helyes�r�s-ellen�rz�s telep�t�se k�zben"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Helyes�r�s-ellen�rz�s t�mogat�sa. (Internetkapcsolat sz�ks�ges a telep�t�shez)"

