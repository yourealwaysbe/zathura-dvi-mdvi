
ZATHURA-DVI-MDVI
================

Port of [Evince](https://wiki.gnome.org/Apps/Evince)'s DVI backend to
[Zathura](https://pwmt.org/projects/zathura/).  I guess based on
[MDVI](http://mdvi.sourceforge.net/).

This is still a work in progress.

DEPENDENCIES
============

Incomplete list:

    + [Cairo](http://cairographics.org/)
    + [Kpathsea](https://www.tug.org/texinfohtml/kpathsea.html)
    + [Zathura](https://pwmt.org/projects/zathura/)

BUILD + INSTALL
===============

Should be able to run

    make

and then copy dvi.so to /usr/lib/zathura/.
