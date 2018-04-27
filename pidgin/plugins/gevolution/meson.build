#######################################################################
# Check for stuff needed by the Evolution integration plugin.
#######################################################################
EVOLUTION_ADDRESSBOOK = [
    dependency('libebook-1.2', required : force_deps),
    dependency('libedata-book-1.2', required : force_deps),
    dependency('evolution-data-server-1.2', version : '>= 3.6', required : force_deps),
]
enable_gevolution = true
foreach dep : EVOLUTION_ADDRESSBOOK
	enable_gevolution = enable_gevolution and dep.found()
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

if PLUGINS and enable_gevolution
	gmodule = dependency('gmodule-2.0')

	gevolution = library('gevolution', gevolution_SOURCES,
	    dependencies : EVOLUTION_ADDRESSBOOK + [libpurple_dep, libpidgin_dep, glib, gmodule],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
