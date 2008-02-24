/* Nabi - X Input Method server for hangul
 * Copyright (C) 2007-2008 Choe Hwanjin
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "debug.h"
#include "gettext.h"
#include "conf.h"

enum {
    CONFIG_BOOL,
    CONFIG_INT,
    CONFIG_STR
};

struct config_item {
    gchar* key;
    gint   type;
    guint  offset;
};

#define OFFSET(member) offsetof(NabiConfig, member)

const static struct config_item config_items[] = {
    { "xim_name",           CONFIG_STR,  OFFSET(xim_name)                 },
    { "x",                  CONFIG_INT,  OFFSET(x)                        },
    { "y",                  CONFIG_INT,  OFFSET(y)                        },
    { "show_palette",       CONFIG_BOOL, OFFSET(show_palette)             },
    { "palette_height",     CONFIG_INT,  OFFSET(palette_height)           },
    { "theme",              CONFIG_STR,  OFFSET(theme)                    },
    { "hangul_keyboard",    CONFIG_STR,  OFFSET(hangul_keyboard)          },
    { "latin_keyboard",     CONFIG_STR,  OFFSET(latin_keyboard)           },
    { "keyboard_layouts",   CONFIG_STR,  OFFSET(keyboard_layouts_file)    },
    { "triggerkeys",        CONFIG_STR,  OFFSET(trigger_keys)             },
    { "offkeys",            CONFIG_STR,  OFFSET(off_keys)                 },
    { "candidatekeys",      CONFIG_STR,  OFFSET(candidate_keys)           },
    { "output_mode",        CONFIG_STR,  OFFSET(output_mode)              },
    { "input_mode_option",  CONFIG_STR,  OFFSET(input_mode_option)        },
    { "preedit_foreground", CONFIG_STR,  OFFSET(preedit_fg)               },
    { "preedit_background", CONFIG_STR,  OFFSET(preedit_bg)               },
    { "candidate_font",	    CONFIG_STR,  OFFSET(candidate_font)           },
    { "candidate_format",   CONFIG_STR,  OFFSET(candidate_format)         },
    { "dynamic_event_flow", CONFIG_BOOL, OFFSET(use_dynamic_event_flow)   },
    { "commit_by_word",     CONFIG_BOOL, OFFSET(commit_by_word)           },
    { "auto_reorder",       CONFIG_BOOL, OFFSET(auto_reorder)             },
    { "use_simplified_chinese", CONFIG_BOOL, OFFSET(use_simplified_chinese) },
    { NULL,                 0,           0                                }
};

static inline gboolean*
nabi_config_get_bool(NabiConfig* config, size_t offset)
{
    return (gboolean*)((char*)(config) + offset);
}

static inline int*
nabi_config_get_int(NabiConfig* config, size_t offset)
{
    return (int*)((char*)(config) + offset);
}

static inline char**
nabi_config_get_str(NabiConfig* config, size_t offset)
{
    return (char**)((char*)(config) + offset);
}

static void
nabi_config_set_bool(NabiConfig* config, guint offset, gchar* value)
{
    if (value != NULL) {
	gboolean *member = nabi_config_get_bool(config, offset);

	if (g_ascii_strcasecmp(value, "true") == 0) {
	    *member = TRUE;
	} else {
	    *member = FALSE;
	}
    }
}

static void
nabi_config_set_int(NabiConfig* config, guint offset, gchar* value)
{
    if (value != NULL) {
	int *member = nabi_config_get_int(config, offset);
	*member = strtol(value, NULL, 10);
    }
}

static void
nabi_config_set_str(NabiConfig* config, guint offset, gchar* value)
{
    char **member = nabi_config_get_str(config, offset);

    g_free(*member);
    if (value == NULL)
	*member = g_strdup("");
    else
	*member = g_strdup(value);
}

static void
nabi_config_write_bool(NabiConfig* config, FILE* file, gchar* key, guint offset)
{
    gboolean *member = nabi_config_get_bool(config, offset);

    if (*member)
	fprintf(file, "%s=%s\n", key, "true");
    else
	fprintf(file, "%s=%s\n", key, "false");
}

static void
nabi_config_write_int(NabiConfig* config, FILE* file, gchar* key, guint offset)
{
    int *member = nabi_config_get_int(config, offset);

    fprintf(file, "%s=%d\n", key, *member);
}

static void
nabi_config_write_str(NabiConfig* config, FILE* file, gchar* key, guint offset)
{
    char **member = nabi_config_get_str(config, offset);

    if (*member != NULL)
	fprintf(file, "%s=%s\n", key, *member);
}

static void
nabi_config_load_item(NabiConfig* config, gchar* key, gchar* value)
{
    gint i;

    for (i = 0; config_items[i].key != NULL; i++) {
	if (strcmp(key, config_items[i].key) == 0) {
	    switch (config_items[i].type) {
	    case CONFIG_BOOL:
		nabi_config_set_bool(config, config_items[i].offset, value);
		break;
	    case CONFIG_INT:
		nabi_config_set_int(config, config_items[i].offset, value);
		break;
	    case CONFIG_STR:
		nabi_config_set_str(config, config_items[i].offset, value);
		break;
	    default:
		break;
	    }
	}
    }
}

NabiConfig*
nabi_config_new()
{
    NabiConfig* config = g_new(NabiConfig, 1);

    /* set default values */
    config->x = 0;
    config->y = 0;
    config->show_palette = FALSE;
    config->palette_height = 24;
    config->xim_name = g_strdup(PACKAGE);
    config->theme = g_strdup("Jini");

    config->hangul_keyboard = g_strdup(DEFAULT_KEYBOARD);
    config->latin_keyboard = g_strdup("none");
    config->keyboard_layouts_file = g_strdup("keyboard_layouts");

    config->trigger_keys = g_strdup("Hangul,Shift+space");
    config->off_keys = g_strdup("Escape");
    config->candidate_keys = g_strdup("Hangul_Hanja,F9");

    config->candidate_font = g_strdup("Sans 14");
    config->candidate_format = g_strdup("hanja");

    config->output_mode = g_strdup("syllable");
    config->input_mode_option = g_strdup("per_toplevel");

    config->preedit_fg = g_strdup("#000000");
    config->preedit_bg = g_strdup("#FFFFFF");

    config->use_dynamic_event_flow = TRUE;
    config->commit_by_word = FALSE;
    config->auto_reorder = TRUE;

    config->use_simplified_chinese = FALSE;

    return config;
}

void
nabi_config_delete(NabiConfig* config)
{
    g_free(config->xim_name);

    g_free(config->theme);

    g_free(config->hangul_keyboard);
    g_free(config->latin_keyboard);
    g_free(config->keyboard_layouts_file);

    g_free(config->trigger_keys);
    g_free(config->off_keys);
    g_free(config->candidate_keys);

    g_free(config->output_mode);
    g_free(config->input_mode_option);

    g_free(config->preedit_fg);
    g_free(config->preedit_bg);

    g_free(config->candidate_font);
    g_free(config->candidate_format);

    g_free(config);
}

void
nabi_config_load(NabiConfig* config)
{
    gchar *line, *saved_position;
    gchar *key, *value;
    const gchar* homedir;
    gchar* config_filename;
    gchar buf[256];
    FILE *file;

    /* load conf file */
    homedir = g_get_home_dir();
    config_filename = g_build_filename(homedir, ".nabi", "config", NULL);
    file = fopen(config_filename, "r");
    if (file == NULL) {
	nabi_log(1, "Can't open config file: %s\n", config_filename);
	return;
    }

    for (line = fgets(buf, sizeof(buf), file);
	 line != NULL;
	 line = fgets(buf, sizeof(buf), file)) {
	key = strtok_r(line, " =\t\n", &saved_position);
	value = strtok_r(NULL, "\r\n", &saved_position);
	if (key == NULL)
	    continue;
	nabi_config_load_item(config, key, value);
    }
    fclose(file);
    g_free(config_filename);
}

void
nabi_config_save(NabiConfig* config)
{
    gint i;
    const gchar* homedir;
    gchar* config_dir;
    gchar* config_filename;
    FILE *file;

    homedir = g_get_home_dir();
    config_dir = g_build_filename(homedir, ".nabi", NULL);

    /* chech for nabi conf dir */
    if (!g_file_test(config_dir, G_FILE_TEST_EXISTS)) {
	int ret;
	/* we make conf dir */
	ret = mkdir(config_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	if (ret != 0) {
	    perror("nabi");
	}
    }

    config_filename = g_build_filename(config_dir, "config", NULL);
    file = fopen(config_filename, "w");
    if (file == NULL) {
	nabi_log(1, "Can't write config file: %s\n", config_filename);
	return;
    }

    for (i = 0; config_items[i].key != NULL; i++) {
	switch (config_items[i].type) {
	case CONFIG_BOOL:
	    nabi_config_write_bool(config, file,
		    config_items[i].key, config_items[i].offset);
	    break;
	case CONFIG_INT:
	    nabi_config_write_int(config, file,
		    config_items[i].key, config_items[i].offset);
	    break;
	case CONFIG_STR:
	    nabi_config_write_str(config, file,
		    config_items[i].key, config_items[i].offset);
	    break;
	default:
	    break;
	}
    }
    fclose(file);

    g_free(config_dir);
    g_free(config_filename);
}
