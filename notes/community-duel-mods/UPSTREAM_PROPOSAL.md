# Proposal draft: optional Tag Duels and reusable Free Duel support

This is a local draft, not a message already sent to the maintainer.

## Suggested issue title

Proposal: mod support for optional Tag Duels and configurable Free Duels

## Suggested issue body

We have a working Windows prototype that adds optional two-versus-two Free
Duels with private decks/hands, shared team fields, AI or human partners,
native-asset setup menus, configurable starting LP/terrain, a Guardian Star
opening contest, and proportional team ranking.

It currently needs a companion runtime based on v0.1.4-preview.2 commit
`759465ed4986714f47513da648087c9f5567d18a`; it is not yet certified against
current master. The goal is to maintain Tag Duels as an optional mod and
contribute the smallest reusable host services that it requires.

Proposed review areas:

1. Named overlay pixel drawing and a logical canvas for consistent menu size.
2. Temporary deck editing that restores the player's inventory and saves.
3. Separate partner recipes, persisted by stable card identity with safe writes.
4. Configurable opening seats while preserving the first-turn attack restriction.
5. Five-digit LP panels that preserve native/HD name rendering.
6. A scoring projection shared by live rank and final result calculation.

A comparison with current master found that its API 5 slot is `duelist_id`,
whereas the prototype had privately used that slot for pixel drawing. The
release candidate removes that private ABI change and resolves drawing by a
named symbol. We would like guidance on the preferred public extension
contract before proposing an official API addition.

The mod now reconstructs Guardian Star artwork from the player's own USA disc
at runtime. No extracted image bundle, disc image or personal save is needed
in a contribution. Disc-free regression tests cover inventory isolation,
opening-turn rules, terrain cadence, rank consistency, LP rendering and
recipe persistence. Full natural-duel and cross-mod acceptance is still being
completed; Linux is not advertised as tested.

Would these host services fit the project's direction? Should Tag Duels live
in `examples/mods`, or remain in a separate repository? We can submit focused
PRs in your preferred order and adapt the extension design to your conventions.

## Contribution sequence

Start with the independent texture-clear optimization and its equivalence
test. It has no Tag Duel dependency and should be reviewed separately.
Then agree on extension design before rebasing the larger support patches.
Do not submit the complete prototype as a single unsolicited feature merge.

## Proposed PR groups

| Patch | Purpose | Dependencies |
|---|---|---|
| 01 texture clear | Clear contiguous wrapped rows with equivalent results | None |
| 02 overlay | Named ARGB drawing and uniformly scaled logical canvas | Agree extension contract |
| 03 opening AI | Preserve opening no-attack rule for either starting side | Gameplay acceptance |
| 04 LP HUD | Fit five-digit life values; preserve native and HD names | Both renderer tests |
| 05 deck services | Temporary inventory sandbox, ownership repair, partner recipes | Save migration tests |
| 06 rank projection | Same proportional view for live and final scores | Agree generic provider contract |
| Optional mod | Tag Duel gameplay/UI | Required services accepted or companion runtime |

The current rank bridge still names `tag-duel-menu:rank_side_v1` directly.
Before proposing it as a generic host interface, discuss whether the maintainer
prefers a managed event or a provider registry, including chaining rules.
No upstream acceptance is implied by this plan.
