;;
;;  romanian.nsh
;;
;;  Romanian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Mi�u Moldovan <dumol@gnome.ro>, (c) 2004 - 2005.
;;

; Startup Checks
!define INSTALLER_IS_RUNNING                     "Instalarea este deja pornit�."
!define PIDGIN_IS_RUNNING                  "O instan�� a programului Pidgin este deja pornit�. �nchide�i-o �i �ncerca�i din nou."

; License Page
!define PIDGIN_LICENSE_BUTTON                      "�nainte >"
!define PIDGIN_LICENSE_BOTTOM_TEXT         "$(^Name) are licen�� GPL (GNU Public License). Licen�a este inclus� aici doar pentru scopuri informative. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Client de mesagerie instant (obligatoriu)"
!define GTK_SECTION_TITLE			"Mediu GTK+ (obligatoriu)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Scurt�turi"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Meniu Start"
!define PIDGIN_SECTION_DESCRIPTION		"Fi�iere Pidgin �i dll-uri"
!define GTK_SECTION_DESCRIPTION		"Un mediu de dezvoltare multiplatform� utilizat de Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Scurt�turi pentru pornirea Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Creeaz� iconi�e Pidgin pe Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Creeaz� o intrare Pidgin �n meniul Start"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE               "Vizita�i pagina de web Windows Pidgin"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "Programul de dezinstalare nu a g�sit intr�ri Pidgin �n regi�tri.$\rProbabil un alt utilizator a instalat aceast� aplica�ie."
!define un.PIDGIN_UNINSTALL_ERROR_2         "Nu ave�i drepturile de acces necesare dezinstal�rii acestei aplica�ii."
