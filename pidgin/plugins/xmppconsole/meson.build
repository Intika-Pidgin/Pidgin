if PLUGINS
	xmppconsole_resource = gnome.compile_resources('xmppconsoleresources',
	    'xmppconsole.gresource.xml',
	    c_name : 'xmppconsole')

	xmppconsole = library('xmppconsole',
	    'xmppconsole.c', xmppconsole_resource,
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif # PLUGINS
