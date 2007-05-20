;;  vim:syn=winbatch:encoding=cp932:
;;
;;  japanese.nsh
;;
;;  Japanese language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 932
;;
;;  Author: "Takeshi Kurosawa" <t-kuro@abox23.so-net.ne.jp>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"�C���X�g�[�������Ɏ��s����Ă��܂�"
!define GAIM_IS_RUNNING				"Gaim �����s����Ă��܂��BGaim ���I�����Ă���ēx���s���Ă�������"
!define GTK_INSTALLER_NEEDED			"GTK+�����^�C�������������������̓A�b�v�O���[�h����K�v������܂��B$\rv${GTK_MIN_VERSION}�������͂���ȏ��GTK+�����^�C�����C���X�g�[�����Ă��������B"

; License Page
!define GAIM_LICENSE_BUTTON			"���� >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name)��GPL���C�Z���X�̌��Ń����[�X����Ă��܂��B���C�Z���X�͂����ɎQ�l�̂��߂����ɒ񋟂���Ă��܂��B $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim�C���X�^���g���b�Z���W�� (�K�{)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (�K�{)"
!define GTK_THEMES_SECTION_TITLE		"GTK+�̃e�[�}"
!define GTK_NOTHEME_SECTION_TITLE		"�e�[�}�Ȃ�"
!define GTK_WIMP_SECTION_TITLE			"Wimp�e�[�}"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve�e�[�}"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue�e�[�}"
!define GAIM_SHORTCUTS_SECTION_TITLE		"�V���[�g�J�b�g"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"�f�X�N�g�b�v"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"�X�^�[�g�A�b�v"
!define GAIM_SECTION_DESCRIPTION		"Gaim�̊j�ƂȂ�t�@�C����dll"
!define GTK_SECTION_DESCRIPTION			"Gaim�̎g���Ă���}���`�v���b�g�t�H�[��GUI�c�[���L�b�g"
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+�̃e�[�}�́AGTK+�̃A�v���P�[�V�����̃��b�N���t�B�[����ς����܂��B"
!define GTK_NO_THEME_DESC			"GTK+�̃e�[�}���C���X�g�[�����Ȃ�"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator)��Windows�̃f�X�N�g�b�v���Ƃ悭���a�����e�[�}�ł��B"
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve�e�[�}�B"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblue�e�[�}�B"
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Gaim �����s���邽�߂̃V���[�g�J�b�g"
!define GAIM_DESKTOP_SHORTCUT_DESC		"�f�X�N�g�b�v�� Gaim �̃V���[�g�J�b�g���쐬����"
!define GAIM_STARTMENU_SHORTCUT_DESC		"�X�^�[�g���j���[�� Gaim �̍��ڂ��쐬����"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"�Â��o�[�W������GTK+�����^�C����������܂����B�A�b�v�O���[�h���܂���?$\r����: Gaim�̓A�b�v�O���[�h���Ȃ����蓮���Ȃ��ł��傤�B"

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Windows Gaim��Web�y�[�W��K��Ă��������B"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (�폜�̂�)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+�����^�C���̃C���X�g�[���ŃG���[���������܂����B"
!define GTK_BAD_INSTALL_PATH			"���Ȃ��̓��͂����p�X�ɃA�N�Z�X�܂��͍쐬�ł��܂���B"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"���Ȃ���GTK+�̃e�[�}���C���X�g�[�����錠���������Ă��܂���B"

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"�A���C���X�g�[����Gaim�̃��W�X�g���G���g���𔭌��ł��܂���ł����B$\r�����炭�ʂ̃��[�U�ɃC���X�g�[�����ꂽ�ł��傤�B"
!define un.GAIM_UNINSTALL_ERROR_2		"���Ȃ��͂��̃A�v���P�[�V�������A���C���X�g�[�����錠���������Ă��܂���B"

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"�X�y���`�F�b�N�̃T�|�[�g"
!define GAIM_SPELLCHECK_ERROR			"�X�y���`�F�b�N�̃C���X�g�[���Ɏ��s���܂���"
!define GAIM_SPELLCHECK_DICT_ERROR		"�X�y���`�F�b�N�����̃C���X�g�[���Ɏ��s���܂����B"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"�X�y���`�F�b�N�̃T�|�[�g  (�C���^�[�l�b�g�ڑ����C���X�g�[���ɕK�v�ł�)"
!define ASPELL_INSTALL_FAILED			"�C���X�g�[���Ɏ��s���܂���"
!define GAIM_SPELLCHECK_BRETON			"�u���^�[�j����"
!define GAIM_SPELLCHECK_CATALAN			"�J�^���[�j����"
!define GAIM_SPELLCHECK_CZECH			"�`�F�R��"
!define GAIM_SPELLCHECK_WELSH			"�E�F�[���Y��"
!define GAIM_SPELLCHECK_DANISH			"�f���}�[�N��"
!define GAIM_SPELLCHECK_GERMAN			"�h�C�c��"
!define GAIM_SPELLCHECK_GREEK			"�M���V����"
!define GAIM_SPELLCHECK_ENGLISH			"�p��"
!define GAIM_SPELLCHECK_ESPERANTO		"�G�X�y�����g��"
!define GAIM_SPELLCHECK_SPANISH			"�X�y�C����"
!define GAIM_SPELLCHECK_FAROESE			"�t�F���[��"
!define GAIM_SPELLCHECK_FRENCH			"�t�����X��"
!define GAIM_SPELLCHECK_ITALIAN			"�C�^���A��"
!define GAIM_SPELLCHECK_DUTCH			"�I�����_��"
!define GAIM_SPELLCHECK_NORWEGIAN		"�m���E�F�[��"
!define GAIM_SPELLCHECK_POLISH			"�|�[�����h��"
!define GAIM_SPELLCHECK_PORTUGUESE		"�|���g�K����"
!define GAIM_SPELLCHECK_ROMANIAN		"���[�}�j�A��"
!define GAIM_SPELLCHECK_RUSSIAN			"���V�A��"
!define GAIM_SPELLCHECK_SLOVAK			"�X�����@�L�A��"
!define GAIM_SPELLCHECK_SWEDISH			"�X�E�F�[�f����"
!define GAIM_SPELLCHECK_UKRAINIAN		"�E�N���C�i��"

