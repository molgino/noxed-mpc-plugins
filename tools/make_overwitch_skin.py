#!/usr/bin/env python3
"""Generate the skins of MPC Overwitch (receive instrument) and MPC Overwitch Send (effect).

Reuses the drawing helpers of make_prismwave_skin.py so the NOXED plugins look alike.
    python tools/make_overwitch_skin.py --params "plugins/MPC Overwitch/build/params.json"
    python tools/make_overwitch_skin.py --send --params "plugins/MPC Overwitch Send/build/params.json"
    python tools/make_overwitch_skin.py --bridge --params "plugins/MPC USB Bridge/build/params.json"
"""
import argparse, json
from pathlib import Path
import gen_ui as ui
import make_prismwave_skin as pw

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'plugins' / 'MPC Overwitch'
SKINS = PROJECT / 'skins'
W, H = pw.W, pw.H
pw.ACCENT = '#e65a46'
pw.BG = '#191919'
ROWS = [(3, 'STATUS'), (4, 'DEVICE')]


def build(params):
    SKINS.mkdir(parents=True, exist_ok=True)
    im, d = pw.canvas(W, H, pw.hex_rgb(pw.BG))
    pw.rrect(d, (0, 0, W, 42), '#0d0f14')
    pw.line(d, [(0, 42), (W, 42)], '#232835', 1)
    pw.prism_logo(d, 22, 8)
    pw.text(d, (76, 21), 'MPC OVERWITCH', 19, 'black', pw.TEXT, 'lm', spacing=2.2)
    pw.text(d, (300, 22), 'NOXED · OVERBRIDGE FOR THE MPC · 0.5.1 PREVIEW', 12, 'semibold', pw.MUTED, 'lm', spacing=1.6)

    children = []
    # Output panel: source and level.
    pw.panel_box(d, (12, 48, 412, 616), 'OUTPUT', subtitle='TO THIS PLUGIN TRACK')
    pw.text(d, (36, 104), 'SOURCE', 12, 'bold', pw.MUTED, 'lm', spacing=1.4)
    children.append(pw.entry('p1', 'owSource', ui.obj(('version', 1), ('handleName', 'Data')), '36 120 352 48',
                             [('Data', 'Parameter 1')], 'Yes', None, 'Hide'))
    pw.text(d, (36, 186), 'Double-tap to choose a device output.', 12, 'medium', pw.DIM, 'lm')
    pw.line(d, [(28, 214), (396, 214)], '#232835', 1)
    children.append(pw.entry('p0', 'owLevel', ui.obj(('version', 1), ('handleName', 'Data')), '142 240 140 118',
                             [('Data', 'Parameter 0')], 'Yes', None, 'Hide'))
    pw.knob_name(d, 142, 240, 'LEVEL')
    pw.line(d, [(28, 384), (396, 384)], '#232835', 1)
    pw.text(d, (36, 404), 'MIDI CHANNEL', 12, 'bold', pw.MUTED, 'lm', spacing=1.4)
    children.append(pw.entry('p%d' % MIDI_PARAM, 'owSource', ui.obj(('version', 1), ('handleName', 'Data')), '36 420 352 48',
                             [('Data', 'Parameter %d' % MIDI_PARAM)], 'Yes', None, 'Hide'))
    for i, t in enumerate(['Notes and CC of this track go to the', 'device on this channel (USB MIDI).',
                           'Set the device USB config to OVERBRIDGE.']):
        pw.text(d, (36, 488 + i * 22), t, 12, 'medium', pw.MUTED, 'lm')
    pw.text(d, (36, 590), 'Based on Overwitch by David Garcia Goni', 10, 'medium', pw.DIM, 'lm')

    # Status panel.
    pw.panel_box(d, (424, 48, 1268, 436), 'STATUS', subtitle='UPDATES BY ITSELF')
    for i, (prm, name) in enumerate(ROWS):
        y = 100 + i * 64
        if i:
            pw.line(d, [(440, y - 10), (1252, y - 10)], '#232835', 1)
        pw.text(d, (452, y + 22), name, 13, 'bold', pw.MUTED, 'lm', spacing=1.4)
        colour = 'ffe65a46' if prm == 3 else 'ffe4e9f2'
        children.append(pw.entry('row %d' % prm, 'Label',
                                 ui.label_data('Value', colour, 28.0 if prm == 3 else 22.0, 'left verticallyCentred'),
                                 '600 %d 650 44' % y, [('Data', 'Parameter %d' % prm)]))
    pw.text(d, (452, 480), 'AUDIO ROUTING', 22, 'bold', pw.TEXT, 'lm')
    pw.text(d, (452, 526), 'Select a source or build a group on the GROUP page.', 18, 'medium', pw.MUTED, 'lm')
    pw.strip(lambda v: pw.knob_frame(v, pw.KN, False), pw.FRAMES, pw.KN, pw.KN, SKINS / 'knob.png')
    pw.field_img(352, 48).save(SKINS / 'field.png')
    pw.finish(im, W, H).convert('RGB').save(SKINS / 'panel.png', optimize=True)

    defs = [
        ('owLevel', ui.component_definition(ui.control_actions('knob overlay'), [
            pw.focus(140, 118),
            pw.entry('knob', 'Knob', ui.knob_filmstrip_data('knob.png', pw.FRAMES), '%d 0 %d %d' % ((140 - pw.KN) // 2, pw.KN, pw.KN)),
            pw.entry('value', 'Label', ui.label_data('Value', pw.VALUE_COL, 19.0, 'horizontallyCentred verticallyCentred'), '0 88 140 28')], '0')),
        ('owSource', ui.component_definition(ui.control_actions('menu overlay'), [
            pw.entry('box', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'field.png')), '0 0 352 48'),
            pw.focus(352, 48),
            pw.entry('value', 'Label', ui.label_data('Value', pw.FIELD_TEXT, 22.0, 'left verticallyCentred'), '16 0 300 48')], '0')),
        ('knob overlay', ui.overlay_component('KnobOverlay', ui.knob_overlay_data(False), 'ff12141a')),
        ('menu overlay', ui.overlay_component('MenuOverlay', ui.menu_overlay_data(), 'ff12141a')),
    ]
    bg = pw.entry('panel', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'panel.png')),
                  '0 0 1280 628')
    tab = ui.obj(('version', 1), ('tabName', 'STREAM'), ('fnKeyIndex', 0), ('fnKeySubIndex', 0),
                 ('componentDefinition', ui.component_definition([], [bg] + children, 'ff12141a')),
                 ('qlinkBoundsData', ['20 92 384 280', '0 0 0 0']))
    group_tab, group_defs, group_qmap = build_group_tab()
    defs += group_defs
    # Only the Source list is static (the menu needs it); every other text comes live from the plugin.
    choices = list(next(p['choices'] for p in params['params'] if p['index'] == 1))
    choices[-1] = 'Group'
    data = ui.obj(('pageData', ui.obj(
        ('version', 1),
        ('componentDefinitions', ui.obj(('version', 2), ('importFiles', []),
                                        ('localComponentDefinitions', [ui.obj(('key', k), ('value', v)) for k, v in defs]))),
        ('replacementStrings', [ui.obj(('key', 'Parameter 1'), ('value', choices))] +
                               [ui.obj(('key', 'Parameter %d' % (GROUP_FIRST + i)), ('value', ['Off', 'On'])) for i in range(20)] +
                               [ui.obj(('key', 'Parameter %d' % MIDI_PARAM), ('value', MIDI_CHOICES))]),
        ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
        ('tabs', [tab, group_tab]))))
    # Row 1 of the Q-Links: Source, Level; row 2: Report.
    qmap = ui.obj(('Tab', 1), ('SubTab', 1), ('Bank Direction', 'RowTopDown'),
                  ('Q-Links', ui.Obj(('Q-Link %d' % q, {13: 1, 14: 0, 15: MIDI_PARAM}.get(q, -1)) for q in range(1, 17))))
    qlinks = ui.obj(('version', 4), ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
                    ('Screen Mode Q-Links', ui.obj(('version', 4), ('map', [qmap, group_qmap]))),
                    ('Program Mode Q-Links', ui.Obj(('Q-Link %d' % (q + 1), {0: 0, 1: 1}.get(q, -1)) for q in range(16))))
    (SKINS / 'TUI.json').write_text(ui.dump_json(data) + '\n')
    (SKINS / 'Q-Links.json').write_text(ui.dump_json(qlinks) + '\n')


GROUP_FIRST = 8   # parameter index of "G Track 1"
MIDI_PARAM = 28   # MPC Overwitch "MIDI Ch"
MIDI_CHOICES = ['Off'] + ['Ch %d' % c for c in range(1, 17)]
FX_NAMES = ['DELAY', 'REVERB', 'CHORUS', 'INPUT']


def pad_img(on, w=150, h=100, caption=''):
    # The caption is part of the image: fixed-text labels in a tab are not drawn by the MPC.
    im, d = pw.canvas(w, h)
    if on:
        pw.rrect(d, (1, 1, w - 1, h - 1), '#46251f', pw.ACCENT, 1.6, 8)
        pw.rrect(d, (10, h - 14, w - 10, h - 9), pw.ACCENT, None, 0, 2)
    else:
        pw.rrect(d, (1, 1, w - 1, h - 1), '#242424', '#515151', 1.2, 8)
        pw.rrect(d, (10, h - 14, w - 10, h - 9), '#2a2f3a', None, 0, 2)
    if caption:
        pw.text(d, (w / 2, h / 2 - 8), caption, 17, 'bold', '#eef9ff' if on else pw.TEXT, 'mm', spacing=1.2)
    return pw.finish(im, w, h)


def build_group_tab():
    """Second page: 16 track pads (Q-Link grid) plus Delay/Reverb/Chorus/Input; Source = Group sums them."""
    im, d = pw.canvas(W, H, pw.hex_rgb(pw.BG))
    pw.rrect(d, (0, 0, W, 42), '#0d0f14')
    pw.line(d, [(0, 42), (W, 42)], '#232835', 1)
    pw.prism_logo(d, 22, 8)
    pw.text(d, (76, 21), 'MPC OVERWITCH', 19, 'black', pw.TEXT, 'lm', spacing=2.2)
    pw.text(d, (300, 22), 'GROUP · SET SOURCE TO "GROUP" ON THE STREAM PAGE', 12, 'semibold', pw.MUTED, 'lm', spacing=1.6)
    pw.panel_box(d, (12, 48, 1268, 378), 'TRACKS', subtitle='TAP TO ADD / REMOVE')
    pw.panel_box(d, (12, 390, 1268, 616), 'EFFECTS & INPUT', subtitle='ALSO SUMMABLE')
    children = []
    pad_w, pad_h = 140, 100
    defs = []
    names = ['TRACK %d' % (i + 1) for i in range(16)] + FX_NAMES
    for i, name in enumerate(names):
        w = pad_w if i < 16 else 292
        if i < 16:
            r, c = divmod(i, 8)
            x, y = 28 + c * 156, 104 + r * 136
        else:
            k = i - 16
            x, y = 28 + k * 312, 450
        on, off = 'pad%02d-on.png' % (i + 1), 'pad%02d-off.png' % (i + 1)
        pad_img(True, w, pad_h, name).save(SKINS / on)
        pad_img(False, w, pad_h, name).save(SKINS / off)
        comp = 'owPad%02d' % (i + 1)
        defs.append((comp, ui.component_definition(ui.control_actions('menu overlay', False), [
            pw.focus(w, pad_h),
            pw.entry('pad', 'Button', ui.button_data(on, off, 1, 1), '0 0 %d %d' % (w, pad_h))], '0')))
        children.append(pw.entry('g%d' % i, comp, ui.obj(('version', 1), ('handleName', 'Data')), '%d %d %d %d' % (x, y, w, pad_h),
                                 [('Data', 'Parameter %d' % (GROUP_FIRST + i))], 'Yes', None, 'Hide'))
    pw.finish(im, W, H).convert('RGB').save(SKINS / 'panel-group.png', optimize=True)
    bg = pw.entry('panel', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'panel-group.png')),
                  '0 0 1280 628')
    tab = ui.obj(('version', 1), ('tabName', 'GROUP'), ('fnKeyIndex', 1), ('fnKeySubIndex', 0),
                 ('componentDefinition', ui.component_definition([], [bg] + children, 'ff12141a')),
                 ('qlinkBoundsData', ['22 98 620 112', '646 98 620 112', '22 234 620 112', '646 234 620 112']))
    qmap = ui.obj(('Tab', 2), ('SubTab', 1), ('Bank Direction', 'RowTopDown'),
                  ('Q-Links', ui.Obj(('Q-Link %d' % q, GROUP_FIRST + (3 - (q - 1) // 4) * 4 + (q - 1) % 4) for q in range(1, 17))))
    return tab, defs, qmap


SEND_SKINS = ROOT / 'plugins' / 'MPC Overwitch Send' / 'skins'
SEND_ROWS = [(3, 'STATUS'), (4, 'DEVICE'), (5, 'STREAM'), (6, 'LEVELS')]


def build_send(params):
    out = SEND_SKINS
    out.mkdir(parents=True, exist_ok=True)
    im, d = pw.canvas(W, H, pw.hex_rgb(pw.BG))
    pw.rrect(d, (0, 0, W, 42), '#0d0f14')
    pw.line(d, [(0, 42), (W, 42)], '#232835', 1)
    pw.prism_logo(d, 22, 8)
    pw.text(d, (76, 21), 'MPC OVERWITCH SEND', 19, 'black', pw.TEXT, 'lm', spacing=2.2)
    pw.text(d, (372, 22), 'NOXED · USB AUDIO SEND · 0.5.1 PREVIEW', 12, 'semibold', pw.MUTED, 'lm', spacing=1.6)
    children = []
    pw.panel_box(d, (12, 48, 412, 616), 'SEND', subtitle='MPC TRACK TO DEVICE')
    pw.text(d, (36, 104), 'DESTINATION', 12, 'bold', pw.MUTED, 'lm', spacing=1.4)
    children.append(pw.entry('p1', 'owDest', ui.obj(('version', 1), ('handleName', 'Data')), '36 120 352 48',
                             [('Data', 'Parameter 1')], 'Yes', None, 'Hide'))
    pw.text(d, (36, 186), 'Double-tap to choose a device input.', 12, 'medium', pw.DIM, 'lm')
    pw.line(d, [(28, 214), (396, 214)], '#232835', 1)
    for x, prm, name in [(56, 2, 'SEND'), (228, 0, 'DRY')]:
        children.append(pw.entry('p%d' % prm, 'owKnob', ui.obj(('version', 1), ('handleName', 'Data')), '%d 240 140 118' % x,
                                 [('Data', 'Parameter %d' % prm)], 'Yes', None, 'Hide'))
        pw.knob_name(d, x, 240, name)
    pw.line(d, [(28, 384), (396, 384)], '#232835', 1)
    for i, t in enumerate(['Insert on any MPC track. SEND goes to', 'the chosen device input; DRY is what', 'stays on the track.',
                           '', 'Hear the result with MPC Overwitch', '(Source Main or the effect).']):
        pw.text(d, (36, 412 + i * 22), t, 12, 'medium', pw.MUTED, 'lm')
    pw.text(d, (36, 590), 'Based on Overwitch by David Garcia Goni', 10, 'medium', pw.DIM, 'lm')
    pw.panel_box(d, (424, 48, 1268, 364), 'STATUS', subtitle='SHARED USB CONNECTION · LEVELS: IN = AT THIS PLUGIN, USB = SENT TO DEVICE')
    for i, (prm, name) in enumerate(SEND_ROWS):
        y = 100 + i * 64
        if i:
            pw.line(d, [(440, y - 10), (1252, y - 10)], '#232835', 1)
        pw.text(d, (452, y + 22), name, 13, 'bold', pw.MUTED, 'lm', spacing=1.4)
        children.append(pw.entry('row %d' % prm, 'Label',
                                 ui.label_data('Value', 'ffe65a46' if prm == 3 else 'ffe4e9f2', 28.0 if prm == 3 else 22.0,
                                               'left verticallyCentred'), '600 %d 650 44' % y, [('Data', 'Parameter %d' % prm)]))
    pw.panel_box(d, (424, 376, 1268, 616), 'ROUTING', subtitle='DEVICE INPUTS')
    rows = [('MAIN', 'Overbridge input Main L / R'), ('DELAY', 'Overbridge input Delay L / R (effect send)'),
            ('REVERB', 'Overbridge input Reverb L / R (effect send)'), ('CHORUS', 'Overbridge input Chorus L / R (effect send)')]
    for i, (a, b) in enumerate(rows):
        y = 436 + i * 40
        pw.rrect(d, (452, y - 16, 572, y + 16), pw.FIELD, '#2a2f3b', 1, 5)
        pw.text(d, (512, y), a, 13, 'bold', pw.ACCENT, 'mm', spacing=1.2)
        pw.text(d, (596, y), b, 14, 'medium', pw.TEXT, 'lm')
    pw.text(d, (452, 590), 'Other Overbridge devices list their own inputs in DESTINATION.', 12, 'medium', pw.DIM, 'lm')

    pw.strip(lambda v: pw.knob_frame(v, pw.KN, False), pw.FRAMES, pw.KN, pw.KN, out / 'knob.png')
    pw.field_img(352, 48).save(out / 'field.png')
    pw.finish(im, W, H).convert('RGB').save(out / 'panel.png', optimize=True)
    defs = [
        ('owKnob', ui.component_definition(ui.control_actions('knob overlay'), [
            pw.focus(140, 118),
            pw.entry('knob', 'Knob', ui.knob_filmstrip_data('knob.png', pw.FRAMES), '%d 0 %d %d' % ((140 - pw.KN) // 2, pw.KN, pw.KN)),
            pw.entry('value', 'Label', ui.label_data('Value', pw.VALUE_COL, 19.0, 'horizontallyCentred verticallyCentred'), '0 88 140 28')], '0')),
        ('owDest', ui.component_definition(ui.control_actions('menu overlay'), [
            pw.entry('box', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'field.png')), '0 0 352 48'),
            pw.focus(352, 48),
            pw.entry('value', 'Label', ui.label_data('Value', pw.FIELD_TEXT, 22.0, 'left verticallyCentred'), '16 0 300 48')], '0')),
        ('knob overlay', ui.overlay_component('KnobOverlay', ui.knob_overlay_data(False), 'ff12141a')),
        ('menu overlay', ui.overlay_component('MenuOverlay', ui.menu_overlay_data(), 'ff12141a')),
    ]
    bg = pw.entry('panel', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'panel.png')),
                  '0 0 1280 628')
    tab = ui.obj(('version', 1), ('tabName', 'SEND'), ('fnKeyIndex', 0), ('fnKeySubIndex', 0),
                 ('componentDefinition', ui.component_definition([], [bg] + children, 'ff12141a')),
                 ('qlinkBoundsData', ['20 92 384 280']))
    choices = next(p['choices'] for p in params['params'] if p['index'] == 1)
    data = ui.obj(('pageData', ui.obj(
        ('version', 1),
        ('componentDefinitions', ui.obj(('version', 2), ('importFiles', []),
                                        ('localComponentDefinitions', [ui.obj(('key', k), ('value', v)) for k, v in defs]))),
        ('replacementStrings', [ui.obj(('key', 'Parameter 1'), ('value', list(choices)))]),
        ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
        ('tabs', [tab]))))
    qmap = ui.obj(('Tab', 1), ('SubTab', 1), ('Bank Direction', 'RowTopDown'),
                  ('Q-Links', ui.Obj(('Q-Link %d' % q, {13: 1, 14: 2, 15: 0}.get(q, -1)) for q in range(1, 17))))
    qlinks = ui.obj(('version', 4), ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
                    ('Screen Mode Q-Links', ui.obj(('version', 4), ('map', [qmap]))),
                    ('Program Mode Q-Links', ui.Obj(('Q-Link %d' % (q + 1), {0: 2, 1: 0, 2: 1}.get(q, -1)) for q in range(16))))
    (out / 'TUI.json').write_text(ui.dump_json(data) + '\n')
    (out / 'Q-Links.json').write_text(ui.dump_json(qlinks) + '\n')
    return out


BRIDGE_SKINS = ROOT / 'plugins' / 'MPC USB Bridge' / 'skins'
BRIDGE_ROWS = [(3, 'STATUS'), (4, 'DEVICE'), (5, 'STREAM'), (6, 'LEVELS'), (7, 'SYSTEM')]


def build_bridge(params):
    """MPC USB Bridge 0.2: LEVEL knob (0), CARD field (1), status rows (3-7), REPORT knob (2)."""
    out = BRIDGE_SKINS
    out.mkdir(parents=True, exist_ok=True)
    im, d = pw.canvas(W, H, pw.hex_rgb(pw.BG))
    pw.rrect(d, (0, 0, W, 42), '#0d0f14')
    pw.line(d, [(0, 42), (W, 42)], '#232835', 1)
    pw.prism_logo(d, 22, 8)
    pw.text(d, (76, 21), 'MPC USB BRIDGE', 19, 'black', pw.TEXT, 'lm', spacing=2.2)
    pw.text(d, (290, 22), 'NOXED · USB AUDIO DEVICE AS A PLUGIN TRACK (ROLAND S-1 ...) · 0.2.1', 12, 'semibold', pw.MUTED, 'lm', spacing=1.6)
    children = []
    pw.panel_box(d, (12, 48, 412, 436), 'INPUT', subtitle='USB DEVICE TO THIS TRACK')
    children.append(pw.entry('p0', 'ubKnob', ui.obj(('version', 1), ('handleName', 'Data')), '36 96 140 118',
                             [('Data', 'Parameter 0')], 'Yes', None, 'Hide'))
    pw.knob_name(d, 36, 96, 'LEVEL')
    pw.text(d, (212, 104), 'MIDI CHANNEL', 12, 'bold', pw.MUTED, 'lm', spacing=1.4)
    children.append(pw.entry('p8', 'ubMidi', ui.obj(('version', 1), ('handleName', 'Data')), '212 120 176 48',
                             [('Data', 'Parameter 8')], 'Yes', None, 'Hide'))
    for i, t in enumerate(['Notes and CC of this', 'track to the device.']):
        pw.text(d, (212, 190 + i * 20), t, 12, 'medium', pw.DIM, 'lm')
    pw.line(d, [(28, 250), (396, 250)], '#232835', 1)
    pw.text(d, (36, 276), 'CARD', 12, 'bold', pw.MUTED, 'lm', spacing=1.4)
    children.append(pw.entry('p1', 'ubCard', ui.obj(('version', 1), ('handleName', 'Data')), '36 292 352 48',
                             [('Data', 'Parameter 1')], 'Yes', None, 'Hide'))
    for i, t in enumerate(['Auto takes the first USB audio device', 'that is not the MPC. S-1: AIRA LINK off.']):
        pw.text(d, (36, 366 + i * 22), t, 12, 'medium', pw.DIM, 'lm')
    pw.panel_box(d, (424, 48, 1268, 436), 'STATUS', subtitle='UPDATES TWICE A SECOND')
    for i, (prm, name) in enumerate(BRIDGE_ROWS):
        y = 96 + i * 66
        if i:
            pw.line(d, [(440, y - 11), (1252, y - 11)], '#232835', 1)
        pw.text(d, (452, y + 22), name, 13, 'bold', pw.MUTED, 'lm', spacing=1.4)
        children.append(pw.entry('row %d' % prm, 'Label',
                                 ui.label_data('Value', 'ffe65a46' if prm == 3 else 'ffe4e9f2', 26.0 if prm == 3 else 19.0,
                                               'left verticallyCentred'), '580 %d 676 44' % y, [('Data', 'Parameter %d' % prm)]))
    pw.panel_box(d, (12, 448, 1268, 616), 'REPORT', subtitle='TURN TO SCROLL · FULL TEXT IN MPC-USB-Bridge-Report.txt')
    children.append(pw.entry('p2', 'ubReport', ui.obj(('version', 1), ('handleName', 'Data')), '28 496 1224 104',
                             [('Data', 'Parameter 2')], 'Yes', None, 'Hide'))
    pw.strip(lambda v: pw.knob_frame(v, pw.KN, False), pw.FRAMES, pw.KN, pw.KN, out / 'knob.png')
    pw.field_img(352, 48).save(out / 'field.png')
    pw.field_img(176, 48).save(out / 'field-small.png')
    pw.finish(im, W, H).convert('RGB').save(out / 'panel.png', optimize=True)
    defs = [
        ('ubMidi', ui.component_definition(ui.control_actions('menu overlay'), [
            pw.entry('box', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'field-small.png')), '0 0 176 48'),
            pw.focus(176, 48),
            pw.entry('value', 'Label', ui.label_data('Value', pw.FIELD_TEXT, 22.0, 'left verticallyCentred'), '16 0 130 48')], '0')),
        ('menu overlay', ui.overlay_component('MenuOverlay', ui.menu_overlay_data(), 'ff12141a')),
        ('ubKnob', ui.component_definition(ui.control_actions('knob overlay'), [
            pw.focus(140, 118),
            pw.entry('knob', 'Knob', ui.knob_filmstrip_data('knob.png', pw.FRAMES), '%d 0 %d %d' % ((140 - pw.KN) // 2, pw.KN, pw.KN)),
            pw.entry('value', 'Label', ui.label_data('Value', pw.VALUE_COL, 19.0, 'horizontallyCentred verticallyCentred'), '0 88 140 28')], '0')),
        ('ubCard', ui.component_definition(ui.control_actions('knob overlay'), [
            pw.entry('box', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'field.png')), '0 0 352 48'),
            pw.focus(352, 48),
            pw.entry('value', 'Label', ui.label_data('Value', pw.FIELD_TEXT, 20.0, 'left verticallyCentred'), '16 0 330 48')], '0')),
        ('ubReport', ui.component_definition(ui.control_actions('knob overlay', False), [
            pw.focus(1224, 104),
            pw.entry('knob', 'Knob', ui.knob_filmstrip_data('knob.png', pw.FRAMES), '20 20 %d %d' % (pw.KN, pw.KN)),
            pw.entry('value', 'Label', ui.label_data('Value', 'ffe4e9f2', 26.0, 'left verticallyCentred'), '110 20 1100 64')], '0')),
        ('knob overlay', ui.overlay_component('KnobOverlay', ui.knob_overlay_data(False), 'ff12141a')),
    ]
    bg = pw.entry('panel', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'panel.png')),
                  '0 0 1280 628')
    tab = ui.obj(('version', 1), ('tabName', 'USB'), ('fnKeyIndex', 0), ('fnKeySubIndex', 0),
                 ('componentDefinition', ui.component_definition([], [bg] + children, 'ff12141a')),
                 ('qlinkBoundsData', ['20 88 384 264']))
    data = ui.obj(('pageData', ui.obj(
        ('version', 1),
        ('componentDefinitions', ui.obj(('version', 2), ('importFiles', []),
                                        ('localComponentDefinitions', [ui.obj(('key', k), ('value', v)) for k, v in defs]))),
        ('replacementStrings', [ui.obj(('key', 'Parameter 8'), ('value', MIDI_CHOICES))]),
        ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
        ('tabs', [tab]))))
    # Bottom row of the hardware knobs: LEVEL, CARD, REPORT, MIDI CH.
    qmap = ui.obj(('Tab', 1), ('SubTab', 1), ('Bank Direction', 'RowTopDown'),
                  ('Q-Links', ui.Obj(('Q-Link %d' % q, {13: 0, 14: 1, 15: 2, 16: 8}.get(q, -1)) for q in range(1, 17))))
    qlinks = ui.obj(('version', 4), ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
                    ('Screen Mode Q-Links', ui.obj(('version', 4), ('map', [qmap]))),
                    ('Program Mode Q-Links', ui.Obj(('Q-Link %d' % (q + 1), {0: 0, 1: 1, 2: 2, 3: 8}.get(q, -1)) for q in range(16))))
    (out / 'TUI.json').write_text(ui.dump_json(data) + '\n')
    (out / 'Q-Links.json').write_text(ui.dump_json(qlinks) + '\n')
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--params', type=Path, required=True)
    ap.add_argument('--send', action='store_true', help='build the MPC Overwitch Send skin')
    ap.add_argument('--bridge', action='store_true', help='build the MPC USB Bridge skin')
    ap.add_argument('--preview', type=Path, default=ROOT / 'docs' / 'screenshots')
    args = ap.parse_args()
    params = json.loads(args.params.read_text())
    if args.bridge:
        pw.SKINS = build_bridge(params)
        name = 'mpc-usb-bridge.png'
    elif args.send:
        pw.SKINS = build_send(params)
        name = 'mpc-overwitch-send.png'
    else:
        build(params)
        pw.SKINS = SKINS
        name = 'mpc-overwitch.png'
    args.preview.mkdir(parents=True, exist_ok=True)
    for i, f in enumerate(pw.render_preview(params, args.preview)):
        target = f.with_name(name if i == 0 else name.replace('.png', '-%s.png' % f.stem.split('-')[-1]))
        f.replace(target)
        print('preview', target)


if __name__ == '__main__':
    main()
