SIMPLESOURCES = [
	'ntlm.c',
	'ntlm.h',
	'simple.c',
	'simple.h',
	'sipmsg.c',
	'sipmsg.h'
]

if DYNAMIC_SIMPLE
	simple_prpl = shared_library('simple', SIMPLESOURCES,
	    dependencies : [libpurple_dep, nettle, glib, gio, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
