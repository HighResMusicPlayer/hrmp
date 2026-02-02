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

#include <hrmp.h>
#include <keyboard.h>
#include <utils.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>


int
hrmp_keyboard_mode(bool enable)
{
   int flags;
   struct termios term;

   tcgetattr(STDIN_FILENO, &term);
   flags = fcntl(STDIN_FILENO, F_GETFL, 0);

   if (enable)
   {
      term.c_lflag &= ~(ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &term);

      fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
   }
   else
   {
      term.c_lflag |= (ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &term);

      fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
   }

   return 0;
}

int
hrmp_keyboard_get(char** keyboard_code)
{
   int ret = KEYBOARD_IGNORE;
   int c = 0;
   char* k = NULL;
   struct configuration* config = NULL;

   config = (struct configuration*)shmem;

   c = getchar();

   if (c >= 0)
   {
      if (config->developer)
      {
         k = hrmp_append(k, "Keyboard: ");
         k = hrmp_append_int(k, c);
      }

      switch (c)
      {
         case 10:
            ret = KEYBOARD_ENTER;
            break;
         case 32:
            ret = KEYBOARD_SPACE;
            break;
         case 68:
            ret = KEYBOARD_LEFT;
            break;
         case 65:
            ret = KEYBOARD_UP;
            break;
         case 67:
            ret = KEYBOARD_RIGHT;
            break;
         case 66:
            ret = KEYBOARD_DOWN;
            break;
         case 113:
            ret = KEYBOARD_Q;
            break;
         case 109:
            ret = KEYBOARD_M;
            break;
         case 44:
            ret = KEYBOARD_COMMA;
            break;
         case 46:
            ret = KEYBOARD_PERIOD;
            break;
         case 47:
            ret = KEYBOARD_SLASH;
            break;
         case 92:
            ret = KEYBOARD_BACKSLASH;
            break;
         default:
            break;
      }
   }

   *keyboard_code = k;

   return ret;
}
