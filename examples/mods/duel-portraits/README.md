# Duel Portraits 0.1.0-beta.2

Independent turn portraits for retail campaign, solo Free Duel and two-player
duels. Tag Duels supplies a presentation snapshot for rotating teammates.
The module never hooks turn logic, AI, cards, deck ownership or rewards.

Requires our companion runtime's `ModMenu_DrawPixelsV1` service. It is not yet
compatible with the official stock executable. Original artwork is decoded
from the player's USA disc, never included in this package.

Apply **Duel Portraits** in Game > Mods. There is no dependency on Tag Duels.
Tag Duels 0.11.0-beta.2 no longer draws portraits itself: enable both to retain
the tag portrait experience. Older Tag Duels builds already draw portraits;
do not combine those with this module, as the panels would overlap.

Both humans use Yugi's waiting/active campaign poses in retail two-player mode.
Their names come from each loaded two-player save. Campaign and solo opponents
use their retail Free Duel portrait. Custom NPC portrait mapping is not yet
supported. At 4:3 or narrow widths the panels hide rather than cover the game.

Validation: actual-module unit checks and native build completed. Complete
campaign/two-player/tag gameplay and real-GPU visual acceptance remain pending.
This is a local development candidate, not an approved public release.

## Player guide

See MOD_GUIDE.md in the release package for every option, defaults, rule timing,
recipe behavior and compatibility. Launcher description:

Show active and waiting duelists in the widescreen margins with native portraits, loaded-save names, control labels and hand/deck counts. Yugi changes pose between waiting and active turns; tag portraits follow teammate rotation. Appears when Life Points begin filling, not during deck editing. Presentation only; independent of Tag Duels.
