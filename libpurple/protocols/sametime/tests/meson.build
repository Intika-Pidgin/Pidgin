foreach prog : ['im_mime']
	e = executable(
	    'test_sametime_' + prog, 'test_sametime_@0@.c'.format(prog),
	    link_with : [sametime_prpl],
	    dependencies : [libpurple_dep, gmime, glib])

	test('sametime_' + prog, e,
	    env : 'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()))
endforeach
