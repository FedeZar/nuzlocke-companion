# Contributing

Please keep changes focused and describe the issue, resulting behavior and validation performed. The implementation is small:

- `src/Gdb.cpp`: local read-only GDB transport and selective memory collection.
- `src/Nuzlocke.cpp`: Pokémon decoding, battle/field monitoring and persistent ledger.
- `src/Overlay.inc`: four-panel drawing, layout and interactions.
- `src/Mac.mm`: macOS fullscreen panel behavior and keyboard shortcuts.
- `src/main.cpp`: settings, app lifetime and panel geometry.
- `tools/`: read-only local resource preparation.

The English UI is the public default. English ROM support is a separate contribution: identify its offsets and verify party, boxes, field transitions, battles and Repel readings before enabling another game code. Do not assume the IRDI addresses work for another edition or translate an address by one constant without validating every structure.

Never commit ROMs, saves, RAM snapshots, extracted bitmap fonts, generated resources, nicknames, personal paths, credentials or ledgers. For screenshots use synthetic data. Keep GPL notices on borrowed geometry code and credit any added dependency.

Compile and check resource-tool syntax for source changes. For monitor changes, use a disposable run or synthetic fixtures. Never move a player's character or trigger a battle without authorization. Do not claim gameplay coverage based only on a successful build.

Bug reports should include the Companion version, macOS architecture/version, melonDS version, game code and steps to reproduce. Avoid posting diagnostic JSON by default; it can contain run data.
