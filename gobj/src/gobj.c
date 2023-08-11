/****************************************************************************
 *          gobj.c
 *
 *          Objects G for Yuneta Simplified
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __linux__
    #include <pwd.h>
    #include <strings.h>
    #include <sys/utsname.h>
    #include <unistd.h>
#endif

#include "ansi_escape_codes.h"
#include "gobj_environment.h"
#include "kwid.h"
#include "helpers.h"
#include "gobj.h"

extern void jsonp_free(void *ptr);

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              GClass/GObj Structures
 ***************************************************************/
/*
 *  trace_level_t is used to describe trace levels
 */
typedef struct event_action_s {
    DL_ITEM_FIELDS

    gobj_event_t event;
    gobj_action_fn action;
    gobj_state_t next_state;
} event_action_t;

typedef struct state_s {
    DL_ITEM_FIELDS

    gobj_state_t state_name;
    dl_list_t dl_actions;
} state_t;

typedef struct event_s {
    DL_ITEM_FIELDS

    event_type_t event_type;
} event_t;

typedef enum { // WARNING add new values to opt2json()
    obflag_destroying       = 0x0001,
    obflag_destroyed        = 0x0002,
    obflag_created          = 0x0004,
} obflag_t;

typedef struct gclass_s {
    DL_ITEM_FIELDS

    char *gclass_name;
    dl_list_t dl_states;            // FSM
    dl_list_t dl_events;            // FSM
    const GMETHODS *gmt;            // Global methods
    const LMETHOD *lmt;

    const sdata_desc_t *tattr_desc;
    size_t priv_size;
    const sdata_desc_t *authz_table; // acl
    /*
     *  16 levels of user trace, with name and description,
     *  applicable to gobj or gclass (all his instances)
     *  Plus until 16 global levels.
     */
    const sdata_desc_t *command_table; // if it exits then mt_command is not used. Both must return a webix json.

    const trace_level_t *s_user_trace_level;    // up to 16
    gclass_flag_t gclass_flag;

    int32_t instances;              // instances of this gclass
    uint32_t trace_level;
    uint32_t no_trace_level;
    json_t *jn_trace_filter;
} gclass_t;

typedef struct gobj_s {
    DL_ITEM_FIELDS

    gclass_t *gclass;
    struct gobj_s *parent;
    dl_list_t dl_childs;

    state_t *current_state;
    state_t *last_state;

    gobj_flag_t gobj_flag;
    obflag_t obflag;

    json_t *dl_subscriptions; // external subscriptions to events of this gobj.
    json_t *dl_subscribings;  // subscriptions of this gobj to events of others gobj.

    // Data allocated
    char *gobj_name;
    json_t *jn_attrs;
    json_t *jn_stats;
    json_t *jn_user_data;
    const char *full_name;
    const char *short_name;
    void *priv;

    char running;           // set by gobj_start/gobj_stop
    char playing;           // set by gobj_play/gobj_pause
    char disabled;          // set by gobj_enable/gobj_disable
    hgobj bottom_gobj;

    uint32_t trace_level;
    uint32_t no_trace_level;
} gobj_t;

/***************************************************************
 *              System Events
 ***************************************************************/
// Timer Events, defined here to easy filtering in trace
GOBJ_DEFINE_EVENT(EV_TIMEOUT);
GOBJ_DEFINE_EVENT(EV_TIMEOUT_PERIODIC);

// System Events
GOBJ_DEFINE_EVENT(EV_STATE_CHANGED);

// Channel Messages: Input Events (Requests)
GOBJ_DEFINE_EVENT(EV_SEND_MESSAGE);
GOBJ_DEFINE_EVENT(EV_DROP);

// Channel Messages: Output Events (Publications)
GOBJ_DEFINE_EVENT(EV_ON_OPEN);
GOBJ_DEFINE_EVENT(EV_ON_CLOSE);
GOBJ_DEFINE_EVENT(EV_ON_MESSAGE);      // with GBuffer
GOBJ_DEFINE_EVENT(EV_ON_IEV_MESSAGE);  // with IEvent
GOBJ_DEFINE_EVENT(EV_ON_ID);
GOBJ_DEFINE_EVENT(EV_ON_ID_NAK);

// Tube Messages
GOBJ_DEFINE_EVENT(EV_CONNECT);
GOBJ_DEFINE_EVENT(EV_CONNECTED);
GOBJ_DEFINE_EVENT(EV_DISCONNECT);
GOBJ_DEFINE_EVENT(EV_DISCONNECTED);
GOBJ_DEFINE_EVENT(EV_RX_DATA);
GOBJ_DEFINE_EVENT(EV_TX_DATA);
GOBJ_DEFINE_EVENT(EV_TX_READY);
GOBJ_DEFINE_EVENT(EV_STOPPED);

// Frequent states
GOBJ_DEFINE_STATE(ST_DISCONNECTED);
GOBJ_DEFINE_STATE(ST_WAIT_CONNECTED);
GOBJ_DEFINE_STATE(ST_CONNECTED);
GOBJ_DEFINE_STATE(ST_WAIT_TXED);
GOBJ_DEFINE_STATE(ST_WAIT_DISCONNECTED);
GOBJ_DEFINE_STATE(ST_WAIT_STOPPED);
GOBJ_DEFINE_STATE(ST_STOPPED);
GOBJ_DEFINE_STATE(ST_SESSION);
GOBJ_DEFINE_STATE(ST_IDLE);
GOBJ_DEFINE_STATE(ST_WAIT_RESPONSE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void *_mem_malloc(size_t size);
PRIVATE void _mem_free(void *p);
PRIVATE void *_mem_realloc(void *p, size_t new_size);
PRIVATE void *_mem_calloc(size_t n, size_t size);
PRIVATE int register_named_gobj(gobj_t *gobj);
PRIVATE int deregister_named_gobj(gobj_t *gobj);
PRIVATE int write_json_parameters(
    gobj_t *gobj,
    json_t *kw,     // not own
    json_t *jn_global  // not own
);
// PRIVATE json_t *extract_all_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw     // not own
// );
// PRIVATE json_t *extract_json_config_variables_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw     // not own
// );
PRIVATE int rc_walk_by_tree(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);
PRIVATE int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);
PRIVATE int rc_walk_by_level(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
);

PRIVATE json_t *sdata_create(gobj_t *gobj, const sdata_desc_t* schema);
PRIVATE int set_default(gobj_t *gobj, json_t *sdata, const sdata_desc_t *it);
PRIVATE json_t *gobj_hsdata2(gobj_t *gobj, const char *name, BOOL verbose);
PUBLIC void trace_vjson(
    hgobj gobj,
    json_t *jn_data,    // now owned
    const char *msgset,
    const char *fmt,
    va_list ap
);
PRIVATE inline BOOL is_machine_tracing(gobj_t * gobj);
PRIVATE inline BOOL is_machine_not_tracing(gobj_t * gobj);
PRIVATE event_action_t *find_event_action(state_t *state, gobj_event_t event);
PRIVATE int add_event_type(
    dl_list_t *dl,
    event_type_t *event_type_
);

PRIVATE json_t *bit2level(
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    uint32_t bit
);
PRIVATE uint32_t level2bit(
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    const char *level
);
PRIVATE void print_track_mem(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int argc;
PRIVATE char **argv;

PRIVATE char __initialized__ = 0;
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */

#ifdef __linux__
PRIVATE struct utsname sys;
#endif

PRIVATE const char *sdata_flag_names[] = {
    "SDF_NOTACCESS",
    "SDF_RD",
    "SDF_WR",
    "SDF_REQUIRED",
    "SDF_PERSIST",
    "SDF_VOLATIL",
    "SDF_RESOURCE",
    "SDF_PKEY",
    "SDF_FUTURE1",
    "SDF_FUTURE2",
    "SDF_WILD_CMD",
    "SDF_STATS",
    "SDF_FKEY",
    "SDF_RSTATS",
    "SDF_PSTATS",
    "SDF_AUTHZ_R",
    "SDF_AUTHZ_W",
    "SDF_AUTHZ_X",
    "SDF_AUTHZ_P",
    "SDF_AUTHZ_S",
    "SDF_AUTHZ_RS",
    0
};

PRIVATE char __hostname__[64] = {0};
PRIVATE char __username__[64] = {0};

PUBLIC const char *event_flag_names[] = { // Strings of event_flag_t type
    "EVF_NO_WARN_SUBS",
    "EVF_OUTPUT_EVENT",
    "EVF_SYSTEM_EVENT",
    "EVF_PUBLIC_EVENT",
    0
};

/*
 *  System (gobj) output events
 */
PRIVATE dl_list_t dl_global_event_types;

PRIVATE int  __inside__ = 0;  // it's a counter
PRIVATE volatile int  __shutdowning__ = 0;
PRIVATE volatile BOOL __yuno_must_die__ = FALSE;
PRIVATE int  __exit_code__ = 0;
PRIVATE json_t * (*__global_command_parser_fn__)(
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
) = 0;
PRIVATE json_t * (*__global_stats_parser_fn__)(
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
) = 0;
PRIVATE authz_checker_fn __global_authz_checker_fn__ = 0;
PRIVATE authenticate_parser_fn __global_authenticate_parser_fn__ = 0;

PRIVATE json_t *__jn_global_settings__ = 0;
PRIVATE int (*__global_startup_persistent_attrs_fn__)(void) = 0;
PRIVATE void (*__global_end_persistent_attrs_fn__)(void) = 0;
PRIVATE int (*__global_load_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE int (*__global_save_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE int (*__global_remove_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;
PRIVATE json_t * (*__global_list_persistent_attrs_fn__)(hgobj gobj, json_t *keys) = 0;

PRIVATE dl_list_t dl_gclass;
PRIVATE json_t *jn_services = 0;        // Dict service:(json_int_t)(size_t)gobj
PRIVATE kw_match_fn __publish_event_match__ = kw_match_simple;

/*
 *  Global trace levels
 */
PRIVATE const trace_level_t s_global_trace_level[16] = {
    {"machine",         "Trace machine"},
    {"create_delete",   "Trace create/delete of gobjs"},
    {"create_delete2",  "Trace create/delete of gobjs level 2: with kw"},
    {"subscriptions",   "Trace subscriptions of gobjs"},
    {"start_stop",      "Trace start/stop of gobjs"},
    {"monitor",         "Monitor activity of gobjs"},
    {"event_monitor",   "Monitor events of gobjs"},
    {"libuv",           "Trace libuv mixins"},
    {"ev_kw",           "Trace event keywords"},
    {"authzs",          "Trace authorizations"},
    {"states",          "Trace change of states"},
    {"periodic_timer",  "Trace periodic timers"},
    {"gbuffers",        "Trace gbuffers"},
    {"timer",           "Trace timers"},
    {0, 0},
};

#define __trace_gobj_create_delete__(gobj)  (gobj_trace_level(gobj) & TRACE_CREATE_DELETE)
#define __trace_gobj_create_delete2__(gobj) (gobj_trace_level(gobj) & TRACE_CREATE_DELETE2)
#define __trace_gobj_subscriptions__(gobj)  (gobj_trace_level(gobj) & TRACE_SUBSCRIPTIONS)
#define __trace_gobj_start_stop__(gobj)     (gobj_trace_level(gobj) & TRACE_START_STOP)
#define __trace_gobj_oids__(gobj)           (gobj_trace_level(gobj) & TRACE_OIDS)
#define __trace_gobj_uv__(gobj)             (gobj_trace_level(gobj) & TRACE_UV)
#define __trace_gobj_ev_kw__(gobj)          (gobj_trace_level(gobj) & TRACE_EV_KW)
#define __trace_gobj_authzs__(gobj)         (gobj_trace_level(gobj) & TRACE_AUTHZS)
#define __trace_gobj_states__(gobj)         (gobj_trace_level(gobj) & TRACE_STATES)
#define __trace_gobj_periodic_timer__(gobj) (gobj_trace_level(gobj) & TRACE_PERIODIC_TIMER)

PRIVATE uint32_t __global_trace_level__ = 0;
PRIVATE uint32_t __global_trace_no_level__ = 0;
PRIVATE volatile uint32_t __deep_trace__ = 0;

PRIVATE gobj_t * __yuno__ = 0;
PRIVATE gobj_t * __default_service__ = 0;

PRIVATE sys_malloc_fn_t sys_malloc_fn = _mem_malloc;
PRIVATE sys_realloc_fn_t sys_realloc_fn = _mem_realloc;
PRIVATE sys_calloc_fn_t sys_calloc_fn = _mem_calloc;
PRIVATE sys_free_fn_t sys_free_fn = _mem_free;

PRIVATE size_t __max_block__ = 1*1024L*1024L;     /* largest memory block, default for no-using apps*/
PRIVATE size_t __max_system_memory__ = 10*1024L*1024L;   /* maximum core memory, default for no-using apps */
PRIVATE size_t __cur_system_memory__ = 0;   /* current system memory */




                    /*---------------------------------*
                     *      Start up functions
                     *---------------------------------*/




/***************************************************************************
 *  Initialize the yuno
 ***************************************************************************/
PUBLIC int gobj_start_up(
    int argc_,
    char *argv_[],
    json_t *jn_global_settings,
    int (*startup_persistent_attrs)(void),
    void (*end_persistent_attrs)(void),
    int (*load_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    int (*save_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    int (*remove_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    json_t * (*list_persistent_attrs)(
        hgobj gobj,
        json_t *attrs  // owned
    ),
    json_function_t global_command_parser,
    json_function_t global_stats_parser,
    authz_checker_fn global_authz_checker,
    authenticate_parser_fn global_authenticate_parser,

    size_t max_block,                       /* largest memory block */
    size_t max_system_memory                /* maximum system memory */
) {
    if(__initialized__) {
        return -1;
    }
    argc = argc_;
    argv = argv_;
    if (!atexit_registered) {
        atexit(gobj_shutdown);
        atexit_registered = 1;
    }

    glog_init();

    __shutdowning__ = 0;
    __jn_global_settings__ =  json_deep_copy(jn_global_settings);

    __global_startup_persistent_attrs_fn__ = startup_persistent_attrs;
    __global_end_persistent_attrs_fn__ = end_persistent_attrs;
    __global_load_persistent_attrs_fn__ = load_persistent_attrs;
    __global_save_persistent_attrs_fn__ = save_persistent_attrs;
    __global_remove_persistent_attrs_fn__ = remove_persistent_attrs;
    __global_list_persistent_attrs_fn__ = list_persistent_attrs;
    __global_command_parser_fn__ = global_command_parser;
    __global_stats_parser_fn__ = global_stats_parser;
    __global_authz_checker_fn__ = global_authz_checker;
    __global_authenticate_parser_fn__ = global_authenticate_parser;

    if(__global_startup_persistent_attrs_fn__) {
        __global_startup_persistent_attrs_fn__();
    }

#ifdef __linux__
    if(uname(&sys)==0) {
        change_char(sys.machine, '_', '-');
    }
#endif

    /*----------------------------------------*
     *          Build Global Events
     *----------------------------------------*/
    event_type_t event_types_[] = {
        {EV_STATE_CHANGED,     EVF_SYSTEM_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {0, 0}
    };
    dl_init(&dl_global_event_types);

    event_type_t *event_types = event_types_;
    while(event_types && event_types->event) {
        add_event_type(&dl_global_event_types, event_types);
        event_types++;
    }

    dl_init(&dl_gclass);
    jn_services = json_object();

    // dl_init(&dl_trans_filter);
    // gobj_add_publication_transformation_filter_fn("webix", webix_trans_filter);
    // helper_quote2doublequote(treedb_schema_gobjs);

    /*
     *  Chequea schema treedb, exit si falla.
     */
    //jn_treedb_schema_gobjs = legalstring2json(treedb_schema_gobjs, TRUE);
    //if(!jn_treedb_schema_gobjs) {
    //    exit(-1);
    //}

    if(max_block) {
        __max_block__ = max_block;
    }
    if(max_system_memory) {
        __max_system_memory__ = max_system_memory;
    }

    kw_add_binary_type(
        "gbuffer",
        "__gbuffer___",
        (serialize_fn_t)gbuffer_serialize,
        (deserialize_fn_t)gbuffer_deserialize,
        (incref_fn_t)gbuffer_incref,
        (decref_fn_t)gbuffer_decref
    );

#ifdef WIN32
    long unsigned int bufsize = sizeof(__username__);
    GetUserNameA(__username__, &bufsize);
#endif
#ifdef __linux__
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if(pw) {
        snprintf(__username__, sizeof(__username__), "%s", pw->pw_name);
    }
    gethostname(__hostname__, sizeof(__hostname__));
#endif

    __initialized__ = TRUE;


    return 0;
}

/***************************************************************************
 *  Order for shutdown the yuno
 ***************************************************************************/
PUBLIC void gobj_shutdown(void)
{
    if(__shutdowning__) {
        return;
    }
    __shutdowning__ = 1;

    if(__yuno__ && !(__yuno__->obflag & obflag_destroying)) {
        if(gobj_is_playing(__yuno__)) {
            gobj_pause(__yuno__);
        }
        if(gobj_is_running(__yuno__)) {
            gobj_stop(__yuno__);
        }
    }
}

/***************************************************************************
 *  Check if yuno is shutdowning
 ***************************************************************************/
PUBLIC BOOL gobj_is_shutdowning(void)
{
    return __shutdowning__;
}

/***************************************************************************
 *  De-initialize the yuno
 ***************************************************************************/
PUBLIC void gobj_end(void)
{
    if(!__initialized__) {
        return;
    }

    dl_flush(&dl_gclass, gclass_unregister);

    event_type_t *event_type;
    while((event_type = dl_first(&dl_global_event_types))) {
        dl_delete(&dl_global_event_types, event_type, 0);
        sys_free_fn(event_type);
    }

    JSON_DECREF(jn_services)

    if(__cur_system_memory__) {
        print_track_mem();
    }

    gobj_log_info(0, 0,
        "msgset",               "%s", MSGSET_STARTUP,
        "msg",                  "%s", "gobj_end",
        "hostname",             "%s", node_uuid(),
        NULL
    );

    glog_end();

    __initialized__ = FALSE;
}




                    /*---------------------------------*
                     *      GClass functions
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgclass gclass_create(
    gclass_name_t gclass_name,
    event_type_t *event_types,
    states_t *states,
    const GMETHODS *gmt,
    const LMETHOD *lmt,
    const sdata_desc_t *tattr_desc,
    size_t priv_size,
    const sdata_desc_t *authz_table,
    const sdata_desc_t *command_table,
    const trace_level_t *s_user_trace_level,
    gclass_flag_t gclass_flag
) {
    if(!__initialized__) {
        return NULL;
    }

    if(strchr(gclass_name, '`') || strchr(gclass_name, '^') || strchr(gclass_name, '.')) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GClass name CANNOT have '.' or '^' or '`' char",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return NULL;
    }

    if(gclass_find_by_name(gclass_name)) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name ALREADY exists",
            "gclass_name",  "%s", gclass_name,
            NULL
        );
        return NULL;
    }

    if(empty_string(gclass_name)) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name EMPTY",
            NULL
        );
        return NULL;
    }

#ifdef ESP_PLATFORM
    if(strlen(gclass_name) > 15) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name name TOO LONG for ESP32",
            "gclass_name",  "%s", gclass_name,
            "len",          "%d", (int)strlen(gclass_name),
            NULL
        );
    }
#endif

    gclass_t *gclass = sys_malloc_fn(sizeof(*gclass));
    if(gclass == NULL) {
        gobj_log_error(NULL, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass_name",  "%s", gclass_name,
            "size",         "%d", (int)sizeof(*gclass),
            NULL
        );
        return NULL;
    }

    dl_add(&dl_gclass, gclass);
    gclass->gclass_name = gobj_strdup(gclass_name);
    gclass->gmt = gmt;
    gclass->lmt = lmt;
    gclass->tattr_desc = tattr_desc;
    gclass->priv_size = priv_size;
    gclass->authz_table = authz_table;
    gclass->command_table = command_table;
    gclass->s_user_trace_level = s_user_trace_level;
    gclass->gclass_flag = gclass_flag;

    /*----------------------------------------*
     *          Build States
     *----------------------------------------*/
    struct states_s *state = states;
    while(state->state_name) {
        gclass_add_state_with_action_list(gclass, state->state_name, state->state);
        state++;
    }

    /*----------------------------------------*
     *          Build Events
     *----------------------------------------*/
    while(event_types && event_types->event) {
        add_event_type(&gclass->dl_events, event_types);
        event_types++;
    }

    /*----------------------------------------*
     *          Check FSM
     *----------------------------------------*/
    // TODO check fsm

    return gclass;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gclass_add_state(
    hgclass hgclass,
    gobj_state_t state_name
) {
    if(!hgclass) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgclass NULL",
            NULL
        );
        return -1;
    }

    gclass_t *gclass = (gclass_t *)hgclass;

    state_t *state = sys_malloc_fn(sizeof(*state));
    if(state == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "state",        "%s", state_name,
            "size",         "%d", (int)sizeof(*state),
            NULL
        );
        return -1;
    }

    state->state_name = state_name;

    dl_add(&gclass->dl_states, state);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE state_t *find_state(gclass_t *gclass, gobj_state_t state_name)
{
    state_t *state = dl_first(&gclass->dl_states);
    while(state) {
        if(state_name == state->state_name) {
            return state;
        }
        state = dl_next(state);
    }
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE event_action_t *find_event_action(state_t *state, gobj_event_t event)
{
    event_action_t *event_action = dl_first(&state->dl_actions);
    while(event_action) {
        if(event == event_action->event) {
            return event_action;
        }
        event_action = dl_next(event_action);
    }
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gclass_add_ev_action(
    hgclass hgclass,
    gobj_state_t state_name,
    gobj_event_t event,
    gobj_action_fn action,
    gobj_state_t next_state
) {
    if(!hgclass) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgclass NULL",
            NULL
        );
        return -1;
    }
    if(empty_string(state_name)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "state_name EMPTY",
            NULL
        );
        return -1;
    }

    gclass_t *gclass = (gclass_t *)hgclass;

    state_t *state = find_state(gclass, state_name);
    if(!state) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "state not found",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            NULL
        );
        return -1;
    }

    if(find_event_action(state, event)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "event already exists",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            "event",        "%s", event,
            NULL
        );
        return -1;
    }

    event_action_t *event_action = sys_malloc_fn(sizeof(*event_action));
    if(event_action == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass",       "%s", gclass->gclass_name,
            "state",        "%s", state_name,
            "event",        "%s", event,
            "size",         "%d", (int)sizeof(*state),
            NULL
        );
        return -1;
    }

    event_action->event = event;
    event_action->action = action;
    event_action->next_state = next_state;

    dl_add(&state->dl_actions, event_action);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gclass_add_state_with_action_list(
    hgclass hgclass,
    gobj_state_t state_name,
    ev_action_t *ev_action_list
) {
    if(gclass_add_state(hgclass, state_name)<0) {
        // Error already logged
        return -1;
    }

    while(ev_action_list->event) {
        gclass_add_ev_action(
            hgclass,
            state_name,
            ev_action_list->event,
            ev_action_list->action,
            ev_action_list->next_state
        );
        ev_action_list++;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgclass gclass_find_by_name(gclass_name_t gclass_name)
{
    gclass_t *gclass = dl_first(&dl_gclass);
    while(gclass) {
        if(strcmp(gclass_name, gclass->gclass_name)==0) {
            return gclass;
        }
        gclass = dl_next(gclass);
    }

    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC gobj_event_t gclass_find_public_event(const char *event, BOOL verbose)
{
    gclass_t *gclass = dl_first(&dl_gclass);
    while(gclass) {
        event_t *event_ = dl_first(&gclass->dl_events);
        while(event_) {
            if((event_->event_type.event_flag & EVF_PUBLIC_EVENT)) {
                if(event_->event_type.event && strcmp(event_->event_type.event, event)==0) {
                    return event_->event_type.event;
                }
            }
            event_ = dl_next(event_);
        }
        gclass = dl_next(gclass);
    }
    if(verbose) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event NOT FOUND",
            "event",        "%s", event,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gclass_unregister(hgclass hgclass)
{
    gclass_t *gclass = (gclass_t *)hgclass;

    if(gclass->instances > 0) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot unregister gclass: instances in use",
            "gclass",       "%s", gclass->gclass_name,
            "instances",    "%d", gclass->instances,
            NULL
        );
        return;
    }

    state_t *state;
    while((state = dl_first(&gclass->dl_states))) {
        dl_delete(&gclass->dl_states, state, 0);

        event_action_t *event_action;
        while((event_action = dl_first(&state->dl_actions))) {
            dl_delete(&state->dl_actions, event_action, 0);
            sys_free_fn(event_action);
        }

        sys_free_fn(state);
    }

    event_type_t *event_type;
    while((event_type = dl_first(&gclass->dl_events))) {
        dl_delete(&gclass->dl_events, event_type, 0);
        sys_free_fn(event_type);
    }

    sys_free_fn(gclass->gclass_name);
    sys_free_fn(gclass);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_event_type(
    dl_list_t *dl,
    event_type_t *event_type_
) {
    event_t *event = sys_malloc_fn(sizeof(*event));
    if(event == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "size",         "%d", (int)sizeof(*event),
            NULL
        );
        return -1;
    }

    event->event_type.event = event_type_->event;
    event->event_type.event_flag = event_type_->event_flag;

    return dl_add(dl, event);
}




                    /*---------------------------------*
                     *      GObj create functions
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_create_gobj(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent_,
    gobj_flag_t gobj_flag
) {
    if(!gobj_name) {
        gobj_name = "";
    }
    gobj_t *parent = parent_;

    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    if(gobj_flag & (gobj_flag_yuno)) {
        if(__yuno__) {
            gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "__yuno__ already created",
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
        gobj_flag |= gobj_flag_service;

    } else {
        if(!parent) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gobj NEEDS a parent!",
                "gclass",       "%s", gclass_name,
                "name",         "%s", gobj_name,
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
    }

    if(gobj_flag & (gobj_flag_service)) {
        if(gobj_find_service(gobj_name, FALSE)) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "service ALREADY registered!",
                "gclass",       "%s", gclass_name,
                "name",         "%s", gobj_name,
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
    }

    if(gobj_flag & (gobj_flag_default_service)) {
        if(gobj_find_service(gobj_name, FALSE) || __default_service__) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "default service ALREADY registered!",
                "gclass",       "%s", gclass_name,
                "name",         "%s", gobj_name,
                NULL
            );
            JSON_DECREF(kw)
            return NULL;
        }
        gobj_flag |= gobj_flag_service;
    }

    if(empty_string(gclass_name)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass_name EMPTY",
            "gobj_name",    "%s", gobj_name,
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    if(strchr(gclass_name, '`') || strchr(gclass_name, '^')) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GObj name CANNOT have '^' or '`' char",
            "gclass",       "%s", gclass_name,
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    gclass_t *gclass = gclass_find_by_name(gclass_name);
    if(!gclass) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass not found",
            "gclass",       "%s", gclass_name,
            "gobj_name",    "%s", gobj_name,
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    // TODO implement gcflag_singleton

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    gobj_t *gobj = sys_malloc_fn(sizeof(*gobj));
    if(gobj == NULL) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass",       "%s", gclass->gclass_name,
            "gobj_name",    "%s", gobj_name,
            "size",         "%d", (int)sizeof(*gobj),
            NULL
        );
        JSON_DECREF(kw)
        return NULL;
    }

    /*--------------------------------*
     *      Initialize variables
     *--------------------------------*/
    gobj->gclass = gclass;
    gobj->parent = parent;
    dl_init(&gobj->dl_childs);
    gobj->dl_subscribings = json_array();
    gobj->dl_subscriptions = json_array();
    gobj->current_state = dl_first(&gclass->dl_states);
    gobj->last_state = 0;
    gobj->obflag = 0;
    gobj->gobj_flag = gobj_flag;

    if(__trace_gobj_create_delete__(gobj)) {
         trace_machine("💙💙⏩ creating: %s^%s",
            gclass->gclass_name,
            gobj_name
        );
    }

    /*--------------------------*
     *      Alloc data
     *--------------------------*/
    gobj->gobj_name = gobj_strdup(gobj_name);
    gobj->jn_attrs = sdata_create(gobj, gclass->tattr_desc);
    gobj->jn_stats = json_object();
    gobj->jn_user_data = json_object();
    gobj->priv = gclass->priv_size? sys_malloc_fn(gclass->priv_size):NULL;

    if(!gobj->gobj_name || !gobj->jn_user_data || !gobj->jn_stats ||
            !gobj->jn_attrs || (gclass->priv_size && !gobj->priv)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "No memory",
            "gclass",       "%s", gclass->gclass_name,
            "gobj_name",    "%s", gobj_name,
            NULL
        );
        JSON_DECREF(kw)
        gobj_destroy(gobj);
        return NULL;
    }

    /*--------------------------------*
     *  Write configuration
     *--------------------------------*/
    write_json_parameters(gobj, kw, __jn_global_settings__);

    /*--------------------------------------*
     *  Load writable and persistent attrs
     *  of named-gobjs and __root__
     *--------------------------------------*/
    if(gobj->gobj_flag & (gobj_flag_service)) {
        if(__global_load_persistent_attrs_fn__) {
            __global_load_persistent_attrs_fn__(gobj, 0);
        }
    }

    /*--------------------------*
     *  Register service
     *--------------------------*/
    if(gobj->gobj_flag & (gobj_flag_service)) {
        register_named_gobj(gobj);
    }
    if(gobj->gobj_flag & (gobj_flag_yuno)) {
        __yuno__ = gobj;
    }
    if(gobj->gobj_flag & (gobj_flag_default_service)) {
        __default_service__ = gobj;
    }

    /*--------------------------------------*
     *      Add to parent
     *--------------------------------------*/
    if(!(gobj->gobj_flag & (gobj_flag_yuno))) {
        dl_add(&parent->dl_childs, gobj);
    }

    /*--------------------------------*
     *      Exec mt_create
     *--------------------------------*/
    gobj->obflag |= obflag_created;
    gobj->gclass->instances++;

    if(gobj->gclass->gmt->mt_create2) {
        JSON_INCREF(kw)
        gobj->gclass->gmt->mt_create2(gobj, kw);
    } else if(gobj->gclass->gmt->mt_create) {
        gobj->gclass->gmt->mt_create(gobj);
    }

    /*--------------------------------------*
     *  Inform to parent now,
     *  when the child is full operative
     *-------------------------------------*/
    if(parent && parent->gclass->gmt->mt_child_added) {
        if(__trace_gobj_create_delete__(gobj)) {
            trace_machine("👦👦🔵 child_added(%s): %s",
                gobj_full_name(parent),
                gobj_short_name(gobj)
            );
        }
        parent->gclass->gmt->mt_child_added(parent, gobj);
    }

    if(__trace_gobj_create_delete__(gobj)) {
        trace_machine("💙💙⏪ created: %s",
            gobj_full_name(gobj)
        );
    }

    /*
     *  __root__ wants to know all
     */
    if(__yuno__ && __yuno__->gclass->gmt->mt_gobj_created) {
        __yuno__->gclass->gmt->mt_gobj_created(__yuno__, gobj);
    }

    JSON_DECREF(kw)
    return (hgobj)gobj;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_destroy(hgobj hgobj)
{
    if(hgobj == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            NULL
        );
        return;
    }

    gobj_t *gobj = (gobj_t *)hgobj;

    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYING",
            NULL
        );
        return;
    }
    gobj->obflag |= obflag_destroying;

    if(__trace_gobj_create_delete__(gobj)) {
        trace_machine("💔💔⏩ destroying: %s",
            gobj_full_name(gobj)
        );
    }

    /*----------------------------------------------*
     *  Inform to parent now,
     *  when the child is NOT still full operative
     *----------------------------------------------*/
    gobj_t *parent = gobj->parent;
    if(parent && parent->gclass->gmt->mt_child_removed) {
        if(__trace_gobj_create_delete__(gobj)) {
            trace_machine("👦👦🔴 child_removed(%s): %s",
                gobj_full_name(parent),
                gobj_short_name(gobj)
            );
        }
        parent->gclass->gmt->mt_child_removed(parent, gobj);
    }

    /*--------------------------------*
     *  Deregister if service
     *--------------------------------*/
    if(gobj->gobj_flag & gobj_flag_service) {
        deregister_named_gobj(gobj);
    }

    /*--------------------------------*
     *      Pause
     *--------------------------------*/
    if(gobj->playing) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "Destroying a PLAYING gobj",
            "full-name",    "%s", gobj_full_name(gobj),
            NULL
        );
        gobj_pause(gobj);
    }
    /*--------------------------------*
     *      Stop
     *--------------------------------*/
    if(gobj->running) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "Destroying a RUNNING gobj",
            "full-name",    "%s", gobj_full_name(gobj),
            NULL
        );
        gobj_stop(gobj);
    }

    /*--------------------------------*
     *      Delete subscriptions
     *--------------------------------*/
    json_t *dl_subs;
    dl_subs = json_copy(gobj->dl_subscriptions);
    gobj_unsubscribe_list(dl_subs, TRUE);
    dl_subs = json_copy(gobj->dl_subscribings);
    gobj_unsubscribe_list(dl_subs, TRUE);

    /*--------------------------------*
     *      Delete from parent
     *--------------------------------*/
    if(parent) {
        dl_delete(&gobj->parent->dl_childs, gobj, 0);
    }

    /*--------------------------------*
     *      Delete childs
     *--------------------------------*/
    gobj_destroy_childs(gobj);

    /*-------------------------------------------------*
     *  Exec mt_destroy
     *  Call this after all childs are destroyed.
     *  Then you can delete resources used by childs
     *  (example: event_loop in main/threads)
     *-------------------------------------------------*/
    if(gobj->obflag & obflag_created) {
        if(gobj->gclass->gmt->mt_destroy) {
            gobj->gclass->gmt->mt_destroy(gobj);
        }
    }

    /*--------------------------------*
     *      Mark as destroyed
     *--------------------------------*/
    if(__trace_gobj_create_delete__(gobj)) {
        trace_machine("💔💔⏪ destroyed: %s",
            gobj_full_name(gobj)
        );
    }
    gobj->obflag |= obflag_destroyed;

    /*--------------------------------*
     *      Dealloc data
     *--------------------------------*/
    EXEC_AND_RESET(sys_free_fn, gobj->gobj_name)
    JSON_DECREF(gobj->jn_attrs)
    JSON_DECREF(gobj->jn_stats)
    JSON_DECREF(gobj->jn_user_data)
    JSON_DECREF(gobj->dl_subscribings);
    JSON_DECREF(gobj->dl_subscriptions);
    EXEC_AND_RESET(sys_free_fn, gobj->priv)

    EXEC_AND_RESET(sys_free_fn, gobj->full_name)
    EXEC_AND_RESET(sys_free_fn, gobj->short_name)

    if(gobj->obflag & obflag_created) {
        gobj->gclass->instances--;
    }
    sys_free_fn(gobj);
}

/***************************************************************************
 *  Destroy childs
 ***************************************************************************/
PUBLIC void gobj_destroy_childs(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return;
    }

    gobj_t *child = dl_first(&gobj->dl_childs);
    while(child) {
        gobj_t *next = dl_next(child);
        if(!(child->obflag & (obflag_destroyed|obflag_destroying))) {
            gobj_destroy(child);
        }
        /*
         *  Next
         */
        child = next;
    }
}

/***************************************************************************
 *  register named gobj
 ***************************************************************************/
PRIVATE int register_named_gobj(gobj_t *gobj)
{
    if(json_object_get(jn_services, gobj->gobj_name)) {
        gobj_t *prev_gobj = (hgobj)(size_t)json_integer_value(
            json_object_get(jn_services, gobj->gobj_name)
        );

        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj unique ALREADY REGISTERED. Will be UPDATED",
            "prev gclass",  "%s", gobj_gclass_name(prev_gobj),
            "gclass",       "%s", gobj_gclass_name(gobj),
            "name",         "%s", gobj_name(gobj),
            NULL
        );
    }

    int ret = json_object_set_new(
        jn_services,
        gobj->gobj_name,
        json_integer((json_int_t)(size_t)gobj)
    );
    if(ret == -1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set_new() FAILED",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "name",         "%s", gobj_name(gobj),
            NULL
        );
    }

    return ret;
}

/***************************************************************************
 *  deregister named gobj
 ***************************************************************************/
PRIVATE int deregister_named_gobj(gobj_t *gobj)
{
    json_t *jn_obj = json_object_get(jn_services, gobj->gobj_name);
    if(!jn_obj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NOT FOUND",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "name",         "%s", gobj_name(gobj),
            NULL
        );
        return -1;
    }
    json_object_del(jn_services, gobj->gobj_name);

    return 0;
}




                    /*---------------------------------*
                     *      Attributes functions
                     *---------------------------------*/




/***************************************************************************
 *  ATTR:
 *  Contain the key my gobj-name or my gclass-name?
 *  Return NULL if not, or the key cleaned of prefix.
 ***************************************************************************/
// PRIVATE const char * is_for_me(
//     const char *gclass_name,
//     const char *gobj_name,
//     const char *key)
// {
//     char *p = strchr(key, '.');
//     if(p) {
//         char temp[256]; // if your names are longer than 256 bytes, you are ...
//         int ln = p - key;
//         if (ln >= sizeof(temp))
//             return 0; // ... very intelligent!
//         strncpy(temp, key, ln);
//         temp[ln]=0;
//
//         if(gobj_name && strcmp(gobj_name, temp)==0) {
//             // match the name
//             return p+1;
//         }
//         if(gclass_name && strcasecmp(gclass_name, temp)==0) {
//             // match the gclass name
//             return p+1;
//         }
//     }
//     return 0;
// }

/***************************************************************************
 *  ATTR:
 ***************************************************************************/
// PRIVATE json_t *extract_all_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw)     // not own
// {
//     json_t *kw_mine = json_object();
//     const char *key;
//     json_t *jn_value;
//
//     json_object_foreach(kw, key, jn_value) {
//         const char *pk = is_for_me(
//             gclass_name,
//             gobj_name,
//             key
//         );
//         if(!pk)
//             continue;
//         if(strcasecmp(pk, "kw")==0) {
//             json_object_update(kw_mine, jn_value);
//         } else {
//             json_object_set(kw_mine, pk, jn_value);
//         }
//     }
//
//     return kw_mine;
// }

/***************************************************************************
 *  ATTR:
 ***************************************************************************/
// PRIVATE json_t *extract_json_config_variables_mine(
//     const char *gclass_name,
//     const char *gobj_name,
//     json_t *kw     // not own
// )
// {
//     json_t *kw_mine = json_object();
//     const char *key;
//     json_t *jn_value;
//
//     json_object_foreach(kw, key, jn_value) {
//         const char *pk = is_for_me(
//             gclass_name,
//             gobj_name,
//             key
//         );
//         if(!pk)
//             continue;
//         if(strcasecmp(pk, "__json_config_variables__")==0) {
//             json_object_set(kw_mine, pk, jn_value);
//         }
//     }
//
//     return kw_mine;
// }

/***************************************************************************
 *  ATTR:
 *  Get data from json kw parameter, and put the data in config sdata.

    With filter_own:
    Filter only the parameters in settings belongs to gobj.
    The gobj is searched by his named-gobj or his gclass name.
    The parameter name in settings, must be a dot-named,
    with the first item being the named-gobj o gclass name.
 *
 ***************************************************************************/
PRIVATE int write_json_parameters(
    gobj_t * gobj,
    json_t *kw,     // not own
    json_t *jn_global) // not own
{
    json_t *hs = gobj_hsdata(gobj);

    json_object_update_existing(hs, kw);


//    json_t *jn_global_mine = extract_all_mine(
//        gobj->gclass->gclass_name,
//        gobj->gobj_name,
//        jn_global
//    );

//    if(__trace_gobj_create_delete2__(gobj)) {
//        trace_machine("🔰 %s^%s => global_mine",
//            gobj->gclass->gclass_name,
//            gobj->name
//        );
//        gobj_trace_json(0, jn_global, "global all");
//        gobj_trace_json(0, jn_global_mine, "global_mine");
//    }

//    json_t *__json_config_variables__ = json_object_get(
//        jn_global_mine,
//        "__json_config_variables__"
//    );
//    if(__json_config_variables__) {
//        __json_config_variables__ = json_object();
//    }
//
//    json_object_update_new(
//        __json_config_variables__,
//        gobj_global_variables()
//    );
//    if(gobj_yuno()) {
//        json_object_update_new(
//            __json_config_variables__,
//            json_pack("{s:s, s:b}",
//                "__bind_ip__", gobj_read_str_attr(gobj_yuno(), "bind_ip"),
//                "__multiple__", gobj_read_bool_attr(gobj_yuno(), "yuno_multiple")
//            )
//        );
//    }

//    if(__trace_gobj_create_delete2__(gobj)) {
//        trace_machine("🔰🔰 %s^%s => __json_config_variables__",
//            gobj->gclass->gclass_name,
//            gobj->name
//        );
//        gobj_trace_json(0, __json_config_variables__, "__json_config_variables__");
//    }

//    json_t * new_kw = kw_apply_json_config_variables(kw, jn_global_mine);
//    json_decref(jn_global_mine);
//    if(!new_kw) {
//        return -1;
//    }

//    if(__trace_gobj_create_delete2__(gobj)) {
//        trace_machine("🔰🔰🔰 %s^%s => final kw",
//            gobj->gclass->gclass_name,
//            gobj->name
//        );
//        gobj_trace_json(0, new_kw, "final kw");
//    }

//    json_t *__user_data__ = kw_get_dict(new_kw, "__user_data__", 0, KW_EXTRACT);
//
//    int ret = json2sdata(
//        hs,
//        new_kw,
//        -1,
//        (gobj->gclass->gclass_flag & gcflag_ignore_unknown_attrs)?0:print_attr_not_found,
//        gobj
//    );
//
//    if(__user_data__) {
//        json_object_update(((gobj_t *)gobj)->jn_user_data, __user_data__);
//        json_decref(__user_data__);
//    }
//    json_decref(new_kw);
    return 0;
}

/***************************************************************************
 *  save now persistent and writable attrs
    attrs can be a string, a list of keys, or a dict with the keys to save/delete
 ***************************************************************************/
PUBLIC int gobj_save_persistent_attrs(hgobj gobj_, json_t *jn_attrs)
{
    gobj_t *gobj = gobj_;

    if(!(gobj->gobj_flag & (gobj_flag_service))) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot save writable-persistent attrs in no named-gobjs",
            NULL
        );
        JSON_DECREF(jn_attrs);
        return -1;
    }
    if(__global_save_persistent_attrs_fn__) {
        return __global_save_persistent_attrs_fn__(gobj, jn_attrs);
    }
    JSON_DECREF(jn_attrs)
    return -1;
}

/***************************************************************************
 *  remove file of persistent and writable attrs
 *  attrs can be a string, a list of keys, or a dict with the keys to save/delete
 *  if attrs is empty save/remove all attrs
 ***************************************************************************/
PUBLIC int gobj_remove_persistent_attrs(hgobj gobj_, json_t *jn_attrs)
{
    gobj_t *gobj = gobj_;

    if(!__global_remove_persistent_attrs_fn__) {
        JSON_DECREF(jn_attrs)
        return -1;
    }
    return __global_remove_persistent_attrs_fn__(gobj, jn_attrs);
}

/***************************************************************************
 *  List persistent attributes
 ***************************************************************************/
PUBLIC json_t * gobj_list_persistent_attrs(hgobj gobj, json_t *jn_attrs)
{
    if(!__global_list_persistent_attrs_fn__) {
        JSON_DECREF(jn_attrs);
        return 0;
    }
    return __global_list_persistent_attrs_fn__(gobj, jn_attrs);
}

/***************************************************************************
 *  WRITE - high level -
 ***************************************************************************/
PRIVATE int sdata_write_default_values(
    gobj_t *gobj,
    sdata_flag_t include_flag,
    sdata_flag_t exclude_flag
)
{
    const sdata_desc_t *it = gobj->gclass->tattr_desc;
    while(it->name) {
        if(exclude_flag && (it->flag & exclude_flag)) {
            it++;
            continue;
        }
        if(include_flag == (sdata_flag_t)-1 || (it->flag & include_flag)) {
            set_default(gobj, gobj->jn_attrs, it);
        }
        it++;
    }

    return 0;
}

/***************************************************************************
 *  Build default values
 ***************************************************************************/
PRIVATE json_t *sdata_create(gobj_t *gobj, const sdata_desc_t* schema)
{
    json_t *sdata = json_object();
    const sdata_desc_t *it = schema;
    while(it->name != 0) {
        set_default(gobj, sdata, it);
        it++;
    }

    return sdata;
}

/***************************************************************************
 *  Set array values to sdata, from json or binary
 ***************************************************************************/
PRIVATE int set_default(gobj_t *gobj, json_t *sdata, const sdata_desc_t *it)
{
    json_t *jn_value = 0;
    const char *svalue = it->default_value;
    if(!svalue) {
        svalue = "";
    }
    switch(it->type) {
        case DTP_STRING:
            jn_value = json_string(svalue);
            if(!jn_value) {
                jn_value = json_null();
            }
            break;
        case DTP_BOOLEAN:
            jn_value = json_boolean(svalue);
            if(strcasecmp(svalue, "true")==0) {
                jn_value = json_true();
            } else if(strcasecmp(svalue, "false")==0) {
                jn_value = json_false();
            } else {
                jn_value = atoi(svalue)? json_true(): json_false();
            }
            break;
        case DTP_INTEGER:
            jn_value = json_integer(atoll(svalue));
            break;
        case DTP_REAL:
            jn_value = json_real(atof(svalue));
            break;
        case DTP_LIST:
            jn_value = anystring2json(svalue, strlen(svalue), FALSE);
            if(!json_is_array(jn_value)) {
                JSON_DECREF(jn_value)
                jn_value = json_array();
            }
            break;
        case DTP_DICT:
            jn_value = anystring2json(svalue, strlen(svalue), FALSE);
            if(!json_is_object(jn_value)) {
                JSON_DECREF(jn_value)
                jn_value = json_object();
            }
            break;
        case DTP_JSON:
            jn_value = anystring2json(svalue, strlen(svalue), FALSE);
            break;
        case DTP_POINTER:
            jn_value = json_integer((json_int_t)(size_t)svalue);
            break;
    }

    if(!jn_value) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "default_value WRONG",
            "item",         "%s", it->name,
            "value",        "%s", svalue,
            NULL
        );
        jn_value = json_null();
    }

    if(json_object_set_new(sdata, it->name, jn_value)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set() FAILED",
            "attr",         "%s", it->name,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Set array values to sdata, from json or binary
 ***************************************************************************/
PRIVATE int json2item(
    gobj_t *gobj,
    json_t *sdata,
    const sdata_desc_t *it,
    json_t *jn_value // now owned
)
{
    if(!it) {
        return -1;
    }
    switch(it->type) {
        case DTP_STRING:
            if(!json_is_string(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be a string",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_BOOLEAN:
            if(!json_is_boolean(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an boolean",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_INTEGER:
            if(!json_is_integer(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an integer",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_REAL:
            if(!json_is_real(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be a real",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_LIST:
            if(!json_is_array(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an array",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_DICT:
            if(!json_is_object(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an object",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
        case DTP_JSON:
            break;
        case DTP_POINTER:
            if(!json_is_integer(jn_value)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "attr must be an integer",
                    "attr",         "%s", it->name,
                    NULL
                );
                return -1;
            }
            break;
    }

    if(json_object_set(sdata, it->name, jn_value)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "json_object_set() FAILED",
            "attr",         "%s", it->name,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_hsdata(hgobj gobj_) // Return is NOT YOURS
{
    gobj_t *gobj = gobj_;
    return gobj?gobj->jn_attrs:NULL;
}

/***************************************************************************
 *  Return the data description of the command `command`
 *  If `command` is null returns full command's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gclass_command_desc(hgclass gclass_, const char *name, BOOL verbose)
{
    gclass_t *gclass = gclass_;

    if(!gclass) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gclass NULL",
                NULL
            );
        }
        return 0;
    }

    if(!name) {
        return gclass->command_table;
    }
    const sdata_desc_t *it = gclass->command_table;
    while(it->name) {
        if(strcmp(it->name, name)==0) {
            return it;
        }
        it++;
    }

    if(verbose) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GClass command NOT FOUND",
            "gclass",       "%s", gclass->gclass_name,
            "command",         "%s", name,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Return the data description of the command `command`
 *  If `command` is null returns full command's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gobj_command_desc(hgobj gobj_, const char *name, BOOL verbose)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gobj NULL",
                NULL
            );
        }
        return 0;
    }

    if(!name) {
        return gobj->gclass->command_table;
    }

    return gclass_command_desc(gobj->gclass, name, verbose);
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gclass_attr_desc(hgclass gclass_, const char *name, BOOL verbose)
{
    gclass_t *gclass = gclass_;

    if(!gclass) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gclass NULL",
                NULL
            );
        }
        return 0;
    }

    if(!name) {
        return gclass->tattr_desc;
    }
    const sdata_desc_t *it = gclass->tattr_desc;
    while(it->name) {
        if(strcmp(it->name, name)==0) {
            return it;
        }
        it++;
    }

    if(verbose) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GClass Attribute NOT FOUND",
            "gclass",       "%s", gclass->gclass_name,
            "attr",         "%s", name,
            NULL
        );
    }
    return NULL;
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
PUBLIC const sdata_desc_t *gobj_attr_desc(hgobj gobj_, const char *name, BOOL verbose)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "gobj NULL",
                NULL
            );
        }
        return 0;
    }

    if(!name) {
        return gobj->gclass->tattr_desc;
    }

    return gclass_attr_desc(gobj->gclass, name, verbose);
}

/***************************************************************************
 *
 ***************************************************************************/
BOOL gclass_has_attr(hgclass gclass, const char* name)
{
    if(!gclass) { // WARNING must be a silence function!
        return FALSE;
    }
    if(empty_string(name)) {
        return FALSE;
    }
    return gclass_attr_desc(gclass, name, FALSE)?TRUE:FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_attr(hgobj hgobj, const char *name)
{
    if(!hgobj) { // WARNING must be a silence function!
        return FALSE;
    }
    if(empty_string(name)) {
        return FALSE;
    }
    gobj_t *gobj = (gobj_t *)hgobj;
    return gclass_attr_desc(gobj->gclass, name, FALSE)?TRUE:FALSE;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC BOOL gobj_is_readable_attr(hgobj gobj, const char *name)
{
    const sdata_desc_t *it = gobj_attr_desc(gobj, name, TRUE);
    if(it && it->flag & (ATTR_READABLE)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC BOOL gobj_is_writable_attr(hgobj gobj, const char *name)
{
    const sdata_desc_t *it = gobj_attr_desc(gobj, name, TRUE);
    if(it && it->flag & (ATTR_WRITABLE)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *  ATTR: write
 *  Reset volatile attributes
 ***************************************************************************/
PUBLIC int gobj_reset_volatil_attrs(hgobj gobj)
{
    return sdata_write_default_values(
        gobj,
        SDF_VOLATIL,    // include_flag
        0               // exclude_flag
    );
}

/***************************************************************************
 *  ATTR: write
 *  Reset rstats attributes
 ***************************************************************************/
PUBLIC int gobj_reset_rstats_attrs(hgobj gobj)
{
    return sdata_write_default_values(
        gobj,
        SDF_RSTATS,     // include_flag
        0               // exclude_flag
    );
}

/***************************************************************************
 *  ATTR: read str
 *  Return is NOT yours! New api (May/2019), js style
 *  Inherit from js-core
 ***************************************************************************/
PUBLIC json_t *gobj_read_attr(
    hgobj gobj_,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom)
    hgobj src
) {
    gobj_t *gobj = gobj_;

    // TODO use ` segments
    if(!gobj_has_attr(gobj, path)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Attribute not found",
            "path",         "%s", path,
            NULL
        );
    }
    json_t *hs = gobj_hsdata(gobj);

//    if(gobj->gclass->gmt->mt_reading) { TODO como hago el reading de todo el record???
//        if(!(gobj->obflag & obflag_destroyed)) {
//            gobj->gclass->gmt->mt_reading(gobj, name);
//        }
//    }

    return json_object_get(hs, path);
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_t *gobj_read_attrs( // Return is yours!
    hgobj gobj_,
    sdata_flag_t include_flag,
    hgobj src
) {
    gobj_t *gobj = gobj_;

    json_t *jn_attrs = json_object();

    const sdata_desc_t *it = gobj->gclass->tattr_desc;
    while(it->name) {
        if(include_flag == (sdata_flag_t)-1 || (it->flag & include_flag)) {
            json_t *jn = json_object_get(gobj->jn_attrs, it->name);
            json_object_set(jn_attrs, it->name, jn);
        }
        it++;
    }

    return jn_attrs;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_t *gobj_read_user_data(
    hgobj gobj,
    const char *name // If empty name return all user record
) {
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    if(empty_string(name)) {
        return ((gobj_t *)gobj)->jn_user_data;
    } else {
        return json_object_get(((gobj_t *)gobj)->jn_user_data, name);
    }
}

/***************************************************************************
 *  ATTR: write
 *  New api (May/2019), js style
 *  Inherit from js-core
 ***************************************************************************/
PUBLIC int gobj_write_attr(
    hgobj gobj,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom)
    json_t *jn_value,  // owned
    hgobj src
)
{
    // TODO use ` segments
    if(!gobj_has_attr(gobj, path)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Attribute not found",
            "path",         "%s", path,
            NULL
        );
    }
    json_t *hs = gobj_hsdata(gobj);
    const sdata_desc_t *it = gobj_attr_desc(gobj, path, TRUE);
    int ret = json2item(gobj, hs, it, jn_value);

    JSON_DECREF(jn_value);
    return ret;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_attrs(
    hgobj gobj,
    json_t *kw,  // owned
    sdata_flag_t flag,
    hgobj src
) {
    json_t *hs = gobj_hsdata(gobj);

    int ret = 0;
    const char *attr;
    json_t *jn_value;
    json_object_foreach(kw, attr, jn_value) {
        const sdata_desc_t *it = gobj_attr_desc(gobj, attr, TRUE);
        if(!it) {
            continue;
        }
        if(!(flag == (sdata_flag_t)-1 || (it->flag & flag))) {
            continue;
        }
        ret += json2item(gobj, hs, it, jn_value);
    }

    JSON_DECREF(kw);
    return ret;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_user_data(
    hgobj gobj,
    const char *name,
    json_t *value // owned
)
{
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    return json_object_set_new(((gobj_t *)gobj)->jn_user_data, name, value);
}

/***************************************************************************
 *  ATTR: Get hsdata of inherited attribute.
 ***************************************************************************/
PRIVATE json_t *gobj_hsdata2(gobj_t *gobj, const char *name, BOOL verbose)
{
    if(gobj_has_attr(gobj, name)) {
        return gobj_hsdata(gobj);
    } else if(gobj && gobj->bottom_gobj) {
        return gobj_hsdata2(gobj->bottom_gobj, name, verbose);
    }
    if(verbose) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "GClass Attribute NOT FOUND",
            "gclass",       "%s", gobj_gclass_name(gobj),
            "attr",         "%s", name,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  ATTR: read str
 ***************************************************************************/
PUBLIC const char *gobj_read_str_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_string_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return NULL;
}

/***************************************************************************
 *  ATTR: read bool
 ***************************************************************************/
PUBLIC BOOL gobj_read_bool_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_boolean_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_int_t gobj_read_integer_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_integer_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC double gobj_read_real_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return json_real_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC json_t *gobj_read_json_attr(hgobj gobj_, const char *name) // WARNING return its NOT YOURS
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        json_t *jn_value = json_object_get(hs, name);
        return jn_value;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
PUBLIC void *gobj_read_pointer_attr(hgobj gobj_, const char *name)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        if(gobj->gclass->gmt->mt_reading) {
            if(!(gobj->obflag & obflag_destroyed)) {
                gobj->gclass->gmt->mt_reading(gobj, name);
            }
        }
        const json_t *jn_value = json_object_get(hs, name);
        return (void *)(size_t)json_integer_value(jn_value);
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return 0;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_str_attr(hgobj gobj_, const char *name, const char *value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_string(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_bool_attr(hgobj gobj_, const char *name, BOOL value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_boolean(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_integer_attr(hgobj gobj_, const char *name, json_int_t value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_integer(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_real_attr(hgobj gobj_, const char *name, double value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_real(value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  ATTR: write.  WARNING json is incref! (new V2)
 ***************************************************************************/
PUBLIC int gobj_write_json_attr(hgobj gobj_, const char *name, json_t *jn_value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set(hs, name, jn_value);
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    JSON_INCREF(jn_value); // value is incref always, in error case too
    return -1;
}

/***************************************************************************
 *  ATTR: write.  WARNING json is NOT incref
 ***************************************************************************/
PUBLIC int gobj_write_new_json_attr(hgobj gobj_, const char *name, json_t *jn_value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, jn_value);
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    JSON_DECREF(jn_value);
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
PUBLIC int gobj_write_pointer_attr(hgobj gobj_, const char *name, void *value)
{
    gobj_t *gobj = gobj_;

    json_t *hs = gobj_hsdata2(gobj, name, FALSE);
    if(hs) {
        int ret = json_object_set_new(hs, name, json_integer((json_int_t)(size_t)value));
        if(gobj->gclass->gmt->mt_writing) {
            if((gobj->obflag & obflag_created) && !(gobj->obflag & obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj->gclass->gmt->mt_writing(gobj, name);
            }
        }
        return ret;
    }

    gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "GClass Attribute NOT FOUND",
        "gclass",       "%s", gobj_gclass_name(gobj),
        "attr",         "%s", name,
        NULL
    );
    return -1;
}




                    /*---------------------------------*
                     *  Operational functions
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *gobj_check_required_attrs(gobj_t *gobj)
{
    json_t *hs = gobj_hsdata(gobj);
    json_t *jn_required_attrs = json_array();

    const sdata_desc_t *it = gobj->gclass->tattr_desc;
    while(it->name) {
        if(it->flag & SDF_REQUIRED) {
            const json_t *jn_value = json_object_get(hs, it->name);
            switch(it->type) {
                case DTP_STRING:
                    if(empty_string(json_string_value(jn_value))) {
                        json_array_append_new(jn_required_attrs, json_string(it->name));
                    }
                    break;
                case DTP_LIST:
                case DTP_DICT:
                case DTP_JSON:
                    if(empty_json(jn_value)) {
                        json_array_append_new(jn_required_attrs, json_string(it->name));
                    }
                    break;
                case DTP_POINTER:
                    if(!json_integer_value(jn_value)) {
                        json_array_append_new(jn_required_attrs, json_string(it->name));
                    }
                    break;
                case DTP_BOOLEAN:
                case DTP_INTEGER:
                case DTP_REAL:
                    break;
            }
        }
        it++;
    }
    if(json_array_size(jn_required_attrs)==0) {
        JSON_DECREF(jn_required_attrs)
    }
    return jn_required_attrs;
}

/***************************************************************************
 *  Execute global method "start" of the gobj
 ***************************************************************************/
PUBLIC int gobj_start(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->running) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj ALREADY RUNNING",
            NULL
        );
        return -1;
    }
    if(gobj->disabled) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj DISABLED",
            NULL
        );
        return -1;
    }

    /*
     *  Check required attributes.
     */
    json_t *jn_required_attrs = gobj_check_required_attrs(gobj);
    if(jn_required_attrs) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "Cannot start without all required attributes",
            "attrs",        "%j", jn_required_attrs,
            NULL
        );
        JSON_DECREF(jn_required_attrs)
        return -1;
    }

    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏺ ⏺ start: %s",
            gobj_full_name(gobj)
        );
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_START, gobj);
//    }

    gobj->running = TRUE;

    int ret = 0;
    if(gobj->gclass->gmt->mt_start) {
        ret = gobj->gclass->gmt->mt_start(gobj);
    }
    return ret;
}

/***************************************************************************
 *  Start all childs of the gobj.
 ***************************************************************************/
PRIVATE int cb_start_child(hgobj child_, void *user_data)
{
    gobj_t *child = child_;
    if(child->gclass->gclass_flag & gcflag_manual_start) {
        return 0;
    }
    if(!gobj_is_running(child) && !gobj_is_disabled(child)) {
        gobj_start(child);
    }
    return 0;
}
PUBLIC int gobj_start_childs(hgobj gobj)
{
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }

    return gobj_walk_gobj_childs(gobj, WALK_FIRST2LAST, cb_start_child, 0);
}

/***************************************************************************
 *  Start this gobj and all childs tree of the gobj.
 ***************************************************************************/
PRIVATE int cb_start_child_tree(hgobj child_, void *user_data)
{
    gobj_t *child = child_;
    if(child->gclass->gclass_flag & gcflag_manual_start) {
        return 1;
    }
    if(gobj_is_disabled(child)) {
        return 1;
    }
    if(!gobj_is_running(child)) {
        gobj_start(child);
    }
    return 0;
}
PUBLIC int gobj_start_tree(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏺ ⏺ ⏺ ⏺ start_tree: %s",
            gobj_full_name(gobj)
        );
    }

    if((gobj->gclass->gclass_flag & gcflag_manual_start)) {
        return 0;
    } else if(gobj->disabled) {
        return 0;
    } else {
        if(!gobj->running) {
            gobj_start(gobj);
        }
    }
    return gobj_walk_gobj_childs_tree(gobj, WALK_TOP2BOTTOM, cb_start_child_tree, 0);
}

/***************************************************************************
 *  Execute global method "stop" of the gobj
 ***************************************************************************/
PUBLIC int gobj_stop(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(!gobj->running) {
        if(!gobj_is_shutdowning()) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
                "msg",          "%s", "GObj NOT RUNNING",
                NULL
            );
        }
        return -1;
    }
    if(gobj->playing) {
        // It's auto-stopping but display error (magic but warn!).
        gobj_log_info(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj stopping without previous pause",
            NULL
        );
        gobj_pause(gobj);
    }

    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏹ ⏹ stop: %s",
            gobj_full_name(gobj)
        );
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_STOP, gobj);
//    }

    gobj->running = FALSE;

    int ret = 0;
    if(gobj->gclass->gmt->mt_stop) {
        ret = gobj->gclass->gmt->mt_stop(gobj);
    }

    return ret;
}

/***************************************************************************
 *  Stop all childs of the gobj.
 ***************************************************************************/
PRIVATE int cb_stop_child(hgobj child, void *user_data)
{
    if(gobj_is_running(child)) {
        gobj_stop(child);
    }
    return 0;
}
PUBLIC int gobj_stop_childs(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    return gobj_walk_gobj_childs(gobj, WALK_FIRST2LAST, cb_stop_child, 0);
}

/***************************************************************************
 *  Stop this gobj and all childs tree of the gobj.
 ***************************************************************************/
PUBLIC int gobj_stop_tree(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(__trace_gobj_start_stop__(gobj)) {
        trace_machine("⏹ ⏹ ⏹ ⏹ stop_tree: %s",
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_running(gobj)) {
        gobj_stop(gobj);
    }
    return gobj_walk_gobj_childs_tree(gobj, WALK_TOP2BOTTOM, cb_stop_child, 0);
}

/***************************************************************************
 *  Execute global method "play" of the gobj
 ***************************************************************************/
PUBLIC int gobj_play(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(gobj->playing) {
        gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj ALREADY PLAYING",
            NULL
        );
        return -1;
    }
    if(gobj->disabled) {
        gobj_log_warning(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj DISABLED",
            NULL
        );
        return -1;
    }
    if(!gobj_is_running(gobj)) {
        if(!(gobj->gclass->gclass_flag & gcflag_required_start_to_play)) {
            // Default: It's auto-starting but display error (magic but warn!).
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
                "msg",          "%s", "GObj playing without previous start",
                NULL
            );
            gobj_start(gobj);
        } else {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
                "msg",          "%s", "Cannot play, start not done",
                NULL
            );
            return -1;
        }
    }

    if(__trace_gobj_start_stop__(gobj)) {
        if(!is_machine_not_tracing(gobj)) {
            trace_machine("⏯ ⏯ play: %s",
                gobj_full_name(gobj)
            );
        }
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_PLAY, gobj);
//    }
    gobj->playing = TRUE;

    if(gobj->gclass->gmt->mt_play) {
        int ret = gobj->gclass->gmt->mt_play(gobj);
        if(ret < 0) {
            gobj->playing = FALSE;
        }
        return ret;
    } else {
        return 0;
    }
}

/***************************************************************************
 *  Execute global method "pause" of the gobj
 ***************************************************************************/
PUBLIC int gobj_pause(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->obflag & obflag_destroying) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj destroying",
            NULL
        );
        return -1;
    }
    if(!gobj->playing) {
        gobj_log_info(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj NOT PLAYING",
            NULL
        );
        return -1;
    }

    if(__trace_gobj_start_stop__(gobj)) {
        if(!is_machine_not_tracing(gobj)) {
            trace_machine("⏸ ⏸ pause: %s",
                gobj_full_name(gobj)
            );
        }
    }
//    if(__trace_gobj_monitor__(gobj)) {
//        monitor_gobj(MTOR_GOBJ_PAUSE, gobj);
//    }
    gobj->playing = FALSE;

    if(gobj->gclass->gmt->mt_pause) {
        return gobj->gclass->gmt->mt_pause(gobj);
    } else {
        return 0;
    }

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gobj_t * _deep_bottom_gobj(gobj_t *gobj)
{
    if(gobj->bottom_gobj) {
        return _deep_bottom_gobj(gobj->bottom_gobj);
    } else {
        return gobj;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_disable(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(gobj->disabled) {
        gobj_log_info(0, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj ALREADY disabled",
            NULL
        );
        return -1;
    }
    gobj->disabled = TRUE;
    if(gobj->gclass->gmt->mt_disable) {
        return gobj->gclass->gmt->mt_disable(gobj);
    } else {
        return gobj_stop_tree(gobj);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_enable(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return -1;
    }
    if(!gobj->disabled) {
        gobj_log_info(0, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
            "msg",          "%s", "GObj NOT disabled",
            NULL
        );
        return -1;
    }
    gobj->disabled = FALSE;
    if(gobj->gclass->gmt->mt_enable) {
        return gobj->gclass->gmt->mt_enable(gobj);
    } else {
        return gobj_start_tree(gobj);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_set_yuno_must_die(void)
{
    __yuno_must_die__ = TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_get_yuno_must_die(void)
{
    return __yuno_must_die__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void gobj_set_exit_code(int exit_code)
{
    __exit_code__ = exit_code;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_get_exit_code(void)
{
    return __exit_code__;
}

/***************************************************************************
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PUBLIC int gobj_autostart_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(jn_services, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        if(gobj->gobj_flag & gobj_flag_autostart) {
            if(gobj->gclass->gmt->mt_play) { // HACK checking mt_play because if exists he have the power on!
                gobj_start(gobj);
            } else {
                gobj_start_tree(gobj);
            }
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_autoplay_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(jn_services, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        if(gobj->gobj_flag & gobj_flag_autoplay) {
            if(gobj->gclass->gmt->mt_play) { // HACK checking mt_play because if exists he have the power on!
                gobj_start(gobj);
            } else {
                gobj_start_tree(gobj);
            }
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_stop_services(void)
{
    const char *key; json_t *jn_service;
    json_object_foreach(jn_services, key, jn_service) {
        gobj_t *gobj = (gobj_t *)(size_t)json_integer_value(jn_service);
        if(gobj->gobj_flag & gobj_flag_yuno) {
            continue;
        }
        gobj_log_debug(0,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_STARTUP,
            "msg",          "%s", "STOP service",
            "service",      "%s", gobj_short_name(gobj),
            NULL
        );
        if(gobj_is_playing(gobj)) {
            gobj_pause(gobj);
        }
        gobj_stop_tree(gobj);
    }

    return 0;
}

/***************************************************************************
 *  Execute a command of gclass command table
 ***************************************************************************/
PUBLIC json_t *gobj_command( // With AUTHZ
    hgobj gobj_,
    const char *command,
    json_t *kw,
    hgobj src
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*-----------------------------------------------*
     *  Trace
     *-----------------------------------------------*/
    BOOL tracea = is_machine_tracing(gobj) && !is_machine_not_tracing(src);
    if(tracea) {
        trace_machine("🌀🌀 mach(%s%s), cmd: %s, src: %s",
            (!gobj->running)?"!!":"",
            gobj_short_name(gobj),
            command,
            gobj_short_name(src)
        );
        gobj_trace_json(gobj, kw, "command kw");
    }

    /*-----------------------------------------------*
     *  The local mt_command_parser has preference
     *-----------------------------------------------*/
    if(gobj->gclass->gmt->mt_command_parser) {
        return gobj->gclass->gmt->mt_command_parser(gobj, command, kw, src);
    }

    /*-----------------------------------------------*
     *  If it has command_table
     *  then use the global command parser
     *-----------------------------------------------*/
    if(gobj->gclass->command_table) {
        if(__global_command_parser_fn__) {
            return __global_command_parser_fn__(gobj, command, kw, src);
        } else {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Global command parser function not available",
                "command",      "%s", command?command:"",
                NULL
            );
            KW_DECREF(kw)
            return 0;
        }
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Command table not available",
            "command",      "%s", command?command:"",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *gobj_stats(hgobj gobj_, const char *stats, json_t *kw, hgobj src)
{
     gobj_t * gobj = gobj_;

    if(!gobj || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*--------------------------------------*
     *  The local mt_stats has preference
     *--------------------------------------*/
    if(gobj->gclass->gmt->mt_stats) {
        return gobj->gclass->gmt->mt_stats(gobj, stats, kw, src);
    }

    /*-----------------------------------------------*
     *  Then use the global stats parser
     *-----------------------------------------------*/
    if(__global_stats_parser_fn__) {
        return __global_stats_parser_fn__(gobj, stats, kw, src);
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Stats parser not available",
            "stats",        "%s", stats?stats:"",
            NULL
        );
        KW_DECREF(kw)
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *build_command_response( // // old build_webix()
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned, if null then not set
    json_t *jn_schema,  // owned, if null then not set
    json_t *jn_data     // owned, if null then not set
) {
    if(!jn_comment) {
        jn_comment = json_string("");
    }
    if(!jn_schema) {
        jn_schema = json_null();
    }
    if(!jn_data) {
        jn_data = json_null();
    }

    json_t *response = json_object();
    json_object_set_new(response, "result", json_integer(result));
    if(jn_comment) {
        json_object_set_new(response, "comment", jn_comment);
    }
    if(jn_schema) {
        json_object_set_new(response, "schema", jn_schema);
    }
    if(jn_data) {
        json_object_set_new(response, "data", jn_data);
    }
    return response;
}

/***************************************************************************
 *  Set manually the bottom gobj
 ***************************************************************************/
PUBLIC hgobj gobj_set_bottom_gobj(hgobj gobj_, hgobj bottom_gobj)
{
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    if(is_machine_tracing(gobj)) {
        trace_machine("🔽 set_bottom_gobj('%s') = '%s'",
            gobj_short_name(gobj),
            bottom_gobj?gobj_short_name(bottom_gobj):""
        );
    }
    if(gobj->bottom_gobj) {
        /*
         *  En los gobj con manual start ellos suelen poner su bottom gobj.
         *  No lo consideramos un error.
         *  Ej: emailsender -> curl
         *      y luego va e intenta poner emailsender -> IOGate
         *      porque está definido con tree, y no tiene en cuenta la creación de un bottom interno.
         *
         */
        if(bottom_gobj) {
            gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "bottom_gobj already set",
                "prev_gobj",    "%s", gobj_full_name(gobj->bottom_gobj),
                "new_gobj",     "%s", gobj_full_name(bottom_gobj),
                NULL
            );
        }
        // anyway set -> NO! -> the already set has preference. New 8-10-2016
        // anyway set -> YES! -> the new set has preference. New 27-Jan-2017
        //return 0;
    }
    gobj->bottom_gobj = bottom_gobj;
    return 0;
}

/***************************************************************************
 *  Return the last bottom gobj of gobj tree.
 *  Useful when there is a stack of gobjs acting as a unit.
 *  Filled by gobj_create_tree0() function.
 ***************************************************************************/
PUBLIC hgobj gobj_last_bottom_gobj(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(gobj->bottom_gobj) {
        return _deep_bottom_gobj(gobj->bottom_gobj);
    } else {
        return 0;
    }
}

/***************************************************************************
 *  Return the next bottom gobj of the gobj.
 *  See gobj_last_bottom_gobj()
 ***************************************************************************/
PUBLIC hgobj gobj_bottom_gobj(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if (!gobj) {
        return 0;
    }
    return gobj->bottom_gobj;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_default_service(void)
{
    return __default_service__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_find_service(const char *service, BOOL verbose)
{
    if(empty_string(service)) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "service EMPTY",
                NULL
            );
        }
        return NULL;
    }

    if(strcasecmp(service, "__default_service__")==0) {
        return __default_service__;
    }
    if(strcasecmp(service, "__yuno__")==0 || strcasecmp(service, "__root__")==0) {
        return gobj_yuno();
    }

    json_t *o = json_object_get(jn_services, service);
    if(!o) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "service NOT FOUND",
                "service",      "%s", service,
                NULL
            );
        }
        return NULL;
    }

    return (hgobj)(size_t)json_integer_value(o);
}

/***************************************************************************
 *  Find a gobj by path
 ***************************************************************************/
PUBLIC hgobj gobj_find_gobj(const char *path)
{
    if(empty_string(path)) {
        return 0;
    }
    /*
     *  WARNING code repeated
     */
    if(strcasecmp(path, "__default_service__")==0) {
        return __default_service__;
    }
    if(strcasecmp(path, "__yuno__")==0 || strcasecmp(path, "__root__")==0) {
        return __yuno__;
    }

    return gobj_search_path(__yuno__, path);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj gobj_first_child(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    return dl_first(&gobj->dl_childs);
}

/***************************************************************************
 *  Return last child, last to `child`
 ***************************************************************************/
PUBLIC hgobj gobj_last_child(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    return dl_last(&gobj->dl_childs);
}


/***************************************************************************
 *  Return next child, next to `child`
 ***************************************************************************/
PUBLIC hgobj gobj_next_child(hgobj child)
{
    gobj_t * gobj = child;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    return dl_next(gobj);
}

/***************************************************************************
 *  Return prev child, prev to `child`
 ***************************************************************************/
PUBLIC hgobj gobj_prev_child(hgobj child)
{
    gobj_t * gobj = child;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    return dl_prev(gobj);
}

/***************************************************************************
 *  Return the child of gobj by name.
 *  The first found is returned.
 ***************************************************************************/
PUBLIC hgobj gobj_child_by_name(hgobj gobj_, const char *name)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }
    if(empty_string(name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "name NULL",
            NULL
        );
        return 0;
    }

    gobj_t *child = dl_first(&gobj->dl_childs);
    while(child) {
        if(!(child->obflag & (obflag_destroyed|obflag_destroying))) {
            const char *name_ = gobj_name(child);
            if(name_ && strcmp(name_, name)==0) {
                return child;
            }
        }
        /*
         *  Next
         */
        child = dl_next(child);
    }
    return 0;
}

/***************************************************************************
 *  Return the size of childs of gobj
 ***************************************************************************/
PUBLIC size_t gobj_child_size(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    return dl_size(&gobj->dl_childs);
}

/***************************************************************************
 *  Match child.
 ***************************************************************************/
PRIVATE BOOL match_child(
    gobj_t *child,
    json_t *jn_filter_  // NOT owned
)
{
    const char *__inherited_gclass_name__ = kw_get_str(child, jn_filter_, "__inherited_gclass_name__", 0, 0);
    const char *__gclass_name__ = kw_get_str(child, jn_filter_, "__gclass_name__", 0, 0);
    const char *__gobj_name__ = kw_get_str(child, jn_filter_, "__gobj_name__", 0, 0);
    const char *__prefix_gobj_name__ = kw_get_str(child, jn_filter_, "__prefix_gobj_name__", 0, 0);
    const char *__state__ = kw_get_str(child, jn_filter_, "__state__", 0, 0);

    /*
     *  Delete the system keys of the jn_filter used in find loop
     */
    json_t *jn_filter = json_deep_copy(jn_filter_);
    if(__inherited_gclass_name__) {
        json_object_del(jn_filter, "__inherited_gclass_name__");
    }
    if(__gclass_name__) {
        json_object_del(jn_filter, "__gclass_name__");
    }
    if(__gobj_name__) {
        json_object_del(jn_filter, "__gobj_name__");
    }
    if(__prefix_gobj_name__) {
        json_object_del(jn_filter, "__prefix_gobj_name__");
    }
    if(__state__) {
        json_object_del(jn_filter, "__state__");
    }

    if(kw_has_key(jn_filter_, "__disabled__")) {
        BOOL disabled = kw_get_bool(child, jn_filter_, "__disabled__", 0, 0);
        if(disabled && !gobj_is_disabled(child)) {
            json_decref(jn_filter); // clone
            return FALSE;
        }
        if(!disabled && gobj_is_disabled(child)) {
            json_decref(jn_filter); // clone
            return FALSE;
        }
        json_object_del(jn_filter, "__disabled__");
    }

   if(!empty_string(__inherited_gclass_name__)) {
        if(!gobj_typeof_inherited_gclass(child, __inherited_gclass_name__)) {
            json_decref(jn_filter); // clone
            return FALSE;
        }
    }
    if(!empty_string(__gclass_name__)) {
        if(!gobj_typeof_gclass(child, __gclass_name__)) {
            json_decref(jn_filter); // clone
            return FALSE;
        }
    }
    const char *child_name = gobj_name(child);
    if(!empty_string(__gobj_name__)) {
        if(strcmp(__gobj_name__, child_name)!=0) {
            json_decref(jn_filter); // clone
            return FALSE;
        }
    }
    if(!empty_string(__prefix_gobj_name__)) {
        if(strncmp(__prefix_gobj_name__, child_name, strlen(__prefix_gobj_name__))!=0) {
            json_decref(jn_filter); // clone
            return FALSE;
        }
    }
    if(!empty_string(__state__)) {
        if(strcasecmp(__state__, gobj_current_state(child))!=0) {
            json_decref(jn_filter); // clone
            return FALSE;
        }
    }

    const char *key;
    json_t *jn_value;

    BOOL matched = TRUE;
    json_object_foreach(jn_filter, key, jn_value) {
        json_t *hs = gobj_hsdata2(child, key, FALSE);
        if(hs) {
            json_t *jn_var1 = json_object_get(hs, key);
            int cmp = cmp_two_simple_json(jn_var1, jn_value);
            JSON_DECREF(jn_var1);
            if(cmp!=0) {
                matched = FALSE;
                break;
            }
        }
    }

    json_decref(jn_filter); // clone
    return matched;
}

/***************************************************************************
 *  Returns the first matched child.
 ***************************************************************************/
PUBLIC hgobj gobj_find_child(
    hgobj gobj,
    json_t *jn_filter  // not owned
)
{
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        JSON_DECREF(jn_filter);
        return 0;
    }

    gobj_t *child = gobj_first_child(gobj);

    while(child) {
        if(match_child(child, jn_filter)) {
            JSON_DECREF(jn_filter);
            return child;
        }
        child = gobj_next_child(child);
    }

    JSON_DECREF(jn_filter);
    return 0;
}

/***************************************************************************
 *  Return the object searched by path.
 *  The separator of tree's gobj must be '`'
 ***************************************************************************/
PUBLIC hgobj gobj_search_path(hgobj gobj_, const char *path_)
{
    gobj_t *gobj = gobj_;
    // TODO check, new function implementation
    char delimiter[2] = {'`',0};
    if(empty_string(path_) || !gobj) {
        return 0;
    }
    char *path = GBMEM_STRDUP(path_);

    int list_size = 0;
    const char **ss = split2(path, delimiter, &list_size);

    hgobj child = 0;
    const char *key = 0;
    for(int i=0; i<list_size && gobj; i++) {
        key = *(ss + i);

        const char *gclass_name_ = 0;
        char *gobj_name_ = strchr(key, '^');
        if(gobj_name_) {
            gclass_name_ = key;
            *gobj_name_ = 0;
            gobj_name_++;
        } else {
            gobj_name_ = (char *)key;
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gobj_name__", gobj_name_
        );
        if(gclass_name_) {
            json_object_set_new(jn_filter, "__gclass_name__", json_string(gclass_name_));
        }

        child = gobj_find_child(
            gobj,
            jn_filter
        );
        if(!child) {
            break;
        }
        gobj = child;
    }

    split_free2(ss);
    GBMEM_FREE(path)
    return child;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_walk_gobj_childs(
    hgobj gobj_,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    return rc_walk_by_list(&gobj->dl_childs, walk_type, cb_walking, user_data);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_walk_gobj_childs_tree(
    hgobj gobj_,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    gobj_t *gobj = gobj_;
    if(!gobj) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        return 0;
    }

    return rc_walk_by_tree(&gobj->dl_childs, walk_type, cb_walking, user_data);
}

/***************************************************************
 *  Walking
 ***************************************************************/
PUBLIC int rc_walk_by_list(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    gobj_t *child;
    if(walk_type & WALK_LAST2FIRST) {
        child = dl_last(iter);
    } else {
        child = dl_first(iter);
    }

    while(child) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        gobj_t *next;
        if(walk_type & WALK_LAST2FIRST) {
            next = dl_prev(child);
        } else {
            next = dl_next(child);
        }

        int ret = (cb_walking)(child, user_data);
        if(ret < 0) {
            return ret;
        }

        child = next;
    }

    return 0;
}

/***************************************************************
 *  Walking
 *  If cb_walking return negative, the iter will stop.
 *  Valid options:
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 ***************************************************************/
PRIVATE int rc_walk_by_level(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    /*
     *  First my childs
     */
    int ret = rc_walk_by_list(iter, walk_type, cb_walking, user_data);
    if(ret < 0) {
        return ret;
    }

    /*
     *  Now child's childs
     */
    gobj_t *child;
    if(walk_type & WALK_LAST2FIRST) {
        child = dl_last(iter);
    } else {
        child = dl_first(iter);
    }

    while(child) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        gobj_t *next;
        if(walk_type & WALK_LAST2FIRST) {
            next = dl_prev(child);
        } else {
            next = dl_next(child);
        }

        dl_list_t *dl_child_list = &child->dl_childs;
        ret = rc_walk_by_level(dl_child_list, walk_type, cb_walking, user_data);
        if(ret < 0) {
            return ret;
        }

        child = next;
    }

    return ret;
}

/***************************************************************
 *  Walking
 *  If cb_walking return negative, the iter will stop.
 *                return 0, continue
 *                return positive, step current branch (only in WALK_TOP2BOTTOM)
 *  Valid options:
 *      WALK_TOP2BOTTOM
 *      or
 *      WALK_BOTTOM2TOP
 *      or
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 ***************************************************************/
PUBLIC int rc_walk_by_tree(
    dl_list_t *iter,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data
) {
    if(walk_type & WALK_BYLEVEL) {
        return rc_walk_by_level(iter, walk_type, cb_walking, user_data);
    }

    gobj_t *child;
    if(walk_type & WALK_BOTTOM2TOP) {
        child = dl_last(iter);
    } else {
        child = dl_first(iter);
    }

    while(child) {
        /*
         *  Get next item before calling callback.
         *  This let destroy the current item,
         *  but NOT more ones.
         */
        gobj_t *next;
        if(walk_type & WALK_BOTTOM2TOP) {
            next = dl_prev(child);
        } else {
            next = dl_next(child);
        }

        if(walk_type & WALK_BOTTOM2TOP) {
            dl_list_t *dl_child_list = &child->dl_childs;
            int ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data);
            if(ret < 0) {
                return ret;
            }

            ret = (cb_walking)(child, user_data);
            if(ret < 0) {
                return ret;
            }
        } else {
            int ret = (cb_walking)(child, user_data);
            if(ret < 0) {
                return ret;
            } else if(ret == 0) {
                dl_list_t *dl_child_list = &child->dl_childs;
                ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data);
                if(ret < 0) {
                    return ret;
                }
            } else {
                // positive, continue next
            }
        }

        child = next;
    }

    return 0;
}



                    /*---------------------------------*
                     *      Info functions
                     *---------------------------------*/




/***************************************************************************
 *  Return yuno, the grandfather
 ***************************************************************************/
PUBLIC hgobj gobj_yuno(void)
{
    if(!__yuno__ || __yuno__->obflag & obflag_destroyed) {
        return 0;
    }
    return __yuno__;
}

/***************************************************************************
 *  Return name
 ***************************************************************************/
PUBLIC const char * gobj_name(hgobj hgobj)
{
    gobj_t *gobj = (gobj_t *)hgobj;
    return gobj? gobj->gobj_name: "???";
}

/***************************************************************************
 *  Return mach name. Same as gclass name.
 ***************************************************************************/
PUBLIC gclass_name_t gobj_gclass_name(hgobj hgobj)
{
    gobj_t *gobj = (gobj_t *)hgobj;
    return (gobj && gobj->gclass)? gobj->gclass->gclass_name: "???";
}

/***************************************************************************
 *  Return full name
 ***************************************************************************/
PUBLIC const char * gobj_full_name(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(!gobj || gobj->obflag & obflag_destroyed) {
        return "???";
    }

    if(!gobj->full_name) {
        #define SIZEBUFTEMP (8*1024)
        gobj_t *parent = gobj;
        char *bf = malloc(SIZEBUFTEMP);
        if(bf) {
            size_t ln;
            memset(bf, 0, SIZEBUFTEMP);
            while(parent) {
                char pp[256];
                const char *format;
                if(strlen(bf))
                    format = "%s`";
                else
                    format = "%s";
                snprintf(pp, sizeof(pp), format, gobj_short_name(parent));
                ln = strlen(pp);
                if(ln + strlen(bf) < SIZEBUFTEMP) {
                    memmove(bf+ln, bf, strlen(bf));
                    memmove(bf, pp, ln);
                } else {
                    break;
                }
                if(parent == parent->parent) {
                    break;
                }
                parent = parent->parent;
            }
            gobj->full_name = gobj_strdup(bf);
            free(bf);
        }

    }
    return gobj->full_name;
}

/***************************************************************************
 *  Return short name (gclass^name)
 ***************************************************************************/
PUBLIC const char * gobj_short_name(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(!gobj || gobj->obflag & obflag_destroyed) {
        return "???";
    }

    if(!gobj->short_name) {
        char temp[256];
        snprintf(temp, sizeof(temp),
            "%s^%s",
            gobj_gclass_name(gobj),
            gobj_name(gobj)
        );
        gobj->short_name = gobj_strdup(temp);
    }
    return gobj->short_name;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t * gobj_global_variables(void)
{
    return json_object(); // TODO no deberían cogerse del __root__???
//    return json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
//        "__node_owner__", __node_owner__,
//        "__realm_id__", __realm_id__,
//        "__realm_owner__", __realm_owner__,
//        "__realm_role__", __realm_role__,
//        "__realm_name__", __realm_name__,
//        "__realm_env__", __realm_env__,
//        "__yuno_id__", __yuno_id__,
//        "__yuno_role__", __yuno_role__,
//        "__yuno_name__", __yuno_name__,
//        "__yuno_tag__", __yuno_tag__,
//        "__yuno_role_plus_name__", __yuno_role_plus_name__,
//        "__yuno_role_plus_id__", __yuno_role_plus_id__,
//        "__hostname__", get_host_name(),
//#ifdef __linux__
//        "__sys_system_name__", sys.sysname,
//        "__sys_node_name__", sys.nodename,
//        "__sys_version__", sys.version,
//        "__sys_release__", sys.release,
//        "__sys_machine__", sys.machine
//#else
//        "__sys_system_name__", "",
//        "__sys_node_name__", "",
//        "__sys_version__", "",
//        "__sys_release__", "",
//        "__sys_machine__", ""
//#endif
//    );
}

/***************************************************************************
 *  Return private data pointer
 ***************************************************************************/
PUBLIC void * gobj_priv_data(hgobj gobj)
{
    return ((gobj_t *)gobj)->priv;
}

/***************************************************************************
 *  Return parent
 ***************************************************************************/
PUBLIC hgobj gobj_parent(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj)
        return NULL;
    return gobj->parent;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_destroying(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj_ || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_running(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(!gobj_ || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
          "function",     "%s", __FUNCTION__,
          "msgset",       "%s", MSGSET_PARAMETER_ERROR,
          "msg",          "%s", "hgobj NULL or DESTROYED",
          NULL
        );
        return FALSE;
    }
    return gobj->running;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_playing(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(!gobj_ || gobj->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        return FALSE;
    }
    return gobj->playing;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_service(hgobj gobj_)
{
    gobj_t *gobj = gobj_;

    if(gobj && (gobj->gobj_flag & (gobj_flag_service))) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_disabled(hgobj gobj)
{
    return ((gobj_t *)gobj)->disabled;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_volatil(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(gobj->gobj_flag & gobj_flag_volatil) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_is_pure_child(hgobj gobj_)
{
    gobj_t *gobj = gobj_;
    if(gobj->gobj_flag & gobj_flag_pure_child) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *  True if gobj is of gclass gclass_name
 ***************************************************************************/
PUBLIC BOOL gobj_typeof_gclass(hgobj gobj, const char *gclass_name)
{
    if(strcasecmp(((gobj_t *)gobj)->gclass->gclass_name, gclass_name)==0)
        return TRUE;
    else
        return FALSE;
}

/***************************************************************************
 *  Is a inherited (bottom) of this gclass?
 ***************************************************************************/
PUBLIC BOOL gobj_typeof_inherited_gclass(hgobj gobj_, const char *gclass_name)
{
    gobj_t * gobj = gobj_;

    while(gobj) {
        if(strcasecmp(gclass_name, gobj->gclass->gclass_name)==0) {
            return TRUE;
        }
        gobj = gobj->bottom_gobj;
    }

    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC size_t get_max_system_memory(void)
{
    return __max_system_memory__;
}
PUBLIC size_t get_cur_system_memory(void)
{
    return __cur_system_memory__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *get_host_name(void)
{
    return __hostname__;
}
PUBLIC const char *get_user_name(void)  // Who started yuno
{
    return __username__;
}

/***************************************************************************
 *  Get names table of sdata flag
 ***************************************************************************/
PUBLIC const char **get_sdata_flag_table(void)
{
    return sdata_flag_names;
}

/***************************************************************************
 *
 *  Return list of objects with gobj's attribute description,
 *  restricted to attributes having SDF_RD|SDF_WR|SDF_STATS|SDF_PERSIST|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS flag.
 *
 *  Esquema tabla webix:

    data:[
        {
            id: integer (index)
            name: string,
            type: string ("real" | "boolean" | "integer" | "string" | "json"),
            flag: string ("SDF_RD",...),
            description: string,
            stats: boolean,
            value: any
        },
        {   Metadata:
            "name", "__state__",
            "name", "__bottom__",
            "name", "__shortname__",
            "name", "__fullname__",
            "name", "__running__",
            "name", "__playing__",
            "name", "__service__",
            "name", "__unique__",
            "name", "__disabled__",
            "name", "__gobj_trace_level__",
            "name", "__gobj_no_trace_level__"
        }
    ]
 ***************************************************************************/
PUBLIC json_t *attr2json(hgobj gobj_)
{
    gobj_t * gobj = gobj_;
    json_t *jn_data = json_array();

    int id = 1;
    const sdata_desc_t *it = gobj->gclass->tattr_desc;
    while(it && it->name) {
        if(it->flag & (SDF_RD|SDF_WR|SDF_STATS|SDF_PERSIST|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS)) {
            char *type;
            if(it->type == DTP_REAL) {
                type = "real";
            } else if(it->type == DTP_BOOLEAN) {
                type = "boolean";
            } else if(it->type == DTP_INTEGER) {
                type = "integer";
            } else if(it->type == DTP_STRING) {
                type = "string";
            } else if(it->type == DTP_JSON) {
                type = "json";
            } else if(it->type == DTP_DICT) {
                type = "dict";
            } else if(it->type == DTP_LIST) {
                type = "list";
            } else {
                it++;
                continue;
            }

            gbuffer_t *gbuf = bits2gbuffer(get_sdata_flag_table(), it->flag);
            char *flag = gbuf?gbuffer_cur_rd_pointer(gbuf):"";
            json_t *attr_dict = json_pack("{s:I, s:s, s:s, s:s, s:s, s:b}",
                "id", (json_int_t)id++,
                "name", it->name,
                "type", type,
                "flag", flag,
                "description", it->description?it->description:"",
                "stats", (it->flag & (SDF_STATS|SDF_RSTATS|SDF_PSTATS))?1:0
            );
            GBUFFER_DECREF(gbuf);

            if(it->type == DTP_REAL) {
                json_object_set_new(
                    attr_dict,
                    "value",
                    json_real(
                        gobj_read_real_attr(gobj, it->name)
                    )
                );
            } else if(it->type == DTP_INTEGER) {
                json_object_set_new(
                    attr_dict,
                    "value",
                    json_integer(
                        gobj_read_integer_attr(gobj, it->name)
                    )
                );
            } else if(it->type == DTP_BOOLEAN) {
                json_object_set_new(
                    attr_dict,
                    "value",
                    json_boolean(
                        gobj_read_bool_attr(gobj, it->name)
                    )
                );
            } else if(it->type == DTP_STRING) {
                json_object_set_new(
                    attr_dict,
                    "value",
                    json_string(
                        gobj_read_str_attr(gobj, it->name)
                    )
                );
            } else if(it->type == DTP_JSON || it->type == DTP_DICT || it->type == DTP_LIST) {
                json_t *jn = gobj_read_json_attr(gobj, it->name);
                char *value = 0;
                if(jn) {
                    value = json_dumps(jn, JSON_ENCODE_ANY|JSON_COMPACT);
                }
                json_object_set_new(
                    attr_dict,
                    "value",
                    json_string(value?value:"")
                );
                if(value) {
                    jsonp_free(value);
                }
            }

            json_array_append_new(jn_data, attr_dict);
        }
        it++;
    }

    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:s}",
            "id", (json_int_t)id++,
            "name", "__state__",
            "type", "string",
            "flag", "",
            "description", "SMachine state of gobj",
            "stats", 0,
            "value", gobj_current_state(gobj)
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:s}",
            "id", (json_int_t)id++,
            "name", "__bottom__",
            "type", "string",
            "flag", "",
            "description", "Bottom gobj",
            "stats", 0,
            "value", gobj->bottom_gobj? gobj_short_name(gobj->bottom_gobj):""
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:s}",
            "id", (json_int_t)id++,
            "name", "__shortname__",
            "type", "string",
            "flag", "",
            "description", "Full name",
            "stats", 0,
            "value", gobj_short_name(gobj)
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:s}",
            "id", (json_int_t)id++,
            "name", "__fullname__",
            "type", "string",
            "flag", "",
            "description", "Full name",
            "stats", 0,
            "value", gobj_full_name(gobj)
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:I}",
            "id", (json_int_t)id++,
            "name", "__running__",
            "type", "boolean",
            "flag", "",
            "description", "Is running the gobj?",
            "stats", 0,
            "value", (json_int_t)(size_t)gobj_is_running(gobj)
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:I}",
            "id", (json_int_t)id++,
            "name", "__playing__",
            "type", "boolean",
            "flag", "",
            "description", "Is playing the gobj?",
            "stats", 0,
            "value", (json_int_t)(size_t)gobj_is_playing(gobj)
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:I}",
            "id", (json_int_t)id++,
            "name", "__service__",
            "type", "boolean",
            "flag", "",
            "description", "Is a service the gobj?",
            "stats", 0,
            "value", (json_int_t)(size_t)gobj_is_service(gobj)
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:I}",
            "id", (json_int_t)id++,
            "name", "__disabled__",
            "type", "boolean",
            "flag", "",
            "description", "Is disabled the gobj?",
            "stats", 0,
            "value", (json_int_t)(size_t)gobj_is_disabled(gobj)
        )
    );
    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:o}",
            "id", (json_int_t)id++,
            "name", "__gobj_trace_level__",
            "type", "array",
            "flag", "",
            "description", "Current trace level",
            "stats", 0,
            "value",  bit2level(
                s_global_trace_level,
                gobj->gclass->s_user_trace_level,
                gobj_trace_level(gobj)
            )
        )
    );

    json_array_append_new(jn_data,
        json_pack("{s:I, s:s, s:s, s:s, s:s, s:b, s:o}",
            "id", (json_int_t)id++,
            "name", "__gobj_no_trace_level__",
            "type", "array",
            "flag", "",
            "description", "Current no trace level",
            "stats", 0,
            "value",  bit2level(
                s_global_trace_level,
                gobj->gclass->s_user_trace_level,
                gobj_trace_no_level(gobj)
            )
        )
    );

    return jn_data;
}




                    /*---------------------------------*
                     *      Events and States
                     *---------------------------------*/





/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_send_event(
    hgobj dst_,
    gobj_event_t event,
    json_t *kw,
    hgobj src_
) {
    if(dst_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj dst NULL",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    gobj_t *src = (gobj_t *)src_;
    gobj_t *dst = (gobj_t *)dst_;
    if(dst->obflag & (obflag_destroyed|obflag_destroying)) {
        gobj_log_error(dst, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", (dst->obflag & obflag_destroyed)? "gobj DESTROYED":"gobj DESTROYING",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    state_t *state = dst->current_state;
    if(!state) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "current_state NULL",
            "gclass_name",  "%s", dst->gclass->gclass_name,
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    /*----------------------------------*
     *  Find the event/action in state
     *----------------------------------*/
    BOOL tracea = is_machine_tracing(dst) && !is_machine_not_tracing(dst) && !is_machine_not_tracing(src);
    __inside__ ++;

    event_action_t *event_action = find_event_action(state, event);
    if(!event_action) {
        if(dst->gclass->gmt->mt_inject_event) {
            __inside__ --;
            if(tracea) {
                trace_machine("🔃 mach(%s%s^%s), st: %s, ev: %s, from(%s%s^%s)",
                    (!dst->running)?"!!":"",
                    gobj_gclass_name(dst), gobj_name(dst),
                    state->state_name,
                    event?event:"",
                    (src && !src->running)?"!!":"",
                    gobj_gclass_name(src), gobj_name(src)
                );
                if(kw) {
                    if(__trace_gobj_ev_kw__(dst)) {
                        if(json_object_size(kw)) {
                            gobj_trace_json(dst, kw, "kw send_event");
                        }
                    }
                }
            }
            return dst->gclass->gmt->mt_inject_event(dst, event, kw, src);
        }

        if(tracea) {
            trace_machine(
                "📛 mach(%s%s^%s), st: %s, ev: %s, 📛📛ERROR Event NOT DEFINED in state📛📛, from(%s%s^%s)",
                (!dst->running)?"!!":"",
                gobj_gclass_name(dst), gobj_name(dst),
                state->state_name,
                event?event:"",
                (src && !src->running)?"!!":"",
                gobj_gclass_name(src), gobj_name(src)
            );
        }
        gobj_log_error(dst, LOG_OPT_TRACE_STACK,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Event NOT DEFINED in state",
            "msg2",          "%s", "📛📛 Event NOT DEFINED in state 📛📛",
            "gclass_name",  "%s", dst->gclass->gclass_name,
            "state_name",   "%s", state->state_name,
            "event",        "%s", event,
            "src",          "%s", gobj_short_name(src),
            NULL
        );

        __inside__ --;

        KW_DECREF(kw)
        return -1;
    }

    /*----------------------------------*
     *      Check AUTHZ
     *----------------------------------*/
//     if(ev_desc->authz & EV_AUTHZ_INJECT) {
//     }

    /*----------------------------------*
     *      Exec the event
     *----------------------------------*/
    if(tracea) {
        trace_machine("🔄 mach(%s%s^%s), st: %s, ev: %s%s%s, from(%s%s^%s)",
            (!dst->running)?"!!":"",
            gobj_gclass_name(dst), gobj_name(dst),
            state->state_name,
            On_Black RBlue,
            event?event:"",
            Color_Off,
            (src && !src->running)?"!!":"",
            gobj_gclass_name(src), gobj_name(src)
        );
        if(kw) {
            if(__trace_gobj_ev_kw__(dst)) {
                if(json_object_size(kw)) {
                    gobj_trace_json(dst, kw, "kw exec event: %s", event?event:"");
                }
            }
        }
    }

    /*
     *  IMPORTANT HACK
     *  Set new state BEFORE run 'action'
     *
     *  The next state is changed before executing the action.
     *  If you don’t like this behavior, set the next-state to NULL
     *  and use change_state() to change the state inside the actions.
     */
    if(event_action->next_state) {
        gobj_change_state(dst, event_action->next_state);
    }

    int ret = -1;
    if(event_action->action) {
        // Execute the action
        ret = (*event_action->action)(dst, event, kw, src);
    } else {
        // No action, there is nothing amiss!.
        KW_DECREF(kw)
    }

    if(tracea && !(dst->obflag & obflag_destroyed)) {
        trace_machine("<- mach(%s%s^%s), st: %s, ev: %s, ret: %d",
            (!dst->running)?"!!":"",
            gobj_gclass_name(dst), gobj_name(dst),
            dst->current_state->state_name,
            event?event:"",
            ret
        );
    }

    __inside__ --;

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_change_state(
    hgobj hgobj,
    gobj_state_t state_name
) {
    if(hgobj == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            NULL
        );
        return FALSE;
    }

    gobj_t *gobj = (gobj_t *)hgobj;

    if(gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYED",
            NULL
        );
        return FALSE;
    }

    if(gobj->current_state->state_name == state_name) {
        return FALSE;
    }
    state_t *new_state = find_state(gobj->gclass, state_name);
    if(!new_state) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "state unknown",
            "gclass_name",  "%s", gobj->gclass->gclass_name,
            "state_name",   "%s", state_name,
            NULL
        );
        return FALSE;
    }
    gobj->last_state = gobj->current_state;
    gobj->current_state = new_state;

    BOOL tracea = is_machine_tracing(gobj);
    BOOL tracea_states = __trace_gobj_states__(gobj)?TRUE:FALSE;
    if(tracea || tracea_states) {
        trace_machine("🔀🔀 mach(%s%s^%s), st(%s%s%s)",
            (!gobj->running)?"!!":"",
            gobj_gclass_name(gobj), gobj_name(gobj),
            On_Black RGreen,
            gobj_current_state(gobj),
            Color_Off
        );
    }

    json_t *kw_st = json_object();
    json_object_set_new(
        kw_st,
        "previous_state",
        json_string(gobj->last_state->state_name)
    );
    json_object_set_new(
        kw_st,
        "current_state",
        json_string(gobj->current_state->state_name)
    );

    if(gobj->gclass->gmt->mt_state_changed) {
        gobj->gclass->gmt->mt_state_changed(gobj, EV_STATE_CHANGED, kw_st);
    } else {
        gobj_publish_event(gobj, EV_STATE_CHANGED, kw_st);
    }

    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC gobj_state_t gobj_current_state(hgobj hgobj)
{
    if(hgobj == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            NULL
        );
        return "";
    }
    gobj_t *gobj = (gobj_t *)hgobj;

    return gobj->current_state->state_name;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL gobj_has_input_event(hgobj gobj_, gobj_event_t event)
{
    if(gobj_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            "event",        "%s", event,
            NULL
        );
        return FALSE;
    }
    gobj_t *gobj = (gobj_t *)gobj_;
    if(find_event_action(gobj->current_state, event)) {
        return TRUE;
    }

    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC event_type_t *gobj_event_type(hgobj gobj_, gobj_event_t event, event_flag_t event_flag)
{
    if(gobj_ == NULL) {
        gobj_log_error(NULL, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL",
            "event",        "%s", event,
            NULL
        );
        return NULL;
    }
    gobj_t *gobj = (gobj_t *)gobj_;

    event_t *event_ = dl_first(&gobj->gclass->dl_events);
    while(event_) {
        if(event_->event_type.event && event_->event_type.event == event) {
            if(event_flag && (event_->event_type.event_flag & event_flag)) {
                return &event_->event_type;
            }
        }
        event_ = dl_next(event_);
    }

    /*
     *  Check global (gobj) output events
     */
    event_ = dl_first(&dl_global_event_types);
    while(event_) {
        if(event_->event_type.event && event_->event_type.event == event) {
            if(event_flag && (event_->event_type.event_flag & event_flag)) {
                return &event_->event_type;
            }
        }
        event_ = dl_next(event_);
    }

    return NULL;
}




                    /*------------------------------------*
                     *      Publication/Subscriptions
                     *------------------------------------*/




/*
Schema of subs (subscription)
=============================
"publisher":            (pointer)int    // publisher gobj
"subscriber:            (pointer)int    // subscriber gobj
"event":                (pointer)str    // event name subscribed
"subs_flag":            int             // subscription flag. See subs_flag_t
"__config__":           json            // subscription config.
"__global__":           json            // global event kw. This json is extended with publishing kw.
"__local__":            json            // local event kw. The keys in this json are removed from publishing kw.

 */

typedef enum {
    __hard_subscription__   = 0x00000001,
    __own_event__           = 0x00000002,   // If gobj_send_event return -1 don't continue publishing
} subs_flag_t;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t * _create_subscription(
    gobj_t * publisher,
    gobj_event_t event,
    json_t *kw, // not owned
    gobj_t * subscriber)
{
    json_t *subs = json_object();
    json_object_set_new(subs, "event", json_integer((json_int_t)(size_t)event));
    json_object_set_new(subs, "subscriber", json_integer((json_int_t)(size_t)subscriber));
    json_object_set_new(subs, "publisher", json_integer((json_int_t)(size_t)publisher));
    subs_flag_t subs_flag = 0;

    if(kw) {
        json_t *__config__ = kw_get_dict(publisher, kw, "__config__", 0, 0);
        json_t *__global__ = kw_get_dict(publisher, kw, "__global__", 0, 0);
        json_t *__local__ = kw_get_dict(publisher, kw, "__local__", 0, 0);
        const char *__service__ = kw_get_str(publisher, kw, "__service__", 0, 0);

        if(__global__) {
            json_t *kw_clone = json_deep_copy(__global__);
            json_object_set_new(subs, "__global__", kw_clone);
        }
        if(__config__) {
            json_t *kw_clone = json_deep_copy(__config__);
            json_object_set_new(subs, "__config__", kw_clone);

            if(kw_has_key(kw_clone, "__hard_subscription__")) {
                BOOL hard_subscription = kw_get_bool(
                    publisher, kw_clone, "__hard_subscription__", 0, 0
                );
                json_object_del(kw_clone, "__hard_subscription__");
                if(hard_subscription) {
                    subs_flag |= __hard_subscription__;
                }
            }
            if(kw_has_key(kw_clone, "__own_event__")) {
                BOOL own_event= kw_get_bool(publisher, kw_clone, "__own_event__", 0, 0);
                json_object_del(kw_clone, "__own_event__");
                if(own_event) {
                    subs_flag |= __own_event__;
                }
            }
        }
        if(__local__) {
            json_t *kw_clone = json_deep_copy(__local__);
            json_object_set_new(subs, "__local__", kw_clone);
        }
        if(__service__) { // TODO check if it's used
            json_object_set_new(subs, "__service__", json_string(__service__));
        }
    }
    json_object_set_new(subs, "subs_flag", json_integer((json_int_t)subs_flag));

    return subs;
}

/***************************************************************************
 *  Delete subscription
 ***************************************************************************/
PRIVATE int _delete_subscription(
    gobj_t * gobj,
    json_t *subs, // not owned
    BOOL force,
    BOOL not_inform
) {
    gobj_t *publisher = (gobj_t *)(size_t)kw_get_int(gobj, subs, "publisher", 0, KW_REQUIRED);
    gobj_t *subscriber = (gobj_t *)(size_t)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);
    gobj_event_t event = (gobj_event_t)(size_t)kw_get_int(gobj, subs, "event", 0, KW_REQUIRED);
    subs_flag_t subs_flag = (subs_flag_t)kw_get_int(gobj, subs, "subs_flag", 0, KW_REQUIRED);
    BOOL hard_subscription = (subs_flag & __hard_subscription__)?1:0;

    /*-------------------------------*
     *  Check if hard subscription
     *-------------------------------*/
    if(hard_subscription) {
        if(!force) {
            return -1;
        }
    }

    /*-----------------------------*
     *          Trace
     *-----------------------------*/
    if(__trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher)
    ) {
        trace_machine("💜💜👎 %s unsubscribing event '%s' of %s",
            gobj_full_name(subscriber),
            event?event:"",
            gobj_full_name(publisher)
        );
        gobj_trace_json(gobj, subs, "subs");
    }

    /*------------------------------------------------*
     *      Inform (BEFORE) of subscription removed
     *------------------------------------------------*/
    if(!(publisher->obflag & obflag_destroyed)) {
        if(!not_inform) {
            if(publisher->gclass->gmt->mt_subscription_deleted) {
                publisher->gclass->gmt->mt_subscription_deleted(
                    publisher,
                    subs
                );
            }
        }
    }

    /*--------------------------------*
     *      Delete subscription
     *--------------------------------*/
    int idx = kw_find_json_in_list(publisher->dl_subscriptions, subs);
    if(idx >= 0) {
        if(json_array_remove(publisher->dl_subscriptions, (size_t)idx)<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "json_array_remove() subscriptions FAILED",
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "subscription in publisher not found",
            NULL
        );
        gobj_trace_json(gobj, subs, "subscription in publisher not found");
    }

    idx = kw_find_json_in_list(subscriber->dl_subscribings, subs);
    if(idx >= 0) {
        if(json_array_remove(subscriber->dl_subscribings, (size_t)idx)<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "json_array_remove() subscribings FAILED",
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "subscription in subscriber not found",
            NULL
        );
        gobj_trace_json(gobj, subs, "subscription in subscriber not found");
    }

//    if((int)subs->refcount > 0) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "subscription NOT DELETED",
//            NULL
//        );
//        gobj_trace_json(gobj, subs, "subscription NOT DELETED");
//    }
    return 0;
}

/***************************************************************************
 *  Return a iter of subscriptions (sdata),
 *  filtering by matching:
 *      event,kw (__config__, __global__, __local__, __filter__),subscriber
 ***************************************************************************/
PRIVATE json_t * _find_subscription(
    json_t *dl_subs,
    gobj_t *publisher,
    gobj_event_t event,
    json_t *kw, // owned
    gobj_t *subscriber,
    BOOL full
) {
    json_t *__config__ = kw_get_dict(0, kw, "__config__", 0, 0);
    json_t *__global__ = kw_get_dict(0, kw, "__global__", 0, 0);
    json_t *__local__ = kw_get_dict(0, kw, "__local__", 0, 0);

    json_t *iter = json_array();

    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        BOOL match = TRUE;

        if(publisher || full) {
            gobj_t *publisher_ = (gobj_t *)(size_t)kw_get_int(0, subs, "publisher", 0, KW_REQUIRED);
            if(publisher != publisher_) {
                match = FALSE;
            }
        }

        if(subscriber || full) {
            gobj_t *subscriber_ = (gobj_t *)(size_t)kw_get_int(0, subs, "subscriber", 0, KW_REQUIRED);
            if(subscriber != subscriber_) {
                match = FALSE;
            }
        }

        if(event || full) {
            gobj_event_t event_ = (gobj_event_t)(size_t)kw_get_int(0, subs, "event", 0, KW_REQUIRED);
            if(event != event_) {
                match = FALSE;
            }
        }

        if(__config__ || full) {
            json_t *kw_config = kw_get_dict(0, subs, "__config__", 0, 0);
            if(kw_config) {
                if(!json_equal(kw_config, __config__)) {
                    match = FALSE;
                }
            } else if(__config__) {
                match = FALSE;
            }
        }
        if(__global__ || full) {
            json_t *kw_global = kw_get_dict(0, subs, "__global__", 0, 0);
            if(kw_global) {
                if(!json_equal(kw_global, __global__)) {
                    match = FALSE;
                }
            } else if(__global__) {
                match = FALSE;
            }
        }
        if(__local__ || full) {
            json_t *kw_local = kw_get_dict(0, subs, "__local__", 0, 0);
            if(kw_local) {
                if(!json_equal(kw_local, __local__)) {
                    match = FALSE;
                }
            } else if(__local__) {
                match = FALSE;
            }
        }

        if(match) {
            json_array_append(iter, subs);
        }
    }

    KW_DECREF(kw)
    return iter;
}

/***************************************************************************
 *  Subscribe to an event
 *
 *  event is only one event (not a string list like ginsfsm).
 *
 *  See .h
 ***************************************************************************/
PUBLIC json_t *gobj_subscribe_event( // return not yours
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber_)
{
    gobj_t * publisher = publisher_;
    gobj_t * subscriber = subscriber_;

    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "publisher NULL",
            "event",        "%s", event,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    if(!subscriber) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscriber NULL",
            "event",        "%s", event,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    if(!empty_string(event)) {
        event_type_t *ev = gobj_event_type(publisher, event, EVF_OUTPUT_EVENT|EVF_SYSTEM_EVENT);
        if(!ev) {
            if(!(publisher->gclass->gclass_flag & gcflag_no_check_output_events)) {
                gobj_log_error(publisher, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "event NOT in output event list",
                    "event",        "%s", event,
                    NULL
                );
                KW_DECREF(kw)
                return 0;
            }
        }
    }

    /*-------------------------------------------------*
     *  Check AUTHZ
     *-------------------------------------------------*/

    /*------------------------------*
     *  Find repeated subscription
     *------------------------------*/
    json_t *dl_subs = _find_subscription(
        publisher->dl_subscriptions,
        publisher,
        event,
        json_incref(kw),
        subscriber,
        TRUE
    );
    if(json_array_size(dl_subs) > 0) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscription(s) REPEATED, will be deleted and override",
            "event",        "%s", event,
            "kw",           "%j", kw,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        gobj_trace_json(publisher, dl_subs, "subscription(s) REPEATED, will be deleted and override");
        gobj_unsubscribe_list(json_incref(dl_subs), FALSE);
    }
    JSON_DECREF(dl_subs)

    /*-----------------------------*
     *  Create subscription
     *-----------------------------*/
    json_t *subs = _create_subscription(
        publisher,
        event,
        kw,  // not owned
        subscriber
    );
    if(!subs) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "_create_subscription() FAILED",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    if(json_array_append(publisher->dl_subscriptions, subs)<0) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_array_append(subscriptions) FAILED",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
    }
    if(json_array_append(subscriber->dl_subscribings, subs)<0) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_array_append(subscribings) FAILED",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
    }

    /*-----------------------------*
     *          Trace
     *-----------------------------*/
    if(__trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher)
    ) {
        trace_machine("💜💜👍 %s subscribing event '%s' of %s",
            gobj_full_name(subscriber),
            event?event:"",
            gobj_full_name(publisher)
        );
        if(kw) {
            if(1 || __trace_gobj_ev_kw__(subscriber) || __trace_gobj_ev_kw__(publisher)) {
                if(json_object_size(kw)) {
                    gobj_trace_json(publisher, kw, "subscribing event");
                }
            }
        }
    }

    /*--------------------------------*
     *      Inform new subscription
     *--------------------------------*/
    if(publisher->gclass->gmt->mt_subscription_added) {
        int result = publisher->gclass->gmt->mt_subscription_added(
            publisher,
            subs
        );
        if(result < 0) {
            _delete_subscription(publisher, subs, TRUE, TRUE);
            subs = 0;
        }
    }

    json_decref(subs);
    json_decref(kw);
    return subs;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int gobj_unsubscribe_event(
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber_)
{
    gobj_t *publisher = publisher_;
    gobj_t *subscriber = subscriber_;

    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "publisher NULL",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(!subscriber) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscriber NULL",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    /*-----------------------------*
     *      Find subscription
     *-----------------------------*/
    KW_INCREF(kw);
    json_t *dl_subs = _find_subscription(
        publisher->dl_subscriptions,
        publisher,
        event,
        kw,
        subscriber,
        TRUE
    );
    int deleted = 0;

    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        _delete_subscription(publisher, subs, FALSE, FALSE);
        deleted++;
    }

    if(!deleted) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No subscription found",
            "event",        "%s", event,
            "publisher",    "%s", gobj_full_name(publisher),
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        gobj_trace_json(publisher, kw, "No subscription found");
    }

    JSON_DECREF(dl_subs)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Unsubscribe a list of subscription hsdata
 ***************************************************************************/
PUBLIC int gobj_unsubscribe_list(
    json_t *dl_subs, // owned
    BOOL force  // delete hard_subscription subs too
)
{
    size_t idx; json_t *subs=0;
    json_array_foreach(dl_subs, idx, subs) {
        _delete_subscription(0, subs, force, FALSE);
    }
    JSON_DECREF(dl_subs)
    return 0;
}

/***************************************************************************
 *  Return a iter of subscriptions (sdata) in the publisher gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__),subscriber
 *  Free return with rc_free_iter(iter, TRUE, FALSE);


 *  gobj_find_subscriptions()
 *  Return the list of subscriptions of the publisher gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__),subscriber

    USO
    ---
    TO resend subscriptions in ac_identity_card_ack() in c_ievent_cli.c

        PRIVATE int resend_subscriptions(hgobj gobj)
        {
            dl_list_t *dl_subs = gobj_find_subscriptions(gobj, 0, 0, 0);

            rc_instance_t *i_subs; hsdata subs;
            i_subs = rc_first_instance(dl_subs, (rc_resource_t **)&subs);
            while(i_subs) {
                send_remote_subscription(gobj, subs);
                i_subs = rc_next_instance(i_subs, (rc_resource_t **)&subs);
            }
            rc_free_iter(dl_subs, TRUE, FALSE);
            return 0;
        }

        que son capturadas en:

             PRIVATE int mt_subscription_added(
                hgobj gobj,
                hsdata subs)
            {
                if(!gobj_in_this_state(gobj, "ST_SESSION")) {
                    // on_open will send all subscriptions
                    return 0;
                }
                return send_remote_subscription(gobj, subs);
            }


    EN mt_stop de c_counter.c (???)

        PRIVATE int mt_stop(hgobj gobj)
        {
            PRIVATE_DATA *priv = gobj_priv_data(gobj);

            gobj_unsubscribe_list(gobj_find_subscriptions(gobj, 0, 0, 0), TRUE, FALSE);

            clear_timeout(priv->timer);
            if(gobj_is_running(priv->timer))
                gobj_stop(priv->timer);
            return 0;
        }


 ***************************************************************************/
PUBLIC json_t * gobj_find_subscriptions(
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw,
    hgobj subscriber)
{
    gobj_t * publisher = publisher_;
    return _find_subscription(
        publisher->dl_subscriptions,
        publisher_,
        event,
        kw,
        subscriber,
        FALSE
    );
}

/***************************************************************************
 *  Return a iter of subscribings (sdata) of the subcriber gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__), publisher
 *  Free return with rc_free_iter(iter, TRUE, FALSE);
 *
 *  gobj_find_subscribings()
 *  Return a list of subscribings of the subscriber gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__), publisher
 *
 *

    USO
    ----
     TO  Delete external subscriptions in c_ievent_src en ac_on_close()
        PRIVATE int ac_on_close(hgobj gobj,  gobj_event_t event, json_t *kw, hgobj src)
            json_t *kw2match = json_object();
            kw_set_dict_value(
                kw2match,
                "__local__`__subscription_reference__",
                json_integer((json_int_t)(size_t)gobj)
            );

            dl_list_t * dl_s = gobj_find_subscribings(gobj, 0, kw2match, 0);
            gobj_unsubscribe_list(dl_s, TRUE, FALSE);

        que son creadas en ac_on_message() en
             *   Dispatch event
            if(strcasecmp(msg_type, "__subscribing__")==0) {
                 *  It's a external subscription

                 *   Protect: only public events
                if(!gobj_event_in_output_event_list(gobj_service, iev_event, EVF_PUBLIC_EVENT)) {
                    char temp[256];
                    snprintf(temp, sizeof(temp),
                        "SUBSCRIBING event ignored, not in output_event_list or not PUBLIC event, check service '%s'",
                        iev_dst_service
                    );
                    log_error(0,
                        "gobj",         "%s", gobj_full_name(gobj),
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", temp,
                        "service",      "%s", iev_dst_service,
                        "gobj_service", "%s", gobj_short_name(gobj_service),
                        "event",        "%s", iev_event,
                        NULL
                    );
                    KW_DECREF(iev_kw);
                    KW_DECREF(kw);
                    return -1;
                }

                // Set locals to remove on publishing
                kw_set_subdict_value(
                    iev_kw,
                    "__local__", "__subscription_reference__",
                    json_integer((json_int_t)(size_t)gobj)
                );
                kw_set_subdict_value(iev_kw, "__local__", "__temp__", json_null());

                // Prepare the return of response
                json_t *__md_iev__ = kw_get_dict(iev_kw, "__md_iev__", 0, 0);
                if(__md_iev__) {
                    KW_INCREF(iev_kw);
                    json_t *kw3 = msg_iev_answer(
                        gobj,
                        iev_kw,
                        0,
                        "__publishing__"
                    );
                    json_object_del(iev_kw, "__md_iev__");
                    json_t *__global__ = kw_get_dict(iev_kw, "__global__", 0, 0);
                    if(__global__) {
                        json_object_update_new(__global__, kw3);
                    } else {
                        json_object_set_new(iev_kw, "__global__", kw3);
                    }
                }

                gobj_subscribe_event(gobj_service, iev_event, iev_kw, gobj);

 ***************************************************************************/
PUBLIC json_t *gobj_find_subscribings(
    hgobj subscriber_,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__, __filter__)
    hgobj publisher
)
{
    gobj_t * subscriber = subscriber_;
    return _find_subscription(
        subscriber->dl_subscribings,
        publisher,
        event,
        kw,
        subscriber,
        FALSE
    );
}

/***************************************************************************
 *  Return the number of sent events
 ***************************************************************************/
PUBLIC int gobj_publish_event(
    hgobj publisher_,
    gobj_event_t event,
    json_t *kw)
{
    gobj_t * publisher = publisher_;

    if(!kw) {
        kw = json_object();
    }

    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj NULL",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    if(publisher->obflag & obflag_destroyed) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gobj DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }
    if(empty_string(event)) {
        gobj_log_error(publisher, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event EMPTY",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    event_type_t *ev = gobj_event_type(publisher, event, EVF_OUTPUT_EVENT|EVF_SYSTEM_EVENT);
    if(!ev) {
        if(!(publisher->gclass->gclass_flag & gcflag_no_check_output_events)) {
            gobj_log_error(publisher, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "event NOT in output event list",
                "event",        "%s", event,
                NULL
            );
            KW_DECREF(kw)
            return 0;
        }
    }

    BOOL tracea = __trace_gobj_subscriptions__(publisher) ||
        (is_machine_tracing(publisher) && !is_machine_not_tracing(publisher));
    if(tracea) {
        trace_machine("🔝🔝 mach(%s%s^%s), st: %s, ev: %s%s%s",
            (!publisher->running)?"!!":"",
            gobj_gclass_name(publisher), gobj_name(publisher),
            publisher->current_state->state_name,
            On_Black BBlue,
            event?event:"",
            Color_Off
        );
        if(__trace_gobj_ev_kw__(publisher)) {
            if(json_object_size(kw)) {
                gobj_trace_json(publisher, kw, "kw publish event %s", event?event:"");
            }
        }
    }

    /*----------------------------------------------------------*
     *  Own publication method
     *  Return:
     *     -1  broke                        -> return
     *      0  continue without publish     -> return
     *      1  publish and continue         -> continue below
     *----------------------------------------------------------*/
    if(publisher->gclass->gmt->mt_publish_event) {
        int topublish = publisher->gclass->gmt->mt_publish_event(
            publisher,
            event,
            kw  // not owned
        );
        if(topublish<=0) {
            KW_DECREF(kw)
            return 0;
        }
    }

    /*--------------------------------------------------------------*
     *      Default publication method
     *--------------------------------------------------------------*/
    json_t *dl_subs = json_copy(publisher->dl_subscriptions); // Protect to inside deleted subs
    int sent_count = 0;
    json_t *subs; size_t idx;
    json_array_foreach(dl_subs, idx, subs) {
        /*-------------------------------------*
         *  Pre-filter
         *  kw NOT owned! you can modify the publishing kw
         *  Return:
         *     -1  broke
         *      0  continue without publish
         *      1  continue to publish
         *-------------------------------------*/
        if(publisher->gclass->gmt->mt_publication_pre_filter) {
            int topublish = publisher->gclass->gmt->mt_publication_pre_filter(
                publisher,
                subs,
                event,
                kw  // not owned
            );
            if(topublish<0) {
                break;
            } else if(topublish==0) {
                continue;
            }
        }
        gobj_t *subscriber = (gobj_t *)(size_t)kw_get_int(
            publisher, subs, "subscriber", 0, KW_REQUIRED
        );
        if(!(subscriber && !(subscriber->obflag & (obflag_destroying|obflag_destroyed)))) {
            continue;
        }

        /*
         *  Check if event null or event in event_list
         */
        subs_flag_t subs_flag = (subs_flag_t)kw_get_int(publisher, subs, "subs_flag", 0, KW_REQUIRED);
        gobj_event_t event_ = (gobj_event_t)(size_t)kw_get_int(
            publisher, subs, "event", 0, KW_REQUIRED
        );
        if(!event_ || event_ == event) {
            // TODO check if need it: json_t *__config__ = kw_get_dict(publisher, subs, "__config__", 0, 0);
            json_t *__global__ = kw_get_dict(publisher, subs, "__global__", 0, 0);
            json_t *__local__ = kw_get_dict(publisher, subs, "__local__", 0, 0);
            json_t *__filter__ = kw_get_dict(publisher, subs, "__filter__", 0, 0);

            json_t *kw2publish = kw_incref(kw);

            /*-------------------------------------*
             *  User filter method or filter parameter
             *  Return:
             *     -1  (broke),
             *      0  continue without publish,
             *      1  continue and publish
             *-------------------------------------*/
            int topublish = 1;
            if(publisher->gclass->gmt->mt_publication_filter) {
                topublish = publisher->gclass->gmt->mt_publication_filter(
                    publisher,
                    event,
                    kw2publish,  // not owned
                    subscriber
                );
            } else if(__filter__) {
                if(__publish_event_match__) {
                    KW_INCREF(__filter__);
                    topublish = __publish_event_match__(kw2publish , __filter__);
                }
            }

            if(topublish<0) {
                KW_DECREF(kw2publish);
                break;
            } else if(topublish==0) {
                /*
                 *  Must not be published
                 *  Next subs
                 */
                KW_DECREF(kw2publish);
                continue;
            }

            /*
             *  Check if System event: don't send if subscriber has not it
             */
            if(event == EV_STATE_CHANGED) {
                if(!gobj_has_input_event(subscriber, event)) {
                    KW_DECREF(kw2publish);
                    continue;
                }
            }

            /*
             *  Remove local keys
             */
            if(__local__) {
                kw_pop(kw2publish,
                    __local__ // not owned
                );
            }

            /*
             *  Apply transformation filters
             */
//            if(__config__) { TODO need it?
//                json_t *jn_trans_filters = kw_get_dict_value(__config__, "__trans_filter__", 0, 0);
//                if(jn_trans_filters) {
//                    kw2publish = apply_trans_filters(kw2publish, jn_trans_filters);
//                }
//            }

            /*
             *  Add global keys
             */
            if(__global__) {
                json_object_update(kw2publish, __global__);
            }

            /*
             *  Send event
             */
            if(tracea) {
                trace_machine("🔝🔄 mach(%s%s), ev: %s, from(%s%s)",
                    (!subscriber->running)?"!!":"",
                    gobj_short_name(subscriber),
                    event?event:"",
                    (publisher && !publisher->running)?"!!":"",
                    gobj_short_name(publisher)
                );
                if(__trace_gobj_ev_kw__(publisher)) {
                    if(json_object_size(kw2publish)) {
                        gobj_trace_json(publisher, kw2publish, "kw publish send event");
                    }
                }
            }

            int ret = gobj_send_event(
                subscriber,
                event,
                kw2publish,
                publisher
            );
            if(ret < 0 && (subs_flag & __own_event__)) {
                sent_count = -1; // Return of -1 indicates that someone owned the event
                break;
            }
            sent_count++;

            if(publisher->obflag & (obflag_destroying|obflag_destroyed)) {
                /*
                 *  break all, self publisher deleted
                 */
                break;
            }
        }
    }

    if(!sent_count) {
        if(!ev || !(ev->event_flag & EVF_NO_WARN_SUBS)) {
            gobj_log_warning(publisher, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Publish event WITHOUT subscribers",
                "event",        "%s", event,
                NULL
            );
            if(__trace_gobj_ev_kw__(publisher)) {
                if(json_object_size(kw)) {
                    gobj_trace_json(publisher, kw, "Publish event WITHOUT subscribers");
                }
            }
        }
    }

    JSON_DECREF(dl_subs)
    KW_DECREF(kw)
    return sent_count;
}




                    /*------------------------------------*
                     *  SECTION: Authorization functions
                     *------------------------------------*/




/***************************************************************************
 *  Authenticate
 *  Return json response
 *
        {
            "result": 0,        // 0 successful authentication, -1 error
            "comment": "",
            "username": ""      // username authenticated
        }

 *  HACK if there is no authentication parser the authentication is TRUE
    and the username is the current system user
 *  WARNING Becare and use no parser only in local services!
 ***************************************************************************/
PUBLIC json_t *gobj_authenticate(hgobj gobj_, json_t *kw, hgobj src)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    /*---------------------------------------------*
     *  The local mt_authenticate has preference
     *---------------------------------------------*/
    if(gobj->gclass->gmt->mt_authenticate) {
        return gobj->gclass->gmt->mt_authenticate(gobj, kw, src);
    }

    /*-----------------------------------------------*
     *  Then use the global authzs parser
     *-----------------------------------------------*/
    if(!__global_authenticate_parser_fn__) {
#ifdef __linux__
        struct passwd *pw = getpwuid(getuid());

        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s, s:o}",
            "result", 0,
            "comment", "Working without authentication",
            "username", pw->pw_name,
            "jwt_payload", json_null()
        );
#else
        KW_DECREF(kw)
        return json_pack("{s:i, s:s, s:s, s:o}",
            "result", 0,
            "comment", "Working without authentication",
            "username", "yuneta",
            "jwt_payload", json_null()
        );
#endif
    }

    return __global_authenticate_parser_fn__(gobj, kw, src);
}

/****************************************************************************
 *  list authzs of gobj
 ****************************************************************************/
PUBLIC json_t *gobj_authzs(
    hgobj gobj  // If null return global authzs
)
{
    return 0;
// TODO    return authzs_list(gobj, "");
}

/****************************************************************************
 *  list authzs of gobj
 ****************************************************************************/
PUBLIC json_t *gobj_authz(
    hgobj gobj_,
    const char *authz // return all list if empty string, else return authz desc
)
{
    gobj_t *gobj = gobj_;
    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        return 0;
    }
    // TODO return authzs_list(gobj, authz);
    return 0;
}

/****************************************************************************
 *  Return if user has authz in gobj in context
 *  HACK if there is no authz checker the authz is TRUE
 ****************************************************************************/
PUBLIC BOOL gobj_user_has_authz(
    hgobj gobj_,
    const char *authz,
    json_t *kw,
    hgobj src
)
{
    gobj_t * gobj = gobj_;

    if(!gobj || gobj->obflag & obflag_destroyed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "hgobj NULL or DESTROYED",
            NULL
        );
        KW_DECREF(kw)
        return FALSE;
    }

    /*----------------------------------------------------*
     *  The local mt_authz_checker has preference
     *----------------------------------------------------*/
    if(gobj->gclass->gmt->mt_authz_checker) {
        BOOL has_permission = gobj->gclass->gmt->mt_authz_checker(gobj, authz, kw, src);
        if(__trace_gobj_authzs__(gobj)) {
//TODO            log_debug_json(0, kw,
//                "local authzs 🔑🔑 %s => %s",
//                gobj_short_name(gobj),
//                has_permission?"👍":"🚫"
//            );
        }
        return has_permission;
    }

    /*-----------------------------------------------*
     *  Then use the global authz checker
     *-----------------------------------------------*/
    if(__global_authz_checker_fn__) {
        BOOL has_permission = __global_authz_checker_fn__(gobj, authz, kw, src);
        if(__trace_gobj_authzs__(gobj)) {
//  TODO          log_debug_json(0, kw,
//                "global authzs 🔑🔑 %s => %s",
//                gobj_short_name(gobj),
//                has_permission?"👍":"🚫"
//            );
        }
        return has_permission;
    }

    KW_DECREF(kw)
    return TRUE; // HACK if there is no authz checker the authz is TRUE
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC const sdata_desc_t *gobj_get_global_authz_table(void)
{
    // TODO return global_authz_table;
    return 0;
}





                    /*---------------------------------*
                     *  SECTION: Stats
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_set_stat(hgobj gobj_, const char *path, json_int_t value)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    json_int_t old_value = kw_get_int(gobj, gobj->jn_stats, path, 0, 0);
    kw_set_dict_value(gobj, gobj->jn_stats, path, json_integer(value));

    return old_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_incr_stat(hgobj gobj_, const char *path, json_int_t value)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }

    json_int_t cur_value = kw_get_int(gobj, gobj->jn_stats, path, 0, 0);
    cur_value += value;
    kw_set_dict_value(gobj, gobj->jn_stats, path, json_integer(cur_value));

    return cur_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_decr_stat(hgobj gobj_, const char *path, json_int_t value)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    json_int_t cur_value = kw_get_int(gobj, gobj->jn_stats, path, 0, 0);
    cur_value -= value;
    kw_set_dict_value(gobj, gobj->jn_stats, path, json_integer(cur_value));

    return cur_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_int_t gobj_get_stat(hgobj gobj, const char *path)
{
    if(!gobj) {
        return 0;
    }
    return kw_get_int(gobj, ((gobj_t *)gobj)->jn_stats, path, 0, 0);
}

/***************************************************************************
 *  WARNING the json return is NOT YOURS!
 ***************************************************************************/
PUBLIC json_t *gobj_jn_stats(hgobj gobj)
{
    return ((gobj_t *)gobj)->jn_stats;
}




                    /*---------------------------------*
                     *      Trace functions
                     *---------------------------------*/




/****************************************************************************
 *  Return gobj trace level
 ****************************************************************************/
PUBLIC uint32_t gobj_trace_level(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    if(__deep_trace__) {
        return (uint32_t)-1;
    }
    uint32_t bitmask = __global_trace_level__;
    if(gobj) {
        if(!gobj->gclass->jn_trace_filter || !gobj->jn_attrs) {
            bitmask |= gobj->trace_level;
            if(gobj->gclass) {
                bitmask |= gobj->gclass->trace_level;
            }
        } else {
            const char *attr; json_t *jn_list_values;
            json_object_foreach(gobj->gclass->jn_trace_filter, attr, jn_list_values) {
                size_t idx; json_t *jn_value;
                json_array_foreach(jn_list_values, idx, jn_value) {
                    const char *value = json_string_value(jn_value);
                    // TODO consider other types than str like int
                    const char *value_ = gobj_read_str_attr(gobj, attr);
                    if(value && value_ && strcmp(value, value_)==0) {
                        bitmask |= gobj->trace_level;
                        if(gobj->gclass) {
                            bitmask |= gobj->gclass->trace_level;
                        }
                        break;
                    }
                }
            }
        }
    }

    return bitmask;
}

/****************************************************************************
 *  Return gobj no trace level
 ****************************************************************************/
PUBLIC uint32_t gobj_trace_no_level(hgobj gobj_)
{
    gobj_t * gobj = gobj_;

    uint32_t bitmask = __global_trace_no_level__;

    if(gobj) {
        bitmask |= gobj->no_trace_level;
        if (gobj->gclass) {
            bitmask |= gobj->gclass->no_trace_level;
        }
    }
    return bitmask;
}

/****************************************************************************
 *  Convert 32 bit to string level
 ****************************************************************************/
PRIVATE json_t *bit2level(
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    uint32_t bit)
{
    json_t *jn_list = json_array();
    for(int i=0; i<16; i++) {
        if(!internal_trace_level[i].name) {
            break;
        }
        uint32_t bitmask = 1 << i;
        bitmask <<= 16;
        if(bit & bitmask) {
            json_array_append_new(
                jn_list,
                json_string(internal_trace_level[i].name)
            );
        }
    }
    if(user_trace_level) {
        for(int i=0; i<16; i++) {
            if(!user_trace_level[i].name) {
                break;
            }
            uint32_t bitmask = 1 << i;
            if(bit & bitmask) {
                json_array_append_new(
                    jn_list,
                    json_string(user_trace_level[i].name)
                );
            }
        }
    }
    return jn_list;
}

/****************************************************************************
 *  Convert string level to 32 bit
 ****************************************************************************/
PRIVATE uint32_t level2bit(
    const trace_level_t *internal_trace_level,
    const trace_level_t *user_trace_level,
    const char *level
)
{
    for(int i=0; i<16; i++) {
        if(!internal_trace_level[i].name) {
            break;
        }
        if(strcasecmp(level, internal_trace_level[i].name)==0) {
            uint32_t bitmask = 1 << i;
            bitmask <<= 16;
            return bitmask;
        }
    }
    if(user_trace_level) {
        for(int i=0; i<16; i++) {
            if(!user_trace_level[i].name) {
                break;
            }
            if(strcasecmp(level, user_trace_level[i].name)==0) {
                uint32_t bitmask = 1 << i;
                return bitmask;
            }
        }
    }

    // WARNING Don't log errors, this functions are called in main(), before logger is setup.
    return 0;
}

/****************************************************************************
 *  Set or Reset gobj trace level
 ****************************************************************************/
PRIVATE int _set_gobj_trace_level(gobj_t * gobj, const char *level, BOOL set)
{
    uint32_t bitmask = 0;

    if(level == NULL) {
        bitmask = (uint32_t)-1;
    } else if(empty_string(level)) {
        bitmask = TRACE_USER_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gobj->gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "gobj_name",    "%s", gobj_short_name(gobj),
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gobj->trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gobj->trace_level &= ~bitmask;
    }

    return 0;
}

/****************************************************************************
 *  Set or Reset gobj trace level
 *  Call mt_trace_on/mt_trace_off
 ****************************************************************************/
PUBLIC int gobj_set_gobj_trace(hgobj gobj_, const char *level, BOOL set, json_t *kw)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        int ret = gobj_set_global_trace(level, set);
        JSON_DECREF(kw)
        return ret;
    }

    if(_set_gobj_trace_level(gobj, level, set)<0) {
        JSON_DECREF(kw)
        return -1;
    }
    if(set) {
        if(gobj->gclass->gmt->mt_trace_on) {
            return gobj->gclass->gmt->mt_trace_on(gobj, level, kw);
        }
    } else {
        if(gobj->gclass->gmt->mt_trace_off) {
            return gobj->gclass->gmt->mt_trace_off(gobj, level, kw);
        }
    }
    JSON_DECREF(kw)
    return 0;
}

/****************************************************************************
 *  Set or Reset gclass trace level
 ****************************************************************************/
PUBLIC int gobj_set_gclass_trace(hgclass gclass_, const char *level, BOOL set)
{
    gclass_t *gclass = gclass_;
    uint32_t bitmask = 0;

    if(!gclass) {
        return gobj_set_global_trace(level, set);
    }
    if(level == NULL) {
        bitmask = (uint32_t)-1;
    } else if(empty_string(level)) {
        bitmask = TRACE_USER_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "gclass",       "%s", gclass->gclass_name,
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gclass->trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gclass->trace_level &= ~bitmask;
    }

    return 0;
}

/***************************************************************************
 *  level 1 all but considering no_trace_level
 *  level >1 all
 ***************************************************************************/
PUBLIC int gobj_set_deep_tracing(int level)
{
    __deep_trace__ = (uint32_t)level;

    return 0;
}
PUBLIC int gobj_get_deep_tracing(void)
{
    return (int)__deep_trace__;
}

/****************************************************************************
 *  Set or Reset global trace level
 ****************************************************************************/
PUBLIC int gobj_set_global_trace(const char *level, BOOL set)
{
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = TRACE_GLOBAL_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, 0, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "global trace level NOT FOUND",
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        __global_trace_level__ |= bitmask;
    } else {
        /*
         *  Reset
         */
        __global_trace_level__ &= ~bitmask;
    }
    return 0;
}

/****************************************************************************
 *  Set or Reset global no trace level
 ****************************************************************************/
PUBLIC int gobj_set_global_no_trace(const char *level, BOOL set)
{
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = TRACE_GLOBAL_LEVEL;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, 0, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "global trace level NOT FOUND",
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        __global_trace_no_level__ |= bitmask;
    } else {
        /*
         *  Reset
         */
        __global_trace_no_level__ &= ~bitmask;
    }
    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC int gobj_load_trace_filter(hgclass gclass_, json_t *jn_trace_filter) // owned
{
    gclass_t *gclass = gclass_;

    JSON_DECREF(gclass->jn_trace_filter)
    gclass->jn_trace_filter = jn_trace_filter;
    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC int gobj_add_trace_filter(hgclass gclass_, const char *attr, const char *value)
{
    gclass_t *gclass = gclass_;

    if(empty_string(attr)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "attr NULL",
            NULL
        );
        return -1;
    }
    if(!gclass_has_attr(gclass, attr)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gclass has not this attribute",
            "attr",         "%s", attr,
            NULL
        );
        return -1;
    }

    if(!gclass->jn_trace_filter) {
        gclass->jn_trace_filter = json_object();
    }

    json_t *jn_list = kw_get_list(0, gclass->jn_trace_filter, attr, json_array(), KW_CREATE);
    if(!jn_list) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "cannot create list",
            "attr",         "%s", attr,
            "value",        "%s", value?value:"",
            NULL
        );
        return -1;
    }

    if(!value) {
        value = "";
    }
    int idx = kw_find_str_in_list(0, jn_list, value);
    if(idx < 0) {
        json_array_append_new(jn_list, json_string(value));
    }
    return 0;
}

/****************************************************************************
 *
 ****************************************************************************/
PUBLIC int gobj_remove_trace_filter(hgclass gclass_, const char *attr, const char *value)
{
    gclass_t *gclass = gclass_;

    if(empty_string(attr)) {
        JSON_DECREF(gclass->jn_trace_filter)
        return 0;
    }
    if(!gclass->jn_trace_filter) {
        return 0;
    }

    json_t *jn_list = kw_get_list(0, gclass->jn_trace_filter, attr, 0, 0);
    if(!jn_list) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "attr not found",
            "attr",         "%s", attr,
            "value",        "%s", value?value:"",
            NULL
        );
        return -1;
    }

    if(empty_string(value)) {
        json_array_clear(jn_list);
        kw_delete(0, gclass->jn_trace_filter, attr);
        if(json_object_size(gclass->jn_trace_filter)==0) {
            JSON_DECREF(gclass->jn_trace_filter)
        }
        return 0;
    }

    int idx = kw_find_str_in_list(0, jn_list, value);
    if(idx < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "value not found",
            "attr",         "%s", attr,
            "value",        "%s", value?value:"",
            NULL
        );
        return -1;
    }

    json_array_remove(jn_list, (size_t)idx);

    if(json_array_size(jn_list) == 0) {
        kw_delete(0, gclass->jn_trace_filter, attr);
        if(json_object_size(gclass->jn_trace_filter)==0) {
            JSON_DECREF(gclass->jn_trace_filter)
        }
    }

    return 0;
}

/****************************************************************************
 *  Return is not YOURS
 ****************************************************************************/
PUBLIC json_t *gobj_get_trace_filter(hgclass gclass_)
{
    gclass_t *gclass = gclass_;
    return gclass->jn_trace_filter;
}

/****************************************************************************
 *  Set or Reset NO gclass trace level
 ****************************************************************************/
PUBLIC int gobj_set_gclass_no_trace(hgclass gclass_, const char *level, BOOL set)
{
    gclass_t *gclass = gclass_;
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = (uint32_t)-1;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "gclass",       "%s", gclass->gclass_name,
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gclass->no_trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gclass->no_trace_level &= ~bitmask;
    }

    return 0;
}

/****************************************************************************
 *  Set or Reset NO gobj trace level
 ****************************************************************************/
PRIVATE int _set_gobj_no_trace_level(hgobj gobj_, const char *level, BOOL set)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    uint32_t bitmask = 0;

    if(empty_string(level)) {
        bitmask = (uint32_t)-1;
    } else {
        if(isdigit(*((unsigned char *)level))) {
            bitmask = (uint32_t)atoi(level);
        }
        if(!bitmask) {
            bitmask = level2bit(s_global_trace_level, gobj->gclass->s_user_trace_level, level);
            if(!bitmask) {
                if(__initialized__) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "gclass trace level NOT FOUND",
                        "level",        "%s", level,
                        NULL
                    );
                }
                return -1;
            }
        }
    }

    if(set) {
        /*
         *  Set
         */
        gobj->no_trace_level |= bitmask;
    } else {
        /*
         *  Reset
         */
        gobj->no_trace_level &= ~bitmask;
    }

    return 0;
}

/****************************************************************************
 *  Indent, return spaces multiple of depth level gobj.
 *  With this, we can see the trace messages indenting according
 *  to depth level.
 ****************************************************************************/
PUBLIC char *tab(char *bf, int bflen)
{
    int i;

    bf[0]=' ';
    for(i=1; i<__inside__*2 && i<bflen-2; i++)
        bf[i] = ' ';
    bf[i] = '\0';
    return bf;
}

/****************************************************************************
 *  Set or Reset NO gobj trace level
 ****************************************************************************/
PUBLIC int gobj_set_gobj_no_trace(hgobj gobj_, const char *level, BOOL set)
{
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return 0;
    }
    _set_gobj_no_trace_level(gobj, level, set);

    return 0;
}

/***************************************************************************
 *  Must trace?
 ***************************************************************************/
PRIVATE inline BOOL is_machine_tracing(gobj_t * gobj)
{
    if(__deep_trace__) {
        return TRUE;
    }
    if(!gobj) {
        return FALSE;
    }
    uint32_t trace = __global_trace_level__ & TRACE_MACHINE ||
        gobj->trace_level & TRACE_MACHINE ||
        gobj->gclass->trace_level & TRACE_MACHINE;

    return trace?TRUE:FALSE;
}

/***************************************************************************
 *  Must no trace?
 ***************************************************************************/
PRIVATE inline BOOL is_machine_not_tracing(gobj_t * gobj)
{
    if(__deep_trace__ > 1) {
        return FALSE;
    }
    if(!gobj) {
        return TRUE;
    }
    uint32_t no_trace = gobj->no_trace_level & TRACE_MACHINE ||
        gobj->gclass->no_trace_level & TRACE_MACHINE;

    return no_trace?TRUE:FALSE;
}

/***************************************************************************
 *  Must trace?
 ***************************************************************************/
PUBLIC BOOL is_level_tracing(hgobj gobj_, uint32_t level)
{
    if(__deep_trace__) {
        return TRUE;
    }
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return FALSE;
    }
    uint32_t trace = __global_trace_level__ & level ||
        gobj->trace_level & level ||
        gobj->gclass->trace_level & level;

    return trace?TRUE:FALSE;
}

/***************************************************************************
 *  Must no trace?
 ***************************************************************************/
PUBLIC BOOL is_level_not_tracing(hgobj gobj_, uint32_t level)
{
    if(__deep_trace__ > 1) {
        return FALSE;
    }
    gobj_t * gobj = gobj_;
    if(!gobj) {
        return TRUE;
    }
    uint32_t no_trace = gobj->no_trace_level & level ||
        gobj->gclass->no_trace_level & level;

    return no_trace?TRUE:FALSE;
}




                        /*---------------------------------*
                         *      SECTION: memory
                         *---------------------------------*/




/***************************************************************************
 *     Set memory functions
 ***************************************************************************/
PUBLIC int gobj_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
) {
    sys_malloc_fn = malloc_func;
    sys_realloc_fn = realloc_func;
    sys_calloc_fn = calloc_func;
    sys_free_fn = free_func;

    return 0;
}

/***************************************************************************
 *     Get memory functions
 ***************************************************************************/
PUBLIC int gobj_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
) {
    if(malloc_func) {
        *malloc_func = sys_malloc_fn;
    }
    if(realloc_func) {
        *realloc_func = sys_realloc_fn;
    }
    if(calloc_func) {
        *calloc_func = sys_calloc_fn;
    }
    if(free_func) {
        *free_func = sys_free_fn;
    }

    return 0;
}

/***********************************************************************
 *      Get memory functions
 ***********************************************************************/
PUBLIC sys_malloc_fn_t gobj_malloc_func(void) { return sys_malloc_fn; }
PUBLIC sys_realloc_fn_t gobj_realloc_func(void) { return sys_realloc_fn; }
PUBLIC sys_calloc_fn_t gobj_calloc_func(void) { return sys_calloc_fn; }
PUBLIC sys_free_fn_t gobj_free_func(void) { return sys_free_fn; }

#define CONFIG_TRACK_MEMORY

#ifdef CONFIG_TRACK_MEMORY
    PRIVATE size_t mem_ref = 0;
    PRIVATE dl_list_t dl_busy_mem = {0};

    typedef struct {
        DL_ITEM_FIELDS
        size_t size;
        size_t ref;
    } track_mem_t;

    unsigned long *memory_check_list = 0;
#else
    typedef struct {
        size_t size;
    } track_mem_t;
#endif


#define TRACK_MEM sizeof(track_mem_t)

/***********************************************************************
 *      Set mem ref list to check
 ***********************************************************************/
PUBLIC void set_memory_check_list(unsigned long *memory_check_list_)
{
#ifdef CONFIG_TRACK_MEMORY
    memory_check_list = memory_check_list_;
#endif
}

/***********************************************************************
 *      Print track memory
 ***********************************************************************/
PRIVATE void print_track_mem(void)
{
    gobj_log_error(0, 0,
        "function",             "%s", __FUNCTION__,
        "msgset",               "%s", MSGSET_STATISTICS,
        "msg",                  "%s", "shutdown: system memory not free",
        NULL
    );
#ifdef CONFIG_TRACK_MEMORY
    track_mem_t *track_mem = dl_first(&dl_busy_mem);
    while(track_mem) {
        gobj_log_debug(0,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRACK_MEM,
            "msg",          "%s", "mem-not-free",
            "ref",          "%lu", (unsigned long)track_mem->ref,
            "size",         "%lu", (unsigned long)track_mem->size,
            NULL
        );

        track_mem = dl_next(track_mem);
    }
    gobj_log_error(0, 0,  // repeat message at the end
        "function",             "%s", __FUNCTION__,
        "msgset",               "%s", MSGSET_STATISTICS,
        "msg",                  "%s", "shutdown: system memory not free",
        NULL
    );
#endif
}

/***********************************************************************
 *
 ***********************************************************************/
#ifdef CONFIG_TRACK_MEMORY
PRIVATE void check_failed_list(track_mem_t *track_mem)
{
    for(int xx=0; memory_check_list && memory_check_list[xx]!=0; xx++) {
        if(track_mem->ref  == memory_check_list[xx]) {
            gobj_log_debug(0, LOG_OPT_TRACE_STACK,
                "msgset",       "%s", MSGSET_STATISTICS,
                "msg",          "%s", "memory lost",
                "ref",          "%ul", (unsigned long)track_mem->ref,
                NULL
            );
        }
        if(xx > 5) {
            break;  // bit a bit please
        }
    }
}
#endif

/***********************************************************************
 *      Alloc memory
 ***********************************************************************/
PRIVATE void *_mem_malloc(size_t size)
{
    size_t extra = TRACK_MEM;
    size += extra;

    if(size > __max_block__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "SIZE GREATER THAN MAX_BLOCK",
            "size",         "%ld", (long)size,
            "max_block",    "%d", (int)__max_block__,
            NULL
        );
        return NULL;
    }

    __cur_system_memory__ += size;

    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
            NULL
        );
    }
    char *pm = calloc(1, size);
    if(!pm) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "NO MEMORY calloc() failed",
            "size",                 "%ld", (long)size,
            NULL
        );
    }
    track_mem_t *pm_ = (track_mem_t*)pm;
    pm_->size = size;

#ifdef CONFIG_TRACK_MEMORY
    pm_->ref = ++mem_ref;
    dl_add(&dl_busy_mem, pm_);

    check_failed_list(pm_);
#endif

    pm += extra;

    return pm;
}

/***********************************************************************
 *      Free memory
 ***********************************************************************/
PRIVATE void _mem_free(void *p)
{
    if(!p) {
        return; // El comportamiento como free() es que no salga error; lo quito por libuv (uv_try_write)
    }
    size_t extra = TRACK_MEM;

    char *pm = p;
    pm -= extra;

    track_mem_t *pm_ = (track_mem_t*)pm;
    size_t size = pm_->size;

#ifdef CONFIG_TRACK_MEMORY
    dl_delete(&dl_busy_mem, pm_, 0);
#endif

    __cur_system_memory__ -= size;
    free(pm);
}

/***************************************************************************
 *     ReAlloca memoria del core
 ***************************************************************************/
PRIVATE void *_mem_realloc(void *p, size_t new_size)
{
    /*---------------------------------*
     *  realloc admit p null
     *---------------------------------*/
    if(!p) {
        return _mem_malloc(new_size);
    }

    size_t extra = TRACK_MEM;
    new_size += extra;

    char *pm = p;
    pm -= extra;

    track_mem_t *pm_ = (track_mem_t*)pm;
    size_t size = pm_->size;

#ifdef CONFIG_TRACK_MEMORY
    dl_delete(&dl_busy_mem, pm_, 0);
#endif

    __cur_system_memory__ -= size;

    __cur_system_memory__ += new_size;
    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
            NULL
        );
    }
    char *pm__ = realloc(pm, new_size);
    if(!pm__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "NO MEMORY realloc() failed",
            "new_size",             "%ld", (long)new_size,
            NULL
        );
    }
    pm = pm__;

    pm_ = (track_mem_t*)pm;
    pm_->size = new_size;

#ifdef CONFIG_TRACK_MEMORY
    pm_->ref = ++mem_ref;
    dl_add(&dl_busy_mem, pm_);
#endif

    pm += extra;
    return pm;
}

/***************************************************************************
 *     duplicate a substring
 ***************************************************************************/
PRIVATE void *_mem_calloc(size_t n, size_t size)
{
    size_t total = n * size;
    return _mem_malloc(total);
}

/***************************************************************************
 *     duplicate a substring
 ***************************************************************************/
PUBLIC char *gobj_strndup(const char *str, size_t size)
{
    char *s;
    size_t len;

    /*-----------------------------------------*
     *     Check null string
     *-----------------------------------------*/
    if(!str) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "str is NULL",
            NULL
        );
        return NULL;
    }

    /*-----------------------------------------*
     *     Alloca memoria
     *-----------------------------------------*/
    len = size;
    s = (char *)sys_malloc_fn(len+1);
    if(!s) {
        return NULL;
    }

    /*-----------------------------------------*
     *     Copia el substring
     *-----------------------------------------*/
    strncpy(s, str, len);

    return s;
}

/***************************************************************************
 *     Duplica un string
 ***************************************************************************/
PUBLIC char *gobj_strdup(const char *string)
{
    if(!string) {
        return NULL;
    }
    return gobj_strndup(string, strlen(string));
}

/*************************************************************************
 *  Return the maximum memory that you can get
 *************************************************************************/
PUBLIC size_t gobj_get_maximum_block(void)
{
    return __max_block__;
}




                        /*---------------------------------*
                         *      SECTION: dl_list
                         *---------------------------------*/




/***************************************************************
 *      Initialize double list
 ***************************************************************/
PUBLIC int dl_init(dl_list_t *dl)
{
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        gobj_trace_msg(0, "dl_init(): Wrong dl_list_t, MUST be empty");
        abort();
    }
    dl->head = 0;
    dl->tail = 0;
    dl->__itemsInContainer__ = 0;
    return 0;
}

/***************************************************************
 *      Seek first item
 ***************************************************************/
PUBLIC void *dl_first(dl_list_t *dl)
{
    return dl->head;
}

/***************************************************************
 *      Seek last item
 ***************************************************************/
PUBLIC void *dl_last(dl_list_t *dl)
{
    return dl->tail;
}

/***************************************************************
 *      next Item
 ***************************************************************/
PUBLIC void *dl_next(void *curr)
{
    if(curr) {
        return ((dl_item_t *) curr)->__next__;
    }
    return (void *)0;
}

/***************************************************************
 *      previous Item
 ***************************************************************/
PUBLIC void *dl_prev(void *curr)
{
    if(curr) {
        return ((dl_item_t *) curr)->__prev__;
    }
    return (void *)0;
}

/***************************************************************
 *  Check if a new item has links: MUST have NO links
 *  Return true if it has links
 ***************************************************************/
static bool check_links(register dl_item_t *item)
{
    if(item->__prev__ || item->__next__ || item->__dl__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_item_t, WITH links",
            NULL
        );
        return true;
    }
    return false;
}

/***************************************************************
 *  Check if a item has no links: MUST have links
 *  Return true if it has not links
 ***************************************************************/
static bool check_no_links(register dl_item_t *item)
{
    if(!item->__dl__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_item_t, WITHOUT links",
            NULL
        );
        return true;
    }
    return false;
}

/***************************************************************
 *      Add at end of the list
 ***************************************************************/
PUBLIC int dl_add(dl_list_t *dl, void *item)
{
    if(check_links(item)) return -1;

    if(dl->tail==0) { /*---- Empty List -----*/
        ((dl_item_t *)item)->__prev__=0;
        ((dl_item_t *)item)->__next__=0;
        dl->head = item;
        dl->tail = item;
    } else { /* LAST ITEM */
        ((dl_item_t *)item)->__prev__ = dl->tail;
        ((dl_item_t *)item)->__next__ = 0;
        dl->tail->__next__ = item;
        dl->tail = item;
    }
    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)item)->__id__ = dl->__last_id__;
    ((dl_item_t *)item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *    Delete current item
 ***************************************************************/
PUBLIC int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *))
{
    register dl_item_t * curr = curr_;
    /*-------------------------*
     *     Check nulls
     *-------------------------*/
    if(curr==0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item NULL",
            NULL
        );
        return -1;
    }
    if(check_no_links(curr))
        return -1;

    if(curr->__dl__ != dl) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item with DIFFERENT dl_list_t",
            NULL
        );
        return -1;
    }

    /*-------------------------*
     *     Check list
     *-------------------------*/
    if(dl->head==0 || dl->tail==0 || dl->__itemsInContainer__==0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item in EMPTY list",
            NULL
        );
        return -1;
    }

    /*------------------------------------------------*
     *                 Delete
     *------------------------------------------------*/
    if(((dl_item_t *)curr)->__prev__==0) {
        /*------------------------------------*
         *  FIRST ITEM. (curr==dl->head)
         *------------------------------------*/
        dl->head = dl->head->__next__;
        if(dl->head) /* is last item? */
            dl->head->__prev__=0; /* no */
        else
            dl->tail=0; /* yes */

    } else {
        /*------------------------------------*
         *    MIDDLE or LAST ITEM
         *------------------------------------*/
        ((dl_item_t *)curr)->__prev__->__next__ = ((dl_item_t *)curr)->__next__;
        if(((dl_item_t *)curr)->__next__) /* last? */
            ((dl_item_t *)curr)->__next__->__prev__ = ((dl_item_t *)curr)->__prev__; /* no */
        else
            dl->tail= ((dl_item_t *)curr)->__prev__; /* yes */
    }

    /*-----------------------------*
     *  Decrement items counter
     *-----------------------------*/
    dl->__itemsInContainer__--;

    /*-----------------------------*
     *  Reset pointers
     *-----------------------------*/
    ((dl_item_t *)curr)->__prev__ = 0;
    ((dl_item_t *)curr)->__next__ = 0;
    ((dl_item_t *)curr)->__dl__ = 0;

    /*-----------------------------*
     *  Free item
     *-----------------------------*/
    if(fnfree) {
        (*fnfree)(curr);
    }

    return 0;
}

/***************************************************************
 *      DL_LIST: Flush double list. (Remove all items).
 ***************************************************************/
PUBLIC void dl_flush(dl_list_t *dl, void (*fnfree)(void *))
{
    register dl_item_t *first;

    while((first = dl_first(dl))) {
        dl_delete(dl, first, fnfree);
    }
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_list_t, MUST be empty",
            NULL
        );
    }
}

/***************************************************************
 *   Return number of items in list
 ***************************************************************/
PUBLIC size_t dl_size(dl_list_t *dl)
{
    if(!dl) {
        return 0;
    }
    return dl->__itemsInContainer__;
}
