#pragma once

#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "common.h"

typedef struct _PRE_2_POST_CONTEXT {

	PVOID SwappedBuffer;

} PRE_2_POST_CONTEXT, *PPRE_2_POST_CONTEXT;

#define PRE_2_POST_TAG      'ppBS'
#define BUFFER_SWAP_TAG		'XCBS'

extern NPAGED_LOOKASIDE_LIST Pre2PostContextList;




#endif