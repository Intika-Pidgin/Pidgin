if PLUGINS
	if with_x
		gntclipboard = library('gntclipboard', 'gntclipboard.c',
		    dependencies : [x11, libpurple_dep, libfinch_dep, glib],
		    name_prefix : '',
		    install : true, install_dir : FINCH_PLUGINDIR)

		gntgf = library('gntgf', 'gntgf.c',
		    c_args : ncurses_header,
		    dependencies : [x11, libpurple_dep, libfinch_dep, ncurses, glib],
		    name_prefix : '',
		    install : true, install_dir : FINCH_PLUGINDIR)
	endif

	gnthistory = library('gnthistory', 'gnthistory.c',
	    dependencies : [libpurple_dep, libfinch_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : FINCH_PLUGINDIR)

	gntlastlog = library('gntlastlog', 'lastlog.c',
	    dependencies : [libpurple_dep, libfinch_dep, ncurses, glib],
	    name_prefix : '',
	    install : true, install_dir : FINCH_PLUGINDIR)

	gnttinyurl = library('gnttinyurl', 'gnttinyurl.c',
	    dependencies : [libpurple_dep, libfinch_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : FINCH_PLUGINDIR)

	grouping = library('grouping', 'grouping.c',
	    dependencies : [libpurple_dep, libfinch_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : FINCH_PLUGINDIR)
endif  # PLUGINS
