# UPDATING VERSION NUMBERS FOR RELEASES
#
# The version number is:
#   <major>.<minor>.<micro><extra>
#
# micro += 1
#
# If any functions have been added to libpurple, Pidgin, or Finch:
#   micro = 0
#   minor += 1
#
# If backwards compatibility has been broken in libpurple, Pidgin, or Finch:
#   micro = 0
#   minor = 0
#   major += 1
#   purple_soversion += 1
#
# extra should be similar to one of the following:
#   For beta releases:          '-beta2'
#   For code under development: '-devel'
#   For production releases:    ''
#
project('pidgin', 'c',
    version : '3.0.0-devel',
    meson_version : '>=0.47.0')
purple_soversion = 20

parts = meson.project_version().split('-')
if parts.length() > 1
  purple_extra_version = parts[1]
else
  purple_extra_version = ''
endif

parts = parts[0].split('.')
purple_major_version = parts[0]
purple_minor_version = parts[1]
purple_micro_version = parts[2]

add_project_arguments('-DHAVE_CONFIG_H=1', language : 'c')
conf = configuration_data()
man_conf = configuration_data()
version_conf = configuration_data()

conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('DISPLAY_VERSION', meson.project_version())

version_conf.set('PURPLE_MAJOR_VERSION', purple_major_version)
version_conf.set('PURPLE_MINOR_VERSION', purple_minor_version)
version_conf.set('PURPLE_MICRO_VERSION', purple_micro_version)
version_conf.set('PURPLE_EXTRA_VERSION', purple_extra_version)
version_conf.set('PURPLE_VERSION', meson.project_version())
version_conf.set('PURPLE_API_VERSION', purple_soversion)

PURPLE_LIB_VERSION = '@0@.@1@.@2@'.format(purple_soversion,
                                          purple_minor_version,
                                          purple_micro_version)

package_revision = vcs_tag(
    input : 'package_revision.h.in',
    output : 'package_revision.h',
    fallback : meson.project_version())

# For man pages.
man_conf.set('VERSION', meson.project_version())
man_conf.set('prefix', get_option('prefix'))

# Used for pkg-config files.
pkgconfig = import('pkgconfig')

sedpath = find_program('sed')

# Storing build arguments
meson.add_postconf_script('mkmesonconf.py')
conf.set('HAVE_MESON_CONFIG', true)

# Checks for programs.
compiler = meson.get_compiler('c')

# Check for Sun compiler
SUNCC = compiler.compiles('void main() {__SUNPRO_C;};')

# Check for Win32
if host_machine.system() == 'windows'
	windows = import('windows')

	IS_WIN32 = true
	ws2_32 = compiler.find_library('ws2_32')
	dnsapi = compiler.find_library('dnsapi')
	if build_machine.system() != 'windows'
		conf.set('IS_WIN32_CROSS_COMPILED', true)
	endif
	conf.set('WIN32_LEAN_AND_MEAN', true)

	conf.set('LIBPIDGIN_DLL_NAMEW',
	    'L"libpidgin3-@0@.dll"'.format(purple_soversion))
else
	IS_WIN32 = false
	ws2_32 = []
	dnsapi = []
endif

# Checks for header files.
# AC_HEADER_SYS_WAIT:
conf.set('HAVE_SYS_WAIT_H', compiler.has_header('sys/wait.h'))

foreach h : ['fcntl.h', 'unistd.h', 'stdint.h']
	if compiler.has_header(h)
		conf.set('HAVE_' + h.to_upper().underscorify(), true)
	else
		error(h + ' is required.')
	endif
endforeach

# Checks for typedefs, structures, and compiler characteristics.
time_t_size = compiler.sizeof('time_t',
    prefix : '''
#include <stdio.h>
#include <time.h>
''')
conf.set('SIZEOF_TIME_T', time_t_size)

conf.set('WORDS_BIGENDIAN', host_machine.endian() != 'little')

# Check for directories
if IS_WIN32
	foreach dir : ['bin', 'lib', 'data', 'sysconf', 'locale']
		path = join_paths(get_option('prefix'), get_option(dir + 'dir'))
		conf.set_quoted('WIN32_FHS_@0@DIR'.format(dir.to_upper()), path)
	endforeach

	conf.set('PURPLE_LIBDIR',
		 'wpurple_lib_dir("purple-@0@")'.format(purple_major_version))
	conf.set('PIDGIN_LIBDIR',
		 'wpurple_lib_dir("pidgin-@0@")'.format(purple_major_version))
	conf.set('FINCH_LIBDIR',
		 'wpurple_lib_dir("finch-@0@")'.format(purple_major_version))

	conf.set('PURPLE_DATADIR', 'wpurple_data_dir()')
	conf.set('PURPLE_SYSCONFDIR', 'wpurple_sysconf_dir()')
	conf.set('PURPLE_LOCALEDIR', 'wpurple_locale_dir()')
else
	foreach dir : ['data', 'sysconf', 'locale']
		path = join_paths(get_option('prefix'), get_option(dir + 'dir'))
		conf.set_quoted('PURPLE_@0@DIR'.format(dir.to_upper()), path)
	endforeach

	common_libdir = join_paths(get_option('prefix'), get_option('libdir'))
	conf.set_quoted('PURPLE_LIBDIR',
	                join_paths(common_libdir,
	                           'purple-@0@'.format(purple_major_version)))
	conf.set_quoted('PIDGIN_LIBDIR',
	                join_paths(common_libdir,
	                           'pidgin-@0@'.format(purple_major_version)))
	conf.set_quoted('FINCH_LIBDIR',
	                join_paths(common_libdir,
	                           'finch-@0@'.format(purple_major_version)))
endif

abslibdir = join_paths(get_option('prefix'), get_option('libdir'))
PURPLE_PLUGINDIR = join_paths(abslibdir, 'purple-@0@'.format(purple_major_version))
conf.set_quoted('PURPLE_PLUGINDIR', PURPLE_PLUGINDIR)
PIDGIN_PLUGINDIR = join_paths(abslibdir, 'pidgin-@0@'.format(purple_major_version))
conf.set_quoted('PIDGIN_PLUGINDIR', PIDGIN_PLUGINDIR)
FINCH_PLUGINDIR = join_paths(abslibdir, 'finch-@0@'.format(purple_major_version))
conf.set_quoted('FINCH_PLUGINDIR', FINCH_PLUGINDIR)

# Check for inet_aton
if not IS_WIN32
	if not compiler.has_function('inet_aton')
		if not compiler.has_function('inet_aton', args : '-lresolv')
			# FIXME: Someone needs to link with -lresolv if needed.
			error('inet_aton not found')
		endif
	endif
endif
if compiler.has_function('gethostent', args : '-lnsl')
	conf.set('HAVE_LIBNSL', true)
endif
if IS_WIN32
	conf.set('HAVE_GETADDRINFO', true)
	conf.set('HAVE_INET_NTOP', true)
else
	if not compiler.has_function('socket')
		if not compiler.has_function('socket', args : '-lsocket')
			error('socket not found')
		endif
	endif
	# If all goes well, by this point the previous two checks will have
	# pulled in -lsocket and -lnsl if we need them.
	if compiler.has_function('getaddrinfo')
		conf.set('HAVE_GETADDRINFO', true)
	else
		if compiler.has_function('getaddrinfo', args : '-lsocket -lnsl')
			conf.set('HAVE_GETADDRINFO', true)
			# FIXME: LIBS += declare_dependency(link_with : ['socket', 'nsl'])
		endif
	endif
	conf.set('HAVE_INET_NTOP',
	    compiler.has_function('inet_ntop'))
endif
conf.set('HAVE_GETIFADDRS',
    compiler.has_function('getifaddrs'))

# Check for socklen_t (in Unix98)
if IS_WIN32
	socket_header = 'ws2tcpip.h'
else
	socket_header = 'sys/socket.h'
endif
if not compiler.has_header_symbol(socket_header, 'socklen_t')
	code = '''
#include <sys/types.h>
#include <@0@>
int accept(int, struct sockaddr *, size_t *);
int main() {}
'''.format(socket_header)
	if compiler.compiles(code, name : 'socklen_t is size_t')
		conf.set('socklen_t', 'size_t')
	else
		conf.set('socklen_t', 'int')
	endif
endif

# Some systems do not have sa_len field for struct sockaddr.
conf.set('HAVE_STRUCT_SOCKADDR_SA_LEN',
    compiler.has_member('struct sockaddr', 'sa_len',
        prefix : '#include <@0@>'.format(socket_header)))

# Check for v6-only sockets
if IS_WIN32
	header = 'ws2tcpip.h'
else
	header = 'netinet/in.h'
endif
conf.set('HAVE_IPV6_V6ONLY',
    compiler.has_header_symbol(header, 'IPV6_V6ONLY'))

# Windows and Haiku do not use libm for the math functions, they are part
# of the C library
math = compiler.find_library('m')

# before gettexting, in case iconv matters
IOKIT = []
if host_machine.system() == 'darwin'
	if compiler.has_header('CoreFoundation/CoreFoundation.h')
		if compiler.has_header('IOKit/IOKitLib.h')
			conf.set('HAVE_IOKIT', true)
			IOKIT = dependency('appleframeworks',
			                   modules : ['IOKit', 'CoreFoundation'])
		endif
	endif

	if run_command('test', '-d', '/sw').returncode() == 0
		# FIXME: Not done...
		#CPPFLAGS="$CPPFLAGS -I/sw/include"
		#LDFLAGS="$LDFLAGS -L/sw/lib"
	endif
endif

# #######################################################################
# # Disable creation and installation of translation files
# #######################################################################

INSTALL_I18N = get_option('nls')

if INSTALL_I18N
	i18n = import('i18n')

	subdir('po')
endif

# #######################################################################
# # Check for GLib 2.44 (required)
# #######################################################################
glib = dependency('glib-2.0', version : '>= 2.48.0')
gio = dependency('gio-2.0')
gobject = dependency('gobject-2.0')
gthread = dependency('gthread-2.0')
gnome = import('gnome')

#######################################################################
# Check for GObject Introspection
#######################################################################

enable_introspection = dependency('gobject-introspection-1.0', version : '>= 1.30.0',
                                  required : get_option('introspection')).found()
conf.set('ENABLE_INTROSPECTION', enable_introspection)

#######################################################################
# Check Pidgin dependencies
#######################################################################
if get_option('gtkui')
	gtk = dependency('gtk+-3.0', version : '>= 3.22.0')

	talkatu_dep = dependency('talkatu', version: '>=0.1.0', required : false)
	if talkatu_dep.found()
		if enable_introspection
			talkatu_gir = 'Talkatu-0.0'
			talkatu_include_directories = include_directories(
			    join_paths(talkatu_dep.get_pkgconfig_variable('prefix'),
			               'share/gir-1.0'))
		else
			talkatu_include_directories = []
		endif
	else
		talkatu_proj = subproject('talkatu',
		    default_options : [
		        'doc=' + get_option('doc').to_string(),
		        'gobject-introspection=' + enable_introspection.to_string(),
		        'nls=' + get_option('nls').to_string(),
		    ]
		)
		talkatu_dep = talkatu_proj.get_variable('talkatu_dep')
		if enable_introspection
			talkatu_gir = talkatu_proj.get_variable('talkatu_gir')[0]
		endif
		talkatu_include_directories = []
	endif
endif	# GTK

ENABLE_GTK = get_option('gtkui')


#######################################################################
# Check if we should compile with X support
#######################################################################
if IS_WIN32
	x11 = disabler()
else
	x11 = dependency('x11', required : get_option('x'))
endif
conf.set('HAVE_X11', x11.found())

enable_gestures = get_option('gestures')
if not get_option('gtkui') or not x11.found()
	enable_gestures = false
endif

#######################################################################
# Check for LibXML2 (required)
#######################################################################
libxml = dependency('libxml-2.0', version : '>= 2.6.0')
if libxml.version().version_compare('<2.6.18')
	message('Versions of libxml2 < 2.6.18 may contain bugs that could cause XMPP messages to be discarded.')
endif

#######################################################################
# Check for JSON-GLib (required)
#######################################################################

json = dependency('json-glib-1.0', version : '>= 0.14.0')

#######################################################################
# Check for libsoup (required)
#######################################################################

libsoup = dependency('libsoup-2.4', version : '>= 2.42')

#######################################################################
# Check for GStreamer
#######################################################################

gstreamer = dependency('gstreamer-1.0', required : get_option('gstreamer'))
conf.set('USE_GSTREAMER', gstreamer.found())

#######################################################################
# Check for GStreamer Video
#######################################################################
if gstreamer.found()
	gstreamer_video = dependency('gstreamer-video-1.0',
	                             required : get_option('gstreamer-video'))
	conf.set('USE_GSTVIDEO', gstreamer_video.found())
else
	gstreamer_video = []
endif

#######################################################################
# Check for Farstream
#######################################################################
farstream = dependency('farstream-0.2', version : '>= 0.2.7',
                       required : get_option('farstream'))

#######################################################################
# Check for Voice and Video support
#######################################################################
if get_option('vv').enabled() or get_option('vv').auto()
	if gstreamer.found() and gstreamer_video.found() and farstream.found()
		conf.set('USE_VV', true)
		enable_vv = true
	else
		if get_option('vv').enabled()
			error('''
Dependencies for voice/video were not met.
Install the necessary gstreamer and farstream packages first.
Or use -Dvv=disabled if you do not need voice/video support.
			''')
		endif
		enable_vv = false
	endif
else
	enable_vv = false
endif

#######################################################################
# Check for Raw data streams support in Farstream
#######################################################################
if enable_vv
	gstreamer_app = dependency('gstreamer-app-1.0',
	                           required : false)
	if gstreamer_app.found()
		conf.set('USE_GSTAPP', true)
		conf.set('HAVE_MEDIA_APPLICATION', true)
	endif
else
	gstreamer_app = []
endif

#######################################################################
# Check for Meanwhile headers (for Sametime)
#######################################################################
meanwhile = dependency('meanwhile', version : ['>= 1.0.0', '< 2.0.0'], required : get_option('meanwhile'))
gmime = dependency('gmime-3.0', version : '>= 3.0.0', required : get_option('meanwhile'))
enable_meanwhile = meanwhile.found() and gmime.found()

#######################################################################
# Check for Native Avahi headers (for Bonjour)
#######################################################################

if IS_WIN32
	# Just keep enabled.
	enable_avahi = get_option('avahi').enabled() or get_option('avahi').auto()
	avahi = []
else
	# Attempt to autodetect Avahi
	avahi_client = dependency('avahi-client', required : get_option('avahi'))
	avahi_glib = dependency('avahi-glib', required : get_option('avahi'))
	avahi = [avahi_client, avahi_glib]
	enable_avahi = avahi_client.found() and avahi_glib.found()
endif


#######################################################################
# Check for SILC client includes and libraries
#######################################################################
silc = dependency('silcclient', version : '>= 1.1.1', required : get_option('silc'))

#######################################################################
# Check for Gadu-Gadu protocol library (libgadu)
#######################################################################

libgadu = dependency('libgadu', version : '>= 1.12.0', required : get_option('libgadu'))

if libgadu.found()
	if not compiler.has_function('gg_is_gpl_compliant', dependencies : libgadu)
		if get_option('libgadu').auto()
			libgadu = disabler()
		else
			message('''
libgadu is not compatible with the GPL when compiled with OpenSSL support.

To link against libgadu, please recompile it using:
./configure --with-openssl=no
Then rerun this Meson build
			''')
		endif
	endif
endif


DEFAULT_PRPLS = ['bonjour', 'facebook', 'gg', 'irc', 'jabber', 'novell',
                 'null', 'sametime', 'silc', 'simple', 'zephyr']
ALL_PRPLS = DEFAULT_PRPLS + ['null']

dynamic_list = get_option('dynamic-prpls').split(',')
if dynamic_list == ['all']
	dynamic_list = DEFAULT_PRPLS
endif
DYNAMIC_PRPLS = []
foreach prpl : dynamic_list
	if prpl == ''
		# The list was empty; do nothing.
	elif prpl == 'sametime' and not enable_meanwhile
		# Do nothing.
	elif prpl == 'bonjour' and not enable_avahi
		# Do nothing.
	elif prpl == 'silc' and not silc.found()
		# Do nothing.
	elif prpl == 'gg' and not libgadu.found()
		# Do nothing.
	elif prpl == 'zephyr' and IS_WIN32
		# Do nothing.
	else
		DYNAMIC_PRPLS += [prpl]
	endif
endforeach

DYNAMIC_BONJOUR = DYNAMIC_PRPLS.contains('bonjour')
DYNAMIC_FACEBOOK = DYNAMIC_PRPLS.contains('facebook')
DYNAMIC_GG  = DYNAMIC_PRPLS.contains('gg')
DYNAMIC_IRC = DYNAMIC_PRPLS.contains('irc')
DYNAMIC_JABBER = DYNAMIC_PRPLS.contains('jabber')
DYNAMIC_NOVELL = DYNAMIC_PRPLS.contains('novell')
DYNAMIC_NULL = DYNAMIC_PRPLS.contains('null')
DYNAMIC_SAMETIME = DYNAMIC_PRPLS.contains('sametime')
DYNAMIC_SILC = DYNAMIC_PRPLS.contains('silc')
DYNAMIC_SIMPLE = DYNAMIC_PRPLS.contains('simple')
DYNAMIC_ZEPHYR = DYNAMIC_PRPLS.contains('zephyr')

conf.set('HAVE_SYS_UTSNAME_H',
    compiler.has_header('sys/utsname.h'))
conf.set('HAVE_UNAME',
    compiler.has_function('uname'))


add_project_arguments(
    '-DPURPLE_DISABLE_DEPRECATED',
    '-DPIDGIN_DISABLE_DEPRECATED',
    '-DFINCH_DISABLE_DEPRECATED',
    '-DGNT_DISABLE_DEPRECATED',
    language : 'c')
if get_option('buildtype') != 'plain' and compiler.get_id() == 'gcc'
	# We enable -Wall later.
	# If it's set after the warning CFLAGS in the compiler invocation, it counteracts the -Wno... flags.
	# This leads to warnings we don't want.
#	CFLAGS=`echo $CFLAGS |$sedpath 's/-Wall//'`

	# ENABLE WARNINGS SUPPORTED BY THE VERSION OF GCC IN USE
	#
	# Future Possibilities
	#
	# Consider adding -Wbad-function-cast.
	#	This leads to spurious warnings using GPOINTER_TO_INT(), et al. directly on a function call.
	#		We'd need an intermediate variable.
	#
	foreach newflag : [
			'-Waggregate-return',
			'-Wcast-align',
			'-Wdeclaration-after-statement',
			'-Wendif-labels',
			'-Werror-implicit-function-declaration',
			'-Wextra -Wno-unused-parameter',
			'-Wformat',
			'-Wformat-security',
			'-Werror=format-security',
			'-Winit-self',
			'-Wmissing-declarations',
			'-Wmissing-noreturn',
			'-Wmissing-prototypes',
			'-Wpointer-arith',
			'-Wfloat-equal',
			'-Wundef']
		if compiler.has_argument(newflag)
			add_project_arguments(newflag, language : 'c')
		endif
	endforeach
endif
if get_option('buildtype') != 'plain' and SUNCC
	add_project_arguments('-features=extensions', language : 'c')
endif

pidginpath = find_program('pidgin', required : false)

if get_option('glib-errors-trace')
	if compiler.get_id() == 'clang'
		error('--enable-glib-errors-trace doesn\'t work with clang')
	endif
	conf.set('ENABLE_GLIBTRACE', true)
	add_project_arguments('-rdynamic', language : 'c')
endif

#######################################################################
# Check for Unity and Messaging Menu
# Remove when Ubuntu 16.04 is EOL
#######################################################################
UNITY = [
	dependency('unity', version : '>= 6.8', required : get_option('unity-integration')),
	dependency('messaging-menu', version : '>= 12.10', required : get_option('unity-integration'))
]
enable_unity = UNITY[0].found() and UNITY[1].found()
if enable_unity
	conf.set('USES_MM_CHAT_SECTION', 'X-MessagingMenu-UsesChatSection=true')
else
	conf.set('USES_MM_CHAT_SECTION', '')
endif

#######################################################################
# Check for Secret Service headers
#######################################################################

if IS_WIN32
	secretservice = disabler()
else
	secretservice = dependency('libsecret-1', required : get_option('secret-service'))
endif

#######################################################################
# Check for KWallet headers
#######################################################################

if IS_WIN32
	kwallet = disabler()
else
	# Ensure C++ compiler works
	add_languages('cpp')
	cxx_compiler = meson.get_compiler('cpp')
	add_project_arguments('-DHAVE_CONFIG_H=1', language : 'cpp')

	qt5 = import('qt5')

	qt5_dep = dependency('qt5', modules: ['Core'], required : get_option('kwallet'))

	kwallet = dependency('KF5Wallet', required : get_option('kwallet'))
endif

#######################################################################
# Check for GPlugin 0.28.0
#######################################################################
gplugin_dep = dependency('gplugin', version : '>= 0.28.0', required : false)
if gplugin_dep.found()
	if enable_introspection
		gplugin_gir = 'GPlugin-0.0'
		gplugin_include_directories = include_directories(
		    join_paths(gplugin_dep.get_pkgconfig_variable('prefix'),
		               'share/gir-1.0'))
	else
		gplugin_include_directories = []
	endif
else
	gplugin_proj = subproject('gplugin',
	    default_options : [
	        'doc=' + get_option('doc').to_string(),
	        'gobject-introspection=' + enable_introspection.to_string(),
	        'nls=' + get_option('nls').to_string(),
	    ]
	)
	gplugin_dep = gplugin_proj.get_variable('gplugin_dep')
	if enable_introspection
		gplugin_gir = gplugin_proj.get_variable('gplugin_gir')[0]
	endif
	gplugin_include_directories = []
endif

PLUGINS = get_option('plugins')

#######################################################################
# Check for Nettle (Crypto Library)
#######################################################################

nettle = dependency('nettle', version : '>= 3.0', required : get_option('nettle'))
conf.set('HAVE_NETTLE', nettle.found())

#######################################################################
# Check for Cyrus-SASL (for xmpp/irc)
#######################################################################
foreach func : ['snprintf', 'connect']
	conf.set('HAVE_' + func.to_upper(),
	    compiler.has_function(func))
endforeach
sasl = dependency('libsasl2', version : '>= 2.0', required : get_option('cyrus-sasl'))
conf.set('HAVE_CYRUS_SASL', sasl.found())

#######################################################################
# Check for external libzephyr
#######################################################################
ext_zephyr = dependency('zephyr', required : get_option('zephyr'))
EXTERNAL_LIBZEPHYR = ext_zephyr.found()
conf.set('LIBZEPHYR_EXT', EXTERNAL_LIBZEPHYR)

#######################################################################
# Check for Kerberos (for Zephyr)
#######################################################################
conf.set('ZEPHYR_INT32', 'long')
#AC_SUBST(KRB4_CFLAGS)
#AC_SUBST(KRB4_LDFLAGS)
#AC_SUBST(KRB4_LIBS)
kerberos = get_option('krb4')
if kerberos
	if kerberos != 'yes'
#		KRB4_CFLAGS='-I${kerberos}/include'
#		if test -d '$kerberos/include/kerberosIV' ; then
#			KRB4_CFLAGS='$KRB4_CFLAGS -I${kerberos}/include/kerberosIV'
#		fi
#		KRB4_LDFLAGS='-L${kerberos}/lib'
	elif run_command('test', '-d', '/usr/local/include/kerberosIV').returncode() == 0
#		KRB4_CFLAGS='-I/usr/local/include/kerberosIV'
	elif run_command('test', '-d', '/usr/include/kerberosIV').returncode() == 0
#		KRB4_CFLAGS='-I/usr/include/kerberosIV'
	endif
	conf.set('ZEPHYR_USES_KERBEROS', true)

#	AC_CHECK_LIB(krb4, krb_rd_req,
#			[KRB4_LIBS='-lkrb4 -ldes425 -lkrb5 -lk5crypto -lcom_err'],
#			[AC_CHECK_LIB(krb, krb_rd_req,
#				[KRB4_LIBS='-lkrb -ldes'],
#				[AC_MSG_ERROR([Kerberos 4 libraries not found])],
#				-ldes)],
#			-ldes425 -lkrb5 -lk5crypto -lcom_err)
#	AC_CHECK_FUNCS(krb_set_key krb_rd_req krb_get_lrealm)
#	AC_CHECK_FUNCS(krb_get_err_text krb_log)
	krb4 = []
endif
if not kerberos
	krb4 = []
endif

#AC_MSG_CHECKING(for me pot o' gold)
#AC_MSG_RESULT(no)
foreach func : ['timegm']
	conf.set('HAVE_' + func.to_upper(),
	    compiler.has_function(func))
endforeach
foreach header : 'sgtty.h sys/cdefs.h sys/file.h sys/filio.h sys/ioctl.h sys/msgbuf.h sys/select.h sys/wait.h termios.h'.split()
	conf.set('HAVE_' + header.to_upper().underscorify(),
	    compiler.has_header(header))
endforeach

# sys/sysctl.h on OpenBSD 4.2 requires sys/param.h
# sys/sysctl.h on FreeBSD requires sys/types.h
have_sys_param_h = compiler.has_header('sys/param.h')
conf.set('HAVE_SYS_PARAM_H', have_sys_param_h)
prefix = '''
#include <sys/types.h>
'''
if have_sys_param_h
	prefix += '''
#include <sys/param.h>
'''
endif
conf.set('HAVE_SYS_SYSCTL_H',
    compiler.has_header('sys/sysctl.h', prefix : prefix))
conf.set('HAVE_SYS_SOCKET_H',
    compiler.has_header('sys/socket.h'))
#AC_VAR_TIMEZONE_EXTERNALS

#######################################################################
# Disable pixmap installation
#######################################################################
INSTALL_PIXMAPS = get_option('pixmaps-install')

# check for gtk-doc
ENABLE_DOC = get_option('doc')

enable_debug = get_option('console-logging')
conf.set('DEBUG', enable_debug)

# So that config.h may be found.
toplevel_inc = include_directories('.')

subdir('libpurple')
subdir('share/sounds')
subdir('finch')
subdir('pidgin')
subdir('doc')

configure_file(output : 'config.h',
    configuration : conf)

message('')
message('pidgin ' + meson.project_version())

message('')
message('Build GTK+ UI................. : ' + get_option('gtkui').to_string())
message('Build console UI.............. : ' + enable_consoleui.to_string())
message('Build for X11................. : ' + x11.found().to_string())
message('')
message('Enable Gestures............... : ' + enable_gestures.to_string())
message('Protocols to build dynamically : @0@'.format(DYNAMIC_PRPLS))
message('')
message('Build with GStreamer support.. : ' + gstreamer.found().to_string())
message('Build with voice and video.... : ' + enable_vv.to_string())
message('Build with GNU Libidn......... : ' + idn.found().to_string())
message('Build with Nettle support..... : ' + nettle.found().to_string())
message('Build with Cyrus SASL support. : ' + sasl.found().to_string())
message('Use external libzephyr........ : ' + EXTERNAL_LIBZEPHYR.to_string())
if not EXTERNAL_LIBZEPHYR
message('Use kerberos 4 with zephyr.... : ' + kerberos.to_string())
endif
message('Install pixmaps............... : ' + INSTALL_PIXMAPS.to_string())
message('Install translations.......... : ' + INSTALL_I18N.to_string())
message('Has you....................... : yes')
message('')
message('Build Unity integration plugin.: ' + enable_unity.to_string())
message('')
message('Build with KWallet............ : ' + kwallet.found().to_string())
message('Build with Secret Service..... : ' + secretservice.found().to_string())
message('')
message('Build plugins................. : ' + PLUGINS.to_string())
message('Enable Introspection...........: ' + enable_introspection.to_string())
message('')
message('Print debugging messages...... : ' + enable_debug.to_string())
message('Generate documentation........ : ' + ENABLE_DOC.to_string())
message('')
bindir = join_paths(get_option('prefix'), get_option('bindir'))
message('Pidgin will be installed in @0@.'.format(bindir))
if pidginpath.found()
	message('Warning: You have an old copy of Pidgin at @0@.'.format(pidginpath.path()))
endif
if not INSTALL_PIXMAPS
	message('')
	message('Warning: You have disabled the installation of pixmap data, but Pidgin')
	message('still requires installed pixmaps.  Be sure you know what you are doing.')
endif
if not INSTALL_I18N
	message('')
	message('Warning: You have disabled the building and installation of translation')
	message('data.  This will prevent building Pidgin desktop files.')
	message('Be sure you know what you are doing.')
endif
message('')
message('configure complete, now type \'ninja\'')
message('')
