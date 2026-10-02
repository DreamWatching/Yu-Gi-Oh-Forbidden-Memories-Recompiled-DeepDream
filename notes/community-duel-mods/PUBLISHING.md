# Publishing the Community Duel Mods

Prepared for deepdream. No repository, issue, pull request or release has been
published by this local preparation.

## Recommended route

Publish a community beta in your own GitHub repository or fork. Separately,
propose the reusable host support to Unchiga. A release on your account is a
community release; an official upstream release requires the maintainer's
acceptance and publication. Opening a pull request does not automatically add
a download to the creator's Releases page.

The current runtime targets v0.1.4-preview.2 commit
759465ed4986714f47513da648087c9f5567d18a with required companion support changes.
It is not a stock-host mod ZIP. Describe the Windows executable as a modified
community companion build and retain exact compatibility information.

## What deepdream needs to do

1. Sign in to GitHub using the account you want publicly associated with the
   project. Use deepdream in credits. Enable GitHub's private commit email if
   you do not want a personal address in future commits.
2. Fork https://github.com/Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled for
   upstream contribution. Keep development on a dedicated branch. Alternatively
   publish modules in a dedicated repository and maintain a separate fork for
   the companion host changes. Do not upload your entire local game folder.
3. Import the reviewed module source, shared headers, required host changes,
   regression tests and build instructions into the chosen source layout. The
   modular-source-review-beta5 ZIPs are source review inputs, not a complete
   replacement for the upstream repository. Preserve existing license notices
   and link back to the upstream project. Check the final public changes before
   committing; do not include user profiles, discs, private logs or backups.
4. Open an integration proposal in the upstream Issues page. The locally
   prepared UPSTREAM_PROPOSAL.md supplies a draft. Explain the optional modules,
   exact companion requirement and tests, and ask which services/design the
   maintainer would accept. Nothing in that draft has been sent.
5. After the maintainer's direction, submit focused pull requests: reusable
   rendering/deck/rank/opening support separately from optional mod gameplay.
   Rebase and test against the maintainer's requested revision. Retain the
   reviewed preview.2 beta for community testing until that integration passes.
6. On your own repository, create a GitHub Release with a new tag and mark it
   **pre-release**. Suggested title: **Forbidden Memories Community Duel Mods —
   Windows Preview.2 Beta 5**. Include the detailed MOD_GUIDE.md descriptions,
   installation instructions, screenshots, exact base/companion build IDs and
   known acceptance limits. Upload only the approved artifacts below. A draft
   release lets you check everything before clicking Publish release.
7. Ask a tester on another Windows computer to extract the full package into a
   new folder, supply their own USA disc, load a copied normal save, enable mods
   independently and test a solo/tag match plus a real second-controller turn.
   Record bugs as issues. Promote to stable only after the remaining release
   checks pass; do not claim support for stock or future executables yet.

## Release assets

Main player download:
`tag-duels-beta5-preview2-windows-candidate.zip`

Optional exact-companion module downloads:
`duel-options-preview2-companion-mods.zip`
`tag-duel-menu-preview2-companion-mods.zip`
`duel-portraits-preview2-companion-mods.zip`
`new-card-markers-preview2-companion-mods.zip`

Include SHA256SUMS.txt. A module-only download does not replace the required
companion executable. Tag Rewards is embedded in Tag Duels; do not publish the
old independent rewards object as a fifth module.

For creators/reviewers, publish the matching source in the repository and/or
attach the reviewed source archives. Keep source and executable versions
aligned. Every archive includes MOD_GUIDE.md; runtime archives include licenses
and dependency/provenance information. Keep original notices for the port and
bundled libraries. No game disc, extracted art/audio or personal save belongs
in a release. Review screenshots for personal names/paths before posting them.

## Links

- Upstream: https://github.com/Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled
- Proposal: https://github.com/Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled/issues
- Mod guide: https://github.com/Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled/blob/master/notes/modding.md
- Fork/contribution: https://docs.github.com/en/get-started/exploring-projects-on-github/contributing-to-a-project
- Releases: https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository

Remaining release limits are documented in README.md and ACCEPTANCE.md. A
successful local regression suite is not a substitute for the clean-machine,
physical-controller and broader GPU/HD checks.
