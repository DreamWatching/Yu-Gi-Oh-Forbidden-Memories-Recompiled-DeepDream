# Forbidden Memories Community Duel Mods

Created by **deepdream and Codex**. Four independently enabled modules share the
same companion runtime. Tag Rewards is a setting inside Tag Duels, not a fifth
mod. These descriptions cover implemented features only.

## Duel Options

Make each Free Duel your own using menus built from the game's original stone
frames, font, portraits and button symbols. Select an opponent in the native
Free Duel screen, then adjust Battle Rules before committing to the duel. Works
by itself for single duels; enable Tag Duels to add team selection.

| Option | Choices and effect |
|---|---|
| Opponent Draw Hand | **5 Cards / 20 Cards**; default **20**. Controls how many hand/upcoming-deck cards the CPU can search when planning its play. The visible hand still has five slots. Applies to every rival in the configured Free Duel, including both tag rivals; campaign AI is unchanged. |
| Starting Life | **4000, 8000, 12000, 16000 or 20000 LP**; default **8000**. Both sides start equally. In Tag Duel, each team shares its selected LP total. |
| Starting Field | **Normal, Forest, Wasteland, Mountain, Sogen, Umi or Yami**; default **Normal**. Starts with that terrain and its existing game effects. |
| Random / Fixed | Randomly chooses one of the six field-card terrains at the start. Does not automatically reroll; ordinary field cards still work. |
| Random / Dynamic | Starts on a random field-card terrain and repeatedly changes it, using the native field-spell animation and recalculating terrain bonuses. Each roll excludes the currently active terrain. |
| Roll For Who Plays First | **On / Off**; default **On**. On opens Guardian Star Face-off; Off uses the ordinary player-first opening. The first individual turn cannot attack, whichever team starts. |

Dynamic fields count completed individual turns relative to the actual starter,
including a CPU or partner chosen through Face-off. Tag changes after turns
**4, 9, 12, 17, 20, 25...**: within each repeating eight-turn block, after its
fourth turn and after the first turn of the next block. Single duels use the
same rule with two seats: **2, 5, 6, 9, 10, 13...**. Animation may wait for a safe
effect boundary. Random terrain never adds new terrain types.

### Guardian Star Face-off

Ten face-down cards form a shuffled ring. Choose a card; the CPU then scans the
remaining pile and chooses its own. Chosen cards visibly leave the ring, then
both flip together. The game's existing Guardian Star advantages decide the
winner; neutral or equal stars draw and start another selection. Winner text
helps explain the matchup. This opening minigame does not change a monster's
Guardian Stars or add abilities during the duel.

The CPU selection uses **45% winning, 45% losing and 10% random** branches
relative to your picked star. These are selection weights, not final win/draw
odds: the random branch can also produce a win or loss. This is a deliberately
balanced opening minigame, not a hidden precommitted coin toss.

The winner chooses **Play First** or **Play Second**. In Tag Duel the winner
also chooses which member of the **team going first** opens. A CPU winner chooses
to play first 80% of the time, second 20%, and chooses between that team's two
members equally. CPU choices animate before confirmation. Once Begin Duel has
committed the opening sequence, Circle cannot cancel or reroll its result.

### Deck recipes and preparation

Choose your current deck or a named recipe. The recipe list has **Player** and
**Partner** tabs, switched with **L1/R1**. **X** chooses a recipe;
**Triangle or Square** opens its temporary editor. These controls are for the
recipe list, not a general tab-switch prompt on every menu.

Partner recipes and player recipes are separate. You can select a partner
recipe for your own deck, but this never grants its borrowed cards to your
inventory. If cards or copies are missing, a warning asks whether to use it
anyway. Accepting equips only owned copies, leaves the missing positions empty,
and requires you to complete a legal deck through the normal chest screen.
The repaired usable deck is then offered for recipe saving. Rejecting does not
equip the borrowed deck. Your own deck is checked before the duel begins.

Options are remembered between Free Duels and frozen when a match is confirmed.
The list uses the game's textured scrollbar in solo and tag modes; the thumb
fills the rail when all rows fit, and shrinks as more rows require scrolling.
The Triangle duel-type prompt appears only when Tag Duels is enabled. With
Duel Options alone, choose an opponent to open the single-duel rules directly.

### For community creators

Duel Options exposes a versioned registration interface in
`examples/mods/shared/duel_options.h`. Contributors can define tabs, labels,
help, supported modes, defaults and numeric choices, and read confirmed match
values. Disabled contributors' rows are hidden. Registering an option creates
its UI and frozen value; the contributing mod must implement its gameplay
effect and preference persistence. The interface remains experimental.

## Tag Duels

Play **two against two** through the native Free Duel character picker. Select
**partner, rival 1, then rival 2**, adjust Battle Rules, prepare decks and play.
Requires **Duel Options**. Your team and the rival team each share a field and
LP, while all four duelists keep their own deck and hand. A card already on the
shared field remains that card when teammates rotate.

The fixed turn ring is **Player > Rival 1 > Partner > Rival 2 > repeat**.
Face-off rotates its starting point without reshuffling the ring. For example,
choosing Partner to open gives Partner > Rival 2 > Player > Rival 1. Choosing
Rival 2 gives Rival 2 > Player > Rival 1 > Partner.

| Tag-only option | Choices and effect |
|---|---|
| Partner Control | **Partner AI**: CPU takes the partner's turn against the rival team. **Player 1**: you control both teammates with your normal inputs. **Player 2**: the second input port controls the partner; a second controller is optional. |
| Partner Deck: Signature | Uses the chosen character's built-in NPC campaign/signature deck. It does not mean one of your saved player decks. |
| Partner Deck: Edit Deck | Starts from that signature deck in a temporary chest/editor labeled for the partner. Changes affect the partner draft, not your chest or the NPC's permanent campaign deck. On leaving, you can save a named partner recipe or leave without saving it. A complete edited draft remains available for the pending duel; it does not become a permanent NPC deck. Your own deck preparation follows separately. |
| Partner Deck: Deck Recipe | Opens the named recipe list to reuse a player or partner recipe. Partner recipes avoid rebuilding an edited deck before every duel. Recipe editing remains available inside that list. |
| Tag Rewards — launcher setting | **On / Off**, default **On**, under **Game > Mods > Tag Duels > Settings**. Controls the enhanced reward policy described below. |

Temporary partner editing restores your real player deck and inventory when
leaving, including when the editor is reopened. Incomplete decks cannot become
valid saved recipes. Overwriting asks for confirmation. Partner recipes use a
versioned, checked file with a previous-file backup; failed saves preserve the
previous valid recipe. Custom-card identities are retained, but a recipe whose
required card mod is missing cannot be used until that content is available.

### Tag ranking

The existing POW/TEC rank calculation is adapted to the whole allied team.
It uses average progress through the two private decks and normalizes combined
action/turn counters for two teammates, so the last active teammate's hand or
deck does not alone determine your rank. LP is scaled back to the original
8000-LP reference for ranking. Existing letter thresholds, POW/TEC drop-table
selection and the native result layout remain in place. There is no new
hard-mode reward multiplier or separate ranking option in this release.

### Tag Rewards

On a tag victory, each ordinary reward roll chooses between the two rivals
with equal probability, then rolls that rival's drop table for the earned
rank/pool. Several identical drops can still occur, and a small set of rolls is
not guaranteed to include an ordinary drop from each rival. Source attribution
on the results/logs identifies the opponent behind each roll.

The **last configured reward roll** is replaced with **Rare Spoils**, selected
from eligible low-probability entries across both rivals' rank-appropriate
pools. This preserves the configured total reward count rather than adding a
bonus card. Rare means a positive table weight **at most 16 out of 2048**;
if neither pool has such entries, it uses their lowest positive weight instead.
Eligible opponent/card entries are selected equally for this spoils roll.
This describes drop rarity, not card power, price or foil rarity. Empty pools
fall back to the native drop behavior and cannot guarantee a rare reward.

Turn Tag Rewards off to use the native reward-selection policy. Inventory
award safety remains active: displayed rewards must be added to the player's
real collection rather than a temporary teammate inventory. This setting does
not alter ordinary campaign or single-duel drop probabilities. After results,
you return to Free Duel with setup choices retained rather than being forced
to reload your campaign save.

## Duel Portraits

An independent presentation mod for campaign, single Free Duel, retail two-player
and tag duels. It adds two portrait panels in the widescreen margins: your
current team member on the left and the rival member on the right. The active
turn portrait is bright; the waiting duelist is dimmer. Team rotation changes
the portrait with a visual transition. Yugi uses his relaxed waiting pose and
serious active pose.

Panels show names, **Player 1 / Player 2 / CPU**, and hand/deck counts. Player
names come from loaded saves; rivals use their character names. They appear
when native LP counters begin filling, remain hidden in deck preparation, and
hide at narrow/4:3 widths so they do not cover the original game. Retail
two-player humans use Yugi's poses; custom NPC portrait mapping is not supported.

Enable/disable **Duel Portraits** independently in Game > Mods. It has no
additional configurable settings and changes no AI, card effects, turn order,
LP totals, reward probabilities or deck ownership. Tag Duels itself does not
draw this HUD, so enable both for tag portraits.

## NEW Card Markers

Find genuinely new reward cards more easily in Build Deck. The original recent
list stores only **16 card IDs** and may label duplicates. This independent mod
tracks every distinct reward card acquired during the current loaded session
and shows NEW only if it was absent from **both your chest and equipped deck**
before acquisition. Getting more copies of a card you already own does not
make it new again. This does not change card drops or inventory quantities.

Enable/disable it independently in Game > Mods; no additional settings are
required. It works with solo rewards and Tag Rewards without depending on Tag
Duels. Extended marker history exists in supported PC save states, but is not
written into the original campaign-save format. After restarting, the native
16-card recent list applies until new rewards are earned. Normal saves still
retain the cards themselves; do not transfer save states across beta builds.

## Installation and current compatibility

This is a **Windows beta companion build**, targeting **v0.1.4-preview.2** at
commit `759465ed4986714f47513da648087c9f5567d18a` with companion build ID
`7298ae13`. It requires your own **USA SLUS-01411 disc**. It is not a drop-in
mod pack for a stock executable, Linux, or an unknown future upstream version.
The runtime includes required support changes; future host updates require
integration and testing.

Extract the complete runtime archive into a new folder. Follow README.md for
disc selection, enabling mods and copying normal save slots. Keep original
saves and recipes backed up. Module-only archives require this exact companion
and their declared dependencies. The old standalone Tag Rewards module should
not be installed alongside the embedded reward system.

Game textures, portraits and minigame art are read from your disc; extracted
artwork, soundtrack, disc images and saves are not included. Physical-controller,
real-GPU/HD and broader compatibility acceptance remain release checks. This
package is prepared for beta review, not yet publicly uploaded. Exodia timers,
Guardian Star powers and dynamic music discussed during brainstorming are not
implemented features of this release.
