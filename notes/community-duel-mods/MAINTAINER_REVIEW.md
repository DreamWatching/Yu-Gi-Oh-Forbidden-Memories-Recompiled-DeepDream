# Review map for Unchiga — DeepDream Community Duel Mods, Beta 5

We built an optional two-versus-two Free Duel experience and split its reusable
parts into four separately enabled mods. Contributor credit is **deepdream and
Codex**. This is a community beta, not an accepted upstream feature. It targets
**v0.1.4-preview.2**, commit
`759465ed4986714f47513da648087c9f5567d18a`.

## What players get

- Duel Options: native-asset Free Duel setup, starting LP (4000–20000), named
  terrain or static/dynamic random terrain, five/twenty-card CPU search, named
  deck recipes and Guardian Star Face-off for the starting team/duelist.
- Tag Duels: four separate decks/hands with shared team LP/fields; CPU, Player 1
  or Player 2 partners; temporary signature-deck editing; a rotating fixed turn
  ring; proportional team ranking. Tag Rewards is an embedded, default-on
  launcher setting, mixing both rivals' rank-based pools and reserving the final
  configured drop for rare spoils.
- Duel Portraits: active/waiting native portraits, loaded-save names and control
  labels in widescreen margins. Independent of Tag Duels and presentation only.
- NEW Card Markers: session-long first-ownership reward badges beyond the native
  recent-card list; independent of Tag Duels and reward quantities.

[MOD_GUIDE.md](MOD_GUIDE.md) lists every option, exact probabilities, field
cadence, deck ownership rules and compatibility limits. Features discussed but
not implemented (Guardian powers, Exodia timers, dynamic music) are not claimed.

## Module entry points

| Area | Start reviewing here |
|---|---|
| Native setup UI, recipes and opening minigame | [tag_duel_menu.c](../../examples/mods/duel-options/tag_duel_menu.c) |
| Shared confirmed LP/terrain/starter/CPU policy | [match_runtime.c](../../examples/mods/duel-options/match_runtime.c) |
| Extensible option descriptors and snapshots | [option_registry.c](../../examples/mods/duel-options/option_registry.c) and [duel_options.h](../../examples/mods/shared/duel_options.h) |
| Tag lifecycle, teammate swapping, AI/control, reward ownership | [tag_duel_runtime.c](../../examples/mods/tag-duel-menu/tag_duel_runtime.c) |
| Module registration and embedded reward integration | [tag_module.c](../../examples/mods/tag-duel-menu/tag_module.c) |
| Optional reward selection and attribution | [tag_rewards.c](../../examples/mods/tag-duel-menu/tag_rewards.c) |
| Proportional team scoring formulas | [tag_rank.h](../../examples/mods/tag-duel-menu/tag_rank.h) |
| Portrait presentation/lifecycle | [duel_portraits.c](../../examples/mods/duel-portraits/duel_portraits.c) |
| First-acquisition badges | [new_card_markers.c](../../examples/mods/new-card-markers/new_card_markers.c) |

The partner planner is private to Tag Duels (tag_planner.c and partner_planner.h); it is the
same planner used by beta 5, relocated to avoid replacing upstream's separate
mods/ai-hard-mode files. Original planner credit/license is preserved.

## Why the executable needed support changes

| Host files | Purpose |
|---|---|
| src/pc/mods/mods.c; overlay_pixels.h | Named pixel drawing and uniform logical canvas services; no private insertion into a numbered API slot. |
| src/pc/saves/deck_menu.c/.h; partner_recipes.c/.h | Temporary NPC inventory sandbox, named player/partner recipes, owned-only recipe preparation, safe persistence and restoration. |
| src/game/main_run_duel.c | PC-only return to Free Duel instead of losing the loaded session after a configured match/editor flow. |
| src/game/duel_scene_field_actions.c | PC opening-turn attack guard for whichever duelist starts. |
| src/pc/cards/rank.c/.h; src/game/duel_result_runtime.c | One projected scoring view for live/final tag rank; native rank thresholds and result layout retained. |
| src/pc/text/hd_text.c | Participant-provider naming so LP labels agree with the currently displayed tag duelist and loaded save. Native preview.2 five-digit panel rendering retained. |
| src/pc/render/texture_dump.c | Equivalent contiguous clearing in place of repeated wrapped writes; independently reviewable optimization. |

The game-side additions are behind MEMORIES_PC; this does not claim a completed
PS1 matching-build acceptance. The existing hard-mode files, upstream renderer
panel geometry and its private-PDB-path protections are preserved.

The current rank and naming bridges explicitly consult the Tag provider. They
are working companion contracts, not a claim of a finalized generic extension
API. We welcome guidance on a provider registry/event interface and chaining
before asking for upstream acceptance. The source changes can be reviewed
separately from whether any optional mod should be bundled.

## Validation and limits

All fifteen disc-free suites pass from this isolated source checkout. These
exercise actual source functions, including recipe faults/ownership restoration,
opening AI, tag ranking, retained preview LP/HD crops, texture-clear equivalence,
module/option lifecycle, match/field cadence, preference migration, rewards,
NEW badges, deck confirmations, partner input routing, Face-off cancellation,
scrollbar bounds and portrait start timing. Run tools/pc/run_tag_release_checks.py.
The LP fixture was updated from older prototype crop assumptions to preview.2's
full/right-half panel and one name-overlay draw; the renderer was not changed
back to the prototype to satisfy that test.

The previously tested beta-5 runtime has native scripted captures and checks for
solo/tag setup, win/rewards, normal save/reload, repeated terrain animations and
human input routing. A fresh reload compared all 722 chest quantities against
pre-duel stock plus the award log with zero mismatches. Private helpers/logs and
personal paths are not included in this source branch. Native screenshots and
current limitations accompany the beta release, not hidden gameplay fixtures.

This is not certification of every mod combination, real controller hotplug,
all GPU/HD settings, losses/Exodia/deck-out, Linux, future host versions or every
cinematic/fusion path. Result-transition warnings still need triage. 400% speed
does not promise 200 displayed FPS. The clean-checkout Windows release host and all four portable modules built
successfully from this branch. This compile check does not replace native
acceptance of a newly built executable.

## Proposed upstream process

Start with the reusable host services and their tests, in focused pull requests
and in your preferred order. Keep optional mods in a separate community branch
unless you want examples bundled. Rebase against your requested revision and
agree the provider contracts before claiming stock-host compatibility.

Original project: https://github.com/Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled
No ROM, extracted graphics/soundtrack or personal save is required in the source
or contributions. This document explains the work; upstream acceptance remains
the maintainer's decision.
