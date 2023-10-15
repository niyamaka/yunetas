/****************************************************************************
 *              kwid.h
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
/*
 *  Dependencies
 */
#include "gobj.h"

#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Constants
 *****************************************************************/
#define KW_GET(__name__, __default__, __func__) \
    __name__ = __func__(gobj, kw, #__name__, __default__, 0);

#define KW_GET_REQUIRED(gobj, __name__, __default__, __func__) \
__name__ = __func__(kw, #__name__, __default__, KW_REQUIRED);

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*---------------------------------*
 *          KW0
 *---------------------------------*/
/*---------------------------------*
 *      Json functions
 *---------------------------------*/
#define JSON_DECREF(json)                                           \
{                                                                   \
    if(json) {                                                      \
        if((int)((json)->refcount) <= 0 || (json)->type > 7 || (json)->type < 0) { \
            gobj_log_error(0, LOG_OPT_TRACE_STACK,                  \
                "gobj",         "%s", __FILE__,                     \
                "function",     "%s", __FUNCTION__,                 \
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,        \
                "msg",          "%s", "BAD json_decref()",          \
                "refcount",     "%d", (int)((json)->refcount),      \
                "type",         "%d", (int)((json)->type),          \
                NULL                                                \
            );                                                      \
        } else {                                                    \
            json_decref(json);                                      \
        }                                                           \
        (json) = 0;                                                 \
    }                                                               \
}
#define JSON_INCREF(json)                                           \
{                                                                   \
    if(json) {                                                      \
        if((int)((json)->refcount) <= 0 || (json)->type > 7 || (json)->type < 0) { \
            gobj_log_error(0, LOG_OPT_TRACE_STACK,                  \
                "gobj",         "%s", __FILE__,                     \
                "function",     "%s", __FUNCTION__,                 \
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,        \
                "msg",          "%s", "BAD json_incref()",          \
                "refcount",     "%d", (int)((json)->refcount),      \
                "type",         "%d", (int)((json)->type),          \
                NULL                                                \
            );                                                      \
        } else {                                                    \
            json_incref(json);                                      \
        }                                                           \
    }                                                               \
}

#define json_array_backward(array, index, value) \
    for(index = json_array_size(array) - 1; \
        index >= 0 && (value = json_array_get(array, index)); \
        index--)

/*---------------------------------*
 *          KW
 *---------------------------------*/
typedef void (*incref_fn_t)(void *);
typedef void (*decref_fn_t)(void *);
typedef json_t * (*serialize_fn_t)(hgobj gobj, void *ptr);
typedef void * (*deserialize_fn_t)(hgobj gobj, json_t *jn);

#define KW_DECREF(ptr)      \
    if(ptr) {               \
        kw_decref(ptr);     \
        (ptr) = 0;          \
    }

#define KW_INCREF(ptr)      \
    if(ptr) {               \
        kw_incref(ptr);     \
    }

typedef enum {
    KW_REQUIRED         = 0x0001,   // Log error message if not exist.
    KW_CREATE           = 0x0002,   // Create if not exist
    KW_WILD_NUMBER      = 0x0004,   // For numbers work with real/int/bool/string without error logging
    KW_EXTRACT          = 0x0008,   // Extract (delete) the key on read from dictionary.
    KW_BACKWARD         = 0x0010,   // Search backward in lists or arrays
} kw_flag_t;

PUBLIC int kw_add_binary_type(
    const char *binary_field_name,
    const char *serialized_field_name,
    serialize_fn_t serialize_fn,
    deserialize_fn_t deserialize_fn,
    incref_fn_t incref_fn,
    decref_fn_t decref_fn
);
PUBLIC json_t *kw_serialize( // return the same kw
    hgobj gobj,
    json_t *kw
);
PUBLIC json_t *kw_deserialize( // return the same kw
    hgobj gobj,
    json_t *kw
);
PUBLIC json_t *kw_incref(json_t *kw);
PUBLIC json_t *kw_decref(json_t* kw);

PUBLIC BOOL kw_has_key(json_t *kw, const char *key);

/**rst**
   Set delimiter. Default is '`'.
   Return previous delimiter
**rst**/
PUBLIC char kw_set_path_delimiter(char delimiter);

/**rst**
    Return the json value find by path
    Walk over dicts and lists
**rst**/
PUBLIC json_t *kw_find_path(hgobj gobj, json_t *kw, const char *path, BOOL verbose);

/**rst**
    Like json_object_set but with a path (doesn't create arrays, only objects)
**rst**/
PUBLIC int kw_set_dict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,   // The last word after delimiter (.) is the key
    json_t *value // owned
);

PUBLIC int kw_set_subdict_value(
    hgobj gobj,
    json_t* kw,
    const char *path,
    const char *key,
    json_t *value // owned
);

/**rst**
   Delete value searched by path
**rst**/
PUBLIC int kw_delete(
    hgobj gobj,
    json_t *kw,
    const char *path
);

/**rst**
   Delete sub-key
**rst**/
PUBLIC int kw_delete_subkey(hgobj gobj, json_t *kw, const char *path, const char *key);


/**rst**
    Get a the idx of string in a json list.
    Return -1 if not found
**rst**/
PUBLIC int kw_find_str_in_list(
    hgobj gobj,
    json_t *kw_list,
    const char *str
);

/**rst**
    Get a the idx of simple json item in a json list.
    Return -1 if not found
**rst**/
PUBLIC int kw_find_json_in_list(
    json_t *kw_list,  // not owned
    json_t *item  // not owned
);

/**rst**
   Return the ``path` dict value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_dict(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` list value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_list(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` int value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_int_t kw_get_int(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_int_t default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` real value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC double kw_get_real(
    hgobj gobj,
    json_t *kw,
    const char *path,
    double default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` boolean value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC BOOL kw_get_bool(
    hgobj gobj,
    json_t *kw,
    const char *path,
    BOOL default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` string value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC const char *kw_get_str(
    hgobj gobj,
    json_t *kw,
    const char *path,
    const char *default_value,
    kw_flag_t flag
);

/**rst**
   Get any value from an json object searched by path.
   Return the ``path` json value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_dict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,  // owned
    kw_flag_t flag
);

/************************************************************************
    WARNING

    **duplicate** is a copy with new references
            to duplicate you must use json_deep_copy()

    **clone** is a copy with incref references

 ************************************************************************/

/**rst**
    Return a new kw only with the keys got by path.
    It's not a deep copy, new keys are the paths.
    Not valid with lists.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_path(
    hgobj gobj,
    json_t *kw,     // owned
    const char **paths
);

/**rst**
    Return a new kw only with the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_keys(
    hgobj gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
);

/**rst**
    Return a new kw except the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_not_keys(
    hgobj gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
);

/**rst**
    Remove from kw1 all keys in kw2
    kw2 can be a string, dict, or list.
**rst**/
PUBLIC int kw_pop(
    json_t *kw1, // NOT owned
    json_t *kw2  // NOT owned
);

/**rst**
    Match a json dict with a json filter
    Only compare str/int/real/bool items
**rst**/
PUBLIC BOOL kw_match_simple(
    json_t *kw,         // NOT owned
    json_t *jn_filter   // owned
);
typedef BOOL (*kw_match_fn)(
    json_t *kw,         // NOT owned
    json_t *jn_filter   // owned
);

/**rst**
    HACK Convention: private data begins with "_".
    Delete private keys (only first level)
**rst**/
PUBLIC int kw_delete_private_keys(
    json_t *kw  // NOT owned
);

/**rst**
    HACK Convention: metadata begins with "__".
    Delete metadata keys (only first level)
**rst**/
PUBLIC int kw_delete_metadata_keys(
    json_t *kw  // NOT owned
);

PUBLIC int kw_walk(
    hgobj gobj,
    json_t *kw, // not owned
    int (*callback)(hgobj gobj, json_t *kw, const char *key, json_t *value)
);

/*---------------------------------*
 *          KWID
 *---------------------------------*/




/***************************************************************************
    Utility for databases.
    Get a json list or dict, get the **first** record that match `id`
    Convention:
        - If it's a list of dict: the records have "id" field as primary key
        - If it's a dict, the key is the `id`
 ***************************************************************************/
PUBLIC json_t *kwid_get(
    hgobj gobj,
    json_t *kw,  // NOT owned
    const char *id,
    json_t *default_value,
    kw_flag_t flag,
    size_t *idx     // If not null set the idx in case of array
);

#ifdef __cplusplus
}
#endif
