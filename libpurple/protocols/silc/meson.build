SILCSOURCES = [
	'buddy.c',
	'chat.c',
	'ft.c',
	'ops.c',
	'pk.c',
	'silc.c',
	'silcpurple.h',
	'util.c',
	'wb.c',
	'wb.h'
]

if DYNAMIC_SILC
	silc_prpl = shared_library('silcpurple', SILCSOURCES,
	    dependencies : [silc, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
