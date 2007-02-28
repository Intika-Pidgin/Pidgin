;;
;;  albanian.nsh
;;
;;  Albanian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 2
;;  Author: Besnik Bleta <besnik@spymac.com>
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Ose mungon mjedisi GTK+ runtime ose lyp përditësim.$\rJu lutem instaloni GTK+ runtime v${GTK_MIN_VERSION} ose më të vonshëm"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Më tej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) qarkullon nën licensën GPL. Licensa këtu sillet vetëm për qëllime njoftimi. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Klient Shkëmbimi Mesazhesh të Atypëratyshëm Gaim (i nevojshëm)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (i nevojshëm)"
!define GTK_THEMES_SECTION_TITLE		"Tema GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Pa Tema"
!define GTK_WIMP_SECTION_TITLE		"Temë Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Temë Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Temë Light House Blue"
!define PIDGIN_SECTION_DESCRIPTION		"Kartela dhe dll bazë të Gaim-it"
!define GTK_SECTION_DESCRIPTION		"Një grup mjetesh shumëplatformësh për GUI, përdorur nga Gaim-i"
!define GTK_THEMES_SECTION_DESCRIPTION	"Temat GTK+ mund të ndryshojnë pamjen dhe sjelljen e zbatimeve GTK+."
!define GTK_NO_THEME_DESC			"Mos instalo temë GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) është një temë GTK që ndërthuret mirë mjedisin Windows."
!define GTK_BLUECURVE_THEME_DESC		"Tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Tema Lighthouseblue."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"U gjet një version i vjetër për GTK+ runtime. Doni të përditësohet?$\rShënim: Gaim-i mund të mos punojë nëse nuk e bëni."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Vizitoni Faqen Web të Gaim-it për Windows"

; Gaim Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"Gaim (vetëm hiq)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"gabim gjatë instalimit të GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Shtegu që treguat nuk mund të arrihet ose krijohet."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nuk keni leje të instaloni tema GTK+."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Çinstaluesi nuk gjeti dot zëra regjistri për Gaim-in.$\rKa mundësi që këtë zbatim ta ketë instaluar një tjetër përdorues."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nuk keni leje të çinstaloni këtë zbatim."
