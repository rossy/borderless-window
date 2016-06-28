borderless-window
=================

Many Windows applications choose to remove the system window borders on their
main window. The naive way of doing this is to create a popup window (with the
WS_POPUP style) instead of an overlapped window, however popup windows are more
suited to things like splash screens and other overlays, not main application
windows. They lack a number of useful behaviours such as window snapping and
the window menu, so they can be frustrating for users who expect these
behaviours.

The proper way to create a borderless application window is to create a regular
overlapped window and remove the borders by expanding the client area. This is
what Office, Visual Studio, iTunes and UWP CoreWindows do, however doing this
exposes some quirks in the windowing system that must be worked around in each
application.

**borderless-window** is a minimal demonstration of how to work around these
quirks. It creates a window that:

- Works with window snapping, including double-click to snap vertically
- Works with Aero Flip 3D
- Obeys the "minimize all" command (Win+M)
- Obeys the cascade and tile commands
- Shows a window menu when Alt+Space is pressed
- Does not cover the taskbar when maximized
- Does not disable auto-hide taskbars when maximized
- Casts a drop-shadow, like other top-level windows
- Can be resized by the user

Copying
-------

This project is a demonstration of something simple that is made unnecessarily
difficult by quirks in the Windows API. In order to be as useful as possible to
as many application developers as possible, it is placed in the public domain.
Attribution would be appreciated, but is not required (after all, this stuff
should have been documented by Microsoft.)

To the extent possible under law, the author(s) have dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

[![CC0](http://i.creativecommons.org/p/zero/1.0/80x15.png)](http://creativecommons.org/publicdomain/zero/1.0/)
