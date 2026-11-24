# Included by the main Makefile. These are engine modules, not a replacement
# engine or a separate web demo. Explicit CPU checks do not touch user saves.
PRODUCTION_MEMORY_SRCS = engine/world_npc_memory.cpp
CPPSRCS += $(PRODUCTION_MEMORY_SRCS)
$(TARGET): $(PRODUCTION_MEMORY_SRCS:.cpp=.o)
PRODUCTION_VALIDATOR_MEMORY_SRCS = engine/world_npc_memory.cpp
PRODUCTION_CPPSRCS = engine/world_project_checkpoint.cpp engine/world_editor_picking.cpp engine/world_designer_scene_index.cpp engine/world_designer.cpp engine/world_designer_assets.cpp engine/world_designer_workspace.cpp engine/world_runtime_components.cpp engine/world_runtime_demo.cpp engine/world_runtime_animation.cpp engine/world_model_cpu.cpp engine/world_surface_images.cpp engine/world_surface_import.cpp engine/world_surface_field.cpp engine/world_entity_graph.cpp engine/world_prefab.cpp engine/world_authoring_identity.cpp engine/world_editor_session.cpp engine/world_production_jobs.cpp engine/world_living.cpp engine/world_micro_policy.cpp engine/world_terrain_authoring.cpp engine/world_cinematics.cpp engine/world_regions.cpp engine/world_cinematics_audio.cpp engine/world_gameplay.cpp engine/world_physics.cpp engine/world_animation.cpp engine/world_animation_render.cpp
CPPSRCS += $(PRODUCTION_CPPSRCS)
PRODUCTION_OBJS = $(PRODUCTION_CPPSRCS:.cpp=.o)
$(TARGET): $(PRODUCTION_OBJS)
$(PRODUCTION_OBJS): Makefile engine/production.mk
PRODUCTION_VALIDATOR_SRCS = engine/world_runtime_components.cpp engine/world_runtime_demo.cpp engine/world_animation.cpp tools/validate_world_authoring.cpp engine/world_prefab.cpp engine/world_entity_graph.cpp engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_shader_graph.cpp engine/world_living.cpp engine/world_micro_policy.cpp engine/world_cinematics.cpp engine/world_regions.cpp engine/world_gameplay.cpp $(PRODUCTION_VALIDATOR_MEMORY_SRCS)
build/physics/libJolt.a: third_party/jolt/VALWOOD_SOURCE.json tools/build_world_physics.py
	$(PYTHON) tools/build_world_physics.py --jobs 2
engine/world_physics.o: CXXFLAGS += -DJPH_DOUBLE_PRECISION -DNDEBUG -fno-rtti -msse2 -mfpmath=sse -ffp-contract=off -Ithird_party/jolt
engine/world_physics.o: build/physics/libJolt.a
LIBS += build/physics/libJolt.a
tools/valwood_project_validate: $(PRODUCTION_VALIDATOR_SRCS) $(wildcard engine/world*.h) engine/world_physics.o build/physics/libJolt.a engine/production.mk
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(PRODUCTION_VALIDATOR_SRCS) engine/world_physics.o build/physics/libJolt.a -pthread -o $@
$(TARGET): tools/valwood_project_validate

PRODUCTION_CPU_FLAGS = -std=c++17 -O2 -Wall -Wextra -Werror -I.
PRODUCTION_DOCUMENT_SRCS = engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_shader_graph.cpp engine/world_entity_graph.cpp

test-world-production-identities:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_entity_graph_test.cpp $(PRODUCTION_DOCUMENT_SRCS) -o /tmp/valwood-production-entity-test
	/tmp/valwood-production-entity-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_prefab_test.cpp engine/world_prefab.cpp $(PRODUCTION_DOCUMENT_SRCS) -o /tmp/valwood-production-prefab-test
	/tmp/valwood-production-prefab-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_authoring_identity_test.cpp engine/world_authoring_identity.cpp engine/world_prefab.cpp $(PRODUCTION_DOCUMENT_SRCS) -o /tmp/valwood-production-identity-test
	/tmp/valwood-production-identity-test

test-world-production-recovery:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_editor_session_test.cpp engine/world_editor_session.cpp $(PRODUCTION_DOCUMENT_SRCS) -pthread -o /tmp/valwood-production-session-test
	/tmp/valwood-production-session-test

test-world-production-terrain:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_terrain_authoring_test.cpp engine/world_terrain_authoring.cpp $(PRODUCTION_DOCUMENT_SRCS) -o /tmp/valwood-production-terrain-test
	/tmp/valwood-production-terrain-test

test-world-production-living:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_living_test.cpp engine/world_living.cpp -o /tmp/valwood-production-living-test
	/tmp/valwood-production-living-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_micro_policy_test.cpp engine/world_micro_policy.cpp engine/world_living.cpp -o /tmp/valwood-production-micro-policy-test
	/tmp/valwood-production-micro-policy-test projects/valwood_world/ai/economic_policy.vwml

test-world-production-cinematics:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_cinematics_test.cpp engine/world_cinematics.cpp -o /tmp/valwood-production-cinematic-test
	/tmp/valwood-production-cinematic-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_regions_test.cpp engine/world_regions.cpp -pthread -o /tmp/valwood-production-region-test
	/tmp/valwood-production-region-test

test-world-production-build:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_production_jobs_test.cpp engine/world_production_jobs.cpp -o /tmp/valwood-production-jobs-test
	/tmp/valwood-production-jobs-test
	$(PYTHON) tests/world_asset_database_test.py
	$(PYTHON) tests/world_package_test.py
	$(PYTHON) tests/world_performance_gate_test.py

test-world-production-runtime: build/physics/libJolt.a
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_gameplay_test.cpp engine/world_gameplay.cpp -o /tmp/valwood-production-gameplay-test
	/tmp/valwood-production-gameplay-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_animation_test.cpp engine/world_animation.cpp -o /tmp/valwood-production-animation-test
	/tmp/valwood-production-animation-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) -DJPH_DOUBLE_PRECISION -DNDEBUG -fno-rtti -msse2 -mfpmath=sse -ffp-contract=off -Ithird_party/jolt tests/world_physics_test.cpp engine/world_physics.cpp build/physics/libJolt.a -pthread -o /tmp/valwood-production-physics-test
	/tmp/valwood-production-physics-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) $(SDL_CFLAGS) tests/world_cinematics_audio_test.cpp engine/world_cinematics_audio.cpp engine/world_cinematics.cpp $(SDL_LIBS) -pthread -o /tmp/valwood-production-audio-test
	SDL_AUDIODRIVER=dummy /tmp/valwood-production-audio-test

PRODUCTION_IMGUI_OBJS = third_party/imgui/imgui.o third_party/imgui/imgui_draw.o third_party/imgui/imgui_tables.o third_party/imgui/imgui_widgets.o
test-world-production-inspectors: $(PRODUCTION_IMGUI_OBJS) engine/world_physics.o build/physics/libJolt.a
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_animation_studio_test.cpp engine/world_animation.cpp $(PRODUCTION_IMGUI_OBJS) -o /tmp/valwood-production-animation-studio-test
	/tmp/valwood-production-animation-studio-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_physics_studio_test.cpp engine/world_physics.o $(PRODUCTION_IMGUI_OBJS) build/physics/libJolt.a -pthread -o /tmp/valwood-production-physics-studio-test
	/tmp/valwood-production-physics-studio-test

test-world-production: test-world-production-identities test-world-production-recovery test-world-production-terrain test-world-production-living test-world-production-cinematics test-world-production-build test-world-production-runtime
.PHONY: test-world-production test-world-production-identities test-world-production-recovery test-world-production-terrain test-world-production-living test-world-production-cinematics test-world-production-build
.PHONY: test-world-production-runtime
.PHONY: test-world-production-inspectors

test-world-npc-memory:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_npc_memory_test.cpp engine/world_npc_memory.cpp -o /tmp/valwood-npc-memory-test
	node tests/npc_memory_editor_test.mjs /tmp/valwood-npc-memory-test

.PHONY: test-world-npc-memory

# Connected live systems: meaningful CPU contracts, no GPU created by this target.
test-world-live-components: engine/world_physics.o build/physics/libJolt.a
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_runtime_components_test.cpp engine/world_runtime_components.cpp engine/world_npc_memory.cpp engine/world_living.cpp engine/world_gameplay.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-live-components-test
	/tmp/valwood-live-components-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_runtime_demo_test.cpp engine/world_runtime_demo.cpp engine/world_runtime_components.cpp engine/world_npc_memory.cpp engine/world_entity_graph.cpp engine/world_living.cpp engine/world_gameplay.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-live-demo-test
	/tmp/valwood-live-demo-test

test-world-live-animation:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_runtime_animation_test.cpp engine/world_runtime_animation.cpp engine/world_animation.cpp -o /tmp/valwood-live-animation-test
	/tmp/valwood-live-animation-test

test-world-model-cpu: engine/texture_cache.o
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_model_cpu_test.cpp engine/world_model_cpu.cpp engine/texture_cache.o -o /tmp/valwood-live-model-cpu-test
	/tmp/valwood-live-model-cpu-test

.PHONY: test-world-live-components test-world-live-animation test-world-model-cpu

# Designer commands and browser contracts run without a graphics context.
DESIGNER_DOCUMENT_SRCS = engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_shader_graph.cpp engine/world_entity_graph.cpp engine/world_authoring_identity.cpp engine/world_prefab.cpp engine/world_studio_tools.cpp
test-world-designer: $(PRODUCTION_IMGUI_OBJS)
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_designer_test.cpp engine/world_designer.cpp $(DESIGNER_DOCUMENT_SRCS) -o /tmp/valwood-designer-test
	/tmp/valwood-designer-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_designer_clipboard_test.cpp engine/world_designer.cpp $(DESIGNER_DOCUMENT_SRCS) -o /tmp/valwood-designer-clipboard-test
	/tmp/valwood-designer-clipboard-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_designer_terrain_snap_test.cpp engine/world_designer.cpp $(DESIGNER_DOCUMENT_SRCS) -o /tmp/valwood-designer-terrain-snap-test
	/tmp/valwood-designer-terrain-snap-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) -Ithird_party/imgui tests/world_designer_assets_test.cpp engine/world_designer_assets.cpp $(PRODUCTION_IMGUI_OBJS) -o /tmp/valwood-designer-assets-test
	/tmp/valwood-designer-assets-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) -pthread tests/world_designer_workspace_test.cpp engine/world_designer_workspace.cpp engine/world_designer_assets.cpp -o /tmp/valwood-designer-workspace-test
	/tmp/valwood-designer-workspace-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_designer_viewport_test.cpp -o /tmp/valwood-designer-viewport-test
	/tmp/valwood-designer-viewport-test
.PHONY: test-world-designer

# Cached editor hot paths and conservative viewport selection.
test-world-editor-optimization:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_designer_scene_index_test.cpp engine/world_designer_scene_index.cpp engine/world_entity_graph.cpp -o /tmp/valwood-designer-scene-index-test
	/tmp/valwood-designer-scene-index-test
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_editor_picking_test.cpp engine/world_editor_picking.cpp engine/world_studio_tools.cpp $(PRODUCTION_DOCUMENT_SRCS) -o /tmp/valwood-editor-picking-test
	/tmp/valwood-editor-picking-test
.PHONY: test-world-editor-optimization

# Integrated demo arc, authored recall, committed experience and persistent resume.
test-world-demo-journey: engine/world_physics.o build/physics/libJolt.a
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_demo_journey_test.cpp engine/world_runtime_demo.cpp engine/world_runtime_components.cpp engine/world_npc_memory.cpp engine/world_entity_graph.cpp engine/world_living.cpp engine/world_gameplay.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-demo-journey-test
	/tmp/valwood-demo-journey-test
.PHONY: test-world-demo-journey

test-world-project-checkpoint:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_project_checkpoint_test.cpp engine/world_project_checkpoint.cpp -o /tmp/valwood-project-checkpoint-test
	/tmp/valwood-project-checkpoint-test
.PHONY: test-world-project-checkpoint

test-world-full-transform:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_full_transform_test.cpp engine/world_designer.cpp $(DESIGNER_DOCUMENT_SRCS) -o /tmp/valwood-full-transform-test
	/tmp/valwood-full-transform-test
.PHONY: test-world-full-transform

test-world-full-transform-gl:
	$(CXX) $(PRODUCTION_CPU_FLAGS) $(SDL_CFLAGS) tests/world_full_transform_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/texture_cache.o -o /tmp/valwood-full-transform-gl-test $(SDL_LIBS) -lGL -Wl,--wrap=glUseProgram
	/tmp/valwood-full-transform-gl-test
.PHONY: test-world-full-transform-gl

# SDL device lifecycle and actual Dear ImGui controller focus regression.
test-world-controller: third_party/imgui/imgui.o third_party/imgui/imgui_draw.o third_party/imgui/imgui_tables.o third_party/imgui/imgui_widgets.o
	bash tools/check_controller_unit.sh
.PHONY: test-world-controller
test: test-world-controller

.PHONY: test-world-town
test-world-town:
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_town_test.cpp engine/world_training_combat.cpp -o /tmp/valwood-town-test
	/tmp/valwood-town-test
