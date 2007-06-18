;;
;;  trad-chinese.nsh
;;
;;  Traditional Chineese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page:950
;;
;;  Author: Paladin R. Liu <paladin@ms1.hinet.net>
;;  Minor updates: Ambrose C. Li <acli@ada.dhs.org>
;;
;;  Last Updated: May 21, 2007
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"�w�˵{�����b���椤�C"
!define PIDGIN_IS_RUNNING				"Pidgin ���b���椤�A�Х������o�ӵ{����A��w�ˡC"
!define GTK_INSTALLER_NEEDED			"�䤣��ŦX�� GTK+ �������ҩάO�ݭn�Q��s�C$\r�Цw�� v${GTK_MIN_VERSION} �ΥH�W������ GTK+ �������ҡC"

; License Page
!define PIDGIN_LICENSE_BUTTON			"�U�@�B >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) �ĥ� GNU General Public License (GPL) ���v�o�G�C�b���C�X���v�ѡA�ȧ@���ѦҤ��ΡC$_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin �D�{�� (����)"
!define GTK_SECTION_TITLE			"GTK+ �������� (����)"
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
!define GTK_WINDOWS_INCOMPATIBLE		"�۪��� 2.8.0 �}�l�AGTK�� �P Windows 95/98/Me �w���A�ۮe�AGTK+ ${GTK_INSTALL_VERSION} �]���N���|�Q�w�ˡC$\r�p�G�t�Τ������w�g�w�˪� GTK+ ${GTK_MIN_VERSION} �Χ�s�������A�w�˵{���N�H�Y�����C"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"���X Windows Pidgin ����"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"�L�k�����ثe�w�w�˪� Pidgin�A�s�����N�b���g�����ª��������p�U�i��w�ˡC"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"�w�� GTK+ �������Үɵo�Ϳ��~�C"
!define GTK_BAD_INSTALL_PATH			"�z�ҿ�J���w�˥ؿ��L�k�s���ΫإߡC"

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI �B�z�{��"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"�����{���L�k��� Pidgin ���w�˸�T�C$\r�o���ӬO����L���ϥΪ̭��s�w�ˤF�o�ӵ{���C"
!define un.PIDGIN_UNINSTALL_ERROR_2		"�z�ثe���v���L�k���� Pidgin�C"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"���r�ˬd�\��"
!define PIDGIN_SPELLCHECK_ERROR			"�w�˫��r�ˬd�~���o�Ϳ��~"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"�w�˫��r�ˬd�Ϊ�����~���o�Ϳ��~"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"���r�ˬd�䴩�]�w�˶������ں����s�u�^�C"
!define ASPELL_INSTALL_FAILED			"�w�˥���"
!define PIDGIN_SPELLCHECK_BRETON		"�����h����"
!define PIDGIN_SPELLCHECK_CATALAN		"�[������"
!define PIDGIN_SPELLCHECK_CZECH			"���J��"
!define PIDGIN_SPELLCHECK_WELSH			"�º�����"
!define PIDGIN_SPELLCHECK_DANISH		"������"
!define PIDGIN_SPELLCHECK_GERMAN		"�w��"
!define PIDGIN_SPELLCHECK_GREEK			"��þ��"
!define PIDGIN_SPELLCHECK_ENGLISH		"�^��"
!define PIDGIN_SPELLCHECK_ESPERANTO		"�@�ɻy"
!define PIDGIN_SPELLCHECK_SPANISH		"��Z����"
!define PIDGIN_SPELLCHECK_FAROESE		"�kù�s�q��"
!define PIDGIN_SPELLCHECK_FRENCH		"�k��"
!define PIDGIN_SPELLCHECK_ITALIAN		"�N�j�Q��"
!define PIDGIN_SPELLCHECK_DUTCH			"������"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"���¤�"
!define PIDGIN_SPELLCHECK_POLISH		"�i����"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"���"
!define PIDGIN_SPELLCHECK_ROMANIAN		"ù�����Ȥ�"
!define PIDGIN_SPELLCHECK_RUSSIAN		"�X��"
!define PIDGIN_SPELLCHECK_SLOVAK		"������J��"
!define PIDGIN_SPELLCHECK_SWEDISH		"����"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"�Q�J����"

