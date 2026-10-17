# Ardvark menu — Matcha reference spec (stored so it can always be re-checked)

This folder is the **visual contract** for the menu. Ardvark's menu is a
1:1 replication of the Matcha menu; this document is the transcription of the
reference screenshots, and `matcha-aimbot-reference.svg` is a vector rebuild of
the target so the look can be re-inspected without the original PNGs.

| file | what it is |
|---|---|
| `matcha-aimbot-reference.svg` | vector rebuild of the target Aimbot page (Ardvark branding) |
| `generate_reference_svg.py` | script that regenerates the SVG — edit geometry here first |
| `MATCHA_UI_SPEC.md` | this file: every measured value + the delta checklist |
| `screenshots/` | drop the raw reference PNGs here (see below) |

## 0. Reference sources

Two screenshots were used. They are *not* committed (they only existed in the
chat), so if they are ever re-supplied, save them as:

* `screenshots/ardvark-before.png` — 840x599 — Ardvark's menu *before* the
  replication pass (blue `PRO` pill, blue switches, tab labels clipped to
  "Aimbo", extra `Customize` row, `Sticky Aim` after `Resolver`)
* `screenshots/matcha-target.png`  — 903x718 — the Matcha menu: the target

The measurements in this document were taken from `matcha-target.png`
(903x718 canvas, menu card 853x702 at (25, 8)) and are the numbers the code
follows. Where a value could not be read reliably off the screenshot it is
marked **[verify]**.

## 1. Global geometry

| thing | value |
|---|---|
| desktop canvas of the reference | 903 x 718 |
| menu card | 853 x 702 at (25, 8) → default size `900 x 720`, centred |
| card corner radius | 20 px (`menu_card_round`, kit card radius 26 at 900px scale) |
| card border | 1 px `rgba(255,255,255,0.07)`, no outer glow |
| card body padding | 20 px left/right, 18 px top/bottom (kit card padding) |
| sidebar rail width | 175 px (`menu_side_w` 176) — divider at x = 200 (abs) |
| rail/page divider | 1 px `rgba(255,255,255,0.06)`, 16 px inset top/bottom |
| content left pad | 10 px from the divider (content starts at x = 210 abs) |
| content right pad | 20 px (content right edge 833 local / 858 abs) |
| column gap | 22 px (`panel_gap`) |
| column width | (content width − 22) / 2 ≈ 306 px each |
| page top | `o.y + 58` (card local y = 76) |
| row pitch | 41.5 px → `k_row_h = 42` |
| row control inset | 14 px from the section-plate edge (`content_pad`) |


## 2. Colours (sampled)

| token | value | used for |
|---|---|---|
| card / panel | `#0f0f13` (glass panel, sampled backdrop darkened) | menu card |
| section plate | `rgba(255,255,255,0.022)` + `rgba(255,255,255,0.04)` edge | the box under each section's rows |
| hairline | `rgba(255,255,255,0.06)` | row separators, rail divider |
| text — primary | `#eceef4` | row labels, wordmark, avatar name |
| text — value | `#d3d5df` | dropdown / slider / config text |
| text — soft | `#7e8090` | section headers (`MAIN`, `SELECTION`, `OTHER`) and group labels (`AIMBOT`, `COMMON`) |
| text — dim | `#b3b5c1` | inactive rail labels |
| icon idle | `#8f919d` | rail icons, chevrons |
| icon active | accent (`#8b5cf6` violet), label `#eceef4` | selected rail row |
| control fill | `rgba(255,255,255,0.045)` idle, `0.07` hovered | config box, dropdowns, value boxes |
| control edge | `rgba(255,255,255,0.09)` idle, `0.12` hovered | same |
| track (off / empty) | `rgba(255,255,255,0.13)` | switch track off, slider rail |
| accent gradient | `#8b5cf6` → `#e04ec8` (violet → magenta) | every "on" switch and every slider fill |
| FOV green | `#35c85f` | `Field Of View` master switch only |
| avatar | `#f0577f` → `#d0305c` (pink) | profile avatar disc |
| popup panel | `rgba(14,15,18,0.985)` + `rgba(255,255,255,0.08)` edge | combo / config popups, profile menu |

Notes
* the accent is data-driven: `Settings::gui::accent` (`0.545, 0.361, 0.965`)
  feeds `Glass::SetAccent`, and the gradient's second stop is derived from it
  (`r*1.35, g*0.55, b*0.95`), so switching the accent keeps the Matcha
  gradient rather than flattening the controls to one colour;
* the `Field Of View` switch is the only green control in the whole menu;
* nothing in the menu uses pure black or pure white except thumb discs
  (`#f5f5f8`) and the small badge text (`#ffffff`).

## 3. Typography

The whole menu is drawn in **Inter** — Regular for labels/values, SemiBold for
headings and the wordmark. That is the face of the Matcha UI remake
(`src/gui/MatchaUI`): the two `.ttf` files it embeds are copied to
`src/gui/resources/fonts/Inter-Regular.ttf` + `Inter-SemiBold.ttf` (byte-identical
to the remake's arrays) and loaded by `fonts::load()` with the remake's
rasterization settings (pixel-snapped pen, 2x oversampling, light hinting +
forced autohint). The menu pushes them at `fonts::menu_size()` = 13 px, which is
the reference's row-label size; the rest of the overlay uses the same faces at
14 px (`gui.font` 9 / 10 = "inter regular" / "inter semibold").

| element | size | weight | notes |
|---|---|---|---|
| wordmark (`ardvark`) | 17 px | SemiBold (`fonts::menu_bold()`) | x = 40 abs, centred on card-local y = 31 |
| badge text (`PRO`) | 10 px | SemiBold | white, letter-spacing 1 px, pill 18 px tall |
| rail group label | 11 px | Regular | letter-spacing 1.3 px, upper-case, soft grey |
| rail row label | 14 px | Regular | `#b3b5c1` idle / `#eceef4` active |
| config selector text | 13 px | Regular | `#d3d5df` |
| section header | 10.5 px | Regular | letter-spacing 1.2 px, upper-case, soft grey |
| row label | 13 px | Regular | `#eceef4` |
| control text (dropdown / box) | 12–12.5 px | Regular | `#d3d5df` |
| avatar name | 13 px | Regular | `#eceef4` |
| avatar subtitle (`Lifetime`) | 11.5 px | Regular | `#8a8c99` |

Letter-spacing in the code is emulated by advancing the pen an extra
`1.2 px` per glyph (`section_header`, group labels, badge).

## 4. Element by element (absolute coords from the reference)

Card origin = (25, 8); card-local values in brackets.

### 4.1 Brand row
| | |
|---|---|
| wordmark | text left 40 (15), baseline 44 (36), 17 px ExtraBold `#eceef4` |
| badge pill | `PRO`: 42 x 18 at (115, 28) [90, 20], radius 5, **gradient** violet→magenta, text 10 px Bold white, letter-spacing 1 |
| gap | wordmark → pill 8 px |

The pill is the only gradient-filled *chrome* in the menu; it must be a
gradient (the pre-pass version was a flat accent rectangle).

### 4.2 Rail (sidebar)
| | |
|---|---|
| rail panel | its own rounded plate: fill `rgba(255,255,255,0.047)`, 1 px `rgba(255,255,255,0.063)` edge, radius 20, spanning card-local y 6 … `avail.y − 10` and ending 4 px short of the rail/page divider — this is the "curved, lighter" sidebar; the page body keeps the card's own tint |
| group label | `AIMBOT` baseline y = 95 [87], x = 40 [15]; `COMMON` baseline y = 243 [235] |
| row plate | 150 x 34 at x = 35 [10], radius 10, first row top y = 110 [102], pitch 38 |
| row plate (active) | fill `rgba(255,255,255,0.06)`, 1 px `rgba(255,255,255,0.05)` edge, plus a 3 x 19 accent bar at the plate's left edge, radius 1.5 |
| row plate (hover) | fill `rgba(255,255,255,0.035)`, animated (spring), no bar |
| icon | centre 13 px into the plate, **16 px box**, geometry, stroke 1.7 px; idle `#8f919d`, active accent |
| label | x = ix + 29, **14 px**, vertically centred |
| row order | AIMBOT → Aimbot, Silent, Trigger; COMMON → Visuals, World, Character, NPC, Teams, Options |
| sub-pages | `Players` / `Extras` nest under `Visuals` and only render while that group is the open one |
| **no** `Customize` row | the reference rail stops at `Options`; Ardvark's customize knobs live on the Options page |

**Deliberate deviation (density):** the measured values above (13 px label, 34 px
plate, 38 px pitch) were read off a 903x718 reference capture. On the real 1080p /
1440p overlay that size reads a full step too small, so the implemented rail is
`row_h` 38 / `sub_row_h` 33 / `row_gap` 3 (42 px pitch), 14 px labels, 11 px group
labels with 1.3 px tracking, 16 px icon box and a 21 px group-label line. The
*shapes*, colours, insets and the 10 px plate radius are unchanged; only the
type/density step is raised.

### 4.3 Config selector (page header)
| | |
|---|---|
| box | 200 x 32 at (210, 30) [185, 22], radius 8 |
| fill / edge | `rgba(255,255,255,0.045)` / `0.09` (hover `0.065` / `0.12`) |
| text | x + 12, 13 px `#d3d5df`; label is the loaded config name, or `No config` when none exists |
| chevron | down-chevron, 5 px half-width, right edge −14, `#8f919d` |
| popup | below the box, +4 px, radius 10, dark panel, row height `text + 8` |

### 4.4 Section header
* upper-case, 10.5 px, `#7e8090`, letter-spacing 1.2;
* sits 12 px above its plate, i.e. header baseline at plate_top − 11;
* **no** rule/hairline under the label.

### 4.5 Section plate
* auto-height rounded box behind a section's rows: radius 14,
  fill `rgba(255,255,255,0.022)`, 1 px `rgba(255,255,255,0.04)` edge;
* it is drawn by the **ImGui pass** (a flat wash inside a reserved draw-list
  channel), *not* as a glass primitive: a second glass layer on top of the card
  stacks blur + tint, which made a page read as a different colour than the rail
  next to it. Because the glass pass runs before all ImGui geometry, a glass
  hover plate would also be painted over by it - so row hovers that must show
  through use the ImGui wash path (`rows_use_imgui_chrome`);
* padding: 14 px left/right (`content_pad`), 4 px top/bottom, rows fill it;
* gap after a plate (before the next header) 12 px, plus 10 px dummy.

### 4.6 Rows
* pitch 41.5 px (`k_row_h = 42`), full width of the plate's inner box;
* label left at the plate's 14 px inset, 13 px `#eceef4`, vertically centred;
* control column is right-aligned at the plate inset, width = the control's;
* separator: 1 px `rgba(255,255,255,0.06)` at each row bottom, inset 14 px from
  both plate edges (drawn by `row_end`);
* hover: rounded 10 px plate behind the row at 50 % of a `rgba(255,255,255,…)`
  spring fade (glass `Thin` material); rows reveal with a fade + 10 px rise.

### 4.7 Switch (`k_switch_w` x `k_switch_h` = 36 x 20)
* track radius 10; **on** = accent gradient left→right, 1 px `0.12` edge;
  **off** = `rgba(255,255,255,0.13)` + 1 px `0.05` edge;
* thumb: 14 px disc `#f5f5f8`, always at the **right** end (3 px inset) — the
  track colour alone carries on/off, exactly as in the reference;
* thumb has a 1 px soft drop shadow;
* the `Field Of View` switch uses the same shape with the green fill.

### 4.8 Slider (`k_track_w` x 90, value box `k_box_w` x `k_ctl_h` = 46 x 26)
* value box: radius 8, fill `rgba(255,255,255,0.045)` (hover `0.065`), edge
  `0.09` (`0.12` hover), value 12 px `#d3d5df` centred;
* track: 90 x 4, radius 2, empty `rgba(255,255,255,0.13)`;
* fill: accent **gradient** across the filled part (not a flat colour);
* thumb: 12 px `#f5f5f8` disc, 1 px soft shadow, grows 1.12x hover / 1.3x drag;
* gap between track and value box 14 px.

### 4.9 Dropdown (`k_drop_w` x `k_drop_h` = 128 x 30)
* radius 8, fill `rgba(255,255,255,0.07)` (`0.09` hover/open), edge `0.10`
  (`0.12`);
* value text x + 13, 12.5 px `#d3d5df`, left aligned (not centred);
* chevron down at right edge −13;
* popup: same styling as the config popup, option rows `text + 8` tall.

### 4.10 "…" marker (rows that own sub-options)
* three 1.5 px dots, 5 px apart, colour `#aaacb8`;
* sits 20 px left of the control column;
* clicking the marker toggles the sub-options, clicking elsewhere works the
  control (both `row_checkbox_expand`/`row_checkbox_more` and
  `row_combo_more` behave this way).

### 4.11 Expandable row (`Prediction`, `Smoothing`, `Field Of View`)
* label left, chevron **right** (`>` when closed, rotated when open);
* optional master switch drawn to the left of the chevron;
* whole row toggles when there is no master switch; with a master switch the
  switch toggles the feature and the rest of the row expands.

### 4.12 Profile card (bottom of the rail)
| | |
|---|---|
| plate | 150 x 48 at (35, 646) [10, 638], radius 14, fill `rgba(255,255,255,0.05)` |
| avatar | 32 px disc at (67, 670), gradient pink `#f0577f → #d0305c`, 14 px Bold white initial |
| name | x = 92, baseline 666, 13 px `#eceef4` |
| subtitle | `Lifetime`, x = 92, baseline 682, 11.5 px `#8a8c99` |
| chevron | `>` at x = 170, vertically centred, `#969aa6` |
| click | opens the profile menu popup (menu key, style, design, glass/blur/dark background, snow) |

## 5. Page content (row-for-row)

### 5.1 Aimbot (the page in the reference screenshot)

`MAIN` (left column) — 7 rows, in this exact order:

| # | row | control | "…" |
|---|---|---|---|
| 1 | Enabled | switch, **on** | yes (key + distance check) |
| 2 | Team Check | switch, **on** | no |
| 3 | Visible Check | switch, off | no |
| 4 | Health Check | switch, off | no |
| 5 | Sticky Aim | switch, **on** | yes (sticky fov) |
| 6 | Rage Method | switch, off | yes |
| 7 | Resolver | switch, off | no |

`SELECTION` (right column) — exactly 4 rows:

| # | row | control | value in the reference |
|---|---|---|---|
| 1 | Distance | slider + value box | `778` |
| 2 | Sensitivity | slider + value box | `2.00` |
| 3 | Hit Part | dropdown | `Head` |
| 4 | Aim Type | dropdown | `Mouse` |

`OTHER` (right column, below SELECTION) — exactly 3 rows, all expandable
(chevron right), no switches except FOV:

| # | row | control |
|---|---|---|
| 1 | Prediction | `>` |
| 2 | Smoothing | `>` |
| 3 | Field Of View | **green switch (on)** + `>` |

> Row-order note: the pre-pass menu had `Sticky Aim` *after* `Resolver`; the
> reference puts it at position 5, directly under `Health Check`.

### 5.2 Every other page

The other pages follow the same two-column `HEADER → rows` grammar with
upper-case headers. The reference's own header names are kept
(`MAIN`, `SELECTION`, `OTHER`, `CHECKS`, `TEAMS`, …) and no page adds a row
type that does not exist in section 4 (switch / slider / dropdown / color /
keybind / `…` / expandable / note).

## 6. Rail icon map

The rail icons are drawn as **geometry**, not as a font glyph (`gui/menu_icons.h`,
geometry lifted from `generate_reference_svg.py`). The icon font the LiquidUI kit
ships is optional (it only exists when the vendored `data/fa` folder is found next
to the exe); when it is missing, a glyph-based row renders with *no* icon at all —
`NPC` and `Teams` did exactly that. Geometry also keeps the 16 px box, 1.7 px
round-capped stroke and the exact shapes below at every size.

| page | icon | `menu_icons::Kind` |
|---|---|---|
| Aimbot | 4-point sparkle | `Spark` |
| Silent | person outline | `Person` |
| Trigger | clock / timer | `Clock` |
| Visuals | eye | `Eye` |
| Players (sub) | person outline (indented) | `Person` |
| Extras (sub) | sparkle (indented) | `Spark` |
| World | globe | `Globe` |
| Character | person outline | `Person` |
| NPC | folder | `Folder` |
| Teams | speech bubble | `Chat` |
| Options | sliders | `Sliders` |

Row chevrons (`profile card`, `config selector`, dropdowns) come from the same
header (`menu_icons::Chevron`) for the same reason: the reference draws them as a
10 px wide / 5 px tall stroke (`M387 45 l5 5 l5 -5`), not a glyph.

The pre-pass rail used `Crosshairs` (Aimbot), `Bolt` (Trigger), `Box` (NPC),
`Flag` (Teams) and still listed `Customize` (`Paintbrush`) — all four icons and
the extra row were replaced.

## 7. Other surfaces in the same style

| surface | notes |
|---|---|
| navbar / launcher pill | same glass pill, 28 px tall; icon set unchanged; hover plate radius 8; active dot 1.8 px under the icon |
| Lua / Players / Explorer / Servers / ESP-preview windows | draw through `Glass::draw_header` with the same tint + hairline, 8 px radius |
| combo / config popups | popup panel `rgba(14,15,18,0.985)`, 1 px `rgba(255,255,255,0.08)`, radius 10, hover row `rgba(255,255,255,0.10)` |
| color popup | `widgets::color_picker` inside the same popup chrome |
| profile menu | solid dark popup (272 px), rows use ImGui chrome (`rows_use_imgui_chrome(true)`) because a glass plate would render behind the popup |

## 8. Replication checklist (what the pass changed)

Tick marks: [x] done in code, [~] deliberately different (documented), [?] to
confirm against a fresh screenshot.

* [x] rail row set: `Customize` removed from the rail (knobs moved onto the
  Options page) so the rail is `Aimbot … Options`, like the reference
* [x] rail icons swapped to the reference's shapes (see section 6)
* [x] rail metrics: plate 150 x 34, pitch 38, radius 10, active plate fill +
  accent bar, icon at x = 22, label at x = 37
* [x] group labels: 10.5 px, letter-spacing 1.2, soft grey, tighter gap
* [x] brand pill is now a violet→magenta **gradient** with a 1 px letter-spaced
  white label and a 18 px pill height
* [x] config selector: 200 x 32, radius 8, softer fill/edge, `No config` empty
  state, chevron at −14
* [x] rows: pitch 42, hairline alpha 0.06, hover plate 10 px radius
* [x] switch: 36 x 20, 14 px thumb always right, gradient when on
* [x] slider: 90 px track, gradient fill, 46 x 26 value box
* [x] dropdown: 128 x 30, left-aligned value, radius 8
* [x] `…` marker 20 px left of the control column
* [x] expandable rows: chevron right, optional master switch, **green** for FOV
* [x] Aimbot `MAIN` order fixed: `Sticky Aim` now row 5 (before `Rage Method`)
* [x] `Visible Check` no longer draws a `…` marker (the reference has none);
  the sub-options stay reachable through the marker on its parent rows
* [x] profile card: initial-letter avatar on a pink gradient, 150 x 48 plate,
  name + `Lifetime` + chevron at the reference's positions
* [?] exact rail/plate pixel positions after a DPI change (the reference is a
  1.0 DPI capture)

## 9. How this folder is validated

1. `python generate_reference_svg.py` regenerates the vector target.
2. Build the external (`msbuild jewsploit.sln /m /p:Configuration=Release
   /p:Platform=x64 /v:minimal` from `src/`).
3. Open the overlay's menu and compare against the SVG side by side; the
   screenshots above covered the Aimbot page at 1.0 DPI with no config loaded.

Any change to the geometry constants must be made in **both** places:
`src/src/gui/tabs/helpers.h` (row/control metrics) plus `src/src/gui/gui.cpp`
(card, rail, config selector, profile card) and this document.



