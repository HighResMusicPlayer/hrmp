# HighResMusicPlayer

`hrmp` is a command line music player for Linux (ALSA) based systems. `hrmp` focuses on
high-resolution loss-less files (44.1kHz+/16bit+).

`hrmp` requires an external DAC as it only supports 16bit, 24bit, 32bit and DSD files.

## Features

* Digital Stream Digital (DSD) 64/128/256/512 (Sony .dsf / Philips .dff)
* Digital Stream Digital (DSD) over Pulse Code Modulation (PCM) (DoP) 64/128/256 (Sony .dsf / Philips .dff)
* FLAC (44.1kHz/16bit, 48kHz/16bit, 88.2kHz/16bit, 96kHz/16bit, 176.4kHz/16bit, 192kHz/16bit, 352.8kHz/16bit, 384kHz/16bit, 44.1kHz/24bit, 48kHz/24bit, 88.2kHz/24bit, 96kHz/24bit, 176.4kHz/24bit, 192kHz/24bit, 352.8kHz/24bit, 384kHz/24bit)
* WAV (44.1kHz/16bit, 48kHz/16bit, 88.2kHz/16bit, 96kHz/16bit, 176.4kHz/16bit, 192kHz/16bit, 352.8kHz/16bit, 384kHz/16bit, 44.1kHz/24bit, 48kHz/24bit, 88.2kHz/24bit, 96kHz/24bit, 176.4kHz/24bit, 192kHz/24bit, 352.8kHz/24bit, 384kHz/24bit)
* By-pass PulseAudio for native bit-stream

See [Getting Started](./doc/GETTING_STARTED.md) on how to get started with `hrmp`.

See [Configuration](./doc/CONFIGURATION.md) on how to configure `hrmp`.

### Best effort

* MP3
* MKV
* WEBM

## Overview

`hrmp` makes use of

* [ALSA](https://www.alsa-project.org/wiki/Main_Page)
* [libsndfile](https://libsndfile.github.io/libsndfile/)
* [opus](https://github.com/xiph/opus)
* [faad2](https://github.com/knik0/faad2)

## Tested platforms

* [Fedora](https://getfedora.org/) 42+

## Compiling the source

`hrmp` requires

* [clang](https://clang.llvm.org/)
* [cmake](https://cmake.org)
* [make](https://www.gnu.org/software/make/)
* [ALSA](https://www.alsa-project.org/wiki/Main_Page)
* [libsndfile](https://libsndfile.github.io/libsndfile/)
* [opus](https://github.com/xiph/opus)
* [faad2](https://github.com/knik0/faad2)
* [rst2man](https://docutils.sourceforge.io/)
* [pandoc](https://pandoc.org/)
* [texlive](https://www.tug.org/texlive/)

```sh
dnf install git gcc clang clang-analyzer cmake make python3-docutils libasan libasan-static alsa-lib alsa-lib-devel libsndfile libsndfile-devel opus opus-devel faad2-libs faad2-devel
```

### Release build

The following commands will install `hrmp` in the `/usr/local` hierarchy.

```sh
git clone https://github.com/HighResMusicPlayer/hrmp.git
cd hrmp
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/usr/local ..
make
sudo make install
```

### Debug build

The following commands will create a `DEBUG` version of `hrmp`.

```sh
git clone https://github.com/HighResMusicPlayer/hrmp.git
cd hrmp
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug ..
make
```

## Contributing

Contributions to `hrmp` are managed on [GitHub.com](https://github.com/HighResMusicPlayer/hrmp)

* [Ask a question](https://github.com/HighResMusicPlayer/hrmp/discussions)
* [Raise an issue](https://github.com/HighResMusicPlayer/hrmp/issues)
* [Feature request](https://github.com/HighResMusicPlayer/hrmp/issues)
* [Code submission](https://github.com/HighResMusicPlayer/hrmp/pulls)

Contributions are most welcome !

Please, consult our [Code of Conduct](./CODE_OF_CONDUCT.md) policies for interacting in our
community.

Consider giving the project a [star](https://github.com/HighResMusicPlayer/hrmp/stargazers) on
[GitHub](https://github.com/HighResMusicPlayer/hrmp/) if you find it useful.

## License

[GPL v3](https://www.gnu.org/licenses/gpl-3.0.en.html)
