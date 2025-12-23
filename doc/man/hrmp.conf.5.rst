===============
hrmp.conf
===============

--------------------------------------
Main configuration file for hrmp
--------------------------------------

:Manual section: 5

DESCRIPTION
===========

hrmp.conf is the main configuration file for hrmp.

The file is split into different sections specified by the ``[`` and ``]`` characters. The main section is called ``[hrmp]``.

Other sections specifies the device configurations.

All properties are in the format ``key = value``.

The characters ``#`` and ``;`` can be used for comments; must be the first character on the line.
The ``Bool`` data type supports the following values: ``on``, ``1``, ``true``, ``off``, ``0`` and ``false``.

OPTIONS
=======

device
  The default device

output
   Defines the console output. Valid expansions are: %n (current track number), %N (total number of tracks), %d (device name), %f (file name), %F (full path of file), %i (file information), %t (current time), %T (total time), %p (percentage), %b (ringbuffer current size in Mb), %B (ringbuffer maximum size in Mb). Default is [%n/%N] %d: %f [%i] (%t/%T) (%p)

volume
  The volume in percent. -1 means use current volume

log_type
  The logging type (console, file, syslog). Default is console

log_level
  The logging level, any of the (case insensitive) strings FATAL, ERROR, WARN, INFO and DEBUG
  (that can be more specific as DEBUG1 thru DEBUG5). Debug level greater than 5 will be set to DEBUG5.
  Not recognized values will make the log_level be INFO. Default is info

log_path
  The log file location. Default is hrmp.log. Can be a strftime(3) compatible string

log_line_prefix
  A strftime(3) compatible string to use as prefix for every log line. Must be quoted if contains spaces.
  Default is %Y-%m-%d %H:%M:%S

log_mode
  Append to or create the log file (append, create). Default is append

update_process_title
  The behavior for updating the operating system process title. Allowed settings are: never (or off),
  does not update the process title; strict to set the process title without overriding the existing
  initial process title length; minimal to set the process title to the base description; verbose (or full)
  to set the process title to the full description. Please note that strict and minimal are honored
  only on those systems that do not provide a native way to set the process title (e.g., Linux).
  On other systems, there is no difference between strict and minimal and the assumed behaviour is minimal
  even if strict is used. never and verbose are always honored, on every system. On Linux systems the
  process title is always trimmed to 255 characters, while on system that provide a natve way to set the
  process title it can be longer. Default is verbose

The options for the device sections are

device
  The device address

description
  The description of the device

volume
  The volume in percent. -1 means use current volume

REPORTING BUGS
==============

hrmp is maintained on GitHub at https://github.com/HighResMusicPlayer/hrmp

COPYRIGHT
=========

hrmp is licensed under the GNU General Public License version 3.

SEE ALSO
========

hrmp(1)
