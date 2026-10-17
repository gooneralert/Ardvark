# Matcha UI reference artifacts

Everything in this folder exists so the menu's target look can be re-checked
at any time without the original chat screenshots.

* `MATCHA_UI_SPEC.md` — every measured value (geometry, colours, typography,
  rows, icons) plus the replication checklist.
* `matcha-aimbot-reference.svg` — a vector rebuild of the target Aimbot page
  (Ardvark branding). Open it in VS Code / a browser to compare.
* `generate_reference_svg.py` — regenerates the SVG (`python
  generate_reference_svg.py`). Change geometry here first, then in code.
* `screenshots/` — put the raw reference PNGs here
  (`ardvark-before.png`, `matcha-target.png`); they were never on disk, only
  in the chat, so this folder is intentionally empty.
