#ifndef __NABI_H_
#define __NABI_H_

typedef struct _NabiKeyboardMap NabiKeyboardMap;
typedef struct _NabiComposeMap NabiComposeMap;
typedef struct _NabiApplication NabiApplication;

struct _NabiKeyboardMap {
    gchar*                  name;
    gint                    type;
    wchar_t*                map;
};

struct _NabiComposeMap {
    gchar*                  name;
    NabiComposeItem**       map;
    gint                    size;
};

struct _NabiApplication {
    gint            x;
    gint            y;
    GtkWidget*      main_window;

    gchar*          keyboardmap_filename;
    gchar*          composemap_filename;

    NabiKeyboardMap keyboardmap;
    NabiComposeMap  composemap;

    /* preedit attribute */
    gchar           *preedit_fg;
    gchar           *preedit_bg;

    /* flags */
    gint            x_clicked;
    gint            y_clicked;
    gboolean        start_moving;
};

extern NabiApplication* nabi;

void load_keyboardmap(void);
void load_compose_map(void);
void load_config_file(void);

void nabi_app_new(void);
void nabi_app_init(void);
void nabi_app_setup_server(void);
void nabi_app_free(void);


GtkWidget* create_main_widget(void);

#endif /* __NABI_H_ */
/* vim: set ts=8 sw=4 : */
