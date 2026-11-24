# Using the new surface tools

Open Studio with **F11**. Edits modify a draft. **Apply** prepares resources and updates the live world; **Save** persists the document. Use the editor's Undo/Redo controls to restore earlier draft states. Invalid images, shaders and unsafe terrain grades reject the Apply operation and retain the previous live result.

## Materials, terrain and texture projection

1. In **Surfaces → Texture library**, enter a full local image path, select its role and choose **Import image**. Source files remain untouched; the project gets an immutable revision. Albedo uses sRGB; normals and ARM use linear values. ARM channels are AO, roughness and metallic respectively.
2. Choose **New PBR layer**, assign the imported maps and set scale, tint and material response. Turn on **DirectX normal (invert green)** for normal maps using that convention. A wrong normal convention makes dents appear raised and can spoil lighting.
3. Select a model and choose **Bind selected model**, or **Bind terrain**. Bindings affect all instances/LODs of that model alias. Blend between two layers or preserve the original source, using constant, noise, height or slope masks.
4. For local terrain detail, set a world target, paint radius and opacity, then **Stamp at world target**. Enable **Erase toward original terrain** to remove the layer contribution. Paint changes material weights; sculpting and foliage masks remain separate. Apply rebuilds the weight field.
5. For a projected stain, mark or material patch, choose **New image projection at target**. Adjust center, size, XYZ rotation, opacity, feather and facing cutoff. Local +Y is the receiving direction; rotate 90° for a wall. Projectors work on terrain and opaque scenery. The nearest eight are drawn within 180m.

The runtime import filter is 1020px per image. Importing a larger source preserves the original but does not increase this filter's resolution. Existing purchased model textures use their own source texture path and can remain higher resolution. This is a deliberate bounded implementation, not an unlimited virtual-texturing system.

## Wind and roads

In **Wind / Roads**, set a world target and add a local wind volume. Change its center, ellipsoid radius, XZ direction, speed, gust strength and period. Wind augments global weather. The renderer caps combined speed at 5m/s and uses the same displacement for foliage and its shadows.

For a road, choose **New road at target**, append target points or type their XYZ coordinates, and set width/shoulder and optional PBR road layer. **Use current ground heights** gives a starting profile. Smoothing uses Catmull-Rom curves; turn it off for straight segments. Apply grades actual ground. Removing the path restores the underlying source terrain. Paths are bounded to 32 points / 1024m, with neighboring points 0.25–256m apart. Split longer routes into sections.

For a river, choose **New river at target**, set the water profile, width, banks and channel depth. At the terrain grid's 2m spacing, use width ≥4m and banks ≥2m. Apply creates a channel and water mesh and updates CPU water/navigation queries. Foundation and clearance checks can reject unsafe grades. These are authored rivers with animated water shaders, not hydrodynamic simulation or an erosion solver.

## Graphs and limits

Graph image nodes sample the imported image library in fragment shaders. Assign the graph to a model alias, terrain, one of the water families or an NPC appearance group. Model/NPC vertex offsets must remain compatible with deformation and shadows. Image sampling is rejected in vertex outputs. Terrain/water vertex offsets are rejected; use their own geometry controls. Water's existing optical model controls reflection and transmission; its graph is not a replacement for the complete water-lighting model.

Keep reusable materials consistent before adding many projectors. This implementation supports up to 16 imported images, eight PBR layers, 64 bindings, 256 paint stamps across 64 terrain tiles, 64 projection records, 16 local wind volumes and 16 road/river curves per document. The limits are validated on Apply, so rejected edits do not partially replace the scene.

The published demo sky is the CC0 [Overcast Soil PureSky panorama from Poly Haven](https://polyhaven.com/a/overcast_soil_puresky). Original image, attribution/provenance and license are included. The reviewed look changes lighting and the environment only; original world state and protected-row checks are in look-published/.
