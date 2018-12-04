enable_consoleui = false
if get_option('consoleui')
	libgnt_dep = dependency('gnt', version : '>= 2.9.0', required : false)
	if libgnt_dep.found()
		libgnt_gir = ['Gnt-2.9']
	else
		libgnt_proj = subproject('libgnt')
		libgnt_dep = libgnt_proj.get_variable('libgnt_dep')
		libgnt_gir = libgnt_proj.get_variable('libgnt_gir')
	endif

	#######################################################################
	# Check for ncurses and other things used by it
	# FIXME: This should be temporary until libgnt wraps the functionality.
	#######################################################################
	ncurses_available = true
	ncurses_header = 'ncurses.h'
	# Some distros put the headers in ncursesw/, some don't. These are ordered to
	# pick the last available as most-specific version.
	ncursesw_header_paths = ['', 'ncursesw/']

	ncurses = dependency('ncursesw', required : false)
	if ncurses.found()
		foreach location : ncursesw_header_paths
			f = location + 'ncurses.h'
			if compiler.has_header_symbol(f, 'get_wch',
			    prefix : '#define _XOPEN_SOURCE_EXTENDED')
				ncurses_header = f
			endif
		endforeach
	else
		ncurses_available = false
		ncurses_inc = []
		ncurses_libs = compiler.find_library('ncursesw', required : false)
		if ncurses_libs.found()
			foreach location : ncursesw_header_paths
				f = location + 'ncurses.h'
				if compiler.has_header_symbol(f, 'get_wch',
				    prefix : '#define _XOPEN_SOURCE_EXTENDED')
					ncurses_available = true
					ncurses_header = f
				endif
			endforeach

			if ncurses_available
				ncurses = declare_dependency(
				    include_directories : ncurses_inc,
				    dependencies : ncurses_libs
				)
			endif
		endif
	endif

	if not ncurses_available
		# ncursesw was not found. Look for plain old ncurses
		ncurses = dependency('ncurses', required : false)
		if ncurses.found()
			ncurses_available = true
		else
			ncurses_libs = compiler.find_library('ncurses', required : false)
			ncurses_available = ncurses_libs.found()
			ncurses = declare_dependency(dependencies : ncurses_libs)
		endif
	endif

	if not ncurses_available and host_machine.system() == 'windows'
		# Try pdcurses too.
		ncurses_header = 'curses.h'
		ncurses_libs = compiler.find_library('pdcurses', required : false)
		ncurses_available = compiler.has_header(ncurses_header) and ncurses_libs.found()
		ncurses = declare_dependency(dependencies : ncurses_libs)
	endif

	if not ncurses_available
		error('ncurses could not be found!')
	endif

	if libgnt_dep.found() and ncurses_available
		ncurses_header = '-DNCURSES_HEADER="@0@"'.format(ncurses_header)
		enable_consoleui = true
	else
		error('''

Finch will not be built. You need to install libgnt (or its requirements) and its development headers.

''')
	endif
endif


libfinch_SOURCES = [
	'gntaccount.c',
	'gntblist.c',
	'gntconn.c',
	'gntconv.c',
	'gntdebug.c',
	'gntidle.c',
	'gntlog.c',
	'gntmedia.c',
	'gntmenuutil.c',
	'gntnotify.c',
	'gntplugin.c',
	'gntpounce.c',
	'gntprefs.c',
	'gntrequest.c',
	'gntroomlist.c',
	'gntsound.c',
	'gntstatus.c',
	'gntui.c',
	'gntxfer.c',
	package_revision,
	'libfinch.c'
]

libfinch_headers = [
	'gntaccount.h',
	'gntblist.h',
	'gntconn.h',
	'gntconv.h',
	'gntdebug.h',
	'finch.h',
	'gntidle.h',
	'gntlog.h',
	'gntmedia.h',
	'gntmenuutil.h',
	'gntnotify.h',
	'gntplugin.h',
	'gntpounce.h',
	'gntprefs.h',
	'gntrequest.h',
	'gntroomlist.h',
	'gntsound.h',
	'gntstatus.h',
	'gntui.h',
	'gntxfer.h'
]

finch_SOURCES = [
	'finch.c'
]

if IS_WIN32
#	libfinch_la_LIBADD += \
#		-lwinmm

	finch_winres = configure_file(
	    input : 'finch_winres.rc.in',
	    output : 'finch_winres.rc',
	    configuration : version_conf)
	finch_SOURCES += windows.compile_resources(finch_winres)
	libfinch_winres = configure_file(
	    input : 'libfinch_winres.rc.in',
	    output : 'libfinch_winres.rc',
	    configuration : version_conf)
	libfinch_SOURCES += windows.compile_resources(libfinch_winres)
endif

if enable_consoleui
	install_headers(libfinch_headers, subdir : 'finch')

	libfinch_inc = include_directories('.')
	libfinch = shared_library('finch',
	    libfinch_SOURCES,
	    c_args : ['-DSTANDALONE', ncurses_header],
	    include_directories : [toplevel_inc],
	    version : PURPLE_LIB_VERSION,
	    dependencies : [libpurple_dep, libgnt_dep, ncurses, glib],
	    install : true)
	libfinch_dep = declare_dependency(
	    include_directories : [toplevel_inc, libfinch_inc],
	    link_with : libfinch,
	    dependencies : [libpurple_dep, libgnt_dep, glib])

	finch = executable('finch',
	    finch_SOURCES,
	    c_args : '-DSTANDALONE',
	    dependencies : [libpurple_dep, libgnt_dep, libfinch_dep],
	    install : true)

	pkgconfig.generate(
	    name : 'Finch',
	    description : 'Finch is an instant messenger application that uses libpurple for protocol support and ncurses (libgnt) for the UI.',
	    version : purple_display_version,
	    filebase : 'finch',
	    subdirs : 'finch',
	    libraries : [libfinch],
	    requires : ['gnt', 'purple-3'])

	if enable_introspection
		introspection_sources = libfinch_headers

		gnome.generate_gir(libfinch,
		    sources : introspection_sources,
		    includes : [libgnt_gir[0], libpurple_gir[0]],
		    namespace : 'Finch',
		    symbol_prefix : 'finch',
		    identifier_prefix : 'Finch',
		    export_packages : 'finch',
		    nsversion : '@0@.@1@'.format(purple_major_version,
		                                 purple_minor_version),
		    include_directories : [
		    	include_directories(join_paths(gplugin.get_pkgconfig_variable('prefix'), 'share/gir-1.0')),
		    ],
		    install : true)
	endif

	subdir('plugins')
endif  # enable_consoleui
