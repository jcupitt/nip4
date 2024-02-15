# nip4 --- a user interface for libvips

nip4 is a gtk4 GUI for the [libvips image processing
library](https://libvips.github.io/libvips). It's a little like a spreadsheet:
you create a set of formula connecting your objects together, and on a
change nip4 will recalculate. This makes it convenient for developing
image processing systems since you can watch pixels change as you adjust
your equations.

Because nip4 uses libvips as the image processing engine it can handle very
large images and only needs a little memory. It scales to fairly complex
workflows: I've used it to develop systems with more than 10,000 cells,
analyzing images of many tens of gigabytes. It has a batch mode, so you
can run any image processing system you develop from the command-line and
without a GUI.

**Work in progress** I'm hoping this will be working by mid 2024, but for
now it doesn't do much.


