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

#include <ctype.h>
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

struct tui_search_entry
{
   char path[PATH_MAX];
   char display[PATH_MAX];
};

struct tui_search_state
{
   struct tui_search_entry* all;
   size_t n_all;
   struct tui_search_entry** matches;
   size_t n_matches;
};

typedef enum {
   TUI_PANEL_DISK,
   TUI_PANEL_PLAYLIST
} tui_panel;

static const char* tui_basename(const char* path);
static int tui_entry_cmp(const void* a, const void* b);
static int tui_search_entry_cmp(const void* a, const void* b);
static void tui_parent_dir(char* dir);
static void tui_get_term_size(int* out_rows, int* out_cols);
static int tui_load_dir(const char* dir, struct tui_entry** entries, size_t* n_entries);
static int tui_search_collect(const char* root, struct tui_search_entry** entries, size_t* n_entries);
static int tui_search_collect_dir(const char* root, const char* dir, struct tui_search_entry** entries, size_t* n_entries, size_t* cap);
static void tui_search_clear(struct tui_search_state* state);
static int tui_search_update_matches(struct tui_search_state* state, const char* query);
static bool tui_match_query(const char* text, const char* query);
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
   bool search_mode = false;
   char search_query[PATH_MAX];
   size_t search_len = 0;
   struct tui_search_state search;
   int search_sel = 0;
   int search_scroll = 0;
   WINDOW* left = NULL;
   WINDOW* right = NULL;
   int last_rows = 0;
   int last_cols = 0;
   int last_split = 0;

   memset(search_query, 0, sizeof(search_query));
   memset(&search, 0, sizeof(search));

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

      if (!search_mode)
      {
         if (tui_load_dir(cur, &entries, &n_entries))
         {
            ret = 1;
            break;
         }
      }
      else
      {
         n_entries = 0;
      }

      int list_start = 1;
      int list_height = rows - 3;
      if (search_mode)
      {
         list_start = 2;
         list_height = rows - 4;
      }
      if (list_height < 1)
      {
         list_height = 1;
      }

      int list_count = search_mode ? (int)search.n_matches : (int)n_entries;
      int* list_sel = search_mode ? &search_sel : &sel;
      int* list_scroll = search_mode ? &search_scroll : &scroll;

      if (list_count <= 0)
      {
         *list_sel = 0;
         *list_scroll = 0;
      }
      else
      {
         if (*list_sel >= list_count)
         {
            *list_sel = list_count - 1;
         }
         if (*list_sel < 0)
         {
            *list_sel = 0;
         }

         if (*list_sel < *list_scroll)
         {
            *list_scroll = *list_sel;
         }
         if (*list_sel >= *list_scroll + list_height)
         {
            *list_scroll = *list_sel - list_height + 1;
         }
         if (*list_scroll < 0)
         {
            *list_scroll = 0;
         }
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

      if (search_mode)
      {
         char search_line[PATH_MAX];
         hrmp_snprintf(search_line, sizeof(search_line), "/ %s", search_query);
         mvwprintw(left, 1, 1, "%-*.*s", split - 2, split - 2, search_line);
      }

      for (int i = 0; i < list_height; i++)
      {
         int idx = *list_scroll + i;
         if (idx >= list_count)
         {
            break;
         }

         if (active == TUI_PANEL_DISK && idx == *list_sel)
         {
            wattron(left, A_REVERSE);
         }

         const char* label = NULL;
         char tmp[NAME_MAX + 8];
         if (search_mode)
         {
            label = search.matches[idx]->display;
         }
         else
         {
            if (entries[idx].is_dir)
            {
               hrmp_snprintf(tmp, sizeof(tmp), "%s/", entries[idx].name);
            }
            else
            {
               hrmp_snprintf(tmp, sizeof(tmp), "%s", entries[idx].name);
            }
            label = tmp;
         }

         mvwprintw(left, list_start + i, 1, "%-*.*s", split - 2, split - 2, label);

         if (active == TUI_PANEL_DISK && idx == *list_sel)
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

      if (search_mode)
      {
         mvprintw(rows - 1, 0, "Search: type to filter  Enter=Jump  Esc=Cancel  Up/Down=Move");
      }
      else if (active == TUI_PANEL_DISK)
      {
         mvprintw(rows - 1, 0, "Up/Down=Move  Left/Right=Switch  Enter=Up/Add  +=Add  *=AddAll  -=Remove  /=Search  Backspace=Up  l=Load  s=Save  p=Play  q=Quit");
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

      if (search_mode)
      {
         if (ch == 27)
         {
            search_mode = false;
            tui_search_clear(&search);
            search_query[0] = '\0';
            search_len = 0;
            search_sel = 0;
            search_scroll = 0;
         }
         else if (ch == KEY_UP)
         {
            search_sel--;
         }
         else if (ch == KEY_DOWN)
         {
            search_sel++;
         }
         else if (ch == KEY_PPAGE)
         {
            search_sel -= list_height;
         }
         else if (ch == KEY_NPAGE)
         {
            search_sel += list_height;
         }
         else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
         {
            if (search_len > 0)
            {
               search_len--;
               search_query[search_len] = '\0';
               if (tui_search_update_matches(&search, search_query) != 0)
               {
                  beep();
               }
               search_sel = 0;
               search_scroll = 0;
            }
         }
         else if (ch == '\n' || ch == KEY_ENTER)
         {
            if (search_sel >= 0 && search_sel < (int)search.n_matches)
            {
               const struct tui_search_entry* entry = search.matches[search_sel];
               char target[PATH_MAX];
               hrmp_snprintf(target, sizeof(target), "%s", entry->path);
               tui_parent_dir(target);

               if (realpath(target, resolved) != NULL)
               {
                  hrmp_snprintf(cur, sizeof(cur), "%s", resolved);
               }
               else
               {
                  hrmp_snprintf(cur, sizeof(cur), "%s", target);
               }

               search_mode = false;
               tui_search_clear(&search);
               search_query[0] = '\0';
               search_len = 0;
               search_sel = 0;
               search_scroll = 0;
               sel = 0;
               scroll = 0;
            }
            else
            {
               beep();
            }
         }
         else if (ch >= 0 && ch < 256 && isprint((unsigned char)ch))
         {
            if (search_len + 1 < sizeof(search_query))
            {
               search_query[search_len++] = (char)ch;
               search_query[search_len] = '\0';
               if (tui_search_update_matches(&search, search_query) != 0)
               {
                  beep();
               }
               search_sel = 0;
               search_scroll = 0;
            }
            else
            {
               beep();
            }
         }
      }
      else if (ch == 'q' || ch == 'Q' || ch == 27)
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
      else if (ch == '/')
      {
         if (active == TUI_PANEL_DISK)
         {
            tui_search_clear(&search);
            if (tui_search_collect(cur, &search.all, &search.n_all) == 0)
            {
               search_query[0] = '\0';
               search_len = 0;
               if (tui_search_update_matches(&search, search_query) == 0)
               {
                  search_mode = true;
                  search_sel = 0;
                  search_scroll = 0;
               }
               else
               {
                  tui_search_clear(&search);
                  beep();
               }
            }
            else
            {
               tui_search_clear(&search);
               beep();
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
   tui_search_clear(&search);

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

static int
tui_search_entry_cmp(const void* a, const void* b)
{
   const struct tui_search_entry* ea = a;
   const struct tui_search_entry* eb = b;

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

   return strcmp(ea->display, eb->display);
}

static bool
tui_match_query(const char* text, const char* query)
{
   if (query == NULL || query[0] == '\0')
   {
      return true;
   }
   if (text == NULL)
   {
      return false;
   }

   size_t text_len = strlen(text);
   size_t query_len = strlen(query);
   if (query_len > text_len)
   {
      return false;
   }

   for (size_t i = 0; i + query_len <= text_len; i++)
   {
      size_t j = 0;
      for (; j < query_len; j++)
      {
         char left = (char)tolower((unsigned char)text[i + j]);
         char right = (char)tolower((unsigned char)query[j]);
         if (left != right)
         {
            break;
         }
      }
      if (j == query_len)
      {
         return true;
      }
   }

   return false;
}

static void
tui_search_clear(struct tui_search_state* state)
{
   if (state == NULL)
   {
      return;
   }

   free(state->all);
   free(state->matches);
   state->all = NULL;
   state->matches = NULL;
   state->n_all = 0;
   state->n_matches = 0;
}

static int
tui_search_update_matches(struct tui_search_state* state, const char* query)
{
   if (state == NULL)
   {
      return 1;
   }

   free(state->matches);
   state->matches = NULL;
   state->n_matches = 0;

   if (state->all == NULL || state->n_all == 0)
   {
      return 0;
   }

   state->matches = calloc(state->n_all, sizeof(*state->matches));
   if (state->matches == NULL)
   {
      return 1;
   }

   for (size_t i = 0; i < state->n_all; i++)
   {
      if (tui_match_query(state->all[i].display, query))
      {
         state->matches[state->n_matches++] = &state->all[i];
      }
   }

   return 0;
}

static int
tui_search_collect(const char* root, struct tui_search_entry** entries, size_t* n_entries)
{
   struct tui_search_entry* arr = NULL;
   size_t n = 0;
   size_t cap = 128;

   if (root == NULL || entries == NULL || n_entries == NULL)
   {
      return 1;
   }

   *entries = NULL;
   *n_entries = 0;

   arr = calloc(cap, sizeof(struct tui_search_entry));
   if (arr == NULL)
   {
      return 1;
   }

   if (tui_search_collect_dir(root, root, &arr, &n, &cap))
   {
      free(arr);
      return 1;
   }

   if (n > 1)
   {
      qsort(arr, n, sizeof(struct tui_search_entry), tui_search_entry_cmp);
   }

   *entries = arr;
   *n_entries = n;
   return 0;
}

static int
tui_search_collect_dir(const char* root, const char* dir, struct tui_search_entry** entries, size_t* n_entries, size_t* cap)
{
   DIR* d = NULL;
   struct dirent* de = NULL;

   if (root == NULL || dir == NULL || entries == NULL || n_entries == NULL || cap == NULL)
   {
      return 1;
   }

   d = opendir(dir);
   if (d == NULL)
   {
      return 1;
   }

   while ((de = readdir(d)) != NULL)
   {
      if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
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

      if (is_dir)
      {
         if (tui_search_collect_dir(root, full, entries, n_entries, cap))
         {
            closedir(d);
            return 1;
         }
         continue;
      }

      if (!is_file || !hrmp_file_is_supported(de->d_name))
      {
         continue;
      }

      if (*n_entries == *cap)
      {
         size_t next = (*cap) * 2;
         struct tui_search_entry* tmp = realloc(*entries, next * sizeof(struct tui_search_entry));
         if (tmp == NULL)
         {
            closedir(d);
            return 1;
         }
         *entries = tmp;
         *cap = next;
      }

      struct tui_search_entry* entry = &(*entries)[*n_entries];
      memset(entry, 0, sizeof(struct tui_search_entry));
      hrmp_snprintf(entry->path, sizeof(entry->path), "%s", full);

      const char* rel = full;
      size_t root_len = strlen(root);
      if (root_len == 1 && root[0] == '/')
      {
         rel = full + 1;
         if (*rel == '\0')
         {
            rel = full;
         }
      }
      else if (root_len > 0 && strncmp(full, root, root_len) == 0 &&
               (full[root_len] == '/' || full[root_len] == '\0'))
      {
         rel = full + root_len;
         if (*rel == '/')
         {
            rel++;
         }
         if (*rel == '\0')
         {
            rel = full;
         }
      }

      hrmp_snprintf(entry->display, sizeof(entry->display), "%s", rel);
      (*n_entries)++;
   }

   closedir(d);
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
