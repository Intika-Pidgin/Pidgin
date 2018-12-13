purple_coresources = [
	'account.c',
	'accounts.c',
	'accountopt.c',
	'attention.c',
	'blistnode.c',
	'buddy.c',
	'buddylist.c',
	'buddyicon.c',
	'chat.c',
	'circularbuffer.c',
	'cmds.c',
	'connection.c',
	'contact.c',
	'conversation.c',
	'conversationtypes.c',
	'conversations.c',
	'core.c',
	'countingnode.c',
	'debug.c',
	'e2ee.c',
	'eventloop.c',
	'group.c',
	'http.c',
	'idle.c',
	'image.c',
	'image-store.c',
	'keyring.c',
	'log.c',
	'media/backend-iface.c',
	'media/candidate.c',
	'media/codec.c',
	'media/enum-types.c',
	'media.c',
	'mediamanager.c',
	'memorypool.c',
	'message.c',
	'mime.c',
	'nat-pmp.c',
	'network.c',
	'notify.c',
	'options.c',
	'plugins.c',
	'pluginpref.c',
	'pounce.c',
	'prefs.c',
	'presence.c',
	'proxy.c',
	'protocol.c',
	'protocols.c',
	'purple-gio.c',
	'queuedoutputstream.c',
	'request.c',
	'request-datasheet.c',
	'roomlist.c',
	'savedstatuses.c',
	'server.c',
	'signals.c',
	'smiley-custom.c',
	'smiley-list.c',
	'smiley-parser.c',
	'smiley-theme.c',
	'smiley.c',
	'status.c',
	'stringref.c',
	'stun.c',
	'sound.c',
	'sound-theme.c',
	'sound-theme-loader.c',
	'sslconn.c',
	'theme.c',
	'theme-loader.c',
	'theme-manager.c',
	'trie.c',
	'upnp.c',
	'util.c',
	'version.c',
	'whiteboard.c',
	'xfer.c',
	'xmlnode.c'
]

purple_coreheaders = [
	'account.h',
	'accounts.h',
	'accountopt.h',
	'attention.h',
	'blistnode.h',
	'buddy.h',
	'buddylist.h',
	'buddyicon.h',
	'chat.h',
	'circularbuffer.h',
	'cmds.h',
	'connection.h',
	'contact.h',
	'conversation.h',
	'conversationtypes.h',
	'conversations.h',
	'core.h',
	'countingnode.h',
	'debug.h',
	'e2ee.h',
	'eventloop.h',
	'group.h',
	'http.h',
	'idle.h',
	'image.h',
	'image-store.h',
	'keyring.h',
	'log.h',
	'media.h',
	'mediamanager.h',
	'memorypool.h',
	'message.h',
	'mime.h',
	'nat-pmp.h',
	'network.h',
	'notify.h',
	'options.h',
	'plugins.h',
	'pluginpref.h',
	'pounce.h',
	'prefs.h',
	'presence.h',
	'proxy.h',
	'protocol.h',
	'protocols.h',
	'purple-gio.h',
	'queuedoutputstream.h',
	'request.h',
	'request-datasheet.h',
	'roomlist.h',
	'savedstatuses.h',
	'server.h',
	'signals.h',
	'smiley-custom.h',
	'smiley-list.h',
	'smiley-parser.h',
	'smiley-theme.h',
	'smiley.h',
	'status.h',
	'stringref.h',
	'stun.h',
	'sound.h',
	'sound-theme.h',
	'sound-theme-loader.h',
	'sslconn.h',
	'tests.h',
	'theme.h',
	'theme-loader.h',
	'theme-manager.h',
	'trie.h',
	'upnp.h',
	'util.h',
	'whiteboard.h',
	'xfer.h',
	'xmlnode.h',
]

if enable_gst
	purple_coresources += 'media/backend-fs2.c'
endif
if enable_vv
	purple_coreheaders += 'media-gst.h'
endif

if IS_WIN32
	purple_coresources += [
		'win32/libc_interface.c',
		'win32/win32dep.c'
	]

	purple_coreheaders += [
		'win32/libc_interface.h',
		'win32/libc_internal.h',
		'win32/win32dep.h',
		'win32/wpurpleerror.h'
	]

	libpurplerc = configure_file(
	    input : 'win32/libpurplerc.rc.in',
	    output : 'libpurplerc.rc',
	    configuration : version_conf)
	purple_coresources += windows.compile_resources(libpurplerc)
endif

purple_mediaheaders = [
	'media/backend-iface.h',
	'media/candidate.h',
	'media/codec.h',
	'media/enum-types.h'
]

purple_enumheaders = [
	'account.h',
	'buddyicon.h',
	'connection.h',
	'conversation.h',
	'conversationtypes.h',
	'debug.h',
	'eventloop.h',
	'notify.h',
	'plugins.h',
	'protocol.h',
	'protocols.h',
	'roomlist.h',
	'status.h',
	'sound.h',
	'xfer.h',
	'xmlnode.h'
]


enums = gnome.mkenums('enums',
    sources : purple_enumheaders,
    h_template : 'enums.h.in',
    c_template : 'enums.c.in',
    install_header : true,
    install_dir : get_option('includedir') + '/libpurple')
enums_c = enums[0]
enums_h = enums[1]

purple_h = configure_file(input : 'purple.h.in',
                          output : 'purple.h',
                          configuration : purple_h_conf,
                          install : true,
                          install_dir : get_option('includedir') + '/libpurple')
version_h = configure_file(input : 'version.h.in',
                           output : 'version.h',
                           configuration : version_conf,
                           install : true,
                           install_dir : get_option('includedir') + '/libpurple')

purple_builtsources = [
	enums_c,
]

purple_builtheaders = [
	purple_h,
	version_h,
	enums_h,
]

libpurple_inc = include_directories('.')
libpurple = library('purple',
                    purple_coresources + purple_builtsources +
                    purple_builtheaders,
                    include_directories : [toplevel_inc, libpurple_inc],
                    install : true,
                    version : PURPLE_LIB_VERSION,
                    dependencies : # static_link_libs
                        [dnsapi, ws2_32, glib, gio, gplugin, libxml,
                         farstream, gstreamer, gstreamer_video,
                         gstreamer_app, json, math])

libpurple_dep = declare_dependency(
    # Ensure purple headers built before any dependencies:
    sources : [purple_builtheaders],
    include_directories : [toplevel_inc, libpurple_inc],
    link_with : libpurple,
    dependencies : [gstreamer, gplugin, glib, gio])

install_headers(purple_coreheaders,
                subdir : 'libpurple')

install_headers(purple_mediaheaders,
                subdir : 'libpurple/media')

pkgconfig.generate(
    name : 'libpurple',
    description : 'libpurple is a GLib-based instant messenger library.',
    version : purple_display_version,
    filebase : 'purple-3',
    subdirs : 'libpurple',
    libraries : [libpurple],
    requires : ['glib-2.0'] + GPLUGIN_REQ,
    variables : ['plugindir=${libdir}/purple-@0@'.format(purple_major_version)])

if enable_introspection
	introspection_sources = (purple_coreheaders + purple_builtheaders +
	                         purple_mediaheaders)

	Purple_gir_includes = ['GObject-2.0', 'Gio-2.0']
	if PLUGINS
		Purple_gir_includes += ['GPlugin-0.0']
	endif
	if enable_gst
		Purple_gir_includes += ['Gst-1.0']
	endif

	libpurple_gir = gnome.generate_gir(libpurple,
	    sources : introspection_sources,
	    includes : Purple_gir_includes,
	    namespace : 'Purple',
	    symbol_prefix : 'purple',
	    identifier_prefix : 'Purple',
	    export_packages : 'purple-@0@'.format(purple_major_version),
	    nsversion : '@0@.@1@'.format(purple_major_version,
	                                 purple_minor_version),
	    include_directories : [
	    	include_directories(join_paths(gplugin.get_pkgconfig_variable('prefix'), 'share/gir-1.0')),
	    ],
	    install : true,
	    extra_args : ['--quiet'])
endif

subdir('tests')
subdir('example')
subdir('plugins')
subdir('protocols')
