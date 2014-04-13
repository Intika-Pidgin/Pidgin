/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#ifndef _GGP_IMAGE_H
#define _GGP_IMAGE_H

#include <internal.h>
#include <libgadu.h>

#define GGP_IMAGE_SIZE_MAX 255000
#define GGP_IMAGE_ID_FORMAT "%016" G_GINT64_MODIFIER "x"

typedef struct _ggp_image_session_data ggp_image_session_data;

typedef enum
{
	GGP_IMAGE_PREPARE_OK = 0,
	GGP_IMAGE_PREPARE_FAILURE,
	GGP_IMAGE_PREPARE_TOO_BIG
} ggp_image_prepare_result;

void
ggp_image_setup(PurpleConnection *gc);

void
ggp_image_cleanup(PurpleConnection *gc);

ggp_image_prepare_result
ggp_image_prepare(PurpleConversation *conv, PurpleImage *image, uint64_t *id);

void
ggp_image_recv(PurpleConnection *gc,
	const struct gg_event_image_reply *image_reply);

void
ggp_image_send(PurpleConnection *gc,
	const struct gg_event_image_request *image_request);

PurpleImage *
ggp_image_request(PurpleConnection *gc, uin_t uin, uint64_t id);

#endif /* _GGP_IMAGE_H */
