ICONS = [
	'pidgin'
]

foreach icon : ICONS
	foreach size : [16, 22, 24, 32, 48]
		path = 'hicolor/@0@x@0@/apps'.format(size)
		install_data(join_paths(path, '@0@.png'.format(icon)),
		    install_dir : join_paths(get_option('datadir'), 'icons', path))
	endforeach
	path = 'hicolor/scalable/apps'
	install_data(join_paths(path, '@0@.svg'.format(icon)),
	    install_dir : join_paths(get_option('datadir'), 'icons', path))
endforeach

EXCLUDE_FILES = [
	'16x16/apps/pidgin.png',
	'22x22/apps/pidgin.png',
	'24x24/apps/pidgin.png',
	'32x32/apps/pidgin.png',
	'48x48/apps/pidgin.png',
	'scalable/apps/pidgin.svg',
]

EXCLUDE_DIRS = [
	'16x16/apps/scalable',
	'24x24/apps/scalable',
	'32x32/apps/scalable',
]

install_subdir('hicolor',
    exclude_files : EXCLUDE_FILES,
    exclude_directories : EXCLUDE_DIRS,
    install_dir : get_option('datadir') + '/pidgin/icons')
