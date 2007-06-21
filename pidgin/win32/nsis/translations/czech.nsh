;;
;;  czech.nsh
;;
;;  Czech language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Jan Kolar <jan@e-kolar.net>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime bu�to chyb�, nebo je pot�eba prov�st upgrade.$\rProve�te instalaci verze${GTK_MIN_VERSION} nebo vy���."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Dal�� >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"K pou�it� $(^Name) se vztahuje GPL licence. Licence je zde uvedena pouze pro Va�� informaci. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (nutn�)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (nutn�)"
!define PIDGIN_SECTION_DESCRIPTION		"Z�kladn� soubory a DLL pro Pidgin"
!define GTK_SECTION_DESCRIPTION		"Multi-platform GUI toolkit pou��van� Pidginem"


; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Byla nalezena star�� verze GTK+ runtime. Chcete prov�st upgrade?$\rUpozorn�n�: Bez upgradu $(^Name) nemus� pracovat spr�vn�."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Nav�t�vit Windows Pidgin Web Page"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Chyba p�i instalaci GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Zadan� cesta je nedostupn�, nebo ji nelze vytvo�it."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Odinstal�n� proces nem��e naj�t z�znamy pro Pidgin v registrech.$\rPravd�podobn� instalaci t�to aplikace provedl jin� u�ivatel."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nem�te opr�vn�n� k odinstalaci t�to aplikace."
