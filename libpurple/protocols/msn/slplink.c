/**
 * @file slplink.c MSNSLP Link support
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "debug.h"

#include "msn.h"
#include "slplink.h"
#include "slpmsg_part.h"

#include "sbconn.h"
#include "switchboard.h"
#include "slp.h"
#include "p2p.h"

#ifdef MSN_DEBUG_SLP_FILES
static int m_sc = 0;
static int m_rc = 0;

static void
debug_part_to_file(MsnSlpMessage *msg, gboolean send)
{
	char *tmp;
	char *dir;
	char *data;
	int c;
	gsize data_size;

	dir = send ? "send" : "recv";
	c = send ? m_sc++ : m_rc++;
	tmp = g_strdup_printf("%s/msntest/%s/%03d", g_get_home_dir(), dir, c);
	data = msn_slpmsg_serialize(msg, &data_size);
	if (!purple_util_write_data_to_file_absolute(tmp, data, data_size))
	{
		purple_debug_error("msn", "could not save debug file\n");
	}
	g_free(tmp);
}
#endif

/**************************************************************************
 * Main
 **************************************************************************/

static MsnSlpLink *
msn_slplink_new(MsnSession *session, const char *username)
{
	MsnSlpLink *slplink;

	g_return_val_if_fail(session != NULL, NULL);

	slplink = g_new0(MsnSlpLink, 1);

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slplink_new: slplink(%p)\n", slplink);

	slplink->session = session;
	slplink->slp_seq_id = rand() % 0xFFFFFF00 + 4;

	slplink->remote_user = g_strdup(username);

	slplink->slp_msg_queue = g_queue_new();

	session->slplinks =
		g_list_append(session->slplinks, slplink);

	return msn_slplink_ref(slplink);
}

void
msn_slplink_destroy(MsnSlpLink *slplink)
{
	MsnSession *session;

	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slplink_destroy: slplink(%p)\n", slplink);

	g_return_if_fail(slplink != NULL);

	if (slplink->swboard != NULL) {
		slplink->swboard->slplinks = g_list_remove(slplink->swboard->slplinks, slplink);
		slplink->swboard = NULL;
	}

	if (slplink->refs > 1) {
		slplink->refs--;
		return;
	}

	session = slplink->session;

	if (slplink->dc != NULL) {
		slplink->dc->slplink = NULL;
		msn_dc_destroy(slplink->dc);
		slplink->dc = NULL;
	}

	while (slplink->slp_calls != NULL)
		msn_slpcall_destroy(slplink->slp_calls->data);

	g_queue_free(slplink->slp_msg_queue);

	session->slplinks =
		g_list_remove(session->slplinks, slplink);

	g_free(slplink->remote_user);

	g_free(slplink);
}

MsnSlpLink *
msn_slplink_ref(MsnSlpLink *slplink)
{
	g_return_val_if_fail(slplink != NULL, NULL);

	slplink->refs++;
	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slplink ref (%p)[%d]\n", slplink, slplink->refs);

	return slplink;
}

void
msn_slplink_unref(MsnSlpLink *slplink)
{
	g_return_if_fail(slplink != NULL);

	slplink->refs--;
	if (purple_debug_is_verbose())
		purple_debug_info("msn", "slplink unref (%p)[%d]\n", slplink, slplink->refs);

	if (slplink->refs == 0)
		msn_slplink_destroy(slplink);
}

MsnSlpLink *
msn_session_find_slplink(MsnSession *session, const char *who)
{
	GList *l;

	for (l = session->slplinks; l != NULL; l = l->next)
	{
		MsnSlpLink *slplink;

		slplink = l->data;

		if (!strcmp(slplink->remote_user, who))
			return slplink;
	}

	return NULL;
}

MsnSlpLink *
msn_session_get_slplink(MsnSession *session, const char *username)
{
	MsnSlpLink *slplink;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(username != NULL, NULL);

	slplink = msn_session_find_slplink(session, username);

	if (slplink == NULL)
		slplink = msn_slplink_new(session, username);

	return slplink;
}

void
msn_slplink_add_slpcall(MsnSlpLink *slplink, MsnSlpCall *slpcall)
{
	if (slplink->swboard != NULL)
		slplink->swboard->flag |= MSN_SB_FLAG_FT;

	slplink->slp_calls = g_list_append(slplink->slp_calls, slpcall);

	/*
	if (slplink->dc != NULL && slplink->dc->state == DC_STATE_ESTABLISHED)
		msn_dc_ref(slplink->dc);
	*/
}

void
msn_slplink_remove_slpcall(MsnSlpLink *slplink, MsnSlpCall *slpcall)
{
	/*
	if (slplink->dc != NULL && slplink->dc->state == DC_STATE_ESTABLISHED)
		msn_dc_unref(slplink->dc);
	*/

	slplink->slp_calls = g_list_remove(slplink->slp_calls, slpcall);

	/* The slplink has no slpcalls in it, release it from MSN_SB_FLAG_FT.
	 * If nothing else is using it then this might cause swboard to be
	 * destroyed. */
	if (slplink->slp_calls == NULL && slplink->swboard != NULL) {
		slplink->swboard->slplinks = g_list_remove(slplink->swboard->slplinks, slplink);
		msn_switchboard_release(slplink->swboard, MSN_SB_FLAG_FT);
		slplink->swboard = NULL;
	}

	/* The slplink has no slpcalls in it, release it from the DC. */
	if (slplink->slp_calls == NULL && slplink->dc != NULL) {
		slplink->dc->slplink = NULL;
		msn_dc_destroy(slplink->dc);
		slplink->dc = NULL;
	}
}

MsnSlpCall *
msn_slplink_find_slp_call(MsnSlpLink *slplink, const char *id)
{
	GList *l;
	MsnSlpCall *slpcall;

	if (!id)
		return NULL;

	for (l = slplink->slp_calls; l != NULL; l = l->next)
	{
		slpcall = l->data;

		if (slpcall->id && !strcmp(slpcall->id, id))
			return slpcall;
	}

	return NULL;
}

MsnSlpCall *
msn_slplink_find_slp_call_with_session_id(MsnSlpLink *slplink, long id)
{
	GList *l;
	MsnSlpCall *slpcall;

	for (l = slplink->slp_calls; l != NULL; l = l->next)
	{
		slpcall = l->data;

		if (slpcall->session_id == id)
			return slpcall;
	}

	return NULL;
}

void
msn_slplink_send_part(MsnSlpLink *slplink, MsnSlpMessagePart *part)
{
	if (slplink->dc != NULL && slplink->dc->state == DC_STATE_ESTABLISHED)
	{
		msn_dc_enqueue_part(slplink->dc, part);
	}
	else
	{
		msn_sbconn_send_part(slplink, part);
	}
}
#if 0
void
msn_slplink_send_msg(MsnSlpLink *slplink, MsnMessage *msg)
{
	if (slplink->dc != NULL && slplink->dc->state == DC_STATE_ESTABLISHED)
	{
		msn_dc_enqueue_msg(slplink->dc, msg);
	}
	else
	{
		msn_sbconn_send_msg(slplink, msg);
	}
}
#endif

void
msn_slplink_send_msgpart(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	MsnSlpMessagePart *part;
	long long real_size;
	size_t len = 0;

	/* Maybe we will want to create a new msg for this slpmsg instead of
	 * reusing the same one all the time. */
	part = msn_slpmsgpart_new(slpmsg->header, slpmsg->footer);
	part->ack_data = slpmsg;

	real_size = (slpmsg->flags == P2P_ACK) ? 0 : slpmsg->size;

	if (slpmsg->offset < real_size)
	{
		if (slpmsg->slpcall && slpmsg->slpcall->xfer && purple_xfer_get_type(slpmsg->slpcall->xfer) == PURPLE_XFER_SEND &&
				purple_xfer_get_status(slpmsg->slpcall->xfer) == PURPLE_XFER_STATUS_STARTED)
		{
			len = MIN(MSN_SBCONN_MAX_SIZE, slpmsg->slpcall->u.outgoing.len);
			msn_slpmsgpart_set_bin_data(part, slpmsg->slpcall->u.outgoing.data, len);
		}
		else
		{
			len = slpmsg->size - slpmsg->offset;

			if (len > MSN_SBCONN_MAX_SIZE)
				len = MSN_SBCONN_MAX_SIZE;

			msn_slpmsgpart_set_bin_data(part, slpmsg->slpcall->u.outgoing.data, len);
		}

		slpmsg->header->offset = slpmsg->offset;
		slpmsg->header->length = len;
	}

#if 0
	/* TODO: port this function to SlpMessageParts */
	if (purple_debug_is_verbose())
		msn_message_show_readable(msg, slpmsg->info, slpmsg->text_body);
#endif

#ifdef MSN_DEBUG_SLP_FILES
	debug_part_to_file(slpmsg, TRUE);
#endif

	slpmsg->parts = g_list_append(slpmsg->parts, part);
	msn_slplink_send_part(slplink, part);

	if ((slpmsg->flags == P2P_MSN_OBJ_DATA || 
	     slpmsg->flags == (P2P_WML2009_COMP | P2P_MSN_OBJ_DATA) ||
	     slpmsg->flags == P2P_FILE_DATA) &&
		(slpmsg->slpcall != NULL))
	{
		slpmsg->slpcall->progress = TRUE;

		if (slpmsg->slpcall->progress_cb != NULL)
		{
			slpmsg->slpcall->progress_cb(slpmsg->slpcall, slpmsg->size,
										 len, slpmsg->offset);
		}
	}

	/* slpmsg->offset += len; */
}

static void
msn_slplink_release_slpmsg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	slpmsg = slpmsg;
	slpmsg->header = g_new0(MsnP2PHeader, 1);
	slpmsg->footer = g_new0(MsnP2PFooter, 1);

	if (slpmsg->flags == P2P_NO_FLAG)
	{
		slpmsg->header->session_id = slpmsg->session_id;
		slpmsg->header->ack_id = rand() % 0xFFFFFF00;
	}
	else if (slpmsg->flags == P2P_ACK)
	{
		slpmsg->header->session_id = slpmsg->session_id;
		slpmsg->header->ack_id = slpmsg->ack_id;
		slpmsg->header->ack_size = slpmsg->ack_size;
		slpmsg->header->ack_sub_id = slpmsg->ack_sub_id;
	}
	else if (slpmsg->flags == P2P_MSN_OBJ_DATA ||
	         slpmsg->flags == (P2P_WML2009_COMP | P2P_MSN_OBJ_DATA) ||
	         slpmsg->flags == P2P_FILE_DATA)
	{
		MsnSlpCall *slpcall;
		slpcall = slpmsg->slpcall;

		g_return_if_fail(slpcall != NULL);
		slpmsg->header->session_id = slpcall->session_id;
		slpmsg->footer->value = slpcall->app_id;
		slpmsg->header->ack_id = rand() % 0xFFFFFF00;
	}
	else if (slpmsg->flags == 0x100)
	{
		slpmsg->header->ack_id     = slpmsg->ack_id;
		slpmsg->header->ack_sub_id = slpmsg->ack_sub_id;
		slpmsg->header->ack_size   = slpmsg->ack_size;
	}

	slpmsg->header->id = slpmsg->id;
	slpmsg->header->flags = (guint32)slpmsg->flags;

	slpmsg->header->total_size = slpmsg->size;

	msn_slplink_send_msgpart(slplink, slpmsg);

#if 0
	passport = purple_normalize(slplink->session->account, slplink->remote_user);
	msn_message_set_header(msg, "P2P-Dest", passport);

	msg->ack_cb = msn_sbconn_msg_ack;
	msg->nak_cb = msn_sbconn_msg_nak;
	msg->ack_data = slpmsg;

	msn_slplink_send_msgpart(slplink, slpmsg);

	msn_message_destroy(msg);
#endif
}

void
msn_slplink_queue_slpmsg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	g_return_if_fail(slpmsg != NULL);

	slpmsg->id = slplink->slp_seq_id++;

	g_queue_push_tail(slplink->slp_msg_queue, slpmsg);
}

void
msn_slplink_send_slpmsg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	slpmsg->id = slplink->slp_seq_id++;

	msn_slplink_release_slpmsg(slplink, slpmsg);
}

void
msn_slplink_send_queued_slpmsgs(MsnSlpLink *slplink)
{
	MsnSlpMessage *slpmsg;

	/* Send the queued msgs in the order they were created */
	while ((slpmsg = g_queue_pop_head(slplink->slp_msg_queue)) != NULL)
	{
		msn_slplink_release_slpmsg(slplink, slpmsg);
	}
}

static MsnSlpMessage *
msn_slplink_create_ack(MsnSlpLink *slplink, MsnP2PHeader *header)
{
	MsnSlpMessage *slpmsg;

	slpmsg = msn_slpmsg_ack_new(header);
	msn_slpmsg_set_slplink(slpmsg, slplink);

	return slpmsg;
}

static void
msn_slplink_send_ack(MsnSlpLink *slplink, MsnP2PHeader *header)
{
	MsnSlpMessage *slpmsg = msn_slplink_create_ack(slplink, header);

	msn_slplink_send_slpmsg(slplink, slpmsg);
	msn_slpmsg_destroy(slpmsg);
}

static void
send_file_cb(MsnSlpCall *slpcall)
{
	MsnSlpMessage *slpmsg;
	PurpleXfer *xfer;

	xfer = (PurpleXfer *)slpcall->xfer;
	if (purple_xfer_get_status(xfer) >= PURPLE_XFER_STATUS_STARTED)
		return;

	purple_xfer_ref(xfer);
	purple_xfer_start(xfer, -1, NULL, 0);
	if (purple_xfer_get_status(xfer) != PURPLE_XFER_STATUS_STARTED) {
		purple_xfer_unref(xfer);
		return;
	}
	purple_xfer_unref(xfer);

	slpmsg = msn_slpmsg_file_new(slpcall, purple_xfer_get_size(xfer));
	msn_slpmsg_set_slplink(slpmsg, slpcall->slplink);

	msn_slplink_send_slpmsg(slpcall->slplink, slpmsg);
}

static MsnSlpMessage *
msn_slplink_message_find(MsnSlpLink *slplink, long session_id, long id)
{
	GList *e;

	for (e = slplink->slp_msgs; e != NULL; e = e->next)
	{
		MsnSlpMessage *slpmsg = e->data;

		if ((slpmsg->session_id == session_id) && (slpmsg->id == id))
			return slpmsg;
	}

	return NULL;
}

void
msn_slplink_process_msg(MsnSlpLink *slplink, MsnP2PHeader *header, const char *data, gsize len)
{
	MsnSlpMessage *slpmsg;
	guint64 offset;

	if (header->total_size < header->length)
	{
		/* We seem to have received a bad header */
		purple_debug_warning("msn", "Total size listed in SLP binary header "
				"was less than length of this particular message.  This "
				"should not happen.  Dropping message.\n");
		return;
	}

	offset = header->offset;

	if (offset == 0)
	{
		slpmsg = msn_slpmsg_new(slplink);
		slpmsg->id = header->id;
		slpmsg->session_id = header->session_id;
		slpmsg->size = header->total_size;
		slpmsg->flags = header->flags;

		if (slpmsg->session_id)
		{
			slpmsg->slpcall = msn_slplink_find_slp_call_with_session_id(slplink, slpmsg->session_id);
			if (slpmsg->slpcall != NULL)
			{
				if (slpmsg->flags == P2P_MSN_OBJ_DATA ||
					slpmsg->flags == (P2P_WML2009_COMP | P2P_MSN_OBJ_DATA) ||
					slpmsg->flags == P2P_FILE_DATA)
				{
					PurpleXfer *xfer = slpmsg->slpcall->xfer;
					if (xfer != NULL)
					{
						slpmsg->ft = TRUE;
						slpmsg->slpcall->xfer_msg = slpmsg;

						purple_xfer_ref(xfer);
						purple_xfer_start(xfer,	-1, NULL, 0);

						if (xfer->data == NULL) {
							purple_xfer_unref(xfer);
							msn_slpmsg_destroy(slpmsg);
							g_return_if_reached();
						} else {
							purple_xfer_unref(xfer);
						}
					}
				}
			}
		}
		if (!slpmsg->ft && slpmsg->size)
		{
			slpmsg->buffer = g_try_malloc(slpmsg->size);
			if (slpmsg->buffer == NULL)
			{
				purple_debug_error("msn", "Failed to allocate buffer for slpmsg\n");
				msn_slpmsg_destroy(slpmsg);
				return;
			}
		}
	}
	else
	{
		slpmsg = msn_slplink_message_find(slplink, header->session_id, header->id);
		if (slpmsg == NULL)
		{
			/* Probably the transfer was canceled */
			purple_debug_error("msn", "Couldn't find slpmsg\n");
			return;
		}
	}

	if (slpmsg->ft)
	{
		slpmsg->slpcall->u.incoming_data =
				g_byte_array_append(slpmsg->slpcall->u.incoming_data, (const guchar *)data, len);
		purple_xfer_prpl_ready(slpmsg->slpcall->xfer);
	}
	else if (slpmsg->size && slpmsg->buffer)
	{
		if (G_MAXSIZE - len < offset || (offset + len) > slpmsg->size || slpmsg->offset != offset)
		{
			purple_debug_error("msn",
				"Oversized slpmsg - msgsize=%lld offset=%" G_GUINT64_FORMAT " len=%" G_GSIZE_FORMAT "\n",
				slpmsg->size, offset, len);
			g_return_if_reached();
		} else {
			memcpy(slpmsg->buffer + offset, data, len);
			slpmsg->offset += len;
		}
	}

	if ((slpmsg->flags == P2P_MSN_OBJ_DATA ||
		slpmsg->flags == (P2P_WML2009_COMP | P2P_MSN_OBJ_DATA) ||
		slpmsg->flags == P2P_FILE_DATA) &&
		(slpmsg->slpcall != NULL))
	{
		slpmsg->slpcall->progress = TRUE;

		if (slpmsg->slpcall->progress_cb != NULL)
		{
			slpmsg->slpcall->progress_cb(slpmsg->slpcall, slpmsg->size,
										 len, offset);
		}
	}

#if 0
	if (slpmsg->buffer == NULL)
		return;
#endif

	if (header->offset + header->length >= header->total_size)
	{
		/* All the pieces of the slpmsg have been received */
		MsnSlpCall *slpcall;

		slpcall = msn_slp_process_msg(slplink, slpmsg);

		if (slpcall == NULL) {
			msn_slpmsg_destroy(slpmsg);
			return;
		}

		purple_debug_info("msn", "msn_slplink_process_msg: slpmsg complete\n");

		if (/* !slpcall->wasted && */ slpmsg->flags == 0x100)
		{
#if 0
			MsnDirectConn *directconn;

			directconn = slplink->directconn;
			if (!directconn->acked)
				msn_directconn_send_handshake(directconn);
#endif
		}
		else if (slpmsg->flags == P2P_NO_FLAG || slpmsg->flags == P2P_WML2009_COMP ||
			slpmsg->flags == P2P_MSN_OBJ_DATA ||
			slpmsg->flags == (P2P_WML2009_COMP | P2P_MSN_OBJ_DATA) ||
			slpmsg->flags == P2P_FILE_DATA)
		{
			/* Release all the messages and send the ACK */

			if (slpcall->wait_for_socket) {
				/*
				 * Save ack for later because we have to send
				 * a 200 OK message to the previous direct connect
				 * invitation before ACK but the listening socket isn't
				 * created yet.
				 */
				purple_debug_info("msn", "msn_slplink_process_msg: save ACK\n");

				slpcall->slplink->dc->prev_ack = msn_slplink_create_ack(slplink, header);
			} else if (!slpcall->wasted) {
				purple_debug_info("msn", "msn_slplink_process_msg: send ACK\n");

				msn_slplink_send_ack(slplink, header);
				msn_slplink_send_queued_slpmsgs(slplink);
			}
		}

		msn_slpmsg_destroy(slpmsg);

		if (!slpcall->wait_for_socket && slpcall->wasted)
			msn_slpcall_destroy(slpcall);
	}
}

static gchar *
gen_context(PurpleXfer *xfer, const char *file_name, const char *file_path)
{
	gsize size = 0;
	MsnFileContext *header;
	gchar *u8 = NULL;
	gchar *ret;
	gunichar2 *uni = NULL;
	glong currentChar = 0;
	glong len = 0;
	const char *preview;
	gsize preview_len;

	size = purple_xfer_get_size(xfer);

	purple_xfer_prepare_thumbnail(xfer, "png");

	if (!file_name) {
		gchar *basename = g_path_get_basename(file_path);
		u8 = purple_utf8_try_convert(basename);
		g_free(basename);
		file_name = u8;
	}

	uni = g_utf8_to_utf16(file_name, -1, NULL, &len, NULL);

	if (u8) {
		g_free(u8);
		file_name = NULL;
		u8 = NULL;
	}

	preview = purple_xfer_get_thumbnail(xfer, &preview_len);
	header = g_malloc(sizeof(MsnFileContext) + preview_len);

	header->length = GUINT32_TO_LE(sizeof(MsnFileContext) - 1);
	header->version = GUINT32_TO_LE(2); /* V.3 contains additional unnecessary data */
	header->file_size = GUINT64_TO_LE(size);
	if (preview)
		header->type = GUINT32_TO_LE(0);
	else
		header->type = GUINT32_TO_LE(1);

	len = MIN(len, MAX_FILE_NAME_LEN);
	for (currentChar = 0; currentChar < len; currentChar++) {
		header->file_name[currentChar] = GUINT16_TO_LE(uni[currentChar]);
	}
	memset(&header->file_name[currentChar], 0x00, (MAX_FILE_NAME_LEN - currentChar) * 2);

	memset(&header->unknown1, 0, sizeof(header->unknown1));
	header->unknown2 = GUINT32_TO_LE(0xffffffff);
	if (preview) {
		memcpy(&header->preview, preview, preview_len);
	}
	header->preview[preview_len] = '\0';

	g_free(uni);
	ret = purple_base64_encode((const guchar *)header, sizeof(MsnFileContext) + preview_len);
	g_free(header);
	return ret;
}

void
msn_slplink_request_ft(MsnSlpLink *slplink, PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	char *context;
	const char *fn;
	const char *fp;

	fn = purple_xfer_get_filename(xfer);
	fp = purple_xfer_get_local_filename(xfer);

	g_return_if_fail(slplink != NULL);
	g_return_if_fail(fp != NULL);

	slpcall = msn_slpcall_new(slplink);
	msn_slpcall_init(slpcall, MSN_SLPCALL_DC);

	slpcall->session_init_cb = send_file_cb;
	slpcall->end_cb = msn_xfer_end_cb;
	slpcall->cb = msn_xfer_completed_cb;
	slpcall->xfer = xfer;
	purple_xfer_ref(slpcall->xfer);

	slpcall->pending = TRUE;

	purple_xfer_set_cancel_send_fnc(xfer, msn_xfer_cancel);
	purple_xfer_set_read_fnc(xfer, msn_xfer_read);
	purple_xfer_set_write_fnc(xfer, msn_xfer_write);

	xfer->data = slpcall;

	context = gen_context(xfer, fn, fp);

	msn_slpcall_invite(slpcall, MSN_FT_GUID, 2, context);

	g_free(context);
}

void
msn_slplink_request_object(MsnSlpLink *slplink,
						   const char *info,
						   MsnSlpCb cb,
						   MsnSlpEndCb end_cb,
						   const MsnObject *obj)
{
	MsnSlpCall *slpcall;
	char *msnobj_data;
	char *msnobj_base64;

	g_return_if_fail(slplink != NULL);
	g_return_if_fail(obj     != NULL);

	msnobj_data = msn_object_to_string(obj);
	msnobj_base64 = purple_base64_encode((const guchar *)msnobj_data, strlen(msnobj_data));
	g_free(msnobj_data);

	slpcall = msn_slpcall_new(slplink);
	msn_slpcall_init(slpcall, MSN_SLPCALL_ANY);

	slpcall->data_info = g_strdup(info);
	slpcall->cb = cb;
	slpcall->end_cb = end_cb;

	msn_slpcall_invite(slpcall, MSN_OBJ_GUID, 1, msnobj_base64);

	g_free(msnobj_base64);
}
