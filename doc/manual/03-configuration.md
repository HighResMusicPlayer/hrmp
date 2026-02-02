\newpage

# Configuration

## hrmp.conf

The configuration is loaded from either the path specified by the `-c` flag or `~/.hrmp/hrmp.conf`.

The configuration of `hrmp` is split into sections using the `[` and `]` characters.

The main section, called `[hrmp]`, is where you configure the overall properties
of `hrmp`.

Other sections doesn't have any requirements to their naming so you can give them
meaningful names like `[MySoundDevice]` for your main external DAC.

All properties are in the format `key = value`.

The characters `#` and `;` can be used for comments; must be the first character on the line.
The `Bool` data type supports the following values: `on`, `yes`, `1`, `true`, `off`, `no`, `0` and `false`.

### hrmp

| Property | Default | Unit | Required | Description |
|----------|---------|------|----------|-------------|
| device   | | String | No | The default device name |
| output | `[%n/%N] %d: %f [%i] (%t/%T) (%p)`| String | No | Defines the console output. Valid expansions are: `%n` (current track number), `%N` (total number of tracks), `%d` (device name), `%f` (file name), `%F` (full path of file), `%i` (file information), `%t` (current time), `%T` (total time), `%p` (percentage), `%b` (ringbuffer current size in Mb), `%B` (ringbuffer maximum size in Mb)|
| volume   | -1 | Int | No | The volume in percent. -1 means use current volume |
| cache   | 256Mb | Int | No | The cache size. `0` means no caching |
| cache_files | `off` | String | No | File caching policy: `off` only caches the current file, `minimal` caches the previous and next files as well, and `all` caches all files in the playlist |
| log_type | console | String | No | The logging type (console, file, syslog) |
| log_level | info | String | No | The logging level, any of the (case insensitive) strings `FATAL`, `ERROR`, `WARN`, `INFO` and `DEBUG` (that can be more specific as `DEBUG1` thru `DEBUG5`). Debug level greater than 5 will be set to `DEBUG5`. Not recognized values will make the log_level be `INFO` |
| log_path | hrmp.log | String | No | The log file location. Can be a strftime(3) compatible string. |
| log_line_prefix | %Y-%m-%d %H:%M:%S | String | No | A strftime(3) compatible string to use as prefix for every log line. Must be quoted if contains spaces. |
| log_mode | append | String | No | Append to or create the log file (append, create) |
| update_process_title | `verbose` | String | No | The behavior for updating the operating system process title. Allowed settings are: `never` (or `off`), does not update the process title; `strict` to set the process title without overriding the existing initial process title length; `minimal` to set the process title to the base description; `verbose` (or `full`) to set the process title to the full description. Please note that `strict` and `minimal` are honored only on those systems that do not provide a native way to set the process title (e.g., Linux). On other systems, there is no difference between `strict` and `minimal` and the assumed behaviour is `minimal` even if `strict` is used. `never` and `verbose` are always honored, on every system. On Linux systems the process title is always trimmed to 255 characters, while on system that provide a natve way to set the process title it can be longer. |


### Device section

| Property | Default | Unit | Required | Description |
|----------|---------|------|----------|-------------|
| device | | String | Yes | The device address |
| description | | String | No | The description of the device |
| volume   | -1 | Int | No | The volume in percent. -1 means use current volume |

## Console output

The default console output format is

```
[%n/%N] %d: %f [%i] (%t/%T) (%p)
```

The expansions supported are

| Expansion | Description |
| :-------- | :---------- |
| `%n`      | Current track number |
| `%N`      | Total number of tracks |
| `%d`      | Device name |
| `%f`      | File name |
| `%F`      | Full path of the file |
| `%i`      | File information |
| `%t`      | Current time |
| `%T`      | Total time |
| `%p`      | Percentage |

The console also supports color using the ANSI color codes

| Code      | Description |
| :-------- | :---------- |
| `\e[0m`   | No Color |
| `\e[0;30m`| Black |
| `\e[1;30m`| Gray |
| `\e[0;31m`| Red |
| `\e[1;31m`| Light red |
| `\e[0;32m`| Green |
| `\e[1;32m`| Light green |
| `\e[0;33m`| Brown |
| `\e[1;33m`| Yellow |
| `\e[0;34m`| Blue |
| `\e[1;34m`| Light blue |
| `\e[0;35m`| Purple |
| `\e[1;35m`| Light purple |
| `\e[0;36m`| Cyan |
| `\e[1;36m`| Light cyan |
| `\e[0;37m`| Light gray |
| `\e[1;37m`| White |

An example

```
output=[\e[1;32m%t\e[0m/\e[0;32m%T\e[0m] \e[1;37m%f\e[0m
```
