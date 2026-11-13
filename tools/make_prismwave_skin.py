#!/usr/bin/env python3
"""Generate Prism Wave's dark wavetable-synth skin (hand-written Plugin Skins), plus previews.

Original NOXED artwork in the spirit of modern software wavetable synths: dark panels,
arc knobs, a 3D wavetable display that follows the oscillator position, a live filter
curve, an ADSR display and a modulation matrix. No third-party artwork, logos or fonts
are copied; panel text uses Inter (OFL) when installed, otherwise Arial/DejaVu.

Requires Pillow and numpy to regenerate artwork. The normal SDK build copies skins/
verbatim and does not need them.

    python tools/make_prismwave_skin.py --params "plugins/Prism Wave/build/params.json"
"""
import argparse, json, math, struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import gen_ui as ui

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'plugins' / 'Prism Wave'
SKINS = PROJECT / 'skins'
W, H = 1280, 628
S = 3  # supersampling for artwork

# ---- palette -------------------------------------------------------------------------------
BG = '#12141a'
PANEL = '#1b1e26'
PANEL_HI = '#222631'
EDGE = '#2d3240'
FIELD = '#0e1015'
DISPLAY = '#0b0d12'
GRID = '#1a1e28'
TEXT = '#d5dbe8'
MUTED = '#7c8496'
DIM = '#4a5162'
ACCENT = '#46c8f0'      # cyan
ACCENT2 = '#a77bff'     # violet (prism gradient partner)
TRACK = '#2b303c'
A_FOCUS = 'ff46c8f0'
VALUE_COL = 'ffbfeaf8'
FIELD_TEXT = 'ffe4e9f2'

# ---- geometry ------------------------------------------------------------------------------
KN = 64                      # knob film-strip size
CW, CH = 140, 118            # knob control: knob, name (art), value label
FRAMES = 101                 # knob frames
DISP_W, DISP_H = 598, 186    # wavetable / filter display
DISP_FRAMES = 21
ENV_H = 200
BAR_W, BAR_H = 360, 40       # matrix amount bar
SMALL = 34                   # header master knob

TABS = ['OSC', 'FILTER', 'ENV', 'LFO', 'MATRIX']

# Parameter indices (fixed; must match PrismWave.cpp order).
P = dict(master=0, a_table=1, a_pos=2, a_level=3, a_oct=4, a_uni=5, a_det=6,
         b_table=7, b_pos=8, b_level=9, b_oct=10, b_semi=11, b_uni=12, b_det=13,
         sub=14, noise=15, width=16, cutoff=17, res=18, mode=19, envamt=20, keytrack=21,
         amp_a=22, amp_d=23, amp_s=24, amp_r=25, e2_a=26, e2_d=27, e2_s=28, e2_r=29,
         l1_shape=30, l1_rate=31, l1_sync=32, l1_div=33, l2_shape=34, l2_rate=35, l2_sync=36, l2_div=37,
         drive=50, chorus=51, delay=52, delay_div=53, feedback=54, bend=55)
MOD = [(38 + 3 * i, 39 + 3 * i, 40 + 3 * i) for i in range(4)]  # source, target, amount

# ---- drawing helpers -----------------------------------------------------------------------
_fonts = {}


def font(size, weight='regular'):
    key = (size, weight)
    if key in _fonts:
        return _fonts[key]
    names = {'regular': ['Inter-Regular.otf', 'arial.ttf', 'DejaVuSans.ttf'],
             'medium': ['Inter-Medium.otf', 'arial.ttf', 'DejaVuSans.ttf'],
             'semibold': ['Inter-SemiBold.otf', 'arialbd.ttf', 'DejaVuSans-Bold.ttf'],
             'bold': ['Inter-Bold.otf', 'arialbd.ttf', 'DejaVuSans-Bold.ttf'],
             'black': ['Inter-Black.otf', 'ariblk.ttf', 'DejaVuSans-Bold.ttf']}[weight]
    dirs = [Path('/usr/share/fonts/opentype/inter'), Path('C:/Windows/Fonts'),
            Path('/usr/share/fonts/truetype/dejavu'), Path('/Library/Fonts')]
    for n in names:
        for d in dirs:
            if (d / n).exists():
                _fonts[key] = ImageFont.truetype(str(d / n), round(size * S))
                return _fonts[key]
    _fonts[key] = ImageFont.load_default()
    return _fonts[key]


def sc(v):
    return tuple(round(x * S) for x in v)


def rrect(d, b, fill, outline=None, width=1, radius=0):
    x0, y0, x1, y1 = b
    if radius:
        d.rounded_rectangle(sc((x0, y0, x1, y1)), radius=round(radius * S), fill=fill, outline=outline,
                            width=round(width * S) if outline else 0)
    else:
        d.rectangle(sc((x0, y0, x1, y1)), fill=fill, outline=outline, width=round(width * S) if outline else 0)


def text(d, xy, msg, size=14, weight='semibold', colour=TEXT, anchor='mm', spacing=0.0):
    if spacing:
        # Letter-spaced caps, drawn character by character.
        f = font(size, weight)
        widths = [d.textlength(c, font=f) for c in msg]
        total = sum(widths) + spacing * S * (len(msg) - 1)
        x, y = xy[0] * S, xy[1] * S
        if anchor[0] == 'm':
            x -= total / 2
        elif anchor[0] == 'r':
            x -= total
        for c, w in zip(msg, widths):
            d.text((x, y), c, font=f, fill=colour, anchor='l' + anchor[1])
            x += w + spacing * S
        return
    d.text(sc(xy), msg, font=font(size, weight), fill=colour, anchor=anchor)


def line(d, pts, colour, width=1.0):
    d.line([sc(p) for p in pts], fill=colour, width=max(1, round(width * S)), joint='curve')


def hex_rgb(h, a=255):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


def mix(c1, c2, t):
    a, b = hex_rgb(c1), hex_rgb(c2)
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def canvas(w, h, colour=(0, 0, 0, 0)):
    im = Image.new('RGBA', (w * S, h * S), colour)
    return im, ImageDraw.Draw(im)


def finish(im, w, h):
    return im.resize((w, h), Image.Resampling.LANCZOS)


# ---- panels ------------------------------------------------------------------------------
def panel_box(d, b, title, led=True, subtitle=None):
    x0, y0, x1, y1 = b
    rrect(d, (x0, y0, x1, y1), PANEL, EDGE, 1, 8)
    rrect(d, (x0 + 1, y0 + 1, x1 - 1, y0 + 38), PANEL_HI, None, 0, 7)
    rrect(d, (x0 + 1, y0 + 30, x1 - 1, y0 + 38), PANEL_HI)
    line(d, [(x0 + 1, y0 + 38.5), (x1 - 1, y0 + 38.5)], EDGE, 1)
    tx = x0 + 16
    if led:
        d.ellipse(sc((x0 + 14, y0 + 14, x0 + 24, y0 + 24)), fill=ACCENT)
        d.ellipse(sc((x0 + 16.5, y0 + 16.5, x0 + 19.5, y0 + 19.5)), fill='#c8f3ff')
        tx = x0 + 34
    text(d, (tx, y0 + 19.5), title, 15, 'bold', TEXT, 'lm', spacing=1.4)
    if subtitle:
        w = d.textlength(title, font=font(15, 'bold')) / S + 1.4 * len(title)
        text(d, (tx + w + 12, y0 + 19.5), subtitle, 12, 'medium', MUTED, 'lm', spacing=0.8)


def display_bg(d, b, grid=True, cols=8, rows=4):
    x0, y0, x1, y1 = b
    rrect(d, (x0, y0, x1, y1), DISPLAY, '#262b37', 1, 6)
    if grid:
        for i in range(1, cols):
            x = x0 + (x1 - x0) * i / cols
            line(d, [(x, y0 + 2), (x, y1 - 2)], GRID, 1)
        for j in range(1, rows):
            y = y0 + (y1 - y0) * j / rows
            line(d, [(x0 + 2, y), (x1 - 2, y)], GRID, 1)


def field_img(w, h):
    # The field box is part of the control (not the panel), so it hides with the control.
    im, d = canvas(w, h)
    rrect(d, (0.5, 0.5, w - 0.5, h - 0.5), FIELD, '#2a2f3b', 1, 5)
    cx, cy = w - 16, h / 2
    d.polygon([sc((cx - 5, cy - 2.5)), sc((cx + 5, cy - 2.5)), sc((cx, cy + 3.5))], fill=MUTED)
    return finish(im, w, h)


def field_bg(d, b, label=None):
    x0, y0, x1, y1 = b
    if label:
        text(d, ((x0 + x1) / 2, y1 + 16), label, 12, 'semibold', MUTED, 'mm', spacing=1.0)


def knob_name(d, x, y, name):
    text(d, (x + CW / 2, y + 76), name, 12, 'semibold', MUTED, 'mm', spacing=1.0)


def prism_logo(d, x, y):
    # A small prism: a triangle with a cyan->violet edge and a split ray. Original mark.
    tri = [(x, y + 26), (x + 15, y), (x + 30, y + 26)]
    d.polygon([sc(p) for p in tri], fill='#1f2430')
    for i in range(3):
        a, b = tri[i], tri[(i + 1) % 3]
        line(d, [a, b], mix(ACCENT, ACCENT2, i / 2), 2)
    line(d, [(x - 10, y + 15), (x + 9, y + 13)], '#d5dbe8', 1.4)
    for i, c in enumerate([ACCENT, '#7aa6ff', ACCENT2]):
        line(d, [(x + 21, y + 12 + i * 2), (x + 40, y + 6 + i * 7)], mix(c, c, 0), 1.4)


def top_strip(d, page):
    rrect(d, (0, 0, W, 42), '#0d0f14')
    line(d, [(0, 42), (W, 42)], '#232835', 1)
    prism_logo(d, 22, 8)
    text(d, (76, 21), 'PRISM WAVE', 19, 'black', TEXT, 'lm', spacing=2.2)
    text(d, (252, 22), 'NOXED', 12, 'semibold', MUTED, 'lm', spacing=2.4)
    # Section breadcrumb (decorative; the MPC function keys switch pages).
    x = 420
    for i, name in enumerate(TABS):
        f = font(13, 'bold')
        w = d.textlength(name, font=f) / S + 1.2 * (len(name) - 1)
        if i == page:
            rrect(d, (x - 12, 9, x + w + 12, 33), '#1f2a36', '#2f5568', 1, 5)
            text(d, (x, 21.5), name, 13, 'bold', ACCENT, 'lm', spacing=1.2)
        else:
            text(d, (x, 21.5), name, 13, 'bold', DIM, 'lm', spacing=1.2)
        x += w + 34
    text(d, (1138, 21.5), 'MASTER', 11, 'bold', MUTED, 'rm', spacing=1.2)


# ---- knob film strips ----------------------------------------------------------------------
def knob_frame(v, size, bipolar, ring=4.0):
    im, d = canvas(size, size)
    c = size / 2
    r_arc = size / 2 - ring / 2 - 1.5
    start, sweep = 135, 270
    box = sc((c - r_arc, c - r_arc, c + r_arc, c + r_arc))
    d.arc(box, start, start + sweep, fill=TRACK, width=round(ring * S))
    if bipolar:
        a0, a1 = 270, start + sweep * v
        if a1 < a0:
            a0, a1 = a1, a0
    else:
        a0, a1 = start, start + sweep * v
    if a1 - a0 > 0.4:
        # Two-tone arc: cyan near the start, drifting toward violet at the far end.
        steps = max(2, int((a1 - a0) / 3))
        for k in range(steps):
            t0 = a0 + (a1 - a0) * k / steps
            t1 = a0 + (a1 - a0) * (k + 1) / steps + 0.6
            frac = (t0 - start) / sweep
            d.arc(box, t0, t1, fill=mix(ACCENT, ACCENT2, frac * 0.75), width=round(ring * S))
    # Body.
    rb = size / 2 - ring - 5
    d.ellipse(sc((c - rb - 1, c - rb + 1.5, c + rb + 1, c + rb + 3)), fill=(0, 0, 0, 120))
    for k in range(12):
        t = k / 11
        rr = rb * (1 - t * 0.08)
        col = mix('#3a4050', '#262a35', t)
        d.ellipse(sc((c - rr, c - rr - (1 - t) * 0.6, c + rr, c + rr - (1 - t) * 0.6)), fill=col)
    d.ellipse(sc((c - rb, c - rb, c + rb, c + rb)), outline='#454c5d', width=S)
    a = math.radians(start + sweep * v)
    line(d, [(c + math.cos(a) * rb * 0.25, c + math.sin(a) * rb * 0.25),
             (c + math.cos(a) * rb * 0.86, c + math.sin(a) * rb * 0.86)], '#eef3fb', 2.4 if size > 40 else 1.8)
    return finish(im, size, size)


def strip(frames_fn, n, w, h, path):
    out = Image.new('RGBA', (w, h * n))
    for i in range(n):
        out.paste(frames_fn(i / (n - 1)), (0, i * h))
    out.save(path, optimize=True)


def bar_frame(v):
    im, d = canvas(BAR_W, BAR_H)
    rrect(d, (0, 6, BAR_W, BAR_H - 6), FIELD, '#2a2f3b', 1, 5)
    c = BAR_W / 2
    x = BAR_W * v
    if abs(x - c) > 0.5:
        lo, hi = sorted((c, x))
        col = ACCENT if v >= 0.5 else ACCENT2
        rrect(d, (lo, 9, hi, BAR_H - 9), mix(col, DISPLAY, 0.55))
        line(d, [(x, 8), (x, BAR_H - 8)], col, 2.5)
    line(d, [(c, 4), (c, BAR_H - 4)], '#5a6274', 1)
    return finish(im, BAR_W, BAR_H)


def toggle_img(on):
    im, d = canvas(58, 28)
    rrect(d, (1, 1, 57, 27), '#1d3a47' if on else FIELD, ACCENT if on else '#353b48', 1.2, 13.5)
    cx = 43 if on else 15
    d.ellipse(sc((cx - 9.5, 4.5, cx + 9.5, 23.5)), fill=ACCENT if on else '#59606f')
    return finish(im, 58, 28)


def seg_img(on):
    im, d = canvas(66, 28)
    rrect(d, (1, 1, 65, 27), '#1d3a47' if on else FIELD, ACCENT if on else '#2f3441', 1.2, 5)
    return finish(im, 66, 28)


# ---- wavetables (mirrors src/Wavetables.h for the built-in banks) -------------------------
N = 2048


def builtin_bank(b):
    p = np.arange(N) / N
    frames = []
    for f in range(8):
        t = f / 7
        if b == 0:
            shapes = [np.sin(2 * np.pi * p), 1 - 4 * np.abs(p - .5), 2 * p - 1, np.where(p < .5, 1.0, -1.0)]
            q = t * 3
            a = min(2, int(q))
            y = shapes[a] + (q - a) * (shapes[a + 1] - shapes[a])
        elif b == 1:
            y = np.sin(2 * np.pi * p + (t * 5) * np.sin(2 * np.pi * p * 2))
        elif b == 2:
            y = np.zeros(N)
            for h in range(1, 25):
                center = 2 + t * 17
                dd = (h - center) / 3
                y += np.sin(2 * np.pi * p * h) * math.exp(-dd * dd) / math.sqrt(h)
        else:
            y = np.zeros(N)
            for h in range(1, 21):
                y += np.sin(2 * np.pi * p * h) * (math.sin(h * 1.71 + t * 5) * .5 + .5) / h
        frames.append(y)
    return normalise(np.array(frames))


def normalise(frames):
    out = []
    for y in frames:
        y = y - y.mean()
        pk = np.abs(y).max()
        out.append(y / pk if pk > 1e-6 else y)
    return np.array(out)


def read_wav_frames(path):
    data = path.read_bytes()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        return None
    i, fmt, raw = 12, None, None
    while i + 8 <= len(data):
        cid, sz = data[i:i + 4], struct.unpack('<I', data[i + 4:i + 8])[0]
        body = data[i + 8:i + 8 + sz]
        if cid == b'fmt ':
            fmt = struct.unpack('<HHIIHH', body[:16])
        elif cid == b'data':
            raw = body
        i += 8 + sz + (sz & 1)
    if not fmt or raw is None:
        return None
    tag, ch, _, _, _, bits = fmt
    if tag == 3 and bits == 32:
        x = np.frombuffer(raw, '<f4').astype(float)
    elif bits == 16:
        x = np.frombuffer(raw, '<i2') / 32768.0
    elif bits == 24:
        b = np.frombuffer(raw[:len(raw) // 3 * 3], np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8) / 8388608.0
    elif bits == 32:
        x = np.frombuffer(raw, '<i4') / 2147483648.0
    else:
        return None
    x = x.reshape(-1, ch).mean(axis=1)
    if len(x) < N or len(x) % N:
        return None
    return normalise(x.reshape(-1, N)[:64])


def interp_frame(frames, pos):
    fp = pos * (len(frames) - 1)
    f = int(fp)
    f2 = min(f + 1, len(frames) - 1)
    return frames[f] + (fp - f) * (frames[f2] - frames[f])


# ---- 3D wavetable display -----------------------------------------------------------------
def wt_frame(frames, pos, generic=False):
    im, d = canvas(DISP_W, DISP_H)
    ghosts = 18
    ox, oy = 26, 156          # front baseline origin
    span = 440                # width of one cycle
    dx, dy = 104, -92         # depth offset (back plane)
    amp = 34
    pts_n = 160
    idx = np.linspace(0, N - 1, pts_n).astype(int)

    def poly(y, depth):
        x0 = ox + dx * depth
        y0 = oy + dy * depth
        return [(x0 + span * k / (pts_n - 1), y0 - amp * y[i]) for k, i in enumerate(idx)]

    # Back to front ghosts.
    for g in range(ghosts - 1, -1, -1):
        t = g / (ghosts - 1)
        y = np.zeros(N) if generic else interp_frame(frames, t)
        near = 1 - abs(t - pos)
        col = mix('#1f2633', '#3a4a5e', max(0.0, near) ** 3)
        line(d, poly(y, 1 - t), col, 1.1)
    # Current frame: filled, glowing.
    y = (np.sin(2 * np.pi * np.arange(N) / N) * 0) if generic else interp_frame(frames, pos)
    pts = poly(y, 1 - pos)
    base = oy + dy * (1 - pos)
    # Translucent 'slice' from the current frame down to its zero line.
    fill_layer, fd = canvas(DISP_W, DISP_H)
    for k in range(len(pts) - 1):
        (xa, ya), (xb, yb) = pts[k], pts[k + 1]
        fd.polygon([sc(q) for q in [(xa, base), (xa, ya), (xb, yb), (xb, base)]], fill=hex_rgb(ACCENT, 34))
    im.alpha_composite(fill_layer)
    glow, gd = canvas(DISP_W, DISP_H)
    line(gd, pts, hex_rgb(ACCENT, 150), 6)
    glow = glow.filter(ImageFilter.GaussianBlur(4 * S))
    im.alpha_composite(glow)
    line(d, pts, ACCENT, 2.2)
    line(d, [(pts[0][0], base), (pts[-1][0], base)], '#2c4f60', 1)
    if generic:
        text(d, (DISP_W / 2 + 30, 34), 'USER WAVETABLE', 13, 'bold', MUTED, 'mm', spacing=2)
        text(d, (DISP_W / 2 + 30, 54), 'imported slot, shape not previewed', 11, 'medium', DIM, 'mm')
    # Position ruler at the bottom right.
    rx0, rx1, ry = 500, 586, 14
    line(d, [(rx0, ry), (rx1, ry)], '#2a3140', 3)
    line(d, [(rx0, ry), (rx0 + (rx1 - rx0) * pos, ry)], ACCENT, 3)
    text(d, (rx0 - 8, ry + 0.5), 'POS', 10, 'bold', DIM, 'rm', spacing=1)
    return finish(im, DISP_W, DISP_H)


# ---- filter curve -------------------------------------------------------------------------
def filter_frame(mode, v, res=0.15):
    im, d = canvas(DISP_W, DISP_H)
    fc = 30 * (600 ** v)
    q = 0.55 + res * 6
    freqs = np.logspace(math.log10(20), math.log10(20000), 300)
    s = 1j * freqs / fc
    den = s * s + s / q + 1
    num = {0: 1, 1: s / q, 2: s * s, 3: s * s + 1}[mode]
    mag = 20 * np.log10(np.maximum(np.abs(num / den), 1e-6))
    top, bottom = 18.0, -42.0
    x0, x1, yt, yb = 10, DISP_W - 10, 14, DISP_H - 14

    def X(f):
        return x0 + (x1 - x0) * (math.log10(f) - math.log10(20)) / 3

    def Y(m):
        return yt + (yb - yt) * (top - min(top, max(bottom, m))) / (top - bottom)

    pts = [(X(f), Y(m)) for f, m in zip(freqs, mag)]
    layer, ld = canvas(DISP_W, DISP_H)
    ld.polygon([sc(p) for p in [(x0, yb)] + pts + [(x1, yb)]], fill=hex_rgb(ACCENT, 40))
    im.alpha_composite(layer)
    glow, gd = canvas(DISP_W, DISP_H)
    line(gd, pts, hex_rgb(ACCENT, 140), 6)
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(4 * S)))
    line(d, pts, ACCENT, 2.2)
    cx = X(min(max(fc, 20), 20000))
    line(d, [(cx, yt), (cx, yb)], hex_rgb(ACCENT2, 140), 1)
    d.ellipse(sc((cx - 4, Y(0) - 4, cx + 4, Y(0) + 4)), outline='#eef3fb', width=S)
    return finish(im, DISP_W, DISP_H)


def filter_grid(d, b):
    x0, y0, x1, y1 = b
    rrect(d, b, DISPLAY, '#262b37', 1, 6)
    for f, label in [(100, '100'), (1000, '1k'), (10000, '10k')]:
        x = x0 + 10 + (x1 - x0 - 20) * (math.log10(f) - math.log10(20)) / 3
        line(d, [(x, y0 + 2), (x, y1 - 2)], GRID, 1)
        text(d, (x + 4, y1 - 9), label, 10, 'medium', DIM, 'lm')
    for k in range(1, 4):
        y = y0 + (y1 - y0) * k / 4
        line(d, [(x0 + 2, y), (x1 - 2, y)], GRID, 1)


# ---- envelope display (follows sustain) ---------------------------------------------------
def env_frame(sus, colour=ACCENT):
    im, d = canvas(DISP_W, ENV_H)
    x0, yb, yt = 20, ENV_H - 24, 22
    a, dcy, hold, rel = 90, 120, 200, 150
    ys = yb - (yb - yt) * sus
    xs = [x0, x0 + a, x0 + a + dcy, x0 + a + dcy + hold, x0 + a + dcy + hold + rel]
    pts = [(xs[0], yb)]
    for k in range(1, 21):
        t = k / 20
        pts.append((xs[0] + a * t, yb - (yb - yt) * (1 - (1 - t) ** 2)))
    for k in range(1, 21):
        t = k / 20
        pts.append((xs[1] + dcy * t, yt + (ys - yt) * (1 - (1 - t) ** 3)))
    pts.append((xs[3], ys))
    for k in range(1, 21):
        t = k / 20
        pts.append((xs[3] + rel * t, ys + (yb - ys) * (1 - (1 - t) ** 3)))
    layer, ld = canvas(DISP_W, ENV_H)
    ld.polygon([sc(p) for p in pts + [(xs[4], yb)]], fill=hex_rgb(colour, 40))
    im.alpha_composite(layer)
    glow, gd = canvas(DISP_W, ENV_H)
    line(gd, pts, hex_rgb(colour, 140), 6)
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(4 * S)))
    line(d, pts, colour, 2.2)
    for x, y in [(xs[1], yt), (xs[2], ys), (xs[3], ys)]:
        d.ellipse(sc((x - 5, y - 5, x + 5, y + 5)), fill=DISPLAY, outline='#eef3fb', width=round(1.6 * S))
    return finish(im, DISP_W, ENV_H)


def env_grid(d, b):
    x0, y0, x1, y1 = b
    display_bg(d, b, True, 10, 4)
    xs = [20, 110, 230, 430, 580]
    for i, lab in enumerate('ADSR'):
        cx = x0 + (xs[i] + xs[i + 1]) / 2
        text(d, (cx, y1 - 11), lab, 10, 'bold', DIM, 'mm', spacing=1)


# ---- LFO shapes ---------------------------------------------------------------------------
def lfo_img(shape, w=DISP_W, h=230):
    im, d = canvas(w, h)
    x0, x1, yc, amp = 24, w - 24, h / 2, h * 0.34
    n = 400
    pts = []
    rng = np.random.default_rng(7)
    sh = rng.uniform(-1, 1, 8)
    for k in range(n + 1):
        t = 2 * k / n                       # two cycles
        ph = t % 1
        if shape == 0:
            y = math.sin(2 * math.pi * ph)
        elif shape == 1:
            y = 1 - 4 * abs(ph - .5)    # rises from -1, as in dsp/lfo.h
        elif shape == 2:
            y = 1 - 2 * ph
        elif shape == 3:
            y = 1 if ph < .5 else -1
        else:
            y = sh[min(7, int(t * 4))]
        pts.append((x0 + (x1 - x0) * k / n, yc - amp * y))
    layer, ld = canvas(w, h)
    ld.polygon([sc(p) for p in [(x0, yc)] + pts + [(x1, yc)]], fill=hex_rgb(ACCENT2, 36))
    im.alpha_composite(layer)
    glow, gd = canvas(w, h)
    line(gd, pts, hex_rgb(ACCENT2, 140), 6)
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(4 * S)))
    line(d, pts, ACCENT2, 2.2)
    line(d, [(x0, yc), (x1, yc)], '#2b3242', 1)
    return finish(im, w, h)


# ---- small icons --------------------------------------------------------------------------
def icon_drive(d, b):
    x0, y0, x1, y1 = b
    display_bg(d, b, False)
    pts = []
    for k in range(61):
        t = -1 + 2 * k / 60
        pts.append((x0 + (x1 - x0) * (k / 60), (y0 + y1) / 2 - (y1 - y0) * 0.38 * math.tanh(2.6 * t)))
    line(d, [(x0 + 6, y1 - 6), (x1 - 6, y0 + 6)], '#273041', 1)
    line(d, pts, ACCENT, 2)


def icon_chorus(d, b):
    x0, y0, x1, y1 = b
    display_bg(d, b, False)
    for j, (off, col) in enumerate([(0, ACCENT), (0.18, '#7aa6ff'), (0.36, ACCENT2)]):
        pts = [(x0 + 8 + (x1 - x0 - 16) * k / 80, (y0 + y1) / 2 + (j - 1) * 4
                - (y1 - y0) * 0.3 * math.sin(2 * math.pi * (k / 80 * 2 + off))) for k in range(81)]
        line(d, pts, col, 1.6)


def icon_delay(d, b):
    x0, y0, x1, y1 = b
    display_bg(d, b, True, 12, 2)
    cy = (y0 + y1) / 2
    for k in range(7):
        x = x0 + 30 + k * ((x1 - x0 - 60) / 6)
        h = (y1 - y0) * 0.38 * (0.72 ** k)
        side = -1 if k % 2 else 1
        col = ACCENT if k == 0 else (ACCENT if side > 0 else ACCENT2)
        line(d, [(x, cy), (x, cy - side * h)], col, 3)
        d.ellipse(sc((x - 3.5, cy - side * h - 3.5, x + 3.5, cy - side * h + 3.5)), fill=col)
    text(d, (x0 + 14, y0 + 16), 'L', 11, 'bold', DIM, 'mm')
    text(d, (x0 + 14, y1 - 16), 'R', 11, 'bold', DIM, 'mm')
    line(d, [(x0 + 24, cy), (x1 - 14, cy)], '#2b3242', 1)


# ---- layout -------------------------------------------------------------------------------
class Page:
    def __init__(self, name):
        self.name = name
        self.children = []      # TUI entries
        self.groups = []        # (rect, [param or -1]*4)

    def add(self, e):
        self.children.append(e)


def slots4(px, y):
    return [(px + 10 + 155 * i, y) for i in range(4)]


def slots3(px, y):
    return [(px + 34, y), (px + 241, y), (px + 448, y)]


def entry(name, typ, data, bounds, remap=(), focus='No', invalidating=None, show='Show'):
    return ui.entry(name, typ, data, bounds, focus, show, 'Always', remap, invalidating)


def control(page, d, kind, param, x, y, w=None, h=None, label=None, invalid=None):
    """Place a control. kind: knob/knobbi/field/toggle/bar. Art for its name goes onto d."""
    w = w or CW
    h = h or CH
    if kind in ('knob', 'knobbi'):
        if label:
            knob_name(d, x, y, label)
        comp = 'pwKnobBi' if kind == 'knobbi' else 'pwKnobUni'
        w, h = CW, CH
    elif kind == 'field':
        field_bg(d, (x, y, x + w, y + h), label)
        comp = 'pwField%dx%d' % (w, h)
    elif kind == 'toggle':
        comp = 'pwToggle'
        w, h = 58, 28
    elif kind == 'bar':
        comp = 'pwAmountBar'
        w, h = BAR_W, BAR_H
    elif kind == 'master':
        comp = 'pwMaster'
    page.add(entry('p%d' % param, comp, ui.obj(('version', 1), ('handleName', 'Data')),
                   '%d %d %d %d' % (x, y, w, h), [('Data', 'Parameter %d' % param)], 'Yes', invalid, 'Hide'))
    return comp


def enabling(i, n, param):
    return ['IndexedEnabling/%d/%d/Parameter %d' % (i, n, param)]


USED_FIELDS = set()


def osc_panel(page, d, px, which, user_frames):
    tbl, pos, lvl, octv = P[which + '_table'], P[which + '_pos'], P[which + '_level'], P[which + '_oct']
    py = 48
    panel_box(d, (px, py, px + 622, 616), 'OSC ' + which.upper(), subtitle='WAVETABLE')
    # Table selector in the header.
    fx, fy, fw, fh = px + 352, py + 5, 258, 28
    page.add(entry('p%d' % tbl, 'pwField%dx%d' % (fw, fh), ui.obj(('version', 1), ('handleName', 'Data')),
                   '%d %d %d %d' % (fx, fy, fw, fh), [('Data', 'Parameter %d' % tbl)], 'Yes', None, 'Hide'))
    USED_FIELDS.add((fw, fh))
    # Display: one film strip per table, shown only while that table is selected; dragging it moves Position.
    dx, dy = px + 12, py + 48
    display_bg(d, (dx, dy, dx + DISP_W, dy + DISP_H), True, 8, 4)
    for t in range(12):
        img = 'wt-%d.png' % t if t < 4 or (t - 4) in user_frames else 'wt-user.png'
        page.add(entry('wt %s %d' % (which, t), 'Knob', ui.knob_filmstrip_data(img, DISP_FRAMES, 'Horizontal'),
                       '%d %d %d %d' % (dx, dy, DISP_W, DISP_H), [('Data', 'Parameter %d' % pos)],
                       'No', enabling(t, 12, tbl), 'Hide'))
    # Row 1: position, octave, level.
    r1 = py + 252
    for (x, y), (kind, prm, lab) in zip(slots3(px, r1), [('knob', pos, 'POSITION'), ('knobbi', octv, 'OCTAVE'),
                                                           ('knob', lvl, 'LEVEL')]):
        control(page, d, kind, prm, x, y, label=lab)
    line(d, [(px + 20, r1 + CH + 14), (px + 602, r1 + CH + 14)], EDGE, 1)
    r2 = r1 + CH + 28
    if which == 'a':
        row2 = [('knob', P['a_uni'], 'UNISON'), ('knob', P['a_det'], 'DETUNE'),
                ('knob', P['sub'], 'SUB'), ('knob', P['noise'], 'NOISE')]
        xdiv = px + 10 + 155 * 2 - 8
        line(d, [(xdiv, r2 + 4), (xdiv, r2 + CH - 4)], EDGE, 1)
    else:
        row2 = [('knobbi', P['b_semi'], 'SEMI'), ('knob', P['b_uni'], 'UNISON'),
                ('knob', P['b_det'], 'DETUNE'), ('knob', P['width'], 'WIDTH')]
    for (x, y), (kind, prm, lab) in zip(slots4(px, r2), row2):
        control(page, d, kind, prm, x, y, label=lab)
    page.groups.append(('%d %d %d %d' % (px + 4, py + 42, 614, r1 + CH + 8 - py - 42), [tbl, pos, octv, lvl]))
    page.groups.append(('%d %d %d %d' % (px + 4, r2 - 8, 614, CH + 16), [p for _, p, _ in row2]))


def build_pages(user_frames):
    pages, arts = [], []

    # ---------- OSC ----------
    im, d = canvas(W, H, hex_rgb(BG))
    top_strip(d, 0)
    pg = Page('OSC')
    osc_panel(pg, d, 12, 'a', user_frames)
    osc_panel(pg, d, 646, 'b', user_frames)
    pages.append(pg); arts.append(im)

    # ---------- FILTER / FX ----------
    im, d = canvas(W, H, hex_rgb(BG))
    top_strip(d, 1)
    pg = Page('FILTER')
    px, py = 12, 48
    panel_box(d, (px, py, px + 622, 438), 'FILTER', subtitle='SVF 12 dB')
    for i, lab in enumerate(['LP', 'BP', 'HP', 'NOTCH']):
        x = px + 330 + i * 70
        pg.add(entry('mode %d' % i, 'Button', ui.button_data('seg-on.png', 'seg-off.png', i, 4),
                     '%d %d 66 28' % (x, py + 5), [('Data', 'Parameter %d' % P['mode'])]))
        pg.add(entry('mode lab %d' % i, 'Label', ui.label_data('Text', 'ffd5dbe8', 15.0,
                     'horizontallyCentred verticallyCentred', lab), '%d %d 66 28' % (x, py + 5)))
    dx, dy = px + 12, py + 48
    filter_grid(d, (dx, dy, dx + DISP_W, dy + DISP_H))
    for m in range(4):
        pg.add(entry('curve %d' % m, 'Knob', ui.knob_filmstrip_data('filter-%d.png' % m, DISP_FRAMES, 'Horizontal'),
                     '%d %d %d %d' % (dx, dy, DISP_W, DISP_H), [('Data', 'Parameter %d' % P['cutoff'])],
                     'No', enabling(m, 4, P['mode']), 'Hide'))
    r = py + 244
    for (x, y), (kind, prm, lab) in zip(slots3(px, r), [('knob', P['cutoff'], 'CUTOFF'), ('knob', P['res'], 'RESONANCE'),
                                                         ('knobbi', P['envamt'], 'ENV 2 AMT')]):
        control(pg, d, kind, prm, x, y, label=lab)
    pg.groups.append(('16 52 614 382', [P['mode'], P['cutoff'], P['res'], P['envamt']]))
    # Voice / global strip.
    panel_box(d, (px, 450, px + 622, 616), 'VOICE', subtitle='TRACKING · BEND · OUTPUT')
    for (x, y), (kind, prm, lab) in zip(slots3(px, 492), [('knob', P['keytrack'], 'KEY TRACK'),
                                                          ('knob', P['bend'], 'BEND RANGE'),
                                                          ('knob', P['master'], 'MASTER')]):
        control(pg, d, kind, prm, x, y, label=lab)
    pg.groups.append(('16 454 614 158', [P['keytrack'], P['bend'], P['master'], -1]))
    # Drive and chorus.
    for bx, title, prm, lab, icon in [(646, 'DISTORTION', P['drive'], 'DRIVE', icon_drive),
                                       (962, 'CHORUS', P['chorus'], 'MIX', icon_chorus)]:
        panel_box(d, (bx, 48, bx + 306, 250), title)
        icon(d, (bx + 16, 104, bx + 140, 214))
        control(pg, d, 'knob', prm, bx + 154, 100, label=lab)
    pg.groups.append(('650 52 614 194', [P['drive'], P['chorus'], -1, -1]))
    # Delay.
    panel_box(d, (646, 262, 1268, 616), 'DELAY', subtitle='PING-PONG · TEMPO SYNC')
    icon_delay(d, (658, 310, 1256, 440))
    x0 = 646
    control(pg, d, 'knob', P['delay'], x0 + 34, 470, label='MIX')
    control(pg, d, 'field', P['delay_div'], x0 + 236, 486, 150, 34, label='DIVISION')
    USED_FIELDS.add((150, 34))
    control(pg, d, 'knob', P['feedback'], x0 + 448, 470, label='FEEDBACK')
    pg.groups.append(('650 300 614 312', [P['delay'], P['delay_div'], P['feedback'], -1]))
    pages.append(pg); arts.append(im)

    # ---------- ENV ----------
    im, d = canvas(W, H, hex_rgb(BG))
    top_strip(d, 2)
    pg = Page('ENV')
    for px, title, sub, base, strip_name, note in [
            (12, 'ENV 1', 'AMPLITUDE', 'amp', 'env-1.png', 'Shapes the volume of every voice.'),
            (646, 'ENV 2', 'MODULATION', 'e2', 'env-2.png', 'Drives the filter via ENV 2 AMT; also a matrix source.')]:
        panel_box(d, (px, 48, px + 622, 616), title, subtitle=sub)
        dx, dy = px + 12, 96
        env_grid(d, (dx, dy, dx + DISP_W, dy + ENV_H))
        pg.add(entry('env %s' % base, 'Knob', ui.knob_filmstrip_data(strip_name, DISP_FRAMES),
                     '%d %d %d %d' % (dx, dy, DISP_W, ENV_H), [('Data', 'Parameter %d' % P[base + '_s'])], 'No'))
        prms = [P[base + '_a'], P[base + '_d'], P[base + '_s'], P[base + '_r']]
        for (x, y), prm, lab in zip(slots4(px, 330), prms, ['ATTACK', 'DECAY', 'SUSTAIN', 'RELEASE']):
            control(pg, d, 'knob', prm, x, y, label=lab)
        line(d, [(px + 20, 470), (px + 602, 470)], EDGE, 1)
        text(d, (px + 24, 498), note, 13, 'medium', MUTED, 'lm')
        text(d, (px + 24, 524), 'The curve shows the sustain level; times are on the knobs.', 12, 'regular', DIM, 'lm')
        pg.groups.append(('%d 322 614 134' % (px + 4), prms))
    pages.append(pg); arts.append(im)

    # ---------- LFO ----------
    im, d = canvas(W, H, hex_rgb(BG))
    top_strip(d, 3)
    pg = Page('LFO')
    for px, n in [(12, 1), (646, 2)]:
        k = 'l%d_' % n
        shape, rate, sync, div = P[k + 'shape'], P[k + 'rate'], P[k + 'sync'], P[k + 'div']
        panel_box(d, (px, 48, px + 622, 616), 'LFO %d' % n, subtitle='GLOBAL')
        dx, dy = px + 12, 96
        display_bg(d, (dx, dy, dx + DISP_W, dy + 230), True, 8, 4)
        for s in range(5):
            pg.add(entry('lfo%d shape %d' % (n, s), 'Image',
                         ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'lfo-%d.png' % s)),
                         '%d %d %d 230' % (dx, dy, DISP_W), [], 'No', enabling(s, 5, shape), 'Hide'))
        y = 360
        control(pg, d, 'field', shape, px + 24, y + 16, 250, 36, label='SHAPE')
        USED_FIELDS.add((250, 36))
        # Rate knob while free-running, division field while synced, in the same place.
        control(pg, d, 'knob', rate, px + 316, y, invalid=enabling(0, 2, sync))
        control(pg, d, 'field', div, px + 316, y + 16, 140, 36, invalid=enabling(1, 2, sync))
        USED_FIELDS.add((140, 36))
        text(d, (px + 316 + CW / 2, y + 76), 'RATE · DIV', 12, 'semibold', MUTED, 'mm', spacing=1.0)
        text(d, (px + 520, y + 76), 'SYNC', 12, 'semibold', MUTED, 'mm', spacing=1.0)
        control(pg, d, 'toggle', sync, px + 491, y + 20)
        line(d, [(px + 20, 500), (px + 602, 500)], EDGE, 1)
        text(d, (px + 24, 528), 'Route LFO %d in the MATRIX page (sources LFO1 / LFO2).' % n, 13, 'medium', MUTED, 'lm')
        text(d, (px + 24, 554), 'SYNC locks the rate to the MPC tempo using DIV.', 12, 'regular', DIM, 'lm')
        pg.groups.append(('%d 352 614 136' % (px + 4), [shape, rate, div, sync]))
    pages.append(pg); arts.append(im)

    # ---------- MATRIX ----------
    im, d = canvas(W, H, hex_rgb(BG))
    top_strip(d, 4)
    pg = Page('MATRIX')
    panel_box(d, (12, 48, 1268, 616), 'MOD MATRIX', subtitle='4 SLOTS · BIPOLAR AMOUNT')
    for x, lab in [(52, '#'), (256, 'SOURCE'), (666, 'AMOUNT'), (1076, 'DESTINATION')]:
        text(d, (x, 108), lab, 11, 'bold', DIM, 'mm', spacing=1.4)
    for i, (src, tgt, amt) in enumerate(MOD):
        y0 = 126 + i * 120
        rrect(d, (24, y0, 1256, y0 + 108), '#191c23', '#252a35', 1, 6)
        text(d, (52, y0 + 54), str(i + 1), 26, 'bold', mix(ACCENT, ACCENT2, i / 3), 'mm')
        control(pg, d, 'field', src, 96, y0 + 32, 320, 44)
        USED_FIELDS.add((320, 44))
        line(d, [(430, y0 + 54), (472, y0 + 54)], '#3a4152', 2)
        control(pg, d, 'bar', amt, 486, y0 + 34)
        line(d, [(860, y0 + 54), (902, y0 + 54)], '#3a4152', 2)
        d.polygon([sc((902, y0 + 48)), sc((912, y0 + 54)), sc((902, y0 + 60))], fill='#3a4152')
        control(pg, d, 'field', tgt, 916, y0 + 32, 320, 44)
        pg.groups.append(('20 %d 1240 116' % (y0 - 4), [src, amt, tgt, -1]))
    pages.append(pg); arts.append(im)

    # Header master knob on every page (touch control; its Q-Link lives on FILTER > VOICE).
    for pg in pages:
        pg.add(entry('master', 'pwMaster', ui.obj(('version', 1), ('handleName', 'Data')), '1146 4 122 34',
                     [('Data', 'Parameter %d' % P['master'])], 'No', None, 'Hide'))
    return pages, arts


# ---- component definitions ---------------------------------------------------------------
def focus(w, h):
    return ui.entry('focus', 'Focus', ui.focus_data(A_FOCUS), '0 0 %d %d' % (w, h), 'No', 'Show', 'WhenFocussed')


def value_label(bounds, size=19.0, colour=VALUE_COL, just='horizontallyCentred verticallyCentred'):
    return entry('value', 'Label', ui.label_data('Value', colour, size, just), bounds)


def definitions():
    defs = []
    for name, img, overlay in [('pwKnobUni', 'knob.png', 'knob overlay'), ('pwKnobBi', 'knob-bi.png', 'knob overlay bipolar')]:
        defs.append((name, ui.component_definition(ui.control_actions(overlay), [
            focus(CW, CH),
            entry('knob', 'Knob', ui.knob_filmstrip_data(img, FRAMES), '%d 0 %d %d' % ((CW - KN) // 2, KN, KN)),
            value_label('0 88 %d 28' % CW)], '0')))
    for w, h in sorted(USED_FIELDS):
        defs.append(('pwField%dx%d' % (w, h), ui.component_definition(ui.control_actions('menu overlay'), [
            entry('box', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'),
                                         ('image', 'field-%dx%d.png' % (w, h))), '0 0 %d %d' % (w, h)),
            focus(w, h),
            value_label('14 0 %d %d' % (w - 40, h), 18.0 if h < 32 else 20.0, FIELD_TEXT, 'left verticallyCentred')], '0')))
    defs.append(('pwToggle', ui.component_definition(ui.control_actions('menu overlay', False), [
        focus(58, 28), entry('toggle', 'Button', ui.button_data('toggle-on.png', 'toggle-off.png', 1, 1), '0 0 58 28')], '0')))
    defs.append(('pwAmountBar', ui.component_definition(ui.control_actions('knob overlay bipolar'), [
        focus(BAR_W, BAR_H),
        entry('bar', 'Knob', ui.knob_filmstrip_data('amount-bar.png', FRAMES, 'Horizontal'), '0 0 %d %d' % (BAR_W, BAR_H)),
        value_label('0 0 %d %d' % (BAR_W, BAR_H), 18.0, 'ffeef3fb')], '0')))
    defs.append(('pwMaster', ui.component_definition(ui.control_actions('knob overlay'), [
        focus(122, 34),
        entry('knob', 'Knob', ui.knob_filmstrip_data('knob-small.png', FRAMES), '0 0 %d %d' % (SMALL, SMALL)),
        value_label('40 0 82 34', 18.0, VALUE_COL, 'left verticallyCentred')], '0')))
    for name, data, typ in [('knob overlay', ui.knob_overlay_data(False), 'KnobOverlay'),
                            ('knob overlay bipolar', ui.knob_overlay_data(True), 'KnobOverlay'),
                            ('menu overlay', ui.menu_overlay_data(), 'MenuOverlay')]:
        defs.append((name, ui.overlay_component(typ, data, 'ff12141a')))
    return defs


# ---- output -------------------------------------------------------------------------------
def make_art(user_frames):
    SKINS.mkdir(parents=True, exist_ok=True)
    strip(lambda v: knob_frame(v, KN, False), FRAMES, KN, KN, SKINS / 'knob.png')
    strip(lambda v: knob_frame(v, KN, True), FRAMES, KN, KN, SKINS / 'knob-bi.png')
    strip(lambda v: knob_frame(v, SMALL, False, 3.0), FRAMES, SMALL, SMALL, SKINS / 'knob-small.png')
    strip(bar_frame, FRAMES, BAR_W, BAR_H, SKINS / 'amount-bar.png')
    toggle_img(True).save(SKINS / 'toggle-on.png'); toggle_img(False).save(SKINS / 'toggle-off.png')
    seg_img(True).save(SKINS / 'seg-on.png'); seg_img(False).save(SKINS / 'seg-off.png')
    banks = [builtin_bank(b) for b in range(4)]
    for b in range(4):
        strip(lambda v, b=b: wt_frame(banks[b], v), DISP_FRAMES, DISP_W, DISP_H, SKINS / ('wt-%d.png' % b))
    for u, fr in user_frames.items():
        strip(lambda v, fr=fr: wt_frame(fr, v), DISP_FRAMES, DISP_W, DISP_H, SKINS / ('wt-%d.png' % (u + 4)))
    strip(lambda v: wt_frame(None, v, generic=True), DISP_FRAMES, DISP_W, DISP_H, SKINS / 'wt-user.png')
    for m in range(4):
        strip(lambda v, m=m: filter_frame(m, v), DISP_FRAMES, DISP_W, DISP_H, SKINS / ('filter-%d.png' % m))
    strip(lambda v: env_frame(v, ACCENT), DISP_FRAMES, DISP_W, ENV_H, SKINS / 'env-1.png')
    strip(lambda v: env_frame(v, ACCENT2), DISP_FRAMES, DISP_W, ENV_H, SKINS / 'env-2.png')
    for s in range(5):
        lfo_img(s).save(SKINS / ('lfo-%d.png' % s))
    for w, h in sorted(USED_FIELDS):
        field_img(w, h).save(SKINS / ('field-%dx%d.png' % (w, h)))


def make_tui(pages, arts, params):
    tabs, maps = [], []
    for ti, (pg, art) in enumerate(zip(pages, arts)):
        name = 'panel-%s.png' % pg.name.lower()
        finish(art, W, H).convert('RGB').save(SKINS / name, optimize=True)
        bg = entry('panel', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', name)),
                   '0 0 1280 628')
        tabs.append(ui.obj(('version', 1), ('tabName', pg.name), ('fnKeyIndex', ti), ('fnKeySubIndex', 0),
                           ('componentDefinition', ui.component_definition([], [bg] + pg.children, 'ff12141a')),
                           ('qlinkBoundsData', [r for r, _ in pg.groups])))
        # Row r of the hardware 4x4 grid (top row first) = group r on screen.
        assigned = {}
        for r, (_, prms) in enumerate(pg.groups):
            for c, prm in enumerate(prms):
                assigned[13 + c - 4 * r] = prm
        maps.append(ui.obj(('Tab', ti + 1), ('SubTab', 1), ('Bank Direction', 'RowTopDown'),
                           ('Q-Links', ui.Obj(('Q-Link %d' % q, assigned.get(q, -1)) for q in range(1, 17)))))
    discrete = {p['index']: p['choices'] for p in params['params'] if p.get('choices')}
    referenced = set()

    def scan(v):
        if isinstance(v, ui.Obj):
            for k, x in v:
                scan(x)
        elif isinstance(v, (list, tuple)):
            for x in v:
                scan(x)
        elif isinstance(v, str) and v.startswith('Parameter '):
            referenced.add(int(v.split()[-1]))
        elif isinstance(v, str) and v.startswith('IndexedEnabling/'):
            referenced.add(int(v.split()[-1]))
    for t in tabs:
        scan(t)
    repl = [ui.obj(('key', 'Parameter %d' % i), ('value', list(discrete[i]))) for i in sorted(referenced) if i in discrete]
    data = ui.obj(('pageData', ui.obj(
        ('version', 1),
        ('componentDefinitions', ui.obj(('version', 2), ('importFiles', []),
                                        ('localComponentDefinitions', [ui.obj(('key', k), ('value', v)) for k, v in definitions()]))),
        ('replacementStrings', repl),
        ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
        ('tabs', tabs))))
    # Program mode: the 16 most played sound controls.
    program = [P['a_pos'], P['b_pos'], P['cutoff'], P['res'], P['envamt'], P['drive'], P['chorus'], P['delay'],
               P['a_level'], P['b_level'], P['sub'], P['noise'], P['amp_a'], P['amp_d'], P['amp_s'], P['amp_r']]
    qlinks = ui.obj(('version', 4), ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
                    ('Screen Mode Q-Links', ui.obj(('version', 4), ('map', maps))),
                    ('Program Mode Q-Links', ui.Obj(('Q-Link %d' % (q + 1), p) for q, p in enumerate(program))))
    (SKINS / 'TUI.json').write_text(ui.dump_json(data) + '\n')
    (SKINS / 'Q-Links.json').write_text(ui.dump_json(qlinks) + '\n')


# ---- preview (renders the emitted TUI tree with default values) ---------------------------
def render_preview(params, output, overrides=None):
    tui = json.loads((SKINS / 'TUI.json').read_text())['pageData']
    defs = {x['key']: x['value'] for x in tui['componentDefinitions']['localComponentDefinitions']}
    info = {p['index']: dict(p) for p in params['params']}
    for k, v in (overrides or {}).items():
        info[k]['default'] = v
    repl = {int(r['key'].split()[-1]): r['value'] for r in tui['replacementStrings']}
    def pf(size):
        f = Path('/usr/share/fonts/opentype/inter/Inter-Medium.otf')
        return ImageFont.truetype(str(f), round(size * 0.92)) if f.exists() else ImageFont.load_default()

    def step(idx, n):
        return round(info[idx]['default'] * (n - 1))

    def visible(bounds):
        for h in bounds.get('additionalInvalidatingHandles', []):
            _, i, n, prm = h.split('/')
            if step(int(prm.split()[-1]), int(n)) != int(i):
                return False
        return True

    def display(idx):
        p = info[idx]
        if idx in repl:
            return repl[idx][step(idx, len(repl[idx]))]
        return p['display']

    out = []
    for tab in tui['tabs']:
        im = Image.new('RGBA', (W, H), BG)

        def walk(defn, origin=(0, 0), binds=None):
            for e in defn.get('componentsData', []):
                b = e['bounds']
                if b['whenVisible'] == 'WhenFocussed' or not visible(b):
                    continue
                x, y, w, h = map(int, b['bounds'].split())
                x += origin[0]; y += origin[1]
                bb = dict(binds or {})
                for pair in e['handle remapping']['map']:
                    bb[pair['key']] = pair['value']
                comp = e['componentData']; typ = comp['type']; data = comp['data']
                if typ in defs:
                    walk(defs[typ], (x, y), bb); continue
                prm = bb.get(data.get('handleName', 'Data'), '')
                idx = int(prm.split()[-1]) if prm.startswith('Parameter ') else None
                if typ == 'Image':
                    im.alpha_composite(Image.open(SKINS / data['image']).convert('RGBA').resize((w, h)), (x, y))
                elif typ == 'Knob':
                    s = Image.open(SKINS / data['filmStrip']).convert('RGBA'); n = data['numFrames']; fh = s.height // n
                    f = round(info[idx]['default'] * (n - 1))
                    im.alpha_composite(s.crop((0, f * fh, s.width, (f + 1) * fh)), (x, y))
                elif typ == 'Button':
                    n = data['numButtonsInGroup']; v = step(idx, max(n, 2))
                    lit = v == data['buttonId']
                    im.alpha_composite(Image.open(SKINS / (data['onImage'] if lit else data['offImage'])).convert('RGBA'), (x, y))
                elif typ == 'Label':
                    msg = data.get('text') if data['type'] == 'Text' else display(idx)
                    col = '#' + data['textStyle']['colour'][2:]
                    dr = ImageDraw.Draw(im)
                    j = data['textStyle']['justification']
                    f = pf(data['textStyle']['font']['height'])
                    if j.startswith('left'):
                        dr.text((x, y + h / 2), msg, font=f, fill=col, anchor='lm')
                    else:
                        dr.text((x + w / 2, y + h / 2), msg, font=f, fill=col, anchor='mm')
        walk(tab['componentDefinition'])
        path = output / ('prism-wave-%s.png' % tab['tabName'].lower())
        im.convert('RGB').save(path)
        out.append(path)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--params', type=Path, required=True)
    ap.add_argument('--preview-dir', type=Path, default=ROOT / 'docs' / 'screenshots')
    ap.add_argument('--no-user-previews', action='store_true',
                    help='do not render the shipped Content/Wavetables/userNN.wav files into the display')
    args = ap.parse_args()
    params = json.loads(args.params.read_text())
    assert len(params['params']) == 56, 'expected Prism Wave 0.1.x with 56 parameters'
    user_frames = {}
    if not args.no_user_previews:
        for u in range(8):
            p = PROJECT / 'Content' / 'Wavetables' / ('user%02d.wav' % (u + 1))
            if p.exists():
                fr = read_wav_frames(p)
                if fr is not None:
                    user_frames[u] = fr
    pages, arts = build_pages(user_frames)
    make_art(user_frames)
    make_tui(pages, arts, params)
    args.preview_dir.mkdir(parents=True, exist_ok=True)
    files = render_preview(params, args.preview_dir)
    print('Generated Prism Wave skin: %d pages, user slots with previews: %s' %
          (len(pages), ', '.join('User %02d' % (u + 1) for u in user_frames) or 'none'))
    for f in files:
        print('  preview', f)


if __name__ == '__main__':
    main()
