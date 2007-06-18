;;
;;  serbian-latin.nsh
;;
;;  Serbian (Latin) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Danilo Segan <dsegan@gmx.net>
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ okolina za izvr�avanje ili nije na�ena ili se moraunaprediti.$\rMolimo instalirajte v${GTK_MIN_VERSION} ili ve�u GTK+ okoline za izvr�avanje"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin klijent za brze poruke (neophodno)"
!define GTK_SECTION_TITLE			"GTK+ okolina za izvr�avanje (neophodno)"
!define PIDGIN_SECTION_DESCRIPTION		"Osnovne Pidgin datoteke i dinami�ke biblioteke"
!define GTK_SECTION_DESCRIPTION		"Skup oru�a za grafi�ko okru�enje, za vi�e platformi, koristi ga Pidgin "

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Na�ena je stara verzija GTK+ izvr�ne okoline. Da li �elite da je unapredite?$\rPrimedba: Ukoliko to ne uradite, $(^Name) mo�da ne�e raditi."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Gre�ka prilikom instalacije GTK+ okoline za izvr�avanje."
!define GTK_BAD_INSTALL_PATH			"Putanja koju ste naveli se ne mo�e ni napraviti niti joj se mo�e pri�i."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "Program za uklanjanje instalacije ne mo�e da prona�e stavke registra za Pidgin.$\rVerovatno je ovu aplikaciju instalirao drugi korisnik."
!define un.PIDGIN_UNINSTALL_ERROR_2         "Nemate ovla��enja za deinstalaciju ove aplikacije."
