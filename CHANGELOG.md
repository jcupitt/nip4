## master

## 9.0.12 2025/08/21

- better power of two detection in openslideload
- fix tile edges with gtk_snapshot_set_snap() [kleisauke]
- better fallback if gtk snap is not there
- high-dpi support
- icon for windows exe
- fix Fontname creation
- add ^G to group selected rows
- add "Group selected" to column menu
- toolkitgroupview is more compact
- add vips_header_get builtin
- add Real, Group, Vector graphic classes
- better row drop
- cancel row drag with Esc
- better row drag animation
- menu pins stack

## 9.0.11 2025/07/21

- fix path for Widget_pathname_item.action [MvGulik]
- add more draw compat definitions [MvGulik]
- better error / alert system
- filters on file dialogs
- fix a segv in Colour widget edit action
- much better next/prev/refresh in imagewindow
- much better imagewindow focus indicator
- new tilecache flickers less and is faster
- matching changes in libvips 8.18 improve render speed and improve
  cancellation of out of date renders
- better sync of view settings between imagewindow and thumbnail
- better row dirty marking stops subrows getting stuck in large workspaces

## 9.0.10 2025/06/22

- refactor of tilesource improves OME_TIFF modes
- fix menu state init
- save and restore more view settings
- add "Merge column" and "Merge tab" submenus
- update interface during recomp, update regions during drag
- better detection of absolute filenames on windows
- various small bugfixes

## 9.0.9 2025/05/28

- fix select modifier handling
- add region resize and drag snap
- add hguide and vguide, plus a gesture for creation
- improve menu homing
- improve antialias on zoom out
- add region context menu

## 9.0.8 2025/04/23

- fix ctrl/shift detect on windows
- add menu pin button
- improve scRGBA display
- fix "gtk_window_get_application: assertion 'GTK_IS_WINDOW (window)' failed"

## 9.0.6 2025/04/16

- fix perspective distort
- fix "save" in save-n-quit
- fix a crash when running with libvips 8.16

## 9.0.5 2025/03/26

- better change handling in ientry improves matrixview behaviour
- better subrow finding fixes replace from file in nested columns
- fix a crash on row access after row drag between columns

## 9.0.4 2025/03/20

- revise image repaint
- more robust workspace load

## 9.0.3 2025/03/13

- fix save-as
- revise image repaint (again)
- fix a crash in graph export

## 9.0.2 2025/03/10

- improve image repaint
- improve file chooser
- better windows build

## 9.0.1-5 2025/02/27

- fix kplot on win
- fix macOS startup
- build polishing

## 9.0.1 2025/02/24

- fix Colour display
- fix 16-bit alpha scaling
- use `VIPS_PATH_MAX` everywhere (not `FILENAME_MAX`) [kleisauke]
- fix `ninja test`
- fix bands -> height image to matrix conversion
- fix `to_colour`
- add `nip4-batch` batch mode interface
- remove ^C/^V in mainwindow
- show parent/child row relationships on hover

## 9.0.0-10 2025/02/18

- first test release
