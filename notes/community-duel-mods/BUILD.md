# Build the Beta 5 Community Companion

The branch is based on v0.1.4-preview.2 commit
`759465ed4986714f47513da648087c9f5567d18a`. The companion support is already
applied. Do not apply the exported support patch again on this branch.

Follow upstream notes/pc-build.md for the Windows prerequisites. Use Python
3.12 or later. The upstream tools fetch/check pinned dependencies when missing.
A disc is not required for compilation or the fifteen logic checks.

From this repository root:

```text
python tools/pc/run_tag_release_checks.py
python tools/pc/build_game32.py --target windows --release
python tools/pc/build_mod.py examples/mods/duel-options --out tmp/community-mods/duel-options --game tmp/pc/game32
python tools/pc/build_mod.py examples/mods/tag-duel-menu --out tmp/community-mods/tag-duel-menu --game tmp/pc/game32
python tools/pc/build_mod.py examples/mods/duel-portraits --out tmp/community-mods/duel-portraits --game tmp/pc/game32
python tools/pc/build_mod.py examples/mods/new-card-markers --out tmp/community-mods/new-card-markers --game tmp/pc/game32
```

Build the executable and modules together; an old export table does not prove
compatibility with a newly linked host. Enable Duel Options before Tag Duels;
Portraits and NEW Card Markers are independent. Tag Rewards is a default-on
setting inside Tag Duels. Read MOD_GUIDE.md for every gameplay option.

The release archive remains the previously playtested beta-5 executable and
objects. This clean source branch additionally relocates the same partner
planner into the Tag module without altering its logic, and makes the LP
fixture target preview.2's retained native panel behavior. These source-layout
and fixture changes should be noted when comparing archive hashes; builds are
not asserted byte-identical. See MAINTAINER_REVIEW.md for validation and limits.

Original assets are decoded from each player's own USA disc. Do not commit
a disc, extracted images/audio, personal saves, tmp build outputs or logs with
machine paths. Keep the upstream MIT notices and link back to the project.

Linux and unknown future/stock hosts are not certified by this beta. GitHub
Actions runs the Windows disc-free checks; ROM-dependent gameplay and physical
controllers need local testing. A new compiler/host build needs its own native
acceptance before being offered as a replacement player download.
