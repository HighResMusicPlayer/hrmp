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

#ifndef HRMP_PLAYLIST_H
#define HRMP_PLAYLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <list.h>

#include <stdbool.h>

/**
 * Load a playlist file (.hrmp) into an existing files list.
 *
 * Each non-empty line can be:
 *  - Relative file/directory path (relative to the playlist file directory)
 *  - Absolute file/directory path
 *  - "*" to add all files in the playlist file directory
 *  - recursive glob ("**" + "/" + "*")
 *
 * @param playlist_path Path to playlist file
 * @param files Target list to append files to
 * @param quiet If true, suppress warnings about missing files
 * @return 0 on success, otherwise 1
 */
int
hrmp_playlist_load(const char* playlist_path, struct list* files, bool quiet);

#ifdef __cplusplus
}
#endif

#endif
