/**
 * @file glibcompat.h Compatibility for many glib versions.
 * @ingroup core
 */

/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef PURPLE_GLIBCOMPAT_H
#define PURPLE_GLIBCOMPAT_H

#include <glib.h>

#if !GLIB_CHECK_VERSION(2,32,0)
# define G_GNUC_BEGIN_IGNORE_DEPRECATIONS
# define G_GNUC_END_IGNORE_DEPRECATIONS
#endif /* !GLIB_CHECK_VERSION(2,32,0) */

#ifdef __clang__

#undef G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#define G_GNUC_BEGIN_IGNORE_DEPRECATIONS \
	_Pragma ("clang diagnostic push") \
	_Pragma ("clang diagnostic ignored \"-Wdeprecated-declarations\"")

#undef G_GNUC_END_IGNORE_DEPRECATIONS
#define G_GNUC_END_IGNORE_DEPRECATIONS \
	_Pragma ("clang diagnostic pop")

#endif /* __clang__ */

#endif /* PURPLE_GLIBCOMPAT_H */

