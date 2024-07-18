# nip4 --- a user interface for libvips

**Work in progress** I'm hoping this will be working by mid 2024, but for
now it doesn't do much.

This is a reworking of [nip2](https://github.com/libvips/nip2) for the gtk4
UI toolkit.

nip2 is a spreadsheet-like interface to the [libvips image processing
library](https://libvips.github.io/libvips). You create a set of formula 
connecting your objects together, and on a change nip2 will recalculate. 
This makes it convenient for developing image processing systems since you 
can watch pixels change as you adjust your equations.

Because nip2 uses libvips as the image processing engine, it can handle very
large images and only needs a little memory. It scales to fairly complex
workflows: I've used it to develop systems with more than 10,000 cells,
analyzing images of many tens of gigabytes. It has a batch mode, so you
can run any image processing system you develop from the command-line and
without a GUI.

[![Screenshot](images/shot1.png)](images/shot1.png)

https://github.com/user-attachments/assets/a7602239-121b-4fac-aec1-2c9521248ff1
