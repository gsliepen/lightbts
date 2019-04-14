About LightBTS
--------------

This is a lightweight bug tracker. The main goal is to make bug tracking as
easy as possible. Once installed, you can start a bug tracker instance in any
directory you want, simply by calling:

    lbts init

From that point on, you can create, modify and close tickets using the command
line interface from that directory, or any of its subdirectories. If you know
git, you will feel instantly familiar with this way of working. However,
LightBTS itself does not require git in any way. To learn about all the
commands that the command line interface supports, just type:

    lbts help

## Features

* Command-line interface
* Email interface
* Stand-alone webserver or as CGI
* Many features from the Debian BTS are supported
* More to come

Building and installing
-----------------------

## Dependencies

The C++ version of LightBTS needs the following libraries:

- sqlite3
- b2
- fmt3
- boost-filesystem
- [mimesis](mimesis)

Make sure you have all these libraries installed before proceeding. If
possible, use your operating system's package manager to install the
dependencies. For example, on Debian and its derivatives, you can get all but
the last library by running the command:

    sudo apt install sqlite3-dev libb2-dev libfmt3-dev libboost-filesystem-dev

Mimesis is available from the same location as LightBTS itself. Make sure you
compile and install Mimesis first.

## Build system

LightBTS uses the Meson build system. Ensure you have this installed. Use your
operating system's package manager if available, for example on Debian and its
derivatives, use `sudo apt install meson`. If that is not possible, download
and install Meson from https://mesonbuild.com/.

To build LightBTS, run these commands in the root directory of the source code:

    meson build
    ninja -C build

To run the test suite, run:

    ninja -C build test

To install the binaries on your system, run:

    ninja -C build install

The latter command might ask for a root password if it detects the installation
directory is not writable by the current user.

By default, a debug version will be built, and the resulting binaries will be
installed inside `/usr/local/`. If you want to create a release build and
install it in `/usr/`, use the following commands:

    meson --buildtype=release --prefix=/usr build-release
    ninja -C build-release install

Distribution packagers that want to override all compiler flags should use
`--buildtype=plain`. The `DESTDIR` environment variable is supported by Meson.
