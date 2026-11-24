#!/usr/bin/env python3
"""Bundle the local quest/story editor into a self-contained HTML and ZIP.

No npm dependencies, remote resources or engine installation are required.
Only the fixed, trusted application sources are bundled. Existing deliveries
are retained unless --replace is explicitly supplied during development.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
import zipfile

ENGINE = Path(__file__).resolve().parents[1]
SOURCE = ENGINE / 'tools/quest_story_editor'
HTML_NAME = 'Valwood_Quest_Story_Studio.html'
README = """VALWOOD QUEST & STORY STUDIO

Open Valwood_Quest_Story_Studio.html in a modern browser. Windows users may
double-click Open_Story_Studio.bat. No Python, Unity, account or internet is
needed for this portable edition. All application code is included in HTML.

Start with Example for a working quest offer, branching conversation,
objective and one-time reward. Story chapters and beats plan the father's
journey from everyday clothes to knight armor. These notes do not change
character models automatically.

Save project creates a portable .vwstory.json file including your outline,
layout and gameplay. Browser draft storage is a convenience; save project
files for backup and handoff, especially when using a local HTML file.
Open accepts saved story projects and native .gameplay files. Native files
do not contain your authoring outline or layout.

Export gameplay produces the engine's VALWOOD_GAMEPLAY 1 format. Test the
quest and dialogue in Preview first. Scene entity IDs and world coordinates
must match the intended game project; the example uses placeholders.

INTEGRATION
Use a duplicate artist project and fresh progress when trying new content.
In the game editor's Live components panel, preserve existing physics,
living and entity bindings, select the exported Gameplay asset, Save
manifest, then Load manifest + referenced assets. The world.components
manifest must reference the new gameplay file. A .gameplay file dropped
into a project by itself does not activate. Existing journey save signatures
do not migrate automatically after content changes.

Dedicated environment playtest slices currently freeze quest runtime.
Authored combat/spawns/cinematics need their own supported game-host hooks.
Quest objectives are concurrent unless gated explicitly by conditions/rules.
Counter rewards in this tool are gameplay counters, not physical gear items.

The source folder contains the editable app and optional local Python server.
The portable HTML edition does not need them to run.
"""


def digest(data):
    return hashlib.sha256(data).hexdigest()


def bundle_html():
    dist = SOURCE / 'dist'
    index = (dist / 'index.html').read_text()
    css = (dist / 'style.css').read_text()
    core = (dist / 'story.mjs').read_text()
    app = (dist / 'app.mjs').read_text()
    statement = r"^import \* as Story from ['\"]\./story\.mjs['\"];?\s*$"
    if len(re.findall(statement, app, re.M)) != 1:
        raise ValueError('Expected one explicit local Story namespace import')
    app = re.sub(statement, '', app, count=1, flags=re.M)
    if re.search(r'^\s*import\b', app, re.M) or re.search(r'^\s*import\b', core, re.M):
        raise ValueError('Portable app must have no remaining module imports')
    # Discover the real named exports instead of maintaining a second API list.
    script = "import {pathToFileURL} from 'node:url'; const m=await import(pathToFileURL(process.argv[1])); console.log(JSON.stringify(Object.keys(m)));"
    names = json.loads(subprocess.check_output(
        ['node', '--input-type=module', '-e', script, str(dist / 'story.mjs')], text=True))
    if not names or any(not re.fullmatch(r'[A-Za-z_$][A-Za-z0-9_$]*', name) for name in names):
        raise ValueError('Unsupported Story export names')
    core = re.sub(r'^export (?=(?:const|let|var|function|class)\b)', '', core, flags=re.M)
    if re.search(r'^\s*export\b', core, re.M):
        raise ValueError('Unsupported Story export declaration')
    inline = 'const Story=(()=>{\n' + core + '\nreturn {' + ','.join(names) + '};\n})();\n' + app
    with tempfile.TemporaryDirectory(prefix='valwood-story-bundle-') as temporary:
        path = Path(temporary) / 'studio.mjs'
        path.write_text(inline)
        subprocess.run(['node', '--check', str(path)], check=True, capture_output=True)
    inline = re.sub(r'</script', r'<\/script', inline, flags=re.I)
    css = re.sub(r'</style', r'<\/style', css, flags=re.I)
    style_tag = '<link rel="stylesheet" href="style.css">'
    module_tag = '<script type="module" src="app.mjs"></script>'
    if index.count(style_tag) != 1 or index.count(module_tag) != 1:
        raise ValueError('Unexpected editor HTML resource references')
    policy = "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src data: blob:; connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'"
    index = index.replace('<head>', '<head><meta http-equiv="Content-Security-Policy" content="' + policy + '">', 1)
    index = index.replace(style_tag, '<style>\n' + css + '\n</style>', 1)
    index = index.replace(module_tag, '<script type="module">\n' + inline + '\n</script>', 1)
    return index.encode()


def build(output, archive, replace=False):
    output = Path(output).resolve()
    archive = Path(archive).resolve()
    if output == archive or (not replace and (output.exists() or archive.exists())):
        raise ValueError('Choose new delivery filenames or explicitly use --replace')
    html = bundle_html()
    members = {
        HTML_NAME: html,
        'README.txt': README.encode(),
        'Open_Story_Studio.bat': ('@echo off\r\nstart "" "%~dp0' + HTML_NAME + '"\r\n').encode(),
    }
    for name in ('index.html', 'style.css', 'app.mjs', 'story.mjs'):
        members['source/tools/quest_story_editor/dist/' + name] = (SOURCE / 'dist' / name).read_bytes()
    for name in ('compile.mjs',):
        members['source/tools/quest_story_editor/' + name] = (SOURCE / name).read_bytes()
    members['source/tools/quest_story_editor.py'] = (ENGINE / 'tools/quest_story_editor.py').read_bytes()
    members['source/tools/build_quest_story_studio.py'] = Path(__file__).read_bytes()
    for path in sorted((SOURCE / 'examples').glob('*')):
        if path.is_file() and not path.is_symlink():
            members['examples/' + path.name] = path.read_bytes()
            members['source/tools/quest_story_editor/examples/' + path.name] = path.read_bytes()
    manifest = {
        'schema': 'valwood.quest-story-portable', 'version': 1,
        'entry': HTML_NAME, 'internet_required': False, 'python_required': False,
        'files': {name: {'bytes': len(data), 'sha256': digest(data)} for name, data in sorted(members.items())},
    }
    members['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    archive.parent.mkdir(parents=True, exist_ok=True)
    # Build the archive before installing either final artifact.
    with tempfile.TemporaryDirectory(prefix='valwood-story-delivery-', dir=str(archive.parent)) as temporary:
        pending = Path(temporary) / 'studio.zip'
        with zipfile.ZipFile(pending, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as packed:
            for name, data in sorted(members.items()):
                info = zipfile.ZipInfo(name, (2026, 11, 2, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                packed.writestr(info, data)
        with zipfile.ZipFile(pending) as packed:
            if packed.testzip() is not None or set(packed.namelist()) != set(members):
                raise ValueError('Portable archive validation failed')
            for name, data in members.items():
                if packed.read(name) != data:
                    raise ValueError('Portable archive content mismatch: ' + name)
        pending_html = Path(temporary) / HTML_NAME
        pending_html.write_bytes(html)
        pending_html.replace(output)
        pending.replace(archive)
    return {'passed': True, 'html': str(output), 'html_bytes': len(html), 'html_sha256': digest(html),
            'zip': str(archive), 'zip_bytes': archive.stat().st_size,
            'zip_sha256': digest(archive.read_bytes()), 'members': len(members),
            'source_files': manifest['files']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--html', type=Path, default=ENGINE.parent / HTML_NAME)
    parser.add_argument('--zip', type=Path, default=ENGINE.parent / 'Valwood_Quest_Story_Studio_2026-11-02.zip')
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    try:
        print(json.dumps(build(args.html, args.zip, args.replace), indent=2))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
