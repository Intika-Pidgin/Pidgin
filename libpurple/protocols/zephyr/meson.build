ZEPHYRSOURCES = [
	'ZAsyncLocate.c',
	'ZCkAuth.c',
	'ZCkIfNot.c',
	'ZClosePort.c',
	'ZCmpUID.c',
	'ZCmpUIDP.c',
	'ZFlsLocs.c',
	'ZFlsSubs.c',
	'ZFmtAuth.c',
	'ZFmtList.c',
	'ZFmtNotice.c',
	'ZFmtRaw.c',
	'ZFmtRawLst.c',
	'ZFmtSmRaw.c',
	'ZFreeNot.c',
	'ZGetLocs.c',
	'ZGetSender.c',
	'ZGetSubs.c',
	'ZGetWGPort.c',
	'ZIfNotice.c',
	'ZInit.c',
	'ZLocations.c',
	'ZMakeAscii.c',
	'ZMkAuth.c',
	'ZNewLocU.c',
	'ZOpenPort.c',
	'ZParseNot.c',
	'ZPeekNot.c',
	'ZPeekPkt.c',
	'ZPending.c',
	'ZReadAscii.c',
	'ZRecvNot.c',
	'ZRecvPkt.c',
	'ZRetSubs.c',
	'ZSendList.c',
	'ZSendNot.c',
	'ZSendPkt.c',
	'ZSetDest.c',
	'ZSetFD.c',
	'ZSetSrv.c',
	'ZSubs.c',
	'ZVariables.c',
	'ZWait4Not.c',
	'ZhmStat.c',
	'Zinternal.c',
	'com_err.h',
	'error_message.c',
	'error_table.h',
	'et_name.c',
	'init_et.c',
	'internal.h',
	'mit-copyright.h',
	'mit-sipb-copyright.h',
	'sysdep.h',
	'zephyr_err.c',
	'zephyr_err.h',
	'zephyr_internal.h',
	'zephyr.c',
	'zephyr.h'
]

ZEPHYRSOURCESEXT = ['zephyr.c', 'zephyr.h']

extdep = krb4
if EXTERNAL_LIBZEPHYR
	ZEPHYRSOURCES = ZEPHYRSOURCESEXT
	extdep = ext_zephyr
endif

if DYNAMIC_ZEPHYR
	zephyr_prpl = shared_library('zephyr', ZEPHYRSOURCES,
	    c_args : '-Dlint',
	    dependencies : [extdep, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
