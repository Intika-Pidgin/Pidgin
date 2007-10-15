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
!define GTK_INSTALLER_NEEDED			"���� ���� ����� GTK+ �� ���� ����� �� ���� ��� ������ ���� ���.$\r����� ���� ${GTK_MIN_VERSION} �� ������� �� ���� ���� ����� GTK+ ��� ����"

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
!define GTK_UPGRADE_PROMPT			"���� ����� ���� ���� ����� GTK+ ���� ��. ��� ������ �� �� ������ ���Ͽ$\r����: $(^Name) ��� ��� ��� �� ������ ��� ���."
!define GTK_WINDOWS_INCOMPATIBLE		"������ 95/98�/Me �� GTK�+� ���� 2.8.0 �� ������ ��Ґ�� ����. GTK+ ${GTK_INSTALL_VERSION} ��� ������ ��.$\r ǐ�  GTK+ ${GTK_MIN_VERSION} �� ������ �� ����� ��� ������ϡ ��� ��� ����� ��."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"���� ��� ����� �� ������"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"��� ������ �� ����� �� �� ��� ���� ��� ��� ��� ����. ���� ���� ���� ��� ���� ����� ��� �����."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"��� ���� ��� ���� ���� ����� GTK+�."
!define GTK_BAD_INSTALL_PATH			"����� �� ���� ������� ���� ������ �� ����� ����."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"�������� ����� ��������"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"��ݝ����� �������� ����� registery ���� �� ���� ���.$\r ��� ��� ����� ���� ��� ������ �� ��� ���� ����."
!define un.PIDGIN_UNINSTALL_ERROR_2		"��� ����� ���� ���� ��� ��� ������ �� ������."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"�������� ��؝���� ������"
!define PIDGIN_SPELLCHECK_ERROR		"��� ���� ��� ��؝��� ������"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"��� ���� ��� ��ʝ���� ��؝��� ������"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"�������� ��؝���� ������. (���� ��� ����� �������� ���� ���)"
!define ASPELL_INSTALL_FAILED			"��� Ԙ�� ����"
!define PIDGIN_SPELLCHECK_BRETON		"���������"
!define PIDGIN_SPELLCHECK_CATALAN		"�������"
!define PIDGIN_SPELLCHECK_CZECH		"���"
!define PIDGIN_SPELLCHECK_WELSH		"�����"
!define PIDGIN_SPELLCHECK_DANISH		"�����ј�"
!define PIDGIN_SPELLCHECK_GERMAN		"������"
!define PIDGIN_SPELLCHECK_GREEK		"������"
!define PIDGIN_SPELLCHECK_ENGLISH		"������"
!define PIDGIN_SPELLCHECK_ESPERANTO		"�Ӂ�����"
!define PIDGIN_SPELLCHECK_SPANISH		"�Ӂ������"
!define PIDGIN_SPELLCHECK_FAROESE		"������"
!define PIDGIN_SPELLCHECK_FRENCH		"�������"
!define PIDGIN_SPELLCHECK_ITALIAN		"���������"
!define PIDGIN_SPELLCHECK_DUTCH		"�����"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"����"
!define PIDGIN_SPELLCHECK_POLISH		"�������"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"�������"
!define PIDGIN_SPELLCHECK_ROMANIAN		"���������"
!define PIDGIN_SPELLCHECK_RUSSIAN		"����"
!define PIDGIN_SPELLCHECK_SLOVAK		"����ǘ�"
!define PIDGIN_SPELLCHECK_SWEDISH		"�����"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"�������"

