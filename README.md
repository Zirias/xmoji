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

* `glitches` (no class): Enable workarounds for rendering glitches. This
  is interpreted as a bit mask to enable individual workarounds for
  rendering bugs. Default: `0`.

  There's only one bit currently available:
  - `1`: Use a transformation for compositing RGBA glyphs instead of a
       simple offset. Enable this if you experience broken rendering of
       emojis.
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
* `border` (class `Border`): Color for borders. Default: `dark gray`.
* `tooltipBorder` (class `Border`): Color for the border of tooltips.
  Default: `black`.

### Example

The following X resources configure a larger default emoji font, a
slightly larger scroll bar and a dark color scheme:

    Xmoji*emojiFont: emoji:pixelsize=24
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
    Xmoji*tooltipBorder: light sea green
    Xmoji*scrollBarWidth: 12
    Xmoji*scrollBarMinHeight: 25

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
* libxcb, libxcb-cursor, libxcb-image, libxcb-xkb and libxcb-xtest
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

* `BUNDLED_POSER` (bool): Uses the bundled poser lib and links it statically.
  When disabled, poser must be installed and will be linked as a shared
  library. Default: `on`.

* `WITH_SVG` (bool): Enable support for fonts with SVG glyphs. When enabled,
  freetype >= 2.12 is required and the bundled
  [nanosvg](https://github.com/memononen/nanosvg) is built for rasterizing
  the SVG glyphs. Default: `on`.

