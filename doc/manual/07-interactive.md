\newpage

# hrmp interactive

This chapter describes **hrmp** in interactive mode (`-i`)

## Overview

hrmp interactive provides:

- A ncurses based text based UI to help you build your playlist.

## Controls

The screen is split into two panels:

- **Left (Disk)**: browse directories and add files to the playlist.
- **Right (Playlist)**: review the current playlist, remove entries, and choose where to start playback.

### Left panel: Disk

- **Up/Down**: Move selection
- **PageUp/PageDown**: Scroll by a page
- **Enter**:
  - On a directory: enter it (".." goes to parent)
  - On a file: add the file to the playlist
- **Backspace**: Go to parent directory
- **+**: Add the selected file to the playlist
- **\***: Add all files in the current directory to the playlist
- **-**: Remove the last entry from the playlist
- **Left/Right**: Switch panels (switching to Playlist only works when the playlist is non-empty)

### Right panel: Playlist

- **Up/Down**: Move selection
- **PageUp/PageDown**: Scroll by a page
- **Enter**: Exit the UI and start playback from the selected playlist entry
- **-**: Remove the selected playlist entry
- **Left/Right**: Switch panels

### Global (works in both panels)

- **l**: Load `playlist.hrmp` from the current working directory (if the file does not exist: no-op)
- **s**: Save the current playlist to `playlist.hrmp` in the current working directory (if the playlist is empty: no-op)
- **p**: Exit the UI and start playback
  - If the playlist panel is active: start from the selected playlist entry
  - Otherwise: start from the beginning of the playlist
- **q**: Quit (exit the UI without playing)

