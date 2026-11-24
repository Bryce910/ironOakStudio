#!/usr/bin/env python3
"""Validate and inventory a Valwood runtime project without changing its sources.

The current renderer has compiled-in model/scene references. Accordingly this is
a conservative *full runtime project* inventory, not a claim of minimal scene
dependency closure. Static GLB import revisions share a stable logical alias ID;
individual files have separate artifact IDs and content versions. Source files,
archives, credentials, saves and captures are never gathered implicitly.
"""
from __future__ import annotations

import argparse
import array
from dataclasses import dataclass
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import stat
import struct
import sys
import tempfile
from urllib.parse import unquote, urlsplit
import uuid

VERSION = 1
NAMESPACE = uuid.UUID("df05247d-f4ca-51b8-8e25-a6a092e70188")
RUNTIME_DIRS = frozenset({"models", "scenery", "ground", "particles", "audio", "sounds",
                          "shaders", "fonts", "environments", "assets",
                          "texture_blocks", "imports", "textures", "surfaces", "ai", "prefabs", "cinematics",
                          "opening", "blood_assets"})
ROOT_FILES = frozenset({"project.valwood", "world.studio", "graphics.settings",
                        "world_generation.json", "visual_upgrade.json", "milestone_standard.json", "README.txt", "world.living", "world.regions", "world.physics", "world.gameplay", "world.components",
                        "city_showcase.json", "publisher_showcase.json", "stronghold.json", "jungle_expansion.json",
                        "city.scene", "city.sky", "city.physics", "showcase.views",
                        "showcase.flames", "showcase.flames.json", "showcase.residents",
                        "opening.config", "opening.scene", "CHARACTER_POLISH.json"})
SUFFIXES = frozenset({".glb", ".gltf", ".bin", ".modelc", ".fbbin", ".glbbin",
                     ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".webp", ".hdr",
                     ".exr", ".img", ".vbc7", ".vfx", ".wav", ".ogg", ".mp3", ".ttf",
                     ".otf", ".txt", ".md", ".json", ".settings", ".ini",
                     ".glsl", ".vs", ".fs", ".geom", ".vert", ".frag", ".inc",
                     ".csv", ".valwood", ".studio", ".svg", ".vwml", ".vwmemory", ".vwminds", ".vwlooks", ".vwprefab", ".vct", ".living", ".regions", ".physics", ".gameplay", ".components",
                     ".height", ".coverage", ".roofs", ".scene", ".sky", ".views", ".flames", ".residents",
                     ".config", ".bank"})
PRIVATE_PARTS = frozenset({"downloads", ".ssh", ".aws", ".codex", ".agents",
                           ".git", "credentials", "secrets", "__pycache__", "saves", "captures"})
PRIVATE_NAMES = frozenset({"credentials", "secrets", "passwords", "token", "tokens",
                            "id_rsa", "id_ed25519"})
PRIVATE_KEYS = frozenset({"password", "api_key", "secret_key", "access_token",
                          "refresh_token", "credentials", "private_key"})
MAX_METADATA = 16 * 1024 * 1024


class ValidationError(ValueError):
    pass


def canonical_json(value):
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True,
                       allow_nan=False) + "\n").encode("utf-8")


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def relative_name(value):
    """References use portable POSIX paths; reject rather than normalize traversal."""
    if not isinstance(value, str) or not value or "\x00" in value or "\\" in value:
        raise ValidationError("Invalid runtime reference")
    if value.startswith("/") or re.match(r"^[A-Za-z]:", value):
        raise ValidationError("Absolute runtime references are not portable")
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ValidationError("Runtime reference contains traversal/empty components")
    if any(part.casefold() in PRIVATE_PARTS or part.startswith(".") for part in parts):
        raise ValidationError("Runtime reference enters a private directory")
    return PurePosixPath(*parts).as_posix()


def open_regular_nofollow(path):
    """Open an already resolved absolute path without following any new symlink.

    Walking dirfds also protects parent components, unlike O_NOFOLLOW on just the
    last component. Identity/hash checks below detect source replacement or edits.
    """
    path = Path(path)
    if not path.is_absolute():
        raise ValidationError("Internal source path must be absolute")
    if os.name == 'nt':
        from windows_file_io import open_regular
        return open_regular(path)
    directory = os.open("/", os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in path.parts[1:-1]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                            dir_fd=directory)
            os.close(directory)
            directory = child
        fd = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory)
    finally:
        os.close(directory)
    if not stat.S_ISREG(os.fstat(fd).st_mode):
        os.close(fd)
        raise ValidationError("Runtime input is not a regular file")
    return os.fdopen(fd, "rb")


@dataclass
class Source:
    relative: str
    lexical: Path
    resolved: Path
    identity: tuple
    source_hash: str
    size: int
    content: bytes | None = None

    def read_verified(self):
        if self.lexical.resolve(strict=True) != self.resolved:
            raise ValidationError(f"Source link changed during build: {self.relative}")
        with open_regular_nofollow(self.resolved) as stream:
            info = os.fstat(stream.fileno())
            if (info.st_dev, info.st_ino) != self.identity:
                raise ValidationError(f"Source replaced during build: {self.relative}")
            value = stream.read()
        if sha256_bytes(value) != self.source_hash:
            raise ValidationError(f"Source changed during build: {self.relative}")
        return self.content if self.content is not None else value


def _sanitize(value, engine, project):
    if isinstance(value, dict):
        return {key: ("[private value omitted]" if key.casefold() in PRIVATE_KEYS else
                      _sanitize(child, engine, project)) for key, child in value.items()}
    if isinstance(value, list):
        return [_sanitize(child, engine, project) for child in value]
    if not isinstance(value, str):
        return value
    # Historical source provenance stays descriptive, never a file dependency.
    if value.startswith(str(project) + "/"):
        return "project:" + value[len(str(project)) + 1:]
    if value.startswith(str(engine) + "/"):
        return "engine:" + value[len(str(engine)) + 1:]
    if value.startswith("/") or re.match(r"^[A-Za-z]:[\\/]", value):
        return "source-file:" + value.replace("\\", "/").rsplit("/", 1)[-1]
    # Some prose records contain an absolute path embedded in a sentence.
    return re.sub(r"(?:/home/|/Users/|/tmp/)[^\s\"<>]+",
                  lambda match: "source-file:" + match[0].rstrip(".,;").rsplit("/", 1)[-1], value)


class ProjectInventory:
    def __init__(self, project, engine_root, *, previous=None, project_id=None, active_imports_only=False):
        self.project = Path(project).resolve(strict=True)
        self.engine = Path(engine_root).resolve(strict=True)
        if not self.project.is_dir() or not self.engine.is_dir():
            raise ValidationError("Project and engine root must be directories")
        if not self.project.is_relative_to(self.engine):
            raise ValidationError("Project must be inside the explicit engine root")
        self.sources: dict[str, Source] = {}
        self.files = []
        self.warnings = []
        if previous is not None and (not isinstance(previous, dict) or
                previous.get("schema") != "valwood.asset-database" or previous.get("schema_version") != VERSION):
            raise ValidationError("Unsupported previous asset database")
        self.previous = previous or {}
        self.previous_project_id = self.previous.get("project_id")
        self.explicit_project_id = project_id
        self.trusted_roots = [self.project] + [self.engine / name for name in
            ("models", "scenery", "ground", "particles", "audio", "shaders", "fonts",
             "environments", "assets", "textures", "texture_blocks", "sound_effects")]
        # The streamed project deliberately links cooked legacy assets from
        # projects/seldonne/models. Permit only named runtime banks of sibling
        # projects under this explicit engine, never their sources or saves.
        projects = self.engine / "projects"
        if projects.is_dir() and not projects.is_symlink():
            for sibling in projects.iterdir():
                if sibling.is_dir() and not sibling.is_symlink() and not sibling.name.startswith("."):
                    for filename in ROOT_FILES:
                        candidate = sibling / filename
                        if candidate.is_file() and not candidate.is_symlink():
                            self.trusted_roots.append(candidate)
                    for bank in RUNTIME_DIRS:
                        candidate = sibling / bank
                        if candidate.is_dir() and not candidate.is_symlink():
                            self.trusted_roots.append(candidate)
        self.aliases = {}
        self.active_imports_only = bool(active_imports_only)
        self._source_hashes = {}

    def _active_managed_revisions(self, dependencies):
        """Keep every nonmanaged runtime input and its complete revision closure."""
        pattern = re.compile(r'^(models/imports/[A-Za-z0-9_-]+/[0-9a-f]{24})/')
        revisions = {}; owner = {}
        for name in self.sources:
            match = pattern.match(name)
            if match:
                owner[name] = match[1]
                revisions.setdefault(match[1],set()).add(name)
        retained = set(); pending = [name for name in self.sources if name not in owner]
        opened = set()
        while pending:
            name = pending.pop()
            if name in retained: continue
            retained.add(name)
            revision = owner.get(name)
            if revision and revision not in opened:
                opened.add(revision); pending.extend(revisions[revision])
            pending.extend(dependencies.get(name,[]))
        removed = set(self.sources)-retained
        self.sources = {name:source for name,source in self.sources.items() if name in retained}
        return dict(excluded_files=len(removed),retained_revisions=len(opened),
            policy='All nonmanaged runtime files retained; dependency-referenced managed revisions retained whole, including recursive cross-revision dependencies')

    def safe_resolve(self, lexical):
        lexical = Path(lexical)
        try:
            resolved = lexical.resolve(strict=True)
        except (OSError, RuntimeError) as error:
            raise ValidationError("Missing, broken or cyclic runtime link: " + lexical.name) from error
        if not any(resolved.is_relative_to(root) for root in self.trusted_roots):
            raise ValidationError("Runtime symlink escapes the explicit runtime roots: " + lexical.name)
        if any(part.casefold() in PRIVATE_PARTS or part.startswith(".")
               for part in resolved.relative_to(self.engine).parts):
            raise ValidationError("Runtime symlink enters a private directory: " + lexical.name)
        if resolved.suffix.casefold() not in SUFFIXES:
            raise ValidationError("Runtime link points to an unsupported/source file: " + lexical.name)
        return resolved

    def add(self, relative, lexical):
        relative = relative_name(relative)
        resolved = self.safe_resolve(lexical)
        if relative in self.sources:
            raise ValidationError("Duplicate runtime path: " + relative)
        with open_regular_nofollow(resolved) as stream:
            before = os.fstat(stream.fileno())
            stamp = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns)
            source_hash = self._source_hashes.get(stamp)
            if source_hash is None:
                digest = hashlib.sha256()
                while block := stream.read(1024 * 1024):
                    digest.update(block)
                source_hash = digest.hexdigest()
            after = os.fstat(stream.fileno())
        if stamp != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            raise ValidationError("Runtime input changed while hashing: " + relative)
        self._source_hashes[stamp] = source_hash
        self.sources[relative] = Source(relative, Path(lexical), resolved,
            (before.st_dev, before.st_ino), source_hash, before.st_size)

    def walk(self, lexical, relative, ancestors=()):
        # Directory links are supported only under the same explicit boundary;
        # resolved identities detect cycles and all package outputs are regular.
        try:
            resolved = lexical.resolve(strict=True)
        except (OSError, RuntimeError) as error:
            raise ValidationError("Missing/broken runtime entry: " + relative) from error
        if not any(resolved.is_relative_to(root) for root in self.trusted_roots):
            raise ValidationError("Runtime directory escapes explicit roots: " + relative)
        if resolved.is_dir():
            identity = (resolved.stat().st_dev, resolved.stat().st_ino)
            if identity in ancestors:
                raise ValidationError("Cyclic runtime directory: " + relative)
            for child in sorted(lexical.iterdir(), key=lambda value: value.name):
                name = child.name
                if (name.startswith(".") or name.casefold() in PRIVATE_PARTS or
                        child.stem.casefold() in PRIVATE_NAMES or "Zone.Identifier" in name):
                    self.warnings.append("Excluded private/OS metadata: " + relative + "/" + name)
                    continue
                if child.is_dir() or child.is_symlink() or child.suffix.casefold() in SUFFIXES:
                    # Archives/raw Blender/FBX sources are intentionally not runtime assets.
                    if not child.is_dir() and child.suffix.casefold() not in SUFFIXES:
                        self.warnings.append("Excluded non-runtime source: " + relative + "/" + name)
                        continue
                    self.walk(child, relative + "/" + name, ancestors + (identity,))
                else:
                    self.warnings.append("Excluded non-runtime source: " + relative + "/" + name)
        else:
            self.add(relative, lexical)

    def _reference(self, owner, reference, *, base=None):
        reference = relative_name(reference)
        path = (PurePosixPath(base if base is not None else PurePosixPath(owner).parent) / reference).as_posix()
        if path not in self.sources:
            raise ValidationError(f"Missing runtime dependency: {owner} -> {path}")
        return path

    def _gltf_dependencies(self, name):
        source = self.sources[name]
        with open_regular_nofollow(source.resolved) as stream:
            if name.endswith(".glb"):
                header = stream.read(20)
                if len(header) != 20:
                    raise ValidationError("Truncated GLB: " + name)
                magic, version, total, count, kind = struct.unpack("<IIIII", header)
                if magic != 0x46546C67 or version != 2 or total != source.size or kind != 0x4E4F534A or count > MAX_METADATA:
                    raise ValidationError("Invalid GLB header/metadata: " + name)
                raw = stream.read(count)
            else:
                raw = stream.read(MAX_METADATA + 1)
                if len(raw) > MAX_METADATA:
                    raise ValidationError("Oversized glTF metadata: " + name)
        try:
            doc = json.loads(raw)
        except (ValueError, UnicodeError) as error:
            raise ValidationError("Invalid glTF JSON: " + name) from error
        if not isinstance(doc, dict) or not isinstance(doc.get("buffers", []), list) or not isinstance(doc.get("images", []), list):
            raise ValidationError("Invalid glTF metadata structure: " + name)
        dependencies = []
        for part in doc.get("buffers", []) + doc.get("images", []):
            if not isinstance(part, dict):
                raise ValidationError("Invalid glTF buffer/image: " + name)
            uri = part.get("uri")
            if uri is not None and not isinstance(uri, str):
                raise ValidationError("Invalid glTF URI: " + name)
            if uri is None or uri.startswith("data:"):
                continue
            parsed = urlsplit(uri)
            if parsed.scheme or parsed.netloc or parsed.query or parsed.fragment:
                raise ValidationError("Network/absolute glTF URI is not packageable: " + name)
            dependencies.append(self._reference(name, unquote(parsed.path)))
        return dependencies

    def _studio_dependencies(self, name, raw=None, *, register=True):
        if self.sources[name].size > MAX_METADATA:
            raise ValidationError("Oversized Studio document")
        raw = self.sources[name].read_verified() if raw is None else raw
        rows = raw.decode("utf-8").splitlines()
        if not rows or rows[0] not in {f"VALWOOD_STUDIO {version}" for version in range(1, 15)}:
            raise ValidationError("Unsupported Studio document version")
        version = int(rows[0].split()[1])
        dependencies = []
        sound_count, weather_count, have_global = 0, 0, False
        sound_weather = set()
        def bounded(value, low, high):
            try:
                number = float(value)
            except ValueError as error:
                raise ValidationError("Malformed weather numeric value") from error
            if not math.isfinite(number) or not low <= number <= high:
                raise ValidationError("Weather value outside supported bounds")
            return number
        def profile(values):
            if len(values) != 17 or any(v not in ("0", "1") for v in values[:3]):
                raise ValidationError("Malformed weather profile")
            for offset, low, high in ((3, 0, 1), (5, 0, 1), (7, 0, 1.5),
                                      (9, 1, 180), (11, 0, 4), (13, 0, 1)):
                if bounded(values[offset], low, high) > bounded(values[offset + 1], low, high):
                    raise ValidationError("Inverted weather range")
            if not re.fullmatch(r"[0-9]{1,10}", values[15]) or int(values[15]) > 0xffffffff:
                raise ValidationError("Invalid weather seed")
            bounded(values[16], .1, 3600)
        for row in rows[1:]:
            fields = shlex.split(row, comments=True)
            if not fields:
                continue
            if fields[0] == "sound":
                if version not in (13, 14) or len(fields) != 14:
                    raise ValidationError("Malformed Studio sound placement")
                if fields[3] not in ("0", "1") or fields[4] not in ("0", "1") or fields[5] not in ("0", "1"):
                    raise ValidationError("Malformed Studio sound switches")
                sound_count += 1
                if fields[2]:
                    reference = relative_name(fields[2])
                    if PurePosixPath(reference).suffix.casefold() != ".wav":
                        raise ValidationError("Placed sounds require a WAV asset")
                    dependencies.append(self._reference(name, reference, base=""))
            elif fields[0] == "sound_weather":
                if version != 14 or len(fields) != 5 or not re.fullmatch(r"[0-9]{1,10}", fields[1]) or not re.fullmatch(r"[0-3]", fields[2]):
                    raise ValidationError("Malformed Studio sound weather condition")
                index = int(fields[1])
                if index >= sound_count or index in sound_weather or bounded(fields[3], 0, 1) > bounded(fields[4], 0, 1):
                    raise ValidationError("Invalid/duplicated sound weather condition")
                sound_weather.add(index)
            elif fields[0] == "weather_global":
                if version != 14 or len(fields) != 20 or have_global or fields[1] not in ("0", "1"):
                    raise ValidationError("Malformed/duplicated global weather")
                bounded(fields[2], 0, 120)
                profile(fields[3:])
                have_global = True
            elif fields[0] == "weather_area":
                if version != 14 or len(fields) != 27 or weather_count >= 16 or fields[2] not in ("0", "1"):
                    raise ValidationError("Malformed/oversized weather areas")
                name = fields[1]
                if not name.strip(" ") or len(name.encode("utf-8")) > 96 or any(ord(c) < 32 or 127 <= ord(c) <= 159 or c in "\u2028\u2029" for c in name):
                    raise ValidationError("Invalid weather area name")
                bounded(fields[3], -50400, 50400); bounded(fields[4], -2000, 12000); bounded(fields[5], -28400, 28400)
                if bounded(fields[6], 0, 10000) > bounded(fields[7], .1, 10000):
                    raise ValidationError("Invalid weather area radii")
                bounded(fields[8], 0, 120)
                if not re.fullmatch(r"-?[0-9]{1,10}", fields[9]) or not -1000 <= int(fields[9]) <= 1000:
                    raise ValidationError("Invalid weather priority")
                profile(fields[10:])
                weather_count += 1
            elif fields[0] == "environment":
                if len(fields) < 2:
                    raise ValidationError("Malformed Studio environment")
                dependencies.append(self._reference(name, fields[1], base=""))
            elif fields[0] == "surfaces":
                if len(fields) != 2:
                    raise ValidationError("Malformed Studio surface record")
                surface = shlex.split(fields[1], comments=False)
                if surface and surface[0] == "image":
                    if len(surface) != 4 or surface[3] not in ("0", "1", "2"):
                        raise ValidationError("Malformed Studio surface image reference")
                    dependencies.append(self._reference(name, surface[2], base=""))
            elif fields[0] == "asset":
                if len(fields) != 9:
                    raise ValidationError("Malformed Studio asset definition")
                alias, stem = fields[1:3]
                stem = relative_name(stem)
                try:
                    lods, collision, vegetation = map(int, fields[3:6])
                    units = float(fields[8])
                except ValueError as error:
                    raise ValidationError("Malformed Studio asset settings") from error
                if lods not in (1, 2, 3) or vegetation not in (0, 1, 2) or not .001 <= units <= 1000:
                    raise ValidationError("Invalid Studio import settings")
                first = "models/" + stem + ("_near.glb" if vegetation == 2 else ".glb")
                if first not in self.sources and "models/" + stem + "_near.glb" in self.sources:
                    first = "models/" + stem + "_near.glb"
                refs = [first] + ["models/" + stem + suffix for suffix in ("_mid.glb", "_far.glb")[:lods - 1]]
                dependencies.extend(self._reference(name, ref, base="") for ref in refs)
                if register:
                    self.aliases[alias] = {"stem": stem, "lods": lods, "collision": collision,
                        "vegetation": vegetation, "units": units, "files": refs}
        return dependencies

    def _production_dependencies(self, name):
        source = self.sources[name]
        if source.size > 7 * 1024 * 1024:
            raise ValidationError("Oversized authoring asset: " + name)
        raw = source.read_verified()
        suffix = PurePosixPath(name).suffix.casefold()
        if suffix == ".vwminds":
            if source.size > 4 * 1024 * 1024:
                raise ValidationError("Oversized NPC mind bindings: " + name)
            # Profiles are embedded, not references to a developer's Downloads.
            # The native project validator checks complete memory schemas and
            # explicit scene/actor closure before a package is accepted.
            try:
                fields = shlex.split(raw.decode("utf-8"))
                count = int(fields[2]); at = 3; targets = set()
                if fields[:2] != ["VALWOOD_NPC_MINDS", "1"] or not 0 <= count <= 32:
                    raise ValueError("header")
                for _ in range(count):
                    marker, entity, tags = fields[at:at + 3]; at += 3
                    entity, tags = int(entity), int(tags)
                    if marker != "BIND" or not 0 < entity <= 18446744073709551615 or entity in targets or not 0 <= tags <= 16:
                        raise ValueError("binding")
                    targets.add(entity); context = fields[at:at + tags]; at += tags
                    if any(not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,31}", tag) for tag in context):
                        raise ValueError("tag")
                    profile = fields[at]; at += 1
                    if not profile.startswith(("VALWOOD_NPC_MEMORY 1\n", "VALWOOD_NPC_MEMORY 2\n")):
                        raise ValueError("profile")
                if fields[at:] != ["END"]:
                    raise ValueError("end")
                return []
            except (ValueError, IndexError, UnicodeError) as error:
                raise ValidationError("Malformed NPC mind bindings: " + name) from error
        if suffix == ".vwlooks":
            if source.size > 32768:
                raise ValidationError("Oversized NPC appearance bindings: " + name)
            try:
                fields = shlex.split(raw.decode("utf-8"))
                count = int(fields[2])
                if fields[:2] != ["VALWOOD_NPC_LOOKS", "1"] or not 0 <= count <= 32 or len(fields) != 4 + 3 * count or fields[-1] != "END":
                    raise ValueError("header/count")
                targets, references = set(), []
                for i in range(count):
                    marker, target, reference = fields[3 + 3 * i:6 + 3 * i]
                    target = int(target)
                    if marker != "BIND" or not 0 <= target < 140 or target in targets or len(reference.encode("utf-8")) > 511 or any(c in reference for c in "\r\n\t\0"):
                        raise ValueError("target")
                    targets.add(target)
                    references.append(self._reference(name, reference, base=""))
                return references
            except (ValueError, IndexError) as error:
                raise ValidationError("Malformed NPC appearance bindings: " + name) from error
        if suffix == ".components":
            return [self._reference(name, reference, base="")
                    for reference in self._component_references(name, raw)]
        if suffix == ".gameplay":
            if source.size > 2 * 1024 * 1024 or raw.partition(b"\n")[0].rstrip(b"\r") != b"VALWOOD_GAMEPLAY 1":
                raise ValidationError("Unsupported/oversized gameplay document: " + name)
            try:
                raw.decode("utf-8")
            except UnicodeError as error:
                raise ValidationError("Invalid UTF-8 gameplay document: " + name) from error
            # Runtime entities, events and typed action references are checked
            # by the native authoring validator, not inferred as file paths.
            return []
        if suffix == ".vwprefab":
            header = raw.split(b"\n", 4)
            if len(header) != 5 or header[0] != b"VALWOOD_PREFAB 1":
                raise ValidationError("Unsupported prefab wrapper: " + name)
            try:
                content_size, graph_size = map(int, header[3].split())
            except ValueError as error:
                raise ValidationError("Malformed prefab lengths: " + name) from error
            if not 0 < content_size <= 4 * 1024 * 1024 or not 0 < graph_size <= 2 * 1024 * 1024 or len(header[4]) != content_size + graph_size:
                raise ValidationError("Truncated/oversized prefab payload: " + name)
            graph = header[4][content_size:]
            if not graph.startswith((b"VALWOOD_ENTITIES 1 ", b"VALWOOD_ENTITIES 2 ", b"VALWOOD_ENTITIES 3 ", b"VALWOOD_ENTITIES 4 ", b"VALWOOD_ENTITIES 5 ", b"VALWOOD_ENTITIES 6 ")):
                raise ValidationError("Unsupported prefab identities: " + name)
            return self._studio_dependencies(name, header[4][:content_size], register=False)
        rows = raw.decode("utf-8").splitlines()
        if suffix == ".vct":
            if not rows or rows[0] != "VALWOOD_CINEMATICS 1":
                raise ValidationError("Unsupported cinematic document: " + name)
            references = []
            for row in rows[1:]:
                fields = shlex.split(row)
                if fields and fields[0] in ("zone", "clip"):
                    index = 3 if fields[0] == "zone" else 2
                    if len(fields) <= index:
                        raise ValidationError("Malformed cinematic audio reference: " + name)
                    references.append(self._reference(name, fields[index], base=""))
            return references
        if suffix == ".vwml":
            fields = raw.decode("ascii").split()
            if fields[:5] != ["VALWOOD_MICRO_POLICY", "1", "18", "24", "5"] or len(fields) != 586:
                raise ValidationError("Unsupported micro-policy architecture: " + name)
            try:
                weights = list(map(float, fields[5:]))
            except ValueError as error:
                raise ValidationError("Malformed micro-policy weights: " + name) from error
            if any(not math.isfinite(value) or abs(value) > 32 for value in weights):
                raise ValidationError("Invalid micro-policy weights: " + name)
            return []
        if suffix == ".living":
            if not rows or len(rows[0].split()) != 3 or rows[0].split()[:2] != ["VALWOOD_LIVING", "1"]:
                raise ValidationError("Unsupported living-world document: " + name)
            payload = raw.partition(b"\n")[2]
            identity = 14695981039346656037
            for byte in payload:
                identity = ((identity ^ byte) * 1099511628211) & 0xffffffffffffffff
            if rows[0].split()[2] != format(identity, "016x"):
                raise ValidationError("Living-world integrity check failed: " + name)
        return []

    def _component_references(self, name, raw=None):
        """Portable file closure only; the native loader validates typed IDs."""
        source = self.sources[name]
        if source.size > 128 * 1024:
            raise ValidationError("Oversized components document: " + name)
        raw = source.read_verified() if raw is None else raw
        if b"\0" in raw:
            raise ValidationError("NUL in components document: " + name)
        try:
            rows = raw.decode("utf-8").splitlines()
            if not rows or rows[0] != "VALWOOD_COMPONENTS 1":
                raise ValidationError("Unsupported components document: " + name)
            references, assets = [], False
            if len(rows) > 1282:
                raise ValidationError("Too many components records: " + name)
            for row in rows[1:]:
                fields = shlex.split(row, comments=False)
                if not fields:
                    raise ValidationError("Empty components record: " + name)
                if fields[0] == "assets" and not assets and len(fields) == 4:
                    assets = True
                    for reference, extension in zip(fields[1:], (".physics", ".gameplay", ".living")):
                        if not reference:
                            continue
                        reference = relative_name(reference)
                        if PurePosixPath(reference).suffix != extension:
                            raise ValidationError("Components asset has the wrong type: " + name)
                        references.append(reference)
                elif fields[0] in ("physics", "living"):
                    expected = 10 if fields[0] == "physics" else 17
                    if len(fields) != expected or any(not re.fullmatch(r"[0-9]{1,20}", value)
                                                      for value in fields[1:3]):
                        raise ValidationError("Malformed components binding: " + name)
                    ids = list(map(int, fields[1:3]))
                    if any(value == 0 or value > 0xffffffffffffffff for value in ids) or (
                            fields[0] == "living" and ids[1] > 0xffffffff):
                        raise ValidationError("Invalid components persistent ID: " + name)
                    values = list(map(float, fields[3:]))
                    if any(not math.isfinite(value) for value in values):
                        raise ValidationError("Nonfinite components binding: " + name)
                else:
                    raise ValidationError("Unknown/duplicated components record: " + name)
            if not assets:
                raise ValidationError("Missing components assets record: " + name)
            return references
        except (UnicodeError, ValueError) as error:
            if isinstance(error, ValidationError):
                raise
            raise ValidationError("Malformed components document: " + name) from error

    def _gather_component_assets(self):
        # A manifest may explicitly refer to a root-level exported subsystem
        # such as runtime-demo.physics. Gather ONLY its three typed files; never
        # expand the root allowlist to arbitrary source files or player saves.
        for name in sorted(self.sources):
            if not name.endswith(".components"):
                continue
            at = self.project
            for part in PurePosixPath(name).parts:
                at /= part
                if at.is_symlink():
                    raise ValidationError("Components document traverses a symlink: " + name)
            for reference in self._component_references(name):
                lexical = self.project / reference
                at = self.project
                for part in PurePosixPath(reference).parts:
                    at /= part
                    if at.is_symlink():
                        raise ValidationError("Components asset traverses a symlink: " + reference)
                if reference not in self.sources:
                    self.add(reference, lexical)

    def _live_runtime_dependencies(self, project_text):
        """Exact built-in live slice resources, not guessed scenery aliases."""
        settings = dict(row.split("=", 1) for row in project_text.splitlines()
                        if "=" in row and not row.lstrip().startswith("#"))
        atlas = any(key.strip() == "terrain" and value.strip() == "valwood_atlas"
                    for key, value in settings.items())
        adapter = self.engine / "engine/world_runtime_visuals.inc"
        if not atlas or not adapter.is_file() or adapter.is_symlink():
            return []
        # These paths are the actual default live actor, physics crate and
        # skeletal vertex contract. Full inventory already materializes any
        # trusted legacy cooked-model/shader links before copying a package.
        return [self._reference("project.valwood", path, base="") for path in (
            "models/animations/white_knight_brown_armor.glb",
            "models/animations/white_knight_brown_armor_demo.glb",
            "models/purchased_village_crate_near.glb",
            "shaders/world_skeletal.vs.glsl", "shaders/world_dynamic.vs.glsl")]

    def _height_region(self, name):
        """Use the same bounded little-endian field contract as the native viewer."""
        raw = self.sources[name].read_verified()
        if len(raw) < 48:
            raise ValidationError("Truncated authored height region: " + name)
        magic, columns, rows, x, z, dx, dz = struct.unpack_from('<8sIIdddd', raw)
        if (magic != b'VWHGT1\0\0' or not 2 <= columns <= 2049 or not 2 <= rows <= 2049
                or len(raw) != 48 + columns * rows * 4
                or not all(math.isfinite(v) for v in (x,z,dx,dz,x+(columns-1)*dx,z+(rows-1)*dz))
                or dx <= 0 or dz <= 0):
            raise ValidationError("Invalid authored height region: " + name)
        values = array.array('f'); values.frombytes(raw[48:])
        if sys.byteorder != 'little': values.byteswap()
        if not all(math.isfinite(v) for v in values):
            raise ValidationError("Non-finite authored height region: " + name)
        if name.endswith('.coverage') and not all(0 <= v <= 1 for v in values):
            raise ValidationError("Invalid authored ground coverage: " + name)

    def _showcase_dependencies(self):
        dependencies = {}
        if 'city_showcase.json' in self.sources:
            required = ['publisher_showcase.json', 'city.scene', 'ground/showcase.height', 'ground/city.coverage']
            required += [name for name in ('city.sky','city.physics','showcase.views') if name in self.sources]
            dependencies['city_showcase.json'] = [self._reference('city_showcase.json', name, base='') for name in required]
        surrounding = ('ground/countryside.height', 'ground/countryside.coverage')
        if any(name in self.sources for name in surrounding):
            dependencies['ground/countryside.height'] = [self._reference('ground/countryside.height', name, base='') for name in surrounding]
        if 'city.sky' in self.sources:
            source = self.sources['city.sky']
            if source.size > 16 * 1024:
                raise ValidationError('Oversized city sky manifest')
            lines = source.read_verified().decode('utf-8').splitlines()
            if not lines or lines[0] != 'VALWOOD_CITY_SKY 1' or len(lines) > 17:
                raise ValidationError('Invalid city sky manifest')
            records = {}
            for line in lines[1:]:
                if len(line) > 1024: raise ValidationError('Oversized city sky record')
                row = shlex.split(line, comments=True)
                if not row: continue
                if row[0] in records: raise ValidationError('Duplicate city sky record')
                records[row[0]] = row[1:]
            try:
                panorama = records['panorama']
                material = list(map(float,records['material']))
                cube = records['cube']; side = int(cube[0])
                valid = (set(records) == {'panorama','material','cube'} and len(panorama) == 1
                    and len(material) == 3 and all(math.isfinite(v) for v in material)
                    and 0 <= material[0] <= 8 and .1 <= material[1] <= 8 and abs(material[2]) <= 8
                    and len(cube) == 1 and 32 <= side <= 512 and not side & (side-1))
                if not valid: raise ValueError('records')
            except (KeyError,IndexError,ValueError) as error:
                raise ValidationError('Invalid city sky settings') from error
            dependencies['city.sky'] = [self._reference('city.sky', panorama[0], base='')]
        for name in self.sources:
            if name.endswith(('.height','.coverage','.roofs')): self._height_region(name)
        return dependencies

    def _metadata(self, name):
        source = self.sources[name]
        if source.size > MAX_METADATA:
            raise ValidationError("Oversized JSON manifest: " + name)
        try:
            doc = json.loads(source.read_verified())
        except (ValueError, UnicodeError) as error:
            raise ValidationError("Invalid JSON manifest: " + name) from error
        dependencies = []
        if isinstance(doc, dict):
            for ref in doc.get("runtime_dependencies", []):
                dependencies.append(self._reference(name, ref))
            if name in ("audio/demo/manifest.json", "blood_assets/BLOOD_ASSETS.json"):
                for entry in doc.get("assets", []):
                    dependencies.append(self._reference(name, entry["file"]))
            if name == "CHARACTER_POLISH.json":
                # This authored manifest lists the fixed runtime pose bank,
                # never arbitrary source files or a developer's Downloads.
                for entry in doc.get("files", []):
                    dependencies.append(self._reference(name, entry["file"], base="models/longsword_mocap"))
        sanitized = canonical_json(_sanitize(doc, self.engine, self.project))
        source.content = sanitized
        provenance = {}
        if isinstance(doc, dict):
            for key in ("source", "source_url", "source_page", "provenance", "license",
                        "license_url", "credit", "creator", "generator", "recipe",
                        "recipe_sha256", "source_recipe_sha256", "source_sha256"):
                if key in doc:
                    provenance[key] = _sanitize(doc[key], self.engine, self.project)
        return dependencies, provenance

    def _scenery_dependencies(self):
        """Read literal compiled alias recipes; never run a project/source script."""
        registry = self.engine / "engine/world_scenery.cpp"
        definitions = {}
        if registry.is_file() and not registry.is_symlink():
            text = registry.read_text(encoding="utf-8")
            for row in text.splitlines():
                match = re.match(r'\s*\{"([A-Za-z0-9_-]+)",\s*"([A-Za-z0-9_/-]+)",\s*(.*)\},?\s*$', row)
                if not match:
                    continue
                tail = [field.strip() for field in match[3].split(",")]
                try:
                    tree, lods = tail[0] == "true", int(tail[4])
                except (IndexError, ValueError):
                    continue
                near = tree or (len(tail) > 12 and tail[12] == "true")
                stem = "models/" + relative_name(match[2])
                files = [stem + ("_near.glb" if near else ".glb")]
                files += [stem + suffix for suffix in ("_mid.glb", "_far.glb")[:lods - 1]]
                definitions[match[1]] = files
        definitions.update({alias: record["files"] for alias, record in self.aliases.items()})
        dependencies = {}
        if not definitions:
            self.warnings.append("No compiled scenery registry supplied; instance aliases not checked")
            return dependencies
        for name, source in sorted(self.sources.items()):
            if not (name.startswith("scenery/") and name.endswith("_instances.txt")):
                continue
            if source.size > MAX_METADATA:
                raise ValidationError("Oversized scenery instance list: " + name)
            references = []
            for number, row in enumerate(source.read_verified().decode("utf-8").splitlines(), 1):
                row = row.partition("#")[0].strip()
                if not row:
                    continue
                alias = row.split()[0]
                if alias not in definitions:
                    raise ValidationError(f"Unknown scenery asset alias: {name}:{number}: {alias}")
                references.extend(self._reference(name, ref, base="") for ref in definitions[alias])
            dependencies[name] = sorted(set(references))
        return dependencies

    def build(self):
        for name in sorted(ROOT_FILES):
            path = self.project / name
            if path.exists() or path.is_symlink():
                self.add(name, path)
        if "project.valwood" not in self.sources:
            raise ValidationError("Missing project.valwood")
        if self.sources["project.valwood"].size > MAX_METADATA:
            raise ValidationError("Oversized project.valwood")
        for path in sorted(self.project.iterdir(), key=lambda value: value.name):
            if path.name.startswith("CREDITS") and path.suffix.casefold() in (".txt", ".md"):
                self.add(path.name, path)
            elif path.name in RUNTIME_DIRS:
                self.walk(path, path.name)
        self._gather_component_assets()
        project_text = self.sources["project.valwood"].read_verified().decode("utf-8")
        name = next((row.partition("=")[2].strip() for row in project_text.splitlines()
                     if row.partition("=")[0].strip() == "name"), "Valwood Project")
        requested = self.explicit_project_id or self.previous_project_id
        try:
            self.project_id = str(uuid.UUID(requested)) if requested else str(uuid.uuid5(NAMESPACE, "project:" + name))
        except (ValueError, AttributeError) as error:
            raise ValidationError("Invalid project ID") from error
        deps, provenance = {}, {}
        live_runtime = self._live_runtime_dependencies(project_text)
        if live_runtime:
            deps["project.valwood"] = live_runtime
        if "world.studio" in self.sources:
            deps["world.studio"] = self._studio_dependencies("world.studio")
        if "opening.config" in self.sources:
            deps["opening.config"] = [self._reference("opening.config", name, base="")
                for name in ("opening/home.settings", "opening/home.physics")]
            # Optional authored subsystems are retained when supplied. Their
            # internal numeric grammar is owned by the engine's runtime parser.
            deps["opening.config"].extend(name for name in self.sources if name.startswith("opening/"))
        deps.update(self._scenery_dependencies())
        for path, source in sorted(self.sources.items()):
            if path.casefold().endswith((".vwprefab", ".vct", ".vwml", ".vwlooks", ".vwminds", ".living", ".gameplay", ".components")):
                deps[path] = self._production_dependencies(path)
            elif path.endswith((".glb", ".gltf")):
                deps[path] = self._gltf_dependencies(path)
            elif path.endswith(".json") or path == "opening.scene":
                deps[path], record = self._metadata(path)
                if record:
                    provenance[path] = record
            elif PurePosixPath(path).suffix in (".glsl", ".vs", ".fs", ".vert", ".frag"):
                if source.size > MAX_METADATA:
                    raise ValidationError("Oversized shader: " + path)
                text = source.read_verified().decode("utf-8")
                deps[path] = [self._reference(path, ref) for ref in
                    re.findall(r'^\s*#\s*include\s+"([^"]+)"', text, re.MULTILINE)]
            elif PurePosixPath(path).suffix in (".txt", ".md") and not path.startswith("scenery/"):
                if source.size > MAX_METADATA:
                    raise ValidationError("Oversized text provenance: " + path)
                original = source.read_verified()
                sanitized = _sanitize(original.decode("utf-8"), self.engine, self.project).encode("utf-8")
                if sanitized != original:
                    source.content = sanitized
        for owner, references in self._showcase_dependencies().items():
            deps[owner] = sorted(set(deps.get(owner, []) + references))
        revision_selection = None
        if self.active_imports_only:
            revision_selection = self._active_managed_revisions(deps)
            from world_package_cache_selection import select_unused_revision_caches
            removed_caches = select_unused_revision_caches(self.sources, deps)
            if not removed_caches.issubset(self.sources):
                raise ValidationError("Cache selection returned an unknown runtime path")
            revision_selection["excluded_texture_cache_files"] = len(removed_caches)
            revision_selection["excluded_texture_cache_bytes"] = sum(self.sources[path].size for path in removed_caches)
            revision_selection["texture_cache_policy"] = "Only verified unused named caches nested in active managed revisions are excluded; unknown formats, explicit dependencies and nonmanaged/global cache files are retained"
            self.sources = {name:source for name,source in self.sources.items() if name not in removed_caches}
            deps = {name:references for name,references in deps.items() if name in self.sources}
            provenance = {name:record for name,record in provenance.items() if name in self.sources}
        namespace = uuid.UUID(self.project_id)
        groups = {}
        for path, source in sorted(self.sources.items()):
            managed = re.fullmatch(r"models/imports/([A-Za-z0-9_-]+)/([0-9a-f]{24})/asset(?:_near|_mid|_far)?\.glb", path)
            key = "import:" + managed[1] if managed else "path:" + path
            asset_id = str(uuid.uuid5(namespace, key))
            artifact_id = str(uuid.uuid5(namespace, "artifact:" + path))
            payload_hash = sha256_bytes(source.content) if source.content is not None else source.source_hash
            artifact = {"path": path, "asset_id": asset_id, "artifact_id": artifact_id,
                "sha256": payload_hash, "source_sha256": source.source_hash,
                "bytes": len(source.content) if source.content is not None else source.size,
                "source_bytes": source.size, "dependency_paths": sorted(set(deps.get(path, []))),
                "source_identity": ("project:" + path if source.resolved.is_relative_to(self.project)
                    else "engine:" + source.resolved.relative_to(self.engine).as_posix()),
                "metadata_sanitized": source.content is not None,
                "symlink_materialized": source.lexical.is_symlink() or source.lexical != source.resolved}
            self.files.append(artifact)
            asset = groups.setdefault(key, {"id": asset_id, "logical_key": key,
                "artifacts": [], "dependencies": [], "provenance_records": [],
                "import_recipe": {"pipeline": "managed-static-glb-v1" if managed else "full-project-runtime-copy-v1",
                    "tool_version": VERSION, "source_preserved": True,
                    "settings": self.aliases.get(managed[1], {}) if managed else {}}})
            asset["artifacts"].append(artifact_id)
        by_path = {item["path"]: item for item in self.files}
        for item in self.files:
            item["dependencies"] = sorted({by_path[path]["asset_id"] for path in item["dependency_paths"]})
            key = next(key for key, asset in groups.items() if asset["id"] == item["asset_id"])
            asset = groups[key]
            asset["dependencies"].extend(item["dependencies"])
            parent = str(PurePosixPath(item["path"]).parent)
            asset["provenance_records"] = sorted(set(asset["provenance_records"]) |
                {ref for ref in provenance if str(PurePosixPath(ref).parent) == parent})
        for asset in groups.values():
            asset["dependencies"] = sorted(set(asset["dependencies"]) - {asset["id"]})
            versions = sorted(by_path[path]["sha256"] for path in by_path if by_path[path]["asset_id"] == asset["id"])
            asset["version"] = sha256_bytes(canonical_json(versions))
        return {"schema": "valwood.asset-database", "schema_version": VERSION,
            "tool_version": VERSION, "project_id": self.project_id, "project_name": name,
            "scope": "full-runtime-with-active-managed-revision-closure" if self.active_imports_only else "conservative-full-project-runtime",
            "managed_revision_selection": revision_selection,
            "dependency_scope": "Explicit glTF, Studio asset/HDR, typed component assets, live demo rig/crate/shader, shader includes and declared runtime dependencies; other compiled references require full runtime project.",
            "live_runtime_dependencies": live_runtime,
            "stable_identity": "Logical project-relative paths; managed import aliases survive content revisions. Renames require explicit migration and are not inferred from hashes.",
            "files": self.files, "assets": sorted(groups.values(), key=lambda item: item["logical_key"]),
            "provenance": provenance, "warnings": sorted(set(self.warnings)),
            "summary": {"files": len(self.files), "logical_assets": len(groups),
                "source_bytes": sum(value.size for value in self.sources.values()),
                "packaged_bytes": sum(value["bytes"] for value in self.files)}}


def write_new_json(path, value, *, forbidden_roots=()):
    path = Path(path).absolute()
    parent = path.parent.resolve(strict=True)
    destination = parent / path.name
    if any(destination.is_relative_to(Path(root).resolve()) for root in forbidden_roots):
        raise ValidationError("Metadata output must be outside source runtime roots")
    fd, temporary = tempfile.mkstemp(prefix=".valwood-manifest-", dir=parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(canonical_json(value))
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary, destination)  # exclusive publication; never replace an original
        if os.name != 'nt':
            directory = os.open(parent, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
    finally:
        os.unlink(temporary)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", required=True, type=Path)
    parser.add_argument("--engine-root", required=True, type=Path)
    parser.add_argument("--output", type=Path, help="New database JSON; never overwrites")
    parser.add_argument("--previous-database", type=Path, help="Retain project identity across project rename/copy")
    parser.add_argument("--project-id", help="Explicit persistent project UUID")
    parser.add_argument("--validate-only", action="store_true", help="Read/validate only; write nothing")
    parser.add_argument("--active-imports-only", action="store_true", help="Exclude only unreferenced managed import revisions; retain every other runtime file")
    args = parser.parse_args(argv)
    try:
        previous = json.loads(args.previous_database.read_text()) if args.previous_database else None
        inventory = ProjectInventory(args.project, args.engine_root, previous=previous, project_id=args.project_id,
                                     active_imports_only=args.active_imports_only)
        database = inventory.build()
        if not args.validate_only:
            if not args.output:
                raise ValidationError("--output is required unless --validate-only is used")
            write_new_json(args.output, database, forbidden_roots=[inventory.project])
        print(json.dumps({"ok": True, "validate_only": args.validate_only,
            "schema_version": VERSION, "project_id": database["project_id"],
            "summary": database["summary"], "warnings": database["warnings"]}, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        print(json.dumps({"ok": False, "error": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
