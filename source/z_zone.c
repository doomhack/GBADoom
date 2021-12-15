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
// $Log:$
//
// DESCRIPTION:
//	Zone Memory Allocation. Neat.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomdef.h"
#include "doomtype.h"
#include "lprintf.h"


//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of no value to free a cachable block,
//  because it will get overwritten automatically if needed.
//

#define ZONEID	0x1d4a11

const unsigned int maxHeapSize = (256 * 1024);

#ifndef GBA
    static int running_count = 0;
#endif

typedef struct memblock_s
{
    unsigned int size:24;	// including the header and possibly tiny fragments
    unsigned int tag:4;	// purgelevel
    void**		user;	// NULL if a free block
    struct memblock_s*	next;
    struct memblock_s*	prev;
} memblock_t;


typedef struct
{
    // start / end cap for linked list
    memblock_t	blocklist;
    memblock_t*	rover;
} memzone_t;

memzone_t*	mainzone;

//
// Z_Init
//
void Z_Init (void)
{
    memblock_t*	block;

    unsigned int heapSize = maxHeapSize;

    //We can now alloc all of the rest fo the memory.
    do
    {
        mainzone = malloc(heapSize);
        heapSize -= 4;

    } while(mainzone == NULL);

    heapSize += 4;

    lprintf(LO_INFO,"Z_Init: Heapsize is %d bytes.", heapSize);

    // set the entire zone to one free block
    mainzone->blocklist.next =
    mainzone->blocklist.prev =
    block = (memblock_t *)( (byte *)mainzone + sizeof(memzone_t) );

    mainzone->blocklist.user = (void *)mainzone;
    mainzone->blocklist.tag = PU_STATIC;
    mainzone->rover = block;

    block->prev = block->next = &mainzone->blocklist;

    // NULL indicates a free block.
    block->user = NULL;

    block->size = heapSize - sizeof(memzone_t);
}


//
// Z_Free
//
void Z_Free (void* ptr)
{
    memblock_t*		block;
    memblock_t*		other;

    if(ptr == NULL)
        return;

    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

    if (block->user > (void **)0x100)
    {
        // smaller values are not pointers
        // Note: OS-dependend?

        // clear the user's mark
        *block->user = 0;
    }

    // mark as free
    block->user = NULL;
    block->tag = 0;


#ifndef GBA
    running_count -= block->size;
    printf("Free: %d\n", running_count);
#endif

    other = block->prev;

    if (!other->user)
    {
        // merge with previous free block
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;

        if (block == mainzone->rover)
            mainzone->rover = other;

        block = other;
    }

    other = block->next;
    if (!other->user)
    {
        // merge the next free block onto the end
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;

        if (other == mainzone->rover)
            mainzone->rover = block;
    }
}



//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#define MINFRAGMENT		64


void* Z_Malloc(int size, int tag, void **user)
{
    int		extra;
    memblock_t*	start;
    memblock_t* rover;
    memblock_t* newblock;
    memblock_t*	base;

    size = (size + 3) & ~3;

    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // account for size of block header
    size += sizeof(memblock_t);

    // if there is a free block behind the rover,
    //  back up over them
    base = mainzone->rover;

    if (!base->prev->user)
    base = base->prev;

    rover = base;
    start = base->prev;

    do
    {
        if (rover == start)
        {
            // scanned all the way around the list
            I_Error ("Z_Malloc: failed on allocation of %i bytes", size);
        }

        if (rover->user)
        {
            if (rover->tag < PU_PURGELEVEL)
            {
                // hit a block that can't be purged,
                //  so move base past it
                base = rover = rover->next;
            }
            else
            {
                // free the rover block (adding the size to base)

                // the rover can be the base block
                base = base->prev;
                Z_Free ((byte *)rover+sizeof(memblock_t));
                base = base->next;
                rover = base->next;
            }
        }
        else
            rover = rover->next;

    } while (base->user || base->size < size);


    // found a block big enough
    extra = base->size - size;

    if (extra >  MINFRAGMENT)
    {
        // there will be a free fragment after the allocated block
        newblock = (memblock_t *) ((byte *)base + size );
        newblock->size = extra;

        // NULL indicates free block.
        newblock->user = NULL;
        newblock->tag = 0;
        newblock->prev = base;
        newblock->next = base->next;
        newblock->next->prev = newblock;

        base->next = newblock;
        base->size = size;
    }

    if (user)
    {
        // mark as an in use block
        base->user = user;
        *(void **)user = (void *) ((byte *)base + sizeof(memblock_t));
    }
    else
    {
        if (tag >= PU_PURGELEVEL)
            I_Error ("Z_Malloc: an owner is required for purgable blocks");

        // mark as in use, but unowned
        base->user = (void *)2;
    }

    base->tag = tag;

    // next allocation will start looking here
    mainzone->rover = base->next;

#ifndef GBA
    running_count += base->size;
    printf("Alloc: %d (%d)\n", base->size, running_count);
#endif

    return (void *) ((byte *)base + sizeof(memblock_t));
}

void* Z_Calloc(size_t count, size_t size, int tag, void **user)
{
    const size_t bytes = count * size;
    void* ptr = Z_Malloc(bytes, tag, user);

    if(ptr)
        memset(ptr, 0, bytes);

    return ptr;
}

char* Z_Strdup(const char* s)
{
    const unsigned int len = strlen(s);

    if(!len)
        return NULL;

    char* ptr = Z_Malloc(len+1, PU_STATIC, NULL);

    if(ptr)
        strcpy(ptr, s);

    return ptr;
}

void* Z_Realloc(void *ptr, size_t n, int tag, void **user)
{
    void *p = Z_Malloc(n, tag, user);

    if (ptr)
    {
        memblock_t *block = (memblock_t *)((char *) ptr - sizeof(memblock_t));

        memcpy(p, ptr, n <= block->size ? n : block->size);

        Z_Free(ptr);

        if (user) // in case Z_Free nullified same user
            *user = p;
    }
    return p;
}

//
// Z_FreeTags
//
void Z_FreeTags(int lowtag, int hightag)
{
    memblock_t*	block;
    memblock_t*	next;

    for (block = mainzone->blocklist.next ;
         block != &mainzone->blocklist ;
         block = next)
    {
        // get link before freeing
        next = block->next;

        // free block?
        if (!block->user)
            continue;

        if (block->tag >= lowtag && block->tag <= hightag)
            Z_Free ( (byte *)block+sizeof(memblock_t));
    }
}

//
// Z_CheckHeap
//
void Z_CheckHeap (void)
{
    memblock_t*	block;

    for (block = mainzone->blocklist.next ; ; block = block->next)
    {
        if (block->next == &mainzone->blocklist)
        {
            // all blocks have been hit
            break;
        }

        if ( (byte *)block + block->size != (byte *)block->next)
            I_Error ("Z_CheckHeap: block size does not touch the next block\n");

        if ( block->next->prev != block)
            I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");

        if (!block->user && !block->next->user)
            I_Error ("Z_CheckHeap: two consecutive free blocks\n");
    }
}
