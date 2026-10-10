# Third-party code, references and assets

- **melonDS 1.1**, GPL-3.0-or-later: `src/ScreenLayout.cpp` and `src/ScreenLayout.h` derive from the Qt frontend's screen geometry code. Original copyright notices remain intact. No emulator core is included. https://github.com/melonDS-emu/melonDS/tree/1.1
- **Qt 6**: a build/runtime dependency, not vendored in this source repository. Qt's applicable license terms remain separate. https://www.qt.io/licensing/
- **AppKit, CoreGraphics, Carbon**: macOS system frameworks.
- **PokeAPI datasets and sprites**: the preparer downloads English terminology, item game indices, Gen V move descriptions and Generation V graphics for local use. https://github.com/PokeAPI/pokeapi/tree/master/data/v2/csv and https://github.com/PokeAPI/sprites . Pokémon names/graphics remain the property of their respective rights holders; no sprite archive is vendored here.
- **Game tables and bitmap fonts**: extracted locally from the user's own Italian White 2 ROM. No extracted font, ROM text archive, save or firmware is committed or provided as a public release asset.
- **Format references**: PKHeX's PK5/SAV5 documentation/code, Universal Pokémon Randomizer ZX's Gen5 text/ROM handlers, Swan's BW2 structure definitions and PokeLua's language-specific address documentation. These informed independent parsing/monitoring implementations; they are not bundled dependencies. https://github.com/kwsch/PKHeX , https://github.com/Ajarmar/universal-pokemon-randomizer-zx , https://github.com/ds-pokemon-hacking/swan , https://github.com/Real96/PokeLua .
- **Logo**: generated with imagegen for this companion. The Poké Ball motif and Pokémon marks remain subject to their respective owners' rights.

The GPL license covers this project's code and configuration, subject to the retained notices. It does not relicense Nintendo/Game Freak/The Pokémon Company's intellectual property or grant trademark rights.
