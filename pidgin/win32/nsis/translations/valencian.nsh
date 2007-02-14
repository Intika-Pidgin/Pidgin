;;
;;  valencian.nsh
;;
;;  Valencian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Note: If translating this file, replace "!insertmacro GAIM_MACRO_DEFAULT_STRING"
;;  with "!define".

; Make sure to update the GAIM_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"L'instalador encara est� eixecutant-se."
!define GAIM_IS_RUNNING				"Una instancia de Gaim est� eixecutant-se. Ix del Gaim i torna a intentar-ho."
!define GTK_INSTALLER_NEEDED			"L'entorn d'eixecucio GTK+ no es troba o necessita ser actualisat.$\rPer favor instala la versio${GTK_MIN_VERSION} o superior de l'entorn GTK+"

; License Page
!define GAIM_LICENSE_BUTTON			"Seg�ent >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) es distribuit baix llicencia GNU General Public License (GPL). La llicencia es proporcionada per proposits informatius aci. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Client Mensageria Instantanea Gaim (necessari)"
!define GTK_SECTION_TITLE			"Entorn d'Eixecucio GTK+ (necessari)"
!define GTK_THEMES_SECTION_TITLE		"Temes GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Sense Tema"
!define GTK_WIMP_SECTION_TITLE			"Tema Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define GAIM_SHORTCUTS_SECTION_TITLE 		"Enlla�os directes"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE 	"Escritori"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE 	"Menu d'Inici"
!define GAIM_SECTION_DESCRIPTION		"Archius i dlls del nucleu de Gaim"
!define GTK_SECTION_DESCRIPTION			"Una ferramenta multi-plataforma GUI, usada per Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION		"Els Temes GTK+ poden canviar l'aspecte de les aplicacions GTK+."
!define GTK_NO_THEME_DESC			"No instalar un tema GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (imitador Windows) es un tema GTK que s'integra perfectament en l'entorn d'escritori de Windows."
!define GTK_BLUECURVE_THEME_DESC		"El tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"El tema Lighthouseblue."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION   	"Enlla�os directes per a iniciar Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC   		"Crear un enlla� directe a Gaim en l'Escritori"
!define GAIM_STARTMENU_SHORTCUT_DESC   		"Crear una entrada per a Gaim en Menu Inici"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Una versio antiua de l'entorn GTK+ fon trobada. �Vols actualisar-la?$\rNota: Gaim no funcionar� si no ho fas."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visita la pagina de Gaim per a Windows"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (nomes borrar)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro instalant l'entorn GTK+."
!define GTK_BAD_INSTALL_PATH			"La ruta introduida no pot ser accedida o creada."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"No tens permissos per a instalar un tema GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"El desinstalador podria no trobar les entrades del registre de Gaim.$\rProbablement un atre usuari instal� esta aplicacio."
!define un.GAIM_UNINSTALL_ERROR_2		"No tens permis per a desinstalar esta aplicacio."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Soport de Correccio Ortografica"
!define GAIM_SPELLCHECK_ERROR			"Erro Instalant Correccio Ortografica"
!define GAIM_SPELLCHECK_DICT_ERROR		"Erro Instalant Diccionari de Correccio Ortografica"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Soport per a Correccio Ortografica.  (es requerix conexio a Internet per a fer l'instalacio)"
!define ASPELL_INSTALL_FAILED			"L'Instalacio fall�"
!define GAIM_SPELLCHECK_BRETON			"Breto"
!define GAIM_SPELLCHECK_CATALAN			"Catal�"
!define GAIM_SPELLCHECK_CZECH			"Chec"
!define GAIM_SPELLCHECK_WELSH			"Gal�s"
!define GAIM_SPELLCHECK_DANISH			"Danes"
!define GAIM_SPELLCHECK_GERMAN			"Alem�"
!define GAIM_SPELLCHECK_GREEK			"Grec"
!define GAIM_SPELLCHECK_ENGLISH			"Angles"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!define GAIM_SPELLCHECK_SPANISH			"Espanyol"
!define GAIM_SPELLCHECK_FAROESE			"Feroes"
!define GAIM_SPELLCHECK_FRENCH			"Frances"
!define GAIM_SPELLCHECK_ITALIAN			"Itali�"
!define GAIM_SPELLCHECK_DUTCH			"Holandes"
!define GAIM_SPELLCHECK_NORWEGIAN		"Noruec"
!define GAIM_SPELLCHECK_POLISH			"Polac"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugues"
!define GAIM_SPELLCHECK_ROMANIAN		"Romanes"
!define GAIM_SPELLCHECK_RUSSIAN			"Rus"
!define GAIM_SPELLCHECK_SLOVAK			"Eslovac"
!define GAIM_SPELLCHECK_SWEDISH			"Suec"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ucrani�"

