# Tavern practical-light receiving bounds

The first interior light domains excluded all four inner wall faces. Their lower Y bound also put the 15mm floor inside the shader’s 10cm feather, retaining at most 6.075% of its clipping weight; edge planks could lose it entirely. The upper Y bound excluded the upper roof underside.

The two interior emitters now receive within village-local X `[-155,-135]`, Z `[-90,-74]`, and world Y `[23.8,32.3]`. The hearth’s inward plane is `+Z`, with local plane constant `90`. This places the domain boundaries inside the solid wall thickness: inner faces receive full weight and outer faces remain excluded. Source sockets, intensities, mesh geometry, paths, other lights and previous scenery files are unchanged.

Run `python3 tests/castle_streets_lights_test.py` from the engine root. The test reads actual near/mid GLB triangles, source collider face coordinates, the published 17-field emitter include, and clipping-feather widths from the foliage, terrain and water shaders. It checks floor planks, all four inner walls, upper ceiling samples and actual outer wall faces. It also demonstrates the previous policy’s failure on those same surfaces.

[CPU results](receiving-domain-cpu.json) show full corrected clipping weight on interior probes and zero weight on exterior wall probes for both LODs. This test isolates receiving-domain clipping; physical distance falloff, surface normals and native appearance are checked separately.
