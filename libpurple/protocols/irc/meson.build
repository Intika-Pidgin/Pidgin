IRCSOURCES = [
	'cmds.c',
	'dcc_send.c',
	'irc.c',
	'irc.h',
	'msgs.c',
	'parse.c'
]

if DYNAMIC_IRC
	irc_prpl = shared_library('irc', IRCSOURCES,
	    dependencies : [sasl, libpurple_dep, glib, gio, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
