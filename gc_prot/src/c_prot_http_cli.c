/****************************************************************************
 *          c_prot_http_cli.c
 *          Prot_http_cli GClass.
 *
 *          Protocol http as client
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <ghttp_parser.h>
#include <c_esp_transport.h>
#include <parse_url.h>
#include "c_prot_http_cli.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "url",              SDF_RD,         "",         "Url to connect"),
SDATA (DTP_STRING,  "jwt",              SDF_RD,         "",         "JWT"),
SDATA (DTP_STRING,  "cert_pem",         SDF_RD,         "",         "SSL server certification, PEM str format"),
SDATA (DTP_BOOLEAN, "use_ssl",          SDF_RD,         "false",    "Set internally if schema is secure"),
SDATA (DTP_INTEGER, "subscriber",       0,              0,          "subscriber of output-events. If null then subscriber is the parent"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRAFFIC         = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"traffic",         "Trace traffic"},
    {0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    char p;
    hgobj gobj_transport;
    GHTTP_PARSER *parsing_response;
    BOOL use_ssl;
    char schema[16];
    char host[120];
    char port[10];
    char path[32];
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:s}",
        "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
        "url", gobj_read_str_attr(gobj, "url")
    );
    priv->gobj_transport = gobj_create(gobj_name(gobj), GC_ESP_TRANSPORT, kw, gobj);
    gobj_set_bottom_gobj(gobj, priv->gobj_transport);

    priv->parsing_response = ghttp_parser_create(
        gobj,
        HTTP_RESPONSE,
        EV_ON_MESSAGE,
        FALSE
    );

    parse_url(
        gobj,
        gobj_read_str_attr(gobj, "url"),
        priv->schema, sizeof(priv->schema),
        priv->host, sizeof(priv->host),
        priv->port, sizeof(priv->port),
        priv->path, sizeof(priv->path),
        0, 0,
        FALSE
    );
    if(strlen(priv->path) > 0 && priv->path[strlen(priv->path)-1]=='/') {
        priv->path[strlen(priv->path)-1] = 0;
    }
    if(strlen(priv->schema) > 0 && priv->schema[strlen(priv->schema)-1]=='s') {
        priv->use_ssl = TRUE;
        gobj_write_bool_attr(gobj, "use_ssl", TRUE);
    }

    /*
     *  Child, default subscriber, the parent
     */
    hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_transport);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ghttp_parser_reset(priv->parsing_response);
    gobj_stop(priv->gobj_transport);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    EXEC_AND_RESET(ghttp_parser_destroy, priv->parsing_response)
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  Parse a http message
 *  Return -1 if error: you must close the socket.
 *  Return 0 if no new request.
 *  Return 1 if new request available in `request`.
 ***************************************************************************/
PRIVATE int parse_message(hgobj gobj, gbuffer *gbuf, GHTTP_PARSER *parser)
{
    size_t ln;
    while((ln=gbuffer_leftbytes(gbuf))>0) {
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        int n = ghttp_parser_received(parser, bf, ln);
        if (n == -1) {
            gobj_trace_dump_full_gbuf(gobj, gbuf, "ghttp_parser_received() FAILED");
            // Some error in parsing
            ghttp_parser_reset(parser);
            return -1;
        } else if (n > 0) {
            gbuffer_get(gbuf, (size_t)n);  // take out the bytes consumed
        }
    }
    return 0;
}

/***************************************************************************
 *  WARNING size for ugly json string (without indent)
 ***************************************************************************/
PRIVATE size_t kw_content_size(json_t *kw)
{
    char *sjn = json_dumps(kw, JSON_INDENT(0));
    size_t ln = sjn?strlen(sjn):0;
    GBMEM_FREE(sjn);
    ln += 2 * json_object_size(kw);
    return ln;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ghttp_parser_reset(priv->parsing_response);
    gobj_publish_event(gobj, EV_ON_OPEN, kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_publish_event(gobj, EV_ON_CLOSE, kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer *gbuf = (gbuffer *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s <- %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    int result = parse_message(gobj, gbuf, priv->parsing_response);
    if (result < 0) {
        gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *method = kw_get_str(gobj, kw, "method", "GET", 0);
    char *resource = gobj_strdup(kw_get_str(gobj, kw, "resource", "/", 0));
    const char *query = kw_get_str(gobj, kw, "query", "", 0);
    json_t *jn_headers_ = kw_get_dict(gobj, kw, "headers", 0, 0);
    json_t *jn_data_ = kw_get_dict(gobj, kw, "data", 0, 0);
    const char *http_version = "1.1";
    size_t content_length = 0;
    char *content = 0;
    const char *key; json_t *v;

    json_t *jn_headers = json_object();
    json_object_set_new(jn_headers,
        "User-Agent", json_sprintf("yuneta-%s", YUNETA_VERSION)
    );
    if(jn_data_) {
        json_object_set_new(
            jn_headers, "Content-Type", json_string("application/json; charset=utf-8")
        );
    }
    json_object_set_new(jn_headers, "Connection", json_string("keep-alive"));
    json_object_set_new(jn_headers, "Accept", json_string("*/*"));
    if(atoi(priv->port) == 80 || atoi(priv->port) == 443) {
        json_object_set_new(jn_headers, "Host", json_string(priv->host));
    } else {
        json_object_set_new(jn_headers, "Host", json_sprintf("%s:%d", priv->host, atoi(priv->port)));
    }
    if(jn_headers_) {
        json_object_update(jn_headers, jn_headers_);
    }

    const char *content_type = kw_get_str(gobj, jn_headers, "Content-Type", "", 0);
    BOOL set_form_urlencoded = strcmp(content_type, "application/x-www-form-urlencoded")==0?1:0;
    if(set_form_urlencoded) {
        if(!empty_string(query)) {
            content = gobj_strdup(query);
            content_length = strlen(content);
        } else {
            // for k in form_data:
            //     v = form_data[k]
            //     if not data:
            //         data += """%s=%s""" % (k,v)
            //     else:
            //         data += """&%s=%s""" % (k,v)

            size_t ln = kw_content_size(jn_data_);
            gbuffer *gbuf = gbuffer_create(ln, ln);
            int more = 0;
            json_object_foreach(jn_data_, key, v) {
                const char *value = json_string_value(v);
                if(empty_string(value)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "http header key without value",
                        "key",          "%s", key,
                        NULL
                    );
                    continue;
                }
                if(more) {
                    gbuffer_append_string(gbuf, "&");
                }
                gbuffer_append_string(gbuf, key);
                gbuffer_append_string(gbuf, "=");
                gbuffer_append_string(gbuf, value);
                more++;
            }
            char *p = gbuffer_cur_rd_pointer(gbuf);
            content = gobj_strdup(p);
            content_length = strlen(content);
            GBUFFER_DECREF(gbuf);
        }
        if (strcasecmp(method, "GET")==0) {
            // parameters must be in resource
            size_t l = strlen(resource) + content_length + 1;
            char *new_resource = GBMEM_MALLOC(l);
            snprintf(new_resource, l, "%s%s%s", resource, "?", content);
            GBMEM_FREE(resource);
            resource = new_resource;
            GBMEM_FREE(content);
        }

    } else {
        if(jn_data_) {
            content = json_dumps(jn_data_, JSON_COMPACT|JSON_INDENT(0));
            content_length = strlen(content);
        }
    }

    size_t len = strlen(method) + strlen(resource) + kw_content_size(jn_headers) + content_length + 256;
    gbuffer *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuf_create() FAILED",
            NULL
        );
        GBMEM_FREE(resource);
        JSON_DECREF(jn_headers);
        GBMEM_FREE(content);
        KW_DECREF(kw);
        return -1;
    }
    gbuffer_printf(gbuf, "%s %s HTTP/%s\r\n", method, resource, http_version);
    json_object_foreach(jn_headers, key, v) {
        gbuffer_printf(gbuf, "%s:%s\r\n", key, json_string_value(v));
    }

    if(content) {
        gbuffer_printf(gbuf, "Content-Length: %d\r\n", (int)content_length);
    }
    gbuffer_printf(gbuf, "\r\n");
    if(content) {
        gbuffer_append(gbuf, content, content_length);
    }

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s -> %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    GBMEM_FREE(resource);
    JSON_DECREF(jn_headers);
    GBMEM_FREE(content);
    KW_DECREF(kw);

    json_t *kw_response = json_pack("{s:I}",
                                    "gbuffer", (json_int_t)(size_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw_response, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(GC_PROT_HTTP_CL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    if(gclass) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,          ac_connected,       ST_CONNECTED},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_SEND_MESSAGE,       ac_send_message,    0},
        {EV_DISCONNECTED,       ac_disconnected,    ST_DISCONNECTED},
        {EV_DROP,               ac_drop,            0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,   st_disconnected},
        {ST_CONNECTED,      st_connected},
        {0, 0}
    };

    event_type_t event_types[] = {
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_prot_http_cli(void)
{
    return create_gclass(GC_PROT_HTTP_CL);
}