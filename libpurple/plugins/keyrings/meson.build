if PLUGINS
	if enable_nettle
		internalkeyring_plugin = library('internalkeyring', 'internalkeyring.c',
		    dependencies : [nettle, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if enable_secret_service
		secretsservice_plugin = library('secretservice', 'secretservice.c',
		    dependencies : [secretservice, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if IS_WIN32
		wincred_plugin = library('wincred', 'wincred.c',
		    dependencies : [libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if enable_kwallet
		kwallet_moc = qt5.preprocess(
			moc_headers: 'purplekwallet.h',
			dependencies: qt5_dep,
		)

		kwallet_plugin = library('kwallet', 'kwallet.cpp', 'purplekwallet.h', kwallet_moc,
		    dependencies : [kwallet, qt5_dep, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif
endif  # PLUGINS
