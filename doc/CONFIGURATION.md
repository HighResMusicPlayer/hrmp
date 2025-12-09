# hrmp configuration

The configuration is loaded from either the path specified by the `-c` flag or `~/.hrmp/hrmp.conf`.

The configuration of `hrmp` is split into sections using the `[` and `]` characters.

The main section, called `[hrmp]`, is where you configure the overall properties
of `hrmp`.

Other sections doesn't have any requirements to their naming so you can give them
meaningful names like `[FIIO K19]` for the FiiO K19 device..

All properties are in the format `key = value`.

The characters `#` and `;` can be used for comments; must be the first character on the line.
The `Bool` data type supports the following values: `on`, `1`, `true`, `off`, `0` and `false`.

## [hrmp]

| Property | Default | Unit | Required | Description |
|----------|---------|------|----------|-------------|
| device   | | String | No | The default device name |
| output | `[%n/%N] %d: %f [%i] (%t/%T) (%p)`| String | No | Defines the console output. Valid expansions are: `%n` (current track number), `%N` (total number of tracks), `%d` (device name), `%f` (file name), `%F` (full path of file), `%i` (file information), `%t` (current time), `%T` (total time), `%p` (percentage)|
| volume   | -1 | Int | No | The volume in percent. -1 means use current volume |
| log_type | console | String | No | The logging type (console, file, syslog) |
| log_level | info | String | No | The logging level, any of the (case insensitive) strings `FATAL`, `ERROR`, `WARN`, `INFO` and `DEBUG` (that can be more specific as `DEBUG1` thru `DEBUG5`). Debug level greater than 5 will be set to `DEBUG5`. Not recognized values will make the log_level be `INFO` |
| log_path | hrmp.log | String | No | The log file location. Can be a strftime(3) compatible string. |
| log_line_prefix | %Y-%m-%d %H:%M:%S | String | No | A strftime(3) compatible string to use as prefix for every log line. Must be quoted if contains spaces. |
| log_mode | append | String | No | Append to or create the log file (append, create) |
| update_process_title | `verbose` | String | No | The behavior for updating the operating system process title. Allowed settings are: `never` (or `off`), does not update the process title; `strict` to set the process title without overriding the existing initial process title length; `minimal` to set the process title to the base description; `verbose` (or `full`) to set the process title to the full description. Please note that `strict` and `minimal` are honored only on those systems that do not provide a native way to set the process title (e.g., Linux). On other systems, there is no difference between `strict` and `minimal` and the assumed behaviour is `minimal` even if `strict` is used. `never` and `verbose` are always honored, on every system. On Linux systems the process title is always trimmed to 255 characters, while on system that provide a natve way to set the process title it can be longer. |

## Device section

| Property | Default | Unit | Required | Description |
|----------|---------|------|----------|-------------|
| device | | String | Yes | The device address |
| description | | String | No | The description of the device |
| volume   | -1 | Int | No | The volume in percent. -1 means use current volume |
