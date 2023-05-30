/****************************************************************************
 *          C_PROT_HTTP_CLI.H
 *          Prot_http_cli GClass.
 *
 *          Protocol http as client
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(GC_PROT_HTTP_CL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_prot_http_cli(void);

#ifdef __cplusplus
}
#endif