#ifndef __SERVER_H_
#define __SERVER_H_

typedef struct _NabiComposeItem NabiComposeItem;
typedef struct _NabiServer NabiServer;

struct _NabiComposeItem {
    uint32_t key;
    wchar_t code;
};

typedef enum {
    NABI_OUTPUT_SYLLABLE,
    NABI_OUTPUT_JAMO,
    NABI_OUTPUT_MANUAL
} NabiOutputMode;

typedef enum {
    NABI_KEYBOARD_2SET,
    NABI_KEYBOARD_3SET
} NabiKeyboardType;

struct _NabiServer {
    /* XIMS */
    XIMS                    xims;
    Display*                display;
    Window                  window;
    long                    filter_mask;
    XIMTriggerKey*          trigger_keys;

    /* Input Context list */
    NabiIC**                ic_table;
    int                     ic_table_size;
    NabiIC*                 ic_freed;

    /* hangul automata */
    wchar_t*                keyboard_map;
    NabiComposeItem**       compose_map;
    int                     compose_map_size;
    Bool                    (*automata)(NabiIC*,
                                        KeySym,
                                        unsigned int state);
    Bool                    check_ksc;
    NabiOutputMode          output_mode;

    /* options */
    Bool                    dynamic_event_flow;
    Bool                    global_input_mode;
    NabiInputMode           input_mode;
    unsigned long           preedit_fg;
    unsigned long           preedit_bg;
};

extern NabiServer* server;

NabiServer* nabi_server_new(void);
void        nabi_server_destroy         (NabiServer* _server);
void        nabi_server_init            (NabiServer* _server);
int         nabi_server_start           (NabiServer* _server,
                                         Display*    display,
                                         Window      window);

Bool        nabi_server_is_trigger      (NabiServer*  _server,
                                         KeySym       key,
                                         unsigned int state);
void        nabi_server_set_automata    (NabiServer *_server,
                                         NabiKeyboardType type);
void        nabi_server_ic_table_expand (NabiServer* _server);
NabiIC*     nabi_server_get_ic          (NabiServer *_server,
                                         CARD16 icid);

#endif  /* __SERVER_H_ */
/* vim: set ts=8 sw=4 : */
