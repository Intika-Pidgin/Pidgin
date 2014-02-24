/* $Id$ */

/*
 *  (C) Copyright 2011 Bartosz Brachaczek <b.brachaczek@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

/**
 * \file fileio.h
 *
 * \brief Makra zapewniające kompatybilność API do obsługi operacji na plikach na różnych systemach
 */

#ifndef LIBGADU_FILEIO_H
#define LIBGADU_FILEIO_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#  include <io.h>
#  define gg_file_close _close
#  define lseek _lseek
#  define open _open
#  define read _read
#  define stat _stat
#  define write _write
#else
#  ifdef sun
#    include <sys/filio.h>
#  endif
#  include <unistd.h>
#  define gg_file_close close
#endif

#ifndef S_IWUSR
#  define S_IWUSR S_IWRITE
#endif

#endif /* LIBGADU_FILEIO_H */
