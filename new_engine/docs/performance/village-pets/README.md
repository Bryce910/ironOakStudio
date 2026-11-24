# Castle district cats and dogs

The village has six cats and six dogs. Three cats stay near the well and service
verge, three dogs sit near arrivals/service areas, and six animals follow short,
slow loops. They are visual residents; petting, feeding, combat and navigation AI
are outside this pass.

The Labrador comes from the user's `50+ Animated Animals` purchase. The textured
Bicolor Cat is by kenchoo, adapted from Fripouille by guillaume bolis, under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Both original creators,
source links and changes are recorded in
[LICENSE_SOURCE.txt](/home/computer/Desktop/final_engine/new_engine/assets/purchased/pets/LICENSE_SOURCE.txt)
and [PROVENANCE.json](/home/computer/Desktop/final_engine/new_engine/assets/purchased/pets/PROVENANCE.json).
Original source files remain unchanged.

Dog walking, sniffing and sitting use the supplied rig/poses. The cat's supplied
standing head/breathing motion is retained; its walk is an authored offline
four-paw IK adaptation. Runtime interpolates fixed compatible meshes entirely on
the GPU, including normals and tangents, using the existing model, material,
shadow, reflection and reactive-alpha contracts. There is no per-frame CPU mesh
deformation or new render pass.

The 32 baked assets share three image policies: two 1K source-color textures and
one 512px cat normal map. Estimated RGBA8 plus mip memory is 12 MiB; all pose
geometry totals 8,426,944 bytes. Each animal is one material/primitive and at most
8,646 triangles. Texture owners are shared and released through the existing
cache.

Validation:

- `python3 tools/build_village_pets.py --test`: source provenance and exact asset
  hashes; stable indices/UVs and image bytes between all poses; material,
  geometry, image, height and memory limits.
- `cpu.log`: 600 seconds at 60 Hz, 432,000 probes against actual scene collision
  boxes and terrain, minimum 2.4 m animal separation, smooth motion, atomic load
  failures, bounded culling, zero hot allocations/uniform queries/relinks, and
  released ownership.
- `human-pet-clearance.log`: actual 128-person and 12-animal runtimes together,
  55,297,536 human-animal pairs across 600 seconds at 60 Hz. Minimum separation
  5.008992 m exceeds the conservative body capsules and safety margin.
- `software-gl.log`: real GLB imports after the full scenery load, 32 compatible
  poses, three shared image keys, animated fractional poses at adjacent 60 Hz
  times, matching color/shadow depth, mirrored reuse, reactive alpha 0.15 and its
  reset, zero unmanaged image uploads and no remaining texture owners.

The CPU/software tests establish correctness. Native frame-rate and close camera
captures are recorded by the parent task for the entire updated village.
