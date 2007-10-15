;;
;;  hebrew.nsh
;;
;;  Hebrew language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1255
;;
;;  Updated: Shalom Craimer <scraimer@gmail.com>
;;  Origional Author: Eugene Shcherbina <eugene@websterworlds.com>
;;  Version 3
;;

; Startup checks
!define INSTALLER_IS_RUNNING			"����� ������ ��� ���."
!define PIDGIN_IS_RUNNING			"���� �� ����'�� ��� ��. �� ����� �� ����'�� ������ ����."
!define GTK_INSTALLER_NEEDED			".�� ����� �� ����� ������ GTK+ �����$\r����� ���� v${GTK_MIN_VERSION} .GTK+ �� ����� ���� �� �����"

; License Page
!define PIDGIN_LICENSE_BUTTON			"��� >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) .������� ���� ��� ����� ���� ���� .GPL ������ ��� ������ $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"(����) .Pidgin �����"
!define GTK_SECTION_TITLE			"(����) .GTK+ �����"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"������ ���"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"����� ������"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"����� �����"
!define PIDGIN_SECTION_DESCRIPTION		".������� DLL-� Pidgin ����"
!define GTK_SECTION_DESCRIPTION		"�����-��������, �� ���� ����'�� GUI ���"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"������-��� ������ ����'��"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"��� �����-��� ���� ����'�� �� ����� ������"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"��� �����-��� ���� ����'�� ������ �����"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"A?�����. ����� GTK+ ����� ���� ��$\rNote: .���� �� ����� �� �� $(^Name)"
!define GTK_WINDOWS_INCOMPATIBLE		"������ 95/98/������� ���� ������ �-GTK+ 2.8.0 �����. GTK+ ${GTK_INSTALL_VERSION} �� �����.$\r�� ��� �� GTK+ ${GTK_MIN_VERSION} ����� �����, ������ ��� �����."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		".Pidgin���� ���� �� "

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"�� ����� ����� �� ������ ������� �� ����'��. ������ ����� ����� ��� ���� ����� �������."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			".GTK+ ����� ������ �����"
!define GTK_BAD_INSTALL_PATH			".������ ������ �� ���� �������"

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"����� URI"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		".GTK+ ������ �� ���� �� �������� ��$\r.���� ����� ������� ��� ����� �� ������ ����"
!define un.PIDGIN_UNINSTALL_ERROR_2		".��� �� ���� ����� ����� ���"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"����� ������� ����"
!define PIDGIN_SPELLCHECK_ERROR		"����� ������ ������ ����"
!define PIDGIN_SPELLCHECK_DICT_ERROR	"����� ������ ����� ������� ����"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"����� ���� ������ ���� (���� ����� ������� ������)"
!define ASPELL_INSTALL_FAILED			"������ �����"
!define PIDGIN_SPELLCHECK_BRETON		"�����"
!define PIDGIN_SPELLCHECK_CATALAN		"�������"
!define PIDGIN_SPELLCHECK_CZECH		"�'���"
!define PIDGIN_SPELLCHECK_WELSH		"������"
!define PIDGIN_SPELLCHECK_DANISH		"����"
!define PIDGIN_SPELLCHECK_GERMAN		"������"
!define PIDGIN_SPELLCHECK_GREEK		"������"
!define PIDGIN_SPELLCHECK_ENGLISH		"������"
!define PIDGIN_SPELLCHECK_ESPERANTO		"�������"
!define PIDGIN_SPELLCHECK_SPANISH		"������"
!define PIDGIN_SPELLCHECK_FAROESE		"�����"
!define PIDGIN_SPELLCHECK_FRENCH		"������"
!define PIDGIN_SPELLCHECK_ITALIAN		"�������"
!define PIDGIN_SPELLCHECK_DUTCH		"�������"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"��������"
!define PIDGIN_SPELLCHECK_POLISH		"������"
!define PIDGIN_SPELLCHECK_PORTUGUESE	"���������"
!define PIDGIN_SPELLCHECK_ROMANIAN		"������"
!define PIDGIN_SPELLCHECK_RUSSIAN		"�����"
!define PIDGIN_SPELLCHECK_SLOVAK		"�������"
!define PIDGIN_SPELLCHECK_SWEDISH		"������"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"���������"

