/***********************************************************************
 *              gbuffer.c
 *
 *              Buffer growable and overflowable
 *
 *              Copyright (c) 2013-2023 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/

#include <ctype.h>
#include <errno.h>
#include "kwid.h"
#include "gbuffer.h"

/***************************************************************
 *              Constants
 ***************************************************************/

#define __trace_gobj_gbuffers__(gobj)       (gobj_trace_level(gobj) & TRACE_GBUFFERS)

/***************************************************************************
 *  Crea un gbuf de tamaño de datos 'data_size'
 *  Retorna NULL if error
 ***************************************************************************/
PUBLIC gbuffer_t *gbuffer_create(
    size_t data_size,
    size_t max_memory_size)
{
    /*---------------------------------*
     *   Alloc memory
     *---------------------------------*/
    gbuffer_t *gbuf = GBMEM_MALLOC(sizeof(*gbuf));
    if(!gbuf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory",
            "sizeof",       "%d", (int)sizeof(struct gbuffer_s),
            NULL
        );
        return NULL;
    }

    gbuf->data = GBMEM_MALLOC(data_size+1);
    if(!gbuf->data) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory",
            "data_size",    "%d", (int)data_size,
            NULL
        );
        GBMEM_FREE(gbuf);
        return NULL;
    }

    /*---------------------------------*
     *   Inicializa atributos
     *---------------------------------*/
    gbuf->data_size = data_size;
    gbuf->max_memory_size = max_memory_size;

    gbuf->tail = 0;
    gbuf->curp = 0;
    gbuf->refcount = 1;

    /*----------------------------*
     *   Retorna pointer a gbuf
     *----------------------------*/
    if(__trace_gobj_gbuffers__(0)) {
        gobj_log_debug(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_GBUFFERS,
            "msg",          "%s", "🚛🟦 Creating gbuffer",
            "pointer",      "%p", gbuf,
            "data_size",    "%d", (int)data_size,
            NULL);
    }

    return gbuf;
}

/***************************************************************************
 *    Realloc buffer
 ***************************************************************************/
PRIVATE BOOL gbuffer_realloc(gbuffer_t *gbuf, size_t need_size)
{
    size_t more;
    char *new_buf;

    more = gbuf->data_size + MAX(gbuf->data_size, need_size);
    if((more + 1) > gbuf->max_memory_size) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL_ERROR,
            "msg",              "%s", "MAXIMUM SPACE REACHED",
            "more",             "%ld", more,
            "max_memory_size",  "%ld", gbuf->max_memory_size,
            NULL
        );
        return FALSE;
    }

    /*
     *  Realloc buffer
     */
    new_buf = GBMEM_REALLOC(gbuf->data, more+1);
    if(!new_buf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NOT ENOUGH SPACE",
            "more",         "%d", (int)more,
            NULL
        );
        return FALSE;
    }
    gbuf->data = new_buf;
    gbuf->data_size = more;
    if(__trace_gobj_gbuffers__(0)) {
        gobj_log_debug(0,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_GBUFFERS,
            "msg",          "%s", "🚛🟦🟦Realloc gbuffer",
            "pointer",      "%p", gbuf,
            "more",         "%d", (int)more,
            NULL
        );
    }
    return TRUE;
}

/***************************************************************************
 *    Elimina paquete
 ***************************************************************************/
PUBLIC void gbuffer_remove(gbuffer_t *gbuf)
{
    if(__trace_gobj_gbuffers__(0)) {
        gobj_log_debug(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_GBUFFERS,
            "msg",          "%s", "🚛🟥 Deleting gbuffer",
            "pointer",      "%p", gbuf,
            NULL
        );
    }

    /*-----------------------*
     *  Libera la memoria
     *-----------------------*/
    if(gbuf->label) {
        GBMEM_FREE(gbuf->label);
        gbuf->label = 0;
    }
    if(gbuf->data) {
        GBMEM_FREE(gbuf->data);
        gbuf->data = 0;
    }

    GBMEM_FREE(gbuf);
}

/***************************************************************************
 *    Incr ref
 ***************************************************************************/
PUBLIC void gbuffer_incref(gbuffer_t *gbuf)
{
    if(!gbuf || gbuf->refcount <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "BAD gbuf_incref()",
            NULL
        );
        return;
    }
    ++(gbuf->refcount);
}

/***************************************************************************
 *    Decr ref
 ***************************************************************************/
PUBLIC void gbuffer_decref(gbuffer_t *gbuf)
{
    if(!gbuf || gbuf->refcount <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "BAD gbuf_decref()",
            NULL
        );
        return;
    }
    --(gbuf->refcount);
    if(gbuf->refcount == 0)
        gbuffer_remove(gbuf);
}

/***************************************************************************
 *    Devuelve puntero a la actual posición de salida de datos
 ***************************************************************************/
PUBLIC void * gbuffer_cur_rd_pointer(gbuffer_t *gbuf)
{
    char *p;

    if(!gbuf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }
    p = gbuf->data;
    p += gbuf->curp;
    return p;
}

/***************************************************************************
 *    Devuelve puntero a la actual posición de entrada de datos
 ***************************************************************************/
PRIVATE inline void * _gbuffer_cur_wr_pointer(gbuffer_t *gbuf)
{
    return gbuf->data + gbuf->tail;
}




                /***************************
                 *      READING
                 ***************************/




/***************************************************************************
 *    Reset current output pointer
 ***************************************************************************/
PUBLIC void gbuffer_reset_rd(gbuffer_t *gbuf)
{
    gbuf->curp = 0;
}

/***************************************************************************
 *  Set current output pointer
 ***************************************************************************/
PUBLIC int gbuffer_set_rd_offset(gbuffer_t *gbuf, size_t position)
{
    if(position >= gbuf->data_size) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "len GREATER than data_size",
            "len",          "%d", (int)position,
            "data_size",    "%d", (int)gbuf->data_size,
            NULL
        );
        return -1;
    }
    if(position > gbuf->tail) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "len GREATER than curp",
            "len",          "%d", (int)position,
            "curp",         "%d", (int)gbuf->curp,
            NULL
        );
        return -1;
    }
    gbuf->curp = position;
    return 0;
}

/***************************************************************************
 *  Unget char
 ***************************************************************************/
PUBLIC int gbuffer_ungetc(gbuffer_t *gbuf, char c)
{
    if(gbuf->curp > 0) {
        gbuf->curp--;

        *(gbuf->data + gbuf->curp) = c;
    }

    return 0;
}

/***************************************************************************
 *  Get current output pointer
 ***************************************************************************/
PUBLIC size_t gbuffer_get_rd_offset(gbuffer_t *gbuf)
{
    return gbuf->curp;
}

/***************************************************************************
 *    Saca 'len' bytes del gbuf.
 *    Devuelve al pointer al primer byte de los 'len' datos sacados.
 *    WARNING primero hay que asegurarse que existen 'len' bytes disponibles
 ***************************************************************************/
PUBLIC void * gbuffer_get(gbuffer_t *gbuf, size_t len)
{
    if(!gbuf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }

    if(len <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf: len is <= 0",
            "len",          "%d", (int)len,
            NULL
        );
        return 0;
    }

    size_t rest = gbuffer_leftbytes(gbuf);

    if(len > rest) {
        return 0;
    }
    char *p;
    p = gbuf->data;
    p += gbuf->curp;
    gbuf->curp += len;     /* elimina los bytes del gbuf */
    return p;
}

/***************************************************************************
 *  pop one byte
 ***************************************************************************/
PUBLIC char gbuffer_getchar(gbuffer_t *gbuf)
{
    char *p = gbuffer_get(gbuf, 1);
    if(p)
        return *p;
    else
        return 0;
}

/***************************************************************************
 *  return the chunk of data available
 ***************************************************************************/
PUBLIC size_t gbuffer_chunk(gbuffer_t *gbuf)
{
    size_t ln = gbuffer_leftbytes(gbuf);
    size_t chunk_size = MIN(gbuf->data_size, ln);

    return chunk_size;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *gbuffer_getline(gbuffer_t *gbuf, char separator)
{
    if(gbuffer_leftbytes(gbuf)<=0) {
        // No more chars
        return (char *)0;
    }
    char *begin = gbuffer_cur_rd_pointer(gbuf); // TODO WARNING with chunks this will failed!
    register char *p;
    while((p=gbuffer_get(gbuf,1))) {
        if(*p == separator) {
            *p = 0;
            return begin;
        }
    }

    return begin;
}




                /***************************
                 *      WRITTING
                 ***************************/




/***************************************************************************
 *    Get current writing position
 ***************************************************************************/
PUBLIC void *gbuffer_cur_wr_pointer(gbuffer_t *gbuf)
{
    return _gbuffer_cur_wr_pointer(gbuf);
}

/***************************************************************************
 *    Reset current input pointer
 ***************************************************************************/
PUBLIC void gbuffer_reset_wr(gbuffer_t *gbuf)
{
    gbuf->tail = 0;
    gbuf->curp = 0;

    /*
     *  Put final null
     */
    char *p = gbuf->data;
    p += gbuf->tail;
    *p = 0;
}

/***************************************************************************
 *  Set current input pointer
 *  Useful when using gbuf as write buffer
 ***************************************************************************/
PUBLIC int gbuffer_set_wr(gbuffer_t *gbuf, size_t offset)
{
    if(offset > gbuf->data_size) { // WARNING collateral damage? (original>=), version 3.4.3
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "offset GREATER than data_size",
            "offset",       "%d", (int)offset,
            "data_size",    "%d", (int)gbuf->data_size,
            NULL
        );
        return -1;
    }
    gbuf->tail = offset;

    /*
     *  Put final null
     */
    char *p = gbuf->data;
    p += gbuf->tail;
    *p = 0;

    return 0;
}

/***************************************************************************
 *   Append 'len' bytes
 *   Retorna bytes guardados
 ***************************************************************************/
PUBLIC size_t gbuffer_append(gbuffer_t *gbuf, void *bf, size_t len)
{
    char *p;

    if(!bf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "bf is NULL",
            NULL
        );
        return 0;
    }
    if(!len) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "len is ZERO",
            NULL
        );
        return 0;
    }
    if(gbuffer_freebytes(gbuf) < len) {
        gbuffer_realloc(gbuf, len);
    }
    if(gbuffer_freebytes(gbuf) < len) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NOT ENOUGH SPACE, append only 'free' bytes",
            "free",         "%d", (int)gbuffer_freebytes(gbuf),
            "needed",       "%d", (int)len,
            NULL
        );
        len = gbuffer_freebytes(gbuf);
        if(len==0) {
            return 0;
        }
    }

    /*--------------------------*
     *  Using data in memory
     *--------------------------*/
    p = gbuf->data;
    memmove(p + gbuf->tail, bf, len);
    gbuf->tail += len;
    *(p + gbuf->tail) = 0;
    return len;
}

/***************************************************************************
 *   Append 'len' bytes
 *   Retorna bytes guardados
 ***************************************************************************/
PUBLIC size_t gbuffer_append_string(gbuffer_t *gbuf, const char *s)
{
    return gbuffer_append(gbuf, (void *)s, strlen(s));
}

/***************************************************************************
 *   Append 'c'
 *   Retorna bytes guardados
 ***************************************************************************/
PUBLIC size_t gbuffer_append_char(gbuffer_t *gbuf, char c)
{
    return gbuffer_append(gbuf, &c, 1);
}

/***************************************************************************
 *   Append a gbuf to another gbuf. The two gbuf must exist.
 ***************************************************************************/
PUBLIC int gbuffer_append_gbuf(gbuffer_t *dst, gbuffer_t *src)
{
    register char *p;
    size_t ln = gbuffer_leftbytes(src);
    size_t chunk_size = MIN(src->data_size, ln);

    while(ln>0) {
        p = gbuffer_get(src, chunk_size);
        if(!p) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gbuf_get() FAILED",
                NULL
            );
            return -1;
        }
        if(gbuffer_append(dst, p, chunk_size)!=chunk_size) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "gbuf_append() FAILED",
                NULL
            );
            return -1;
        }
        ln = gbuffer_leftbytes(src);
        chunk_size = MIN(src->data_size, ln);
    }
    return 0;
}

/***************************************************************************
 *    ESCRITURA del mensaje.
 *    Añade message with format al final del mensaje
 *    Return bytes written
 ***************************************************************************/
PUBLIC int gbuffer_printf(gbuffer_t *gbuf, const char *format, ...)
{
    BOOL ret;
    va_list ap;

    va_start(ap, format);
    ret = gbuffer_vprintf(gbuf, format, ap);
    va_end(ap);
    return ret;
}

/***************************************************************************
 *    ESCRITURA del mensaje.
 *    Añade message with format al final del mensaje
 *    Return bytes written
 ***************************************************************************/
PUBLIC int gbuffer_vprintf(gbuffer_t *gbuf, const char *format, va_list ap)
{
    char *bf;
    size_t len;
    int written;

    va_list aq;

    /*--------------------------*
     *  Using data in memory
     *--------------------------*/
    bf = _gbuffer_cur_wr_pointer(gbuf);
    len = gbuffer_freebytes(gbuf);

    va_copy(aq, ap);
    written = vsnprintf(bf, len, format, aq);
    va_end(aq);
    if(written < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "vsnprintf FAILED",
            NULL
        );
        written = 0;

    } else if(written >= (int)len) {
        if(!gbuffer_realloc(gbuf, (size_t)written)) {
            written = 0;
        } else {
            bf = _gbuffer_cur_wr_pointer(gbuf);
            len = gbuffer_freebytes(gbuf);
            va_copy(aq, ap);
            written = vsnprintf(bf, len, format, aq);
            va_end(aq);
            if(written < 0) {
                gobj_log_error(0, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "vsnprintf FAILED",
                    "errno",        "%d", errno,
                    "strerror",     "%s", strerror(errno),
                    NULL
                );
                written = 0;
            } else if(written >= (int)len) {
                gobj_log_error(0, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "NOT ENOUGH SPACE",
                    "free",         "%d", (int)gbuffer_freebytes(gbuf),
                    "needed",       "%d", (int)len,
                    NULL
                );
                gbuf->tail += len;
            } else {
                gbuf->tail += (size_t)written;
            }
        }
    } else {
        gbuf->tail += (size_t)written;
    }
    return written;
}






                /***************************
                 *      Util
                 ***************************/




/***************************************************************************
 *  Return pointer to first position of data
 ***************************************************************************/
PUBLIC void *gbuffer_head_pointer(gbuffer_t *gbuf)
{
    char *p;

    if(!gbuf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }
    p = gbuf->data;
    return p;
}

/***************************************************************************
 *  Reset write/read pointers
 ***************************************************************************/
PUBLIC void gbuffer_clear(gbuffer_t *gbuf)
{
    if(!gbuf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return;
    }
    gbuffer_reset_wr(gbuf);
    gbuffer_reset_rd(gbuf);
}

/***************************************************************************
 *    Devuelve los bytes que quedan en el packet por procesar
 ***************************************************************************/
PUBLIC size_t gbuffer_leftbytes(gbuffer_t *gbuf)
{
    if(!gbuf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return 0;
    }
    return gbuf->tail - gbuf->curp;
}

/***************************************************************************
 *    Devuelve el número de bytes escritos
 ***************************************************************************/
PUBLIC size_t gbuffer_totalbytes(gbuffer_t *gbuf)
{
    return gbuf->tail;
}

/***************************************************************************
 *    Devuelve el espacio libre de bytes de datos
 ***************************************************************************/
PUBLIC size_t gbuffer_freebytes(gbuffer_t *gbuf)
{
    return gbuf->data_size - gbuf->tail;
}

/***************************************************************************
 *    Set label
 ***************************************************************************/
PUBLIC int gbuffer_setlabel(gbuffer_t *gbuf, const char *label)
{
    if(!gbuf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf is NULL",
            NULL
        );
        return -1;
    }
    if(gbuf->label) {
        GBMEM_FREE(gbuf->label);
        gbuf->label = 0;
    }
    if(label) {
        gbuf->label = gobj_strdup(label);
    }
    return 0;
}

/***************************************************************************
 *    Get label
 ***************************************************************************/
PUBLIC char *gbuffer_getlabel(gbuffer_t *gbuf)
{
    if(!gbuf) {
        return 0;
    }
    return gbuf->label;
}

/***************************************************************************
 *    Set mark
 ***************************************************************************/
PUBLIC void gbuffer_setmark(gbuffer_t *gbuf, size_t mark)
{
    if(gbuf) {
        gbuf->mark = mark;
    }
}

/***************************************************************************
 *    Get mark
 ***************************************************************************/
PUBLIC size_t gbuffer_getmark(gbuffer_t *gbuf)
{
    if(gbuf) {
        return gbuf->mark;
    }
    return 0;
}

/***************************************************************************
 *  Serialize GBUFFER
 ***************************************************************************/
PUBLIC json_t *gbuffer_serialize(
    hgobj gobj,
    gbuffer_t *gbuf  // not owned
)
{
    json_t *__gbuffer__ = json_object();
    char *label = gbuffer_getlabel(gbuf);
    json_t *jn_label = json_string(label?label:"");

    json_object_set_new(__gbuffer__, "label", jn_label);
    size_t mark = gbuffer_getmark(gbuf);
    json_object_set_new(__gbuffer__, "mark", json_integer((json_int_t)mark));

    /*
     *  Convert to base64 the content of gbuffer
     */
    // TODO use chunks!!
    char *p = gbuffer_cur_rd_pointer(gbuf);
    size_t len = gbuffer_leftbytes(gbuf);
    gbuffer_t *gbuf_base64 = gbuffer_string_to_base64(p, len);

    char *data = gbuffer_cur_rd_pointer(gbuf_base64);
    json_t *jn_bf = json_string(data);
    if(!jn_bf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot serialize gbuffer data, json_string() FAILED",
            NULL
        );
        jn_bf = json_string(""); // TODO must return null? or use the FUTURE binary option
    }
    json_object_set_new(__gbuffer__, "data", jn_bf);
    gbuffer_decref(gbuf_base64);
    return __gbuffer__;
}

/***************************************************************************
 *  Deserialize GBUFFER
 ***************************************************************************/
PUBLIC gbuffer_t *gbuffer_deserialize(
    hgobj gobj,
    const json_t *jn  // not owned
) {
    const json_t *__gbuffer__ = jn;
//    const char *label = kw_get_str(__gbuffer__, "label", "", KW_REQUIRED); TODO include kw???
//    int32_t mark = kw_get_int(__gbuffer__, "mark", 0, KW_REQUIRED);
//    const char *base64 = kw_get_str(__gbuffer__, "data", "", KW_REQUIRED);

    const char *label = json_string_value(json_object_get(__gbuffer__, "label"));
    size_t mark = (size_t)json_integer_value(json_object_get(__gbuffer__, "mark"));
    const char *base64 = json_string_value(json_object_get(__gbuffer__, "data"));

    gbuffer_t *gbuf_decoded = gbuffer_base64_to_string(base64, strlen(base64));
    const char *data = gbuffer_cur_rd_pointer(gbuf_decoded);

    size_t len = strlen(data);
    gbuffer_t *gbuf;
    if(!len) {
        gbuf = gbuffer_create(1, 1);
    } else {
        gbuf = gbuffer_create(len, len);
    }
    if(!gbuf) {
        gbuffer_decref(gbuf_decoded);
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "cannot deserialize gbuffer",
            NULL
        );
        return 0;
    }
    if(len) {
        gbuffer_append(gbuf, (void *)data, len);
    }
    gbuffer_setlabel(gbuf, label);
    gbuffer_setmark(gbuf, mark);
    gbuffer_decref(gbuf_decoded);
    return gbuf;
}

#define Assert(Cond) if (!(Cond)) return (size_t)-1;

static const char Base64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
   The following encoding technique is taken from RFC 1521 by Borenstein
   and Freed.  It is reproduced here in a slightly edited form for
   convenience.

   A 65-character subset of US-ASCII is used, enabling 6 bits to be
   represented per printable character. (The extra 65th character, "=",
   is used to signify a special processing function.)

   The encoding process represents 24-bit groups of input bits as output
   strings of 4 encoded characters. Proceeding from left to right, a
   24-bit input group is formed by concatenating 3 8-bit input groups.
   These 24 bits are then treated as 4 concatenated 6-bit groups, each
   of which is translated into a single digit in the base64 alphabet.

   Each 6-bit group is used as an index into an array of 64 printable
   characters. The character referenced by the index is placed in the
   output string.

                         Table 1: The Base64 Alphabet

      Value Encoding  Value Encoding  Value Encoding  Value Encoding
          0 A            17 R            34 i            51 z
          1 B            18 S            35 j            52 0
          2 C            19 T            36 k            53 1
          3 D            20 U            37 l            54 2
          4 E            21 V            38 m            55 3
          5 F            22 W            39 n            56 4
          6 G            23 X            40 o            57 5
          7 H            24 Y            41 p            58 6
          8 I            25 Z            42 q            59 7
          9 J            26 a            43 r            60 8
         10 K            27 b            44 s            61 9
         11 L            28 c            45 t            62 +
         12 M            29 d            46 u            63 /
         13 N            30 e            47 v
         14 O            31 f            48 w         (pad) =
         15 P            32 g            49 x
         16 Q            33 h            50 y

   Special processing is performed if fewer than 24 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a quantity.  When fewer than 24 input
   bits are available in an input group, zero bits are added (on the
   right) to form an integral number of 6-bit groups.  Padding at the
   end of the data is performed using the '=' character.

   Since all base64 input is an integral number of octets, only the
         -------------------------------------------------
   following cases can arise:

       (1) the final quantum of encoding input is an integral
           multiple of 24 bits; here, the final unit of encoded
       output will be an integral multiple of 4 characters
       with no "=" padding,
       (2) the final quantum of encoding input is exactly 8 bits;
           here, the final unit of encoded output will be two
       characters followed by two "=" padding characters, or
       (3) the final quantum of encoding input is exactly 16 bits;
           here, the final unit of encoded output will be three
       characters followed by one "=" padding character.
   */

PRIVATE size_t b64_encode(const char* src, size_t srclength, char* target, size_t targsize)
{
    size_t datalength = 0;
    char input[3];
    char output[4];
    size_t i;

    while (2 < srclength) {
        input[0] = *src++;
        input[1] = *src++;
        input[2] = *src++;
        srclength -= 3;

        output[0] = input[0] >> 2;
        output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
        output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
        output[3] = input[2] & 0x3f;
        Assert(output[0] < 64);
        Assert(output[1] < 64);
        Assert(output[2] < 64);
        Assert(output[3] < 64);

        if (datalength + 4 > targsize)
            return (size_t)(-1);
        target[datalength++] = Base64[(int)output[0]];
        target[datalength++] = Base64[(int)output[1]];
        target[datalength++] = Base64[(int)output[2]];
        target[datalength++] = Base64[(int)output[3]];
    }

    /* Now we worry about padding. */
    if (0 != srclength) {
        /* Get what's left. */
        input[0] = input[1] = input[2] = '\0';
        for (i = 0; i < srclength; i++)
            input[i] = *src++;

        output[0] = input[0] >> 2;
        output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
        output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
        Assert(output[0] < 64);
        Assert(output[1] < 64);
        Assert(output[2] < 64);

        if (datalength + 4 > targsize)
            return (size_t)(-1);
        target[datalength++] = Base64[(int)output[0]];
        target[datalength++] = Base64[(int)output[1]];
        if (srclength == 1)
            target[datalength++] = Pad64;
        else
            target[datalength++] = Base64[(int)output[2]];
        target[datalength++] = Pad64;
    }
    if (datalength >= targsize)
        return (size_t)(-1);
    target[datalength] = '\0';  /* Returned value doesn't count \0. */
    return (datalength);
}

/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 64 numbers into three 8 bit bytes in the target area.
   it returns the number of data bytes stored at the target, or -1 on error.
 */

PRIVATE size_t b64_decode(const char *src, uint8_t *target, size_t targsize)
{
    size_t tarindex, state;
    char *pos, ch;

    state = 0;
    tarindex = 0;

    while ((ch = *src++) != '\0') {
        if (isspace((int)ch))    /* Skip whitespace anywhere. */
            continue;

        if (ch == Pad64)
            break;

        pos = strchr(Base64, ch);
        if (pos == 0)       /* A non-base64 character. */
            return (size_t)(-1);

        switch (state) {
        case 0:
            if (target) {
                if ((size_t)tarindex >= targsize)
                    return (size_t)(-1);
                target[tarindex] = (uint8_t)((pos - Base64) << 2);
            }
            state = 1;
            break;
        case 1:
            if (target) {
                if ((size_t)tarindex + 1 >= targsize)
                    return (size_t)(-1);
                target[tarindex]   |=  (uint8_t)(size_t)((pos - Base64) >> 4);
                target[tarindex+1]  = (uint8_t)(size_t)(((pos - Base64) & 0x0f) << 4);
            }
            tarindex++;
            state = 2;
            break;
        case 2:
            if (target) {
                if ((size_t)tarindex + 1 >= targsize)
                    return (size_t)(-1);
                target[tarindex]   |=  (uint8_t)(size_t)((pos - Base64) >> 2);
                target[tarindex+1]  = (uint8_t)(size_t)(((pos - Base64) & 0x03) << 6);
            }
            tarindex++;
            state = 3;
            break;
        case 3:
            if (target) {
                if ((size_t)tarindex >= targsize)
                    return (size_t)(-1);
                target[tarindex] |= (uint8_t)(pos - Base64);
            }
            tarindex++;
            state = 0;
            break;
        default:
            return (size_t)-1;
        }
    }

    /*
     * We are done decoding Base-64 chars.  Let's see if we ended
     * on a byte boundary, and/or with erroneous trailing characters.
     */

    if (ch == Pad64) {      /* We got a pad char. */
        ch = *src++;        /* Skip it, get next. */
        switch (state) {
        case 0:     /* Invalid = in first position */
        case 1:     /* Invalid = in second position */
            return (size_t)(-1);

        case 2:     /* Valid, means one byte of info */
            /* Skip any number of spaces. */
            for ((void)NULL; ch != '\0'; ch = *src++)
                if (!isspace((int)ch))
                    break;
            /* Make sure there is another trailing = sign. */
            if (ch != Pad64)
                return (size_t)(-1);
            ch = *src++;        /* Skip the = */
            /* Fall through to "single trailing =" case. */
            /* FALLTHROUGH */

        case 3:     /* Valid, means two bytes of info */
            /*
             * We know this char is an =.  Is there anything but
             * whitespace after it?
             */
            for ((void)NULL; ch != '\0'; ch = *src++)
                if (!isspace((int)ch))
                    return (size_t)(-1);

            /*
             * Now make sure for cases 2 and 3 that the "extra"
             * bits that slopped past the last full byte were
             * zeros.  If we don't check them, they become a
             * subliminal channel.
             */
            if (target && target[tarindex] != 0)
                return (size_t)(-1);
        }
    } else {
        /*
         * We ended by seeing the end of the string.  Make sure we
         * have no partial bytes lying around.
         */
        if (state != 0)
            return (size_t)(-1);
    }

    return (tarindex);
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE size_t b64_output_encode_len(size_t input_length)
{
    size_t output_length = 4*((input_length + 4)/3) + 1;
    return output_length;
}
/*****************************************************************
 *
 *****************************************************************/
PRIVATE size_t b64_output_decode_len(size_t input_length)
{
    size_t output_length = (input_length/4 + 1)*3 + 1;
    return output_length;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC gbuffer_t *gbuffer_string_to_base64(const char *src, size_t len)
{
    size_t output_len = b64_output_encode_len(len);

    gbuffer_t *gbuf_output = gbuffer_create(output_len, output_len);
    if(!gbuf_output) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuffer_create() FAILED",
            "len",          "%d", (int)len,
            NULL
        );
        return 0;
    }
    char *p = _gbuffer_cur_wr_pointer(gbuf_output);
    size_t encoded = b64_encode(src, len, p, output_len);
    if(encoded == (size_t)-1) {
        gbuffer_decref(gbuf_output);
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "len",          "%d", (int)len,
            NULL
        );
        return 0;

    }
    gbuffer_set_wr(gbuf_output, encoded);
    return gbuf_output;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC gbuffer_t *gbuffer_base64_to_string(const char* base64, size_t base64_len)
{
    size_t output_len = b64_output_decode_len(base64_len);

    gbuffer_t *gbuf_output = gbuffer_create(output_len, output_len);
    if(!gbuf_output) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            "len",          "%d", (int)output_len,
            NULL
        );
        return 0;
    }
    uint8_t *p = _gbuffer_cur_wr_pointer(gbuf_output);
    size_t decoded = b64_decode(base64, p, output_len);
    if(decoded == (size_t)-1) {
        gbuffer_decref(gbuf_output);
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gbuffer_create() FAILED",
            "len",          "%d", (int)output_len,
            NULL
        );
        return 0;

    }
    gbuffer_set_wr(gbuf_output, decoded);
    return gbuf_output;
}

/***************************************************************************
 *      Dump json into gbuf
 ***************************************************************************/
PRIVATE int dump2gbuf(const char *buffer, size_t size, void *data)
{
    gbuffer_t *gbuf = data;

    if(size > 0) {
        gbuffer_append(gbuf, (void *)buffer, size);
    }
    return 0;
}
PUBLIC gbuffer_t *json2gbuf(
    gbuffer_t *gbuf,
    json_t *jn, // owned
    size_t flags)
{
    if(!gbuf) {
        gbuf = gbuffer_create(4*1024, gobj_get_maximum_block());
    }
    json_dump_callback(jn, dump2gbuf, gbuf, flags);
    JSON_DECREF(jn);
    return gbuf;
}

/***************************************************************************
 *  Convert a json message from gbuffer into a json struct.
 *  gbuf is stolen
 *  Return 0 if error
 ***************************************************************************/
PRIVATE size_t on_load_callback(void *bf, size_t bfsize, void *data)
{
    gbuffer_t *gbuf = data;

    size_t chunk = gbuffer_leftbytes(gbuf);
    if(!chunk)
        return 0;
    if(chunk > bfsize)
        chunk = bfsize;
    memcpy(bf, gbuffer_get(gbuf, chunk), chunk);
    return chunk;
}

PUBLIC json_t * gbuf2json(
    gbuffer_t *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
)
{
    size_t flags = JSON_DECODE_ANY|JSON_ALLOW_NUL;
    json_error_t jn_error;
    json_t *jn_msg = json_load_callback(on_load_callback, gbuf, flags, &jn_error);

    if(!jn_msg) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "json_load_callback() FAILED",
                "error",        "%s", jn_error.text,
                NULL
            );
            if(verbose > 1) {
                gbuffer_reset_rd(gbuf);
                gobj_trace_dump_gbuf(
                    0,
                    gbuf,
                    "Bad json format"
                );
            }
        }
    }
    gbuffer_decref(gbuf);
    return jn_msg;
}

/*****************************************************************
 *      Log hexa dump gbuffer
 *  WARNING only print a chunk size of data.
 *****************************************************************/
PUBLIC void gobj_trace_dump_gbuf(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *fmt,
    ...
)
{
    if(!gbuf) {
        return;
    }

    uint8_t *bf = gbuffer_cur_rd_pointer(gbuf);
    size_t len = gbuffer_chunk(gbuf);


    json_t *jn_data = json_object();
    char *label = gbuffer_getlabel(gbuf);
    json_object_set_new(jn_data, "pointer", json_sprintf("%p", gbuf));
    json_object_set_new(jn_data, "len", json_integer(len));
    json_object_set_new(jn_data, "label", json_string(label?label:""));
    json_object_set_new(jn_data, "mark", json_integer((json_int_t)gbuffer_getmark(gbuf)));
    json_object_set_new(jn_data, "data", tdump2json(bf, len));

    va_list ap;
    va_start(ap, fmt);
    trace_vjson(gobj, jn_data, "trace_dump_gbuf", fmt, ap);
    va_end(ap);

    json_decref(jn_data);
}

/*****************************************************************
 *      Log hexa dump gbuffer
 *  WARNING only print a chunk size of data.
 *****************************************************************/
PUBLIC void gobj_trace_dump_full_gbuf(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *fmt,
    ...
)
{
    if(!gbuf) {
        return;
    }

    uint8_t *bf = gbuffer_head_pointer(gbuf);
    size_t len = gbuffer_totalbytes(gbuf);

    json_t *jn_data = json_object();
    char *label = gbuffer_getlabel(gbuf);
    json_object_set_new(jn_data, "pointer", json_sprintf("%p", gbuf));
    json_object_set_new(jn_data, "len", json_integer(len));
    json_object_set_new(jn_data, "label", json_string(label?label:""));
    json_object_set_new(jn_data, "mark", json_integer((json_int_t)gbuffer_getmark(gbuf)));
    json_object_set_new(jn_data, "data", tdump2json(bf, len));

    va_list ap;
    va_start(ap, fmt);
    trace_vjson(gobj, jn_data, "trace_dump_full_gbuf", fmt, ap);
    va_end(ap);

    json_decref(jn_data);
}
