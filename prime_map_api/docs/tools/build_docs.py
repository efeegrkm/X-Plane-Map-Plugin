import html
import os
import re
import sys

import markdown

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TOOLS = os.path.join(ROOT, 'tools')

CSS = """
:root { --ink: #1c2330; --muted: #5a6474; --line: #d9dee6; --soft: #f4f6f9; --accent: #1f5fa8; }
* { box-sizing: border-box; }
body { margin: 0; background: #ffffff; color: var(--ink);
       font-family: "Segoe UI", "Inter", "Helvetica Neue", Arial, sans-serif; font-size: 10.5pt; line-height: 1.55; }
main { max-width: 860px; margin: 0 auto; padding: 40px 28px 64px; }
.cover { border-bottom: 2px solid var(--accent); padding-bottom: 18px; margin-bottom: 28px; }
.cover h1 { font-size: 26pt; margin: 0; border: none; padding: 0; }
.cover .subtitle { font-size: 14pt; color: var(--muted); margin-top: 4px; }
.cover .meta { font-size: 9.5pt; color: var(--muted); margin-top: 10px; }
nav.toc { background: var(--soft); border: 1px solid var(--line); border-radius: 6px; padding: 14px 20px; margin-bottom: 32px; }
nav.toc .toctitle { font-weight: 600; margin-bottom: 6px; }
nav.toc ul { margin: 0; padding-left: 18px; }
nav.toc li { margin: 2px 0; }
nav.toc a { color: var(--ink); text-decoration: none; }
nav.toc > ul > li > ul { font-size: 9.5pt; color: var(--muted); }
h1 { font-size: 17pt; margin: 40px 0 12px; padding-bottom: 6px; border-bottom: 1px solid var(--line); color: var(--accent); }
h2 { font-size: 13pt; margin: 28px 0 8px; }
h3 { font-size: 11pt; margin: 20px 0 6px; }
p { margin: 8px 0; }
ul, ol { margin: 8px 0; padding-left: 22px; }
li { margin: 3px 0; }
code { font-family: "Cascadia Mono", Consolas, "DejaVu Sans Mono", monospace; font-size: 9pt;
       background: var(--soft); padding: 1px 4px; border-radius: 3px; }
pre { background: #f6f8fa; border: 1px solid var(--line); border-radius: 5px; padding: 10px 12px; overflow-x: auto;
      font-size: 8.8pt; line-height: 1.45; }
pre code { background: none; padding: 0; font-size: inherit; }
table { border-collapse: collapse; width: 100%; margin: 10px 0 14px; font-size: 9.3pt; }
th, td { border: 1px solid var(--line); padding: 5px 8px; text-align: left; vertical-align: top; }
th { background: var(--soft); font-weight: 600; }
tr.group td { background: #eaf0f8; font-weight: 600; }
td.dots { white-space: nowrap; letter-spacing: 1px; }
strong { font-weight: 600; }
a { color: var(--accent); }
@media (max-width: 640px) { main { padding: 24px 16px 48px; } table { font-size: 8.5pt; } }
@media print {
  main { max-width: none; padding: 0; }
  h1 { page-break-before: always; }
  .cover + nav.toc + h1, .cover + h1 { page-break-before: avoid; }
  pre, table, tr { page-break-inside: avoid; }
  h1, h2, h3 { page-break-after: avoid; }
}
@page { size: A4; margin: 16mm 15mm 16mm 15mm; }
"""

ON = '●'
OFF = '○'


def layer_table():
    rows = []
    path = os.path.join(TOOLS, 'layer_table.tsv')
    with open(path, encoding='utf-8') as f:
        lines = [line.rstrip('\n').split('\t') for line in f if line.strip()]
    enum_names = []
    with open(os.path.join(ROOT, '..', 'src', 'api', 'XMapTypes.h'), encoding='utf-8') as f:
        text = f.read()
    body = text[text.index('enum class XMapLayer'):]
    body = body[body.index('{') + 1:body.index('}')]
    for token in re.split(r'[,\s]+', body):
        if token and token != 'Count':
            enum_names.append(token)
    colorable = {'Grid', 'RoadsMotorways', 'RoadsPrimary', 'RoadsSecondary', 'RoadsLocal', 'RoadsRailways',
                 'RoadsPowerLines', 'TerrainBuiltUp', 'Vor', 'Ndb', 'Ils', 'Fixes', 'AirwaysLow', 'AirwaysHigh',
                 'Runways'}
    out = ['<table class="layers">',
           '<tr><th>XMapLayer</th><th>Anahtar</th><th>Paneldeki ad</th><th>Eşik</th><th>Sert alt sınır</th>'
           '<th>S V L H D</th><th>Renk</th></tr>']
    for fields in lines:
        index, name, key, kind, parent, modes, zmin, zmax, zhard = fields
        enum = enum_names[int(index)]
        kind = int(kind)
        if kind == 0:
            out.append(f'<tr class="group"><td colspan="7">{html.escape(name)} grubu '
                       f'(<code>XMapLayer::{enum}</code>)</td></tr>')
            continue
        if kind == 1 and parent == '':
            out.append('<tr class="group"><td colspan="7">Gruba bağlı olmayan katmanlar</td></tr>')
        zmin_f = float(zmin)
        zhard_f = float(zhard)
        threshold = '' if zmin_f == 0.0 else f'{zmin_f:g}'
        hard = '' if zmin_f == 0.0 else f'{zhard_f:g}'
        if kind == 1 and zmin_f == 0.0 and enum in ('Grid', 'Aircraft', 'FmsRoute', 'FlightTrack', 'Drawings'):
            threshold = 'her zoom'
        label = ('&nbsp;&nbsp;&nbsp;' if kind == 2 else '') + f'<code>{enum}</code>'
        dots = ''.join(ON if c == '1' else OFF for c in modes)
        color = 'evet' if enum in colorable else ''
        out.append(f'<tr><td>{label}</td><td><code>{html.escape(key)}</code></td><td>{html.escape(name)}</td>'
                   f'<td>{threshold}</td><td>{hard}</td><td class="dots">{dots}</td><td>{color}</td></tr>')
    out.append('</table>')
    return '\n'.join(out)


def build(source, target, title_fallback):
    with open(source, encoding='utf-8') as f:
        text = f.read()
    meta = {}
    if text.startswith('---'):
        end = text.index('\n---', 3)
        for line in text[3:end].strip().split('\n'):
            k, v = line.split(':', 1)
            meta[k.strip()] = v.strip()
        text = text[end + 4:]
    if '<!-- LAYER_TABLE -->' in text:
        text = text.replace('<!-- LAYER_TABLE -->', layer_table())
    md = markdown.Markdown(extensions=['tables', 'fenced_code', 'toc', 'sane_lists'],
                           extension_configs={'toc': {'toc_depth': '1-2', 'title': 'İçindekiler'}})
    body = md.convert(text)
    toc = md.toc.replace('<div class="toc">', '<nav class="toc">').replace('</div>', '</nav>')
    toc = toc.replace('<span class="toctitle">', '<div class="toctitle">').replace('</span>', '</div>')
    title = meta.get('title', title_fallback)
    cover = (f'<div class="cover"><h1>{html.escape(title)}</h1>'
             f'<div class="subtitle">{html.escape(meta.get("subtitle", ""))}</div>'
             f'<div class="meta">Sürüm {html.escape(meta.get("version", ""))} &middot; '
             f'{html.escape(meta.get("date", ""))}</div></div>')
    page = ('<!doctype html>\n<html lang="tr">\n<head>\n<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            f'<title>{html.escape(title)} {html.escape(meta.get("version", ""))}</title>\n'
            f'<style>{CSS}</style>\n</head>\n<body>\n<main>\n{cover}\n{toc}\n{body}\n</main>\n</body>\n</html>\n')
    with open(target, 'w', encoding='utf-8', newline='\n') as f:
        f.write(page)
    return target


def to_pdf(html_path, pdf_path):
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()
        page.goto('file://' + os.path.abspath(html_path))
        page.pdf(path=pdf_path, format='A4', print_background=True,
                 margin={'top': '16mm', 'bottom': '16mm', 'left': '15mm', 'right': '15mm'},
                 display_header_footer=True, header_template='<span></span>',
                 footer_template='<div style="font-size:8px;width:100%;text-align:center;color:#777">'
                                 '<span class="pageNumber"></span> / <span class="totalPages"></span></div>')
        browser.close()


if __name__ == '__main__':
    docs = [('PrimeMap_API.md', 'PrimeMap_API', 'Prime XMap API'), ('MIMARI.md', 'MIMARI', 'Prime XMap Mimari')]
    for source, name, fallback in docs:
        src = os.path.join(TOOLS, source)
        if not os.path.exists(src):
            continue
        out = build(src, os.path.join(ROOT, name + '.html'), fallback)
        if '--pdf' in sys.argv:
            to_pdf(out, os.path.join(ROOT, name + '.pdf'))
        print('built', out)
