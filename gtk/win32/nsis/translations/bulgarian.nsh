;;
;;  bulgarian.nsh
;;
;;  Bulgarian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Hristo Todorov <igel@bofh.bg>
;;


; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime ������ ��� ������ �� ���� ��������.$\r���� ������������ ������ v${GTK_MIN_VERSION} ��� ��-����"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim ������ �� ����� ��������� (������� ��)"
!define GTK_SECTION_TITLE			"GTK+ Runtime ����� (required)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ ����"
!define GTK_NOTHEME_SECTION_TITLE		"��� ����"
!define GTK_WIMP_SECTION_TITLE		"Wimp ����"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve ����"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue ����"
!define GAIM_SECTION_DESCRIPTION		"������� �� ������ �� Gaim � ����������"
!define GTK_SECTION_DESCRIPTION		"�������������� ��� �� �������� ������, ��������� �� Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ ������ ����� �� �������� ������� �� GTK+ ����������."
!define GTK_NO_THEME_DESC			"�� ������������ GTK+ ����"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) � GTK ���� ����� �� ������ ����� ��� Windows."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve ������."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Lighthouseblue ������."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"����� ������ GTK+ runtime � �������. ������ �� �� ��������?$\rNote: Gaim ���� �� �� ������� ��� �� �� ���������."

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (���� ����������)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"������ ��� ����������� �� GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"���������� ��� �� ���� �� ���� �������� ��� ��������."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"������ ����� �� �� ����������� GTK+ ����."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "����������� �� ���� �� ������ ������ � ��������� �� Gaim.$\r�������� � ��� ���������� �� ���� ����������."
!define un.GAIM_UNINSTALL_ERROR_2         "������ ����� �� ������������� ���� ��������."
