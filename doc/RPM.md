# hrmp rpm

`hrmp` can be built into a RPM for [Fedora](https://getfedora.org/) systems.

## Setup RPM development

```sh
sudo dnf install rpmdevtools
rpmdev-setuptree
```

## Get the source tree

```sh
git clone https://github.com/HighResMusicPlayer/hrmp.git
cd hrmp
```

## Install build requirements

```sh
sudo dnf builddep hrmp.spec
```

## Create the source archive

```sh
VERSION=0.14.0 # Should match one in hrmp.spec.
TAG=HEAD # You can also use any real tag or sha.
git archive --format=tar.gz --prefix=hrmp-$VERSION/ $TAG >~/rpmbuild/SOURCES/hrmp-$VERSION.tar.gz
```

## Create RPM packages

```sh
rpmbuild -ba hrmp.spec
```

The resulting RPMs will be located in `~/rpmbuild/RPMS/x86_64/`, if your architecture is `x86_64`.
