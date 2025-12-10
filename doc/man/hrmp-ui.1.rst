=======
hrmp-ui
=======

-----------------------------
GTK UI for HighResMusicPlayer
-----------------------------

:Manual section: 1

SYNOPSIS
========

hrmp-ui

DESCRIPTION
===========

hrmp-ui is a GTK-based graphical user interface for hrmp, the HighResMusicPlayer.
It provides a toolbar with playback, skip, stop, volume, and play mode controls,
along with a playlist view and an output log window.

The application launches the hrmp command-line player in the background and
controls it via the keyboard commands that hrmp understands.

CONTROLS
========

Toolbar buttons
---------------

Prev
  Skip to the previous track in the playlist.

Skip back
  Skip backward in the current track (mapped to ARROW_DOWN in hrmp).

Play / Pause
  Start playback of the current playlist or toggle pause/resume.

Skip ahead
  Skip forward in the current track (mapped to ARROW_UP in hrmp).

Next
  Skip to the next track in the playlist.

Stop
  Stop playback and clear hrmp's internal queue.

Volume down
  Decrease playback volume.

Volume up
  Increase playback volume.

Play mode
  Toggle between once, repeat, and shuffle playback modes.

Menu items
----------

File → Add
  Add audio files to the playlist.

File → Clear
  Clear the playlist and stop any running hrmp instance.

File → Quit
  Quit hrmp-ui.

Edit → Preferences
  Configure the hrmp binary path, default playback device, and playlist display mode.

Devices → List devices
  Show devices detected by hrmp, including per-format support.

Help → About hrmp-ui
  Display version and authorship information.

Help → License
  Display the full text of the GNU General Public License version 3.

FILES
=====

~/.hrmp/hrmp-ui.conf
  Configuration file storing the hrmp binary path, default device, and file display mode.

REPORTING BUGS
==============

hrmp-ui is maintained on GitHub as part of the HighResMusicPlayer project at
https://github.com/HighResMusicPlayer/hrmp/.

COPYRIGHT
=========

hrmp-ui is licensed under the GNU General Public License version 3.

SEE ALSO
========

hrmp(1), hrmp.conf(5)
