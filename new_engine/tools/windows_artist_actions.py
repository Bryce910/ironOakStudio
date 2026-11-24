"""Double-click actions for the portable Windows artist edition."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import webbrowser
sys.dont_write_bytecode = True
from world_artist_handoff import create_workspace, export_return
from build_world_package import verify_package


def editor_command(root, extra):
    """Choose the delivered project's scene; explicit artist views still win."""
    project = root/'project'
    if (project/'city_showcase.json').is_file(): scenic = 'city-walk'
    elif (project/'stronghold.json').is_file():
        scenic = 'stronghold-town' if (project/'publisher_showcase.json').is_file() else 'stronghold'
    elif (project/'jungle_expansion.json').is_file(): scenic = 'rainforest-milestone'
    else: scenic = 'castle-finished-arcade'
    defaults = [] if '--scenic' in extra or any(a.startswith('--scenic=') for a in extra) else ['--scenic',scenic]
    return [str(root/'bin/rpg.exe'),'--project',str(project),'--editor',*defaults,*extra]


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('launch','create','export','verify','memory'))
    parser.add_argument('--output',type=Path)
    parser.add_argument('--no-open',action='store_true',help='Do not open the resulting folder in Explorer')
    args, extra = parser.parse_known_args()
    if extra and args.action != 'launch': parser.error('Unexpected arguments: '+' '.join(extra))
    try:
        if args.action == 'launch':
            if not (root/'artist-baseline.json').is_file():
                print('Run Create_Artist_Workspace.bat first, then Launch_Editor.bat in the new folder.')
                return 1
            (root/'logs').mkdir(exist_ok=True)
            log = root/'logs/editor-latest.log'
            print('Starting Valwood Studio. First load may take time. Log:',log,flush=True)
            with log.open('w',encoding='utf-8') as stream:
                code = subprocess.call(editor_command(root,extra),cwd=root,stdout=stream,stderr=subprocess.STDOUT)
            print('Editor exited with code',code,'. Log:',log)
            return code
        if args.action == 'memory':
            threading.Timer(1.0,lambda:webbrowser.open('http://127.0.0.1:8871/',new=2)).start()
            return subprocess.call([sys.executable,'-B',str(root/'tools/npc_memory_editor.py'),'--serve'],cwd=root)
        if args.action == 'verify': result = verify_package(root)
        else:
            timestamp = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
            destination = args.output or root.parent/('Valwood_Artist_Work_'+timestamp if args.action=='create' else 'Valwood_Artist_Return_'+timestamp)
            print('Checking and copying files. Please keep this window open...',flush=True)
            result = create_workspace(root,destination) if args.action=='create' else export_return(root,destination)
            print('Created:',destination,flush=True)
            if os.name == 'nt' and not args.no_open: os.startfile(destination)
        print(json.dumps(result,indent=2));return 0
    except (OSError,ValueError,KeyError,TypeError) as error:
        print('ERROR:',error);return 1


if __name__ == '__main__': raise SystemExit(main())
