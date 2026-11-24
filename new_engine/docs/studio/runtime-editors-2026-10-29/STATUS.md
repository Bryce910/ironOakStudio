# Runtime editors — work log

Implemented: v3 authoring documents (v1/v2 still readable), bounded local A* navigation and editable resident patrols, smoke/dust/fire editing, fixed-water appearance plus sculpted closed-basin ponds, and a typed model-material node graph with transactional GLSL compilation. These use the existing custom renderer and pose assets.

CPU tests pass for navigation detours, rejected water/blocked routes, document round trips, graph validation and program-ID cache invalidation. Native AMD tests pass for graph shaders, population CPU/GL, scenery CPU/GL, particles, water and weather. Full executable builds successfully.

Native Apply/Undo/reject/save/reload and actual graph Compile/Expand UI clicks passed. Native committed-feature captures and the Studio guide/report are saved. All four edited routes completed with zero blocked/slope-blocked steps, GL errors or remaining texture references. Average FPS 58.37–59.95; 1% lows 45.52–52.69; 21 brief sub-45 frames across 30,920 measured frames. Saved 0.7 render scale and quality features never changed. Production world.studio, graphics.settings and protected purchased placement files remain unchanged. Studio is open at the castle; native executable/PID/window and normal PulseAudio were verified, startup GL0. All planned work and verification for this bounded runtime-editor pass is complete.
