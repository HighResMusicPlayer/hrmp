/*
 * Copyright (C) 2026 The HighResMusicPlayer community
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

#ifndef HRMP_LIST_H
#define HRMP_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hrmp.h>

#include <stdbool.h>
#include <stddef.h>

/** @struct list_entry
 * Node of a singly linked list of strings
 */
struct list;

struct list_entry
{
   char value[MAX_PATH];    /**< Stored string value (NUL-terminated) */
   struct list_entry* next; /**< Pointer to the next entry */
   struct list* list;       /**< Owning list */
};

/** @struct list
 * Singly linked list of strings
 */
struct list
{
   struct list_entry* head; /**< First entry in the list */
   struct list_entry* tail; /**< Last entry in the list */
   size_t size;             /**< Number of elements in the list */
};

/**
 * Create an empty list
 * @param list The created list
 * @return 0 if success, otherwise 1
 */
int
hrmp_list_create(struct list** list);

/**
 * Destroy a list and free all its entries
 * @param list The list
 */
void
hrmp_list_destroy(struct list* list);

/**
 * Is the list empty
 * @param list The list
 * @return true if the list is empty, otherwise false
 */
bool
hrmp_list_empty(const struct list* list);

/**
 * Get the number of elements in the list
 * @param list The list
 * @return The size of the list
 */
size_t
hrmp_list_size(const struct list* list);

/**
 * Append a value to the end of the list
 * @param list The list
 * @param value The string value to append
 * @return 0 if success, otherwise 1
 */
int
hrmp_list_append(struct list* list, const char* value);

/**
 * Prepend a value to the beginning of the list
 * @param list The list
 * @param value The string value to prepend
 * @return 0 if success, otherwise 1
 */
int
hrmp_list_prepend(struct list* list, const char* value);

/**
 * Get the first entry of the list
 * @param list The list
 * @return The first entry, or NULL if the list is empty
 */
struct list_entry*
hrmp_list_head(struct list* list);

/**
 * Get the next entry in the list
 * @param entry The current entry
 * @return The next entry, or NULL if there are no more entries
 */
struct list_entry*
hrmp_list_next(struct list_entry* entry);

/**
 * Get the previous entry in the list.
 * If the given entry is the first in the list, the head is returned.
 * @param entry The current entry
 * @return The previous entry, or NULL if entry is NULL or not found
 */
struct list_entry*
hrmp_list_prev(struct list_entry* entry);

#ifdef __cplusplus
}
#endif

#endif
