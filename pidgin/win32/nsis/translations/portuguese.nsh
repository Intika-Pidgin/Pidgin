;;
;;  portuguese.nsh
;;
;;  Portuguese (PT) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Duarte Henriques <duarte.henriques@gmail.com>, 2003-2005.
;;  Version 3
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"O instalador j� est� a ser executado."
!define PIDGIN_IS_RUNNING			"Uma inst�ncia do Pidgin j� est� a ser executada. Saia do Pidgin e tente de novo."
!define GTK_INSTALLER_NEEDED			"O ambiente de GTK+ est� ausente ou precisa de ser actualizado.$\rPor favor instale a vers�o v${GTK_MIN_VERSION} ou mais recente do ambiente de GTK+."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Seguinte >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) est� dispon�vel sob a licen�a GNU General Public License (GPL). O texto da licen�a � fornecido aqui meramente a t�tulo informativo. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Cliente de Mensagens Instant�neas Pidgin (obrigat�rio)"
!define GTK_SECTION_TITLE			"Ambiente de Execu��o GTK+ (obrigat�rio)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Atalhos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Ambiente de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Menu de Iniciar"
!define PIDGIN_SECTION_DESCRIPTION		"Ficheiros e bibliotecas principais do Pidgin"
!define GTK_SECTION_DESCRIPTION		"Um conjunto de ferramentas de interface gr�fica multi-plataforma, usado pelo Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Atalhos para iniciar o Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Criar um atalho para o Pidgin no Ambiente de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Criar uma entrada para o Pidgin na Barra de Iniciar"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Foi encontrada uma vers�o antiga do ambiente de execu��o GTK+. Deseja actualiz�-lo?$\rNota: O $(^Name) poder� n�o funcionar se n�o o fizer."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a P�gina Web do Pidgin para Windows"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o ambiente de execu��o GTK+."
!define GTK_BAD_INSTALL_PATH			"O caminho que digitou n�o pode ser acedido nem criado."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador n�o encontrou entradas de registo do Pidgin.$\r� prov�vel que outro utilizador tenha instalado este programa."
!define un.PIDGIN_UNINSTALL_ERROR_2		"N�o tem permiss�o para desinstalar este programa."
