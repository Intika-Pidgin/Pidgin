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

; Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"��� Windows Pidgin ��ҳ"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "ж�س����Ҳ��� Pidgin ��ע�����Ŀ��$\r������������û���װ�˴˳���"
!define un.PIDGIN_UNINSTALL_ERROR_2         "��û��Ȩ��ж�ش˳���"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"ƴд���֧��"
!define PIDGIN_SPELLCHECK_ERROR			"��װƴд������"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"ƴд���֧�֡�(��װ��Ҫ���ӵ� Internet)"
