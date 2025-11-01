# hrmp

**hrmp** is a high resolution music player.

```
hrmp 0.8.0
  High resolution music player

Usage:
  hrmp <FILES>

Options:
  -c, --config CONFIG_FILE   Set the path to the hrmp.conf file
                             Default: $HOME/.hrmp/hrmp.conf
  -D, --device               Set the device name
  -R, --recursive            Add files recursive of the directory
  -I, --sample-configuration Generate a sample configuration
  -s, --status               Status of the devices
      --dop                  Use DSD over PCM
  -q, --quiet                Quiet the player
  -V, --version              Display version information
  -?, --help                 Display help

hrmp: https://hrmp.github.io/
Report bugs: https://github.com/HighResMusicPlayer/hrmp/issues
```

## -I

Create a sample configuration based on the connected DACs


```sh
hrmp -I
```

## -D

Select a non-default device for output

```sh
hrmp -D "MyDAC" .
```

## -R

Play supported music files, and recurse through directories

```sh
hrmp -R .
```

## Keyboard shortcuts

| Key             | Description          |
| :-------------- | :------------------- |
| `<SPACEBAR>`    | Pause current file   |
| `q`             | Quit                 |
| `<ENTER>`       | Quit current file    |
| `<ARROW_UP>`    | Forward 1 minute     |
| `<ARROW_DOWN>`  | Rewind 1 minute      |
| `<ARROW_LEFT>`  | Forward 15 seconds   |
| `<ARROW_RIGHT>` | Rewind 15 seconds    |
| `,`             | Reduce volume by 5   |
| `.`             | Increase volume by 5 |
| `m`             | Mute / unmute        |
| `/`             | Volume at 100        |
