;;
;;  czech.nsh
;;
;;  Czech language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Jan Kolar <jan@e-kolar.net>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime bu�to chyb�, nebo je pot�eba prov�st upgrade.$\rProve�te instalaci verze${GTK_MIN_VERSION} nebo vy���."

; License Page
!define GAIM_LICENSE_BUTTON			"Dal�� >"
!define GAIM_LICENSE_BOTTOM_TEXT		"K pou�it� $(^Name) se vztahuje GPL licence. Licence je zde uvedena pouze pro Va�� informaci. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (nutn�)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (nutn�)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ t�mata"
!define GTK_NOTHEME_SECTION_TITLE		"Bez t�mat"
!define GTK_WIMP_SECTION_TITLE		"Wimp t�ma"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve t�ma"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue t�ma"
!define GAIM_SECTION_DESCRIPTION		"Z�kladn� soubory a DLL pro Gaim"
!define GTK_SECTION_DESCRIPTION		"Multi-platform GUI toolkit pou��van� Gaimem"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ t�mata umo��uj� m�nit vzhled a zp�sob ovl�d�n� GTK+ aplikac�."
!define GTK_NO_THEME_DESC			"Neinstalovat GTK+ t�ma"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) je GTK t�ma kter� zapadne do Va�eho pracovn�ho prost�ed� ve Windows."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve t�ma."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Lighthouseblue t�ma."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Byla nalezena star�� verze GTK+ runtime. Chcete prov�st upgrade?$\rUpozorn�n�: Bez upgradu Gaim nemus� pracovat spr�vn�."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Nav�t�vit Windows Gaim Web Page"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (odinstalovat)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Chyba p�i instalaci GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Zadan� cesta je nedostupn�, nebo ji nelze vytvo�it."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nem�te opr�vn�n� k instalaci GTK+ t�matu."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Odinstal�n� proces nem��e naj�t z�znamy pro Gaim v registrech.$\rPravd�podobn� instalaci t�to aplikace provedl jin� u�ivatel."
!define un.GAIM_UNINSTALL_ERROR_2		"Nem�te opr�vn�n� k odinstalaci t�to aplikace."
