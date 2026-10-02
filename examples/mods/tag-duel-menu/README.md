# Tag Duels — 0.11.0-beta.3 module-separation development build

An optional Free Duel mod with Solo/Tag setup inside the native character
picker. Triangle opens duel type; Tag collects Partner, Rival 1 and Rival 2,
then offers Battle Rules. Your save name and Player 1/CPU/Player 2 labels
identify who controls the team. A second controller is optional.

This candidate requires its accompanying Windows runtime. It uses the shared
API 4 prefix plus named drawing, logical-canvas, deck-editor and ranking
services. It is not certified for stock upstream executables or Linux.

Turn portraits now belong to the separate **Duel Portraits** module. Enable
both to retain the existing tag HUD. Solo and campaign portraits can run
without Tag Duels. **Duel Options is required** and now owns the native menu,
recipe/editor flow and shared match rules. Tag Duels owns private teams/decks,
partner control, AI and tag ranking. **Tag Rewards** is enabled by default under **Tag Duels > Settings** in the
launcher. It owns enhanced drop rules and attribution; campaign award repair
remains mandatory even when the setting is off.
Previously staged
0.11.0-beta.1 packages remain untouched.

## Features

- Four private 40-card decks and five-card hands; teammates share LP and field.
- Partner AI, Player 1 controls both, or optional Player 2 control.
- Signature partner deck, temporary native deck editor, or separate partner
  recipes; the player's deck and inventory are restored after partner editing.
- Player recipe selection and owned-only shortage repair before a duel.
- Starting LP: 4000, 8000, 12000, 16000, 20000.
- Normal/named terrain, static random terrain, or animated dynamic changes.
  Rerolls exclude the active terrain. Tag changes after completed turns
  4, 9, 12, 17, 20, 25… relative to whichever duelist starts; solo uses
  corresponding alternating two-turn/one-turn boundaries.
- Opponent Draw Pool: 5 or 20 searchable cards for both CPUs. This controls
  AI inspection of the hand/upcoming deck, not a physically larger hand.
- Roll For Who Plays First opens Guardian Star Face-off. Ten shuffled cards
  use the native star advantage relationships; draws retry. The rival rolls
  only after the player's choice: 45% winning, 45% losing, 10% random among
  remaining cards. CPU selection animations last about two seconds.
  The winner chooses Play First/Play Second and the first team's starter.
- The canonical Player/Rival 1/Partner/Rival 2 order rotates around that
  starter. The opening duelist cannot attack.
- Existing widescreen turn portraits, native assets/font/button icons, and
  native field-change animation.
- Shared live/final rank: average private-deck progress, action/turn counters
  normalized for two teammates, LP normalized to the original 8000-point scale.
  Letter thresholds, POW/TEC pools and the result layout remain unchanged.
- The embedded Tag Rewards setting offers both opponents plus one rare SPOILS roll, replacing
  the last configured drop. The log records the source opponent of each roll.
- NEW Card Markers is a separate optional mod.

## Assets and saves

Cards, stars, portraits, menu textures and crossed swords are reconstructed
from the player's USA disc. No artwork pixels are embedded in this mod.
The two crossed blades use the same complete native sword sprite.

Partner recipes use a versioned/checksummed sidecar, separate from player
slots. Safe replacement keeps the previous valid file as `.bak`. Mod cards
are stored by stable identity; missing card mods make those recipes unavailable
without changing their saved identities. Legacy retail recipes migrate on
save; ambiguous old numeric mod-card recipes cannot be recovered reliably.

This is a local release candidate: full natural-duel, clean-install and
cross-mod acceptance remains pending. See `RELEASE_READINESS.md` and the
release notes' acceptance ledger. Added duelists and extended NPC signature
pools are not advertised as supported.

## Build

From the prepared companion source repository:

```sh
python tools/pc/build_game32.py --target windows
python tools/pc/build_mod.py examples/mods/tag-duel-menu --out tmp/tag-mod
python tools/pc/run_tag_release_checks.py
```

The source overlay must be applied to its exact documented upstream commit.
The mod refuses a host missing the required named capabilities. Keep the
companion installation separate from the original game, and transfer ordinary
save slots rather than cross-build save states.

Beta.3 polish: Guardian Face-off cannot be cancelled after Begin Duel; panel-wide
opaque fade, no redundant begin icon, portraits appear only in native duel startup,
and Triangle duel-type prompts are hidden while Tag Duels is unavailable.
Credits: deepdream and Codex.

## Player guide

See MOD_GUIDE.md in the release package for every option, defaults, rule timing,
recipe behavior and compatibility. Launcher description:

Two-versus-two Free Duels with separate decks and hands, shared team Life Points and field, rotating teammates and team-based ranking. Choose a signature, edited or recipe partner deck; let CPU, Player 1 or Player 2 control the partner. Tag Rewards is a default-on setting that mixes both rivals' drop pools and reserves the final drop for rare spoils. Requires Duel Options.
