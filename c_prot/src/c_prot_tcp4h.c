/****************************************************************************
 *          c_prot_tcp4h.c
 *          Prot_tcp4h GClass.
 *
 *          Protocol tcp4h, messages with a header of 4 bytes
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <netinet/in.h>
#include <comm_prot.h>
#ifdef ESP_PLATFORM
    #include <c_esp_transport.h>
#endif
#ifdef __linux__
    #include <c_linux_transport.h>
#endif
#include <kwid.h>
#include <gbuffer.h>
#include "c_prot_tcp4h.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
typedef union {
    unsigned char bf[4];
    uint32_t len;
} HEADER_ERPL2;

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "url",              SDF_PERSIST,    "",         "Url to connect"),
SDATA (DTP_STRING,  "jwt",              SDF_PERSIST,    "",         "JWT"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,    "",         "SSL server certification, PEM str format"),
SDATA (DTP_INTEGER, "max_pkt_size",     SDF_WR,         "4048",     "Package maximum size"),
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
    gbuffer_t *last_pkt;  /* packet currently receiving */
    char bf_header_erpl2[sizeof(HEADER_ERPL2)];
    size_t idx_header;

    uint32_t max_pkt_size;
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

    if(!gobj_is_pure_child(gobj)) {
        /*
         *  Not pure child, explicitly use subscriber
         */
        hgobj subscriber = (hgobj)(size_t)gobj_read_integer_attr(gobj, "subscriber");
        if(subscriber) {
            gobj_subscribe_event(gobj, NULL, NULL, subscriber);
        }
    }

    SET_PRIV(max_pkt_size,      (uint32_t)gobj_read_integer_attr)
    if(priv->max_pkt_size == 0) {
        priv->max_pkt_size = (uint32_t)gobj_get_maximum_block();
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(max_pkt_size,    gobj_read_integer_attr)
        if(priv->max_pkt_size == 0) {
            priv->max_pkt_size = (uint32_t)gobj_get_maximum_block();
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(!bottom_gobj) {
        json_t *kw = json_pack("{s:s, s:s}",
            "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
            "url", gobj_read_str_attr(gobj, "url")
        );

        #ifdef ESP_PLATFORM
            hgobj gobj_bottom = gobj_create_pure_child(gobj_name(gobj), C_ESP_TRANSPORT, kw, gobj);
        #endif
        #ifdef __linux__
            hgobj gobj_bottom = gobj_create_pure_child(gobj_name(gobj), C_LINUX_TRANSPORT, kw, gobj);
        #endif
        gobj_set_bottom_gobj(gobj, gobj_bottom);
    }

    gobj_start(gobj_bottom_gobj(gobj));

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    gobj_stop(gobj_bottom_gobj(gobj));

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Local methods
                     ***************************/








                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_ON_OPEN, kw, gobj); // use the same kw
    } else {
        gobj_publish_event(gobj, EV_ON_OPEN, kw); // use the same kw
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if (gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), EV_ON_CLOSE, kw, gobj); // use the same kw
    } else {
        gobj_publish_event(gobj, EV_ON_CLOSE, kw); // use the same kw
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s <- %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    size_t pend_size = 0;
    size_t len = gbuffer_leftbytes(gbuf);
    while(len>0) {
        if(priv->last_pkt) {
            /*--------------------*
              *   Estoy a medias
              *--------------------*/
            pend_size = gbuffer_freebytes(priv->last_pkt); /* mira lo que falta */
            if(len >= pend_size) {
                /*----------------------------------------*
                 *   Justo lo que falta o
                 *   Resto de uno y principio de otro
                 *---------------------------------------*/
                gbuffer_append(
                    priv->last_pkt,
                    gbuffer_get(gbuf, pend_size),
                    pend_size
                );
                len -= pend_size;
                json_t *kw_tx = json_pack("{s:I}",
                    "gbuffer", (json_int_t)(size_t)priv->last_pkt
                );
                priv->last_pkt = 0;

                if (gobj_is_pure_child(gobj)) {
                    gobj_send_event(gobj_parent(gobj), EV_ON_MESSAGE, kw_tx, gobj);
                } else {
                    gobj_publish_event(gobj, EV_ON_MESSAGE, kw_tx);
                }

            } else { /* len < pend_size */
                /*-------------------------------------*
                  *   Falta todavía mas
                  *-------------------------------------*/
                gbuffer_append(
                    priv->last_pkt,
                    gbuffer_get(gbuf, len),
                    len
                );
                len = 0;
            }
        } else {
            /*--------------------*
             *   New packet
             *--------------------*/
            size_t need2header = sizeof(HEADER_ERPL2) - priv->idx_header;
            if(len < need2header) {
                memcpy(
                    priv->bf_header_erpl2 + priv->idx_header,
                    gbuffer_get(gbuf, len),
                    len
                );
                priv->idx_header += len;
                len = 0;
                continue;
            } else {
                memcpy(
                    priv->bf_header_erpl2 + priv->idx_header,
                    gbuffer_get(gbuf, need2header),
                    need2header
                );
                len -= need2header;
                priv->idx_header = 0;
            }

            /*
             *  Quita la cabecera
             */
            HEADER_ERPL2 header_erpl2;
            memmove((char *)&header_erpl2, priv->bf_header_erpl2, sizeof(HEADER_ERPL2));
            header_erpl2.len = ntohl(header_erpl2.len);
            header_erpl2.len -= sizeof(HEADER_ERPL2); // remove header

            if(header_erpl2.len > priv->max_pkt_size) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
                    "msg",          "%s", "TOO LONG SIZE",
                    "len",          "%d", header_erpl2.len,
                    NULL
                );
                gobj_trace_dump_gbuf(
                    gobj,
                    gbuf,
                    "ERROR: TOO LONG SIZE (%d)",
                    header_erpl2.len
                );
                gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
                break;
            }
            gbuffer_t *new_pkt = gbuffer_create(header_erpl2.len, header_erpl2.len);
            if(!new_pkt) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
                    "msg",          "%s", "gbuffer_create() FAILED",
                    "len",          "%d", header_erpl2.len,
                    NULL
                );
                gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
                break;
            }
            /*
             *  Put the data
             */
            if(len >= header_erpl2.len) {
                /* PAQUETE COMPLETO o MULTIPLE */
                if(header_erpl2.len > 0) {
                    // SSQ/SSR are 0 length
                    gbuffer_append(
                        new_pkt,
                        gbuffer_get(gbuf, header_erpl2.len),
                        header_erpl2.len
                    );
                    len -= header_erpl2.len;
                }
                json_t *kw_tx = json_pack("{s:I}",
                    "gbuffer", (json_int_t)(size_t)new_pkt
                );
                if (gobj_is_pure_child(gobj)) {
                    gobj_send_event(gobj_parent(gobj), EV_ON_MESSAGE, kw_tx, gobj);
                } else {
                    gobj_publish_event(gobj, EV_ON_MESSAGE, kw_tx);
                }

            } else { /* len < header_erpl2.len */
                /* PAQUETE INCOMPLETO */
                if(len>0) {
                    gbuffer_append(
                        new_pkt,
                        gbuffer_get(gbuf, len),
                        len
                    );
                }
                priv->last_pkt = new_pkt;
                len = 0;
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf_payload = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    gbuffer_t *gbuf_header;
    HEADER_ERPL2 header_erpl2;

    /*---------------------------*
     *      Send header
     *---------------------------*/
    memset(&header_erpl2, 0, sizeof(HEADER_ERPL2));
    gbuf_header = gbuffer_create(sizeof(HEADER_ERPL2), sizeof(HEADER_ERPL2));
    header_erpl2.len = htonl(gbuffer_leftbytes(gbuf_payload));
    gbuffer_append(
        gbuf_header,
        &header_erpl2,
        sizeof(HEADER_ERPL2)
    );
    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf_header, "%s -> %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }
    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf_header
    );
    gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_tx, gobj);

    /*---------------------------*
     *      Send payload
     *---------------------------*/
    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf_payload, "%s -> %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }
    kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf_payload
    );
    gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_tx, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

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
GOBJ_DEFINE_GCLASS(C_PROT_TCP4H);

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
        {EV_TX_READY,           0,                  0},
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
        {EV_ON_MESSAGE,     EVF_OUTPUT_EVENT},
        {EV_ON_OPEN,        EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,       EVF_OUTPUT_EVENT},
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

    /*----------------------------------------*
     *          Register comm protocol
     *----------------------------------------*/
    comm_prot_register(gclass_name, "tcph");
    comm_prot_register(gclass_name, "tcphs");

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_prot_tcp4h(void)
{
    return create_gclass(C_PROT_TCP4H);
}
