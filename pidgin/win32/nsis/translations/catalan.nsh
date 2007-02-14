;;  vim:syn=winbatch:encoding=cp1252:
;;
;;  catalan.nsh
;;
;;  Catalan language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: "Bernat L�pez" <bernatl@adequa.net>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"L'instal.lador encara est� executant-se."
!define GAIM_IS_RUNNING				"Hi ha una inst�ncia del Gaim executant-se. Surt del Gaim i torna a intentar-ho."
!define GTK_INSTALLER_NEEDED			"L'entorn d'execuci� GTK+ no existeix o necessita �sser actualitzat.$\rSius plau instal.la la versi�${GTK_MIN_VERSION} o superior de l'entonr GTK+"

; License Page
!define GAIM_LICENSE_BUTTON			"Seg�ent >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) �s distribu�t sota llic�ncia GPL. Podeu consultar la llic�ncia, nom�s per proposits informatius, aqu�.  $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Client Gaim de Missatgeria Instant�nia (necessari)"
!define GTK_SECTION_TITLE			"Entorn d'Execuci� GTK+ (necessari)"
!define GTK_THEMES_SECTION_TITLE		"Temes GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Sense tema"
!define GTK_WIMP_SECTION_TITLE			"Tema Imwi"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Corba Blava"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Enlla�os directes"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Escriptori"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu Inici"
!define GAIM_SECTION_DESCRIPTION		"Fitxers i dlls del nucli de Gaim"
!define GTK_SECTION_DESCRIPTION			"Una eina IGU multiplataforma, utilitzada per Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION		"Els temes GTK+ poden modificar l'aspecte de les aplicacions GTK+."
!define GTK_NO_THEME_DESC			"No instal.lis un tema GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Imwi (imitador Windows) �s un tema GTK que s'integra perfectament en un entorn d'escriptori Windows."
!define GTK_BLUECURVE_THEME_DESC		"El tema Corba Blava."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"The Lighthouseblue theme."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Enlla�os directes per iniciar el Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Afegir un enlla� directe al Gaim a l'Escriptori"
!define GAIM_STARTMENU_SHORTCUT_DESC		"Crear una entrada Gaim al Menu Inici"


; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"S'ha trobat una versi� antiga de l'entorn d'execuci� GTK. Vols actualitzar-la?$\rNota: Gaim no funcionar� sino ho fas."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visita la p�gina web de Gaim per Windows"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (nom�s esborrar)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Error installlant l'entorn d'execuci� GTK+."
!define GTK_BAD_INSTALL_PATH			"El directori que has introdu�t no pot �sser accedit o creat."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"No tens permisos per instal.lar un tema GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"L'instal.lador podria no trobar les entrades del registre de Gaim.$\rProbablement un altre usuari ha instal.lat aquesta aplicaci�."
!define un.GAIM_UNINSTALL_ERROR_2		"No tens perm�s per desinstal.lar aquesta aplicaci�."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Suport a la Verificaci� de l'Ortografia "
!define GAIM_SPELLCHECK_ERROR			"Error instal.lant verificaci� de l'ortografia"
!define GAIM_SPELLCHECK_DICT_ERROR		"Error Instal.lant Diccionari  per a Verificaci� de l'Ortografia"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Suport per a Verificaci� de l'Ortografia.  (�s necesaria connexi� a internet per dur a terme la instal.laci�)"
!define ASPELL_INSTALL_FAILED			"La instal.laci� ha fallat"
!define GAIM_SPELLCHECK_BRETON			"Bret�"
!define GAIM_SPELLCHECK_CATALAN			"Catal�"
!define GAIM_SPELLCHECK_CZECH			"Txec"
!define GAIM_SPELLCHECK_WELSH			"Gal�l�s"
!define GAIM_SPELLCHECK_DANISH			"Dan�s"
!define GAIM_SPELLCHECK_GERMAN			"Alemany"
!define GAIM_SPELLCHECK_GREEK			"Grec"
!define GAIM_SPELLCHECK_ENGLISH			"Angl�s"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!define GAIM_SPELLCHECK_SPANISH			"Espanyol"
!define GAIM_SPELLCHECK_FAROESE			"Fero�s"
!define GAIM_SPELLCHECK_FRENCH			"Franc�s"
!define GAIM_SPELLCHECK_ITALIAN			"Itali�"
!define GAIM_SPELLCHECK_DUTCH			"Holand�s"
!define GAIM_SPELLCHECK_NORWEGIAN		"Noruec"
!define GAIM_SPELLCHECK_POLISH			"Polon�s"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugu�s"
!define GAIM_SPELLCHECK_ROMANIAN		"Roman�s"
!define GAIM_SPELLCHECK_RUSSIAN			"Rus"
!define GAIM_SPELLCHECK_SLOVAK			"Eslovac"
!define GAIM_SPELLCHECK_SWEDISH			"Suec"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ucra�n�s"

