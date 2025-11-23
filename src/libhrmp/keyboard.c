/*
 * Copyright (C) 2025 The HighResMusicPlayer community
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
         default:
            break;
      }
   }

   *keyboard_code = k;

   return ret;
}
