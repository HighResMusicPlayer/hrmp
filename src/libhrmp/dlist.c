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

/* Based on https://github.com/sheredom/dlist.h */

#include <dlist.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define DLIST_CAST(type, x) ((type)x)

__attribute__((used))
int
hrmp_dlist_create(struct dlist** new_dlist)
{
   struct dlist* l = NULL;

   l = DLIST_CAST(struct dlist*, calloc(1, sizeof(struct dlist)));

   if (!l)
   {
      return 1;
   }

   l->data = NULL;

   *new_dlist = l;

   return 0;
}

__attribute__((used))
int
hrmp_dlist_append(struct dlist* dlist, void* value)
{
   struct dlist_element* n = NULL;
   struct dlist_element* e = NULL;

   if (dlist == NULL)
   {
      goto error;
   }

   e = dlist->data;

   if (e != NULL)
   {
      while (e->next != NULL)
      {
         e = e->next;
      }
   }

   n = DLIST_CAST(struct dlist_element*, calloc(1, sizeof(struct dlist_element)));

   if (!n)
   {
      goto error;
   }

   n->data = value;
   n->next = NULL;

   if (e == NULL)
   {
      n->prev = NULL;
      dlist->data = n;
   }
   else
   {
      n->prev = e;
      e->next = n;
   }

   return 0;

error:

   return 1;
}

__attribute__((used))
void*
hrmp_dlist_get(struct dlist* dlist, int index)
{
   struct dlist_element* e = NULL;

   if (dlist == NULL)
   {
      return NULL;
   }

   e = dlist->data;

   if (e == NULL)
   {
      return NULL;
   }

   for (int i = 0; i < index; i++)
   {
      e = e->next;

      if (e == NULL)
      {
         return NULL;
      }
   }

   return e->data;
}

__attribute__((used))
int
hrmp_dlist_remove(struct dlist* dlist, int index)
{
   struct dlist_element* e = NULL;

   if (dlist == NULL)
   {
      return 1;
   }

   e = dlist->data;

   if (e == NULL)
   {
      return 1;
   }

   if (index == 0)
   {
      dlist->data = e->next;

      if (e->next != NULL)
      {
         e->next->prev = NULL;
      }

      e->data = NULL;
      e->prev = NULL;
      e->next = NULL;
      free(e);

      return 0;
   }
   else
   {
      for (int i = 0; i < index; i++)
      {
         e = e->next;

         if (e == NULL)
         {
            return 1;
         }
      }

      e->prev->next = e->next;

      if (e->next != NULL)
      {
         e->next->prev = e->prev;
      }

      e->data = NULL;
      e->prev = NULL;
      e->next = NULL;
      free(e);

      return 0;
   }

   return 1;
}

__attribute__((used))
int
hrmp_dlist_size(struct dlist* dlist)
{
   int i = 0;
   struct dlist_element* e = NULL;

   if (dlist == NULL)
   {
      return 0;
   }

   e = dlist->data;
   while (e != NULL)
   {
      i++;
      e = e->next;
   }

   return i;
}

__attribute__((used))
void
hrmp_dlist_destroy(struct dlist* dlist)
{
   int size = 0;

   if (dlist != NULL)
   {
      size = hrmp_dlist_size(dlist);
      for (int i = 0; i < size; i++)
      {
         hrmp_dlist_remove(dlist, 0);
      }
      free(dlist);
      dlist = NULL;
   }
}
