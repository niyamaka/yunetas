/****************************************************************************
 *              helpers.h
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

/*
 *  Dependencies
 */
#include <stdio.h>
#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              DL_LIST Structures
 ***************************************************************/
#define DL_ITEM_FIELDS              \
    struct dl_list_s *__dl__;       \
    struct dl_item_s  *__next__;    \
    struct dl_item_s  *__prev__;    \
    size_t __id__;

typedef struct dl_item_s {
    DL_ITEM_FIELDS
} dl_item_t;

typedef struct dl_list_s {
    struct dl_item_s *head;
    struct dl_item_s *tail;
    size_t __itemsInContainer__;
    size_t __last_id__; // auto-incremental, always.
} dl_list_t;

typedef void (*fnfree)(void *);


/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*------------------------------------*
 *      Directory/Files
 *------------------------------------*/
PUBLIC int newdir(const char *path, int permission);
PUBLIC int newfile(const char *path, int permission, BOOL overwrite);
PUBLIC int open_exclusive(const char *path, int flags, int permission);  // open exclusive

PUBLIC off_t filesize(const char *path);
PUBLIC off_t filesize2(int fd);

PUBLIC int lock_file(int fd);
PUBLIC int unlock_file(int fd);

PUBLIC BOOL is_regular_file(const char *path);
PUBLIC BOOL is_directory(const char *path);
PUBLIC BOOL file_exists(const char *directory, const char *filename);
PUBLIC BOOL subdir_exists(const char *directory, const char *subdir);
PUBLIC int file_remove(const char *directory, const char *filename);

PUBLIC int mkrdir(const char *path, int index, int permission);
PUBLIC int rmrdir(const char *root_dir);
PUBLIC int rmrcontentdir(const char *root_dir);

/*------------------------------------*
 *          Strings
 *------------------------------------*/
PUBLIC char *delete_right_char(char *s, char x);
PUBLIC char *delete_left_char(char *s, char x);
PUBLIC char *build_path(char *bf, size_t bfsize, ...);

/*------------------------------------*
 *          Json
 *------------------------------------*/
PUBLIC json_t *load_json_from_file(
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
);

PUBLIC int save_json_to_file(
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
);

/***************************************************************************
 *
 *  type can be: str, int, real, bool, null, dict, list
 *  Example:

static json_desc_t jn_xxx_desc[] = {
    // Name         Type        Default
    {"string",      "str",      ""},
    {"string2",     "str",      "Pepe"},
    {"integer",     "int",      "0660"},     // beginning with "0":octal,"x":hexa, others: integer
    {"boolean",     "bool",     "false"},
    {0}   // HACK important, final null
};
 ***************************************************************************/
typedef struct {
    const char *name;
    const char *type;   // type can be: "str", "int", "real", "bool", "null", "dict", "list"
    const char *defaults;
} json_desc_t;

PUBLIC json_t *create_json_record(
    const json_desc_t *json_desc
);

PUBLIC json_t *bits2str(
    const char **string_table,
    uint64_t bits
);

/*---------------------------------*
 *      Utilities functions
 *---------------------------------*/
typedef int (*view_fn_t)(const char *format, ...);;
PUBLIC void tdump(const char *prefix, const uint8_t *s, size_t len, view_fn_t view, int nivel);
PUBLIC json_t *tdump2json(const uint8_t *s, size_t len);
PUBLIC int print_json2(const char *label, json_t *jn);
PUBLIC char *current_timestamp(char *bf, size_t bfsize);
PUBLIC time_t start_sectimer(time_t seconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_sectimer(time_t value);      /* Return TRUE if timer has finish */
PUBLIC uint64_t start_msectimer(uint64_t miliseconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_msectimer(uint64_t value);           /* Return TRUE if timer has finish */
PUBLIC uint64_t time_in_miliseconds(void);   // Return current time in miliseconds
PUBLIC uint64_t time_in_seconds(void);       // Return current time in seconds (standart time(&t))

PUBLIC char *helper_quote2doublequote(char *str);
PUBLIC char *helper_doublequote2quote(char *str);
PUBLIC json_t * anystring2json(const char *bf, size_t len, BOOL verbose);
PUBLIC json_int_t jn2integer(json_t *jn_var);
PUBLIC void nice_size(char* bf, size_t bfsize, uint64_t bytes);
PUBLIC void delete_right_blanks(char *s);
PUBLIC void delete_left_blanks(char *s);
PUBLIC void left_justify(char *s);
PUBLIC char *strntoupper(char* s, size_t n);
PUBLIC char *strntolower(char* s, size_t n);
PUBLIC int change_char(char *s, char old_c, char new_c);

/**rst**
    Split a string by delim returning the list of strings.
    Return filling `list_size` if not null with items size,
        It MUST be initialized to 0 (no limit) or to maximum items wanted.
    WARNING Remember free with split_free2().
    HACK: No, It does NOT include the empty strings!
**rst**/
PUBLIC const char ** split2(const char *str, const char *delim, int *list_size);
PUBLIC void split_free2(const char **list);

/**rst**
    Split string `str` by `delim` chars returning the list of strings.
    Return filling `list_size` if not null with items size,
        It MUST be initialized to 0 (no limit) or to maximum items wanted.
    WARNING Remember free with split_free3().
    HACK: Yes, It does include the empty strings!
**rst**/
PUBLIC const char **split3(const char *str, const char *delim, int *plist_size);
PUBLIC void split_free3(const char **list);

/*---------------------------------*
 *      Double link  functions
 *---------------------------------*/
PUBLIC int dl_init(dl_list_t *dl);
PUBLIC void *dl_first(dl_list_t *dl);
PUBLIC void *dl_last(dl_list_t *dl);
PUBLIC void *dl_next(void *curr);
PUBLIC void *dl_prev(void *curr);
PUBLIC int dl_add(dl_list_t *dl, void *item);
PUBLIC int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *));
PUBLIC void dl_flush(dl_list_t *dl, void (*fnfree)(void *));
PUBLIC size_t dl_size(dl_list_t *dl);


#ifdef __cplusplus
}
#endif
