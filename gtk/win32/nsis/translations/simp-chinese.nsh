;;  vim:syn=winbatch:encoding=8bit-cp936:fileencoding=8bit-cp936:
;;  simp-chinese.nsh
;;
;;  Simplified Chinese language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 936
;;
;;  Author: Funda Wang" <fundawang@linux.net.cn>
;;  Version 2
;;

; Startup GTK+ check
!define INSTALLER_IS_RUNNING			"��װ�����Ѿ����С�"
!define GAIM_IS_RUNNING			"Gaim ��ʵ�����������С����˳� Gaim Ȼ������һ�Ρ�"
!define GTK_INSTALLER_NEEDED			"����ȱ�� GTK+ ����ʱ�̻�����������Ҫ���¸û�����$\r�밲װ v${GTK_MIN_VERSION} ����߰汾�� GTK+ ����ʱ�̻���"

; License Page
!define GAIM_LICENSE_BUTTON			"��һ�� >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) �� GPL ��ɷ������ڴ��ṩ����ɽ�Ϊ�ο���$_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim ��ʱͨѶ����(����)"
!define GTK_SECTION_TITLE			"GTK+ ����ʱ�̻���(����)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ ����"
!define GTK_NOTHEME_SECTION_TITLE		"������"
!define GTK_WIMP_SECTION_TITLE		"Wimp ����"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve ����"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue ����"
!define GAIM_SHORTCUTS_SECTION_TITLE "��ݷ�ʽ"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE "����"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE "��ʼ�˵�"
!define GAIM_SECTION_DESCRIPTION		"Gaim �����ļ��� DLLs"
!define GTK_SECTION_DESCRIPTION		"Gaim ���õĶ�ƽ̨ GUI ���߰�"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ ������Ը��� GTK+ ����Ĺ۸С�"
!define GTK_NO_THEME_DESC			"����װ GTK+ ����"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp ���ʺ� Windows ���滷���� GTK+ ���⡣"
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve ���⡣"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Lighthouseblue ���⡣"
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION   "���� Gaim �Ŀ�ݷ�ʽ"
!define GAIM_DESKTOP_SHORTCUT_DESC   "�������ϴ��� Gaim �Ŀ�ݷ�ʽ"
!define GAIM_STARTMENU_SHORTCUT_DESC   "�ڿ�ʼ�˵��д��� Gaim �Ŀ�ݷ�ʽ"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"�����˾ɰ汾�� GTK+ ����ʱ�̡�����Ҫ������?$\rע��: �������������������� Gaim �����޷�������"

; Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"��� Windows Gaim ��ҳ"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (ֻ��ɾ��)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"��װ GTK+ ����ʱ��ʧ�ܡ�"
!define GTK_BAD_INSTALL_PATH			"�޷����ʻ򴴽��������·����"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"��û��Ȩ�ް�װ GTK+ ���⡣"

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "ж�س����Ҳ��� Gaim ��ע�����Ŀ��$\r������������û���װ�˴˳���"
!define un.GAIM_UNINSTALL_ERROR_2         "��û��Ȩ��ж�ش˳���"

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"ƴд���֧��"
!define GAIM_SPELLCHECK_ERROR			"��װƴд������"
!define GAIM_SPELLCHECK_DICT_ERROR		"��װƴд����ֵ����"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"ƴд���֧�֡�(��װ��Ҫ���ӵ� Internet)"
!define ASPELL_INSTALL_FAILED			"��װʧ��"
!define GAIM_SPELLCHECK_BRETON			"���������"
!define GAIM_SPELLCHECK_CATALAN			"��̩��������"
!define GAIM_SPELLCHECK_CZECH			"�ݿ���"
!define GAIM_SPELLCHECK_WELSH			"����ʿ��"
!define GAIM_SPELLCHECK_DANISH			"������"
!define GAIM_SPELLCHECK_GERMAN			"����"
!define GAIM_SPELLCHECK_GREEK			"ϣ����"
!define GAIM_SPELLCHECK_ENGLISH			"Ӣ��"
!define GAIM_SPELLCHECK_ESPERANTO		"������"
!define GAIM_SPELLCHECK_SPANISH			"��������"
!define GAIM_SPELLCHECK_FAROESE			"������"
!define GAIM_SPELLCHECK_FRENCH			"����"
!define GAIM_SPELLCHECK_ITALIAN			"�������"
!define GAIM_SPELLCHECK_DUTCH			"������"
!define GAIM_SPELLCHECK_NORWEGIAN		"Ų����"
!define GAIM_SPELLCHECK_POLISH			"������"
!define GAIM_SPELLCHECK_PORTUGUESE		"��������"
!define GAIM_SPELLCHECK_ROMANIAN			"����������"
!define GAIM_SPELLCHECK_RUSSIAN			"����"
!define GAIM_SPELLCHECK_SLOVAK			"˹�工����"
!define GAIM_SPELLCHECK_SWEDISH			"�����"
!define GAIM_SPELLCHECK_UKRAINIAN		"�ڿ�����"
