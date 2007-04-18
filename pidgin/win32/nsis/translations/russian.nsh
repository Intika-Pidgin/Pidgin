;;
;;  russian.nsh
;;
;;  Russian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Tasselhof <anr@nm.ru>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"��������� ��� ������� GTK+ ���������� ��� ��������� � ����������.$\r���������� ���������� v${GTK_MIN_VERSION} ��� ����� ������� ������ GTK+."

; License Page
!define PIDGIN_LICENSE_BUTTON			"��������� >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) �������� ��� ��������� GPL. �������� ��������� ����� ��� ��������������� �����. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin - ������ ��� ����������� ������ ����������� �� ��������� ���������� (����������)."
!define GTK_SECTION_TITLE			"GTK+ ��������� ��� ������� (����������)."
!define GTK_THEMES_SECTION_TITLE		"���� ��� GTK+."
!define GTK_NOTHEME_SECTION_TITLE		"��� ����."
!define GTK_WIMP_SECTION_TITLE		"���� 'Wimp Theme'"
!define GTK_BLUECURVE_SECTION_TITLE		"���� 'Bluecurve'."
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"���� 'Light House Blue'."
!define PIDGIN_SECTION_DESCRIPTION		"�������� ����� Pidgin � ����������."
!define GTK_SECTION_DESCRIPTION		"������������������� ����������� ��������������, ������������ Pidgin."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"������� ������ ������ GTK+. �� �� ������ �� �������� � ?$\r����������: Pidgin ����� �� �������� ���� �� �� �������� �����."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"�������� ���-�������� Pidgin ��� ������������� Windows."

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"Pidgin (������ ��������)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"������ ��� ��������� ��������� GTK+."
!define GTK_BAD_INSTALL_PATH			"���� ������� �� ����� ���������� ��� �� ����������."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"� ��� ��� ���� �� ������������ ���� ��� GTK+."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"��������� �������� �� ����� ����� ������ Pidgin � ��������..$\r�������� ��� ���������� ��������� ������ ������������."
!define un.PIDGIN_UNINSTALL_ERROR_2		"� ��� ��� ���� �� �������� ����� ����������."
