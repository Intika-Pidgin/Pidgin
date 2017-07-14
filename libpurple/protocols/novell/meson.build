NOVELLSOURCES = [
	'nmfield.h',
	'nmfield.c',
	'nmconn.h',
	'nmconn.c',
	'nmconference.h',
	'nmconference.c',
	'nmcontact.h',
	'nmcontact.c',
	'nmevent.h',
	'nmevent.c',
	'nmmessage.h',
	'nmmessage.c',
	'nmrequest.h',
	'nmrequest.c',
	'nmrtf.h',
	'nmrtf.c',
	'nmuser.h',
	'nmuser.c',
	'nmuserrecord.h',
	'nmuserrecord.c',
	'novell.h',
	'novell.c'
]

if DYNAMIC_NOVELL
	novell_prpl = shared_library('novell', NOVELLSOURCES,
	    dependencies : [libpurple_dep, glib, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
