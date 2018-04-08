# LightBTS

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

## Building LightBTS

The C++ version of LightBTS needs the following libraries:

- sqlite3
- b2
- fmt3
- boost-filesystem
- mimesis

On a Debian installation, you can get all but the last library by running the
command:

    sudo apt install sqlite3-dev libb2-dev libfmt3-dev libboost-filesystem-dev

Mimesis is available from the same location as LightBTS itself. Make sure you
compile and install Mimesis first.
