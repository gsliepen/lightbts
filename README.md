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
* Read-only web interface, stand-alone or via FastCGI or CGI
* Many features from the Debian BTS are supported
* More to come

## Web interface

The `lbts-web` program serves a read-only web interface for a LightBTS
instance. It works without JavaScript; if JavaScript is enabled, it uses
[htmx](https://htmx.org/) to update the ticket list without reloading the page.
To run it stand-alone on http://127.0.0.1:8080/, run this in the directory of
the instance:

    lbts-web

Use `--listen host:port` to listen on another address. `lbts-web` detects when
it is started by a web server via CGI or FastCGI, and `--fastcgi=SOCKET` lets
it listen for FastCGI requests on a socket. See `lbts-web(1)` for details.

Building and installing
-----------------------

## Dependencies

LightBTS needs a C++23 compiler (tested with GCC 14) and the following libraries:

- sqlite3
- b2 (BLAKE2)
- [Mimesis](https://github.com/gsliepen/mimesis) 0.2 or later

The web interface additionally needs:

- cpp-httplib
- libfcgi
- kainjow-mustache
- htmx (optional, it is embedded into `lbts-web`)

If possible, use your operating system's package manager to install the
dependencies. For example, on Debian and its derivatives, you can get all of
them by running the command:

    sudo apt install libsqlite3-dev libb2-dev libcpp-httplib-dev libfcgi-dev libkainjow-mustache-dev libjs-htmx

Mimesis is included as a git submodule in `external/mimesis`, and is used
automatically if it is not installed on your system. Make sure to clone this
repository with `git clone --recursive`, or run `git submodule update --init`.

## Build system

LightBTS uses the Meson build system. Ensure you have this installed. Use your
operating system's package manager if available, for example on Debian and its
derivatives, use `sudo apt install meson`. If that is not possible, download
and install Meson from https://mesonbuild.com/.

To build LightBTS, run these commands in the root directory of the source code:

    meson setup build
    ninja -C build

The web interface is built if its dependencies are found. Add `-Dweb=disabled`
to the setup command to never build it, or `-Dweb=enabled` to make its
dependencies required. Use `-Dhtmx=PATH` if `htmx.min.js` is not in
`/usr/share/javascript/htmx/`, or `-Dhtmx=` to build without htmx.

To run the test suite, run:

    meson test -C build

To install the binaries and manual pages on your system, run:

    meson install -C build --skip-subprojects

The `--skip-subprojects` option prevents the bundled copy of Mimesis from being
installed as well; LightBTS links it statically, so it is not needed at runtime.

The install command might ask for a root password if it detects the installation
directory is not writable by the current user.

By default, a debug version will be built, and the resulting binaries will be
installed inside `/usr/local/`. If you want to create a release build and
install it in `/usr/`, use the following commands:

    meson setup --buildtype=release --prefix=/usr build-release
    meson install -C build-release --skip-subprojects

Distribution packagers that want to override all compiler flags should use
`--buildtype=plain`. The `DESTDIR` environment variable is supported by Meson.
