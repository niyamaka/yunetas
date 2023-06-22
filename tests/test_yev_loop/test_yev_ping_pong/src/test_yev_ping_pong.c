/****************************************************************************
 *          test_yev_ping_pong
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <gobj.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>

/***************************************************************
 *              Constants
 ***************************************************************/
#define BUFFER_SIZE (8*1024) // TODO si aumento se muere, el buffer transmitido es la mitad

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int yev_server_callback(yev_event_t *event);
PRIVATE int yev_client_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;
const char *server_url = "tcp://localhost:2222";

static char PING[] = "PING\n";

gbuffer *gbuf_server_tx = 0;
yev_event_t *yev_server_tx = 0;

gbuffer *gbuf_server_rx = 0;
yev_event_t *yev_server_rx = 0;

gbuffer *gbuf_client_tx = 0;
yev_event_t *yev_client_tx = 0;

gbuffer *gbuf_client_rx = 0;
yev_event_t *yev_client_rx = 0;

uint64_t t;
unsigned msgsec = 0;
BOOL dump = FALSE;

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        NULL,
        2024,
        &yev_loop
    );

    /*--------------------------------*
     *      Setup server
     *--------------------------------*/
    yev_event_t *yev_server_accept = yev_create_accept_event(
        yev_loop,
        yev_server_callback,
        NULL
    );
    int fd_listen = yev_setup_accept_event(
        yev_server_accept,
        server_url,     // server_url,
        0,              // backlog, default 512
        FALSE           // shared
    );
    if(fd_listen < 0) {
        gobj_trace_msg(0, "Error setup listen on %s", server_url);
        exit(0);
    }

    yev_start_event(yev_server_accept, NULL);

    /*--------------------------------*
     *      Setup client
     *--------------------------------*/
    yev_event_t *yev_client_connect = yev_create_connect_event(
        yev_loop,
        yev_client_callback,
        NULL
    );
    int fd_connect = yev_setup_connect_event(
        yev_client_connect,
        server_url,     // client_url
        NULL            // local bind
    );
    if(fd_connect < 0) {
        gobj_trace_msg(0, "Error setup connect to %s", server_url);
        exit(0);
    }

    yev_start_event(yev_client_connect, NULL);

    /*--------------------------------*
     *      Begin run loop
     *--------------------------------*/
    t = start_msectimer(1000);
    yev_loop_run(yev_loop);

    /*--------------------------------*
     *      Stop
     *--------------------------------*/
    yev_stop_event(yev_server_accept);
    yev_stop_event(yev_server_rx);
    yev_stop_event(yev_server_tx);

    yev_stop_event(yev_client_connect);
    yev_stop_event(yev_client_rx);
    yev_stop_event(yev_client_tx);

    yev_destroy_event(yev_server_accept);
    yev_destroy_event(yev_server_rx);
    yev_destroy_event(yev_server_tx);

    yev_destroy_event(yev_client_connect);
    yev_destroy_event(yev_client_rx);
    yev_destroy_event(yev_client_tx);

    yev_loop_destroy(yev_loop);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    BOOL stopped = (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE;

    if(dump) {
        gobj_trace_msg(
            gobj, "yev server callback %s%s", yev_event_type_name(yev_event), stopped ? ", STOPPED" : ""
        );
    }
    int srv_cli_fd;

    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                msgsec++;

                if(test_msectimer(t)) {
                    printf("Msg/sec %u\r", msgsec); fflush(stdout);
                    msgsec = 0;
                    t = start_msectimer(1000);
                }

                /*
                 *  Save received data to transmit: do echo
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Server receiving");
                }
                gbuffer_reset_wr(gbuf_server_tx);
                gbuffer_append_gbuf(gbuf_server_tx, yev_event->gbuf);

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, gbuf_server_tx, "Server transmitting");
                }
                yev_start_event(yev_server_tx, gbuf_server_tx);

                /*
                 *  Re-arm read
                 */
                gbuffer_reset_rd(yev_event->gbuf);
                yev_start_event(yev_server_rx, yev_event->gbuf);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                // Write ended
            }
            break;

        case YEV_ACCEPT_TYPE:
            {
                // Create a srv_cli structure
                srv_cli_fd = yev_event->result;

                /*
                 *  Ready to receive
                 */
                gbuf_server_rx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                yev_server_rx = yev_create_read_event(
                    yev_event->yev_loop,
                    yev_server_callback,
                    NULL,
                    srv_cli_fd
                );
                yev_start_event(yev_server_rx, gbuf_server_rx);

                /*
                 *  Read to Transmit
                 */
                gbuf_server_tx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                yev_server_tx = yev_create_write_event(
                    yev_event->yev_loop,
                    yev_server_callback,
                    NULL,
                    srv_cli_fd
                );
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    BOOL stopped = (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE;

    if(dump) {
        gobj_trace_msg(
            gobj, "yev client callback %s%s", yev_event_type_name(yev_event), stopped ? ", STOPPED" : ""
        );
    }
    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                /*
                 *  Save received data to transmit: do echo
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Client receiving");
                }
                gbuffer_reset_wr(gbuf_client_tx);
                gbuffer_append_gbuf(gbuf_client_tx, yev_event->gbuf);

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, gbuf_client_tx, "Client transmitting");
                }
                yev_start_event(yev_client_tx, gbuf_client_tx);

                /*
                 *  Re-arm read
                 */
                gbuffer_reset_rd(yev_event->gbuf);
                yev_start_event(yev_client_rx, yev_event->gbuf);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                // Write ended
            }
            break;

        case YEV_CONNECT_TYPE:
            {
                /*
                 *  Ready to receive
                 */
                gbuf_client_rx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                yev_client_rx = yev_create_read_event(
                    yev_event->yev_loop,
                    yev_client_callback,
                    NULL,
                    yev_event->fd
                );
                yev_start_event(yev_client_rx, gbuf_client_rx);

                /*
                 *  Transmit
                 */
                gbuf_client_tx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);

//                for(int i= 0; i<BUFFER_SIZE/2; i++) { // TODO quita el /2 para depurar el espacio en los gbuffer
//                    gbuffer_append_char(gbuf_client_tx, 'A');
//                }

                gbuffer_append_string(gbuf_client_tx, PING);

                yev_client_tx = yev_create_write_event(
                    yev_event->yev_loop,
                    yev_client_callback,
                    NULL,
                    yev_event->fd
                );

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Client transmitting");
                }
                yev_start_event(yev_client_tx, gbuf_client_tx);
            }
            break;
        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    //gobj_set_deep_tracing(2);           // TODO TEST
    //gobj_set_global_trace(0, TRUE);     // TODO TEST

#ifdef DEBUG
    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);
#endif

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // startup_persistent_attrs
        NULL, // end_persistent_attrs
        0,  // load_persistent_attrs
        0,  // save_persistent_attrs
        0,  // remove_persistent_attrs
        0,  // list_persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        60*1024L,  // max_block, largest memory block
        120*1024L   // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);


    /*--------------------------------*
     *      Test
     *--------------------------------*/
    do_test();

    return gobj_get_exit_code();
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    yev_loop->running = 0;
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
}
