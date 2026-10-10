# Building and packaging

The root README contains the supported local build instructions. C++17 and Objective-C++ are used with Qt Widgets/Network, AppKit, CoreGraphics and Carbon. There is no JavaScript overlay or custom emulator build.

## Resource preparation

`tools/rom_reader.py` reads the DS filesystem, NARC archives and Generation V text tables. `tools/prepare_resources.py` generates species data, encounter tables, family relationships and Latin bitmap glyphs from an IRDI ROM supplied by the user. English names, item game-index mappings and Gen V move descriptions come from the public PokeAPI CSV dataset; the table cache is under `~/.cache/nuzlocke-companion/` by default. Sprites use PokeAPI's Generation V Black/White directory.

Boss teams and English location labels are small factual configuration files in `tools/`. Generated resources, ROMs, saves and ledgers must remain untracked.

To refresh generated data, remove only the preparer's cache files you intend to refresh and rerun it. A cached setup can use `--cache /path/to/cache`.

## A local standalone app

For personal use on a machine without a separate Qt installation, use Qt's deployment tool **after the final executable is built**:

```sh
"$(brew --prefix qt)/bin/macdeployqt" build/NuzlockeCompanion.app -no-codesign
codesign --force --deep --sign - build/NuzlockeCompanion.app
codesign --verify --deep --strict build/NuzlockeCompanion.app
```

Copying a newly built executable into an already deployed bundle requires running `macdeployqt` again; otherwise Homebrew Qt and bundled Qt may be loaded together.

Ad-hoc signing is for a local build. Public macOS binaries would need an appropriate release/signing/notarization process. Do not package `Contents/Resources/nuzlocke`, `resources/`, a ledger folder or source-machine diagnostics into a public download. Bare builds prompt for a locally prepared resource folder.

## Validation

The CI workflow compiles a bare macOS bundle and checks Python syntax, with no ROM or generated resources. Compilation is not a gameplay compatibility claim. Use your own disposable run for manual gameplay validation; do not automate inputs into a player's active run.

An optional `--render-preview <4 MiB RAM snapshot> <output.png>` mode can render an offline preview without connecting to melonDS. Treat real snapshots as private and never commit them. The README's preview uses synthetic data only.
