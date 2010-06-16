#ifndef MSN_SBCONN_H
#define MSN_SBCONN_H

#include "msg.h"
#include "slplink.h"

#define MSN_SBCONN_MAX_SIZE 1202

void msn_sbconn_msg_ack(MsnMessage *msg, void *data);

void msn_sbconn_msg_nak(MsnMessage *msg, void *data);

void msn_sbconn_send_msg(MsnSlpLink *slplink, MsnMessage *msg);
void msn_sbconn_send_part(MsnSlpLink *slplink, MsnSlpMessagePart *part);

#endif /* MSN_SBCONN_H */
