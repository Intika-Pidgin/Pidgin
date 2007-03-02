;;
;;  danish.nsh
;;
;;  Danish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Ewan Andreasen <wiredloose@myrealbox.com>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime environment enten mangler eller skal opgraderes.$\rInstall�r venligst GTK+ runtime version v${GTK_MIN_VERSION} eller h�jere."

; License Page
!define PIDGIN_LICENSE_BUTTON			"N�ste >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) er frigivet under GPL licensen. Licensen er kun medtaget her til generel orientering. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ Temaer"
!define GTK_NOTHEME_SECTION_TITLE		"Intet Tema"
!define GTK_WIMP_SECTION_TITLE		"Wimp Tema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve Tema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue Tema"
!define PIDGIN_SECTION_DESCRIPTION		"Basale Pidgin filer og biblioteker"
!define GTK_SECTION_DESCRIPTION		"Et multi-platform grafisk interface udviklingsv�rkt�j, bruges af Pidgin"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ Temaer kan �ndre GTK+ programmers generelle udseende."
!define GTK_NO_THEME_DESC			"Install�r ikke noget GTK+ tema"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows efterligner) er et GTK tema som falder i med Windows skrivebordsmilj�et."
!define GTK_BLUECURVE_THEME_DESC		"The Bluecurve tema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"The Lighthouseblue tema."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Der blev fundet en �ldre version af GTK+ runtime. �nsker du at opgradere?$\rNB: $(^Name) virker muligvis ikke uden denne opgradering."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Bes�g Windows Pidgin's hjemmeside"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (fjern)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fejl under installeringen af GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Stien du har angivet kan ikke findes eller oprettes."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Du har ikke tilladelse til at installere et GTK+ tema."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Afinstallationen kunne ikke finde Pidgin i registreringsdatabasen.$\rMuligvis har en anden bruger installeret programmet."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Du har ikke tilladelse til at afinstallere dette program."

