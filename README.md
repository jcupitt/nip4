# nip4 --- a user interface for libvips

**Work in progress** See screenshots and videos below for the current state.
I'm hoping this will be mostly working in early 2025.

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

[![Screenshot](images/shot2.png)](images/shot2.png)

https://github.com/user-attachments/assets/6f7bdee1-183c-4554-9701-e0c30e75d58a
