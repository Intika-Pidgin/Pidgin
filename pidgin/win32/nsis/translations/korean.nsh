;;
;;  korean.nsh
;;
;;  Korean language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 949
;;
;;  Author: Kyung-uk Son <vvs740@chol.com>
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ ��Ÿ�� ȯ�濡 ������ �ְų� ���׷��̵尡 �ʿ��մϴ�.$\rGTK+ ��Ÿ�� ȯ���� v${GTK_MIN_VERSION}�̳� �� �̻� �������� ��ġ���ּ���."

; Components Page
!define PIDGIN_SECTION_TITLE			"���� �޽��� (�ʼ�)"
!define GTK_SECTION_TITLE			"GTK+ ��Ÿ�� ȯ�� (�ʼ�)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ �׸�"
!define GTK_NOTHEME_SECTION_TITLE		"�׸� ����"
!define GTK_WIMP_SECTION_TITLE		"���� �׸�"
!define GTK_BLUECURVE_SECTION_TITLE		"���Ŀ�� �׸�"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue �׸�"
!define PIDGIN_SECTION_DESCRIPTION		"������ �ھ� ���ϰ� dll"
!define GTK_SECTION_DESCRIPTION		"������ ����ϴ� ��Ƽ �÷��� GUI ��Ŷ"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"���� ���� GTK+ ��Ÿ���� ã�ҽ��ϴ�. ���׷��̵��ұ��?$\rNote: ���׷��̵����� ������ ������ �������� ���� ���� �ֽ��ϴ�."

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (remove only)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ ��Ÿ�� ��ġ �� ���� �߻�."
!define GTK_BAD_INSTALL_PATH			"�Է��Ͻ� ��ο� ������ �� ���ų� ���� �� �������ϴ�."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"GTK+ �׸��� ��ġ�� ������ �����ϴ�."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "���ν��緯�� ������ ������Ʈ�� ��Ʈ���� ã�� �� �����ϴ�.$\r�� ���α׷��� �ٸ� ���� �������� ��ġ�� �� �����ϴ�."
!define un.PIDGIN_UNINSTALL_ERROR_2         "�� ���α׷��� ������ �� �ִ� ������ �����ϴ�."
