;;
;;  portuguese-br.nsh
;;
;;  Portuguese (BR) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Maur�cio de Lemos Rodrigues Collares Neto <mauricioc@myrealbox.com>, 2003-2005.
;;  Version 3
;;


; License Page
!define PIDGIN_LICENSE_BUTTON			"Avan�ar >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) � distribu�do sob a licen�a GPL. Esta licen�a � disponibilizada aqui apenas para fins informativos. $_CLICK" 

; Components Page
!define PIDGIN_SECTION_TITLE			"Cliente de mensagens instant�neas Pidgin (requerido)"
!define GTK_SECTION_TITLE			"Ambiente de tempo de execu��o do GTK+ (requerido)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Atalhos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "�rea de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Menu Iniciar"
!define PIDGIN_SECTION_DESCRIPTION		"Arquivos e bibliotecas principais do Pidgin"
!define GTK_SECTION_DESCRIPTION		"Um conjunto de ferramentas multi-plataforma para interface do usu�rio, usado pelo Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Atalhos para iniciar o Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Crie um atalho para o Pidgin na �rea de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Crie uma entrada no Menu Iniciar para o Pidgin"

; GTK+ Directory Page

; GTK+ Section Prompts

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a p�gina da web do Pidgin para Windows"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador n�o p�de encontrar entradas de registro do Pidgin.$\r� prov�vel que outro usu�rio tenha instalado esta aplica��o."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Voc� n�o tem permiss�o para desinstalar essa aplica��o."

!define INSTALLER_IS_RUNNING                   "O instalador j� est� em execu��o."
!define PIDGIN_IS_RUNNING                      "Uma inst�ncia do Pidgin est� em execu��o. Feche o Pidgin e tente novamente."
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL       "N�o foi poss�vel desinstalar a vers�o do Pidgin que est� instalada atualmente. A nova vers�o ser� instalada sem que a vers�o antiga seja removida."
!define URI_HANDLERS_SECTION_TITLE             "Handlers para endere�os"
!define PIDGIN_SPELLCHECK_SECTION_TITLE        "Suporte a verifica��o ortogr�fica"
!define PIDGIN_SPELLCHECK_ERROR                "Erro ao instalar a verifica��o ortogr�fica"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION  "Suporte a verifica��o ortogr�fica (A instala��o necessita de conex�o a internet)"
