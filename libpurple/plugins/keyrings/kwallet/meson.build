if PLUGINS
	if kwallet.found()
		kwallet_moc = qt5.preprocess(
			moc_headers: 'purplekwallet.h',
			dependencies: qt5_dep,
		)

		kwallet_plugin = library('purplekwallet', 'purplekwallet.cpp', 'purplekwallet.h', kwallet_moc,
		    dependencies : [kwallet, qt5_dep, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif
endif
