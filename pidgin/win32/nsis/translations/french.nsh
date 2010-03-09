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

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visitez la page web de Pidgin Windows" 

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Impossible de d�sinstaller la version de Pidgin en place. La nouvelle version sera install�e sans supprimer la version en place."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"Gestion des liens (URI)"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Le programme de d�sinstallation n'a pas retrouv� les entr�es de Pidgin dans la base de registres.$\rL'application a peut-�tre �t� install�e par un utilisateur diff�rent."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Vous n'avez pas les permissions pour supprimer cette application."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Correction orthographique"
!define PIDGIN_SPELLCHECK_ERROR			"Erreur � l'installation du correcteur orthographique"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Correction orthogaphique. (Une connexion internet est n�cessaire pour son installation)"
