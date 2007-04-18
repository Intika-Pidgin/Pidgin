;;
;;  bulgarian.nsh
;;
;;  Bulgarian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Hristo Todorov <igel@bofh.bg>
;;


; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime ������ ��� ������ �� ���� ��������.$\r���� ������������ ������ v${GTK_MIN_VERSION} ��� ��-����"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin ������ �� ����� ��������� (������� ��)"
!define GTK_SECTION_TITLE			"GTK+ Runtime ����� (required)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ ����"
!define GTK_NOTHEME_SECTION_TITLE		"��� ����"
!define GTK_WIMP_SECTION_TITLE			"Wimp ����"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve ����"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue ����"
!define PIDGIN_SECTION_DESCRIPTION		"������� �� ������ �� Pidgin � ����������"
!define GTK_SECTION_DESCRIPTION			"�������������� ��� �� �������� ������, ��������� �� Pidgin"


; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"����� ������ GTK+ runtime � �������. ������ �� �� ��������?$\rNote: Pidgin ���� �� �� ������� ��� �� �� ���������."

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (���� ����������)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"������ ��� ����������� �� GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"���������� ��� �� ���� �� ���� �������� ��� ��������."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"������ ����� �� �� ����������� GTK+ ����."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"����������� �� ���� �� ������ ������ � ��������� �� Pidgin.$\r�������� � ��� ���������� �� ���� ����������."
!define un.PIDGIN_UNINSTALL_ERROR_2		"������ ����� �� ������������� ���� ��������."
