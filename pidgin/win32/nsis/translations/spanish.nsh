;;
;;  spanish.nsh
;;
;;  Spanish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;  Translator: Javier Fernandez-Sanguino Pe�a
;;
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"El entorno de ejecuci�n de GTK+ falta o necesita ser actualizado.$\rPor favor, instale la versi�n v${GTK_MIN_VERSION} del ejecutable GTK+ o alguna posterior."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Siguiente >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) se distribuye bajo la licencia GPL. Esta licencia se incluye aqu� s�lo con prop�sito informativo: $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Cliente de mensajer�a instant�nea de Pidgin (necesario)"
!define GTK_SECTION_TITLE			"Entorno de ejecuci�n de GTK+ (necesario)"
!define PIDGIN_SECTION_DESCRIPTION		"Ficheros y dlls principales de Core"
!define GTK_SECTION_DESCRIPTION		"Una suite de herramientas GUI multiplataforma, utilizada por Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Se ha encontrado una versi�n antig�a del ejecutable de GTK+. �Desea actualizarla?$\rObservaci�n: $(^Name) no funcionar� a menos que lo haga."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite la p�gina Web de Pidgin Windows"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Error al instalar el ejecutable GTK+."
!define GTK_BAD_INSTALL_PATH			"No se pudo acceder o crear la ruta que vd. indic�."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "El desinstalador no pudo encontrar las entradas en el registro de Pidgin.$\rEs probable que otro usuario instalara la aplicaci�n."
!define un.PIDGIN_UNINSTALL_ERROR_2         "No tiene permisos para desinstalar esta aplicaci�n."
