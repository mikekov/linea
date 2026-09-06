# Compiling Linea on MacOS

Linea on MacOS can be built with Homebrew.

## Using Homebrew

### Dependencies

Prerequisites:

- Xcode command line tools (`xcode-select --install`); a full Xcode install is not required
- [HomeBrew](https://brew.sh/)

Make sure you don't have any MacPorts stuff in your PATH. <!-- TODO - how? -->

Install packages:
```
brew install \
    bdw-gc \
    boost \
    cairomm \
    ccache \
    cmake \
    double-conversion \
    gettext \
    glibmm \
    gsl \
    icu4c \
    imagemagick \
    lcms2 \
    libepoxy \
    libxslt \
    ninja \
    pango \
    pangomm \
    pkg-config \
    poppler \
    potrace \
    qt6
```

You may substitute `imagemagick` with `graphicsmagick`.


### Get Linea Source
Check out the source if you haven't already:

```
git clone --recurse-submodules https://github.com/mikekov/linea.git
cd linea
```

### Build Linea

Inside the Linea directory, run the following commands

```
# for debug build run:
./configure.sh

# or for release build:
# ./configure.sh -r

cd build

ninja

# Start Linea
./bin/linea.app/Contents/MacOS/linea
```

To create a distributable app bundle, run:

```
ninja install
```

It will create `build/install/linea.app` with all dependencies bundled.

## Packaging

`ninja install` (or `cmake --install build`) installs to `build/install` by default,
producing `build/install/linea.app` with Qt frameworks bundled via `macdeployqt`.
Set `-DCMAKE_INSTALL_PREFIX=...` at configure time to install elsewhere.

## Problems

### gettext / Intl not found

`gettext` is keg-only. If configure can't find `Intl` or `msgfmt`, run
`brew link gettext` and re-run `./configure.sh`.

### Wrong library paths

Some libraries can cause trouble if they are picked up from the SDK instead of Homebrew (observed with libxslt and libxml2). Adding them to `$PKG_CONFIG_PATH` should fix this, e.g.:

```
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$(brew --prefix)/opt/libxslt/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$(brew --prefix)/opt/libxml2/lib/pkgconfig"
```
