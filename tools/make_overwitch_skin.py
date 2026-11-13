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
ROWS = [(2, 'STATUS'), (3, 'DEVICE'), (4, 'USB MODE'), (5, 'DRIVER'), (6, 'ACCESS'), (7, 'CLAIM'), (8, 'SYSTEM')]


def build():
    SKINS.mkdir(parents=True, exist_ok=True)
    im, d = pw.canvas(W, H, pw.hex_rgb(pw.BG))
    pw.rrect(d, (0, 0, W, 42), '#0d0f14')
    pw.line(d, [(0, 42), (W, 42)], '#232835', 1)
    pw.prism_logo(d, 22, 8)
    pw.text(d, (76, 21), 'MPC OVERWITCH', 19, 'black', pw.TEXT, 'lm', spacing=2.2)
    pw.text(d, (268, 22), 'NOXED · OVERBRIDGE FOR MPC · STAGE 1: USB TEST · NO SOUND YET', 12, 'semibold', pw.MUTED, 'lm', spacing=1.6)

    children = []
    # Status panel.
    pw.panel_box(d, (12, 48, 1268, 436), 'USB STATUS', subtitle='RESCANS EVERY 2 S')
    for i, (prm, name) in enumerate(ROWS):
        y = 96 + i * 48
        if i:
            pw.line(d, [(28, y - 4), (1252, y - 4)], '#232835', 1)
        pw.text(d, (40, y + 20), name, 13, 'bold', pw.MUTED, 'lm', spacing=1.4)
        colour = 'ff46c8f0' if prm == 2 else 'ffe4e9f2'
        children.append(pw.entry('row %d' % prm, 'Label',
                                 ui.label_data('Value', colour, 26.0 if prm == 2 else 22.0, 'left verticallyCentred'),
                                 '220 %d 1030 40' % y, [('Data', 'Parameter %d' % prm)]))
    # Report panel.
    pw.panel_box(d, (12, 448, 1268, 616), 'REPORT', subtitle='TURN THE FIRST Q-LINK OR DRAG TO SCROLL · FULL TEXT IN MPC-Overwitch-Report.txt')
    children.append(pw.entry('p0', 'owReport', ui.obj(('version', 1), ('handleName', 'Data')), '28 496 900 104',
                             [('Data', 'Parameter 0')], 'Yes', None, 'Hide'))
    pw.text(d, (1040, 548), 'CLAIM TEST', 13, 'bold', pw.MUTED, 'mm', spacing=1.4)
    children.append(pw.entry('p1', 'owToggle', ui.obj(('version', 1), ('handleName', 'Data')), '1150 534 58 28',
                             [('Data', 'Parameter 1')], 'Yes', None, 'Hide'))

    pw.strip(lambda v: pw.knob_frame(v, pw.KN, False), pw.FRAMES, pw.KN, pw.KN, SKINS / 'knob.png')
    pw.toggle_img(True).save(SKINS / 'toggle-on.png')
    pw.toggle_img(False).save(SKINS / 'toggle-off.png')
    pw.finish(im, W, H).convert('RGB').save(SKINS / 'panel.png', optimize=True)

    defs = [
        ('owReport', ui.component_definition(ui.control_actions('knob overlay', False), [
            pw.focus(900, 104),
            pw.entry('knob', 'Knob', ui.knob_filmstrip_data('knob.png', pw.FRAMES), '20 20 %d %d' % (pw.KN, pw.KN)),
            pw.entry('value', 'Label', ui.label_data('Value', 'ffe4e9f2', 26.0, 'left verticallyCentred'), '110 20 780 64')], '0')),
        ('owToggle', ui.component_definition(ui.control_actions('menu overlay', False), [
            pw.focus(58, 28), pw.entry('toggle', 'Button', ui.button_data('toggle-on.png', 'toggle-off.png', 1, 1), '0 0 58 28')], '0')),
        ('knob overlay', ui.overlay_component('KnobOverlay', ui.knob_overlay_data(False), 'ff12141a')),
        ('menu overlay', ui.overlay_component('MenuOverlay', ui.menu_overlay_data(), 'ff12141a')),
    ]
    bg = pw.entry('panel', 'Image', ui.obj(('version', 2), ('imageType', 'Regular'), ('colour', '0'), ('image', 'panel.png')),
                  '0 0 1280 628')
    tab = ui.obj(('version', 1), ('tabName', 'USB'), ('fnKeyIndex', 0), ('fnKeySubIndex', 0),
                 ('componentDefinition', ui.component_definition([], [bg] + children, 'ff12141a')),
                 ('qlinkBoundsData', ['20 488 1240 120']))
    # No replacementStrings: every value text must come live from the plugin.
    data = ui.obj(('pageData', ui.obj(
        ('version', 1),
        ('componentDefinitions', ui.obj(('version', 2), ('importFiles', []),
                                        ('localComponentDefinitions', [ui.obj(('key', k), ('value', v)) for k, v in defs]))),
        ('replacementStrings', []),
        ('info', ui.obj(('version', 1), ('type', 'CompleteDescription'))),
        ('tabs', [tab]))))
    qmap = ui.obj(('Tab', 1), ('SubTab', 1), ('Bank Direction', 'RowTopDown'),
                  ('Q-Links', ui.Obj(('Q-Link %d' % q, {13: 0, 14: 1}.get(q, -1)) for q in range(1, 17))))
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
    build()
    pw.SKINS = SKINS
    params = json.loads(args.params.read_text())
    args.preview.mkdir(parents=True, exist_ok=True)
    for f in pw.render_preview(params, args.preview):
        target = f.with_name('mpc-overwitch.png')
        f.replace(target)
        print('preview', target)


if __name__ == '__main__':
    main()
