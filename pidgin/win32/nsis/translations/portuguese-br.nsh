;;
;;  portuguese-br.nsh
;;
;;  Portuguese (BR) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Maur�cio de Lemos Rodrigues Collares Neto <mauricioc@myrealbox.com>, 2003-2005.
;;  Version 3
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"O ambiente de tempo de execu��o do GTK+ est� ausente ou precisa ser atualizado.$\rFavor instalar a vers�o v${GTK_MIN_VERSION} ou superior do ambiente de tempo de execu��o do GTK+."

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
!define GTK_UPGRADE_PROMPT			"Uma vers�o antiga do ambiente de tempo de execu��o do GTK+ foi encontrada. Voc� deseja atualiz�-lo?$\rNota: O $(^Name) poder� n�o funcionar a menos que voc� o fa�a."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o ambiente de tempo de execu��o do GTK+."
!define GTK_BAD_INSTALL_PATH			"O caminho que voc� digitou n�o p�de ser acessado ou criado."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a p�gina da web do Pidgin para Windows"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador n�o p�de encontrar entradas de registro do Pidgin.$\r� prov�vel que outro usu�rio tenha instalado esta aplica��o."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Voc� n�o tem permiss�o para desinstalar essa aplica��o."

!define INSTALLER_IS_RUNNING                   "O instalador j� est� em execu��o."
!define PIDGIN_IS_RUNNING                      "Uma inst�ncia do Pidgin est� em execu��o. Feche o Pidgin e tente novamente."
!define GTK_WINDOWS_INCOMPATIBLE               "O Windows 95/98/Me � incompat�vel com o GTK+ 2.8.0 ou superior. O GTK+ ${GTK_INSTALL_VERSION} n�o ser� instalado.$\rSe voc� n�o possuir o GTK+ vers�o ${GTK_MIN_VERSION} ou superior j� instalado, o instalador ir� fechar agora."
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL       "N�o foi poss�vel desinstalar a vers�o do Pidgin que est� instalada atualmente. A nova vers�o ser� instalada sem que a vers�o antiga seja removida."
!define URI_HANDLERS_SECTION_TITLE             "Handlers para endere�os"
!define PIDGIN_SPELLCHECK_SECTION_TITLE        "Suporte a verifica��o ortogr�fica"
!define PIDGIN_SPELLCHECK_ERROR                "Erro ao instalar a verifica��o ortogr�fica"
!define PIDGIN_SPELLCHECK_DICT_ERROR           "Erro ao instalar o dicion�rio da verifica��o ortogr�fica"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION  "Suporte a verifica��o ortogr�fica (A instala��o necessita de conex�o a internet)"
!define ASPELL_INSTALL_FAILED                  "Falha na instala��o"
!define PIDGIN_SPELLCHECK_BRETON               "Bret�o"
!define PIDGIN_SPELLCHECK_CATALAN              "Catal�o"
!define PIDGIN_SPELLCHECK_CZECH                "Tcheco"
!define PIDGIN_SPELLCHECK_WELSH                "Gal�s" 
!define PIDGIN_SPELLCHECK_DANISH               "Dinamarqu�s"
!define PIDGIN_SPELLCHECK_GERMAN               "Alem�o" 
!define PIDGIN_SPELLCHECK_GREEK                "Grego"
!define PIDGIN_SPELLCHECK_ENGLISH              "Ingl�s"
!define PIDGIN_SPELLCHECK_ESPERANTO            "Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH              "Espanhol"
!define PIDGIN_SPELLCHECK_FAROESE              "Fero�s"
!define PIDGIN_SPELLCHECK_FRENCH               "Franc�s"
!define PIDGIN_SPELLCHECK_ITALIAN              "Italiano"
!define PIDGIN_SPELLCHECK_DUTCH                "Holand�s"
!define PIDGIN_SPELLCHECK_NORWEGIAN            "Noruegu�s" 
!define PIDGIN_SPELLCHECK_POLISH               "Polon�s"
!define PIDGIN_SPELLCHECK_PORTUGUESE           "Portugu�s"
!define PIDGIN_SPELLCHECK_ROMANIAN             "Romeno"
!define PIDGIN_SPELLCHECK_RUSSIAN              "Russo" 
!define PIDGIN_SPELLCHECK_SLOVAK               "Eslovaco"
!define PIDGIN_SPELLCHECK_SWEDISH              "Sueco"
!define PIDGIN_SPELLCHECK_UKRAINIAN            "Ucraniano"
