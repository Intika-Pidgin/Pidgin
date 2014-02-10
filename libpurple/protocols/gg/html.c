/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Component written by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * This file is dual-licensed under the GPL2+ and the X11 (MIT) licences.
 * As a recipient of this file you may choose, which license to receive the
 * code under. As a contributor, you have to ensure the new code is
 * compatible with both.
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
#include "html.h"

#include <debug.h>

typedef struct
{
	GRegex *re_html_attr;
	GRegex *re_css_attr;
	GRegex *re_color_hex;
	GRegex *re_color_rgb;
} ggp_html_global_data;

static ggp_html_global_data global_data;

void ggp_html_setup(void)
{
	global_data.re_html_attr = g_regex_new(
		"([a-z-]+)=\"([^\"]+)\"",
		G_REGEX_OPTIMIZE, 0, NULL);
	global_data.re_css_attr = g_regex_new(
		"([a-z-]+): *([^;]+)",
		G_REGEX_OPTIMIZE, 0, NULL);
	global_data.re_color_hex = g_regex_new(
		"^#([0-9a-fA-F]+){6}$",
		G_REGEX_OPTIMIZE, 0, NULL);
	global_data.re_color_rgb = g_regex_new(
		"^rgb\\(([0-9]+), *([0-9]+), *([0-9]+)\\)$",
		G_REGEX_OPTIMIZE, 0, NULL);
}

void ggp_html_cleanup(void)
{
	g_regex_unref(global_data.re_html_attr);
	g_regex_unref(global_data.re_css_attr);
	g_regex_unref(global_data.re_color_hex);
	g_regex_unref(global_data.re_color_rgb);
}

GHashTable * ggp_html_tag_attribs(const gchar *attribs_str)
{
	GMatchInfo *match;
	GHashTable *attribs = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, g_free);

	if (attribs_str == NULL)
		return attribs;

	g_regex_match(global_data.re_html_attr, attribs_str, 0, &match);
	while (g_match_info_matches(match)) {
		g_hash_table_insert(attribs,
			g_match_info_fetch(match, 1),
			g_match_info_fetch(match, 2));

		g_match_info_next(match, NULL);
	}
	g_match_info_free(match);

	return attribs;
}

GHashTable * ggp_html_css_attribs(const gchar *attribs_str)
{
	GMatchInfo *match;
	GHashTable *attribs = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, g_free);

	if (attribs_str == NULL)
		return attribs;

	g_regex_match(global_data.re_css_attr, attribs_str, 0, &match);
	while (g_match_info_matches(match)) {
		g_hash_table_insert(attribs,
			g_match_info_fetch(match, 1),
			g_match_info_fetch(match, 2));

		g_match_info_next(match, NULL);
	}
	g_match_info_free(match);

	return attribs;
}

int ggp_html_decode_color(const gchar *str)
{
	GMatchInfo *match;
	int color = -1;

	g_regex_match(global_data.re_color_hex, str, 0, &match);
	if (g_match_info_matches(match)) {
		if (sscanf(str + 1, "%x", &color) != 1)
			color = -1;
	}
	g_match_info_free(match);
	if (color >= 0)
		return color;

	g_regex_match(global_data.re_color_rgb, str, 0, &match);
	if (g_match_info_matches(match)) {
		int r = -1, g = -1, b = -1;
		gchar *c_str;

		c_str = g_match_info_fetch(match, 1);
		if (c_str)
			r = atoi(c_str);
		g_free(c_str);

		c_str = g_match_info_fetch(match, 2);
		if (c_str)
			g = atoi(c_str);
		g_free(c_str);

		c_str = g_match_info_fetch(match, 3);
		if (c_str)
			b = atoi(c_str);
		g_free(c_str);

		if (r >= 0 && r < 256 && g >= 0 && g < 256 && b >= 0 && b < 256)
			color = (r << 16) | (g << 8) | b;
	}
	g_match_info_free(match);
	if (color >= 0)
		return color;

	return -1;
}

ggp_html_tag ggp_html_parse_tag(const gchar *tag_str)
{
	if (0 == g_ascii_strcasecmp(tag_str, "eom"))
		return GGP_HTML_TAG_EOM;
	if (0 == g_ascii_strcasecmp(tag_str, "span"))
		return GGP_HTML_TAG_SPAN;
	if (0 == g_ascii_strcasecmp(tag_str, "div"))
		return GGP_HTML_TAG_DIV;
	if (0 == g_ascii_strcasecmp(tag_str, "br"))
		return GGP_HTML_TAG_BR;
	if (0 == g_ascii_strcasecmp(tag_str, "a"))
		return GGP_HTML_TAG_A;
	if (0 == g_ascii_strcasecmp(tag_str, "b"))
		return GGP_HTML_TAG_B;
	if (0 == g_ascii_strcasecmp(tag_str, "i"))
		return GGP_HTML_TAG_I;
	if (0 == g_ascii_strcasecmp(tag_str, "u"))
		return GGP_HTML_TAG_U;
	if (0 == g_ascii_strcasecmp(tag_str, "s"))
		return GGP_HTML_TAG_S;
	if (0 == g_ascii_strcasecmp(tag_str, "img"))
		return GGP_HTML_TAG_IMG;
	if (0 == g_ascii_strcasecmp(tag_str, "font"))
		return GGP_HTML_TAG_FONT;
	if (0 == g_ascii_strcasecmp(tag_str, "hr"))
		return GGP_HTML_TAG_HR;
	return GGP_HTML_TAG_UNKNOWN;
}
