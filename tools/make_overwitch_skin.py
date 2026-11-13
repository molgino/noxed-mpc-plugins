#!/usr/bin/env python3
"""Generate the MPC Overwitch skin (one page: USB status rows, REPORT knob, CLAIM TEST switch).

Reuses the drawing helpers of make_prismwave_skin.py so both NOXED plugins look alike.
    python tools/make_overwitch_skin.py --params "plugins/MPC Overwitch/build/params.json"
"""
import argparse, json
from pathlib import Path
import gen_ui as ui
import make_prismwave_skin as pw

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'plugins' / 'MPC Overwitch'
SKINS = PROJECT / 'skins'
W, H = pw.W, pw.H
ROWS = [(3, 'STATUS'), (4, 'DEVICE'), (5, 'STREAM'), (6, 'DROPOUTS'), (7, 'SYSTEM')]


def build(params):
    SKINS.mkdir(parents=True, exist_ok=True)
    im, d = pw.canvas(W, H, pw.hex_rgb(pw.BG))
    pw.rrect(d, (0, 0, W, 42), '#0d0f14')
    pw.line(d, [(0, 42), (W, 42)], '#232835', 1)
    pw.prism_logo(d, 22, 8)
    pw.text(d, (76, 21), 'MPC OVERWITCH', 19, 'black', pw.TEXT, 'lm', spacing=2.2)
    pw.text(d, (300, 22), 'NOXED · OVERBRIDGE FOR THE MPC · 0.2 PREVIEW', 12, 'semibold', pw.MUTED, 'lm', spacing=1.6)

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
    for i, t in enumerate(['Digitone II / Digitakt II and other', 'Overbridge 2 devices. Set the device',
                           'USB config to OVERBRIDGE.', '', 'One instance owns the device;', 'more instances stay silent.']):
        pw.text(d, (36, 412 + i * 22), t, 12, 'medium', pw.MUTED if t else pw.DIM, 'lm')
    pw.text(d, (36, 590), 'Based on Overwitch by David Garcia Goni', 10, 'medium', pw.DIM, 'lm')

    # Status panel.
    pw.panel_box(d, (424, 48, 1268, 436), 'STATUS', subtitle='UPDATES BY ITSELF')
    for i, (prm, name) in enumerate(ROWS):
        y = 100 + i * 64
        if i:
            pw.line(d, [(440, y - 10), (1252, y - 10)], '#232835', 1)
        pw.text(d, (452, y + 22), name, 13, 'bold', pw.MUTED, 'lm', spacing=1.4)
        colour = 'ff46c8f0' if prm == 3 else 'ffe4e9f2'
        children.append(pw.entry('row %d' % prm, 'Label',
                                 ui.label_data('Value', colour, 28.0 if prm == 3 else 22.0, 'left verticallyCentred'),
                                 '600 %d 650 44' % y, [('Data', 'Parameter %d' % prm)]))
    # Report panel.
    pw.panel_box(d, (424, 448, 1268, 616), 'REPORT', subtitle='TURN OR DRAG TO SCROLL · ALSO IN MPC-Overwitch-Report.txt')
    children.append(pw.entry('p2', 'owReport', ui.obj(('version', 1), ('handleName', 'Data')), '440 496 812 104',
                             [('Data', 'Parameter 2')], 'Yes', None, 'Hide'))

    pw.strip(lambda v: pw.knob_frame(v, pw.KN, False), pw.FRAMES, pw.KN, pw.KN, SKINS / 'knob.png')
    pw.field_img(352, 48).save(SKINS / 'field.png')
    pw.finish(im, W, H).convert('RGB').save(SKINS / 'panel.png', optimize=True)

    defs = [
        ('owReport', ui.component_definition(ui.control_actions('knob overlay', False), [
            pw.focus(812, 104),
            pw.entry('knob', 'Knob', ui.knob_filmstrip_data('knob.png', pw.FRAMES), '20 20 %d %d' % (pw.KN, pw.KN)),
            pw.entry('value', 'Label', ui.label_data('Value', 'ffe4e9f2', 26.0, 'left verticallyCentred'), '110 20 690 64')], '0')),
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
                 ('qlinkBoundsData', ['20 92 384 280', '432 488 828 120']))
    # Only the Source list is static (the menu needs it); every other text comes live from the plugin.
    choices = next(p['choices'] for p in params['params'] if p['index'] == 1)
    data = ui.obj(('pageData', ui.obj(
        ('version', 1),
        ('componentDefinitions', ui.obj(('version', 2), ('importFiles', []),
                                        ('localComponentDefinitions', [ui.obj(('key', k), ('value', v)) for k, v in defs]))),
        ('replacementStrings', [ui.obj(('key', 'Parameter 1'), ('value', list(choices)))]),
        ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
        ('tabs', [tab]))))
    # Row 1 of the Q-Links: Source, Level; row 2: Report.
    qmap = ui.obj(('Tab', 1), ('SubTab', 1), ('Bank Direction', 'RowTopDown'),
                  ('Q-Links', ui.Obj(('Q-Link %d' % q, {13: 1, 14: 0, 9: 2}.get(q, -1)) for q in range(1, 17))))
    qlinks = ui.obj(('version', 4), ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
                    ('Screen Mode Q-Links', ui.obj(('version', 4), ('map', [qmap]))),
                    ('Program Mode Q-Links', ui.Obj(('Q-Link %d' % (q + 1), {0: 0, 1: 1}.get(q, -1)) for q in range(16))))
    (SKINS / 'TUI.json').write_text(ui.dump_json(data) + '\n')
    (SKINS / 'Q-Links.json').write_text(ui.dump_json(qlinks) + '\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--params', type=Path, required=True)
    ap.add_argument('--preview', type=Path, default=ROOT / 'docs' / 'screenshots')
    args = ap.parse_args()
    params = json.loads(args.params.read_text())
    build(params)
    pw.SKINS = SKINS
    args.preview.mkdir(parents=True, exist_ok=True)
    for f in pw.render_preview(params, args.preview):
        target = f.with_name('mpc-overwitch.png')
        f.replace(target)
        print('preview', target)


if __name__ == '__main__':
    main()
