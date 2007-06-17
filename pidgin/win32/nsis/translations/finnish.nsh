;;
;;  finish.nsh
;;
;;  Finish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: "Toni_"Daigle"_Impi�" <toni.impio@pp1.inet.fi>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime ymp�rist� joko puuttuu tai tarvitsee p�ivityst�.$\rOle hyv� ja asenna v${GTK_MIN_VERSION} tai uudempi GTK+ runtime"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Seuraava >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) on julkaistu GPL lisenssin alla. Lisenssi esitet��n t�ss� vain tiedotuksena. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Suoraviestint�ohjelma (required)"
!define GTK_SECTION_TITLE			"GTK+ runtime ymp�rist� (required)"
!define PIDGIN_SECTION_DESCRIPTION		"Pidfinin ytimen tiedostot ja dll:t"
!define GTK_SECTION_DESCRIPTION		"Monipohjainen GUI (k�ytt�j�ulkoasu) ty�kalupakki, Pidginin k�ytt�m�"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Vanha versio GTK+ runtimest� l�ytynyt. Tahdotko p�ivitt��?$\rHuomio: $(^Name) ei v�ltt�m�tt� toimi mik�li j�t�t p�ivitt�m�tt�."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Vieraile Pidinin Windows -sivustolla"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Virhe asennettaessa GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Antamasi polku ei toimi tai sit� ei voi luoda."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Asennuksen poistaja ei l�yt�nyt reksiterist� tietoja Pidginista.$\rOn todenn�k�ist� ett� joku muu k�ytt�j� on asentanut ohjelman."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Sinulla ei ole valtuuksia poistaa ohjelmaa."
