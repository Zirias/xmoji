# How to add a new translation

This will guide you through the process of adding a translation for the
hypothetical language "Xylophone" with language-code `xy`.

### Preparations

Any changes should be done on top of the current `master` branch.

The recommended workflow is to create a fork of `Xmoji` on github and work on
a dedicated branch in your fork. For example, after forking to your account
`__JohnDoe__`, you'd do the following:

    git clone --recurse-submodules https://github.com/__JohnDoe__/xmoji.git
    cd xmoji
    git checkout -b translate-xy

In case you don't want to use github, you can also just clone the original
repository and create your local branch (same as above with `Zirias` instead
of `__JohnDoe__`).

### Add the new language

In `src/bin/xmoji/xmoji.mk`, add `xy` to the `xmoji_LANGUAGES` variable.
Please keep the language codes sorted alphabetically.

Then run

    make update-translations

This will generate a new file `src/bin/xmoji/translations/xmoji-ui-xy.def`.

### Translate the texts

Edit the file `src/bin/xmoji/translations/xmoji-ui-xy.def` with your
preferred text editor. The entries have the following format:

    $[cw]$<translation key>
    <original text>
    .
    <translated text>
    .

The `<translated text>` part will be missing in newly generated entries or
when the original text changed since running `make update-translations` the
last time. You should only edit this part. Never change the `<original text>`,
it will be used to determine whether a text changed.

Edit `src/bin/xmoji/xmoji.desktop.in` and add `GenericName[xy]` and
`Comment[xy]` entries. Please keep the language codes here sorted
alphabetically as well.

### Finish

Now, building and installing Xmoji should give you a version fully localized
for "Xylophone".

`git add` the new file `src/bin/xmoji/translations/xmoji-ui-xy.def`,
`src/bin/xmoji/xmoji.mk` and `src/bin/xmoji/xmoji.desktop.in`, create a
commit, and please, send a pull request, thank you very much! 🤩

If you didn't fork on github, you can also run `git format-patch -1` after
committing and send me the resulting patch-file to
[felix@palmen-it.de](mailto:felix@palmen-it.de).
