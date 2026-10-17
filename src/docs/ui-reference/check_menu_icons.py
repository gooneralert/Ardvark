#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# check_menu_icons.py
#
# Sanity-check for src/gui/menu_icons.h: mirrors the coordinate lists that are
# transcribed from generate_reference_svg.py ("sidebar icons") and reports the
# bounding box of each icon in the reference's own 16 x 16 unit box (half-extent
# 8, screen y-down). Everything must stay inside a sane envelope around the box
# and be centred on it, otherwise the rail would show the icon off-centre or
# clipped. Run from anywhere:
#
#     python check_menu_icons.py
# ---------------------------------------------------------------------------
import math

PI = math.pi

def bez(p0, p1, p2, p3, segs):
    out = []
    for i in range(segs + 1):
        t = i / segs
        u = 1 - t
        a, b, c, d = u**3, 3 * u * u * t, 3 * u * t * t, t**3
        out.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0],
                    a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))
    return out

def arc(c, r, a0, a1, segs):
    return [(c[0] + r * math.cos(a0 + (a1 - a0) * i / segs),
             c[1] + r * math.sin(a0 + (a1 - a0) * i / segs)) for i in range(segs + 1)]

def pts_spark():
    s = 8.0
    a = [(0, -s), (s * 0.32, -s * 0.32), (s, 0), (s * 0.32, s * 0.32)]
    b = [(0, s), (-s * 0.32, s * 0.32), (-s, 0), (-s * 0.32, -s * 0.32)]
    return a + b

def pts_person():
    return arc((0, -3), 3.1, 0, 2 * PI, 24) + arc((0, 6.4), 5.4, PI, 2 * PI, 18)

def pts_clock():
    return arc((0, 0), 6.6, 0, 2 * PI, 28) + [(0, 0), (0, -4.2), (0, 0), (3.4, 1.6)]

def pts_eye():
    return (bez((-7.4, 0), (-3, -5.4), (3, -5.4), (7.4, 0), 16)
            + bez((7.4, 0), (3, 5.4), (-3, 5.4), (-7.4, 0), 16)
            + arc((0, 0), 2.3, 0, 2 * PI, 20))

def pts_globe():
    return (arc((0, 0), 6.8, 0, 2 * PI, 32)
            + [(-6.8, 0), (6.8, 0)]
            + [(3.0 * math.cos(2 * PI * i / 32), 6.8 * math.sin(2 * PI * i / 32))
               for i in range(33)])

def pts_folder():
    r = 1.6
    return ([(-7.0, -5.6), (-2.8, -5.6), (-1.2, -3.4), (5.4, -3.4)]
            + arc((5.4, -1.8), r, -PI / 2, 0, 6)
            + [(7.0, -1.8), (7.0, 3.6)]
            + arc((5.4, 3.6), r, 0, PI / 2, 6)
            + [(5.4, 5.2), (-7.0, 5.2)]
            + arc((-7.0, 3.6), r, PI / 2, PI, 6)
            + [(-8.6, 3.6), (-8.6, -4.0)]
            + arc((-7.0, -4.0), r, PI, 3 * PI / 2, 6))

def pts_chat():
    r = 1.8
    return ([(-7.7, -5.4), (4.9, -5.4)]
            + arc((4.9, -3.6), r, -PI / 2, 0, 6)
            + [(6.7, -3.6), (6.7, 2.6)]
            + arc((4.9, 2.6), r, 0, PI / 2, 6)
            + [(4.9, 4.4), (-1.5, 4.4), (-5.1, 7.4), (-5.1, 4.4), (-7.7, 4.4)]
            + arc((-7.7, 2.6), r, PI / 2, PI, 6)
            + [(-9.5, 2.6), (-9.5, -3.6)]
            + arc((-7.7, -3.6), r, PI, 3 * PI / 2, 6))

def pts_sliders():
    out = []
    for dy in (-3.6, 1.2, 6.0):
        out += [(-6.5, dy), (6.5, dy)]
    for dx, dy in ((1.4, -3.6), (-2.6, 1.2), (3.4, 6.0)):
        out += arc((dx, dy), 2.0, 0, 2 * PI, 16)
    return out

ICONS = {
    "Spark":   pts_spark,
    "Person":  pts_person,
    "Clock":   pts_clock,
    "Eye":     pts_eye,
    "Globe":   pts_globe,
    "Folder":  pts_folder,
    "Chat":    pts_chat,
    "Sliders": pts_sliders,
}

# Expected bounding-box centres, in reference units. These are NOT all zero: the
# SVG draws the bubble and the slider rails slightly off the 16 px box centre,
# and the rail reproduces that verbatim (1:1 with the reference, not re-centred).
EXPECT = {
    "Spark":   (0.00, 0.00),
    "Person":  (0.00, 0.15),
    "Clock":   (0.00, 0.00),
    "Eye":     (0.00, 0.00),
    "Globe":   (0.00, 0.00),
    "Folder":  (-0.80, -0.20),
    "Chat":    (-1.40, 1.00),
    "Sliders": (0.00, 1.20),
}

LIMIT = 10.0     # widest glyph is the bubble at x = -9.5
TOL = 0.05       # transcription tolerance
bad = 0
print("%-8s %-24s %-16s %s" % ("icon", "bbox (x0,y0)-(x1,y1)", "centre", "expected"))
for name, fn in ICONS.items():
    pts = fn()
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    ex, ey = EXPECT[name]
    print("%-8s (%5.2f,%5.2f)-(%5.2f,%5.2f)  (%5.2f,%5.2f)  (%5.2f,%5.2f)"
          % (name, min(xs), min(ys), max(xs), max(ys), cx, cy, ex, ey))
    if not pts:
        print("  FAIL: no points"); bad += 1
        continue
    reach = max(abs(min(xs)), abs(max(xs)), abs(min(ys)), abs(max(ys)))
    if reach > LIMIT:
        print("  FAIL: escapes the %0.1f px envelope" % LIMIT); bad += 1
    if abs(cx - ex) > TOL or abs(cy - ey) > TOL:
        print("  FAIL: centre moved by (%.2f,%.2f)" % (cx - ex, cy - ey)); bad += 1

print("check_menu_icons: %s" % ("all icons match the reference geometry"
                                if not bad else "%d problems" % bad))
raise SystemExit(1 if bad else 0)
