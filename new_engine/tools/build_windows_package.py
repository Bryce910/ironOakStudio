#!/usr/bin/env python3
"""Build a portable Windows x64 Valwood artist/runtime package. Standard library only."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from world_asset_database import ProjectInventory, ValidationError, VERSION, canonical_json, sha256_bytes, write_new_json
from build_world_package import (_copy_checked, _write, _publish_directory, _native_hash,
    _extra_inputs, verify_package, validation_project)

SYSTEM_DLLS = set('kernel32 ntdll msvcrt ucrtbase advapi32 bcrypt user32 gdi32 imm32 ole32 oleaut32 shell32 version winmm setupapi ws2_32 dinput8 opengl32 rpcrt4 secur32 comdlg32 dwmapi shcore comctl32 crypt32 iphlpapi powrprof winspool userenv shlwapi dbghelp psapi wintrust cfgmgr32 hid msacm32 avrt propsys normaliz usp10'.split())
SYSTEM_DLLS.update(('cabinet','msi','netapi32','pathcch','wldap32','dhcpcsvc'))


def windows_names(names):
    seen = set()
    for name in names:
        parts = name.split('/')
        if any(not p or re.search(r'[<>:"\\|?*\x00-\x1f]', p) or p.endswith(('.', ' '))
               or re.fullmatch(r'(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?', p, re.I) for p in parts):
            raise ValidationError('Invalid Windows filename: '+name)
        key = name.casefold()
        if key in seen:
            raise ValidationError('Windows filename collision: '+name)
        seen.add(key)


def pe_imports(path):
    """Inspect AMD64 PE import tables without executing binaries or requiring objdump."""
    raw = path.read_bytes()
    def unpack(fmt, offset):
        return struct.unpack_from(fmt, raw, offset)
    if raw[:2] != b'MZ':
        raise ValidationError('Not a Windows executable: '+path.name)
    pe, = unpack('<I', 60)
    if raw[pe:pe+4] != b'PE\0\0':
        raise ValidationError('Invalid PE header: '+path.name)
    machine, sections = unpack('<HH', pe+4)
    optional_size, = unpack('<H', pe+20)
    optional = pe+24
    if machine != 0x8664 or unpack('<H', optional)[0] != 0x20b:
        raise ValidationError('Windows package requires AMD64 PE32+: '+path.name)
    table = optional+optional_size
    def offset(rva):
        for index in range(sections):
            virtual_size, virtual, size, start = unpack('<IIII', table+index*40+8)
            if virtual <= rva < virtual+max(virtual_size, size):
                result = start+rva-virtual
                if result >= len(raw): break
                return result
        raise ValidationError('Invalid PE RVA: '+path.name)
    imports, size = unpack('<II', optional+112+8)
    result = []
    if imports:
        at = offset(imports)
        for _ in range(min(size//20+1, 4096)):
            descriptor = unpack('<IIIII', at)
            if not any(descriptor): break
            start = offset(descriptor[3])
            end = raw.find(b'\0', start, start+256)
            if end < 0: raise ValidationError('Invalid DLL name')
            name = raw[start:end].decode('ascii')
            if not re.fullmatch(r'[A-Za-z0-9_.+-]+', name): raise ValidationError('Unsafe DLL import')
            result.append(name)
            at += 20
        else:
            raise ValidationError('Unterminated PE imports')
    return result


def dependency_report(runtime, files):
    records = []
    for name, path in files:
        if path.suffix.casefold() not in ('.exe', '.dll', '.pyd'): continue
        imports = pe_imports(path)
        for dll in imports:
            stem = dll.casefold().removesuffix('.dll')
            if stem in SYSTEM_DLLS or stem.startswith(('api-ms-win-', 'ext-ms-win-')): continue
            local = [p for n,p in files if p.name.casefold() == dll.casefold()
                and p.parent in (path.parent, runtime, runtime/'python')]
            if not local: raise ValidationError('Unbundled DLL '+dll+' required by '+name)
        records.append(dict(path=name, imports=imports))
    return dict(format='PE32+', architecture='AMD64', target_platform=dict(os='Windows', architecture='x64'),
        minimum_windows='Windows 10 x64; Windows 11 recommended', requires_opengl='3.3 core',
        graphics_driver='GPU vendor OpenGL driver required; system/driver DLLs are not bundled',
        inspection='PE import tables inspected without executing binaries', binaries=records,
        validation_scope='Cross-build and Wine checks; native Windows GPU/performance acceptance pending')


def validate(args):
    engine = args.engine_root.resolve(strict=True)
    runtime = (args.runtime or (engine/'bin' if (engine/'bin/rpg.exe').is_file() else engine/'build/windows/editor/bin')).resolve(strict=True)
    if not runtime.is_relative_to(engine): raise ValidationError('Runtime must be inside engine root')
    if args.executable and args.executable.resolve() != runtime/'rpg.exe': raise ValidationError('Executable must be RUNTIME/rpg.exe')
    previous = json.loads(args.previous_database.read_text()) if args.previous_database else None
    inventory = ProjectInventory(args.project, engine, previous=previous, project_id=args.project_id,
                                 active_imports_only=getattr(args,'active_imports_only',False))
    database = inventory.build()
    validator = runtime/'valwood_project_validate.exe' if os.name == 'nt' else engine/'tools/valwood_project_validate'
    with validation_project(inventory) as materialized:
        result = subprocess.run([str(validator), '--project', str(materialized)], capture_output=True, text=True, timeout=60)
    if result.returncode: raise ValidationError('Authoring validation failed: '+result.stdout[:4096]+result.stderr[:1024])
    report = json.loads(result.stdout)
    components = sum(n.endswith('.components') for n in inventory.sources)
    if report.get('ok') is not True or report.get('components_validated', 0) != components:
        raise ValidationError('Native validator did not confirm all components')
    required = {'typed-components-v1', 'scene-id-closure-v1'} if components else set()
    if database['live_runtime_dependencies']: required.add('skeletal-knight-v1')
    if not required.issubset(report.get('capabilities', [])): raise ValidationError('Rebuild native authoring validator')
    database['authoring_validation'] = report
    runtime_files = []
    for p in sorted(runtime.rglob('*')):
        if p.is_symlink(): raise ValidationError('Runtime must contain ordinary files')
        if not p.is_file(): continue
        relative = p.relative_to(runtime)
        if len(relative.parts) == 1 and p.name not in ('rpg.exe', 'valwood_project_validate.exe') and p.suffix.lower() != '.dll': continue
        if len(relative.parts) > 1 and relative.parts[0] != 'python': continue
        if p.suffix.lower() in ('.pyc', '.pyo') or '__pycache__' in p.parts: continue
        runtime_files.append(('bin/'+relative.as_posix(), p))
    for required_name in ('rpg.exe', 'valwood_project_validate.exe', 'python/python.exe'):
        if not (runtime/required_name).is_file(): raise ValidationError('Missing runtime: '+required_name)
    native = dependency_report(runtime, runtime_files)
    extras = [(n,p) for n,p in _extra_inputs(inventory) if not n.endswith('.sh')
        and n != 'tools/valwood_project_validate' and n not in ('ARTIST_START_HERE.md','TESTING_STATUS.md','HANDOFF_PLAN.md')]
    for helper in ('build_windows_package.py', 'windows_file_io.py', 'windows_artist_actions.py'):
        extras.append(('tools/'+helper, engine/'tools'/helper))
    # Windows launches the browser tool directly; it does not ship a shell launcher.
    memory = engine/'tools/npc_memory_editor'
    if (engine/'tools/npc_memory_editor.py').is_file() and memory.is_dir():
        extras.append(('tools/npc_memory_editor.py',engine/'tools/npc_memory_editor.py'))
        for name in ('dist/index.html','dist/style.css','dist/app.mjs','dist/memory.mjs',
                'compile.mjs','examples/npc_mara_fenwick.vwmemory.json'):
            extras.append(('tools/npc_memory_editor/'+name,memory/name))
    # Override Linux-specific brief instructions in the Windows handoff.
    docs = engine/'docs/editor-readiness/2026-11-02/windows'
    for name in ('ARTIST_START_HERE.md','TESTING_STATUS.md','WINDOWS_BUILD.md','FIRST_ASSIGNMENT.md',
            'EDITOR_GUIDE.md','ACCEPTANCE_CHECKLIST.csv'):
        source = docs/name
        if not source.is_file(): source = engine/'docs/windows-build/2026-11-01'/name
        if not source.is_file(): source = engine/name
        if source.is_file():
            extras = [(n,p) for n,p in extras if n != name]
            extras.append((name,source))
    for root in (engine/'platform/windows/licenses', engine/'licenses/windows'):
        if root.is_dir():
            extras += [('licenses/windows/'+p.relative_to(root).as_posix(),p) for p in root.rglob('*') if p.is_file()]
    # Deduplicate identical extras selected through reusable documentation paths.
    extras = list(dict(extras).items())
    windows_names(['project/'+r['path'] for r in database['files']] + [n for n,p in runtime_files+extras])
    return inventory, database, runtime_files, extras, native


def batch(action):
    return ('@echo off\r\nsetlocal\r\nset "PYTHONDONTWRITEBYTECODE=1"\r\n'
        '"%~dp0bin\\python\\python.exe" -B "%~dp0tools\\windows_artist_actions.py" '+action+' %*\r\n'
        'set "result=%errorlevel%"\r\nif not "%result%"=="0" echo Operation failed. Read the error above.\r\n'
        'pause\r\nexit /b %result%\r\n').encode()


def build(validated, args):
    inventory, database, runtime_files, extras, native = validated
    output = args.output.absolute()
    output = output.parent.resolve(strict=True)/output.name
    if output.exists() or output.is_symlink() or output.is_relative_to(inventory.project) or output == inventory.engine:
        raise ValidationError('Output must be a new directory outside source project')
    stage = Path(tempfile.mkdtemp(prefix='.valwood-windows-', dir=output.parent))
    try:
        entries = []
        for record in database['files']:
            source = inventory.sources[record['path']]
            if source.lexical.resolve(strict=True) != source.resolved: raise ValidationError('Source link changed')
            entries.append(_copy_checked(stage, 'project/'+record['path'],source.resolved,source.source_hash,
                payload=source.content,expected_identity=source.identity))
        for name, source in runtime_files+extras:
            payload = None
            if source.suffix.casefold() in ('.md','.txt'):
                content = source.read_text(encoding='utf-8')
                content = content.replace(str(inventory.engine), 'engine-source')
                content = re.sub(r'(?:/home/|/Users/|/tmp/)[^\s"<>]+', 'source-file',content)
                payload = content.encode('utf-8')
            entries.append(_copy_checked(stage,name,source,_native_hash(source),payload=payload))
        for action, name in (('launch','Launch_Editor.bat'), ('create','Create_Artist_Workspace.bat'),
                ('export','Return_Changes.bat'), ('verify','Verify_Seed.bat'), ('memory','Launch_Memory_Editor.bat')):
            entries.append(_write(stage,name,batch(action)))
        # Production jobs locate scripts beside the engine executable.
        wrapper = b'import runpy,sys\nfrom pathlib import Path\nsys.dont_write_bytecode=True\nroot=Path(__file__).resolve().parents[2]\nsys.path.insert(0,str(root/"tools"))\nargs=sys.argv[1:]\nif "--engine-root" in args:\n i=args.index("--engine-root")\n if i+1<len(args) and Path(args[i+1]).resolve()==root/"bin":args[i+1]=str(root)\nsys.argv=[str(root/"tools/build_windows_package.py")]+args\nrunpy.run_path(sys.argv[0],run_name="__main__")\n'
        entries.append(_write(stage,'bin/tools/build_world_package.py',wrapper))
        entries.append(_write(stage,'README.txt',b'Valwood custom engine - Windows x64 artist edition\r\n\r\nRead ARTIST_START_HERE.md. Extract the entire ZIP to a local writable folder.\r\nDouble-click Create_Artist_Workspace.bat; edit the new working copy.\r\nLaunch_Editor.bat starts Studio. Python and runtime DLLs are bundled.\r\nA GPU vendor OpenGL 3.3 driver is required. No admin install is needed.\r\nNative Windows GPU, input and 45-60 FPS acceptance must be measured on the artist PC.\r\nOriginal archives, private saves and unsaved recovery drafts are excluded.\r\nAsset licenses remain with the owner and permitted collaborators.\r\n'))
        entries.append(_write(stage,'asset-database.json',canonical_json(database)))
        entries.append(_write(stage,'native-dependencies.json',canonical_json(native)))
        provenance = dict(schema_version=VERSION,project_id=database['project_id'],records=database['provenance'],
            credit_files=sorted(e['path'] for e in entries if 'CREDITS' in e['path'] or e['path'].startswith('licenses/')))
        entries.append(_write(stage,'asset-provenance.json',canonical_json(provenance)))
        entries.append(_write(stage,'build-recipe.json',canonical_json(dict(builder='build_windows_package.py',
            target='windows-x64',mode=args.mode,project_id=database['project_id'],
            builder_sha256=_native_hash(Path(__file__)),executable_sha256=_native_hash(dict(runtime_files)['bin/rpg.exe'])))))
        entries.sort(key=lambda e:e['path'])
        windows_names([e['path'] for e in entries])
        manifest = dict(schema='valwood.windows-package',schema_version=VERSION,project_id=database['project_id'],
            scope=database['scope'],content_id=sha256_bytes(canonical_json(entries)),files=entries,
            total_bytes=sum(e['bytes'] for e in entries))
        _write(stage,'package-manifest.json',canonical_json(manifest))
        verify_package(stage)
        _publish_directory(stage,output)
        return dict(ok=True,output=str(output),content_id=manifest['content_id'],files=len(entries),bytes=manifest['total_bytes'],warnings=database['warnings'])
    finally:
        if stage.exists(): shutil.rmtree(stage)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    for flag in ('project','engine-root','output','runtime','executable','database-output','previous-database','verify'):
        p.add_argument('--'+flag,type=Path)
    p.add_argument('--project-id')
    p.add_argument('--mode',choices=('development','production'),default='development')
    p.add_argument('--validate-only',action='store_true')
    p.add_argument('--active-imports-only',action='store_true',help='Exclude only dependency-unreferenced managed import revisions')
    p.add_argument('--bundle-libs',action='store_true',help='Windows runtime DLL closure is always included')
    args = p.parse_args(argv)
    try:
        if args.verify: result = verify_package(args.verify)
        else:
            if not args.project or not args.engine_root: raise ValidationError('--project and --engine-root are required')
            if not args.validate_only and not args.output: raise ValidationError('--output is required')
            if args.database_output and (args.database_output.exists() or args.database_output.is_symlink()
                    or args.database_output.absolute().is_relative_to(args.project.resolve())):
                raise ValidationError('Database sidecar must be new and outside source project')
            validated = validate(args)
            result = dict(ok=True,validate_only=True,summary=validated[1]['summary'],native_binaries=len(validated[4]['binaries']),warnings=validated[1]['warnings']) if args.validate_only else build(validated,args)
            if args.database_output and not args.validate_only:
                write_new_json(args.database_output,validated[1],forbidden_roots=[args.project.resolve()])
        print(json.dumps(result,sort_keys=True));return 0
    except (OSError,ValueError,KeyError,TypeError,struct.error,subprocess.SubprocessError) as error:
        print(json.dumps(dict(ok=False,error=str(error)),sort_keys=True));return 1


if __name__ == '__main__': raise SystemExit(main())
