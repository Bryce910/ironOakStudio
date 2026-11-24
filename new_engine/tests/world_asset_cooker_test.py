from pathlib import Path
import importlib.util
import struct
import tempfile
import json
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("asset_cooker", ROOT/"tools/import_world_asset.py")
cooker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cooker)
source = ROOT/"projects/valwood_world/models/starting_crate.glb"
original = source.read_bytes()
with tempfile.TemporaryDirectory(prefix="valwood-import-test-", dir="/tmp") as directory:
    project = Path(directory)
    stem = cooker.cook(project, "user_crate", (source, source, None), 1)
    managed = project/"models"/(stem+".glb")
    assert managed.read_bytes() == original and (project/"models"/(stem+"_near.glb")).read_bytes() == original
    assert cooker.cook(project, "user_crate", (source, source, None), 1) == stem
    changed = cooker.cook(project, "user_crate", (source, None, None), .01)
    assert changed != stem and managed.read_bytes() == original
    raw=(project/"models"/(changed+".glb")).read_bytes(); size, kind=struct.unpack_from("<II", raw, 12)
    doc=json.loads(raw[20:20+size]); children={v for node in doc["nodes"] for v in node.get("children", [])}
    for i,node in enumerate(doc["nodes"]):
        if i not in children:
            assert "matrix" in node or "scale" in node
    for bad in ("../escape", "", "a/b"):
        try:cooker.cook(project, bad, (source,None,None),1)
        except ValueError:pass
        else:raise AssertionError("Unsafe alias accepted")
    frozen=project/"frozen-source.glb";frozen.write_bytes(original)
    linked_stem = cooker.cook(project, "source_linked", (frozen, frozen, None), 1, immutable_links=True)
    linked = project/"models"/(linked_stem+".glb")
    assert linked.stat().st_ino == frozen.stat().st_ino
    assert (project/"models"/(linked_stem+"_mid.glb")).stat().st_ino == frozen.stat().st_ino
    assert cooker.cook(project, "source_linked", (frozen, frozen, None), 1, immutable_links=True) == linked_stem
    linked.unlink()
    assert source.read_bytes() == original
    try:cooker.cook(project,"reject_scaled_link",(source,None,None),.01,immutable_links=True)
    except ValueError:pass
    else:raise AssertionError("Converted geometry cannot hardlink original bytes")
    (project/"models/imports/escaped").symlink_to("/tmp")
    try:cooker.cook(project,"escaped",(source,None,None),1)
    except ValueError:pass
    else:raise AssertionError("Escaping managed symlink accepted")
assert source.read_bytes()==original
print("PASS immutable asset imports: source preservation, unit conversion, LOD copies, content-addressed reimport, invalid aliases and symlink confinement")
