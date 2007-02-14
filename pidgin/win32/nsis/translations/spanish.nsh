;;
;;  spanish.nsh
;;
;;  Spanish language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;  Translator: Javier Fernandez-Sanguino Pe�a
;;
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"El entorno de ejecuci�n de GTK+ falta o necesita ser actualizado.$\rPor favor, instale la versi�n v${GTK_MIN_VERSION} del ejecutable GTK+ o alguna posterior."

; License Page
!define GAIM_LICENSE_BUTTON			"Siguiente >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) se distribuye bajo la licencia GPL. Esta licencia se incluye aqu� s�lo con prop�sito informativo: $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Cliente de mensajer�a instant�nea de Gaim (necesario)"
!define GTK_SECTION_TITLE			"Entorno de ejecuci�n de GTK+ (necesario)"
!define GTK_THEMES_SECTION_TITLE		"Temas GTK+" 
!define GTK_NOTHEME_SECTION_TITLE		"Sin tema"
!define GTK_WIMP_SECTION_TITLE		"Tema Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define GAIM_SECTION_DESCRIPTION		"Ficheros y dlls principales de Core"
!define GTK_SECTION_DESCRIPTION		"Una suite de herramientas GUI multiplataforma, utilizada por Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Los temas pueden cambiar la apariencia de aplicaciones GTK+."
!define GTK_NO_THEME_DESC			"No instalar un tema GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) es un tema GTK que se fusiona muy bien con el entorno de escritorio de Windows."
!define GTK_BLUECURVE_THEME_DESC		"El tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"El tema Lighthouseblue."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Se ha encontrado una versi�n antig�a del ejecutable de GTK+. �Desea actualizarla?$\rObservaci�n: Gaim no funcionar� a menos que lo haga."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visite la p�gina Web de Gaim Windows"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (s�lo eliminar)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Error al instalar el ejecutable GTK+."
!define GTK_BAD_INSTALL_PATH			"No se pudo acceder o crear la ruta que vd. indic�."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"No tiene permisos para instalar un tema GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "El desinstalador no pudo encontrar las entradas en el registro de Gaim.$\rEs probable que otro usuario instalara la aplicaci�n."
!define un.GAIM_UNINSTALL_ERROR_2         "No tiene permisos para desinstalar esta aplicaci�n."
