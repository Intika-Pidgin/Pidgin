if PLUGINS
#######################################################################
# Check for stuff needed by the Evolution integration plugin.
#######################################################################
EVOLUTION_ADDRESSBOOK = [
    dependency('libebook-1.2', required : get_option('gevolution')),
    dependency('libedata-book-1.2', required : get_option('gevolution')),
    dependency('evolution-data-server-1.2', version : '>= 3.6', required : get_option('gevolution')),
]
foreach dep : EVOLUTION_ADDRESSBOOK
	if not dep.found()
		EVOLUTION_ADDRESSBOOK += [disabler()]
	endif
endforeach

gevolution_SOURCES = [
	'add_buddy_dialog.c',
	'assoc-buddy.c',
	'gevolution.c',
	'gevolution.h',
	'gevo-util.c',
	'new_person_dialog.c',
	'eds-utils.c'
]

gmodule = dependency('gmodule-2.0', required : get_option('gevolution'))

gevolution = library('gevolution', gevolution_SOURCES,
    dependencies : EVOLUTION_ADDRESSBOOK + [libpurple_dep, libpidgin_dep, glib, gmodule],
    name_prefix : '',
    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
