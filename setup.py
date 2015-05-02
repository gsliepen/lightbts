#!/usr/bin/env python

from distutils.core import setup

setup(
    name = "LightBTS",
    version = "0.1",
    description = "Light-weight bug tracking system",
    author = "Guus Sliepen",
    author_email = "guus@lightbts.info",
    url = "http://lightbts.info/",
    py_modules = ["lightbts"],
    scripts = [
        "lightbts-cli",
        "lightbts-email",
        "lightbts-web",
    ],
)
