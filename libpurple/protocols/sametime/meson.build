SAMETIMESOURCES = [
	'sametime.c',
	'sametime.h',
	'im_mime.c',
	'im_mime.h',
]

if DYNAMIC_SAMETIME
	sametime_prpl = shared_library('sametime', SAMETIMESOURCES,
	    c_args : ['-DG_LOG_DOMAIN="sametime"'],
	    dependencies : [meanwhile, gmime, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)

	subdir('tests')
endif
