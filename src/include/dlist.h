/*
 * Copyright (C) 2025 HighResMusicPlayer
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may
 * be used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HRMP_DLIST_H
#define HRMP_DLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>

/** @struct dlist_element
 * Define an element of the double linked list.
 */
struct dlist_element
{
   void* data;                 /**< The data */
   struct dlist_element* prev; /**< The previous element */
   struct dlist_element* next; /**< The next element */
};

/** @struct dlist
 * A double linked list implementation where the client owns
 * both the value pointers.
 */
struct dlist
{
   struct dlist_element* data; /**< The elements */
};

/**
 *  Create a dlist.
 *  @param new_dlist The new dlist.
 *  @return On success 0 is returned.
 */
int
hrmp_dlist_create(struct dlist** new_dlist);

/**
 *  Append an element into the dlist.
 *  @param dlist The dlist to insert into.
 *  @param value The value to insert.
 *  @return On success 0 is returned.
 */
int
hrmp_dlist_append(struct dlist* dlist, void* value);

/**
 *  Get an element from the dlist.
 *  @param dlist The dlist to get from.
 *  @param index The index
 *  @return The element, or NULL if none exists.
 */
void*
hrmp_dlist_get(struct dlist* dlist, int index);

/**
 *  Remove an element from the dlist.
 *  @param dlist The dlist to remove from.
 *  @param index The index
 *  @return On success 0 is returned.
 */
int
hrmp_dlist_remove(struct dlist* dlist, int index);

/**
 *  Get the size of the dlist.
 *  @param dlist The dlist to get the size of.
 *  @return The size of the dlist.
 */
int
hrmp_dlist_size(struct dlist* dlist);

/**
 *  Destroy the dlist.
 *  @param dlist The dlist to destroy.
 */
void
hrmp_dlist_destroy(struct dlist* dlist);

#ifdef __cplusplus
}
#endif

#endif
