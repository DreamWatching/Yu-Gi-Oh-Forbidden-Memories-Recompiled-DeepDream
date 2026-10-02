# NEW Card Markers

This independent Game > Mods entry extends Build Deck's NEW badges for duel
rewards. The original game remembers only 16 recent card IDs and badges all of
them, including cards already owned. This mod remembers every distinct card
awarded during the current loaded session and badges it only if it was absent
from both the chest and equipped deck before the award. It works in solo and
tag duels, and does not depend on Tag Duels being enabled.

The extra marker history is kept in PC save states, but not in the original
campaign save format. Save the game normally to retain the cards themselves;
after restarting the application the original 16-card recent list applies
until new rewards are earned.

Build from the source root with:

```sh
python tools/pc/build_mod.py examples/mods/new-card-markers
```

## Player guide

See MOD_GUIDE.md in the release package for every option, defaults, rule timing,
recipe behavior and compatibility. Launcher description:

Track every distinct newly acquired reward card during the loaded session, beyond the native 16-card recent list. NEW appears only when the card was absent from both chest and equipped deck before the award. Works independently in solo and tag play, without changing drops or card quantities. Extra history is not stored in normal campaign saves.
