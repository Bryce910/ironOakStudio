# Windows delivery

Run **Valwood World Designer.exe** after extracting the entire archive. It includes the Windows x64 custom-engine runtime, matching document validator, runtime DLLs, embedded Python, asset metadata and install/return helpers. No Unity or separate Python installation is required.

The archive stores shared SHA256 payloads once and installs ordinary independent files for each selected seed/workcopy. No project-asset staging extraction was used to build the delivery. The included manifests and SHA sidecar identify the exact delivered bytes.

This updated editor reads Studio versions 1–9. Version 9 is required for independently painted vegetation layers and derived pond-basin provenance. All four bundled areas use the matching runtime and export parser.

OpenGL 3.3 support from the GPU vendor driver is required. Performance and available VRAM vary by computer; adjust graphics settings and complete TESTING_STATUS.md on the receiving PC.

Extract into a new folder. The corresponding `<delivery>_Work` directory holds pristine Seeds, editable Workspaces, Returns and Logs. Earlier deliveries and artist workcopies stay separate. Allow disk space for the archive, extracted delivery and each installed seed plus independent workcopy.
