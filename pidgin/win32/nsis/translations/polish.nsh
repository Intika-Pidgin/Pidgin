;;
;;  polish.nsh
;;
;;  Polish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Jan Eldenmalm <jan.eldenmalm@amazingports.com>
;;  Version 2
;;


; License Page
!define PIDGIN_LICENSE_BUTTON			"Dalej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) jest wydzielone w licencji GPL. Udziela si� licencji wy��cznie do cel�w informacyjnych. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Wymagany jest Pidgin Instant Messaging Client"
!define GTK_SECTION_TITLE			"Wymagany jest runtime �rodowiska GTK+"
!define PIDGIN_SECTION_DESCRIPTION		"Zbiory Core Pidgin oraz dll"
!define GTK_SECTION_DESCRIPTION		"Wieloplatformowe narz�dzie GUI, u�ywane w Pidgin"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Wejd� na stron� Pidgin Web Page"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Deinstalator nie mo�e znale�� rejestr�w dla Pidgin.$\r Wskazuje to na to, �e instalacj� przeprowadzi� inny u�ytkownik."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nie masz uprawnie� do deinstalacji tej aplikacji."
