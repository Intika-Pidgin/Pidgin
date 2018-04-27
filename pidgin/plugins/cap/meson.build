#######################################################################
# Check for libsqlite3 (for the Contact Availability Prediction plugin)
#######################################################################
SQLITE3 = dependency('sqlite3', version : '>= 3.3', required : force_deps)

if PLUGINS and SQLITE3.found()
	cap = library('cap', 'cap.c', 'cap.h', 'cap_statistics.h',
	    dependencies : [SQLITE3, libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
