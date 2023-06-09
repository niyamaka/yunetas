/****************************************************************************
 *          c_linux_yuno.h
 *
 *          GClass __yuno__
 *          Low level
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>
#include "yuneta_ev_loop.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_YUNO);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_linux_yuno(void);

// PUBLIC int gobj_post_event(
//     hgobj dst,
//     gobj_event_t event,
//     json_t *kw,  // owned
//     hgobj src
// );

/*
 *  Get yuno event loop
 */
PUBLIC yev_loop_h yuno_event_loop(void);

/*
 *  Set quit/debug signals
 */
PUBLIC void yuno_catch_signals(void);

/*--------------------------------------------------*
 *  Denied ips (prevalence over allowed)
 *
 *  Allowed ips for authz without jwt
 *      (in addition to local ip with yuneta user)
 *--------------------------------------------------*/
PUBLIC BOOL is_ip_denied(hgobj yuno, const char *peername);
PUBLIC int add_denied_ip(hgobj yuno, const char *ip, BOOL denied); // denied: TRUE to deny, FALSE to not deny
PUBLIC int remove_denied_ip(hgobj yuno, const char *ip); // Remove from internal list

PUBLIC BOOL is_ip_allowed(hgobj yuno, const char *peername);
PUBLIC int add_allowed_ip(hgobj yuno, const char *ip, BOOL allowed); // allowed: TRUE to allow, FALSE to not allow
PUBLIC int remove_allowed_ip(hgobj yuno, const char *ip); // Remove from internal list


#ifdef __cplusplus
}
#endif
