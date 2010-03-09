;;
;;  persian.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: CP1256
;;  As this file needs to be encoded in CP1256 and CP1256 doesn't support U+06CC 
;;  and U+0654 characters, I have removed all U+0654 characters and replaced U+06CC
;;  with U+064A in the middle of the words and with U+0649 at the end of the words.
;;  The Persian text will display correctly but the encoding is incorrect.
;;
;;  Author: Elnaz Sarbar <elnaz@farsiweb.info>, 2007

; Startup Checks
!define INSTALLER_IS_RUNNING			"��ȝ����� �� ��� �� ��� ���� ���."
!define PIDGIN_IS_RUNNING			"������ ����� �� ��� �� ��� ���� ���. ����� �� ����� ���� ��� � ������ ��� ����."

; License Page
!define PIDGIN_LICENSE_BUTTON			"��� >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) ��� ���� ����� ����� ��� (GPL) ����� ��� ���. ��� ���� ���� ���� ����ڝ����� ����� ����� ��� ���. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"��ѐ�� ��������� �������� ����� (������)"
!define GTK_SECTION_TITLE			"���� ���� ����� GTK+� (ǐ� ���� ����� ������ ���)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"���������"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"������"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"���� ����"
!define PIDGIN_SECTION_DESCRIPTION		"�������� � DLL��� ���� �����"
!define GTK_SECTION_DESCRIPTION		"��������� ���� ����� ������ ��� ����� �� ����� �� �� ������� �의��"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"���������� ��������� �����"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"����� ������� �� ����� ��� ������"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"����� ���� ���� ����� �� ��� ����"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"���� ��� ����� �� ������"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"��� ������ �� ����� �� �� ��� ���� ��� ��� ��� ����. ���� ���� ���� ��� ���� ����� ��� �����."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"�������� ����� ��������"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"��ݝ����� �������� ����� registery ���� �� ���� ���.$\r ��� ��� ����� ���� ��� ������ �� ��� ���� ����."
!define un.PIDGIN_UNINSTALL_ERROR_2		"��� ����� ���� ���� ��� ��� ������ �� ������."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"�������� ��؝���� ������"
!define PIDGIN_SPELLCHECK_ERROR		"��� ���� ��� ��؝��� ������"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"�������� ��؝���� ������. (���� ��� ����� �������� ���� ���)"

