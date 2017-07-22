#ifndef _SAMETIME_H_
#define _SAMETIME_H_

#include <gmodule.h>

#define MW_TYPE_PROTOCOL             (mw_protocol_get_type())
#define MW_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_PROTOCOL, mwProtocol))
#define MW_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_PROTOCOL, mwProtocolClass))
#define MW_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_PROTOCOL))
#define MW_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_PROTOCOL))
#define MW_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_PROTOCOL, mwProtocolClass))

/* CFLAGS trumps configure values */


/** default host for the purple plugin. You can specialize a build to
    default to your server by supplying this at compile time */
#ifndef MW_PLUGIN_DEFAULT_HOST
#define MW_PLUGIN_DEFAULT_HOST  ""
#endif
/* "" */


/** default port for the purple plugin. You can specialize a build to
    default to your server by supplying this at compile time */
#ifndef MW_PLUGIN_DEFAULT_PORT
#define MW_PLUGIN_DEFAULT_PORT  1533
#endif
/* 1533 */


/** default encoding for the purple plugin.*/
#ifndef MW_PLUGIN_DEFAULT_ENCODING
#define MW_PLUGIN_DEFAULT_ENCODING "ISO-8859-1"
#endif
/* ISO-8859-1 */


typedef struct _mwProtocol
{
	PurpleProtocol parent;
} mwProtocol;

typedef struct _mwProtocolClass
{
	PurpleProtocolClass parent_class;
} mwProtocolClass;


/**
 * Returns the GType for the mwProtocol object.
 */
G_MODULE_EXPORT GType mw_protocol_get_type(void);

#endif /* _SAMETIME_H_ */
