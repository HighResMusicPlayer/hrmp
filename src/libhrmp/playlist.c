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

#include <playlist.h>

#include <utils.h>

#include <ctype.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
pathcmp(const void* a, const void* b)
{
   const char* const* pa = a;
   const char* const* pb = b;

   if (pa == NULL || *pa == NULL)
   {
      return -1;
   }
   if (pb == NULL || *pb == NULL)
   {
      return 1;
   }

   return strcmp(*pa, *pb);
}

static void
append_sorted_files(char* dir, bool recursive, struct list* files)
{
   struct list* tmp = NULL;

   if (dir == NULL || files == NULL)
   {
      return;
   }

   if (hrmp_list_create(&tmp))
   {
      return;
   }

   if (hrmp_get_files(dir, recursive, tmp))
   {
      hrmp_list_destroy(tmp);
      return;
   }

   if (tmp->size == 0)
   {
      hrmp_list_destroy(tmp);
      return;
   }

   char** arr = calloc(tmp->size, sizeof(char*));
   if (arr == NULL)
   {
      hrmp_list_destroy(tmp);
      return;
   }

   size_t n = 0;
   for (struct list_entry* e = hrmp_list_head(tmp); e != NULL; e = hrmp_list_next(e))
   {
      arr[n++] = hrmp_copy_string(e->value);
   }

   qsort(arr, n, sizeof(char*), pathcmp);

   for (size_t i = 0; i < n; i++)
   {
      if (arr[i] != NULL)
      {
         hrmp_list_append(files, arr[i]);
         free(arr[i]);
      }
   }

   free(arr);
   hrmp_list_destroy(tmp);
}

static const char*
basename_ptr(const char* path)
{
   const char* s;

   if (path == NULL)
   {
      return NULL;
   }

   s = strrchr(path, '/');
   return s != NULL ? s + 1 : path;
}

static bool
match_rel_anywhere(const char* pattern, const char* rel)
{
   if (pattern == NULL || rel == NULL)
   {
      return false;
   }

   /* recursive-glob semantics: pattern may start at any path component boundary */
   if (fnmatch(pattern, rel, FNM_PATHNAME) == 0)
   {
      return true;
   }

   const char* p = rel;
   while ((p = strchr(p, '/')) != NULL)
   {
      p++;
      if (fnmatch(pattern, p, FNM_PATHNAME) == 0)
      {
         return true;
      }
   }

   return false;
}

static void
append_recursive_glob(char* dir, const char* pattern, struct list* files)
{
   struct list* tmp = NULL;

   if (dir == NULL || pattern == NULL || files == NULL)
   {
      return;
   }

   if (hrmp_list_create(&tmp))
   {
      return;
   }

   if (hrmp_get_files(dir, true, tmp))
   {
      hrmp_list_destroy(tmp);
      return;
   }

   if (tmp->size == 0)
   {
      hrmp_list_destroy(tmp);
      return;
   }

   bool match_rel = strchr(pattern, '/') != NULL;
   size_t dir_len = strlen(dir);
   size_t cap = tmp->size;

   char** arr = calloc(cap, sizeof(char*));
   if (arr == NULL)
   {
      hrmp_list_destroy(tmp);
      return;
   }

   size_t n = 0;
   for (struct list_entry* e = hrmp_list_head(tmp); e != NULL; e = hrmp_list_next(e))
   {
      const char* path = e->value;
      const char* rel = path;

      if (dir_len > 0 && strncmp(path, dir, dir_len) == 0)
      {
         rel = path + dir_len;
         if (rel[0] == '/')
         {
            rel++;
         }
      }

      int ok;
      if (match_rel)
      {
         ok = match_rel_anywhere(pattern, rel);
      }
      else
      {
         ok = (fnmatch(pattern, basename_ptr(path), 0) == 0);
      }

      if (ok)
      {
         arr[n++] = hrmp_copy_string(e->value);
      }
   }

   qsort(arr, n, sizeof(char*), pathcmp);

   for (size_t i = 0; i < n; i++)
   {
      if (arr[i] != NULL)
      {
         hrmp_list_append(files, arr[i]);
         free(arr[i]);
      }
   }

   free(arr);
   hrmp_list_destroy(tmp);
}

static char*
trim_inplace(char* s)
{
   char* end;

   if (s == NULL)
   {
      return NULL;
   }

   while (*s != '\0' && isspace((unsigned char)*s))
   {
      s++;
   }

   if (*s == '\0')
   {
      return s;
   }

   end = s + strlen(s) - 1;
   while (end > s && isspace((unsigned char)*end))
   {
      *end = '\0';
      end--;
   }

   return s;
}

static void
get_playlist_dir(const char* playlist_path, char* out, size_t out_size)
{
   const char* slash;

   if (out == NULL || out_size == 0)
   {
      return;
   }

   memset(out, 0, out_size);

   if (playlist_path == NULL)
   {
      strncpy(out, ".", out_size - 1);
      return;
   }

   slash = strrchr(playlist_path, '/');
   if (slash == NULL)
   {
      strncpy(out, ".", out_size - 1);
      return;
   }

   size_t len = (size_t)(slash - playlist_path);
   if (len == 0)
   {
      strncpy(out, "/", out_size - 1);
      return;
   }

   if (len >= out_size)
   {
      len = out_size - 1;
   }

   memcpy(out, playlist_path, len);
   out[len] = '\0';
}

static void
join_path(const char* dir, const char* rel, char* out, size_t out_size)
{
   if (out == NULL || out_size == 0)
   {
      return;
   }

   memset(out, 0, out_size);

   if (dir == NULL || dir[0] == '\0' || strcmp(dir, ".") == 0)
   {
      strncpy(out, rel != NULL ? rel : "", out_size - 1);
      return;
   }

   snprintf(out, out_size, "%s/%s", dir, rel != NULL ? rel : "");
}

int
hrmp_playlist_load(const char* playlist_path, struct list* files, bool quiet)
{
   FILE* f = NULL;
   char line_buf[MAX_PATH];
   char base_dir[MAX_PATH];

   if (playlist_path == NULL || files == NULL)
   {
      return 1;
   }

   f = fopen(playlist_path, "r");
   if (f == NULL)
   {
      if (!quiet)
      {
         printf("Playlist not found '%s'\n", playlist_path);
      }
      return 1;
   }

   get_playlist_dir(playlist_path, base_dir, sizeof(base_dir));

   while (fgets(line_buf, sizeof(line_buf), f) != NULL)
   {
      /* Strip CR/LF and surrounding whitespace */
      line_buf[strcspn(line_buf, "\r\n")] = '\0';
      char* line = trim_inplace(line_buf);

      if (line == NULL || line[0] == '\0')
      {
         continue;
      }

      if (line[0] == '#')
      {
         continue;
      }

      if (strcmp(line, "*") == 0)
      {
         append_sorted_files(base_dir, false, files);
         continue;
      }

      if (strcmp(line, "**/*") == 0)
      {
         append_sorted_files(base_dir, true, files);
         continue;
      }

      /* Support "<dir>" + "/" + "**" + "/" + "*" as a convenience for recursive expansion */
      const char* rec_suffix = "/**/*";
      size_t rec_len = strlen(rec_suffix);
      size_t line_len = strlen(line);
      if (line_len > rec_len && strcmp(line + (line_len - rec_len), rec_suffix) == 0)
      {
         char dir_part[MAX_PATH];
         memset(dir_part, 0, sizeof(dir_part));

         /* Copy without the suffix */
         size_t dir_len = line_len - rec_len;
         if (dir_len >= sizeof(dir_part))
         {
            dir_len = sizeof(dir_part) - 1;
         }
         memcpy(dir_part, line, dir_len);
         dir_part[dir_len] = '\0';

         char resolved[MAX_PATH];
         memset(resolved, 0, sizeof(resolved));

         if (dir_part[0] != '/')
         {
            join_path(base_dir, dir_part, resolved, sizeof(resolved));
            if (!hrmp_exists(resolved))
            {
               strncpy(resolved, dir_part, sizeof(resolved) - 1);
            }
         }
         else
         {
            strncpy(resolved, dir_part, sizeof(resolved) - 1);
         }

         if (hrmp_is_directory(resolved))
         {
            append_sorted_files(resolved, true, files);
         }
         else if (!quiet)
         {
            printf("Directory not found '%s'\n", resolved);
         }
         continue;
      }

      /* Support recursive glob: "**" + "/" + PATTERN */
      const char* rec_mid = "**/";
      char* mid = strstr(line, rec_mid);
      if (mid != NULL)
      {
         char prefix_part[MAX_PATH];
         memset(prefix_part, 0, sizeof(prefix_part));

         size_t prefix_len = (size_t)(mid - line);
         if (prefix_len >= sizeof(prefix_part))
         {
            prefix_len = sizeof(prefix_part) - 1;
         }
         memcpy(prefix_part, line, prefix_len);
         prefix_part[prefix_len] = '\0';

         const char* pat = mid + strlen(rec_mid);

         /* Trim trailing '/' from prefix */
         while (prefix_len > 0 && prefix_part[prefix_len - 1] == '/')
         {
            prefix_part[prefix_len - 1] = '\0';
            prefix_len--;
         }

         char resolved[MAX_PATH];
         memset(resolved, 0, sizeof(resolved));

         if (prefix_part[0] == '\0')
         {
            strncpy(resolved, base_dir, sizeof(resolved) - 1);
         }
         else if (prefix_part[0] != '/')
         {
            join_path(base_dir, prefix_part, resolved, sizeof(resolved));
            if (!hrmp_exists(resolved))
            {
               strncpy(resolved, prefix_part, sizeof(resolved) - 1);
            }
         }
         else
         {
            strncpy(resolved, prefix_part, sizeof(resolved) - 1);
         }

         if (hrmp_is_directory(resolved))
         {
            append_recursive_glob(resolved, pat, files);
         }
         else if (!quiet)
         {
            printf("Directory not found '%s'\n", resolved);
         }
         continue;
      }

      char path[MAX_PATH];
      memset(path, 0, sizeof(path));

      if (line[0] != '/')
      {
         join_path(base_dir, line, path, sizeof(path));
         if (!hrmp_exists(path))
         {
            strncpy(path, line, sizeof(path) - 1);
         }
      }
      else
      {
         strncpy(path, line, sizeof(path) - 1);
      }

      if (hrmp_is_directory(path))
      {
         hrmp_get_files(path, false, files);
      }
      else
      {
         if (hrmp_exists(path))
         {
            hrmp_list_append(files, path);
         }
         else if (!quiet)
         {
            printf("File not found '%s'\n", path);
         }
      }
   }

   fclose(f);

   return 0;
}
