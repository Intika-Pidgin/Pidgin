;;  vim:syn=winbatch:encoding=cp932:
;;
;;  japanese.nsh
;;
;;  Japanese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 932
;;
;;  Author: "Takeshi Kurosawa" <t-kuro@abox23.so-net.ne.jp>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"�C���X�g�[�������Ɏ��s����Ă��܂�"
!define PIDGIN_IS_RUNNING				"Pidgin �����s����Ă��܂��BPidgin ���I�����Ă���ēx���s���Ă�������"
!define GTK_INSTALLER_NEEDED			"GTK+�����^�C�������������������̓A�b�v�O���[�h����K�v������܂��B$\rv${GTK_MIN_VERSION}�������͂���ȏ��GTK+�����^�C�����C���X�g�[�����Ă��������B"

; License Page
!define PIDGIN_LICENSE_BUTTON			"���� >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name)��GPL���C�Z���X�̌��Ń����[�X����Ă��܂��B���C�Z���X�͂����ɎQ�l�̂��߂����ɒ񋟂���Ă��܂��B $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin�C���X�^���g���b�Z���W�� (�K�{)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (�K�{)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"�V���[�g�J�b�g"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"�f�X�N�g�b�v"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"�X�^�[�g�A�b�v"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin�̊j�ƂȂ�t�@�C����dll"
!define GTK_SECTION_DESCRIPTION			"Pidgin�̎g���Ă���}���`�v���b�g�t�H�[��GUI�c�[���L�b�g"


!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Pidgin �����s���邽�߂̃V���[�g�J�b�g"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"�f�X�N�g�b�v�� Pidgin �̃V���[�g�J�b�g���쐬����"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"�X�^�[�g���j���[�� Pidgin �̍��ڂ��쐬����"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"�Â��o�[�W������GTK+�����^�C����������܂����B�A�b�v�O���[�h���܂���?$\r����: $(^Name)�̓A�b�v�O���[�h���Ȃ����蓮���Ȃ��ł��傤�B"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Windows Pidgin��Web�y�[�W��K��Ă��������B"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+�����^�C���̃C���X�g�[���ŃG���[���������܂����B"
!define GTK_BAD_INSTALL_PATH			"���Ȃ��̓��͂����p�X�ɃA�N�Z�X�܂��͍쐬�ł��܂���B"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"�A���C���X�g�[����Pidgin�̃��W�X�g���G���g���𔭌��ł��܂���ł����B$\r�����炭�ʂ̃��[�U�ɃC���X�g�[�����ꂽ�ł��傤�B"
!define un.PIDGIN_UNINSTALL_ERROR_2		"���Ȃ��͂��̃A�v���P�[�V�������A���C���X�g�[�����錠���������Ă��܂���B"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"�X�y���`�F�b�N�̃T�|�[�g"
!define PIDGIN_SPELLCHECK_ERROR			"�X�y���`�F�b�N�̃C���X�g�[���Ɏ��s���܂���"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"�X�y���`�F�b�N�����̃C���X�g�[���Ɏ��s���܂����B"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"�X�y���`�F�b�N�̃T�|�[�g  (�C���^�[�l�b�g�ڑ����C���X�g�[���ɕK�v�ł�)"
!define ASPELL_INSTALL_FAILED			"�C���X�g�[���Ɏ��s���܂���"
!define PIDGIN_SPELLCHECK_BRETON			"�u���^�[�j����"
!define PIDGIN_SPELLCHECK_CATALAN			"�J�^���[�j����"
!define PIDGIN_SPELLCHECK_CZECH			"�`�F�R��"
!define PIDGIN_SPELLCHECK_WELSH			"�E�F�[���Y��"
!define PIDGIN_SPELLCHECK_DANISH			"�f���}�[�N��"
!define PIDGIN_SPELLCHECK_GERMAN			"�h�C�c��"
!define PIDGIN_SPELLCHECK_GREEK			"�M���V����"
!define PIDGIN_SPELLCHECK_ENGLISH			"�p��"
!define PIDGIN_SPELLCHECK_ESPERANTO		"�G�X�y�����g��"
!define PIDGIN_SPELLCHECK_SPANISH			"�X�y�C����"
!define PIDGIN_SPELLCHECK_FAROESE			"�t�F���[��"
!define PIDGIN_SPELLCHECK_FRENCH			"�t�����X��"
!define PIDGIN_SPELLCHECK_ITALIAN			"�C�^���A��"
!define PIDGIN_SPELLCHECK_DUTCH			"�I�����_��"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"�m���E�F�[��"
!define PIDGIN_SPELLCHECK_POLISH			"�|�[�����h��"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"�|���g�K����"
!define PIDGIN_SPELLCHECK_ROMANIAN		"���[�}�j�A��"
!define PIDGIN_SPELLCHECK_RUSSIAN			"���V�A��"
!define PIDGIN_SPELLCHECK_SLOVAK			"�X�����@�L�A��"
!define PIDGIN_SPELLCHECK_SWEDISH			"�X�E�F�[�f����"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"�E�N���C�i��"

