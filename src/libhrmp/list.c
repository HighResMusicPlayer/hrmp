/*
 * Copyright (C) 2025 The HighResMusicPlayer community
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* hrmp */
#include <hrmp.h>
#include <list.h>

/* system */
#include <stdlib.h>
#include <string.h>

int
hrmp_list_create(struct list** list)
{
   struct list* l = NULL;

   if (list == NULL)
   {
      return 1;
   }

   l = malloc(sizeof(struct list));
   if (l == NULL)
   {
      return 1;
   }

   l->head = NULL;
   l->tail = NULL;
   l->size = 0;

   *list = l;

   return 0;
}

void
hrmp_list_destroy(struct list* list)
{
   struct list_entry* current = NULL;

   if (list == NULL)
   {
      return;
   }

   current = list->head;
   while (current != NULL)
   {
      struct list_entry* next = current->next;

      free(current);
      current = next;
   }

   free(list);
}

bool
hrmp_list_empty(const struct list* list)
{
   if (list == NULL)
   {
      return true;
   }

   return list->size == 0;
}

size_t
hrmp_list_size(const struct list* list)
{
   if (list == NULL)
   {
      return 0;
   }

   return list->size;
}

static int
hrmp_list_insert_node(struct list* list, const char* value, bool at_head)
{
   struct list_entry* node = NULL;

   if (list == NULL || value == NULL)
   {
      return 1;
   }

   node = malloc(sizeof(struct list_entry));
   if (node == NULL)
   {
      return 1;
   }

   memset(node->value, 0, sizeof(node->value));
   /* Ensure NUL-termination */
   strncpy(node->value, value, MAX_PATH - 1);
   node->value[MAX_PATH - 1] = '\0';

   node->next = NULL;
   node->list = list;

   if (hrmp_list_empty(list))
   {
      list->head = node;
      list->tail = node;
   }
   else if (at_head)
   {
      node->next = list->head;
      list->head = node;
   }
   else
   {
      list->tail->next = node;
      list->tail = node;
   }

   list->size++;

   return 0;
}

int
hrmp_list_append(struct list* list, const char* value)
{
   return hrmp_list_insert_node(list, value, false);
}

int
hrmp_list_prepend(struct list* list, const char* value)
{
   return hrmp_list_insert_node(list, value, true);
}

struct list_entry*
hrmp_list_head(struct list* list)
{
   if (list == NULL)
   {
      return NULL;
   }

   return list->head;
}

struct list_entry*
hrmp_list_next(struct list_entry* entry)
{
   if (entry == NULL)
   {
      return NULL;
   }

   return entry->next;
}

struct list_entry*
hrmp_list_prev(struct list_entry* entry)
{
   struct list* list;
   struct list_entry* current;
   struct list_entry* prev;

   if (entry == NULL)
   {
      return NULL;
   }

   list = entry->list;
   if (list == NULL)
   {
      return NULL;
   }

   /* If entry is the head or list has fewer than 2 elements, return head */
   if (list->head == NULL || list->head == entry || list->head->next == NULL)
   {
      return list->head;
   }

   prev = list->head;
   current = list->head->next;

   while (current != NULL && current != entry)
   {
      prev = current;
      current = current->next;
   }

   if (current == NULL)
   {
      /* entry not found in list */
      return NULL;
   }

   return prev;
}
