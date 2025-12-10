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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define HRMP_DEFAULT_PATH "/usr/bin/hrmp"

typedef enum
{
   HRMP_PLAY_MODE_END = 0,
   HRMP_PLAY_MODE_REPEAT,
   HRMP_PLAY_MODE_SHUFFLE
} HrmpPlayMode;

typedef enum
{
   HRMP_FILES_MODE_FULL = 0,
   HRMP_FILES_MODE_SHORT
} HrmpFilesMode;

struct App
{
   GtkWidget* window;

   GtkWidget* play_button;
   GtkWidget* prev_button;
   GtkWidget* next_button;
   GtkWidget* stop_button;
   GtkWidget* mode_button;
   GtkWidget* volume_down_button;
   GtkWidget* volume_up_button;
   GtkWidget* skip_back_button;
   GtkWidget* skip_ahead_button;

   GtkWidget* list_view;
   GtkListStore* list_store;

   GtkWidget* output_view;
   GtkTextBuffer* output_buffer;
   GtkWidget* song_label;
   GtkWidget* status_label;
   GtkWidget* debug_window;

   gchar* hrmp_path;
   gchar* default_device;

   GPid child_pid;
   GIOChannel* child_stdin;
   GIOChannel* child_stdout;
   guint stdout_watch_id;
   gboolean hrmp_running;
   gboolean is_playing;
   HrmpPlayMode play_mode;
   HrmpFilesMode files_mode;

   GPtrArray* devices; /* array of HrmpDevice* */
};

static void start_hrmp(struct App* app);
static void stop_hrmp(struct App* app);
static void hrmp_gtk_update_status(struct App* app);
static void hrmp_gtk_update_song_from_output(struct App* app);

static gboolean on_debug_window_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data);
static void hrmp_gtk_show_debug_window(struct App* app);
static void on_menu_debug(GtkWidget* widget, gpointer user_data);
static void on_debug_clear_clicked(GtkWidget* widget, gpointer user_data);
static void on_debug_close_clicked(GtkWidget* widget, gpointer user_data);

static gboolean
on_debug_window_delete(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
   (void)event;
   (void)user_data;
   gtk_widget_hide(widget);
   return TRUE;
}

static void
hrmp_gtk_show_debug_window(struct App* app)
{
   if (app->debug_window == NULL)
   {
      app->debug_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title(GTK_WINDOW(app->debug_window), "hrmp-ui Debug");
      gtk_window_set_default_size(GTK_WINDOW(app->debug_window), 800, 300);

      GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
      gtk_container_add(GTK_CONTAINER(app->debug_window), vbox);

      GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                     GTK_POLICY_AUTOMATIC,
                                     GTK_POLICY_AUTOMATIC);

      if (app->output_view == NULL)
      {
         app->output_view = gtk_text_view_new();
         gtk_text_view_set_editable(GTK_TEXT_VIEW(app->output_view), FALSE);
         gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->output_view), FALSE);
         app->output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->output_view));
      }

      gtk_container_add(GTK_CONTAINER(scrolled), app->output_view);
      gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 4);

      GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_box_set_homogeneous(GTK_BOX(button_box), FALSE);

      GtkWidget* clear_btn = gtk_button_new_with_label("Clear");
      g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_debug_clear_clicked), app);
      gtk_box_pack_start(GTK_BOX(button_box), clear_btn, FALSE, FALSE, 4);

      GtkWidget* close_btn = gtk_button_new_with_label("Close");
      g_signal_connect(close_btn, "clicked", G_CALLBACK(on_debug_close_clicked), app);
      gtk_box_pack_end(GTK_BOX(button_box), close_btn, FALSE, FALSE, 4);

      gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 4);

      g_signal_connect(app->debug_window,
                       "delete-event",
                       G_CALLBACK(on_debug_window_delete),
                       app);
   }

   gtk_widget_show_all(app->debug_window);
   gtk_window_present(GTK_WINDOW(app->debug_window));
}

static void
on_menu_debug(GtkWidget* widget, gpointer user_data)
{
   (void)widget;
   struct App* app = user_data;
   hrmp_gtk_show_debug_window(app);
}

static void
on_debug_clear_clicked(GtkWidget* widget, gpointer user_data)
{
   (void)widget;
   struct App* app = user_data;

   if (app->output_buffer != NULL)
   {
      gtk_text_buffer_set_text(app->output_buffer, "", -1);
   }

   if (app->song_label != NULL)
   {
      gtk_label_set_text(GTK_LABEL(app->song_label), "");
   }
}

static void
on_debug_close_clicked(GtkWidget* widget, gpointer user_data)
{
   (void)widget;
   struct App* app = user_data;

   if (app->debug_window != NULL)
   {
      gtk_widget_hide(app->debug_window);
   }
}

static void hrmp_gtk_ensure_devices(struct App* app);

static gchar*
hrmp_gtk_get_config_path(void)
{
   const gchar* home;
   char* dir;
   char* path;

   home = g_get_home_dir();
   if (home == NULL)
   {
      return NULL;
   }

   dir = g_build_filename(home, ".hrmp", NULL);

   if (mkdir(dir, 0700) != 0 && errno != EEXIST)
   {
      g_free(dir);
      return NULL;
   }

   path = g_build_filename(dir, "hrmp-ui.conf", NULL);
   g_free(dir);

   return path;
}

static void
hrmp_gtk_load_preferences(struct App* app)
{
   GKeyFile* keyfile;
   GError* error = NULL;
   gchar* config_path;
   gchar* value;
   gchar* files_value;

   app->hrmp_path = g_strdup(HRMP_DEFAULT_PATH);
   app->default_device = NULL;
   app->files_mode = HRMP_FILES_MODE_FULL;

   keyfile = g_key_file_new();
   config_path = hrmp_gtk_get_config_path();

   if (config_path != NULL &&
       g_key_file_load_from_file(keyfile, config_path, G_KEY_FILE_NONE, &error))
   {
      value = g_key_file_get_string(keyfile, "General", "hrmp_path", NULL);
      if (value != NULL && value[0] != '\0')
      {
         g_free(app->hrmp_path);
         app->hrmp_path = value;
      }

      value = g_key_file_get_string(keyfile, "General", "default_device", NULL);
      if (value != NULL && value[0] != '\0')
      {
         app->default_device = value;
      }

      files_value = g_key_file_get_string(keyfile, "General", "files", NULL);
      if (files_value != NULL && files_value[0] != '\0')
      {
         if (g_ascii_strcasecmp(files_value, "Short") == 0)
         {
            app->files_mode = HRMP_FILES_MODE_SHORT;
         }
         else
         {
            app->files_mode = HRMP_FILES_MODE_FULL;
         }
         g_free(files_value);
      }
   }
   else if (error != NULL && !g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
   {
      g_warning("Failed to load hrmp-ui preferences: %s", error->message);
   }

   if (error != NULL)
   {
      g_error_free(error);
   }

   g_free(config_path);
   g_key_file_unref(keyfile);
}

typedef enum
{
   HRMP_SUPPORT_UNKNOWN = 0,
   HRMP_SUPPORT_NO,
   HRMP_SUPPORT_YES
} HrmpSupport;

struct HrmpDevice
{
   gchar* name;          /* e.g. "FIIO QX13" */
   gboolean active;      /* Active: Yes */
   gchar* active_text;   /* "Yes" / "No" / "Unknown" */

   gchar* device;        /* Device: */
   gchar* description;   /* Description: */
   gchar* hardware;      /* Hardware: */
   gchar* selem;         /* Selem: */
   gchar* volume;        /* Volume: */
   gchar* paused;        /* Paused: */

   /* Per-format support (defaults to HRMP_SUPPORT_UNKNOWN) */
   HrmpSupport s16;
   HrmpSupport s16_le;
   HrmpSupport s16_be;
   HrmpSupport u16;
   HrmpSupport u16_le;
   HrmpSupport u16_be;

   HrmpSupport s24;
   HrmpSupport s24_3le;
   HrmpSupport s24_le;
   HrmpSupport s24_be;
   HrmpSupport u24;
   HrmpSupport u24_le;
   HrmpSupport u24_be;

   HrmpSupport s32;
   HrmpSupport s32_le;
   HrmpSupport s32_be;
   HrmpSupport u32;
   HrmpSupport u32_le;
   HrmpSupport u32_be;

   HrmpSupport dsd_u8;
   HrmpSupport dsd_u16_le;
   HrmpSupport dsd_u16_be;
   HrmpSupport dsd_u32_le;
   HrmpSupport dsd_u32_be;
};

static void
hrmp_gtk_free_devices(struct App* app)
{
   if (app->devices == NULL)
   {
      return;
   }

   for (guint i = 0; i < app->devices->len; i++)
   {
      struct HrmpDevice* dev = app->devices->pdata[i];
      g_free(dev->name);
      g_free(dev->active_text);
      g_free(dev->device);
      g_free(dev->description);
      g_free(dev->hardware);
      g_free(dev->selem);
      g_free(dev->volume);
      g_free(dev->paused);
      g_free(dev);
   }

   g_ptr_array_free(app->devices, TRUE);
   app->devices = NULL;
}

static GtkWidget*
hrmp_gtk_support_icon(HrmpSupport support)
{
   const gchar* text;

   switch (support)
   {
      case HRMP_SUPPORT_YES:
         text = "Yes";
         break;
      case HRMP_SUPPORT_NO:
         text = "No";
         break;
      case HRMP_SUPPORT_UNKNOWN:
      default:
         text = "?";
         break;
   }

   return gtk_label_new(text);
}

static void
hrmp_gtk_parse_devices(struct App* app, const gchar* text)
{
   hrmp_gtk_free_devices(app);

   if (text == NULL)
   {
      return;
   }

   app->devices = g_ptr_array_new();

   gchar** lines = g_strsplit(text, "\n", -1);
   struct HrmpDevice* current = NULL;
   const gchar* current_group = NULL; /* "16bit", "24bit", "32bit", "DSD" */

   for (gint i = 0; lines[i] != NULL; i++)
   {
      const gchar* orig = lines[i];
      gboolean indented = (orig[0] != '\0' && g_ascii_isspace(orig[0]));
      gchar* line = g_strstrip(lines[i]);

      if (line[0] == '\0')
      {
         continue;
      }

      if (indented && g_str_has_prefix(line, "Active:"))
      {
         if (current != NULL)
         {
            const gchar* p = line + strlen("Active:");
            while (g_ascii_isspace(*p))
            {
               p++;
            }

            /* Store textual state */
            g_free(current->active_text);
            {
               gchar* tmp = g_strdup(p);
               current->active_text = g_strstrip(tmp);
            }

            if (g_ascii_strncasecmp(p, "Yes", 3) == 0)
            {
               current->active = TRUE;
            }
            else
            {
               current->active = FALSE;
            }
         }

         continue;
      }

      if (indented && g_str_has_prefix(line, "Device:"))
      {
         if (current != NULL)
         {
            const gchar* p = line + strlen("Device:");
            while (g_ascii_isspace(*p))
            {
               p++;
            }
            g_free(current->device);
            {
               gchar* tmp = g_strdup(p);
               current->device = g_strstrip(tmp);
            }
         }
         continue;
      }

      if (indented && g_str_has_prefix(line, "Description:"))
      {
         if (current != NULL)
         {
            const gchar* p = line + strlen("Description:");
            while (g_ascii_isspace(*p))
            {
               p++;
            }
            g_free(current->description);
            {
               gchar* tmp = g_strdup(p);
               current->description = g_strstrip(tmp);
            }
         }
         continue;
      }

      if (indented && g_str_has_prefix(line, "Hardware:"))
      {
         if (current != NULL)
         {
            const gchar* p = line + strlen("Hardware:");
            while (g_ascii_isspace(*p))
            {
               p++;
            }
            g_free(current->hardware);
            {
               gchar* tmp = g_strdup(p);
               current->hardware = g_strstrip(tmp);
            }
         }
         continue;
      }

      if (indented && g_str_has_prefix(line, "Selem:"))
      {
         if (current != NULL)
         {
            const gchar* p = line + strlen("Selem:");
            while (g_ascii_isspace(*p))
            {
               p++;
            }
            g_free(current->selem);
            {
               gchar* tmp = g_strdup(p);
               current->selem = g_strstrip(tmp);
            }
         }
         continue;
      }

      if (indented && g_str_has_prefix(line, "Volume:"))
      {
         if (current != NULL)
         {
            const gchar* p = line + strlen("Volume:");
            while (g_ascii_isspace(*p))
            {
               p++;
            }
            g_free(current->volume);
            {
               gchar* tmp = g_strdup(p);
               current->volume = g_strstrip(tmp);
            }
         }
         continue;
      }

      if (indented && g_str_has_prefix(line, "Paused:"))
      {
         if (current != NULL)
         {
            const gchar* p = line + strlen("Paused:");
            while (g_ascii_isspace(*p))
            {
               p++;
            }
            g_free(current->paused);
            {
               gchar* tmp = g_strdup(p);
               current->paused = g_strstrip(tmp);
            }
         }
         continue;
      }

      if (indented && g_strcmp0(line, "16bit:") == 0)
      {
         current_group = "16bit";
         continue;
      }
      if (indented && g_strcmp0(line, "24bit:") == 0)
      {
         current_group = "24bit";
         continue;
      }
      if (indented && g_strcmp0(line, "32bit:") == 0)
      {
         current_group = "32bit";
         continue;
      }
      if (indented && g_strcmp0(line, "DSD:") == 0)
      {
         current_group = "DSD";
         continue;
      }

      if (indented && current != NULL && current_group != NULL)
      {
         /* Per-format line under current_group, e.g. "S16: Yes" */
         gchar* colon = strrchr(line, ':');
         if (colon != NULL)
         {
            gchar* key = g_strndup(line, colon - line);
            const gchar* p = colon + 1;
            HrmpSupport val = HRMP_SUPPORT_UNKNOWN;

            key = g_strstrip(key);

            while (g_ascii_isspace(*p))
            {
               p++;
            }

            if (g_ascii_strncasecmp(p, "Yes", 3) == 0)
            {
               val = HRMP_SUPPORT_YES;
            }
            else if (g_ascii_strncasecmp(p, "No", 2) == 0)
            {
               val = HRMP_SUPPORT_NO;
            }
            else
            {
               val = HRMP_SUPPORT_UNKNOWN;
            }

            if (g_strcmp0(current_group, "16bit") == 0)
            {
               if (g_strcmp0(key, "S16") == 0)
               {
                  current->s16 = val;
               }
               else if (g_strcmp0(key, "S16_LE") == 0)
               {
                  current->s16_le = val;
               }
               else if (g_strcmp0(key, "S16_BE") == 0)
               {
                  current->s16_be = val;
               }
               else if (g_strcmp0(key, "U16") == 0)
               {
                  current->u16 = val;
               }
               else if (g_strcmp0(key, "U16_LE") == 0)
               {
                  current->u16_le = val;
               }
               else if (g_strcmp0(key, "U16_BE") == 0)
               {
                  current->u16_be = val;
               }
            }
            else if (g_strcmp0(current_group, "24bit") == 0)
            {
               if (g_strcmp0(key, "S24") == 0)
               {
                  current->s24 = val;
               }
               else if (g_strcmp0(key, "S24_3LE") == 0)
               {
                  current->s24_3le = val;
               }
               else if (g_strcmp0(key, "S24_LE") == 0)
               {
                  current->s24_le = val;
               }
               else if (g_strcmp0(key, "S24_BE") == 0)
               {
                  current->s24_be = val;
               }
               else if (g_strcmp0(key, "U24") == 0)
               {
                  current->u24 = val;
               }
               else if (g_strcmp0(key, "U24_LE") == 0)
               {
                  current->u24_le = val;
               }
               else if (g_strcmp0(key, "U24_BE") == 0)
               {
                  current->u24_be = val;
               }
            }
            else if (g_strcmp0(current_group, "32bit") == 0)
            {
               if (g_strcmp0(key, "S32") == 0)
               {
                  current->s32 = val;
               }
               else if (g_strcmp0(key, "S32_LE") == 0)
               {
                  current->s32_le = val;
               }
               else if (g_strcmp0(key, "S32_BE") == 0)
               {
                  current->s32_be = val;
               }
               else if (g_strcmp0(key, "U32") == 0)
               {
                  current->u32 = val;
               }
               else if (g_strcmp0(key, "U32_LE") == 0)
               {
                  current->u32_le = val;
               }
               else if (g_strcmp0(key, "U32_BE") == 0)
               {
                  current->u32_be = val;
               }
            }
            else if (g_strcmp0(current_group, "DSD") == 0)
            {
               if (g_strcmp0(key, "U8") == 0)
               {
                  current->dsd_u8 = val;
               }
               else if (g_strcmp0(key, "U16_LE") == 0)
               {
                  current->dsd_u16_le = val;
               }
               else if (g_strcmp0(key, "U16_BE") == 0)
               {
                  current->dsd_u16_be = val;
               }
               else if (g_strcmp0(key, "U32_LE") == 0)
               {
                  current->dsd_u32_le = val;
               }
               else if (g_strcmp0(key, "U32_BE") == 0)
               {
                  current->dsd_u32_be = val;
               }
            }

            g_free(key);
         }

         continue;
      }

      if (indented)
      {
         /* Other indented properties are ignored for now */
         continue;
      }

      current = g_new0(struct HrmpDevice, 1);
      current->name = g_strdup(line);
      current->active = FALSE; /* only set TRUE if we see Active: Yes */
      current_group = NULL;
      g_ptr_array_add(app->devices, current);
   }

   g_strfreev(lines);

   hrmp_gtk_update_status(app);
}

static void
hrmp_gtk_ensure_devices(struct App* app)
{
   if (app->devices != NULL && app->devices->len > 0)
   {
      return;
   }

   const gchar* program;
   gchar* argv[3];
   gchar* stdout_buf = NULL;
   gchar* stderr_buf = NULL;
   gint status = 0;
   GError* error = NULL;

   program = (app->hrmp_path != NULL && app->hrmp_path[0] != '\0') ? app->hrmp_path : HRMP_DEFAULT_PATH;
   argv[0] = (gchar*)program;
   argv[1] = (gchar*)"-s";
   argv[2] = NULL;

   if (!g_spawn_sync(NULL,
                     argv,
                     NULL,
                     G_SPAWN_SEARCH_PATH,
                     NULL,
                     NULL,
                     &stdout_buf,
                     &stderr_buf,
                     &status,
                     &error))
   {
      if (error != NULL)
      {
         g_warning("Failed to list devices on startup: %s", error->message);
         g_error_free(error);
      }
   }

   if (stdout_buf != NULL)
   {
      hrmp_gtk_parse_devices(app, stdout_buf);
   }

   g_free(stdout_buf);
   g_free(stderr_buf);
}

static void
hrmp_gtk_save_preferences(struct App* app)
{
   GKeyFile* keyfile;
   gchar* config_path;
   gchar* data;
   gsize length;
   GError* error = NULL;

   if (app->hrmp_path == NULL || app->hrmp_path[0] == '\0')
   {
      return;
   }

   keyfile = g_key_file_new();
   g_key_file_set_string(keyfile, "General", "hrmp_path", app->hrmp_path);
   if (app->default_device != NULL && app->default_device[0] != '\0')
   {
      g_key_file_set_string(keyfile, "General", "default_device", app->default_device);
   }

   g_key_file_set_string(keyfile,
                         "General",
                         "files",
                         (app->files_mode == HRMP_FILES_MODE_SHORT) ? "Short" : "Full");

   data = g_key_file_to_data(keyfile, &length, NULL);
   config_path = hrmp_gtk_get_config_path();

   if (config_path != NULL &&
       !g_file_set_contents(config_path, data, length, &error))
   {
      g_warning("Failed to save hrmp-ui preferences: %s", error->message);
      g_error_free(error);
   }

   g_free(config_path);
   g_free(data);
   g_key_file_unref(keyfile);
}

static void
hrmp_gtk_update_song_from_output(struct App* app)
{
   if (app->song_label == NULL || app->output_buffer == NULL)
   {
      return;
   }

   if (gtk_text_buffer_get_char_count(app->output_buffer) == 0)
   {
      gtk_label_set_text(GTK_LABEL(app->song_label), "");
      return;
   }

   GtkTextIter end;
   gtk_text_buffer_get_end_iter(app->output_buffer, &end);

   GtkTextIter line_start = end;

   if (!gtk_text_iter_starts_line(&line_start))
   {
      gtk_text_iter_backward_char(&line_start);
      while (!gtk_text_iter_starts_line(&line_start))
      {
         gunichar ch = gtk_text_iter_get_char(&line_start);
         if (ch == '\n')
         {
            gtk_text_iter_forward_char(&line_start);
            break;
         }
         if (!gtk_text_iter_backward_char(&line_start))
         {
            break;
         }
      }
   }

   gchar* line = gtk_text_buffer_get_text(app->output_buffer, &line_start, &end, FALSE);
   if (line != NULL)
   {
      g_strchomp(line);
      gtk_label_set_text(GTK_LABEL(app->song_label), line);
      g_free(line);
   }
}

static void
append_output(struct App* app, const gchar* text)
{
   GtkTextIter end;

   gtk_text_buffer_get_end_iter(app->output_buffer, &end);
   gtk_text_buffer_insert(app->output_buffer, &end, text, -1);
   gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(app->output_view),
                                      gtk_text_buffer_get_insert(app->output_buffer));

   hrmp_gtk_update_song_from_output(app);
}

static void
hrmp_gtk_update_status(struct App* app)
{
   if (app->status_label == NULL)
   {
      return;
   }

   if (app->devices == NULL || app->devices->len == 0)
   {
      hrmp_gtk_ensure_devices(app);
   }

   struct HrmpDevice* dev = NULL;
   const gchar* device_name = NULL;

   if (app->devices != NULL && app->devices->len > 0)
   {
      /* Prefer the configured default device if it matches a known device name */
      if (app->default_device != NULL && app->default_device[0] != '\0')
      {
         for (guint i = 0; i < app->devices->len; i++)
         {
            struct HrmpDevice* cand = app->devices->pdata[i];
            if (cand->name != NULL && g_strcmp0(cand->name, app->default_device) == 0)
            {
               dev = cand;
               break;
            }
         }
      }

      /* If no matching default, fall back to first active device, then first device */
      if (dev == NULL)
      {
         for (guint i = 0; i < app->devices->len; i++)
         {
            struct HrmpDevice* cand = app->devices->pdata[i];
            if (cand->active)
            {
               dev = cand;
               break;
            }
         }
      }

      if (dev == NULL)
      {
         dev = app->devices->pdata[0];
      }

      device_name = dev->name;
   }
   else if (app->default_device != NULL && app->default_device[0] != '\0')
   {
      /* No parsed device list yet, but we have a default name */
      device_name = app->default_device;
   }

   if (device_name == NULL)
   {
      gtk_label_set_text(GTK_LABEL(app->status_label), "");
      return;
   }

   gchar* text = NULL;

   if (dev == NULL)
   {
      /* No capability information available */
      text = g_strdup(device_name);
   }
   else
   {
      gboolean has16 =
         (dev->s16 == HRMP_SUPPORT_YES || dev->s16_le == HRMP_SUPPORT_YES ||
          dev->s16_be == HRMP_SUPPORT_YES || dev->u16 == HRMP_SUPPORT_YES ||
          dev->u16_le == HRMP_SUPPORT_YES || dev->u16_be == HRMP_SUPPORT_YES);
      gboolean has24 =
         (dev->s24 == HRMP_SUPPORT_YES || dev->s24_3le == HRMP_SUPPORT_YES ||
          dev->s24_le == HRMP_SUPPORT_YES || dev->s24_be == HRMP_SUPPORT_YES ||
          dev->u24 == HRMP_SUPPORT_YES || dev->u24_le == HRMP_SUPPORT_YES ||
          dev->u24_be == HRMP_SUPPORT_YES);
      gboolean has32 =
         (dev->s32 == HRMP_SUPPORT_YES || dev->s32_le == HRMP_SUPPORT_YES ||
          dev->s32_be == HRMP_SUPPORT_YES || dev->u32 == HRMP_SUPPORT_YES ||
          dev->u32_le == HRMP_SUPPORT_YES || dev->u32_be == HRMP_SUPPORT_YES);
      gboolean has_dsd =
         (dev->dsd_u8 == HRMP_SUPPORT_YES || dev->dsd_u16_le == HRMP_SUPPORT_YES ||
          dev->dsd_u16_be == HRMP_SUPPORT_YES || dev->dsd_u32_le == HRMP_SUPPORT_YES ||
          dev->dsd_u32_be == HRMP_SUPPORT_YES);

      GString* s = g_string_new(device_name != NULL ? device_name : "");

      if (has16)
      {
         g_string_append(s, "/16bit");
      }
      if (has24)
      {
         g_string_append(s, "/24bit");
      }
      if (has32)
      {
         g_string_append(s, "/32bit");
      }
      if (has_dsd)
      {
         g_string_append(s, "/DSD");
      }

      text = g_string_free(s, FALSE);
   }

   gtk_label_set_text(GTK_LABEL(app->status_label), text != NULL ? text : "");
   g_free(text);
}

static void
hrmp_gtk_update_playlist_display(struct App* app)
{
   if (app->list_store == NULL)
   {
      return;
   }

   GtkTreeModel* model = GTK_TREE_MODEL(app->list_store);
   GtkTreeIter iter;
   gboolean valid = gtk_tree_model_get_iter_first(model, &iter);

   while (valid)
   {
      gchar* full = NULL;
      gtk_tree_model_get(model, &iter, 0, &full, -1);
      if (full != NULL)
      {
         gchar* display;
         if (app->files_mode == HRMP_FILES_MODE_SHORT)
         {
            display = g_path_get_basename(full);
         }
         else
         {
            display = g_strdup(full);
         }

         gtk_list_store_set(app->list_store, &iter, 1, display, -1);
         g_free(display);
      }
      g_free(full);

      valid = gtk_tree_model_iter_next(model, &iter);
   }
}

static void
update_mode_button_icon(struct App* app)
{
   const gchar* icon_name = "media-playlist-consecutive";

   if (app->mode_button == NULL)
   {
      return;
   }

   switch (app->play_mode)
   {
      case HRMP_PLAY_MODE_REPEAT:
         icon_name = "media-playlist-repeat";
         gtk_widget_set_tooltip_text(app->mode_button, "Repeat (Ctrl+M)");
         break;
      case HRMP_PLAY_MODE_SHUFFLE:
         icon_name = "media-playlist-shuffle";
         gtk_widget_set_tooltip_text(app->mode_button, "Shuffle (Ctrl+M)");
         break;
      case HRMP_PLAY_MODE_END:
      default:
         icon_name = "media-playlist-consecutive";
         gtk_widget_set_tooltip_text(app->mode_button, "Once (Ctrl+M)");
         break;
   }

   GtkWidget* image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
   gtk_button_set_image(GTK_BUTTON(app->mode_button), image);
}

static void
update_play_button_icon(struct App* app)
{
   GtkWidget* image;

   if (app->play_button == NULL)
   {
      return;
   }

   if (app->hrmp_running && app->is_playing)
   {
      image = gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
      gtk_widget_set_tooltip_text(app->play_button, "Pause (Ctrl+P)");
   }
   else
   {
      image = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
      gtk_widget_set_tooltip_text(app->play_button, "Play (Ctrl+P)");
   }

   gtk_button_set_image(GTK_BUTTON(app->play_button), image);
}

static void
hrmp_gtk_handle_child_finished(struct App* app, gboolean allow_repeat)
{
   gboolean has_files = FALSE;

   if (app->list_store != NULL)
   {
      GtkTreeModel* model = GTK_TREE_MODEL(app->list_store);
      GtkTreeIter iter;

      has_files = gtk_tree_model_get_iter_first(model, &iter);
   }

   if (allow_repeat && app->play_mode == HRMP_PLAY_MODE_REPEAT && has_files)
   {
      stop_hrmp(app);
      start_hrmp(app);
   }
   else
   {
      stop_hrmp(app);
   }
}

static gboolean
on_child_stdout(GIOChannel* source, GIOCondition condition, gpointer data)
{
   struct App* app = data;
   GError* error = NULL;

   if (condition & (G_IO_ERR | G_IO_NVAL))
   {
      hrmp_gtk_handle_child_finished(app, FALSE);
      return FALSE;
   }

   if (condition & G_IO_HUP)
   {
      hrmp_gtk_handle_child_finished(app, TRUE);
      return FALSE;
   }

   gchar buf[4096];
   gsize bytes_read = 0;
   GIOStatus status = g_io_channel_read_chars(source, buf, sizeof(buf) - 1, &bytes_read, &error);

   if (status == G_IO_STATUS_ERROR)
   {
      if (error != NULL)
      {
         g_warning("Error reading hrmp output: %s", error->message);
         g_error_free(error);
      }
      return TRUE;
   }

   if (status == G_IO_STATUS_EOF)
   {
      hrmp_gtk_handle_child_finished(app, TRUE);
      return FALSE;
   }

   if (bytes_read > 0)
   {
      buf[bytes_read] = '\0';

      GtkTextBuffer* buffer = app->output_buffer;
      GtkTextIter end_iter;

      const gchar* p = buf;
      while (*p != '\0')
      {
         if (*p == '\r')
         {
            /* Replace last line with new status text after \r */
            ++p;
            gtk_text_buffer_get_end_iter(buffer, &end_iter);

            GtkTextIter line_start = end_iter;
            if (!gtk_text_iter_starts_line(&line_start))
            {
               gtk_text_iter_set_line_offset(&line_start, 0);
            }

            gtk_text_buffer_delete(buffer, &line_start, &end_iter);

            const gchar* start = p;
            while (*p != '\0' && *p != '\r' && *p != '\n')
            {
               ++p;
            }

            gtk_text_buffer_get_end_iter(buffer, &end_iter);
            gtk_text_buffer_insert(buffer, &end_iter, start, p - start);

            if (*p == '\n')
            {
               ++p;
               gtk_text_buffer_get_end_iter(buffer, &end_iter);
               gtk_text_buffer_insert(buffer, &end_iter, "\n", -1);
            }
         }
         else if (*p == '\n')
         {
            ++p;
            gtk_text_buffer_get_end_iter(buffer, &end_iter);
            gtk_text_buffer_insert(buffer, &end_iter, "\n", -1);
         }
         else
         {
            const gchar* start = p;
            while (*p != '\0' && *p != '\r' && *p != '\n')
            {
               ++p;
            }

            gtk_text_buffer_get_end_iter(buffer, &end_iter);
            gtk_text_buffer_insert(buffer, &end_iter, start, p - start);
         }
      }

      gtk_text_buffer_get_end_iter(buffer, &end_iter);
      gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(app->output_view),
                                         gtk_text_buffer_get_insert(buffer));

      hrmp_gtk_update_song_from_output(app);
   }

   return TRUE;
}

static void
stop_hrmp(struct App* app)
{
   if (app->stdout_watch_id != 0)
   {
      g_source_remove(app->stdout_watch_id);
      app->stdout_watch_id = 0;
   }

   if (app->child_stdout != NULL)
   {
      g_io_channel_shutdown(app->child_stdout, FALSE, NULL);
      g_io_channel_unref(app->child_stdout);
      app->child_stdout = NULL;
   }

   if (app->child_stdin != NULL)
   {
      g_io_channel_shutdown(app->child_stdin, FALSE, NULL);
      g_io_channel_unref(app->child_stdin);
      app->child_stdin = NULL;
   }

   if (app->child_pid > 0)
   {
      g_spawn_close_pid(app->child_pid);
      app->child_pid = 0;
   }

   app->hrmp_running = FALSE;
   app->is_playing = FALSE;

   update_play_button_icon(app);

   if (app->song_label != NULL)
   {
      gtk_label_set_text(GTK_LABEL(app->song_label), "");
   }
}

static void
send_escape_sequence(struct App* app, const gchar* seq)
{
   if (!app->hrmp_running || app->child_stdin == NULL || seq == NULL || *seq == '\0')
   {
      return;
   }

   gsize bytes_written = 0;
   GError* error = NULL;

   GIOStatus status = g_io_channel_write_chars(app->child_stdin,
                                               seq,
                                               strlen(seq),
                                               &bytes_written,
                                               &error);
   if (status == G_IO_STATUS_ERROR)
   {
      if (error != NULL)
      {
         g_warning("Error writing to hrmp: %s", error->message);
         g_error_free(error);
      }
      return;
   }

   g_io_channel_flush(app->child_stdin, NULL);
}

static void
send_key(struct App* app, gchar c)
{
   if (!app->hrmp_running || app->child_stdin == NULL)
   {
      return;
   }

   gchar buf[2];
   buf[0] = c;
   buf[1] = '\0';

   gsize bytes_written = 0;
   GError* error = NULL;

   GIOStatus status = g_io_channel_write_chars(app->child_stdin,
                                               buf,
                                               1,
                                               &bytes_written,
                                               &error);
   if (status == G_IO_STATUS_ERROR)
   {
      if (error != NULL)
      {
         g_warning("Error writing to hrmp: %s", error->message);
         g_error_free(error);
      }
      return;
   }

   g_io_channel_flush(app->child_stdin, NULL);
}

static void
on_button_prev_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   if (app->hrmp_running)
   {
      GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->list_view));
      GtkTreeModel* model = NULL;
      GtkTreeIter iter;

      if (gtk_tree_selection_get_selected(selection, &model, &iter))
      {
         GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
         if (path != NULL)
         {
            if (gtk_tree_path_prev(path))
            {
               GtkTreeIter prev_iter;
               if (gtk_tree_model_get_iter(model, &prev_iter, path))
               {
                  gtk_tree_selection_unselect_all(selection);
                  gtk_tree_selection_select_iter(selection, &prev_iter);
               }
            }
            gtk_tree_path_free(path);
         }
      }
   }

   /* Map Previous to backslash key */
   send_key(app, '\\');
}

static void
on_button_next_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   if (app->hrmp_running)
   {
      GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->list_view));
      GtkTreeModel* model = NULL;
      GtkTreeIter iter;

      if (gtk_tree_selection_get_selected(selection, &model, &iter))
      {
         GtkTreeIter next_iter = iter;
         if (gtk_tree_model_iter_next(model, &next_iter))
         {
            gtk_tree_selection_unselect_all(selection);
            gtk_tree_selection_select_iter(selection, &next_iter);
         }
      }
   }

   /* hrmp maps ENTER to KEYBOARD_ENTER; send newline directly */
   send_key(app, '\n');
}

static void
on_button_volume_down_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   /* Map volume down to ',' */
   send_key(app, ',');
}

static void
on_button_volume_up_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   /* Map volume up to '.' */
   send_key(app, '.');
}

static void
on_button_skip_back_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   /* Map Back skip to ARROW_DOWN (rewind 1 minute) */
   send_escape_sequence(app, "\x1b[B");
}

static void
on_button_skip_ahead_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   /* Map Ahead skip to ARROW_UP (forward 1 minute) */
   send_escape_sequence(app, "\x1b[A");
}

static void
on_button_stop_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   /* hrmp maps 'q' to quit */
   if (app->hrmp_running)
   {
      send_key(app, 'q');
   }
   stop_hrmp(app);
}

static void
start_hrmp(struct App* app)
{
   if (app->hrmp_running)
   {
      return;
   }

   GtkTreeModel* model = GTK_TREE_MODEL(app->list_store);
   GtkTreeSelection* selection;
   GtkTreeIter iter;
   GtkTreeIter start_iter;
   GtkTreePath* start_path = NULL;
   gboolean have_start = FALSE;
   gboolean valid;
   GPtrArray* args = g_ptr_array_new();
   const gchar* program;
   guint file_start_index;
   gchar* first_file_display = NULL;

   program = (app->hrmp_path != NULL && app->hrmp_path[0] != '\0') ? app->hrmp_path : HRMP_DEFAULT_PATH;
   g_ptr_array_add(args, g_strdup(program));

   if (app->default_device != NULL && app->default_device[0] != '\0')
   {
      g_ptr_array_add(args, g_strdup("-D"));
      g_ptr_array_add(args, g_strdup(app->default_device));
   }

   file_start_index = args->len;

   selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->list_view));
   have_start = gtk_tree_selection_get_selected(selection, &model, &start_iter);

   /* If nothing is selected, start from the first row and select it so the
    * currently playing file is highlighted in the playlist. */
   if (!have_start)
   {
      have_start = gtk_tree_model_get_iter_first(model, &start_iter);
      if (have_start)
      {
         gtk_tree_selection_unselect_all(selection);
         gtk_tree_selection_select_iter(selection, &start_iter);
      }
   }

   if (have_start)
   {
      start_path = gtk_tree_model_get_path(model, &start_iter);
   }

   if (have_start)
   {
      iter = start_iter;
      do
      {
         gchar* filepath = NULL;

         gtk_tree_model_get(model,
                            &iter,
                            0,
                            &filepath,
                            -1);

         if (filepath != NULL && filepath[0] != '\0')
         {
            g_ptr_array_add(args, filepath);
         }
      }
      while (gtk_tree_model_iter_next(model, &iter));

      valid = gtk_tree_model_get_iter_first(model, &iter);
      while (valid)
      {
         GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
         if (gtk_tree_path_compare(path, start_path) == 0)
         {
            gtk_tree_path_free(path);
            break;
         }

         gchar* filepath = NULL;
         gtk_tree_model_get(model,
                            &iter,
                            0,
                            &filepath,
                            -1);

         if (filepath != NULL && filepath[0] != '\0')
         {
            g_ptr_array_add(args, filepath);
         }

         gtk_tree_path_free(path);
         valid = gtk_tree_model_iter_next(model, &iter);
      }

      if (start_path != NULL)
      {
         gtk_tree_path_free(start_path);
      }
   }

   /* Determine first file for status panel (current song). */
   if (args->len > file_start_index)
   {
      gchar* first_file = args->pdata[file_start_index];
      if (first_file != NULL && first_file[0] != '\0')
      {
         if (app->files_mode == HRMP_FILES_MODE_SHORT)
         {
            first_file_display = g_path_get_basename(first_file);
         }
         else
         {
            first_file_display = g_strdup(first_file);
         }
      }
   }

   /* Shuffle playback order if requested (keep first file as starting point) */
   if (app->play_mode == HRMP_PLAY_MODE_SHUFFLE && args->len > file_start_index + 1)
   {
      for (guint i = file_start_index + 1; i < args->len; i++)
      {
         guint j = g_random_int_range(i, args->len);
         gpointer tmp = args->pdata[i];
         args->pdata[i] = args->pdata[j];
         args->pdata[j] = tmp;
      }
   }

   if (args->len <= 1)
   {
      append_output(app, "No files selected. Add files before starting hrmp.\n");

      if (app->song_label != NULL)
      {
         gtk_label_set_text(GTK_LABEL(app->song_label), "");
      }

      for (guint i = 0; i < args->len; i++)
      {
         g_free(args->pdata[i]);
      }
      g_ptr_array_free(args, TRUE);
      return;
   }

   g_ptr_array_add(args, NULL);

   gchar** argv = (gchar**)g_ptr_array_free(args, FALSE);

   gint stdin_fd = -1;
   gint stdout_fd = -1;
   gint stderr_fd = -1;
   GError* error = NULL;

   gboolean spawned = g_spawn_async_with_pipes(NULL,
                                               argv,
                                               NULL,
                                               G_SPAWN_DO_NOT_REAP_CHILD,
                                               NULL,
                                               NULL,
                                               &app->child_pid,
                                               &stdin_fd,
                                               &stdout_fd,
                                               &stderr_fd,
                                               &error);

   for (guint i = 0; argv[i] != NULL; i++)
   {
      g_free(argv[i]);
   }
   g_free(argv);

   if (!spawned)
   {
      gchar* msg = NULL;

      if (first_file_display != NULL)
      {
         g_free(first_file_display);
         first_file_display = NULL;
      }

      if (app->default_device != NULL && app->default_device[0] != '\0')
      {
         msg = g_strdup_printf("Failed to acquire device %s\n", app->default_device);
      }
      else if (error != NULL)
      {
         msg = g_strdup_printf("Failed to start hrmp: %s\n", error->message);
      }

      if (msg != NULL)
      {
         append_output(app, msg);
         g_free(msg);
      }

      if (error != NULL)
      {
         g_error_free(error);
      }

      return;
   }

   app->child_stdin = g_io_channel_unix_new(stdin_fd);
   app->child_stdout = g_io_channel_unix_new(stdout_fd);

   g_io_channel_set_encoding(app->child_stdout, NULL, NULL);
   g_io_channel_set_buffered(app->child_stdout, FALSE);

   g_io_channel_set_encoding(app->child_stdin, NULL, NULL);
   g_io_channel_set_buffered(app->child_stdin, FALSE);

   app->stdout_watch_id = g_io_add_watch(app->child_stdout,
                                         G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                                         on_child_stdout,
                                         app);

   app->hrmp_running = TRUE;
   app->is_playing = TRUE;
   update_play_button_icon(app);

   if (first_file_display != NULL && app->song_label != NULL)
   {
      gtk_label_set_text(GTK_LABEL(app->song_label), first_file_display);
      g_free(first_file_display);
      first_file_display = NULL;
   }
}

static void
on_button_play_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   if (!app->hrmp_running)
   {
      /* (Re)start hrmp with current playlist */
      start_hrmp(app);
      app->is_playing = TRUE;
   }
   else
   {
      /* Toggle pause/resume via space */
      app->is_playing = !app->is_playing;
      send_key(app, ' ');
   }

   update_play_button_icon(app);
}

static void
on_button_add_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   GtkWidget* dialog = gtk_file_chooser_dialog_new("Select audio files",
                                                   GTK_WINDOW(app->window),
                                                   GTK_FILE_CHOOSER_ACTION_OPEN,
                                                   "_Cancel",
                                                   GTK_RESPONSE_CANCEL,
                                                   "_Open",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);

   gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

   if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
   {
      GSList* files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
      for (GSList* l = files; l != NULL; l = l->next)
      {
         const gchar* filename = l->data;
         GtkTreeIter iter;

         gtk_list_store_append(app->list_store, &iter);

         if (app->files_mode == HRMP_FILES_MODE_SHORT)
         {
            gchar* base = g_path_get_basename(filename);
            gtk_list_store_set(app->list_store,
                               &iter,
                               0,
                               filename,
                               1,
                               base,
                               -1);
            g_free(base);
         }
         else
         {
            gtk_list_store_set(app->list_store,
                               &iter,
                               0,
                               filename,
                               1,
                               filename,
                               -1);
         }

         g_free(l->data);
      }
      g_slist_free(files);
   }

   gtk_widget_destroy(dialog);
}

static void
on_button_clear_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   /* Clear hrmp queue: stop any running instance and clear UI list */
   if (app->hrmp_running)
   {
      send_key(app, 'q');
      stop_hrmp(app);
   }

   gtk_list_store_clear(app->list_store);
}

static gchar*
hrmp_gtk_load_license_text(void)
{
   GBytes* bytes;
   GError* error = NULL;
   gsize length = 0;
   const gchar* data;

   bytes = g_resources_lookup_data("/org/hrmp/hrmp-ui/gpl-3.0.txt",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   &error);
   if (bytes == NULL)
   {
      if (error != NULL)
      {
         g_warning("Failed to load license resource: %s", error->message);
         g_error_free(error);
      }
      return g_strdup("GNU General Public License version 3\n\nLicense resource not available.\n");
   }

   data = g_bytes_get_data(bytes, &length);
   gchar* text = g_strndup(data, length);
   g_bytes_unref(bytes);

   return text;
}

static gchar*
hrmp_gtk_load_authors_text(void)
{
   GBytes* bytes;
   GError* error = NULL;
   gsize length = 0;
   const gchar* data;

   bytes = g_resources_lookup_data("/org/hrmp/hrmp-ui/AUTHORS",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   &error);
   if (bytes == NULL)
   {
      if (error != NULL)
      {
         g_warning("Failed to load AUTHORS resource: %s", error->message);
         g_error_free(error);
      }
      return g_strdup("Authors information not available.\n");
   }

   data = g_bytes_get_data(bytes, &length);
   gchar* text = g_strndup(data, length);
   g_bytes_unref(bytes);

   return text;
}

static void
on_menu_about(GtkWidget* widget, gpointer user_data)
{
   struct App* app = user_data;
   GtkWidget* dialog;
   gchar* message;
   gchar* authors;

   message = g_strdup_printf("hrmp-ui %s", HRMP_GTK_VERSION_STRING);
   authors = hrmp_gtk_load_authors_text();

   dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   "%s",
                                   message);

   gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", authors);

   g_free(authors);
   g_free(message);

   gtk_dialog_run(GTK_DIALOG(dialog));
   gtk_widget_destroy(dialog);
}

static void
on_menu_list_devices(GtkWidget* widget, gpointer user_data)
{
   struct App* app = user_data;
   const gchar* program;
   gchar* argv[3];
   gchar* stdout_buf = NULL;
   gchar* stderr_buf = NULL;
   gint status = 0;
   GError* error = NULL;

   program = (app->hrmp_path != NULL && app->hrmp_path[0] != '\0') ? app->hrmp_path : HRMP_DEFAULT_PATH;

   argv[0] = (gchar*)program;
   argv[1] = (gchar*)"-s";
   argv[2] = NULL;

   if (!g_spawn_sync(NULL,
                     argv,
                     NULL,
                     G_SPAWN_SEARCH_PATH,
                     NULL,
                     NULL,
                     &stdout_buf,
                     &stderr_buf,
                     &status,
                     &error))
   {
      gchar* msg = g_strdup_printf("Failed to list devices: %s", error != NULL ? error->message : "unknown error");
      GtkWidget* md = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             "%s",
                                             msg);
      g_free(msg);
      if (error != NULL)
      {
         g_error_free(error);
      }
      gtk_dialog_run(GTK_DIALOG(md));
      gtk_widget_destroy(md);
      g_free(stdout_buf);
      g_free(stderr_buf);
      return;
   }

   if (stdout_buf != NULL)
   {
      hrmp_gtk_parse_devices(app, stdout_buf);
   }

   GtkWidget* dialog = gtk_dialog_new_with_buttons("Devices",
                                                   GTK_WINDOW(app->window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "_Close",
                                                   GTK_RESPONSE_CLOSE,
                                                   NULL);
   gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 500);

   GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
   GtkWidget* notebook = gtk_notebook_new();
   gtk_widget_set_hexpand(notebook, TRUE);
   gtk_widget_set_vexpand(notebook, TRUE);
   gtk_container_add(GTK_CONTAINER(content_area), notebook);

   /* One tab per device */
   if (app->devices != NULL && app->devices->len > 0)
   {
      for (guint i = 0; i < app->devices->len; i++)
      {
         struct HrmpDevice* dev = app->devices->pdata[i];

         GtkWidget* tab_label = gtk_label_new(dev->name != NULL ? dev->name : "(unknown)");
         GtkWidget* tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
         gtk_container_set_border_width(GTK_CONTAINER(tab_box), 8);

         /* Overview grid */
         GtkWidget* overview_frame = gtk_frame_new("Overview");
         GtkWidget* overview_grid = gtk_grid_new();
         gint orow = 0;
         gtk_grid_set_row_spacing(GTK_GRID(overview_grid), 4);
         gtk_grid_set_column_spacing(GTK_GRID(overview_grid), 8);

         GtkWidget* dev_label = gtk_label_new("Device:");
         GtkWidget* dev_value = gtk_label_new(dev->device != NULL ? dev->device : "-");
         gtk_widget_set_halign(dev_label, GTK_ALIGN_START);
         gtk_widget_set_halign(dev_value, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(overview_grid), dev_label, 0, orow, 1, 1);
         gtk_grid_attach(GTK_GRID(overview_grid), dev_value, 1, orow, 1, 1);
         orow++;

         GtkWidget* desc_label = gtk_label_new("Description:");
         GtkWidget* desc_value = gtk_label_new(dev->description != NULL ? dev->description : "-");
         gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
         gtk_widget_set_halign(desc_value, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(overview_grid), desc_label, 0, orow, 1, 1);
         gtk_grid_attach(GTK_GRID(overview_grid), desc_value, 1, orow, 1, 1);
         orow++;

         GtkWidget* hw_label = gtk_label_new("Hardware:");
         GtkWidget* hw_value = gtk_label_new(dev->hardware != NULL ? dev->hardware : "-");
         gtk_widget_set_halign(hw_label, GTK_ALIGN_START);
         gtk_widget_set_halign(hw_value, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(overview_grid), hw_label, 0, orow, 1, 1);
         gtk_grid_attach(GTK_GRID(overview_grid), hw_value, 1, orow, 1, 1);
         orow++;

         GtkWidget* selem_label = gtk_label_new("Selem:");
         GtkWidget* selem_value = gtk_label_new(dev->selem != NULL ? dev->selem : "-");
         gtk_widget_set_halign(selem_label, GTK_ALIGN_START);
         gtk_widget_set_halign(selem_value, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(overview_grid), selem_label, 0, orow, 1, 1);
         gtk_grid_attach(GTK_GRID(overview_grid), selem_value, 1, orow, 1, 1);
         orow++;

         GtkWidget* active_label = gtk_label_new("Active:");
         const gchar* active_text = dev->active_text != NULL ? dev->active_text : (dev->active ? "Yes" : "No");
         GtkWidget* active_value = gtk_label_new(active_text);
         gtk_widget_set_halign(active_label, GTK_ALIGN_START);
         gtk_widget_set_halign(active_value, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(overview_grid), active_label, 0, orow, 1, 1);
         gtk_grid_attach(GTK_GRID(overview_grid), active_value, 1, orow, 1, 1);
         orow++;

         GtkWidget* volume_label = gtk_label_new("Volume:");
         GtkWidget* volume_value = gtk_label_new(dev->volume != NULL ? dev->volume : "-");
         gtk_widget_set_halign(volume_label, GTK_ALIGN_START);
         gtk_widget_set_halign(volume_value, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(overview_grid), volume_label, 0, orow, 1, 1);
         gtk_grid_attach(GTK_GRID(overview_grid), volume_value, 1, orow, 1, 1);
         orow++;

         GtkWidget* paused_label = gtk_label_new("Paused:");
         GtkWidget* paused_value = gtk_label_new(dev->paused != NULL ? dev->paused : "-");
         gtk_widget_set_halign(paused_label, GTK_ALIGN_START);
         gtk_widget_set_halign(paused_value, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(overview_grid), paused_label, 0, orow, 1, 1);
         gtk_grid_attach(GTK_GRID(overview_grid), paused_value, 1, orow, 1, 1);

         gtk_container_add(GTK_CONTAINER(overview_frame), overview_grid);
         gtk_box_pack_start(GTK_BOX(tab_box), overview_frame, FALSE, FALSE, 0);

         /* Audio section */
         GtkWidget* audio_frame = gtk_frame_new("Audio");
         GtkWidget* audio_grid = gtk_grid_new();
         gtk_grid_set_row_spacing(GTK_GRID(audio_grid), 4);
         gtk_grid_set_column_spacing(GTK_GRID(audio_grid), 8);

         /* 16bit row: S16, S16_LE, S16_BE, U16, U16_LE, U16_BE */
         GtkWidget* lbl16 = gtk_label_new("16bit:");
         gtk_widget_set_halign(lbl16, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(audio_grid), lbl16, 0, 0, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s16), 1, 0, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s16_le), 2, 0, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s16_be), 3, 0, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u16), 4, 0, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u16_le), 5, 0, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u16_be), 6, 0, 1, 1);

         /* 24bit row: S24, S24_3LE, S24_LE, S24_BE, U24, U24_LE, U24_BE */
         GtkWidget* lbl24 = gtk_label_new("24bit:");
         gtk_widget_set_halign(lbl24, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(audio_grid), lbl24, 0, 1, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s24), 1, 1, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s24_3le), 2, 1, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s24_le), 3, 1, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s24_be), 4, 1, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u24), 5, 1, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u24_le), 6, 1, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u24_be), 7, 1, 1, 1);

         /* 32bit row: S32, S32_LE, S32_BE, U32, U32_LE, U32_BE */
         GtkWidget* lbl32 = gtk_label_new("32bit:");
         gtk_widget_set_halign(lbl32, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(audio_grid), lbl32, 0, 2, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s32), 1, 2, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s32_le), 2, 2, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->s32_be), 3, 2, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u32), 4, 2, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u32_le), 5, 2, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->u32_be), 6, 2, 1, 1);

         /* DSD row: U8, U16_LE, U16_BE, U32_LE, U32_BE */
         GtkWidget* lbldsd = gtk_label_new("DSD:");
         gtk_widget_set_halign(lbldsd, GTK_ALIGN_START);
         gtk_grid_attach(GTK_GRID(audio_grid), lbldsd, 0, 3, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->dsd_u8), 1, 3, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->dsd_u16_le), 2, 3, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->dsd_u16_be), 3, 3, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->dsd_u32_le), 4, 3, 1, 1);
         gtk_grid_attach(GTK_GRID(audio_grid), hrmp_gtk_support_icon(dev->dsd_u32_be), 5, 3, 1, 1);

         gtk_container_add(GTK_CONTAINER(audio_frame), audio_grid);
         gtk_box_pack_start(GTK_BOX(tab_box), audio_frame, FALSE, FALSE, 0);

         gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
      }
   }
   else
   {
      GtkWidget* label = gtk_label_new("No devices found.");
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), label, gtk_label_new("Devices"));
   }

   gtk_widget_show_all(dialog);
   gtk_dialog_run(GTK_DIALOG(dialog));
   gtk_widget_destroy(dialog);

   g_free(stdout_buf);
   g_free(stderr_buf);
}

static void
on_menu_license(GtkWidget* widget, gpointer user_data)
{
   struct App* app = user_data;
   GtkWidget* dialog;
   GtkWidget* content_area;
   GtkWidget* scrolled;
   GtkWidget* text_view;
   GtkTextBuffer* buffer;
   gchar* license_text;

   license_text = hrmp_gtk_load_license_text();

   dialog = gtk_dialog_new_with_buttons("License",
                                        GTK_WINDOW(app->window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "_Close",
                                        GTK_RESPONSE_CLOSE,
                                        NULL);

   gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 600);

   content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

   scrolled = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
   gtk_widget_set_hexpand(scrolled, TRUE);
   gtk_widget_set_vexpand(scrolled, TRUE);

   text_view = gtk_text_view_new();
   gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
   gtk_widget_set_hexpand(text_view, TRUE);
   gtk_widget_set_vexpand(text_view, TRUE);
   gtk_container_add(GTK_CONTAINER(scrolled), text_view);

   buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
   gtk_text_buffer_set_text(buffer, license_text, -1);

   g_free(license_text);

   GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
   gtk_container_add(GTK_CONTAINER(content_area), vbox);

   gtk_widget_show_all(dialog);

   gtk_dialog_run(GTK_DIALOG(dialog));
   gtk_widget_destroy(dialog);
}

static void
on_menu_preferences(GtkWidget* widget, gpointer user_data)
{
   struct App* app = user_data;
   GtkWidget* dialog;
   GtkWidget* content_area;
   GtkWidget* box;
   GtkWidget* hbox;
   GtkWidget* label;
   GtkWidget* entry;
   GtkWidget* device_hbox;
   GtkWidget* device_label;
   GtkWidget* device_combo = NULL;
   GtkWidget* files_hbox;
   GtkWidget* files_label;
   GtkWidget* files_combo;
   GtkListStore* device_store = NULL;
   GtkTreeIter iter;
   const gchar* current;

   dialog = gtk_dialog_new_with_buttons("Preferences",
                                        GTK_WINDOW(app->window),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "_Cancel",
                                        GTK_RESPONSE_CANCEL,
                                        "_OK",
                                        GTK_RESPONSE_OK,
                                        NULL);

   content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

   box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
   gtk_container_set_border_width(GTK_CONTAINER(box), 8);
   gtk_container_add(GTK_CONTAINER(content_area), box);

   hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
   label = gtk_label_new("hrmp binary path:");
   entry = gtk_entry_new();

   current = (app->hrmp_path != NULL && app->hrmp_path[0] != '\0') ? app->hrmp_path : HRMP_DEFAULT_PATH;
   gtk_entry_set_text(GTK_ENTRY(entry), current);

   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
   gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 4);
   gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 4);

   /* Default device selection */
   device_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
   device_label = gtk_label_new("Default device:");
   gtk_box_pack_start(GTK_BOX(device_hbox), device_label, FALSE, FALSE, 4);

   /* Ensure we have a device list by calling hrmp -s once if needed */
   if (app->devices == NULL || app->devices->len == 0)
   {
      const gchar* program;
      gchar* argv[3];
      gchar* stdout_buf = NULL;
      gchar* stderr_buf = NULL;
      gint status = 0;
      GError* error = NULL;

      program = (app->hrmp_path != NULL && app->hrmp_path[0] != '\0') ? app->hrmp_path : HRMP_DEFAULT_PATH;
      argv[0] = (gchar*)program;
      argv[1] = (gchar*)"-s";
      argv[2] = NULL;

      if (!g_spawn_sync(NULL,
                        argv,
                        NULL,
                        G_SPAWN_SEARCH_PATH,
                        NULL,
                        NULL,
                        &stdout_buf,
                        &stderr_buf,
                        &status,
                        &error))
      {
         gchar* msg = g_strdup_printf("Failed to list devices: %s", error != NULL ? error->message : "unknown error");
         GtkWidget* md = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_OK,
                                                "%s",
                                                msg);
         g_free(msg);
         if (error != NULL)
         {
            g_error_free(error);
         }
         gtk_dialog_run(GTK_DIALOG(md));
         gtk_widget_destroy(md);
      }

      if (stdout_buf != NULL)
      {
         hrmp_gtk_parse_devices(app, stdout_buf);
      }

      g_free(stdout_buf);
      g_free(stderr_buf);
   }

   device_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);

   if (app->devices != NULL)
   {
      for (guint i = 0; i < app->devices->len; i++)
      {
         struct HrmpDevice* dev = app->devices->pdata[i];
         gtk_list_store_append(device_store, &iter);
         gtk_list_store_set(device_store,
                            &iter,
                            0, dev->name,
                            1, dev->active,
                            -1);
      }
   }

   device_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(device_store));

   GtkCellRenderer* cell = gtk_cell_renderer_text_new();
   gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(device_combo), cell, TRUE);
   gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(device_combo), cell, "text", 0);
   gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(device_combo), cell, "sensitive", 1);

   /* preselect current default, or first active */
   if (app->devices != NULL && app->devices->len > 0)
   {
      GtkTreeModel* model = GTK_TREE_MODEL(device_store);
      gboolean found = FALSE;

      if (app->default_device != NULL && app->default_device[0] != '\0')
      {
         gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
         while (valid)
         {
            gchar* name = NULL;
            gtk_tree_model_get(model, &iter, 0, &name, -1);
            if (name != NULL && g_strcmp0(name, app->default_device) == 0)
            {
               gtk_combo_box_set_active_iter(GTK_COMBO_BOX(device_combo), &iter);
               found = TRUE;
               g_free(name);
               break;
            }
            g_free(name);
            valid = gtk_tree_model_iter_next(model, &iter);
         }
      }

      if (!found)
      {
         gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
         while (valid)
         {
            gboolean active = FALSE;
            gtk_tree_model_get(model, &iter, 1, &active, -1);
            if (active)
            {
               gtk_combo_box_set_active_iter(GTK_COMBO_BOX(device_combo), &iter);
               break;
            }
            valid = gtk_tree_model_iter_next(model, &iter);
         }
      }
   }

   gtk_box_pack_start(GTK_BOX(device_hbox), device_combo, TRUE, TRUE, 4);
   gtk_box_pack_start(GTK_BOX(box), device_hbox, FALSE, FALSE, 4);

   /* Files display mode */
   files_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
   files_label = gtk_label_new("Files:");
   gtk_box_pack_start(GTK_BOX(files_hbox), files_label, FALSE, FALSE, 4);

   files_combo = gtk_combo_box_text_new();
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(files_combo), "Full");
   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(files_combo), "Short");
   gtk_combo_box_set_active(GTK_COMBO_BOX(files_combo),
                            (app->files_mode == HRMP_FILES_MODE_SHORT) ? 1 : 0);

   gtk_box_pack_start(GTK_BOX(files_hbox), files_combo, FALSE, FALSE, 4);
   gtk_box_pack_start(GTK_BOX(box), files_hbox, FALSE, FALSE, 4);

   g_object_unref(device_store);

   gtk_widget_show_all(dialog);

   if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
   {
      const gchar* text = gtk_entry_get_text(GTK_ENTRY(entry));

      if (text != NULL && text[0] != '\0')
      {
         g_free(app->hrmp_path);
         app->hrmp_path = g_strdup(text);
      }

      if (device_combo != NULL)
      {
         GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(device_combo));
         GtkTreeIter ditem;
         if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(device_combo), &ditem))
         {
            gchar* name = NULL;
            gboolean active = FALSE;
            gtk_tree_model_get(model, &ditem, 0, &name, 1, &active, -1);
            if (name != NULL && active)
            {
               g_free(app->default_device);
               app->default_device = g_strdup(name);
            }
            g_free(name);
         }
      }

      if (files_combo != NULL)
      {
         gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(files_combo));
         app->files_mode = (active == 1) ? HRMP_FILES_MODE_SHORT : HRMP_FILES_MODE_FULL;
         hrmp_gtk_update_playlist_display(app);
      }

      hrmp_gtk_save_preferences(app);
      hrmp_gtk_update_status(app);
   }

   gtk_widget_destroy(dialog);
}

static void
on_mode_button_clicked(GtkWidget* button, gpointer user_data)
{
   struct App* app = user_data;

   app->play_mode = (app->play_mode + 1) % 3;
   update_mode_button_icon(app);
}

static gboolean
on_playlist_button_press(GtkWidget* widget, GdkEventButton* event, gpointer user_data)
{
   struct App* app = user_data;

   if (event->type == GDK_BUTTON_PRESS && event->button == 3)
   {
      GtkTreeView* tree_view = GTK_TREE_VIEW(widget);
      GtkTreePath* path = NULL;
      GtkTreeViewColumn* column = NULL;

      if (gtk_tree_view_get_path_at_pos(tree_view,
                                        (gint)event->x,
                                        (gint)event->y,
                                        &path,
                                        &column,
                                        NULL,
                                        NULL))
      {
         GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
         GtkTreeIter iter;

         if (gtk_tree_model_get_iter(model, &iter, path))
         {
            gchar* filepath = NULL;
            gtk_tree_model_get(model, &iter, 0, &filepath, -1);

            /* Do not remove the row corresponding to the currently playing file: treat the
             * currently selected row as "playing" when hrmp is running. */
            GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);
            GtkTreeIter sel_iter;
            gboolean has_sel = gtk_tree_selection_get_selected(selection, NULL, &sel_iter);
            GtkTreePath* cur_path = NULL;
            if (has_sel)
            {
               cur_path = gtk_tree_model_get_path(model, &sel_iter);
            }

            if (!(app->hrmp_running && has_sel && gtk_tree_path_compare(path, cur_path) == 0))
            {
               gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            }

            if (cur_path != NULL)
            {
               gtk_tree_path_free(cur_path);
            }
            g_free(filepath);
         }

         gtk_tree_path_free(path);
         return TRUE;
      }
   }

   return FALSE;
}

static void
on_playlist_row_activated(GtkTreeView* tree_view,
                          GtkTreePath* path,
                          GtkTreeViewColumn* column,
                          gpointer user_data)
{
   struct App* app = user_data;
   GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
   GtkTreeIter iter;

   if (!gtk_tree_model_get_iter(model, &iter, path))
   {
      return;
   }

   GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);
   gtk_tree_selection_unselect_all(selection);
   gtk_tree_selection_select_iter(selection, &iter);

   if (app->hrmp_running)
   {
      send_key(app, 'q');
      stop_hrmp(app);
   }

   start_hrmp(app);
}

static void
on_window_destroy(GtkWidget* widget, gpointer user_data)
{
   struct App* app = user_data;

   if (app->hrmp_running)
   {
      /* Ask hrmp to quit gracefully */
      send_key(app, 'q');
   }

   stop_hrmp(app);
   gtk_main_quit();
}

static GtkWidget*
create_main_window(struct App* app)
{
   GtkWidget* window;
   GtkWidget* vbox;
   GtkWidget* menubar;
   GtkAccelGroup* accel_group;
   GtkWidget* file_item;
   GtkWidget* file_menu;
   GtkWidget* menu_quit;
   GtkWidget* edit_item;
   GtkWidget* edit_menu;
   GtkWidget* menu_preferences;
   GtkWidget* devices_item;
   GtkWidget* devices_menu;
   GtkWidget* menu_list_devices;
   GtkWidget* help_item;
   GtkWidget* help_menu;
   GtkWidget* menu_about;
   GtkWidget* menu_license;
   GtkWidget* hbox;
   GtkWidget* toolbar_spacer;
   GtkWidget* add_button;
   GtkWidget* clear_button;
   GtkWidget* scrolled_window;
   GtkCellRenderer* renderer;
   GtkTreeViewColumn* column;

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(window), "hrmp-ui");
   gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

   vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
   gtk_container_add(GTK_CONTAINER(window), vbox);

   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

   /* Menu bar */
   menubar = gtk_menu_bar_new();

   file_item = gtk_menu_item_new_with_mnemonic("_File");
   file_menu = gtk_menu_new();
   menu_quit = gtk_menu_item_new_with_mnemonic("_Quit");

   gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), menu_quit);
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
   gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);

   edit_item = gtk_menu_item_new_with_mnemonic("_Edit");
   edit_menu = gtk_menu_new();
   menu_preferences = gtk_menu_item_new_with_mnemonic("_Preferences");
   gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), menu_preferences);
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);
   gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_item);

   devices_item = gtk_menu_item_new_with_mnemonic("_Devices");
   devices_menu = gtk_menu_new();
   menu_list_devices = gtk_menu_item_new_with_mnemonic("_List devices");
   gtk_menu_shell_append(GTK_MENU_SHELL(devices_menu), menu_list_devices);
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(devices_item), devices_menu);
   gtk_menu_shell_append(GTK_MENU_SHELL(menubar), devices_item);

   help_item = gtk_menu_item_new_with_mnemonic("_Help");
   help_menu = gtk_menu_new();
   menu_about = gtk_menu_item_new_with_mnemonic("_About");
   menu_license = gtk_menu_item_new_with_mnemonic("_License");
   GtkWidget* menu_debug = gtk_menu_item_new_with_mnemonic("_Debug");
   gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), menu_about);
   gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), menu_license);
   gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), menu_debug);
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
   gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_item);

   gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

   /* Toolbar */
   hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

   GtkWidget* image_prev = gtk_image_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_play = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_mode_default = gtk_image_new_from_icon_name("media-playlist-consecutive", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_next = gtk_image_new_from_icon_name("media-skip-forward", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_stop = gtk_image_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_volume_down = gtk_image_new_from_icon_name("audio-volume-low", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_volume_up = gtk_image_new_from_icon_name("audio-volume-high", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_skip_back = gtk_image_new_from_icon_name("media-seek-backward", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_skip_ahead = gtk_image_new_from_icon_name("media-seek-forward", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_add = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON);
   GtkWidget* image_clear = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_BUTTON);

   app->prev_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->prev_button), image_prev);
   gtk_button_set_relief(GTK_BUTTON(app->prev_button), GTK_RELIEF_NONE);

   app->skip_back_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->skip_back_button), image_skip_back);
   gtk_button_set_relief(GTK_BUTTON(app->skip_back_button), GTK_RELIEF_NONE);

   app->play_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->play_button), image_play);
   gtk_button_set_relief(GTK_BUTTON(app->play_button), GTK_RELIEF_NONE);

   app->mode_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->mode_button), image_mode_default);
   gtk_button_set_relief(GTK_BUTTON(app->mode_button), GTK_RELIEF_NONE);

   app->skip_ahead_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->skip_ahead_button), image_skip_ahead);
   gtk_button_set_relief(GTK_BUTTON(app->skip_ahead_button), GTK_RELIEF_NONE);

   app->next_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->next_button), image_next);
   gtk_button_set_relief(GTK_BUTTON(app->next_button), GTK_RELIEF_NONE);

   app->stop_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->stop_button), image_stop);
   gtk_button_set_relief(GTK_BUTTON(app->stop_button), GTK_RELIEF_NONE);

   app->volume_down_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->volume_down_button), image_volume_down);
   gtk_button_set_relief(GTK_BUTTON(app->volume_down_button), GTK_RELIEF_NONE);

   app->volume_up_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(app->volume_up_button), image_volume_up);
   gtk_button_set_relief(GTK_BUTTON(app->volume_up_button), GTK_RELIEF_NONE);

   add_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(add_button), image_add);
   gtk_button_set_relief(GTK_BUTTON(add_button), GTK_RELIEF_NONE);

   clear_button = gtk_button_new();
   gtk_button_set_image(GTK_BUTTON(clear_button), image_clear);
   gtk_button_set_relief(GTK_BUTTON(clear_button), GTK_RELIEF_NONE);

   gtk_widget_set_tooltip_text(app->prev_button, "Previous (Ctrl+B)");
   gtk_widget_set_tooltip_text(app->skip_back_button, "Back (Alt+Left)");
   gtk_widget_set_tooltip_text(app->play_button, "Play (Ctrl+P)");
   gtk_widget_set_tooltip_text(app->skip_ahead_button, "Ahead (Alt+Right)");
   gtk_widget_set_tooltip_text(app->next_button, "Next (Ctrl+N)");
   gtk_widget_set_tooltip_text(app->stop_button, "Stop (Ctrl+S)");
   gtk_widget_set_tooltip_text(app->volume_down_button, "Volume down (Ctrl+-)");
   gtk_widget_set_tooltip_text(app->volume_up_button, "Volume up (Ctrl++)");
   gtk_widget_set_tooltip_text(app->mode_button, "Mode (Ctrl+M)");
   gtk_widget_set_tooltip_text(add_button, "Add (Ctrl+A)");
   gtk_widget_set_tooltip_text(clear_button, "Clear (Ctrl+L)");

   /* Toolbar mnemonics (accelerators) */
   gtk_widget_add_accelerator(app->prev_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_B,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->skip_back_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_Left,
                              GDK_MOD1_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->play_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_P,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->skip_ahead_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_Right,
                              GDK_MOD1_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->next_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_N,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->stop_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_S,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->volume_down_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_minus,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->volume_up_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_plus,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(app->mode_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_M,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(add_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_A,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
   gtk_widget_add_accelerator(clear_button,
                              "clicked",
                              accel_group,
                              GDK_KEY_L,
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

   gtk_box_pack_start(GTK_BOX(hbox), app->prev_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->skip_back_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->play_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->skip_ahead_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->next_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->stop_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->volume_down_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->volume_up_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), app->mode_button, FALSE, FALSE, 2);

   toolbar_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
   gtk_widget_set_hexpand(toolbar_spacer, TRUE);
   gtk_box_pack_start(GTK_BOX(hbox), toolbar_spacer, TRUE, TRUE, 0);

   gtk_box_pack_start(GTK_BOX(hbox), add_button, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), clear_button, FALSE, FALSE, 2);

   /* Playlist */
   app->list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
   app->list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->list_store));

   renderer = gtk_cell_renderer_text_new();
   column = gtk_tree_view_column_new_with_attributes("Files",
                                                     renderer,
                                                     "text",
                                                     1,
                                                     NULL);
   gtk_tree_view_append_column(GTK_TREE_VIEW(app->list_view), column);

   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
   gtk_container_add(GTK_CONTAINER(scrolled_window), app->list_view);

   gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 4);

   /* Output view (shown in Debug window) */
   app->output_view = gtk_text_view_new();
   gtk_text_view_set_editable(GTK_TEXT_VIEW(app->output_view), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->output_view), FALSE);
   app->output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->output_view));

   /* Status panel */
   GtkWidget* status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
   gtk_box_pack_start(GTK_BOX(vbox), status_box, FALSE, FALSE, 2);

   app->song_label = gtk_label_new("");
   gtk_widget_set_halign(app->song_label, GTK_ALIGN_START);
   gtk_widget_set_hexpand(app->song_label, TRUE);
   gtk_label_set_xalign(GTK_LABEL(app->song_label), 0.0);
   gtk_box_pack_start(GTK_BOX(status_box), app->song_label, TRUE, TRUE, 4);

   app->status_label = gtk_label_new("");
   gtk_widget_set_halign(app->status_label, GTK_ALIGN_END);
   gtk_widget_set_hexpand(app->status_label, TRUE);
   gtk_label_set_xalign(GTK_LABEL(app->status_label), 1.0);
   gtk_box_pack_start(GTK_BOX(status_box), app->status_label, TRUE, TRUE, 4);

   /* Signals */
   g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), app);

   g_signal_connect(menu_quit, "activate", G_CALLBACK(on_window_destroy), app);
   g_signal_connect(menu_preferences, "activate", G_CALLBACK(on_menu_preferences), app);
   g_signal_connect(menu_list_devices, "activate", G_CALLBACK(on_menu_list_devices), app);
   g_signal_connect(menu_about, "activate", G_CALLBACK(on_menu_about), app);
   g_signal_connect(menu_license, "activate", G_CALLBACK(on_menu_license), app);
   g_signal_connect(menu_debug, "activate", G_CALLBACK(on_menu_debug), app);

   g_signal_connect(add_button, "clicked", G_CALLBACK(on_button_add_clicked), app);
   g_signal_connect(clear_button, "clicked", G_CALLBACK(on_button_clear_clicked), app);

   g_signal_connect(app->prev_button, "clicked", G_CALLBACK(on_button_prev_clicked), app);
   g_signal_connect(app->skip_back_button, "clicked", G_CALLBACK(on_button_skip_back_clicked), app);
   g_signal_connect(app->play_button, "clicked", G_CALLBACK(on_button_play_clicked), app);
   g_signal_connect(app->skip_ahead_button, "clicked", G_CALLBACK(on_button_skip_ahead_clicked), app);
   g_signal_connect(app->next_button, "clicked", G_CALLBACK(on_button_next_clicked), app);
   g_signal_connect(app->stop_button, "clicked", G_CALLBACK(on_button_stop_clicked), app);
   g_signal_connect(app->volume_down_button, "clicked", G_CALLBACK(on_button_volume_down_clicked), app);
   g_signal_connect(app->volume_up_button, "clicked", G_CALLBACK(on_button_volume_up_clicked), app);
   g_signal_connect(app->mode_button, "clicked", G_CALLBACK(on_mode_button_clicked), app);

   return window;
}

int
main(int argc, char** argv)
{
   struct App app;

   memset(&app, 0, sizeof(app));

   gtk_init(&argc, &argv);

   hrmp_gtk_load_preferences(&app);
   hrmp_gtk_ensure_devices(&app);

   app.window = create_main_window(&app);
   app.output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.output_view));
   hrmp_gtk_update_status(&app);
   update_mode_button_icon(&app);

   /* If the default device is still reported as "Unknown" after the first
    * query, wait a short time and try once more. This handles setups where
    * the first hrmp -s call races with device initialisation. */
   const gchar* status_text = gtk_label_get_text(GTK_LABEL(app.status_label));
   if (status_text != NULL && g_strcmp0(status_text, "Unknown") == 0)
   {
      g_usleep(500 * 1000);
      hrmp_gtk_ensure_devices(&app);
      hrmp_gtk_update_status(&app);
   }

   g_signal_connect(app.list_view,
                    "row-activated",
                    G_CALLBACK(on_playlist_row_activated),
                    &app);
   g_signal_connect(app.list_view,
                    "button-press-event",
                    G_CALLBACK(on_playlist_button_press),
                    &app);

   gtk_widget_show_all(app.window);

   gtk_main();

   stop_hrmp(&app);

   g_free(app.hrmp_path);
   g_free(app.default_device);
   hrmp_gtk_free_devices(&app);

   return 0;
}
