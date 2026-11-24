# Windows orientation build

This is a Windows x64 cross-build of the owner's custom C/C++ SDL/OpenGL engine,
not a Unity project. Application DLLs, embedded Python, project runtime content,
guides, provenance, and change-return tools are bundled. No Unity installation
or Python installation is required to run the delivered editor.

Use Windows 10/11 x64 with the GPU manufacturer's OpenGL driver. Shader/material
and imported-model limits are documented in EDITOR_GUIDE.md and SURFACE_GUIDE.md.
Keep original Blender/FBX/Unity source archives separately; static editor imports
use embedded GLB. Rig conversion/retargeting is a separate workflow.

The transport stores shared bytes once. Installation creates independent regular
files in a new package folder; creating a work folder copies that package again.
The installer reports each project's materialized size and checks disk space.
Install only the area you need, and retain the original orientation download.

Per-file SHA256 manifests are checked before publication. Opening the editor,
testing its input, and walking the result on the artist PC are still required.
Wine compatibility checks do not certify native Windows GPU behavior or FPS.

Approximate installed seed / seed plus separate work folder: city **16 /32GB**,
jungle **7 /14GB**, Grandstone **25 /50GB**. Allow additional space for the
extracted transport (about23GB uncompressed) and the downloaded ZIP itself.
These round up the actual validated project plans; the installer checks exact
bytes and a filesystem margin before copying. Start with one project.
