# Duel Options framework: development build

Owns the native Free Duel setup, Guardian Star Face-off, recipe selection,
temporary partner deck editor and shared LP/terrain/opening-turn lifecycle.
Solo operation does not require Tag Duels. Tag team setup is available when
the optional Tag Duels provider is enabled. This build requires the companion
runtime and is not yet approved for public release.

Extension contract: `../shared/duel_options.h`. Declare an optional dependency
using `after`, or a mandatory dependency using `requires`, before resolving
`duel-options:options_v1` through the host. Check both ABI and struct size.
Register descriptors during Init; settings and their persistence belong to
the contributing mod. Contributors' disabled options are hidden automatically.
Registration copies all descriptor strings and choices. Host pointers remain
loaded until game exit, as guaranteed by the existing mod API.

Each option declares a tab, label, explanation, supported match modes,
default and explicit numeric choices. The registry rejects duplicate keys per
mod, invalid versions, unknown modes, reserved host keys and invalid values.
Stale settings fall back to the default without overwriting user preferences.
`rows` returns the total eligible count, even when the output buffer is smaller.

At duel confirmation the framework freezes eligible options. Resolve
`duel-options:match_value_v1` as `int (*)(const char *,const char *,int *)`
to read your mod's confirmed owner/key/value during the match. Live preference
changes take effect at the next confirmation. A caller can also request an
owned snapshot; no callback or registration pointer is retained in it.
The full snapshot is large (about 70 KB): allocate it statically
or on the heap, not in a constrained game stack or save-state record.

`builtin_options.c` registers CPU search pool, LP, terrain and opening-roll settings. Existing
Tag preferences migrate once into the framework namespace without overwriting
existing framework values. External options appear in their declared tabs,
reached with L1/R1 from Battle Rules. Five visible rows use the native scrollbar;
additional rows scroll. Invalid or disabled contributors are excluded.

Remaining: full startup/disable matrix, save-state compatibility, real-GPU
playtests and preview-runtime integration. The extension contract remains
experimental. Private test contributors under tmp are not release content.

Opponent Draw Hand is a built-in solo/tag rule, default 20 searchable cards.
It keeps five visible hand slots and is frozen when the duel is confirmed.
Campaign AI is unchanged. Triangle duel-type prompts appear only while the
Tag Duels provider is enabled; standalone solo rules open after opponent choice.
Credits: deepdream and Codex.

## Player guide

See MOD_GUIDE.md in the release package for every option, defaults, rule timing,
recipe behavior and compatibility. Launcher description:

Customize solo and tag Free Duels using native menus: starting Life Points, named or random terrain, five/twenty-card CPU search and Guardian Star Face-off to choose who starts. Includes named player/partner recipes and safe deck preparation. Solo play works without Tag Duels; other mods can register additional rules.
