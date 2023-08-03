/***********************************************************************
 *          COMM_PROT.C
 *
 *          Communication protocols register
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include "helpers.h"
#include "comm_prot.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char __initialized__ = FALSE;
PRIVATE dl_list_t dl_prot;

/***************************************************************************
 *  Register a gclass with a communication protocol
 ***************************************************************************/
PUBLIC int comm_prot_register(const char *schema, gclass_name_t gclass_name)
{
    if(!__initialized__) {
        __initialized__ = TRUE;
        dl_init(&dl_prot);
    }

    return 0;
}

/***************************************************************************
 *  Get the schema of an url
 ***************************************************************************/
PUBLIC const char *comm_prot_get_schema(const char *url)
{
    const char *schema;

    return schema;
}

/***************************************************************************
 *  Get the gclass name implementing the schema
 ***************************************************************************/
PUBLIC gclass_name_t comm_prot_get_gclass(const char *schema)
{
    gclass_name_t gclass_name;

    return gclass_name;
}
