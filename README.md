# Omarchy File Browser (Qt 6 / QML)

Native file browser for Omarchy with feature parity to the TUI prototype under
`TUI_File_Browser`. Live Omarchy theme sync and desktop text scaling (like Omacalc).

## Quick install

```bash
bash install.sh              # check deps, build, install to /usr
omarchy-fb-qt                # run
bash install.sh uninstall    # remove (+ restore defaults / hotkey)
```

After install, the script asks whether to:

1. Set **Omarchy File Browser** as the default folder handler (`xdg-mime` / `inode/directory`)
2. Bind **SUPER+SHIFT+F** to `omarchy-fb-qt` (overrides Omarchy’s Nautilus binding in
   `~/.config/hypr/bindings.lua`)

Non-interactive flags:

```bash
bash install.sh --default-fm --hotkey
bash install.sh --no-default-fm --no-hotkey
```

Optional pacman package:

```bash
bash install.sh package
sudo pacman -U packaging/out/omarchy-fb-qt-*.pkg.tar.*
```

Note: the repo may live on a CIFS share without `+x` — always use `bash install.sh`
(not `./install.sh`). Builds run under `/tmp` by default.

## Dependencies

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-multimedia`
- CMake ≥ 3.21
- Optional: `gio` / `trash-cli` (trash), `ttf-ia-writer`, `gst-libav` / `gst-plugins-good`
  (video codecs), `qt6-webengine` (PDF page preview)

## Manual build

```bash
cmake -S . -B /tmp/omarchy-fb-qt-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/omarchy-fb-qt-build -j"$(nproc)"
/tmp/omarchy-fb-qt-build/omarchy-fb-qt [path]
```

## Features

- Keyboard-first navigation (`Enter` / `→` open, `←` / `Backspace` parent)
- Preview: text, images, video/audio (Qt Multimedia), PDF (optional `qt6-webengine`)
- Open / edit / rename / trash / permanent delete / new file & folder
- Copy / cut / paste, marks, name filter, sort modes, hidden files
- Favorites (`F`, `Ctrl+F`)
- Open with… for unknown types (`Shift+O`); choice remembered per MIME type
- Textual files open in `$EDITOR` / system text editor — never in a browser
- Live theme from `~/.local/state/omarchy/current/theme/colors.toml`

## Shortcuts

Press `?` in the app for the full list (localized).

## Configuration

`~/.config/omarchy-fb-qt/config.conf` (QSettings / INI):

```ini
[browser]
start_path=/home/you
show_hidden=false
preview_max_bytes=65536
use_trash=true
sort_by=name
language=auto
bookmarks=

[open_with]
# mime/type=some-app.desktop
application/x-sharedlib=nvim.desktop
```

- `language=auto` (default) follows the **system UI locale** (`QLocale` / `LANG`).
- Set `language=en` or `language=de` to force a catalog; press `l` to cycle at runtime.

## Languages

JSON catalogs in `languages/` (`en.json`, `de.json`, …). English is the fallback.
To add a language: copy `languages/_template.json` → `fr.json` and translate.

## Packaging

- PKGBUILD: `packaging/PKGBUILD` (local package via `bash install.sh package`)
- Upstream feature-request draft: [`packaging/FEATURE_REQUEST.md`](packaging/FEATURE_REQUEST.md)

For an Omarchy package repo, copy `packaging/` into
`omarchy-pkgs/pkgbuilds/omarchy-fb-qt/`, set a release tag, and update `sha256sums`.
