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

#include <interactive.h>

#include <files.h>
#include <playlist.h>
#include <utils.h>

#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

struct tui_entry
{
   char name[NAME_MAX + 1];
   bool is_dir;
};

typedef enum {
   TUI_PANEL_DISK,
   TUI_PANEL_PLAYLIST
} tui_panel;

static const char* tui_basename(const char* path);
static int tui_entry_cmp(const void* a, const void* b);
static void tui_parent_dir(char* dir);
static void tui_get_term_size(int* out_rows, int* out_cols);
static int tui_load_dir(const char* dir, struct tui_entry** entries, size_t* n_entries);
static void tui_playlist_clear(struct list* files);
static void tui_playlist_pop(struct list* files);
static void tui_playlist_remove_at(struct list* files, int idx);

int
hrmp_interactive_ui(struct list* files, const char* start_path, int* play_from_index)
{
   char cur[PATH_MAX];
   char resolved[PATH_MAX];

   if (files == NULL)
   {
      return 1;
   }

   if (play_from_index != NULL)
   {
      *play_from_index = 0;
   }

   memset(cur, 0, sizeof(cur));

   if (start_path != NULL && hrmp_is_directory((char*)start_path) && realpath(start_path, resolved) != NULL)
   {
      hrmp_snprintf(cur, sizeof(cur), "%s", resolved);
   }
   else
   {
      if (getcwd(cur, sizeof(cur)) == NULL)
      {
         hrmp_snprintf(cur, sizeof(cur), "/");
      }
      if (realpath(cur, resolved) != NULL)
      {
         hrmp_snprintf(cur, sizeof(cur), "%s", resolved);
      }
   }

   use_env(FALSE);

   initscr();
   raw();
   noecho();
   keypad(stdscr, TRUE);
   curs_set(0);
   clearok(stdscr, TRUE);
   timeout(-1);

   int init_rows = 0;
   int init_cols = 0;
   tui_get_term_size(&init_rows, &init_cols);
   if (init_rows > 0 && init_cols > 0)
   {
      resizeterm(init_rows, init_cols);
   }

   refresh();

   int ret = 0;
   tui_panel active = TUI_PANEL_DISK;

   int sel = 0;
   int scroll = 0;

   int pl_sel = 0;
   int pl_scroll = 0;

   bool first_paint = true;
   struct tui_entry* entries = NULL;
   size_t n_entries = 0;
   WINDOW* left = NULL;
   WINDOW* right = NULL;
   int last_rows = 0;
   int last_cols = 0;
   int last_split = 0;

   for (;;)
   {
      int rows = 0;
      int cols = 0;
      tui_get_term_size(&rows, &cols);
      if (rows > 0 && cols > 0 && (rows != LINES || cols != COLS))
      {
         resizeterm(rows, cols);
      }
      getmaxyx(stdscr, rows, cols);

      if (rows < 6 || cols < 40)
      {
         clear();
         mvprintw(0, 0, "Terminal too small (%dx%d). Need at least 40x6.", cols, rows);
         mvprintw(2, 0, "Press 'p' to play or 'q' to quit.");
         refresh();
         int ch = getch();
         if (ch == 'q' || ch == 'Q' || ch == 27)
         {
            if (play_from_index != NULL)
            {
               *play_from_index = -1;
            }
            break;
         }
         if (ch == 'p' || ch == 'P')
         {
            break;
         }
         continue;
      }

      int split = cols / 2;
      if (split < 20)
      {
         split = 20;
      }
      if (cols - split < 20)
      {
         split = cols - 20;
      }

      if (tui_load_dir(cur, &entries, &n_entries))
      {
         ret = 1;
         break;
      }

      if (sel >= (int)n_entries)
      {
         sel = (int)n_entries - 1;
      }
      if (sel < 0)
      {
         sel = 0;
      }

      int list_height = rows - 3;
      if (list_height < 1)
      {
         list_height = 1;
      }

      if (sel < scroll)
      {
         scroll = sel;
      }
      if (sel >= scroll + list_height)
      {
         scroll = sel - list_height + 1;
      }
      if (scroll < 0)
      {
         scroll = 0;
      }

      if (left == NULL || right == NULL || rows != last_rows || cols != last_cols || split != last_split)
      {
         if (left != NULL)
         {
            delwin(left);
            left = NULL;
         }
         if (right != NULL)
         {
            delwin(right);
            right = NULL;
         }

         left = newwin(rows - 1, split, 0, 0);
         right = newwin(rows - 1, cols - split, 0, split);

         if (left == NULL || right == NULL)
         {
            ret = 1;
            break;
         }

         last_rows = rows;
         last_cols = cols;
         last_split = split;
      }

      erase();
      werase(left);
      werase(right);

      box(left, 0, 0);
      box(right, 0, 0);

      mvwprintw(left, 0, 2, " %s ", cur);
      mvwprintw(right, 0, 2, " Playlist ");

      for (int i = 0; i < list_height; i++)
      {
         int idx = scroll + i;
         if (idx >= (int)n_entries)
         {
            break;
         }

         if (active == TUI_PANEL_DISK && idx == sel)
         {
            wattron(left, A_REVERSE);
         }

         char label[NAME_MAX + 8];
         if (entries[idx].is_dir)
         {
            hrmp_snprintf(label, sizeof(label), "%s/", entries[idx].name);
         }
         else
         {
            hrmp_snprintf(label, sizeof(label), "%s", entries[idx].name);
         }

         mvwprintw(left, 1 + i, 1, "%-*.*s", split - 2, split - 2, label);

         if (active == TUI_PANEL_DISK && idx == sel)
         {
            wattroff(left, A_REVERSE);
         }
      }

      int pr_h, pr_w;
      getmaxyx(right, pr_h, pr_w);

      int pl_height = pr_h - 2;
      if (pl_height < 1)
      {
         pl_height = 1;
      }

      int pl_size = (int)hrmp_list_size(files);
      if (pl_size <= 0)
      {
         pl_sel = 0;
         pl_scroll = 0;
      }
      else
      {
         if (pl_sel >= pl_size)
         {
            pl_sel = pl_size - 1;
         }
         if (pl_sel < 0)
         {
            pl_sel = 0;
         }

         if (pl_sel < pl_scroll)
         {
            pl_scroll = pl_sel;
         }
         if (pl_sel >= pl_scroll + pl_height)
         {
            pl_scroll = pl_sel - pl_height + 1;
         }
         if (pl_scroll < 0)
         {
            pl_scroll = 0;
         }
      }

      int pr_y = 0;
      int idx = 0;
      for (struct list_entry* e = hrmp_list_head(files);
           e != NULL && pr_y < pl_height;
           e = hrmp_list_next(e), idx++)
      {
         if (idx < pl_scroll)
         {
            continue;
         }

         if (active == TUI_PANEL_PLAYLIST && idx == pl_sel)
         {
            wattron(right, A_REVERSE);
         }

         mvwprintw(right, 1 + pr_y, 1, "%-*.*s", pr_w - 2, pr_w - 2, tui_basename((char*)e->value));

         if (active == TUI_PANEL_PLAYLIST && idx == pl_sel)
         {
            wattroff(right, A_REVERSE);
         }

         pr_y++;
      }

      if (active == TUI_PANEL_DISK)
      {
         mvprintw(rows - 1, 0, "Up/Down=Move  Left/Right=Switch  Enter=Up/Add  +=Add  *=AddAll  -=Remove  Backspace=Up  l=Load  s=Save  p=Play  q=Quit");
      }
      else
      {
         mvprintw(rows - 1, 0, "Up/Down=Move  Left/Right=Switch  Enter=Play  -=Remove  l=Load  s=Save  p=Play  q=Quit");
      }
      clrtoeol();

      wnoutrefresh(stdscr);
      wnoutrefresh(left);
      wnoutrefresh(right);
      doupdate();
      fflush(stdout);

      if (first_paint)
      {
         /* Force an initial paint on terminals that only update after an input read. */
         timeout(0);
         (void)getch();
         timeout(-1);
         first_paint = false;
      }

      int ch = getch();

      if (ch == 'q' || ch == 'Q' || ch == 27)
      {
         if (play_from_index != NULL)
         {
            *play_from_index = -1;
         }

         free(entries);
         entries = NULL;
         n_entries = 0;
         break;
      }
      else if (ch == 'p' || ch == 'P')
      {
         if (active == TUI_PANEL_PLAYLIST && play_from_index != NULL)
         {
            *play_from_index = pl_sel;
         }

         free(entries);
         entries = NULL;
         n_entries = 0;
         break;
      }
      else if (ch == KEY_LEFT)
      {
         active = TUI_PANEL_DISK;
      }
      else if (ch == KEY_RIGHT)
      {
         if (!hrmp_list_empty(files))
         {
            active = TUI_PANEL_PLAYLIST;
         }
      }
      else if (ch == '\t')
      {
         if (active == TUI_PANEL_DISK)
         {
            if (!hrmp_list_empty(files))
            {
               active = TUI_PANEL_PLAYLIST;
            }
         }
         else
         {
            active = TUI_PANEL_DISK;
         }
      }
      else if (ch == KEY_UP)
      {
         if (active == TUI_PANEL_DISK)
         {
            sel--;
         }
         else
         {
            pl_sel--;
         }
      }
      else if (ch == KEY_DOWN)
      {
         if (active == TUI_PANEL_DISK)
         {
            sel++;
         }
         else
         {
            pl_sel++;
         }
      }
      else if (ch == KEY_PPAGE)
      {
         if (active == TUI_PANEL_DISK)
         {
            sel -= list_height;
         }
         else
         {
            pl_sel -= list_height;
         }
      }
      else if (ch == KEY_NPAGE)
      {
         if (active == TUI_PANEL_DISK)
         {
            sel += list_height;
         }
         else
         {
            pl_sel += list_height;
         }
      }
      else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8 || ch == '\\')
      {
         if (active == TUI_PANEL_DISK)
         {
            tui_parent_dir(cur);
            sel = 0;
            scroll = 0;
         }
      }
      else if (ch == 'l' || ch == 'L')
      {
         const char* playlist_path = "playlist.hrmp";

         struct stat st;
         if (stat(playlist_path, &st) == 0 && S_ISREG(st.st_mode))
         {
            tui_playlist_clear(files);
            (void)hrmp_playlist_load(playlist_path, files, true);
            pl_sel = 0;
            pl_scroll = 0;
            if (hrmp_list_empty(files))
            {
               active = TUI_PANEL_DISK;
            }
         }
      }
      else if (ch == 's' || ch == 'S')
      {
         if (!hrmp_list_empty(files))
         {
            const char* playlist_path = "playlist.hrmp";

            FILE* f = fopen(playlist_path, "w");
            if (f != NULL)
            {
               for (struct list_entry* e = hrmp_list_head(files); e != NULL; e = hrmp_list_next(e))
               {
                  fprintf(f, "%s\n", (char*)e->value);
               }
               fclose(f);
            }
         }
      }
      else if (ch == '-' || ch == '_')
      {
         if (active == TUI_PANEL_PLAYLIST)
         {
            tui_playlist_remove_at(files, pl_sel);
            int pl_size = (int)hrmp_list_size(files);
            if (pl_size <= 0)
            {
               pl_sel = 0;
               pl_scroll = 0;
               active = TUI_PANEL_DISK;
            }
            else if (pl_sel >= pl_size)
            {
               pl_sel = pl_size - 1;
            }
         }
         else
         {
            tui_playlist_pop(files);
         }
      }
      else if (ch == '*' )
      {
         if (active == TUI_PANEL_DISK)
         {
            for (size_t i = 0; i < n_entries; i++)
            {
               if (entries[i].is_dir)
               {
                  continue;
               }

               char full[PATH_MAX];
               if (strcmp(cur, "/") == 0)
               {
                  hrmp_snprintf(full, sizeof(full), "/%s", entries[i].name);
               }
               else
               {
                  hrmp_snprintf(full, sizeof(full), "%s/%s", cur, entries[i].name);
               }

               if (realpath(full, resolved) != NULL)
               {
                  hrmp_snprintf(full, sizeof(full), "%s", resolved);
               }

               hrmp_list_append(files, full);
            }
         }
      }
      else if (ch == '+' || ch == '=')
      {
         if (active == TUI_PANEL_DISK && sel >= 0 && sel < (int)n_entries && !entries[sel].is_dir)
         {
            char full[PATH_MAX];
            if (strcmp(cur, "/") == 0)
            {
               hrmp_snprintf(full, sizeof(full), "/%s", entries[sel].name);
            }
            else
            {
               hrmp_snprintf(full, sizeof(full), "%s/%s", cur, entries[sel].name);
            }

            if (realpath(full, resolved) != NULL)
            {
               hrmp_snprintf(full, sizeof(full), "%s", resolved);
            }

            hrmp_list_append(files, full);
         }
      }
      else if (ch == '\n' || ch == KEY_ENTER)
      {
         if (active == TUI_PANEL_PLAYLIST)
         {
            if (play_from_index != NULL)
            {
               *play_from_index = pl_sel;
            }

            free(entries);
            entries = NULL;
            n_entries = 0;
            break;
         }

         if (active == TUI_PANEL_DISK && sel >= 0 && sel < (int)n_entries)
         {
            if (entries[sel].is_dir)
            {
               if (strcmp(entries[sel].name, "..") == 0)
               {
                  tui_parent_dir(cur);
               }
               else
               {
                  char tmp[PATH_MAX];
                  if (strcmp(cur, "/") == 0)
                  {
                     hrmp_snprintf(tmp, sizeof(tmp), "/%s", entries[sel].name);
                  }
                  else
                  {
                     hrmp_snprintf(tmp, sizeof(tmp), "%s/%s", cur, entries[sel].name);
                  }

                  if (realpath(tmp, resolved) != NULL)
                  {
                     hrmp_snprintf(cur, sizeof(cur), "%s", resolved);
                  }
                  else
                  {
                     hrmp_snprintf(cur, sizeof(cur), "%s", tmp);
                  }
               }

               sel = 0;
               scroll = 0;
            }
            else
            {
               char full[PATH_MAX];
               if (strcmp(cur, "/") == 0)
               {
                  hrmp_snprintf(full, sizeof(full), "/%s", entries[sel].name);
               }
               else
               {
                  hrmp_snprintf(full, sizeof(full), "%s/%s", cur, entries[sel].name);
               }

               if (realpath(full, resolved) != NULL)
               {
                  hrmp_snprintf(full, sizeof(full), "%s", resolved);
               }

               hrmp_list_append(files, full);
            }
         }
      }

      free(entries);
      entries = NULL;
      n_entries = 0;
   }

   free(entries);

   if (left != NULL)
   {
      delwin(left);
   }
   if (right != NULL)
   {
      delwin(right);
   }

   /* Restore terminal state explicitly (some terminals need this in addition to endwin()). */
   noraw();
   echo();
   keypad(stdscr, FALSE);
   curs_set(1);
   timeout(-1);
   clear();
   refresh();

   endwin();
   return ret;
}









static const char*
tui_basename(const char* path)
{
   const char* s;

   if (path == NULL)
   {
      return "";
   }

   s = strrchr(path, '/');
   return s != NULL ? s + 1 : path;
}

static int
tui_entry_cmp(const void* a, const void* b)
{
   const struct tui_entry* ea = a;
   const struct tui_entry* eb = b;

   if (ea == NULL && eb == NULL)
   {
      return 0;
   }
   if (ea == NULL)
   {
      return 1;
   }
   if (eb == NULL)
   {
      return -1;
   }

   if (ea->is_dir != eb->is_dir)
   {
      return ea->is_dir ? -1 : 1;
   }

   return strcmp(ea->name, eb->name);
}

static void
tui_parent_dir(char* dir)
{
   size_t len;

   if (dir == NULL)
   {
      return;
   }

   len = strlen(dir);
   while (len > 1 && dir[len - 1] == '/')
   {
      dir[len - 1] = '\0';
      len--;
   }

   if (strcmp(dir, "/") == 0)
   {
      return;
   }

   char* slash = strrchr(dir, '/');
   if (slash == NULL)
   {
      hrmp_snprintf(dir, PATH_MAX, "/");
      return;
   }

   if (slash == dir)
   {
      dir[1] = '\0';
   }
   else
   {
      *slash = '\0';
   }
}

static void
tui_get_term_size(int* out_rows, int* out_cols)
{
   if (out_rows != NULL)
   {
      *out_rows = LINES;
   }
   if (out_cols != NULL)
   {
      *out_cols = COLS;
   }

   struct winsize ws;
   memset(&ws, 0, sizeof(ws));

   int ok = (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0);
   if (!ok)
   {
      memset(&ws, 0, sizeof(ws));
      ok = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);
   }

   if (!ok)
   {
      return;
   }

   if (ws.ws_row > 0 && out_rows != NULL)
   {
      *out_rows = (int)ws.ws_row;
   }
   if (ws.ws_col > 0 && out_cols != NULL)
   {
      *out_cols = (int)ws.ws_col;
   }
}

static int
tui_load_dir(const char* dir, struct tui_entry** entries, size_t* n_entries)
{
   DIR* d = NULL;
   struct dirent* de = NULL;
   struct tui_entry* arr = NULL;
   size_t n = 0;
   size_t cap = 0;

   if (dir == NULL || entries == NULL || n_entries == NULL)
   {
      return 1;
   }

   *entries = NULL;
   *n_entries = 0;

   d = opendir(dir);
   if (d == NULL)
   {
      return 1;
   }

   cap = 64;
   arr = calloc(cap, sizeof(struct tui_entry));
   if (arr == NULL)
   {
      closedir(d);
      return 1;
   }

   hrmp_snprintf(arr[n].name, sizeof(arr[n].name), "..");
   arr[n].is_dir = true;
   n++;

   while ((de = readdir(d)) != NULL)
   {
      if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
      {
         continue;
      }

      char full[PATH_MAX];
      struct stat st;

      if (strcmp(dir, "/") == 0)
      {
         hrmp_snprintf(full, sizeof(full), "/%s", de->d_name);
      }
      else
      {
         hrmp_snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
      }

      if (lstat(full, &st) != 0)
      {
         continue;
      }

      bool is_dir = S_ISDIR(st.st_mode);
      bool is_file = S_ISREG(st.st_mode);

      if (!is_dir)
      {
         if (!is_file)
         {
            continue;
         }

         if (!hrmp_file_is_supported(de->d_name))
         {
            continue;
         }
      }

      if (n == cap)
      {
         cap *= 2;
         struct tui_entry* tmp = realloc(arr, cap * sizeof(struct tui_entry));
         if (tmp == NULL)
         {
            free(arr);
            closedir(d);
            return 1;
         }
         arr = tmp;
      }

      memset(&arr[n], 0, sizeof(struct tui_entry));
      hrmp_snprintf(arr[n].name, sizeof(arr[n].name), "%s", de->d_name);
      arr[n].is_dir = is_dir;
      n++;
   }

   closedir(d);

   if (n > 1)
   {
      qsort(arr + 1, n - 1, sizeof(struct tui_entry), tui_entry_cmp);
   }

   *entries = arr;
   *n_entries = n;

   return 0;
}

static void
tui_playlist_clear(struct list* files)
{
   if (files == NULL)
   {
      return;
   }

   struct list_entry* e = files->head;
   while (e != NULL)
   {
      struct list_entry* next = e->next;
      free(e->value);
      free(e);
      e = next;
   }

   files->head = NULL;
   files->tail = NULL;
   files->size = 0;
}

static void
tui_playlist_pop(struct list* files)
{
   if (files == NULL || files->head == NULL)
   {
      return;
   }

   if (files->head == files->tail)
   {
      free(files->head->value);
      free(files->head);
      files->head = NULL;
      files->tail = NULL;
      files->size = 0;
      return;
   }

   struct list_entry* prev = files->head;
   while (prev->next != NULL && prev->next != files->tail)
   {
      prev = prev->next;
   }

   if (files->tail != NULL)
   {
      free(files->tail->value);
      free(files->tail);
   }

   prev->next = NULL;
   files->tail = prev;
   if (files->size > 0)
   {
      files->size--;
   }
}

static void
tui_playlist_remove_at(struct list* files, int idx)
{
   if (files == NULL || files->head == NULL || idx < 0)
   {
      return;
   }

   if (idx == 0)
   {
      struct list_entry* old = files->head;
      files->head = old->next;
      if (files->tail == old)
      {
         files->tail = files->head;
      }
      free(old->value);
      free(old);
      if (files->size > 0)
      {
         files->size--;
      }
      return;
   }

   struct list_entry* prev = files->head;
   for (int i = 0; i < idx - 1 && prev != NULL; i++)
   {
      prev = prev->next;
   }

   if (prev == NULL || prev->next == NULL)
   {
      return;
   }

   struct list_entry* cur = prev->next;
   prev->next = cur->next;
   if (files->tail == cur)
   {
      files->tail = prev;
   }
   free(cur->value);
   free(cur);
   if (files->size > 0)
   {
      files->size--;
   }
}

