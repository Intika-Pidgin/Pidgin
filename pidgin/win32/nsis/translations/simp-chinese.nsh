;;  vim:syn=winbatch:encoding=8bit-cp936:fileencoding=8bit-cp936:
;;  simp-chinese.nsh
;;
;;  Simplified Chinese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 936
;;
;;  Author: Funda Wang" <fundawang@linux.net.cn>
;;  Version 2
;;

; Startup GTK+ check
!define INSTALLER_IS_RUNNING			"��װ�����Ѿ����С�"
!define PIDGIN_IS_RUNNING			"Pidgin ��ʵ�����������С����˳� Pidgin Ȼ������һ�Ρ�"
!define GTK_INSTALLER_NEEDED			"����ȱ�� GTK+ ����ʱ�̻�����������Ҫ���¸û�����$\r�밲װ v${GTK_MIN_VERSION} ����߰汾�� GTK+ ����ʱ�̻���"

; License Page
!define PIDGIN_LICENSE_BUTTON			"��һ�� >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) �� GPL ��ɷ������ڴ��ṩ����ɽ�Ϊ�ο���$_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin ��ʱͨѶ����(����)"
!define GTK_SECTION_TITLE			"GTK+ ����ʱ�̻���(����)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "��ݷ�ʽ"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "����"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "��ʼ�˵�"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin �����ļ��� DLLs"
!define GTK_SECTION_DESCRIPTION		"Pidgin ���õĶ�ƽ̨ GUI ���߰�"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "���� Pidgin �Ŀ�ݷ�ʽ"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "�������ϴ��� Pidgin �Ŀ�ݷ�ʽ"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "�ڿ�ʼ�˵��д��� Pidgin �Ŀ�ݷ�ʽ"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"�����˾ɰ汾�� GTK+ ����ʱ�̡�����Ҫ������?$\rע��: �������������������� $(^Name) �����޷�������"

; Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"��� Windows Pidgin ��ҳ"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"��װ GTK+ ����ʱ��ʧ�ܡ�"
!define GTK_BAD_INSTALL_PATH			"�޷����ʻ򴴽��������·����"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "ж�س����Ҳ��� Pidgin ��ע�����Ŀ��$\r������������û���װ�˴˳���"
!define un.PIDGIN_UNINSTALL_ERROR_2         "��û��Ȩ��ж�ش˳���"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"ƴд���֧��"
!define PIDGIN_SPELLCHECK_ERROR			"��װƴд������"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"��װƴд����ֵ����"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"ƴд���֧�֡�(��װ��Ҫ���ӵ� Internet)"
!define ASPELL_INSTALL_FAILED			"��װʧ��"
!define PIDGIN_SPELLCHECK_BRETON			"���������"
!define PIDGIN_SPELLCHECK_CATALAN			"��̩��������"
!define PIDGIN_SPELLCHECK_CZECH			"�ݿ���"
!define PIDGIN_SPELLCHECK_WELSH			"����ʿ��"
!define PIDGIN_SPELLCHECK_DANISH			"������"
!define PIDGIN_SPELLCHECK_GERMAN			"����"
!define PIDGIN_SPELLCHECK_GREEK			"ϣ����"
!define PIDGIN_SPELLCHECK_ENGLISH			"Ӣ��"
!define PIDGIN_SPELLCHECK_ESPERANTO		"������"
!define PIDGIN_SPELLCHECK_SPANISH			"��������"
!define PIDGIN_SPELLCHECK_FAROESE			"������"
!define PIDGIN_SPELLCHECK_FRENCH			"����"
!define PIDGIN_SPELLCHECK_ITALIAN			"�������"
!define PIDGIN_SPELLCHECK_DUTCH			"������"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Ų����"
!define PIDGIN_SPELLCHECK_POLISH			"������"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"��������"
!define PIDGIN_SPELLCHECK_ROMANIAN			"����������"
!define PIDGIN_SPELLCHECK_RUSSIAN			"����"
!define PIDGIN_SPELLCHECK_SLOVAK			"˹�工����"
!define PIDGIN_SPELLCHECK_SWEDISH			"�����"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"�ڿ�����"
