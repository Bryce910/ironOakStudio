#!/usr/bin/env python3
"""Build a relocatable Linux Valwood runtime package from validated project data.

Validation and packaging never edit the source project. Output is a NEW directory
published atomically after every copied input has been checked. No archive,
Blender/FBX source, save, capture, credential or private Downloads file is gathered.
The current engine needs a conservative full runtime project, not a minimal scene
closure. Linux system graphics drivers and glibc remain host prerequisites.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import tempfile
import sys

sys.dont_write_bytecode = True

from world_asset_database import (ProjectInventory, ValidationError, VERSION,
    canonical_json, open_regular_nofollow, relative_name, sha256_bytes, write_new_json)

LIBRARY_PREFIXES = ("libSDL2", "libstdc++.so", "libgcc_s.so", "libfreetype.so",
    "libharfbuzz.so", "libpng16.so", "libjpeg.so", "libwebp.so", "libz.so",
    "libbz2.so", "liblzma.so", "libFLAC.so", "libogg.so", "libvorbis",
    "libmpg123.so", "libopus.so", "libmodplug.so", "libxmp.so")
SYSTEM_LIBRARY_ROOTS = (Path("/usr/lib"), Path("/lib"), Path("/lib64"), Path("/usr/lib64"))
EPOCH = 0


def _tool_output(command):
    try:
        process = subprocess.run(command, capture_output=True, text=True, timeout=20,
                                 env={"PATH": "/usr/sbin:/usr/bin:/sbin:/bin", "LC_ALL": "C"})
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ValidationError("Cannot inspect native dependencies: " + command[0]) from error
    if process.returncode:
        raise ValidationError("Native inspection failed: " + command[0])
    return process.stdout


def _native_hash(path):
    with open_regular_nofollow(path) as stream:
        digest = hashlib.sha256()
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def native_dependencies(executable):
    """Read ELF metadata; never execute ldd/an unknown executable to inspect it."""
    executable = Path(executable).resolve(strict=True)
    header = _tool_output(["readelf", "-h", str(executable)])
    if "ELF" not in header:
        raise ValidationError("The packaged executable must be a Linux ELF binary")
    architecture = next((line.partition(":")[2].strip() for line in header.splitlines()
                         if line.strip().startswith("Machine:")), "unknown")
    program = _tool_output(["readelf", "-l", str(executable)])
    interpreter = re.search(r"Requesting program interpreter:\s*([^\]]+)", program)
    cache = _tool_output(["ldconfig", "-p"])
    candidates = {}
    for line in cache.splitlines():
        match = re.match(r"\s*(\S+)\s+\(([^)]+)\)\s+=>\s+(\S+)", line)
        if match:
            candidates.setdefault(match[1], []).append((match[2], Path(match[3])))
    resolved_sources = {}
    inspected = set()
    records = {}
    needed_by = {}
    queue = [("rpg", executable)]
    while queue:
        owner, path = queue.pop(0)
        if path in inspected:
            continue
        inspected.add(path)
        dynamic = _tool_output(["readelf", "-d", str(path)])
        search_directories = []
        for search in re.findall(r"\((?:RPATH|RUNPATH)\).*?\[([^\]]*)\]", dynamic):
            for item in search.split(":"):
                origin_relative = item == "$ORIGIN" or (item.startswith("$ORIGIN/") and ".." not in item.split("/"))
                host_system = item.startswith("/") and any(Path(item).is_relative_to(root) for root in SYSTEM_LIBRARY_ROOTS)
                if item and not origin_relative and not host_system:
                    raise ValidationError("Native binary/library has a non-relocatable ELF search path: " + owner)
                if item:
                    search_directories.append(Path(item.replace("$ORIGIN", str(path.parent))))
        for soname in re.findall(r"\(NEEDED\).*?\[([^\]]+)\]", dynamic):
            if "/" in soname or "\\" in soname or not re.fullmatch(r"[A-Za-z0-9_.+-]+", soname):
                raise ValidationError("Unsafe ELF dependency name")
            needed_by.setdefault(soname, set()).add(owner)
            if soname in records:
                continue
            choices = [("", directory / soname) for directory in search_directories
                       if (directory / soname).is_file()] + candidates.get(soname, [])
            # The project currently targets the host architecture. Prefer the
            # host ABI rather than accepting a differently sized cached library.
            abi = "x86-64" if "X86-64" in architecture else "AArch64" if "AArch64" in architecture else None
            choices = sorted(choices, key=lambda item: (not (abi and abi.casefold() in item[0].casefold()), str(item[1])))
            selected = None
            for _, candidate in choices:
                real = candidate.resolve(strict=True)
                if real.is_file() and any(real.is_relative_to(root.resolve()) for root in SYSTEM_LIBRARY_ROOTS):
                    candidate_header = _tool_output(["readelf", "-h", str(real)])
                    machine = next((line.partition(":")[2].strip() for line in candidate_header.splitlines()
                                    if line.strip().startswith("Machine:")), "unknown")
                    if machine == architecture:
                        selected = real
                        break
            if selected is None:
                raise ValidationError("Unresolved native dependency: " + soname)
            resolved_sources[soname] = selected
            records[soname] = {"soname": soname, "sha256": _native_hash(selected),
                "bytes": selected.stat().st_size, "bundled": False,
                "policy": "eligible-explicit-runtime-library" if soname.startswith(LIBRARY_PREFIXES)
                    else "host-system-library-or-driver"}
            queue.append((soname, selected))
    for soname, record in records.items():
        record["needed_by"] = sorted(needed_by[soname])
    return {"format": "ELF", "architecture": architecture,
        "host_system": "Linux", "interpreter": Path(interpreter[1]).name if interpreter else None,
        "inspection": "readelf DT_NEEDED and system ldconfig cache; no executable/ldd execution",
        "bundling_policy": "Only explicitly eligible application libraries. glibc, ELF loader, graphics drivers, GL, X11, Wayland and host service libraries remain host prerequisites.",
        "libraries": sorted(records.values(), key=lambda item: item["soname"])}, resolved_sources


def _write(stage, path, content, executable=False):
    path = relative_name(path)
    destination = stage / path
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("xb") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())
    os.chmod(destination, 0o755 if executable else 0o644)
    os.utime(destination, (EPOCH, EPOCH))
    return {"path": path, "bytes": len(content), "sha256": sha256_bytes(content),
            "mode": "0755" if executable else "0644"}


def _copy_checked(stage, name, source, expected_hash, *, executable=False,
                  payload=None, expected_identity=None):
    name = relative_name(name)
    destination = stage / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    total = 0
    with open_regular_nofollow(source) as stream, destination.open("xb") as output:
        before = os.fstat(stream.fileno())
        if expected_identity and (before.st_dev, before.st_ino) != expected_identity:
            raise ValidationError("Source replaced before copy: " + name)
        while block := stream.read(1024 * 1024):
            digest.update(block)
            if payload is None:
                output.write(block)
                total += len(block)
        after = os.fstat(stream.fileno())
        if digest.hexdigest() != expected_hash or (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (
                after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            raise ValidationError("Source changed during packaging: " + name)
        if payload is not None:
            output.write(payload)
            total = len(payload)
        output.flush()
        os.fsync(output.fileno())
    os.chmod(destination, 0o755 if executable else 0o644)
    os.utime(destination, (EPOCH, EPOCH))
    return {"path": name, "bytes": total,
        "sha256": sha256_bytes(payload) if payload is not None else expected_hash,
        "mode": "0755" if executable else "0644"}


def _publish_directory(stage, destination):
    """Linux no-replace rename: atomic even if someone creates output meanwhile."""
    if os.name == 'nt':
        from windows_file_io import publish_directory
        return publish_directory(stage, destination)
    libc = ctypes.CDLL(None, use_errno=True)
    rename = getattr(libc, "renameat2", None)
    if rename is None:
        raise ValidationError("Safe atomic publication needs Linux renameat2")
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    if rename(-100, os.fsencode(stage), -100, os.fsencode(destination), 1):
        code = ctypes.get_errno()
        if code in (errno.EEXIST, errno.ENOTEMPTY):
            raise ValidationError("Output already exists; nothing was overwritten")
        raise OSError(code, os.strerror(code))
    directory = os.open(destination.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


def _extra_inputs(inventory):
    """Only small explicitly located runtime helpers/credits, never source trees."""
    extra = []
    for name in ("snd_attack.wav", "snd_hit.wav", "snd_shield.wav", "snd_dash.wav"):
        source = inventory.engine / "sound_effects" / name
        if source.exists() or source.is_symlink():
            real = inventory.safe_resolve(source)
            extra.append(("bin/sound_effects/" + name, real))
    importer = inventory.engine / "tools/import_world_asset.py"
    if importer.exists():
        real = importer.resolve(strict=True)
        if not real.is_relative_to(inventory.engine / "tools") or not real.is_file():
            raise ValidationError("Static import helper escapes its tools directory")
        extra.append(("bin/tools/import_world_asset.py", real))
    # Relocated Studio finds helpers relative to bin/rpg. The production
    # wrapper redirects its engine root to the package root, keeping the
    # materialized project inside the inventory's explicit trust boundary.
    for helper in ("build_world_package.py", "world_asset_database.py", "world_package_cache_selection.py", "check_world_performance.py", "valwood_project_validate", "import_world_asset.py", "world_artist_handoff.py"):
        source = inventory.engine / "tools" / helper
        if source.is_file() and not source.is_symlink():
            extra.append(("tools/" + helper, source))
            if helper == "check_world_performance.py":
                extra.append(("bin/tools/" + helper, source))
    for metadata in ("engine/world_scenery.cpp", "engine/world_runtime_visuals.inc"):
        source = inventory.engine / metadata
        if source.is_file() and not source.is_symlink():
            extra.append((metadata, source))
    licenses = inventory.engine / "docs/licenses"
    if not licenses.is_dir():
        licenses = inventory.engine / "licenses/engine"
    if licenses.is_dir() and not licenses.is_symlink():
        for source in sorted(licenses.rglob("*")):
            if source.is_symlink():
                raise ValidationError("License provenance must not be a symlink")
            if source.is_file() and source.suffix.casefold() in (".md", ".txt"):
                extra.append(("licenses/engine/" + source.relative_to(licenses).as_posix(), source))
    for relative in ("third_party/imgui/LICENSE.txt", "third_party/jolt/LICENSE", "LICENSE", "LICENSE.txt"):
        source = inventory.engine / relative
        if source.is_file() and not source.is_symlink():
            extra.append(("licenses/engine/" + relative.replace("/", "_"), source))
    demo_guide = inventory.engine / "docs/demo-completion/2026-10-29/DEMO_GUIDE.md"
    if not demo_guide.is_file():
        demo_guide = inventory.engine / "DEMO_GUIDE.md"
    if demo_guide.is_file() and not demo_guide.is_symlink():
        extra.append(("DEMO_GUIDE.md", demo_guide))
    preview_guide=inventory.engine/"docs/vertical-slice/2026-10-30/PRESENTATION_GUIDE.md"
    polish_docs=inventory.engine/"docs/slice-polish/2026-10-30"
    if (polish_docs/"PRESENTATION_GUIDE.md").is_file():preview_guide=polish_docs/"PRESENTATION_GUIDE.md"
    visual_guide=inventory.engine/"docs/visual-upgrade/2026-10-30/PRESENTATION_GUIDE.md"
    if (inventory.project/"visual_upgrade.json").is_file() and visual_guide.is_file():preview_guide=visual_guide
    if (inventory.project/"scenery/presentation_instances.txt").is_file() and preview_guide.is_file():
        extra.append(("PRESENTATION_GUIDE.md",preview_guide))
        for name in ("CHARACTER_ASSETS.md","voice-recording-script.csv"):
            if (polish_docs/name).is_file():extra.append((name,polish_docs/name))
    editor_guide = inventory.engine / "docs/editor-completion/2026-10-30/EDITOR_GUIDE.md"
    if not editor_guide.is_file():
        editor_guide = inventory.engine / "EDITOR_GUIDE.md"
    if editor_guide.is_file() and not editor_guide.is_symlink():
        extra.append(("EDITOR_GUIDE.md", editor_guide))
    # Artist delivery is explicit documentation/tooling, never an unrestricted
    # copy of the documentation tree or the owner's working/recovery files.
    for name in ("ARTIST_START_HERE.md", "FIRST_ASSIGNMENT.md", "ACCEPTANCE_CHECKLIST.csv", "HANDOFF_PLAN.md", "TESTING_STATUS.md", "EDITOR_LAYOUT.png"):
        source = inventory.engine / "docs/artist-handoff/2026-10-31" / name
        if not source.is_file():
            source = inventory.engine / name
        if source.is_file() and not source.is_symlink():
            extra.append((name, source))
    surface_guide = inventory.engine / "docs/studio/surface-finish-2026-10-29/USER-GUIDE.md"
    if not surface_guide.is_file():
        surface_guide = inventory.engine / "SURFACE_GUIDE.md"
    if surface_guide.is_file() and not surface_guide.is_symlink():
        extra.append(("SURFACE_GUIDE.md", surface_guide))
    memory_server = inventory.engine / "tools/npc_memory_editor.py"
    memory_files = ["dist/index.html", "dist/style.css", "dist/app.mjs", "dist/memory.mjs", "compile.mjs", "examples/npc_mara_fenwick.vwmemory.json"]
    memory_root = inventory.engine / "tools/npc_memory_editor"
    memory_launcher = inventory.engine / "tools/launch_packaged_memory_studio.sh"
    if not memory_launcher.is_file():
        memory_launcher = inventory.engine / "memory-studio.sh"
    if memory_server.is_file() and memory_launcher.is_file() and all((memory_root / name).is_file() for name in memory_files):
        for destination, source in [("tools/npc_memory_editor.py", memory_server), ("memory-studio.sh", memory_launcher)] + [("tools/npc_memory_editor/" + name, memory_root / name) for name in memory_files]:
            if source.is_symlink():
                raise ValidationError("Memory Studio bundle must contain ordinary files")
            extra.append((destination, source))
    return extra


@contextmanager
def validation_project(inventory):
    """Read-only native validation of the same ordinary files a package ships.

    Trusted runtime links have already been bounded by ProjectInventory. Never
    relax the native validator's ordinary-directory policy: materialize a
    disposable view instead. Hardlinks receive no chmod/utime; native validation
    is read-only. Cross-filesystem copies are checked and duplicate bytes reuse
    an already materialized file. Source identity/content are checked again.
    """
    with tempfile.TemporaryDirectory(prefix='valwood-package-validation-') as directory:
        project = Path(directory)/'project'; project.mkdir()
        verified = {}; payloads = {}; observed = {}
        for name, source in sorted(inventory.sources.items()):
            if source.lexical.resolve(strict=True) != source.resolved:
                raise ValidationError('Source link changed during validation: '+name)
            with open_regular_nofollow(source.resolved) as stream:
                info = os.fstat(stream.fileno())
                identity = (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns)
                if identity[:2] != source.identity or info.st_size != source.size:
                    raise ValidationError('Source identity changed during validation: '+name)
                if verified.get(identity) != source.source_hash:
                    if hashlib.file_digest(stream,'sha256').hexdigest() != source.source_hash:
                        raise ValidationError('Source bytes changed during validation: '+name)
                    verified[identity] = source.source_hash
            target = project/name; target.parent.mkdir(parents=True,exist_ok=True)
            payload_key = (source.source_hash,sha256_bytes(source.content) if source.content is not None else None)
            if source.content is not None:
                target.write_bytes(source.content)
            else:
                link_source = payloads.get(payload_key,source.resolved)
                linked = False
                try:
                    os.link(link_source,target,follow_symlinks=False)
                    linked = True
                except OSError as error:
                    if error.errno not in (errno.EXDEV,errno.EPERM,errno.EACCES,errno.ENOSYS,getattr(errno,'ENOTSUP',errno.EPERM)):
                        raise
                    with open_regular_nofollow(source.resolved) as incoming, target.open('wb') as outgoing:
                        shutil.copyfileobj(incoming,outgoing,1024*1024)
                    if _native_hash(target) != source.source_hash:
                        raise ValidationError('Source changed during validation copy: '+name)
                if target.is_symlink() or not target.is_file():
                    raise ValidationError('Validation view contains a link/non-file: '+name)
                if linked and link_source == source.resolved:
                    actual = target.stat()
                    if (actual.st_dev,actual.st_ino) != source.identity:
                        raise ValidationError('Source replaced during validation linking: '+name)
                payloads.setdefault(payload_key,target)
            observed[source.resolved] = identity
        yield project
        for path, identity in observed.items():
            info = path.stat()
            if (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns) != identity:
                raise ValidationError('Source changed during native validation: '+path.name)


def validate_inputs(project, engine_root, executable=None, previous=None, project_id=None, active_imports_only=False):
    inventory = ProjectInventory(project, engine_root, previous=previous, project_id=project_id, active_imports_only=active_imports_only)
    database = inventory.build()
    components = sum(name.endswith(".components") for name in inventory.sources)
    validator = inventory.engine / "tools/valwood_project_validate"
    if validator.exists():
        if validator.is_symlink() or not validator.is_file() or not os.access(validator, os.X_OK):
            raise ValidationError("Authoring schema validator must be the built local engine tool")
        try:
            with validation_project(inventory) as materialized:
                validation = subprocess.run([str(validator), "--project", str(materialized)],
                    capture_output=True, text=True, timeout=30)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise ValidationError("Authoring schema validator failed to run") from error
        if validation.returncode:
            raise ValidationError("Authoring schema validation failed: " + validation.stdout[:4096])
        try:
            report = json.loads(validation.stdout)
        except ValueError as error:
            raise ValidationError("Malformed authoring validation result") from error
        if report.get("ok") is not True:
            raise ValidationError("Authoring schema validator did not confirm success")
        capabilities = report.get("capabilities", [])
        if not isinstance(capabilities, list) or not all(isinstance(value, str) for value in capabilities):
            raise ValidationError("Malformed native authoring validation capabilities")
        if components and not {"typed-components-v1", "scene-id-closure-v1"}.issubset(capabilities):
            raise ValidationError("Rebuild the native authoring validator: typed component/scene ID validation is unavailable")
        if components and report.get("components_validated") != components:
            raise ValidationError("Native authoring validator did not verify every packaged components document")
        if database["live_runtime_dependencies"] and "skeletal-knight-v1" not in capabilities:
            raise ValidationError("Rebuild the native authoring validator: shipped skeletal rig validation is unavailable")
        database["authoring_validation"] = report
    else:
        if components or database["live_runtime_dependencies"]:
            raise ValidationError("Build tools/valwood_project_validate before packaging live components or skeletal actors")
        database["warnings"].append("Native authoring schema validator unavailable; dependency checks do not certify document semantics")
    executable = Path(executable) if executable else inventory.engine / "rpg"
    lexical = executable.absolute()
    executable = executable.resolve(strict=True)
    if not executable.is_relative_to(inventory.engine) or not executable.is_file():
        raise ValidationError("Executable must be a regular file inside the engine root")
    if not os.access(executable, os.X_OK):
        raise ValidationError("Engine binary is not executable")
    with open_regular_nofollow(executable) as stream:
        info = os.fstat(stream.fileno())
        executable_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    native, libraries = native_dependencies(executable)
    extras = [(name, source, _native_hash(source)) for name, source in _extra_inputs(inventory)]
    return inventory, database, lexical, executable, executable_hash, (info.st_dev, info.st_ino), native, libraries, extras


def build_package(validated, output, *, bundle_libraries=False, mode="production"):
    inventory, database, lexical_executable, executable, executable_hash, executable_identity, native, libraries, extras = validated
    output = Path(output).absolute()
    output = output.parent.resolve(strict=True) / output.name
    if output.is_relative_to(inventory.project) or output == inventory.engine:
        raise ValidationError("Output must be outside the source project")
    if output.exists() or output.is_symlink():
        raise ValidationError("Output already exists; use a new package directory")
    stage = Path(tempfile.mkdtemp(prefix=".valwood-package-", dir=output.parent))
    published = False
    try:
        entries = []
        for record in database["files"]:
            source = inventory.sources[record["path"]]
            if source.lexical.resolve(strict=True) != source.resolved:
                raise ValidationError("Source link changed after validation: " + record["path"])
            entries.append(_copy_checked(stage, "project/" + record["path"], source.resolved,
                source.source_hash, payload=source.content, expected_identity=source.identity))
        if lexical_executable.resolve(strict=True) != executable:
            raise ValidationError("Executable link changed after validation")
        entries.append(_copy_checked(stage, "bin/rpg", executable, executable_hash,
                                     executable=True, expected_identity=executable_identity))
        for name, source, digest in extras:
            # Text provenance may describe historical source paths. Redact those
            # descriptions without gathering any referenced purchased archives.
            payload = None
            if source.suffix.casefold() in (".md", ".txt"):
                text = source.read_text(encoding="utf-8")
                text = text.replace(str(inventory.engine), "engine-source")
                text = re.sub(r"(?:/home/|/Users/|/tmp/)[^\s\"<>]+", "source-file", text)
                payload = text.encode("utf-8")
            entries.append(_copy_checked(stage, name, source, digest, payload=payload, executable=name in ("memory-studio.sh", "tools/valwood_project_validate")))
        for record in native["libraries"]:
            if bundle_libraries and record["policy"] == "eligible-explicit-runtime-library":
                entries.append(_copy_checked(stage, "lib/" + record["soname"], libraries[record["soname"]], record["sha256"]))
                record["bundled"] = True
        default = "--editor" if mode == "development" else "--play"
        launcher = '''#!/bin/sh
set -eu
package_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -d "$package_root/lib" ]; then
    LD_LIBRARY_PATH="$package_root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
fi
cd -- "$package_root"
if [ -f "$package_root/project/scenery/presentation_instances.txt" ]; then
    VALWOOD_PRESENTATION=1
    export VALWOOD_PRESENTATION
    set -- --scenic castle-finished-arcade --no-hud --width 1600 --height 900 "$@"
fi
exec "$package_root/bin/rpg" --project "$package_root/project" DEFAULT "$@"
'''.replace("DEFAULT", default).encode()
        entries.append(_write(stage, "launch.sh", launcher, executable=True))
        if any(name == "tools/world_artist_handoff.py" for name, _, _ in extras):
            for command, name, argument in (("create", "artist-workspace.sh", "--package"),
                                             ("export", "artist-return.sh", "--workspace")):
                wrapper = ('#!/bin/sh\nset -eu\nexport PYTHONDONTWRITEBYTECODE=1\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                    'exec python3 "$root/tools/world_artist_handoff.py" ' + command + ' ' + argument + ' "$root" "$@"\n').encode()
                entries.append(_write(stage, name, wrapper, executable=True))
        if (inventory.engine / "tools/build_world_package.py").is_file():
            wrapper = b'#!/usr/bin/env python3\nimport os,sys\nfrom pathlib import Path\nbase=Path(__file__).resolve().parents[1]\nargs=sys.argv[1:]\nlib=base.parent/"lib"\nif lib.is_dir():os.environ["LD_LIBRARY_PATH"]=str(lib)+os.pathsep+os.environ.get("LD_LIBRARY_PATH","")\nif "--engine-root" in args:\n i=args.index("--engine-root")\n if i+1<len(args) and Path(args[i+1]).resolve()==base:\n  args[i+1]=str(base.parent)\n  if "--executable" not in args:args.extend(["--executable",str(base/"rpg")])\nos.execv(sys.executable,[sys.executable,str(base.parent/"tools/build_world_package.py")]+args)\n'
            entries.append(_write(stage, "bin/tools/build_world_package.py", wrapper, executable=True))
        notes = ("Valwood Linux runtime package\n\n"
            "Run ./launch.sh from any directory. The entire package can be moved.\n"
            "Run ./check-system.sh first (Python 3) to inspect OS, CPU, glibc and required libraries.\n"
            "This build contains the full selected project's runtime folders, not a minimal scene closure.\n"
            "Original source archives, personal saves, captures, credentials and Downloads were not copied.\n"
            "JSON source-provenance paths are descriptive and sanitized; runtime dependencies are local.\n"
            "Requires a compatible Linux ELF ABI, glibc, SDL/OpenGL dependencies listed in native-dependencies.json,\n"
            "and a functioning OpenGL 3.3 graphics driver. Optional bundled application libraries do not bundle\n"
            "glibc, the ELF loader, the display server, audio services or graphics drivers. This is not an\n"
            "AppImage/container and does not establish compatibility with every Linux distribution.\n"
            "The existing binary includes Studio; production mode starts play and does not strip editor code.\n"
            "Source asset credits/provenance are included; this package does not change asset license rights.\n"
            "Project graphics settings are copied unchanged. Runtime settings/saves affect this package copy.\n"
            "The library inventory covers DT_NEEDED dependencies; driver/plugin libraries loaded dynamically\n"
            "remain host responsibilities. Run the real game smoke test on each intended target machine.\n").encode()
        entries.append(_write(stage, "README.txt", notes))
        entries.append(_write(stage, "asset-database.json", canonical_json(database)))
        requirement_paths = [executable] + [libraries[item["soname"]] for item in native["libraries"] if item["bundled"]]
        for name, source, _ in extras:
            if name == "tools/valwood_project_validate" and source.read_bytes()[:4] == b"\x7fELF":
                requirement_paths.append(source)
        glibc = set()
        for requirement_path in requirement_paths:
            versions = _tool_output(["readelf", "--version-info", str(requirement_path)])
            needs = versions.partition("Version needs section")[2]
            glibc.update(re.findall(r"Name: GLIBC_([0-9]+(?:\.[0-9]+)+)", needs))
        native["minimum_glibc"] = max(glibc, key=lambda value: tuple(map(int, value.split(".")))) if glibc else None
        native["requires_opengl"] = "3.3"
        native["target_platform"] = {"os": "Linux", "elf_architecture": native["architecture"]}
        native["validation_scope"] = "Dependency metadata only; run the relocated game on every intended target machine."
        preflight = inventory.engine / "tools/check_valwood_system.py"
        if preflight.is_file():
            entries.append(_write(stage, "tools/check_valwood_system.py", preflight.read_bytes()))
            entries.append(_write(stage, "check-system.sh", b'#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec python3 "$root/tools/check_valwood_system.py" --package "$root" "$@"\n', executable=True))
        entries.append(_write(stage, "native-dependencies.json", canonical_json(native)))
        provenance = {"schema_version": VERSION, "project_id": database["project_id"],
            "records": database["provenance"], "credit_files": sorted(item["path"] for item in entries
                if "CREDITS" in item["path"] or item["path"].startswith("licenses/"))}
        entries.append(_write(stage, "asset-provenance.json", canonical_json(provenance)))
        recipe = {"schema_version": VERSION, "builder": "build_world_package.py", "builder_version": VERSION,
            "mode": mode, "bundle_libraries": bundle_libraries, "source_date_epoch": EPOCH,
            "project_id": database["project_id"], "project_database_sha256": sha256_bytes(canonical_json(database)),
            "executable_sha256": executable_hash, "host_abi": native["architecture"],
            "tools": {name: _native_hash(Path(__file__).parent / name)
                      for name in ("build_world_package.py", "world_asset_database.py")}}
        entries.append(_write(stage, "build-recipe.json", canonical_json(recipe)))
        entries.sort(key=lambda item: item["path"])
        manifest = {"schema": "valwood.linux-package", "schema_version": VERSION,
            "project_id": database["project_id"], "scope": database["scope"],
            "content_id": sha256_bytes(canonical_json(entries)), "files": entries,
            "total_bytes": sum(item["bytes"] for item in entries)}
        _write(stage, "package-manifest.json", canonical_json(manifest))
        verify_package(stage)
        for directory in sorted((path for path in stage.rglob("*") if path.is_dir()), reverse=True):
            os.chmod(directory, 0o755)
            os.utime(directory, (EPOCH, EPOCH))
        os.chmod(stage, 0o755)
        os.utime(stage, (EPOCH, EPOCH))
        _publish_directory(stage, output)
        published = True
        return {"ok": True, "validate_only": False, "output": str(output),
            "content_id": manifest["content_id"], "files": len(entries),
            "bytes": manifest["total_bytes"], "scope": database["scope"],
            "warnings": database["warnings"]}
    finally:
        if not published:
            shutil.rmtree(stage)


def verify_package(package):
    package = Path(package).resolve(strict=True)
    manifest_file = package / "package-manifest.json"
    if manifest_file.is_symlink():
        raise ValidationError("Package manifest must not be a symlink")
    manifest = json.loads(manifest_file.read_text())
    if manifest.get("schema") not in ("valwood.linux-package", "valwood.windows-package") or manifest.get("schema_version") != VERSION:
        raise ValidationError("Unsupported package manifest")
    entries = manifest.get("files", [])
    if manifest.get("content_id") != sha256_bytes(canonical_json(entries)):
        raise ValidationError("Package content ID mismatch")
    seen = set()
    actual = set()
    for path in package.rglob("*"):
        if path.is_symlink():
            raise ValidationError("Package path is an external/symlink reference: " + path.relative_to(package).as_posix())
        if path.is_file():
            actual.add(path.relative_to(package).as_posix())
    for entry in entries:
        name = relative_name(entry["path"])
        if name in seen:
            raise ValidationError("Duplicate package path")
        seen.add(name)
        path = package / name
        if path.is_symlink() or not path.resolve(strict=True).is_relative_to(package):
            raise ValidationError("Package path is an external/symlink reference: " + name)
        with open_regular_nofollow(path) as stream:
            info = os.fstat(stream.fileno())
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if info.st_size != entry["bytes"] or digest != entry["sha256"]:
            raise ValidationError("Package file hash/length mismatch: " + name)
        if manifest['schema'] == 'valwood.linux-package' and os.name != 'nt' and stat.S_IMODE(info.st_mode) != int(entry["mode"], 8):
            raise ValidationError("Package file mode mismatch: " + name)
    if actual != seen | {"package-manifest.json"}:
        raise ValidationError("Unexpected or missing package files")
    return {"ok": True, "verified": True, "content_id": manifest["content_id"], "files": len(seen)}


def main(argv=None):
    if os.name == 'nt':
        from build_windows_package import main as windows_main
        return windows_main(argv)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path)
    parser.add_argument("--engine-root", type=Path)
    parser.add_argument("--output", type=Path, help="New output directory; must not exist")
    parser.add_argument("--executable", type=Path, help="Defaults to ENGINE_ROOT/rpg")
    parser.add_argument("--validate-only", action="store_true", help="Read/validate/inventory; write nothing")
    parser.add_argument("--active-imports-only", action="store_true", help="Exclude only dependency-unreferenced managed import revisions")
    parser.add_argument("--database-output", type=Path, help="Optional new JSON outside source project (build only)")
    parser.add_argument("--previous-database", type=Path)
    parser.add_argument("--project-id")
    parser.add_argument("--bundle-libs", action="store_true")
    parser.add_argument("--mode", choices=("production", "development"), default="production")
    parser.add_argument("--verify", type=Path, help="Verify an existing package without launching it")
    args = parser.parse_args(argv)
    try:
        if args.verify:
            if args.project or args.engine_root or args.output or args.validate_only or args.database_output:
                raise ValidationError("--verify is separate from build/validation")
            result = verify_package(args.verify)
        else:
            if not args.project or not args.engine_root:
                raise ValidationError("--project and --engine-root are required")
            if not args.validate_only and not args.output:
                raise ValidationError("--output is required for a build")
            previous = json.loads(args.previous_database.read_text()) if args.previous_database else None
            validated = validate_inputs(args.project, args.engine_root, args.executable, previous, args.project_id,args.active_imports_only)
            inventory, database = validated[:2]
            if args.validate_only:
                result = {"ok": True, "validate_only": True, "scope": database["scope"],
                    "project_id": database["project_id"], "summary": database["summary"],
                    "native_libraries": len(validated[6]["libraries"]), "warnings": database["warnings"]}
            else:
                # Avoid publishing the main package and only then discovering a
                # requested sidecar would overwrite an original.
                if args.database_output and (args.database_output.exists() or args.database_output.is_symlink()
                        or args.database_output.absolute().is_relative_to(inventory.project)):
                    raise ValidationError("Database sidecar must be new and outside the source project")
                result = build_package(validated, args.output, bundle_libraries=args.bundle_libs, mode=args.mode)
                if args.database_output:
                    write_new_json(args.database_output, database, forbidden_roots=[inventory.project])
        print(json.dumps(result, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        print(json.dumps({"ok": False, "error": str(error)}, sort_keys=True))
        return 1
if __name__ == "__main__":
    raise SystemExit(main())
