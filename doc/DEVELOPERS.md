# Developer guide

## Install hrmp

### Pre-install

#### Basic dependencies

``` sh
dnf install git gcc clang clang-analyzer cmake make python3-docutils libasan libasan-static alsa-lib alsa-lib-devel flac-libs flac-libs-devel
```

#### Generate user and developer guide

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

#### Generate API guide

This process is optional. If you choose not to generate the API HTML files, you can opt out of downloading these dependencies, and the process will automatically skip the generation.

Download dependencies

```sh
    dnf install graphviz doxygen
```

### Build

``` sh
cd /usr/local
git clone https://github.com/HighResMusicPlayer/hrmp.git
cd hrmp
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug ..
make
make install
```

This will install [**hrmp**](https://github.com/HighResMusicPlayer/hrmp) in the `/usr/local` hierarchy with the debug profile.

You can do

```
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-DCORE_DEBUG" ..
```

in order to get information from the core libraries too.

### Check version

You can navigate to `build/src` and execute `./hrmp -?` to make the call. Alternatively, you can install it into `/usr/local/` and call it directly using:

``` sh
hrmp -?
```

If you see an error saying `error while loading shared libraries: libhrmp.so.0: cannot open shared object` running the above command. you may need to locate where your `libhrmp.so.0` is. It could be in `/usr/local/lib` or `/usr/local/lib64` depending on your environment. Add the corresponding directory into `/etc/ld.so.conf`.

To enable these directories, you would typically add the following lines in your `/etc/ld.so.conf` file:

``` sh
/usr/local/lib
/usr/local/lib64
```

Remember to run `ldconfig` to make the change effective.

## Setup hrmp

We can generate an initial setup by running

``` sh
hrmp -I
```

and then save that to `~/.hrmp/hrmp.conf` as the default configuration.

## End

Now that we have `hrmp` installed, we can continue to enhance its functionality.

## Audio formats

### FLAC

* [Specification](https://xiph.org/flac/format.html) (Max 655350Hz)

### WAV

* [Header](http://www.ringthis.com/dev/wave_format.htm) (Max 4.3GHz)

### Direct Stream Digital (DSD)

* [Introduction](https://dsd-guide.com/dsd-new-addiction-andreas-koch)

* DSD64 (2.8224 MHz)
* DSD128 (5.6448 MHz)
* DSD256 (11.2896 MHz)
* DSD512 (22.5792 MHz)
* DSD1024 (45.1584 MHz)

* [Sony's DSF file format specification](https://dsd-guide.com/sonys-dsf-file-format-spec)
* [Sony's DSF disc format specification](https://dsd-guide.com/sonys-dsd-disc-format-specs)

Digital Stream Digital (DSD) over Pulse Code Modulation (PCM) (DoP)

* DSD 64 requires a 176400Hz PCM package
* DSD 128 requires a 352800Hz PCM package
* DSD 256 requires a 705600Hz PCM package (WAV only)

* [DSD over PCM specification](https://dsd-guide.com/dop-open-standard)

Direct Stream Digital (DSD) Interchange File Format (IFF)

* [Philips' DSDIFF specification](https://dsd-guide.com/philips-dsdiff-spec-15)

## C programming

[**hrmp**](https://github.com/HighResMusicPlayer/hrmp) is developed using the [C programming language](https://en.wikipedia.org/wiki/C_(programming_language)) so it is a good
idea to have some knowledge about the language before you begin to make changes.

There are books like,

* [C in a Nutshell](https://www.oreilly.com/library/view/c-in-a/9781491924174/)
* [21st Century C](https://www.oreilly.com/library/view/21st-century-c/9781491904428/)

that can help you

### Debugging

In order to debug problems in your code you can use [gdb](https://www.sourceware.org/gdb/), or add extra logging using
the `pgmoneta_log_XYZ()` API

## Basic git guide

Here are some links that will help you

* [How to Squash Commits in Git](https://www.git-tower.com/learn/git/faq/git-squash)
* [ProGit book](https://github.com/progit/progit2/releases)

### Start by forking the repository

This is done by the "Fork" button on GitHub.

### Clone your repository locally

This is done by

```sh
git clone git@github.com:<username>/hrmp.git
```

### Add upstream

Do

```sh
cd hrmp
git remote add upstream https://github.com/HighResMusicPlayer/hrmp.git
```

### Do a work branch

```sh
git checkout -b mywork main
```

### Make the changes

Remember to verify the compile and execution of the code

### AUTHORS

Remember to add your name to the following files,

```
AUTHORS
doc/manual/97-acknowledgement.md
```

in your first pull request

### Multiple commits

If you have multiple commits on your branch then squash them

``` sh
git rebase -i HEAD~2
```

for example. It is `p` for the first one, then `s` for the rest

### Rebase

Always rebase

``` sh
git fetch upstream
git rebase -i upstream/main
```

### Force push

When you are done with your changes force push your branch

``` sh
git push -f origin mywork
```

and then create a pull requests for it

### Repeat

Based on feedback keep making changes, squashing, rebasing and force pushing

### Undo

Normally you can reset to an earlier commit using `git reset <commit hash> --hard`.
But if you accidentally squashed two or more commits, and you want to undo that,
you need to know where to reset to, and the commit seems to have lost after you rebased.

But they are not actually lost - using `git reflog`, you can find every commit the HEAD pointer
has ever pointed to. Find the commit you want to reset to, and do `git reset --hard`.
