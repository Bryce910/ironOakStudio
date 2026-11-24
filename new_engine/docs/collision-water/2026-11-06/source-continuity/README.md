# Accepted source and future builds

The collision/water release is built from a private, versioned source snapshot. The canonical `engine/` and `shaders/` trees contain separate work and are **not the accepted release source**. Running the ordinary root `make` compiles that development tree. Its result must not be described or installed as the accepted collision/water release.

This folder provides a separate explicit entrypoint. It does not change the default Makefile, canonical source, any frozen input, or an artist's project. There is no compile, install, acceptance or promotion command here.

From the engine root:

```sh
make -f docs/collision-water/2026-11-06/source-continuity/Makefile verify
make -f docs/collision-water/2026-11-06/source-continuity/Makefile info
make -f docs/collision-water/2026-11-06/source-continuity/Makefile canonical-status
```

`verify` reads `docs/collision-water/2026-11-06/CURRENT.json`. It requires an explicitly accepted `source_continuity` record. It hashes every source manifest file, the reviewed builder, both frozen build plans and READY receipts, both runtimes and validators, and every registered installed executable/launcher. It rejects missing or changed bytes and inconsistent source/plan/runtime ancestry. `canonical-status` reports differences separately and exits 2 because this workflow does not certify a canonical merge, even if the manifest happens to match.

The verifier is a consistency check against the local registry, not a signature, test of gameplay, or substitute for runtime QA. Basic verification does not rehash every compiler, retained object, asset or parent-build input. For a reproduction prerequisite check, explicitly hash the union of the plans' protected inputs too:

```sh
python3 docs/collision-water/2026-11-06/source-continuity/verify_release.py verify --full-inputs
```

Frozen plans record the exact compile/link commands, retained object graph, source hashes, helper/artifact hashes and parent build receipts. Reproduction requires those recorded inputs to remain available. Do not rerun a builder into an already accepted output directory.

## Starting the next change

Copy the verified manifest into a **new** private destination, then implement/review a successor builder and plan using the recorded object graph. The copy preserves source bytes and file modes and writes `SOURCE_PARENT.json` with the parent registry, source manifest, builder and plan hashes. It copies no world, artist asset bank, collision payload or existing runtime output. It rejects existing destinations, paths outside the workspace's `build/`, and destinations inside accepted source/runtime outputs. It never removes existing paths; an interrupted copy remains marked `complete:false`.

```sh
make -f docs/collision-water/2026-11-06/source-continuity/Makefile source-copy \
  DEST=/home/computer/Desktop/final_engine/new_engine/build/my-next-change/source-v1
```

Freeze/build into new output paths, run the necessary CPU and actual runtime checks, and only then update CURRENT and installed file records. Resolving the canonical divergence requires a separate reviewed integration; do not blindly copy the private tree over concurrent edits. The read-only audit is `../structural/CANONICAL-CONTINUITY.json` (118 source differences at the audit time, including 23 conflicts within the new feature delta).

## Registry contract

Add the following nested object to CURRENT after runtime acceptance and installation. Existing delivery/QA fields can remain alongside it. All paths are absolute. Hashes are lowercase SHA-256. The accepted record must pin at least one installed runtime binary. Use roles `native-binary`, `native-validator`, `windows-binary`, `windows-validator`, or `launcher`; runtime/validator installed hashes must match their accepted build outputs. Do not register mutable artist scene or model files as installed runtime artifacts.

```json
{
  "source_continuity": {
    "schema": "valwood.accepted-source.v1",
    "release_status": "accepted",
    "source": "/absolute/private/source-v5",
    "builder": "/absolute/build_runtime_v5.py",
    "builder_sha256": "<64 hex characters>",
    "canonical_source_is_release": false,
    "canonical_integration": "pending",
    "native": {
      "plan": "/absolute/native/BUILD_PLAN.json",
      "plan_sha256": "<64 hex characters>",
      "ready": "/absolute/native/READY.json",
      "ready_sha256": "<64 hex characters>",
      "binary": "/absolute/native/native-bin/rpg",
      "binary_sha256": "<64 hex characters>",
      "validator": "/absolute/native/native-bin/valwood_project_validate",
      "validator_sha256": "<64 hex characters>"
    },
    "windows": { "same_fields_as_native": "using Windows outputs" },
    "installed_files": [
      {"role": "native-binary", "path": "/absolute/installed/rpg", "sha256": "<accepted native hash>"}
    ]
  }
}
```

To generate a **candidate** record from finished builds (stdout only):

```sh
python3 docs/collision-water/2026-11-06/source-continuity/verify_release.py draft \
  --native-plan /absolute/native/BUILD_PLAN.json \
  --windows-plan /absolute/windows/BUILD_PLAN.json \
  --builder /absolute/build_runtime_v5.py \
  --installed-file native-binary=/absolute/installed/rpg
```

`draft` checks the build/source/runtime chain and never writes CURRENT or marks acceptance. Add exact installed file records only after copying the accepted bytes; complete runtime QA before changing `release_status` to `accepted`. `CANDIDATE-v5.json` is an engineering candidate, not authority to install or a claim that acceptance has finished.

CPU fixture checks are available with the separate Makefile's `test` target. They exercise changed-source/output/receipt rejection, candidate rejection, canonical divergence, source-copy provenance and refusal to overwrite or escape the build directory. No graphics, production files, source freeze or installed executable is touched by the tests.
