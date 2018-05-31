# UPDATING VERSION NUMBERS FOR RELEASES
#
# If any code has changed in libgnt:
#   gnt_micro_version += 1
#
# If any functions have been added to libgnt:
#   gnt_micro_version = 0
#   gnt_minor_version += 1
#   gnt_lt_current += 1
#
# If backwards compatibility has been broken in libgnt:
#   gnt_micro_version = 0
#   gnt_minor_version = 0
#   gnt_major_version += 1;
#   gnt_lt_current += 1
#
# gnt_version_suffix should be similar to one of the following:
#   For beta releases:          'beta2'
#   For code under development: 'devel'
#   For production releases:    ''
#

# the last version for Finch 2 was 2.8.10,
# the first version for Finch 3 was 2.9.0
gnt_lt_current = 9
gnt_major_version = 2
gnt_minor_version = 9
gnt_micro_version = 0
gnt_version_suffix = 'devel'
gnt_version = '@0@.@1@.@2@'.format(gnt_major_version,
                                   gnt_minor_version,
                                   gnt_micro_version)
gnt_display_version = '@0@@1@'.format(gnt_version,
                                      gnt_version_suffix)

GNT_LIB_VERSION = '@0@.@1@.@2@'.format(gnt_lt_current - gnt_minor_version,
                                       gnt_minor_version,
                                       gnt_micro_version)

# Always true when built as part of pidgin repo.
PURPLE_AVAILABLE = true

gnt_config = configuration_data()

#######################################################################
# Check for ncurses and other things used by the console UI
#######################################################################
ncurses_available = false
if get_option('consoleui')
	ncurses_available = true
	ncurses_inc = []
	ncurses_libs = [
		compiler.find_library('ncursesw', required : false),
		compiler.find_library('panelw', required : false)
	]
	if not ncurses_libs[0].found() or not ncurses_libs[1].found()
		ncurses_available = false
	endif

	if IS_WIN32
		# FIXME: $host ?
		ncurses_sys_prefix = '/usr/$host/sys-root/mingw'
	else
		ncurses_sys_prefix = '/usr'
	endif

	ncurses_sys_dirs = [ncurses_sys_prefix + '/include/ncursesw',
	                    ncurses_sys_prefix + '/include']

	if ncurses_available
		# Some distros put the headers in ncursesw/, some don't
		found_ncurses_h = false
		foreach location : ncurses_sys_dirs
			f = location + '/ncurses.h'
			if not found_ncurses_h
				if compiler.has_header_symbol(f, 'get_wch',
				       prefix : '#define _XOPEN_SOURCE_EXTENDED')
					if location != '.'
						ncurses_inc += [include_directories(location)]
					endif
					found_ncurses_h = true
				endif
			endif
		endforeach

		if not found_ncurses_h
			ncurses_inc = []
			ncurses_libs = []
			ncurses_available = false
		endif
	else
		# ncursesw was not found. Look for plain old ncurses
		ncurses_libs = [
			compiler.find_library('ncurses', required : false),
			compiler.find_library('panel', required : false)
		]
		ncurses_available = ncurses_libs[0].found() and ncurses_libs[1].found()
		gnt_config.set('NO_WIDECHAR', true)
	endif

	ncurses = declare_dependency(
	    include_directories : ncurses_inc,
	    dependencies : ncurses_libs
	)
endif

libgnt_SOURCES = [
	'gntwidget.c',
	'gntbindable.c',
	'gntbox.c',
	'gntbutton.c',
	'gntcheckbox.c',
	'gntclipboard.c',
	'gntcolors.c',
	'gntcombobox.c',
	'gntentry.c',
	'gntfilesel.c',
	'gntkeys.c',
	'gntlabel.c',
	'gntline.c',
	'gntmenu.c',
	'gntmenuitem.c',
	'gntmenuitemcheck.c',
	'gntprogressbar.c',
	'gntslider.c',
	'gntstyle.c',
	'gnttextview.c',
	'gnttree.c',
	'gntutils.c',
	'gntwindow.c',
	'gntwm.c',
	'gntws.c',
	'gntmain.c'
]

libgnt_headers = [
	'gntwidget.h',
	'gntbindable.h',
	'gntbox.h',
	'gntbutton.h',
	'gntcheckbox.h',
	'gntclipboard.h',
	'gntcolors.h',
	'gntcombobox.h',
	'gntentry.h',
	'gntfilesel.h',
	'gntkeys.h',
	'gntlabel.h',
	'gntline.h',
	'gntmenu.h',
	'gntmenuitem.h',
	'gntmenuitemcheck.h',
	'gntprogressbar.h',
	'gntslider.h',
	'gntstyle.h',
	'gnttextview.h',
	'gnttree.h',
	'gntutils.h',
	'gntwindow.h',
	'gntwm.h',
	'gntws.h',
	'gnt.h'
]

if ncurses_available

# Check for Python headers
python_dep = dependency('python3')
gnt_config.set('USE_PYTHON', python_dep.found())

configure_file(output : 'gntconfig.h',
    configuration : gnt_config)

gmodule = dependency('gmodule-2.0')

install_headers(libgnt_headers, subdir : 'gnt')

if IS_WIN32
	libgnt_SOURCES += windows.compile_resources('libgnt_winres.rc')
endif

libgnt_inc = include_directories('.')
libgnt = library('gnt',
                 libgnt_SOURCES,
                 include_directories : [toplevel_inc],
                 install : true,
                 version : GNT_LIB_VERSION,
                 dependencies : [ncurses, libxml, glib, gobject, gmodule, python_dep])
libgnt_dep = declare_dependency(
    include_directories : [toplevel_inc, libgnt_inc],
    link_with : libgnt,
    dependencies : [ncurses, glib])

pkgconfig.generate(
    name : 'LibGNT',
    description : 'Glib Ncurses Toolkit is a collection of curses-widgets.',
    version : purple_display_version,
    filebase : 'gnt',
    subdirs : 'gnt',
    libraries : [libgnt],
    requires : ['glib-2.0'])

if enable_introspection
	libgnt_gir = gnome.generate_gir(libgnt,
	    sources : libgnt_headers,
	    includes : 'GObject-2.0',
	    namespace : 'Gnt',
	    symbol_prefix : 'gnt',
	    identifier_prefix : 'Gnt',
	    nsversion : '@0@.@1@'.format(gnt_major_version, gnt_minor_version),
	    install : true)
endif

subdir('wms')
subdir('test')

endif  # ncurses_available
