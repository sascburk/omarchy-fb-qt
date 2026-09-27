# Feature request draft — Omarchy native file browser (Qt 6 / QML)

Paste / adapt this when opening an issue or discussion upstream
(e.g. [basecamp/omarchy](https://github.com/basecamp/omarchy) or the Omarchy packages repo).

**Clone URL:** https://github.com/sascburk/omarchy-fb-qt

Screenshots (also in the repo root):

- Root listing: https://github.com/sascburk/omarchy-fb-qt/blob/main/omarchy-fb-qt_root.png
- Image preview: https://github.com/sascburk/omarchy-fb-qt/blob/main/omarchy-fb-qt_preview.png

---

## Title

Native Qt file browser for Omarchy (`omarchy-fb-qt`) — keyboard-first, theme-synced

## Summary

Proposal to ship a **native Qt 6 / QML file browser** as an **optional** Omarchy app
(alongside Omacalc / Omawrite): keyboard-driven, Hyprland-friendly, live Omarchy theme
sync, and feature parity with the existing TUI file browser experiments.

This is meant as an **alternative** to Nautilus, not a forced replacement. Installer
can optionally set it as the default folder handler and/or rebind `SUPER+SHIFT+F`.

## Why

- Omarchy already has strong Qt Quick reference apps (Omacalc, Omawrite).
- A light, keyboard-first browser fits the Omarchy workflow better than always using a
  full GNOME file manager.
- Live theme + text-scale integration keeps the desktop feel consistent.
- Users who prefer Nautilus keep it; the Qt browser is opt-in.

## What’s already implemented (prototype)

Repo: https://github.com/sascburk/omarchy-fb-qt — proposed package name **`omarchy-fb-qt`**

- Navigate folders (`Enter` / `←` `→` / `Backspace`), preview pane (text, images,
  video/audio, optional PDF via `qt6-webengine`)
- Open / edit / rename / trash / permanent delete / new file & folder
- Copy / cut / paste, marks, name filter, sort modes, hidden files
- Favorites (`F` / `Ctrl+F`), directory size calc
- Open with… for unknown types (persisted per MIME); text files use `$EDITOR`, never a browser
- i18n (`en`/`de`); `language=auto` follows the system UI locale
- Live theme from `~/.local/state/omarchy/current/theme/colors.toml`
- Follows Omarchy / GNOME text scaling
- Desktop entry + PKGBUILD + `bash install.sh`

Installer desktop integration (asked interactively, or via flags):

1. Set as default handler for `inode/directory` (`xdg-mime`)
2. Bind `SUPER+SHIFT+F` → `omarchy-fb-qt` (override Omarchy’s Nautilus binding in
   `~/.config/hypr/bindings.lua`; uninstall restores defaults)

## Try it

```bash
# from the project root (use bash — shares may be noexec)
bash install.sh
# optional non-interactive:
# bash install.sh --default-fm --hotkey
omarchy-fb-qt
```

Uninstall: `bash install.sh uninstall`  
Or pacman package: `bash install.sh package` then `sudo pacman -U packaging/out/omarchy-fb-qt-*.pkg.tar.*`

**Deps (runtime):** `qt6-base`, `qt6-declarative`, `qt6-multimedia`  
**Optional:** `qt6-webengine` (PDF preview), GStreamer plugins (video codecs)

## Ask

1. Interest in packaging this as an official **optional** Omarchy package?
2. Preferred name / binary (`omarchy-fb-qt`, `omarchy-fb`, `omarchy-files`, …)?
3. Should official packaging expose the same optional “default FM + SUPER+SHIFT+F”
   prompts, or leave that to user config only?
4. Any must-have integrations (walker, portals, further keybindings)?

Happy to adjust branding, keybindings, and packaging layout to Omarchy conventions.

## Out of scope (for now)

- Full Nautilus feature parity (network places, advanced permissions UI, etc.)
- Replacing Omarchy’s default Nautilus binding unless the user opts in
