/****************************************************************************
 *          C_PROT_MODBUS_MASTER.H
 *          Prot_modbus_master GClass.
 *
 *          Modbus protocol (master side)
 *
 *          Copyright (c) 2021-2023 Niyamaka.
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
GOBJ_DECLARE_GCLASS(GC_PROT_MODBUS_M);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_prot_modbus_master(void);

#ifdef __cplusplus
}
#endif
