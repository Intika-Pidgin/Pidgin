;;
;;  galician.nsh
;;
;;  Galician language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Translator: Ignacio Casal Quinteiro 
;;  Version 1
;;

; Startup GTK+ check

; License Page
!define PIDGIN_LICENSE_BUTTON			"Seguinte >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) distrib�ese baixo a licencia GPL. Esta licencia incl�ese aqu� s� con prop�sito informativo: $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Cliente de mensaxer�a instant�nea de Pidgin (necesario)"
!define GTK_SECTION_TITLE			"Entorno de execuci�n de GTK+ (necesario)"
!define PIDGIN_SECTION_DESCRIPTION		"Ficheiros e dlls principais de Core"
!define GTK_SECTION_DESCRIPTION		"Unha suite de ferramentas GUI multiplataforma, utilizada por Pidgin"


; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a p�xina Web de Pidgin Windows"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "O desinstalador non puido atopar as entradas no rexistro de Pidgin.$\r� probable que outro usuario instalara a aplicaci�n."
!define un.PIDGIN_UNINSTALL_ERROR_2         "Non ten permisos para desinstalar esta aplicaci�n."
