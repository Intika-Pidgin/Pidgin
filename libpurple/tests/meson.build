PROGS = [
    'attention_type',
    'image',
    'protocol_attention',
    'protocol_xfer',
    'smiley',
    'smiley_list',
    'trie',
    'util',
    'xmlnode'
]

test_ui = static_library(
    'test-ui',
    'test_ui.c',
    'test_ui.h',
    c_args: [
        '-DTEST_DATA_DIR="@0@/data"'.format(meson.current_source_dir())
    ],
    dependencies: [libpurple_dep, glib, dbus, dbus_glib]
)

foreach prog : PROGS
    e = executable('test_' + prog, 'test_@0@.c'.format(prog),
                   dbus_builtheaders, dbus_builtsources,
                   c_args : [
                       '-DTEST_DATA_DIR="@0@/data"'.format(meson.current_source_dir())
                   ],
                   dependencies : [libpurple_dep, glib, dbus, dbus_glib],
                   link_with: test_ui,
    )
    test(prog, e)
endforeach
