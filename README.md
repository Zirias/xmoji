# Xmoji – plain X11 emoji keyboard

Xmoji is a simple emoji keyboard for X11 designed to work without relying on
any toolkit or input method. It doesn't use a toolkit itself and instead
renders its GUI using XRender requests.

![Xmoji in fvwm3](.github/screenshots/xmoji.png?raw=true)

This the successor of my [qXmoji](https://github.com/Zirias/qxmoji) tool which
used Qt for its GUI. Some of qXmoji's features (notably translations) are still missing though.

[![Packaging status](https://repology.org/badge/vertical-allrepos/xmoji.svg)](
https://repology.org/project/xmoji/versions)

## Features

* Left-clicking an emoji sends it to whatever X11 application currently has
  the keyboard focus using faked key press events. There are settings to
  automatically append a zero-width space and for ZWJ sequences to prepend
  another ZWJ, which might help some receiving X clients to get their display
  correct.
* Middle-clicking an emoji selects it, so it can be transferred using the X11
  "primary selection" mechanism (typically middle-click again where you want
  to insert it).
* Emojis are displayed in tabs with one tab per main group as suggested in
  Unicode files.
* Emojis with skin-tone variants are grouped again, only the neutral version
  is shown and right-clicking it shows a fly-out style menu with all available
  versions.
* The search tab allows to find emojis by their name.
* The history tab shows the most recently used emojis and is automatically
  persisted.

## XResources

Everything concerning appearance and rendering is configured in the
traditional X11 way, using X resources. Xmoji only looks for them on the
root window of the running X session, so make sure you upload them with
the `xrdb` tool.

The application class name is `Xmoji`, the instance name defaults to the
name of the executable (`xmoji`), but can be overridden with the `-name`
commandline argument. Most resources can also be overridden on the
commandline by giving the instance name of their last component, so e.g.
running `xmoji -emojiFont emoji-24` will set the emoji font to that
font.

### Fonts

Fonts are given as search patterns for `fontconfig`, multiple patterns
can be given separated by commas, for example `Noto
Sans-14:bold,sans-14`. Fonts are global, so can't be scoped to
individual widgets.

* `font` (class `Font`): The font for display of normal text. Default:
  `sans`.
* `emojiFont` (class `Font`): The font for display of emojis. Default:
  `emoji`.

### Generic rendering options

* `backingStore` (no class): Use an offscreen pixmap for window contents
  and contents of scrollable areas. Disable this to save X server memory
  at the cost of visible flicker. Default: `1`.
* `scrollBarWidth` (no class): Width in pixels for scroll bars.
  Default: `10`.
* `scrollBarMinHeight` (no class): Minimum height in pixels for scroll
  bars. Default: `16`.

### Colors

Colors can be given in these formats:

* A well-known X11 color name
* RGB(A) values in simple hex notation: `#rgb`, `#rrggbb`, `#rrrgggbbb`,
  `#rrrrggggbbbb`, `#rgba`, `#rrggbbaa`, `#rrrrggggbbbbaaaa`.
* RGB(A) values in X11 notation `rgb:<r>/<g>/<b>`/`rgba:<r>/<g>/<b>/<a>`,
  where each component can have 1 to 4 hex digits.
* RGB(A) values in CSS notation `rgb(<r>,<g>,<b>)`/`rgba(<r>,<g>,<b>,<a>)`,
  where each component is a decimal number between 0 and 255. The `rgb`
  prefix can also be omitted.

Colors can be scoped to individual widgets by their class and instance
names. The following colors are available:

* `foreground` (class `Foreground`): Normal foreground color. Default:
  `black`.
* `background` (class `Background`): Normal background color. Default:
  `gray`.
* `aboveForeground` (class `Foreground`): Foreground color for elemets
  appearing above normal. Default: `black`.
* `aboveBackground` (class `Background`): Background color for elements
  appearing above normal. Default: `light gray`.
* `belowForeground` (class `Foreground`): Foreground color for elements
  appearing below normal. Default: `black`.
* `belowBackground` (class `Background`): Background color for elements
  appearing below normal. Default: `dark gray`.
* `lowestForeground` (class `Foreground`): Foreground color for elements
  appearing lowest. Default: `black`.
* `lowestBackground` (class `Background`): Background color for elements
  appearing lowest. Default: `web gray`.
* `activeForeground` (class `Foreground`): Foreground color for active
  elements. Default: `dark slate gray`.
* `activeBackground` (class `Background`): Background color for active
  elements. Default: `light blue`.
* `disabledForeground` (class `Foreground`): Foreground color for
  disabled elements. Default: `dim gray`.
* `disabledBackground` (class `Background`): Background color for
  disabled elements. Default: `silver`.
* `selectedForeground` (class `Foreground`): Foreground color for
  selected elements. Default: `light blue`.
* `selectedBackground` (class `Background`): Background color for
  selected elements. Default: `medium blue`.
* `tooltipForeground` (class `Foreground`): Foreground color for
  tooltips. Default: `black`.
* `tooltipBackground` (class `Background`): Background color for
  tooltips. Default: `navajo white`.
* `linkForeground` (class `Foreground`): Foreground color for hyperlinks.
  Default: `dark blue`.
* `hoverForeground` (class `Foreground`): Foreground color for hovered items,
  used with hyperlinks. Default: `blue`.
* `border` (class `Border`): Color for borders. Default: `dark gray`.
* `tooltipBorder` (class `Border`): Color for the border of tooltips.
  Default: `black`.

### Example

The following X resources configure a different emoji font, a slightly larger
scroll bar and a dark color scheme:

    Xmoji*emojiFont: Twitter Color Emoji
    Xmoji*Foreground: #c8c6c5
    Xmoji*Background: #211f1d
    Xmoji*Border: #191210
    Xmoji*belowBackground: #191210
    Xmoji*lowestBackground: #120f0d
    Xmoji*aboveBackground: #302a28
    Xmoji*activeBackground: #403830
    Xmoji*disabledForeground: #777777
    Xmoji*selectedForeground: black
    Xmoji*selectedBackground: #77ddff
    Xmoji*tooltipForeground: light cyan
    Xmoji*tooltipBackground: dark slate gray
    Xmoji*linkForeground: dodger blue
    Xmoji*hoverForeground: cyan
    Xmoji*tooltipBorder: light sea green
    Xmoji*scrollBarWidth: 12
    Xmoji*scrollBarMinHeight: 25

## Runtime configuration

Runtime configuration is stored in a configuration file. This file is
automatically monitored for external changes.

By default, Xmoji will use a configuration file in these places:

* `${XDG_CONFIG_HOME}/xmoji.cfg`, if `XDG_CONFIG_HOME` is set.
* `${HOME}/.config/xmoji.cfg`, if `HOME` is set.
* `~/.config/xmoji.cfg`, with `~` refering to the home directory obtained
  from the passwd database.

The location of the configuration file can be overridden from the command line
with `-cfg`, e.g.

    xmoji -cfg /tmp/xmoji.cfg

CAUTION: Xmoji will attempt to create missing directories for storing its
runtime configuration.

The file stores everything available in the Settings dialog, which you can
access by right-clicking in the window, outside of an Emoji with a fly-out.
The settings are described in the dialog using tooltips.

Additionally, the history of recently used emojis is also stored in the
configuration file.

## Building

To obtain the source from git, make sure to include submodules, e.g. with the
`--recurse-submodules` option to `git clone`. Release tarballs will include
everything needed for building.

Dependencies:

* A C compiler understanding GNU commandline options and the C11 standard
  (GNU GCC and LLVM clang work fine)
* GNU make
* fontconfig
* freetype ( >= 2.12 when SVG support is enabled, see below )
* harfbuzz
* libpng ( >= 1.6 )
* libxcb ( >= 1.14 ), libxcb-cursor, libxcb-image, libxcb-xkb and libxcb-xtest
* xkbcommon and xkbcommon-x11

For example, on a Debian or Ubuntu system, you would install these packages:

    libfontconfig1-dev libfreetype-dev libharfbuzz-dev libxcb-cursor-dev
    libxcb-image0-dev libxcb-xkb-dev libxcb-xtest0-dev libxkbcommon-x11-dev

To build and install Xmoji, you can simply type

    make
    make install

If your default `make` utility is not GNU make (like e.g. on a BSD system),
install GNU make first and type `gmake` instead of `make`.

### Build options

Options can be given as variables in each make invocation, e.g. like this:

    make FOO=yes
    make FOO=yes install

Alternatively, they can be saved and are then used automatically, like this:

    make FOO=on config
    make
    make install

The following build options are available:

* `TRACE` (bool): Enables tracing of all X11 requests. When running with
  `-vv`, this prints all requests to stderr. Also, when an error occurs,
  the code and location of the request that caused it is printed.
  For debugging only. Default: `off`.

* `BUNDLED_FREETYPE` (bool): Build freetype2 and link it statically. For this
  to work, download a freetype tarball and place it in `ftbundle/` first.
  Use this when the installed freetype on your system doesn't match the
  requirements, e.g. misses PNG support. Default: `off`.

* `BUNDLED_POSER` (bool): Uses the bundled poser lib and links it statically.
  When disabled, poser must be installed and will be linked as a shared
  library. Default: `on`.

* `WITH_NLS` (bool): Support loading translations for the current locale
  (from `LC_MESSAGES`) at runtime and build/install the existing translation
  files. See [TRANSLATE](TRANSLATE.md) for how to add new translations.
  Default: `on`.

* `WITH_SVG` (bool): Enable support for fonts with SVG glyphs. When enabled,
  freetype >= 2.12 is required and the bundled
  [nanosvg](https://github.com/memononen/nanosvg) is built for rasterizing
  the SVG glyphs. Default: `on`.

* `WITH_KQUEUE` (bool): Use `kqueue` (available on BSD systems) for watching
  the configuration file instead of periodically calling `stat()`.
  Default: `on` when `kqueue` is detected, `off` otherwise.

* `WITH_INOTIFY` (bool): Use `inotify` (available on Linux) for watching the
  configuration file instead of periodically calling `stat()`.
  Default: `on` when `inotify` is detected, `off` otherwise.

Enabling both `WITH_KQUEUE` and `WITH_INOTIFY` at the same time is an error.
Even when one of them is enabled, Xmoji will try to detect whether the
configuration file is stored on NFS and in that case silently fall back to
periodically calling `stat()` on it.

