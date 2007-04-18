;;
;;  trad-chinese.nsh
;;
;;  Traditional Chineese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page:950
;;
;;  Author: Paladin R. Liu <paladin@ms1.hinet.net>
;;  Minor updates: Ambrose C. Li <acli@ada.dhs.org>
;;
;;  Last Updated: July 5, 2005
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"�w�˵{�����b���椤�C"
!define PIDGIN_IS_RUNNING				"Pidgin ���b���椤�A�Х������o�ӵ{����A��w�ˡC"
!define GTK_INSTALLER_NEEDED			"�䤣��ŦX�� GTK+ �������ҩάO�ݭn�Q��s�C$\r�Цw�� v${GTK_MIN_VERSION} �H�W������ GTK+ �������ҡC"

; License Page
!define PIDGIN_LICENSE_BUTTON			"�U�@�B >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) �ĥ� GNU General Public License (GPL) ���v�o�G�C�b���C�X���v�ѡA�ȧ@���ѦҤ��ΡC$_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin �D�{�� (����)"
!define GTK_SECTION_TITLE			"GTK+ �������� (����)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ �G���D�D"
!define GTK_NOTHEME_SECTION_TITLE		"���w�˧G���D�D"
!define GTK_WIMP_SECTION_TITLE			"Wimp �G���D�D"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve �G���D�D"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue �G���D�D"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"���|"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"�ୱ���|"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "�}�l�\���"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin �֤��ɮפΰʺA�禡�w"
!define GTK_SECTION_DESCRIPTION			"Pidgin �ҨϥΪ��󥭥x�ϧΤ����禡�w"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"�إ� Pidgin ���|"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"�b�ୱ�إ߱��|"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"�b�}�l�\���إ߱��|"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"�o�{�@���ª��� GTK+ �������ҡC�z�n�N���ɯŶܡH$\r�Ъ`�N�G�p�G�z���ɯšA $(^Name) �i��L�k���T���Q����C"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"���X Windows Pidgin ����"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (�u�Ѳ���)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"�w�� GTK+ �������Үɵo�Ϳ��~�C"
!define GTK_BAD_INSTALL_PATH			"�z�ҿ�J���w�˥ؿ��L�k�s���ΫإߡC"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"�z�ثe���v���L�k�w�� GTK+ �G���D�D�C"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"�����{���L�k��� Pidgin ���w�˸�T�C$\r�o���ӬO����L���ϥΪ̭��s�w�ˤF�o�ӵ{���C"
!define un.PIDGIN_UNINSTALL_ERROR_2		"�z�ثe���v���L�k���� Pidgin �C"
