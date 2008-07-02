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
!define GTK_INSTALLER_NEEDED			"GTK+ vykdymo meto aplinkos n�ra arba ji turi b�ti atnaujinta.$\r�diekite v${GTK_MIN_VERSION} arba naujesn� GTK+ vykdymo meto aplinkos versij�"

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
!define GTK_UPGRADE_PROMPT			"Rasta sena GTK+ vykdymo meto aplinkos versija. Ar norite j� atnaujinti?$\rPastaba: $(^Name) gali neveikti, jeigu to nepadarysite."
!define GTK_WINDOWS_INCOMPATIBLE		"Windows 95/98/Me yra nesuderinami su GTK+ 2.8.0 ir naujesn�mis versijomis.  GTK+ ${GTK_INSTALL_VERSION} nebus �diegta.$\rJeigu neturite �sidieg� GTK+ ${GTK_MIN_VERSION} ar naujesn�s versijos, diegimas dabar bus nutrauktas."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Aplankyti Pidgin tinklalap�"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Nepavyko i�diegti anks�iau �diegtos Pidgin versijos.  Nauja versija bus �diegta nei�diegus senosios."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ vykdymo meto aplinkos diegimo klaida"
!define GTK_BAD_INSTALL_PATH			"J�s� �vestas kelias negali b�ti pasiektas ar sukurtas."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI dorokl�s"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"I�diegimo programa nerado Pidgin registro �ra��.$\rTik�tina, kad progama buvo �diegta kito naudotojo."
!define un.PIDGIN_UNINSTALL_ERROR_2		"J�s neturite teisi� i�diegti �ios programos."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Ra�ybos tikrinimo palaikymas"
!define PIDGIN_SPELLCHECK_ERROR		"Ra�ybos tikrinimo palaikymo diegimo klaida"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Ra�ybos tikrinimo �odyno diegimo klaida"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Ra�ybos tikrinimo palaikymas.  (Diegimui b�tina interneto jungtis)"
!define ASPELL_INSTALL_FAILED			"Diegimas nepavyko"
!define PIDGIN_SPELLCHECK_BRETON		"Breton� kalba"
!define PIDGIN_SPELLCHECK_CATALAN		"Katalon� kalba"
!define PIDGIN_SPELLCHECK_CZECH		"�ek� kalba"
!define PIDGIN_SPELLCHECK_WELSH		"Val� kalba"
!define PIDGIN_SPELLCHECK_DANISH		"Dan� kalba"
!define PIDGIN_SPELLCHECK_GERMAN		"Vokie�i� kalba"
!define PIDGIN_SPELLCHECK_GREEK		"Graik� kalba"
!define PIDGIN_SPELLCHECK_ENGLISH		"Angl� kalba"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto kalba"
!define PIDGIN_SPELLCHECK_SPANISH		"Ispan� kalba"
!define PIDGIN_SPELLCHECK_FAROESE		"Farer� kalba"
!define PIDGIN_SPELLCHECK_FRENCH		"Pranc�z� kalba"
!define PIDGIN_SPELLCHECK_ITALIAN		"Ital� kalba"
!define PIDGIN_SPELLCHECK_DUTCH		"Oland� kalba"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norveg� kalba"
!define PIDGIN_SPELLCHECK_POLISH		"Lenk� kalba"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugal� kalba"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumun� kalba"
!define PIDGIN_SPELLCHECK_RUSSIAN		"Rus� kalba"
!define PIDGIN_SPELLCHECK_SLOVAK		"Slovak� kalba"
!define PIDGIN_SPELLCHECK_SWEDISH		"�ved� kalba"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainie�i� kalba"

