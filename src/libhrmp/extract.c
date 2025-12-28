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
#include <extract.h>
#include <logging.h>
#include <redbook.h>
#include <scarletbook.h>
#include <utils.h>

int
hrmp_extract(char* d)
{
   if (hrmp_starts_with(d, "/dev/"))
   {
      return hrmp_extract_redbook(d);
   }
   else if (hrmp_ends_with(d, ".iso"))
   {
      /* TODO: Redbook / Scarletbook */
      return hrmp_extract_scarletbook(d);
   }
   else
   {
      printf("Unsupported file: %s\n", d);
   }

   return 1;
}
