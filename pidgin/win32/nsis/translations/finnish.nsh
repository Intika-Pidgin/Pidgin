;;
;;  finish.nsh
;;
;;  Finish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Authors: Toni "Daigle" Impi� <toni.impio@pp1.inet.fi>
;;           Timo Jyrinki <timo.jyrinki@iki.fi>, 2008
;;           
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Ajonaikainen GTK+-ymp�rist� joko puuttuu tai tarvitsee p�ivityst�.$\rOle hyv� ja asenna v${GTK_MIN_VERSION} tai uudempi ajonaikainen GTK+-ymp�rist�."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Seuraava >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) on julkaistu GPL-lisenssin alla. Lisenssi esitet��n t�ss� vain tiedotuksena. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin-pikaviestin (vaaditaan)"
!define GTK_SECTION_TITLE			"Ajonaikainen GTK+-ymp�rist� (vaaditaan)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE 		"Pikakuvakkeet"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE 	"Ty�p�yt�"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "K�ynnistysvalikko"
!define PIDGIN_SECTION_DESCRIPTION		"Pidginin ytimen tiedostot ja kirjastot"
!define GTK_SECTION_DESCRIPTION		"Monialustainen Pidginin k�ytt�m� k�ytt�liittym�kirjasto"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   	"Pikakuvakkeet Pidginin k�ynnist�miseksi"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   		"Tee Pidgin-pikakuvake ty�p�yd�lle"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   	"Tee Pidgin-pikakuvake k�ynnistysvalikkoon"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Vanha versio ajonaikaisesta GTK+-ymp�rist�st� l�ytynyt. Tahdotko p�ivitt��?$\rHuomio: $(^Name) ei v�ltt�m�tt� toimi mik�li j�t�t p�ivitt�m�tt�."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Vieraile Pidginin WWW-sivustolla"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Virhe asennettaessa ajonaikaista GTK+-ymp�rist��."
!define GTK_BAD_INSTALL_PATH			"Antamasi polku ei toimi tai sit� ei voi luoda."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI-k�sittelij�t"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Asennuksen poistaja ei l�yt�nyt rekisterist� tietoja Pidginista.$\rOn todenn�k�ist� ett� joku muu k�ytt�j� on asentanut ohjelman."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Sinulla ei ole valtuuksia poistaa ohjelmaa."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Oikolukutuki"
!define PIDGIN_SPELLCHECK_ERROR			"Virhe asennettaessa oikolukua"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Virhe asennettaessa oikoluvun sanakirjaa"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Tuki oikoluvulle.  (Asennukseen tarvitaan Internet-yhteys)"
!define ASPELL_INSTALL_FAILED			"Asennus ep�onnistui"

