// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//      Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//	Remark: this was the only stuff that, according
//	 to John Carmack, might have been useful for
//	 Quake.
//
//---------------------------------------------------------------------



#ifndef __Z_ZONE__
#define __Z_ZONE__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//
// ZONE MEMORY
// PU - purge tags.
// Tags < 100 are not overwritten until freed.
#define PU_STATIC		1	// static entire execution time
#define PU_LEVEL		2	// static until level exited
#define PU_LEVSPEC		3      // a special thinker in a level
#define PU_CACHE		4

#define PU_PURGELEVEL PU_CACHE



void	Z_Init (void);
void*	Z_Malloc (int size, int tag, void **ptr);
void    Z_Free (void *ptr);
void    Z_FreeTags (int lowtag, int hightag);
void    Z_CheckHeap (void);
void*   Z_Calloc(size_t count, size_t size, int tag, void **user);
char*   Z_Strdup(const char* s);
void*   Z_Realloc(void *ptr, size_t n, int tag, void **user);
void    Z_CheckHeap (void);





#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
