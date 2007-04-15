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
!define GTK_THEMES_SECTION_TITLE		"Temas do GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Nenhum tema"
!define GTK_WIMP_SECTION_TITLE		"Tema 'Wimp'"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema 'Bluecurve'"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema 'Light House Blue'"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Atalhos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "�rea de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Menu Iniciar"
!define PIDGIN_SECTION_DESCRIPTION		"Arquivos e bibliotecas principais do Pidgin"
!define GTK_SECTION_DESCRIPTION		"Um conjunto de ferramentas multi-plataforma para interface do usu�rio, usado pelo Pidgin"
!define GTK_THEMES_SECTION_DESCRIPTION	"Os temas do GTK+ podem mudar a apar�ncia e o funcionamento dos aplicativos GTK+."
!define GTK_NO_THEME_DESC			"N�o instalar um tema do GTK+"
!define GTK_WIMP_THEME_DESC			"O tema 'GTK-Wimp' ('Windows impersonator', personificador do Windows) � um tema GTK que combina bem com o ambiente de �rea de trabalho do Windows."
!define GTK_BLUECURVE_THEME_DESC		"O tema 'Bluecurve'."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"O tema 'Lighthouseblue'."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Atalhos para iniciar o Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Crie um atalho para o Pidgin na �rea de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Crie uma entrada no Menu Iniciar para o Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Uma vers�o antiga do ambiente de tempo de execu��o do GTK+ foi encontrada. Voc� deseja atualiz�-lo?$\rNota: O $(^Name) poder� n�o funcionar a menos que voc� o fa�a."

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (apenas remover)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o ambiente de tempo de execu��o do GTK+."
!define GTK_BAD_INSTALL_PATH			"O caminho que voc� digitou n�o p�de ser acessado ou criado."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Voc� n�o tem permiss�o para instalar um tema do GTK+."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a p�gina da web do Pidgin para Windows"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador n�o p�de encontrar entradas de registro do Pidgin.$\r� prov�vel que outro usu�rio tenha instalado esta aplica��o."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Voc� n�o tem permiss�o para desinstalar essa aplica��o."
