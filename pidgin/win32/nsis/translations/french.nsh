;;  vim:syn=winbatch:fileencoding=cp1252:
;;
;;  french.nsh
;;
;;  French language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Author: Eric Boumaour <zongo_fr@users.sourceforge.net>, 2003-2007.
;;

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Le programme d'installation est d�j� en cours d'ex�cution."
!define PIDGIN_IS_RUNNING				"Une instance de Pidgin est en cours d'ex�cution. Veuillez quitter Pidgin et r�essayer."
!define GTK_INSTALLER_NEEDED			"Les biblioth�ques de l'environnement GTK+ ne sont pas install�es ou ont besoin d'une mise � jour.$\rVeuillez installer la version ${GTK_MIN_VERSION} ou plus r�cente des biblioth�ques GTK+."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Suivant >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) est disponible sous licence GNU General Public License (GPL). Le texte de licence suivant est fourni uniquement � titre informatif. $_CLICK" 

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin client de messagerie instantan�e (obligatoire)"
!define GTK_SECTION_TITLE			"Biblioth�ques GTK+ (obligatoire)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Raccourcis"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Bureau"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu D�marrer"
!define PIDGIN_SECTION_DESCRIPTION		"Fichiers et DLLs de base de Pidgin"
!define GTK_SECTION_DESCRIPTION			"Un ensemble d'outils pour interfaces graphiques multi-plateforme, utilis� par Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Raccourcis pour lancer Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Cr�er un raccourci pour Pidgin sur le bureau"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Cr�er un raccourci pour Pidgin dans le menu D�marrer"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Une ancienne version des biblioth�ques GTK+ a �t� trouv�e. Voulez-vous la mettre � jour ?$\rNote : $(^Name) peut ne pas fonctionner si vous ne le faites pas."
!define GTK_WINDOWS_INCOMPATIBLE		"Windows 95/98/Me est incompatible avec GTK+ version 2.8.0 ou plus r�centes.  GTK+ ${GTK_INSTALL_VERSION} ne sera pas install�.$\rSi vous n'avez pas install� GTK+ version ${GTK_MIN_VERSION} ou pkus r�cente, l'installation s'arr�tera."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visitez la page web de Pidgin Windows" 

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Impossible de d�sinstaller la version de Pidgin en place. La nouvelle version sera install�e sans supprimer la version en place."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erreur lors de l'installation des biblioth�ques GTK+"
!define GTK_BAD_INSTALL_PATH			"Le dossier d'installation ne peut pas �tre cr�� ou n'est pas accessible."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"Gestion des liens (URI)"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Le programme de d�sinstallation n'a pas retrouv� les entr�es de Pidgin dans la base de registres.$\rL'application a peut-�tre �t� install�e par un utilisateur diff�rent."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Vous n'avez pas les permissions pour supprimer cette application."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Correction orthographique"
!define PIDGIN_SPELLCHECK_ERROR			"Erreur � l'installation du correcteur orthographique"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Erreur � l'installation du dictionnaire pour le correcteur orthographique"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Correction orthogaphique. (Une connexion internet est n�cessaire pour son installation)"
!define ASPELL_INSTALL_FAILED			"�chec de l'installation"
!define PIDGIN_SPELLCHECK_BRETON			"Breton"
!define PIDGIN_SPELLCHECK_CATALAN			"Catalan"
!define PIDGIN_SPELLCHECK_CZECH			"Tch�que"
!define PIDGIN_SPELLCHECK_WELSH			"Gallois"
!define PIDGIN_SPELLCHECK_DANISH			"Danois"
!define PIDGIN_SPELLCHECK_GERMAN			"Allemand"
!define PIDGIN_SPELLCHECK_GREEK			"Grec"
!define PIDGIN_SPELLCHECK_ENGLISH			"Anglais"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esp�ranto"
!define PIDGIN_SPELLCHECK_SPANISH			"Espagnol"
!define PIDGIN_SPELLCHECK_FAROESE			"F�ringien"
!define PIDGIN_SPELLCHECK_FRENCH			"Fran�ais"
!define PIDGIN_SPELLCHECK_ITALIAN			"Italien"
!define PIDGIN_SPELLCHECK_DUTCH			"Hollandais"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norv�gien"
!define PIDGIN_SPELLCHECK_POLISH			"Polonais"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugais"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Roumain"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Russe"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slovaque"
!define PIDGIN_SPELLCHECK_SWEDISH			"Su�dois"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainien"
