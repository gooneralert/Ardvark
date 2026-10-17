#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# generate_reference_svg.py
#
# Rebuilds the "target" menu mock (the Matcha reference layout, Ardvark
# branding) as a vector SVG so the look can be re-checked at any time without
# the original screenshots. Run from anywhere:
#
#     python generate_reference_svg.py
#
# writes matcha-aimbot-reference.svg next to this file.
#
# Every number below is the measured value from the reference screenshots
# (903x718 desktop, menu card 853x702 at (25,8)). Keep this file in sync with
# the constants in src/src/gui/gui.cpp / src/src/gui/tabs/helpers.h.
# ---------------------------------------------------------------------------
import io
import os

# --- palette (measured) ----------------------------------------------------
CARD        = "#0f0f13"
CARD_EDGE   = "rgba(255,255,255,0.07)"
PLATE       = "rgba(255,255,255,0.022)"
PLATE_EDGE  = "rgba(255,255,255,0.04)"
HAIRLINE    = "rgba(255,255,255,0.06)"
TXT         = "#eceef4"
TXT_DIM     = "#b3b5c1"
TXT_SOFT    = "#7e8090"
TXT_VALUE   = "#d3d5df"
CTL_FILL    = "rgba(255,255,255,0.045)"
CTL_EDGE    = "rgba(255,255,255,0.09)"
TRACK_OFF   = "rgba(255,255,255,0.13)"
ACCENT_A    = "#8b5cf6"   # violet (accent[0..2] from Settings.h)
ACCENT_B    = "#e04ec8"   # magenta end of the gradient
GREEN       = "#35c85f"

FONT = "Segoe UI, Inter, Helvetica, Arial, sans-serif"

out = io.StringIO()
w = out.write

w('<?xml version="1.0" encoding="UTF-8"?>\n')
w('<svg xmlns="http://www.w3.org/2000/svg" width="903" height="718" '
  'viewBox="0 0 903 718" font-family="%s">\n' % FONT)
w('<defs>\n')
w('  <linearGradient id="accent" x1="0" y1="0" x2="1" y2="0">'
  '<stop offset="0" stop-color="%s"/><stop offset="1" stop-color="%s"/></linearGradient>\n'
  % (ACCENT_A, ACCENT_B))
w('  <linearGradient id="avatar" x1="0" y1="0" x2="1" y2="1">'
  '<stop offset="0" stop-color="#f0577f"/><stop offset="1" stop-color="#d0305c"/></linearGradient>\n')
w('  <linearGradient id="gamebg" x1="0" y1="0" x2="0" y2="1">'
  '<stop offset="0" stop-color="#2a2d3a"/><stop offset="1" stop-color="#141620"/></linearGradient>\n')
w('  <clipPath id="cardclip"><rect x="25" y="8" width="853" height="702" rx="20"/></clipPath>\n')
w('</defs>\n')

# --- desktop behind the card ----------------------------------------------
w('<rect width="903" height="718" fill="url(#gamebg)"/>\n')
w('<rect x="0" y="600" width="903" height="118" fill="rgba(0,0,0,0.35)"/>\n')

# --- menu card ------------------------------------------------------------
w('<rect x="25" y="8" width="853" height="702" rx="20" fill="%s" stroke="%s"/>\n' % (CARD, CARD_EDGE))
w('<g clip-path="url(#cardclip)">\n')
w('<line x1="200" y1="24" x2="200" y2="694" stroke="%s"/>\n' % HAIRLINE)

# --- brand row ------------------------------------------------------------
w('<text x="40" y="44" font-size="17" font-weight="700" fill="%s" letter-spacing="0.2">ardvark</text>\n' % TXT)
w('<rect x="115" y="28" width="42" height="18" rx="5" fill="url(#accent)"/>\n')
w('<text x="136" y="41" font-size="10" font-weight="700" fill="#ffffff" '
  'letter-spacing="1" text-anchor="middle">PRO</text>\n')

# --- config selector ------------------------------------------------------
w('<rect x="210" y="30" width="200" height="32" rx="8" fill="%s" stroke="%s"/>\n' % (CTL_FILL, CTL_EDGE))
w('<text x="222" y="50" font-size="13" fill="%s">No config</text>\n' % TXT_VALUE)
w('<path d="M387 45 l5 5 l5 -5" fill="none" stroke="#8f919d" stroke-width="1.6" '
  'stroke-linecap="round" stroke-linejoin="round"/>\n')


# --- sidebar icons (shape-matched line icons) -----------------------------
def group(x, y, label):
    w('<text x="%d" y="%d" font-size="10.5" fill="%s" letter-spacing="1.2">%s</text>\n'
      % (x, y, TXT_SOFT, label))

def icon(kind, cx, cy, col):
    s = 8.0
    st = 'fill="none" stroke="%s" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"' % col
    if kind == "spark":      # aimbot: 4-point sparkle
        w('<path d="M%.1f %.1f L%.1f %.1f L%.1f %.1f L%.1f %.1f Z" %s/>\n'
          % (cx, cy - s, cx + s * 0.32, cy - s * 0.32, cx + s, cy,
             cx + s * 0.32, cy + s * 0.32, st))
        w('<path d="M%.1f %.1f L%.1f %.1f L%.1f %.1f L%.1f %.1f Z" %s/>\n'
          % (cx, cy + s, cx - s * 0.32, cy + s * 0.32, cx - s, cy,
             cx - s * 0.32, cy - s * 0.32, st))
    elif kind == "person":   # silent / character
        w('<circle cx="%.1f" cy="%.1f" r="3.1" %s/>\n' % (cx, cy - 3.0, st))
        w('<path d="M%.1f %.1f a5.4 5.4 0 0 1 10.8 0" %s/>\n' % (cx - 5.4, cy + 6.4, st))
    elif kind == "clock":    # trigger
        w('<circle cx="%.1f" cy="%.1f" r="6.6" %s/>\n' % (cx, cy, st))
        w('<path d="M%.1f %.1f L%.1f %.1f M%.1f %.1f L%.1f %.1f" %s/>\n'
          % (cx, cy, cx, cy - 4.2, cx, cy, cx + 3.4, cy + 1.6, st))
    elif kind == "eye":      # visuals
        w('<path d="M%.1f %.1f C%.1f %.1f %.1f %.1f %.1f %.1f C%.1f %.1f %.1f %.1f %.1f %.1f Z" %s/>\n'
          % (cx - 7.4, cy, cx - 3.0, cy - 5.4, cx + 3.0, cy - 5.4, cx + 7.4, cy,
             cx + 3.0, cy + 5.4, cx - 3.0, cy + 5.4, cx - 7.4, cy, st))
        w('<circle cx="%.1f" cy="%.1f" r="2.3" %s/>\n' % (cx, cy, st))
    elif kind == "globe":    # world
        w('<circle cx="%.1f" cy="%.1f" r="6.8" %s/>\n' % (cx, cy, st))
        w('<path d="M%.1f %.1f h13.6" %s/>\n' % (cx - 6.8, cy, st))
        w('<ellipse cx="%.1f" cy="%.1f" rx="3.0" ry="6.8" %s/>\n' % (cx, cy, st))
    elif kind == "folder":   # npc
        w('<path d="M%.1f %.1f h4.2 l1.6 2.2 h6.6 a1.6 1.6 0 0 1 1.6 1.6 v5.4 '
          'a1.6 1.6 0 0 1 -1.6 1.6 h-12.4 a1.6 1.6 0 0 1 -1.6 -1.6 v-7.6 '
          'a1.6 1.6 0 0 1 1.6 -1.6 Z" %s/>\n' % (cx - 7.0, cy - 5.6, st))
    elif kind == "chat":     # teams
        w('<path d="M%.1f %.1f h12.6 a1.8 1.8 0 0 1 1.8 1.8 v6.2 a1.8 1.8 0 0 1 '
          '-1.8 1.8 h-6.4 l-3.6 3.0 v-3.0 h-2.6 a1.8 1.8 0 0 1 -1.8 -1.8 v-6.2 '
          'a1.8 1.8 0 0 1 1.8 -1.8 Z" %s/>\n' % (cx - 7.7, cy - 5.4, st))
    elif kind == "sliders":  # options
        for dy in (-3.6, 1.2, 6.0):
            w('<path d="M%.1f %.1f h13" %s/>\n' % (cx - 6.5, cy + dy, st))
        for dx, dy in ((1.4, -3.6), (-2.6, 1.2), (3.4, 6.0)):
            w('<circle cx="%.1f" cy="%.1f" r="2.0" fill="%s"/>\n' % (cx + dx, cy + dy, col))

NAV = [
    ("AIMBOT", None, None, 95, None),
    (None, "spark",   "Aimbot",   110, True),
    (None, "person",  "Silent",   148, False),
    (None, "clock",   "Trigger",  186, False),
    ("COMMON", None, None, 243, None),
    (None, "eye",     "Visuals",  254, False),
    (None, "globe",   "World",    292, False),
    (None, "person",  "Character", 330, False),
    (None, "folder",  "NPC",      368, False),
    (None, "chat",    "Teams",    406, False),
    (None, "sliders", "Options",  444, False),
]

for g, kind, label, top, active in NAV:
    if g:
        group(40, top, g)
        continue
    if active:
        w('<rect x="35" y="%d" width="150" height="34" rx="10" fill="rgba(255,255,255,0.06)" '
          'stroke="rgba(255,255,255,0.05)"/>\n' % top)
        w('<rect x="35" y="%d" width="3" height="18" rx="1.5" fill="url(#accent)"/>\n' % (top + 8))
    icon(kind, 47, top + 17, ACCENT_A if active else "#8f919d")
    w('<text x="62" y="%d" font-size="13" fill="%s">%s</text>\n'
      % (top + 22, TXT if active else TXT_DIM, label))

# --- page: control-column helpers ----------------------------------------
LEFT_RIGHT   = 516.0      # right edge of the left column
RIGHT_LEFT   = 538.0      # left edge of the right column
RIGHT_RIGHT  = 844.0      # right edge of the right column (content right pad 20)
CTL_INSET    = 14.0       # rows stop short of the plate edge
ROW_H        = 41.5       # row pitch

def plate(x0, x1, y0, y1):
    w('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="14" fill="%s" stroke="%s"/>\n'
      % (x0, y0, x1 - x0, y1 - y0, PLATE, PLATE_EDGE))

def rows_rule(x0, x1, y, n_rows):
    for i in range(n_rows - 1):
        yy = y + ROW_H * (i + 1)
        w('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s"/>\n'
          % (x0 + CTL_INSET, yy, x1 - CTL_INSET, yy, HAIRLINE))

def rlabel(x, y, text):
    w('<text x="%.1f" y="%.1f" font-size="13" fill="%s">%s</text>\n' % (x, y, TXT, text))

def dots(x, cy):
    for i in (-1, 0, 1):
        w('<circle cx="%.1f" cy="%.1f" r="1.5" fill="#aaacb8"/>\n' % (x, cy + i * 5))

def switch(right, cy, on, on_fill="url(#accent)"):
    x0, y0, sw, sh = right - 36.0, cy - 10.0, 36.0, 20.0
    fill = on_fill if on else TRACK_OFF
    w('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" fill="%s" stroke="%s"/>\n'
      % (x0, y0, sw, sh, sh * 0.5, fill, "rgba(255,255,255,0.12)" if on else "rgba(255,255,255,0.05)"))
    kx = x0 + sw - 10.0
    w('<circle cx="%.1f" cy="%.1f" r="7.0" fill="#f5f5f8"/>\n' % (kx, cy))

def slider(right, cy, t, box_text, accent=True):
    """value box + 90px track, exactly like a row control"""
    bx0 = right - 46.0
    w('<rect x="%.1f" y="%.1f" width="46" height="26" rx="8" fill="%s" stroke="%s"/>\n'
      % (bx0, cy - 13.0, CTL_FILL, CTL_EDGE))
    w('<text x="%.1f" y="%.1f" font-size="12" fill="%s" text-anchor="middle">%s</text>\n'
      % (bx0 + 23.0, cy + 4.0, TXT, box_text))
    tx0 = bx0 - CTL_INSET - 90.0
    w('<rect x="%.1f" y="%.1f" width="90" height="4" rx="2" fill="%s"/>\n' % (tx0, cy - 2.0, TRACK_OFF))
    if t > 0.0 and accent:
        w('<rect x="%.1f" y="%.1f" width="%.1f" height="4" rx="2" fill="url(#accent)"/>\n'
          % (tx0, cy - 2.0, 90.0 * t))
    w('<circle cx="%.1f" cy="%.1f" r="6.0" fill="#f5f5f8"/>\n' % (tx0 + 90.0 * t, cy))

def dropdown(right, cy, text):
    x0 = right - 128.0
    w('<rect x="%.1f" y="%.1f" width="128" height="30" rx="8" fill="%s" stroke="%s"/>\n'
      % (x0, cy - 15.0, CTL_FILL, CTL_EDGE))
    w('<text x="%.1f" y="%.1f" font-size="12.5" fill="%s">%s</text>\n' % (x0 + 13.0, cy + 4.0, TXT_DIM, text))
    w('<path d="M%.1f %.1f l5 5 l5 -5" fill="none" stroke="#8f919d" stroke-width="1.6" '
      'stroke-linecap="round" stroke-linejoin="round"/>\n' % (right - 23.0, cy - 2.0))

def chevron_r(right, cy):
    w('<path d="M%.1f %.1f l5 -5 l5 5" fill="none" stroke="#aaacb8" stroke-width="1.7" '
      'stroke-linecap="round" stroke-linejoin="round" transform="rotate(90 %.1f %.1f)"/>\n'
      % (right - 14.0, cy + 5.0, right - 9.0, cy))

def sec_header(x, y, text):
    w('<text x="%.1f" y="%.1f" font-size="10.5" fill="%s" letter-spacing="1.2">%s</text>\n'
      % (x, y, TXT_SOFT, text))

# --- MAIN (left column) ---------------------------------------------------
sec_header(215, 94, "MAIN")
plate(210, LEFT_RIGHT, 105, 401)
rows_rule(210, LEFT_RIGHT, 107, 7)

main_rows = [
    ("Enabled",       "on",  True),
    ("Team Check",    "on",  False),
    ("Visible Check", "off", False),
    ("Health Check",  "off", False),
    ("Sticky Aim",    "on",  True),
    ("Rage Method",   "off", True),
    ("Resolver",      "off", False),
]
for i, (label, state, more) in enumerate(main_rows):
    cy = 128.0 + i * ROW_H
    rlabel(215, cy + 4, label)
    if more:
        dots(LEFT_RIGHT - CTL_INSET - 36.0 - 20.0, cy)
    switch(LEFT_RIGHT - CTL_INSET, cy, state == "on")

# --- SELECTION (right column) --------------------------------------------
sec_header(538, 94, "SELECTION")
plate(538, RIGHT_RIGHT, 105, 276)
rows_rule(538, RIGHT_RIGHT, 107, 4)

rlabel(538 + CTL_INSET, 128 + 4, "Distance")
slider(RIGHT_RIGHT - CTL_INSET, 128, 0.40, "778")
rlabel(538 + CTL_INSET, 169.5 + 4, "Sensitivity")
slider(RIGHT_RIGHT - CTL_INSET, 169.5, 0.92, "2.00")
rlabel(538 + CTL_INSET, 211 + 4, "Hit Part")
dropdown(RIGHT_RIGHT - CTL_INSET, 211, "Head")
rlabel(538 + CTL_INSET, 252.5 + 4, "Aim Type")
dropdown(RIGHT_RIGHT - CTL_INSET, 252.5, "Mouse")

# --- OTHER (right column) -------------------------------------------------
sec_header(538, 300, "OTHER")
plate(538, RIGHT_RIGHT, 311, 449)
rows_rule(538, RIGHT_RIGHT, 313, 3)

for i, (label, kind) in enumerate((("Prediction", "chev"),
                                   ("Smoothing", "chev"),
                                   ("Field Of View", "green"))):
    cy = 334.0 + i * ROW_H
    rlabel(538 + CTL_INSET, cy + 4, label)
    if kind == "green":
        switch(RIGHT_RIGHT - CTL_INSET - 22.0, cy, True, on_fill=GREEN)
    chevron_r(RIGHT_RIGHT - CTL_INSET, cy)

# --- profile card ---------------------------------------------------------
w('<rect x="35" y="646" width="150" height="48" rx="14" fill="rgba(255,255,255,0.05)" stroke="%s"/>\n' % PLATE_EDGE)
w('<circle cx="67" cy="670" r="16" fill="url(#avatar)"/>\n')
w('<text x="67" y="675" font-size="14" font-weight="700" fill="#ffffff" '
  'text-anchor="middle">A</text>\n')
w('<text x="92" y="666" font-size="13" fill="%s">ardvark</text>\n' % TXT)
w('<text x="92" y="682" font-size="11.5" fill="#8a8c99">Lifetime</text>\n')
w('<path d="M170 665 l5 5 l-5 5" fill="none" stroke="#969aa6" stroke-width="1.7" '
  'stroke-linecap="round" stroke-linejoin="round"/>\n')

w('</g>\n</svg>\n')

out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "matcha-aimbot-reference.svg")
with open(out_path, "w", encoding="utf-8") as f:
    f.write(out.getvalue())
print("wrote %s (%d bytes)" % (out_path, len(out.getvalue())))

