# HighResMusicPlayer

`hrmp` is a command line music player for Linux (ALSA) based systems. `hrmp` focuses on
high-resolution loss-less files (44.1kHz+/16bit+).

`hrmp` requires an external DAC as it only supports 16bit, 24bit, 32bit and DSD files.

## Features

* FLAC (44.1kHz/16bit) (Stereo)
* By-pass PulseAudio for native bit-stream

See [Getting Started](./doc/GETTING_STARTED.md) on how to get started with `hrmp`.

See [Configuration](./doc/CONFIGURATION.md) on how to configure `hrmp`.

## Overview

`hrmp` makes use of

* [ALSA](https://www.alsa-project.org/wiki/Main_Page)
* [libflac](https://xiph.org/flac/)

## Tested platforms

* [Fedora](https://getfedora.org/) 42+

## Compiling the source

`hrmp` requires

* [clang](https://clang.llvm.org/)
* [cmake](https://cmake.org)
* [make](https://www.gnu.org/software/make/)
* [ALSA](https://www.alsa-project.org/wiki/Main_Page)
* [libflac](https://xiph.org/flac/)
* [rst2man](https://docutils.sourceforge.io/)
* [pandoc](https://pandoc.org/)
* [texlive](https://www.tug.org/texlive/)

```sh
dnf install git gcc clang clang-analyzer cmake make python3-docutils libasan libasan-static alsa-lib alsa-lib-devel flac-libs flac-libs-devel
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

[BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)
