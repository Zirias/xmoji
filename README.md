# Xmoji – plain X11 emoji keyboard

Xmoji is a simple emoji keyboard for X11 designed to work without relying on
any toolkit or input method. It doesn't use a toolkit itself and instead
renders its GUI using XRender requests.

![Xmoji in fvwm3](.github/screenshots/xmoji.png?raw=true)

This is currently work in progress and should become the successor of my
[qXmoji](https://github.com/Zirias/qxmoji) tool which used Qt for its GUI.

## Features

* Left-clicking an emoji sends it to whatever X11 application currently has
  the keyboard focus using faked key press events. It automatically appends
  a zero-width space and for ZWJ sequences prepends another ZWJ, which helps
  some receiving X clients to get their display correct. This behavior will
  be made configurable in a future release.
* Middle-clicking an emoji selects it, so it can be transferred using the X11
  "primary selection" mechanism (typically middle-click again where you want
  to insert it).
* Emojis are displayed in tabs with one tab per main group as suggested in
  Unicode files.
* Emojis with skin-tone variants are grouped again, only the neutral version
  is shown and right-clicking it shows a fly-out style menu with all available
  versions.
* There's also a search tab to find emojis by their name.

## Building

To obtain the source from git, make sure to include submodules, e.g. with the
`--recurse-submodules` option to `git clone`. Release tarballs will include
everything needed for building.

Dependencies:

* A C compiler understanding GNU commandline options and the C11 standard
  (GNU GCC and LLVM clang work fine)
* GNU make
* fontconfig
* freetype
* harfbuzz
* xcb libs: libxcb-cursor, libxcb-image, libxcb-xkb and libxcb-xtest
* xkbcommon and xkbcommon-x11

For example, on a Debian or Ubuntu system, you would install these packages:

    libfontconfig1-dev libfreetype-dev libharfbuzz-dev libxcb-cursor-dev
    libxcb-image0-dev libxcb-xkb-dev libxcb-xtest0-dev libxkbcommon-x11-dev

To build and install Xmoji, you can simply type

    make
    make install

