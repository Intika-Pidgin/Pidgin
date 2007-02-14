;;  vim:syn=winbatch:encoding=cp1252:
;;
;;  french.nsh
;;
;;  French language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Author: Eric Boumaour <zongo_fr@users.sourceforge.net>, 2003-2005.
;;

; Make sure to update the GAIM_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Le programme d'installation est d�j� en cours d'ex�cution."
!define GAIM_IS_RUNNING				"Une instance de Gaim est en cours d'ex�cution. Veuillez quitter Gaim et r�essayer."
!define GTK_INSTALLER_NEEDED			"Les biblioth�ques de l'environnement GTK+ ne sont pas install�es ou ont besoin d'une mise � jour.$\rVeuillez installer la version ${GTK_MIN_VERSION} ou plus r�cente des biblioth�ques GTK+."

; License Page
!define GAIM_LICENSE_BUTTON			"Suivant >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) est disponible sous licence GNU General Public License (GPL). Le texte de licence suivant est fourni uniquement � titre informatif. $_CLICK" 

; Components Page
!define GAIM_SECTION_TITLE			"Gaim client de messagerie instantan�e (obligatoire)"
!define GTK_SECTION_TITLE			"Biblioth�ques GTK+ (obligatoire)"
!define GTK_THEMES_SECTION_TITLE		"Th�mes GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Pas de th�me"
!define GTK_WIMP_SECTION_TITLE			"Th�me Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Th�me Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Th�me Light House Blue"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Raccourcis"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Bureau"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu D�marrer"
!define GAIM_SECTION_DESCRIPTION		"Fichiers et DLLs de base de Gaim"
!define GTK_SECTION_DESCRIPTION			"Un ensemble d'outils pour interfaces graphiques multi-plateforme, utilis� par Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION		"Les th�mes GTK+ permettent de changer l'aspect des applications GTK+."
!define GTK_NO_THEME_DESC			"Ne pas installer de th�me GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (imitateur de Windows) est un th�me de GTK+ qui se fond dans l'environnement graphique de Windows."
!define GTK_BLUECURVE_THEME_DESC		"Th�me Bluecurve"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Th�me Lighthouseblue"
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Raccourcis pour lancer Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Cr�er un raccourci pour Gaim sur le bureau"
!define GAIM_STARTMENU_SHORTCUT_DESC		"Cr�er un raccourci pour Gaim dans le menu D�marrer"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Une ancienne version des biblioth�ques GTK+ a �t� trouv�e. Voulez-vous la mettre � jour ?$\rNote : Gaim peut ne pas fonctionner si vous ne le faites pas."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visitez la page web de Gaim Windows" 

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (supprimer uniquement)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erreur lors de l'installation des biblioth�ques GTK+"
!define GTK_BAD_INSTALL_PATH			"Le dossier d'installation ne peut pas �tre cr�� ou n'est pas accessible."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Vous n'avez pas les permissions pour installer un th�me GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Le programme de d�sinstallation n'a pas retrouv� les entr�es de Gaim dans la base de registres.$\rL'application a peut-�tre �t� install�e par un utilisateur diff�rent."
!define un.GAIM_UNINSTALL_ERROR_2		"Vous n'avez pas les permissions pour supprimer cette application."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Correction orthographique"
!define GAIM_SPELLCHECK_ERROR			"Erreur � l'installation du correcteur orthographique"
!define GAIM_SPELLCHECK_DICT_ERROR		"Erreur � l'installation du dictionnaire pour le correcteur orthographique"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Correction orthogaphique. (Une connexion internet est n�cessaire pour son installation)"
!define ASPELL_INSTALL_FAILED			"�chec de l'installation"
!define GAIM_SPELLCHECK_BRETON			"Breton"
!define GAIM_SPELLCHECK_CATALAN			"Catalan"
!define GAIM_SPELLCHECK_CZECH			"Tch�que"
!define GAIM_SPELLCHECK_WELSH			"Gallois"
!define GAIM_SPELLCHECK_DANISH			"Danois"
!define GAIM_SPELLCHECK_GERMAN			"Allemand"
!define GAIM_SPELLCHECK_GREEK			"Grec"
!define GAIM_SPELLCHECK_ENGLISH			"Anglais"
!define GAIM_SPELLCHECK_ESPERANTO		"Esp�ranto"
!define GAIM_SPELLCHECK_SPANISH			"Espagnol"
!define GAIM_SPELLCHECK_FAROESE			"F�ringien"
!define GAIM_SPELLCHECK_FRENCH			"Fran�ais"
!define GAIM_SPELLCHECK_ITALIAN			"Italien"
!define GAIM_SPELLCHECK_DUTCH			"Hollandais"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norv�gien"
!define GAIM_SPELLCHECK_POLISH			"Polonais"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugais"
!define GAIM_SPELLCHECK_ROMANIAN		"Roumain"
!define GAIM_SPELLCHECK_RUSSIAN			"Russe"
!define GAIM_SPELLCHECK_SLOVAK			"Slovaque"
!define GAIM_SPELLCHECK_SWEDISH			"Su�dois"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukrainien"
