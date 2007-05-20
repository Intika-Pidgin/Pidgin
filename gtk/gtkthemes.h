/**
 * @file gtkthemes.h GTK+ Smiley Theme API
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_GTKTHEMES_H_
#define _GAIM_GTKTHEMES_H_

struct smiley_list {
	char *sml;
	GSList *smileys;
	struct smiley_list *next;
};

struct smiley_theme {
	char *path;
	char *name;
	char *desc;
	char *icon;
	char *author;

	struct smiley_list *list;
};

extern struct smiley_theme *current_smiley_theme;
extern GSList *smiley_themes;

void gaim_gtkthemes_init(void);
gboolean gaim_gtkthemes_smileys_disabled(void);
void gaim_gtkthemes_smiley_themeize(GtkWidget *);
void gaim_gtkthemes_smiley_theme_probe(void);
void gaim_gtkthemes_load_smiley_theme(const char *file, gboolean load);
GSList *gaim_gtkthemes_get_proto_smileys(const char *id);
#endif /* _GAIM_GTKDIALOGS_H_ */
