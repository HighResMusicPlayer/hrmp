\newpage

# Installation

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
dnf install git gcc clang clang-analyzer cmake make \
    python3-docutils libasan libasan-static \
    alsa-lib alsa-lib-devel \
    libsndfile libsndfile-devel \
    opus opus-devel \
    faad2 faad2-devel
```

### Generate the guide

This process is optional. If you choose not to generate the PDF and HTML files by `-DDOCS=FALSE`, you can opt out of downloading these dependencies, and the process will automatically skip the generation.

1. Download dependencies

    ``` sh
dnf install pandoc texlive-scheme-basic
    ```

2. Download Eisvogel

    Use the command `pandoc --version` to locate the user data directory. On Fedora systems, this directory is typically located at `$HOME/.local/share/pandoc`.

    Download the `Eisvogel` template for `pandoc`, please visit the [pandoc-latex-template](https://github.com/Wandmalfarbe/pandoc-latex-template) repository. For a standard installation, you can follow the steps outlined below.

```sh
wget https://github.com/Wandmalfarbe/pandoc-latex-template/releases/download/v3.2.1/Eisvogel-3.2.1.tar.gz
tar -xzf Eisvogel-3.2.1.tar.gz
mkdir -p $HOME/.local/share/pandoc/templates
mv Eisvogel-3.2.1/eisvogel.latex $HOME/.local/share/pandoc/templates/
```

3. Add package for LaTeX

    Download the additional packages required for generating PDF and HTML files.

```sh
dnf install 'tex(footnote.sty)' 'tex(footnotebackref.sty)' 'tex(pagecolor.sty)' 'tex(hardwrap.sty)' 'tex(mdframed.sty)' 'tex(sourcesanspro.sty)' 'tex(ly1enc.def)' 'tex(sourcecodepro.sty)' 'tex(titling.sty)' 'tex(csquotes.sty)' 'tex(zref-abspage.sty)' 'tex(needspace.sty)' 'tex(selnolig.sty)'
```

**Generate API guide**

This process is optional. If you choose not to generate the API HTML files, you can opt out of downloading these dependencies, and the process will automatically skip the generation.

Download dependencies

```sh
dnf install graphviz doxygen
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
