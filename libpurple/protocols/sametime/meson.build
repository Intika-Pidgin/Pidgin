SAMETIMESOURCES = [
	'sametime.c',
	'sametime.h'
]

if DYNAMIC_SAMETIME
	sametime_prpl = shared_library('sametime', SAMETIMESOURCES,
	    c_args : ['-DG_LOG_DOMAIN="sametime"'],
	    dependencies : [meanwhile, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
