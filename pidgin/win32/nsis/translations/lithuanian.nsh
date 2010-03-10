;;
;;  lithuanian.nsh
;;
;;  Lithuanian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1257
;;
;;  Version 3

; Startup Checks
!define INSTALLER_IS_RUNNING			"Diegimo programa jau paleista."
!define PIDGIN_IS_RUNNING			"�iuo metu Pidgin yra paleistas. U�darykite �i� program� ir pabandykite i� naujo."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Toliau >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) yra i�leistas GNU bendrosios vie�osios licenzijos (GPL) s�lygomis.  Licenzija �ia yra pateikta tik susipa�inimo tikslams. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin pokalbi� kliento programa (b�tina)"
!define GTK_SECTION_TITLE			"GTK+ vykdymo meto aplinka (b�tina, jeigu n�ra)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Nuorodos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"darbalaukyje"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"pradiniame meniu"
!define PIDGIN_SECTION_DESCRIPTION		"Pagrindiniai Pidgin failai ir DLL bibliotekos"
!define GTK_SECTION_DESCRIPTION		"Daugiaplatforminis vartotojo s�sajos priemoni� komplektas, naudojamas Pidgin."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Pidgin paleidimo nuorodos"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Sukurti nuorod� � Pidgin darbastalyje."
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Sukurti pradinio meniu �ra��, skirt� Pidgin."

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Aplankyti Pidgin tinklalap�"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Nepavyko i�diegti anks�iau �diegtos Pidgin versijos.  Nauja versija bus �diegta nei�diegus senosios."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI dorokl�s"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"I�diegimo programa nerado Pidgin registro �ra��.$\rTik�tina, kad progama buvo �diegta kito naudotojo."
!define un.PIDGIN_UNINSTALL_ERROR_2		"J�s neturite teisi� i�diegti �ios programos."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Ra�ybos tikrinimo palaikymas"
!define PIDGIN_SPELLCHECK_ERROR		"Ra�ybos tikrinimo palaikymo diegimo klaida"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Ra�ybos tikrinimo palaikymas.  (Diegimui b�tina interneto jungtis)"

