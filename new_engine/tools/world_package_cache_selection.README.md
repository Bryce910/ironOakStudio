# Managed revision texture-cache selection

`select_unused_revision_caches(sources, deps)` returns a `set[str]` of safe-to-omit
inventory paths. It never writes files or changes either input. The inventory
caller opts in after selecting complete active managed revisions. Normal project
inventory remains unchanged.

The only eligible paths are
`models/imports/<alias>/<24 lowercase hex>/texture_blocks/<16 lowercase hex>(.linear)?.vbc7`.
Geometry, source images, root-level/global caches, unrelated directories and
explicit dependency references are preserved.

## Why omission cannot change rendering

`engine/world_model_cpu.cpp`'s `texture(...)` uses an embedded glTF image's exact
encoded bufferView bytes to request a cache. The filename fingerprint is only a
lookup key. `world_texture_blocks::read(...)` calls
`world_texture_blocks::parse(...)` in
`engine/world_texture_blocks_format.h`; `parse` checks encoded length and compares
the **complete original encoded source bytes** before accepting the cache.

For each retained revision, the selector hashes every bounded embedded image
across all its GLBs using C-backed SHA256. A recognized cache stores its original
encoded source immediately after the 64-byte header and `level_count * 40` bytes
of mip descriptors. The selector validates this bounded layout and hashes that
source region. If its `(encoded length, SHA256)` matches no image in the revision,
the native byte-identity comparison cannot accept it for any of those models.
The selector retains every matching sRGB, linear and normal variant; it does not
infer requested color space from material usage or reproduce FNV in Python.

The cache's source region is streamed once per verified inode/size/hash/stamp
identity, even when many revision directories link the same global cache file.
BC7 payloads are not decoded or copied. Cache quality certificates and CRCs remain
the native loader's responsibility; source mismatch alone is sufficient proof
that a cache cannot be used.

## Conservative boundaries

Any unsupported/malformed GLB, loose glTF, external/data URI image or buffer,
unbounded view, unknown image extension, unsupported cache header, incomplete mip
chain, changed source identity, or revision without a readable GLB causes the
affected cache/scope to remain. Limits mirror the native cache bounds; GLB input
is additionally bounded to 512 MiB and JSON to 16 MiB. Normal packaging identity
and safe-open checks still apply. No cache path outside the strict nested pattern
can be selected.

## Verification

Run from the repository root:

```sh
python3 new_engine/tests/world_package_cache_selection_test.py -v
python3 new_engine/tests/world_asset_database_test.py
```

The focused fixtures use real GLB JSON/BIN bufferViews and the native cache header,
mip descriptor and encoded-source layout. They cover all-image/all-LOD unions,
revision isolation, shared inode reuse, both color spaces and normal format,
explicit dependencies, malformed bounds, unknown formats, modified sources,
global-cache protection and input immutability. They do not require GPU execution.
