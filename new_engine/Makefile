CC         ?= gcc
CXX        ?= g++
PKG_CONFIG ?= pkg-config

TARGET = rpg
SRCS   = main.c ufbx.c engine/audio.c engine/save.c engine/pause_menu.c engine/frame_runtime.c engine/input_runtime.c engine/combat.c engine/player_world.c engine/render_runtime.c engine/effects.c engine/editor_ui.c engine/asset_system.c engine/app_bootstrap.c engine/editor_runtime_state.c engine/editor_selection.c engine/project_runtime.c engine/performance.c engine/thumbnail_runtime.c engine/grass_scatter.c engine/texture_cache.c engine/world_atlas.c engine/world_tiles.c engine/world_walk.c
CPPSRCS = engine/world_combat_vfx.cpp engine/world_combat_vfx_render.cpp engine/world_navigation.cpp engine/world_environment.cpp engine/world_studio_tools.cpp engine/world_assets.cpp engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_shader_graph.cpp engine/world_castle_dragons.cpp engine/world_dragon_breath.cpp engine/world_combat_motion.cpp engine/world_training_combat.cpp engine/world_local_probes.cpp engine/world_demo_audio.cpp engine/world_scene_particles.cpp engine/world_bandit_camp.cpp engine/world_trail_camp.cpp engine/world_graphics_settings.cpp engine/world_wildlife.cpp engine/world_village_watch.cpp engine/world_torch_flames.cpp engine/world_castle_training.cpp engine/world_village_population.cpp engine/world_village_pets.cpp engine/world_viewer.cpp engine/world_sky.cpp engine/world_occlusion.cpp engine/world_bloom.cpp engine/world_weather.cpp engine/world_models.cpp engine/world_cover.cpp engine/world_temporal.cpp engine/world_scenery.cpp engine/world_water_scene.cpp engine/editor_ui_imgui.cpp third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_sdl2.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp
CPPSRCS += engine/world_arena_render.cpp engine/world_stronghold.cpp engine/world_snow.cpp
CPPSRCS += engine/world_blood_effects.cpp engine/world_blood_render.cpp
OBJS   = $(SRCS:.c=.o) $(CPPSRCS:.cpp=.o)
DEPS   = ufbx.h cgltf.h gl_helpers.h stb_image.h $(wildcard engine/*.h engine/*.inc) projects/valwood_world/models/castle_dragons/dragon_metadata.inc

PKGS = sdl2 SDL2_mixer SDL2_ttf

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS) 2>/dev/null)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null)

CFLAGS ?= -std=c99 -O2 -Wall -Wextra
CFLAGS += -I. $(SDL_CFLAGS)
CPPFLAGS += -I. -Ithird_party/imgui -Ithird_party/imgui/backends $(SDL_CFLAGS)
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CXXFLAGS += $(CPPFLAGS)
ENGINE_ENABLE_IMGUI ?= 1
CFLAGS += -DENGINE_ENABLE_IMGUI=$(ENGINE_ENABLE_IMGUI)
CXXFLAGS += -DENGINE_ENABLE_IMGUI=$(ENGINE_ENABLE_IMGUI)
LIBS   := $(SDL_LIBS) -lGL -lm -lstdc++ -pthread

PKG_OK := $(shell $(PKG_CONFIG) --exists $(PKGS) && echo yes || echo no)

ifeq ($(PKG_OK),no)
$(warning Missing build dependencies. Run `make deps-check` for details.)
endif

all: $(TARGET)

test-world-trail-camp:
	$(CXX) $(CXXFLAGS) tests/world_trail_camp_test.cpp engine/world_trail_camp.cpp -o /tmp/valwood-trail-camp-test
	/tmp/valwood-trail-camp-test

.PHONY: test-world-trail-camp
test: test-world-trail-camp

test-world-seated-uv:
	python3 tests/village_seated_uv_test.py

.PHONY: test-world-seated-uv
test: test-world-seated-uv

engine/world_starting_layout.inc: projects/valwood_world/scenery/starting_area_layout.json projects/valwood_world/scenery/starting_area_manifest.json tools/build_starting_terrain.py
	python3 tools/build_starting_terrain.py

deps-check:
	@echo "Checking build dependencies..."
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { \
		echo "Missing: $(PKG_CONFIG)"; \
		echo "Install pkg-config first."; \
		exit 1; \
	}
	@for pkg in $(PKGS); do \
		if $(PKG_CONFIG) --exists $$pkg; then \
			echo "OK: $$pkg"; \
		else \
			echo "Missing: $$pkg"; \
			missing=1; \
		fi; \
	done; \
	if [ "$$missing" = "1" ]; then \
		echo ""; \
		echo "Ubuntu/Debian: make deps-ubuntu"; \
		echo "Fedora:        make deps-fedora"; \
		echo "Arch:          install sdl2 sdl2_mixer sdl2_ttf pkgconf"; \
		exit 1; \
	fi

$(TARGET): $(OBJS) Makefile
	@$(MAKE) --no-print-directory deps-check
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

$(OBJS): Makefile

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

deps-ubuntu:
	sudo apt-get update
	sudo apt-get install -y build-essential pkg-config libsdl2-dev libsdl2-mixer-dev libsdl2-ttf-dev libgl1-mesa-dev

deps-fedora:
	sudo dnf install -y gcc make pkgconf-pkg-config SDL2-devel SDL2_mixer-devel SDL2_ttf-devel mesa-libGL-devel

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	./$(TARGET) character.glb

.PHONY: all deps-check deps-ubuntu deps-fedora clean run

PYTHON ?= python3

seldonne: $(TARGET)
	$(PYTHON) tools/build_seldonne.py
	./run-valwood.sh

studio: $(TARGET)
	./run-valwood.sh --editor

test-project:
	$(CC) -std=c99 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/project_runtime_test.c engine/project_runtime.c -o /tmp/valwood-project-runtime-test $(LIBS)
	/tmp/valwood-project-runtime-test

test-audio:
	$(CC) -std=c99 -O1 -g -fsanitize=address -I. $(SDL_CFLAGS) tests/audio_shutdown_test.c engine/audio.c -o /tmp/valwood-audio-test $(LIBS)
	ASAN_OPTIONS=detect_leaks=0 /tmp/valwood-audio-test

tests/studio_integration.o: tests/studio_integration.c main.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

test-studio: tests/studio_integration.o $(filter-out main.o,$(OBJS))
	$(CXX) $^ -o /tmp/valwood-studio-integration $(LIBS)
	/tmp/valwood-studio-integration

test-grass:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/grass_scatter_test.c engine/grass_scatter.c -lm -o /tmp/valwood-grass-test
	/tmp/valwood-grass-test

test-performance:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/performance_test.c engine/performance.c -lm -o /tmp/valwood-performance-test
	/tmp/valwood-performance-test

test: test-project test-audio test-studio test-grass test-performance

.PHONY: seldonne studio test test-project test-audio test-studio test-grass test-performance

test-world-temporal:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_temporal_math_test.cpp engine/world_temporal.cpp -o /tmp/valwood-world-temporal-math-test $(LIBS)
	/tmp/valwood-world-temporal-math-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_temporal_gl_test.cpp engine/world_temporal.cpp -o /tmp/valwood-world-temporal-gl-test $(LIBS)
	/tmp/valwood-world-temporal-gl-test $(CURDIR)

test: test-world-temporal
.PHONY: test-world-temporal

test-world-scenery-cpu: engine/world_atlas.o engine/texture_cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_scenery_test.cpp engine/world_scenery.cpp engine/world_regions.cpp engine/world_atlas.o engine/texture_cache.o -o /tmp/valwood-world-scenery-test $(LIBS)
	/tmp/valwood-world-scenery-test

test-world-scenery: test-world-scenery-cpu
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_scenery_gl_test.cpp engine/world_scenery.cpp engine/world_regions.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/world_atlas.o engine/texture_cache.o -o /tmp/valwood-world-scenery-gl-test $(LIBS)
	/tmp/valwood-world-scenery-gl-test $(CURDIR)

test: test-world-scenery
.PHONY: test-world-scenery test-world-scenery-cpu

# Resource ownership and shader equivalence use offscreen GL, not performance measurements.
test-texture-cache:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -DTEXTURE_CACHE_TESTING -I. tests/texture_cache_test.c engine/texture_cache.c -o /tmp/valwood-texture-cache-test
	/tmp/valwood-texture-cache-test

test-texture-gl:
	$(CC) -std=c99 -O2 -Wall -Wextra -I. $(SDL_CFLAGS) -DTEXTURE_CACHE_GL_OTHER_TU -c tests/texture_cache_gl_test.c -o /tmp/valwood-texture-cache-gl-other.o
	$(CC) -std=c99 -O2 -Wall -Wextra -I. $(SDL_CFLAGS) tests/texture_cache_gl_test.c /tmp/valwood-texture-cache-gl-other.o engine/texture_cache.c -o /tmp/valwood-texture-cache-gl-test $(LIBS)
	/tmp/valwood-texture-cache-gl-test

test-shaders:
	$(CC) -std=c99 -O2 -Wall -Wextra -I. $(SDL_CFLAGS) tests/shader_equivalence.c -o /tmp/valwood-shader-equivalence $(LIBS)
	/tmp/valwood-shader-equivalence

test: test-texture-cache test-texture-gl test-shaders
.PHONY: test-texture-cache test-texture-gl test-shaders

# World geography and bounded streaming are CPU-only correctness checks.
test-world-atlas:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_atlas_test.c engine/world_atlas.c -lm -o /tmp/valwood-world-atlas-test
	/tmp/valwood-world-atlas-test

test-world-height-region:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_height_region_test.c engine/world_atlas.c -lm -o /tmp/valwood-world-height-region-test
	/tmp/valwood-world-height-region-test

.PHONY: test-world-height-region
test: test-world-height-region

test-world-model-baked-light:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_model_baked_light_test.cpp -o /tmp/valwood-world-model-baked-light-test
	/tmp/valwood-world-model-baked-light-test

.PHONY: test-world-model-baked-light
test: test-world-model-baked-light

test-world-model-source-paint:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_model_source_paint_test.cpp -o /tmp/valwood-world-model-source-paint-test
	/tmp/valwood-world-model-source-paint-test

.PHONY: test-world-model-source-paint
test: test-world-model-source-paint

test-world-tiles:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_tiles_test.c engine/world_tiles.c -lm -pthread -o /tmp/valwood-world-tiles-test
	/tmp/valwood-world-tiles-test

test: test-world-atlas test-world-tiles

test-world-rainforest:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_rainforest_test.c engine/world_atlas.c -lm -o /tmp/valwood-world-rainforest-test
	/tmp/valwood-world-rainforest-test

test: test-world-rainforest
.PHONY: test-world-rainforest

test-starting-terrain:
	python3 tools/build_starting_terrain.py --check
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_starting_terrain_test.c engine/world_atlas.c -lm -o /tmp/valwood-world-starting-terrain-test
	/tmp/valwood-world-starting-terrain-test

test: test-starting-terrain
.PHONY: test-starting-terrain

test-world-camera:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections $(CPPFLAGS) tests/world_camera_test.cpp engine/world_walk.c -Wl,--gc-sections -o /tmp/valwood-world-camera-test
	/tmp/valwood-world-camera-test

test: test-world-camera
.PHONY: test-world-camera
.PHONY: test-world-atlas test-world-tiles

test-world-cover:
	$(CC) -std=c99 -O2 -I. -c engine/world_atlas.c -o /tmp/valwood-cover-atlas.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_cover_test.cpp engine/world_cover.cpp /tmp/valwood-cover-atlas.o -lm -o /tmp/valwood-world-cover-test
	/tmp/valwood-world-cover-test

test: test-world-cover
.PHONY: test-world-cover

test-world-walk:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_walk_test.c engine/world_walk.c -lm -o /tmp/valwood-world-walk-test
	/tmp/valwood-world-walk-test

test: test-world-walk
.PHONY: test-world-walk

test-world-models:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. -c engine/texture_cache.c -o /tmp/valwood-world-models-cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) -DWORLD_MODELS_TEST_WRAP_UNIFORM_LOCATION tests/world_models_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp /tmp/valwood-world-models-cache.o -Wl,--wrap=glGetUniformLocation -o /tmp/valwood-world-models-gl-test $(LIBS)
	/tmp/valwood-world-models-gl-test

test: test-world-models
.PHONY: test-world-models

test-world-pose-blend:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. -c engine/texture_cache.c -o /tmp/valwood-world-pose-blend-cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_pose_blend_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp /tmp/valwood-world-pose-blend-cache.o -Wl,--wrap=glUseProgram -o /tmp/valwood-world-pose-blend-gl-test $(LIBS)
	/tmp/valwood-world-pose-blend-gl-test $(CURDIR)

test: test-world-pose-blend
.PHONY: test-world-pose-blend

test-world-distance-motion: engine/texture_cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_distance_motion_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/world_temporal.cpp engine/texture_cache.o -o /tmp/valwood-world-distance-motion-gl-test $(LIBS)
	/tmp/valwood-world-distance-motion-gl-test $(CURDIR) /tmp/valwood-distance-motion-results.json

test: test-world-distance-motion
.PHONY: test-world-distance-motion

test-world-lod:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_atlas_lod_test.c engine/world_atlas.c engine/world_tiles.c -lm -pthread -o /tmp/valwood-world-atlas-lod-test
	/tmp/valwood-world-atlas-lod-test

test: test-world-lod
.PHONY: test-world-lod

# Copy ownership and actual water/vegetation/lighting shader regressions.
test-world-water:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_water_gl_test.cpp engine/world_water_scene.cpp -o /tmp/valwood-world-water-gl-test $(LIBS)
	/tmp/valwood-world-water-gl-test $(CURDIR)

test-world-foliage:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_foliage_gl_test.cpp -o /tmp/valwood-world-foliage-gl-test $(LIBS)
	/tmp/valwood-world-foliage-gl-test $(CURDIR)

test: test-world-water test-world-foliage
.PHONY: test-world-water test-world-foliage

test-world-pool-reflection-scissor:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_pool_reflection_scissor_test.cpp -o /tmp/valwood-pool-reflection-scissor-test
	/tmp/valwood-pool-reflection-scissor-test

test-world-pool-reflection-scissor-gl:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_pool_reflection_scissor_gl_test.cpp -o /tmp/valwood-pool-reflection-scissor-gl-test $(LIBS)
	/tmp/valwood-pool-reflection-scissor-gl-test $(CURDIR)

.PHONY: test-world-pool-reflection-scissor test-world-pool-reflection-scissor-gl

test-world-pool-reflection-visibility:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_pool_reflection_visibility_test.cpp -o /tmp/valwood-pool-reflection-visibility-test
	/tmp/valwood-pool-reflection-visibility-test
	python3 tests/world_pool_reflection_visibility_source_test.py

test: test-world-pool-reflection-visibility
.PHONY: test-world-pool-reflection-visibility

test-world-adaptive-queries:
	python3 tools/test_world_adaptive_queries.py

test: test-world-adaptive-queries
.PHONY: test-world-adaptive-queries

test-world-zero-coverage:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_zero_coverage_gl_test.cpp -o /tmp/valwood-zero-coverage-gl-test $(LIBS)
	/tmp/valwood-zero-coverage-gl-test $(CURDIR)

test-world-horizon-grid:
	python3 tools/test_world_horizon_grid.py

.PHONY: test-world-zero-coverage test-world-horizon-grid

test-world-zero-contribution:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_zero_contribution_gl_test.cpp -o /tmp/valwood-zero-contribution-gl-test $(LIBS)
	/tmp/valwood-zero-contribution-gl-test $(CURDIR)

.PHONY: test-world-zero-contribution

test-world-dressing-selection:
	python3 tools/test_world_dressing_selection.py

.PHONY: test-world-dressing-selection

test: test-world-horizon-grid test-world-dressing-selection test-world-zero-coverage test-world-zero-contribution test-world-pool-reflection-scissor test-world-pool-reflection-scissor-gl

test-world-leaf-shadow:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_leaf_shadow_gl_test.cpp -o /tmp/valwood-leaf-shadow-gl-test $(LIBS)
	/tmp/valwood-leaf-shadow-gl-test $(CURDIR)

test-world-shadow-gather:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_shadow_gather_gl_test.cpp -o /tmp/valwood-shadow-gather-gl-test $(LIBS)
	/tmp/valwood-shadow-gather-gl-test $(CURDIR)

test-world-grass-transition-differential:
	python3 tests/run_world_grass_transition_differential.py

test: test-world-leaf-shadow test-world-shadow-gather test-world-grass-transition-differential
.PHONY: test-world-leaf-shadow test-world-shadow-gather test-world-grass-transition-differential

test-world-lighting:
	python3 tests/world_village_ground_contract_test.py
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_lighting_gl_test.cpp -o /tmp/valwood-world-lighting-gl-test $(LIBS)
	/tmp/valwood-world-lighting-gl-test $(CURDIR)

test: test-world-lighting
.PHONY: test-world-lighting

test-world-post:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_post_gl_test.cpp -o /tmp/valwood-world-post-gl-test $(LIBS)
	/tmp/valwood-world-post-gl-test $(CURDIR)

test: test-world-post
.PHONY: test-world-post

test-world-portrait:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_portrait_gl_test.cpp -o /tmp/valwood-world-portrait-gl-test $(LIBS)
	/tmp/valwood-world-portrait-gl-test

test: test-world-portrait
.PHONY: test-world-portrait

test-world-sky:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -I. $(SDL_CFLAGS) tests/world_sky_gl_test.cpp engine/world_sky.cpp -o /tmp/valwood-world-sky-gl-test $(LIBS)
	/tmp/valwood-world-sky-gl-test $(CURDIR)

test-world-occlusion:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_occlusion_gl_test.cpp engine/world_occlusion.cpp -o /tmp/valwood-world-occlusion-gl-test $(LIBS)
	/tmp/valwood-world-occlusion-gl-test $(CURDIR)

test: test-world-sky test-world-occlusion
.PHONY: test-world-sky test-world-occlusion


test-world-pool:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_pool_test.c engine/world_atlas.c -lm -o /tmp/valwood-world-pool-test
	/tmp/valwood-world-pool-test

test-world-weather:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_weather_gl_test.cpp engine/world_weather.cpp -o /tmp/valwood-world-weather-gl-test $(LIBS)
	/tmp/valwood-world-weather-gl-test .

test: test-world-pool test-world-weather
.PHONY: test-world-pool test-world-weather


test-world-shadow-filter:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_shadow_filter_gl_test.cpp -o /tmp/valwood-world-shadow-filter-gl-test $(LIBS)
	/tmp/valwood-world-shadow-filter-gl-test $(CURDIR)

test: test-world-shadow-filter
.PHONY: test-world-shadow-filter

# Valdoa castle village: compact terrain and shared road/ecology masks.
test-world-village:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_village_test.c engine/world_atlas.c -lm -o /tmp/valwood-world-village-test
	/tmp/valwood-world-village-test
	python3 tools/build_castle_village.py --validate-only --require-models
	python3 tools/build_village_props.py --validate
	python3 tools/test_village_dressing.py

test: test-world-village
.PHONY: test-world-village

test-world-village-ground:
	python3 tools/build_village_ground.py --validate
	python3 tools/test_village_ground.py

test: test-world-village-ground
.PHONY: test-world-village-ground

test-world-village-keep:
	python3 tools/test_village_keep_hero.py

test: test-world-village-keep
.PHONY: test-world-village-keep

test-castle-demo:
	python3 tests/castle_demo_acceptance_test.py
	python3 tools/test_village_demo_district.py
	python3 tools/test_village_demo_dressing.py
	python3 tools/build_village_demo_bunting.py --test
	python3 tools/build_village_demo_frontages.py --test
	python3 tools/build_village_market_lively.py --test
	python3 tools/build_village_market_hero.py --test
	python3 tools/build_village_hero_grove.py --test
	python3 tools/check_castle_demo.py --hash-models

test: test-castle-demo
.PHONY: test-castle-demo

test-world-village-population: engine/world_atlas.o engine/texture_cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_village_population_test.cpp engine/world_village_population.cpp engine/world_atlas.o -o /tmp/valwood-population-test $(LIBS)
	/tmp/valwood-population-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_village_population_gl_test.cpp engine/world_village_population.cpp engine/world_scenery.cpp engine/world_regions.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/world_atlas.o engine/texture_cache.o -o /tmp/valwood-world-village-population-gl-test $(LIBS)
	/tmp/valwood-world-village-population-gl-test $(CURDIR)

test: test-world-village-population
.PHONY: test-world-village-population

test-world-village-pets: engine/world_atlas.o engine/texture_cache.o
	python3 tools/build_village_pets.py --test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_village_pets_test.cpp engine/world_village_pets.cpp engine/world_atlas.o -o /tmp/valwood-pets-test $(LIBS)
	/tmp/valwood-pets-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_village_pets_crowd_test.cpp engine/world_village_pets.cpp engine/world_village_population.cpp engine/world_atlas.o -o /tmp/valwood-pets-crowd-test $(LIBS)
	/tmp/valwood-pets-crowd-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_village_pets_gl_test.cpp engine/world_village_pets.cpp engine/world_scenery.cpp engine/world_regions.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/world_atlas.o engine/texture_cache.o -o /tmp/valwood-world-village-pets-gl-test $(LIBS)
	/tmp/valwood-world-village-pets-gl-test $(CURDIR)

test: test-world-village-pets
.PHONY: test-world-village-pets

# Stable camera-relative shadows across the full atlas, without a GL context.
engine/world_viewer.o: engine/world_shadow_math.h
engine/world_viewer.o engine/world_village_population.o: engine/world_village_population.h

test-world-viewer-caches:
	python3 tools/test_world_viewer_caches.py

test: test-world-viewer-caches
.PHONY: test-world-viewer-caches

test-world-shadow-math:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_shadow_math_test.cpp -o /tmp/valwood-world-shadow-math-test
	/tmp/valwood-world-shadow-math-test

test: test-world-shadow-math
.PHONY: test-world-shadow-math

engine/world_viewer.o: engine/world_village_sky.h engine/world_village_indirect.h

test-world-village-sky:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_village_sky_test.cpp -o /tmp/valwood-world-village-sky-test
	/tmp/valwood-world-village-sky-test

test: test-world-village-sky
.PHONY: test-world-village-sky

engine/world_viewer.o: engine/world_grass_transition.h

test-world-grass-transition:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_grass_transition_test.cpp -o /tmp/valwood-world-grass-transition-test
	/tmp/valwood-world-grass-transition-test

test: test-world-grass-transition
.PHONY: test-world-grass-transition

# Real ecology publications must retain every outgoing root during a rebase.
test-world-grass-transition-cover:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. -c engine/world_atlas.c -o /tmp/valwood-grass-transition-cover-atlas.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_grass_transition_cover_test.cpp engine/world_cover.cpp /tmp/valwood-grass-transition-cover-atlas.o -lm -o /tmp/valwood-world-grass-transition-cover-test
	/tmp/valwood-world-grass-transition-cover-test

test: test-world-grass-transition-cover
.PHONY: test-world-grass-transition-cover

# Exact startup uploads and the GL3.3 terrain sampler budget.
test-world-village-textures:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_village_textures_gl_test.cpp -o /tmp/valwood-world-village-textures-gl-test $(LIBS)
	/tmp/valwood-world-village-textures-gl-test $(CURDIR)

test: test-world-village-textures
.PHONY: test-world-village-textures

# Castle training/creek expansion: actual posed meshes, measured supports and streamed bed.
test-castle-expansion: engine/world_atlas.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_castle_training_test.cpp engine/world_castle_training.cpp -o /tmp/valwood-castle-training-test $(LIBS)
	/tmp/valwood-castle-training-test
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_castle_creek_test.c -lm -o /tmp/valwood-castle-creek-test
	/tmp/valwood-castle-creek-test
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_castle_creek_atlas_test.c engine/world_atlas.c engine/world_tiles.c -lm -pthread -o /tmp/valwood-castle-creek-atlas-test
	/tmp/valwood-castle-creek-atlas-test
	python3 tools/build_castle_expansion.py --test
	python3 tools/build_castle_inner_wing.py --test
	python3 tools/build_castle_creek_banks.py --test
	python3 tools/build_castle_drawbridge.py --validate
	python3 tools/test_castle_creek.py
	python3 tools/check_castle_training.py --output docs/performance/castle-expansion/training-geometry-proof.json

.PHONY: test-castle-expansion
test: test-castle-expansion

# Keep grass hidden only by proven opaque source walls, with camera-cache guards.
test-castle-visibility:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_castle_visibility_test.cpp -o /tmp/valwood-castle-visibility-test
	/tmp/valwood-castle-visibility-test
	python3 tools/check_castle_visibility.py --output docs/performance/castle-expansion/wall-visibility-source-proof.json

.PHONY: test-castle-visibility
test: test-castle-visibility

# Real ImGui input/lifecycle with counted SDL/GL mocks; no desktop GPU needed.
test-editor-ui-passive:
	$(CC) -std=c99 -O1 -g -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I. -DENGINE_ENABLE_IMGUI=1 -c engine/editor_ui.c -o /tmp/valwood-passive-editor-ui.o
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I. -Ithird_party/imgui tests/editor_ui_passive_test.cpp engine/editor_ui_imgui.cpp third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp /tmp/valwood-passive-editor-ui.o -Wl,--gc-sections -o /tmp/valwood-editor-ui-passive-test
	/tmp/valwood-editor-ui-passive-test

.PHONY: test-editor-ui-passive
test: test-editor-ui-passive

.PHONY: test-world-torches
test-world-torches:
	python3 tests/world_castle_hall_water_test.py
	python3 tests/village_torches_test.py
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_torch_flames_cpu_test.cpp -o /tmp/valwood-torch-flames-cpu-test $(LIBS)
	/tmp/valwood-torch-flames-cpu-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_torch_flames_gl_test.cpp engine/world_torch_flames.cpp -o /tmp/valwood-torch-flames-gl-test $(LIBS)
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen /tmp/valwood-torch-flames-gl-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_torch_lights_test.cpp -o /tmp/valwood-torch-lights-test $(LIBS)
	/tmp/valwood-torch-lights-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_torch_lights_gl_test.cpp -o /tmp/valwood-torch-lights-gl-test $(LIBS)
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen /tmp/valwood-torch-lights-gl-test
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. -c engine/texture_cache.c -o /tmp/valwood-torch-models-cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_torch_models_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp /tmp/valwood-torch-models-cache.o -Wl,--wrap=glDrawElementsInstanced -o /tmp/valwood-torch-models-gl-test $(LIBS)
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen /tmp/valwood-torch-models-gl-test

.PHONY: test-world-farms
test-world-farms:
	python3 tests/village_farms_test.py
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_crop_scenery_test.cpp -o /tmp/valwood-crop-scenery-test
	/tmp/valwood-crop-scenery-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_crop_wind_gl_test.cpp -lSDL2 -lGL -o /tmp/valwood-crop-wind-gl-test
	env -u VALWOOD_TEST_NATIVE_GL /tmp/valwood-crop-wind-gl-test
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_farm_ground_test.c -lm -o /tmp/valwood-farm-ground-test
	/tmp/valwood-farm-ground-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_farm_ground_gl_test.cpp -lSDL2 -lGL -o /tmp/valwood-farm-ground-gl-test
	env -u VALWOOD_TEST_NATIVE_GL /tmp/valwood-farm-ground-gl-test

# Supplied asset guards use bounded adjacent-pose GPU interpolation.
test-world-village-watch:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_village_watch_test.cpp engine/world_village_watch.cpp -o /tmp/valwood-village-watch-test $(LIBS)
	/tmp/valwood-village-watch-test

.PHONY: test-world-village-watch
test: test-world-village-watch

# Bounded wildlife banks, deterministic source-pose animation and ownership.
test-world-wildlife:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_wildlife_test.cpp engine/world_wildlife.cpp -o /tmp/valwood-world-wildlife-test
	/tmp/valwood-world-wildlife-test
.PHONY: test-world-wildlife

test-world-wildlife-habitats:
	$(PYTHON) tests/wildlife_habitats_test.py
	$(PYTHON) tools/build_wildlife_habitats.py --validate-only
.PHONY: test-world-wildlife-habitats
test: test-world-wildlife test-world-wildlife-habitats


test-world-graphics-settings:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -pedantic -I. engine/world_graphics_settings.cpp tests/world_graphics_settings_test.cpp -o /tmp/valwood-world-graphics-settings-test
	/tmp/valwood-world-graphics-settings-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_quality_controller_test.cpp -o /tmp/valwood-quality-controller-test
	/tmp/valwood-quality-controller-test
.PHONY: test-world-graphics-settings
test: test-world-graphics-settings

test-world-bandit-camp:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_bandit_camp_test.cpp engine/world_bandit_camp.cpp -o /tmp/valwood-bandit-camp-test
	/tmp/valwood-bandit-camp-test
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_bandit_ground_test.c engine/world_atlas.c -lm -o /tmp/valwood-bandit-ground-test
	/tmp/valwood-bandit-ground-test
	$(CC) -std=c99 -O2 -I. -c engine/world_atlas.c -o /tmp/valwood-bandit-scene-atlas.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -I. $(SDL_CFLAGS) tests/world_bandit_camp_scene_test.cpp engine/world_scenery.cpp engine/world_regions.cpp engine/world_bandit_camp.cpp /tmp/valwood-bandit-scene-atlas.o -lm -o /tmp/valwood-bandit-scene-test
	/tmp/valwood-bandit-scene-test projects/valwood_world
.PHONY: test-world-bandit-camp
test: test-world-bandit-camp

# Real encoded source bounds and production scenery/compound colliders; CPU only.
test-world-castle-streets-scene:
	$(CC) -std=c99 -O2 -I. -c engine/world_atlas.c -o /tmp/valwood-castle-streets-atlas.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -I. $(SDL_CFLAGS) tests/world_castle_streets_scene_test.cpp engine/world_scenery.cpp engine/world_regions.cpp /tmp/valwood-castle-streets-atlas.o -lm -o /tmp/valwood-castle-streets-scene-test
	/tmp/valwood-castle-streets-scene-test projects/valwood_world
.PHONY: test-world-castle-streets-scene
test: test-world-castle-streets-scene

.PHONY: test-world-scene-particles
test-world-scene-particles:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/test_world_scene_particles.cpp -o /tmp/valwood-scene-particles-test $(LIBS)
	$(if $(filter 1,$(VALWOOD_TEST_NATIVE_GL)),,env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1) /tmp/valwood-scene-particles-test --gl
	env $(if $(filter 1,$(VALWOOD_TEST_NATIVE_GL)),,SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1) VALWOOD_TEST_PURCHASED_PARTICLES=1 /tmp/valwood-scene-particles-test --gl

# Uniform-name ownership and program-generation invalidation; no GL context.
test-world-uniform-cache: engine/world_uniform_cache.h tests/world_uniform_cache_test.cpp
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_uniform_cache_test.cpp -o /tmp/valwood-uniform-cache-test
	/tmp/valwood-uniform-cache-test

# Requires the shipped PCM bank; tests never synthesize or replace audio assets.
test-world-demo-audio: projects/valwood_world/audio/demo/manifest.json
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. -c engine/world_atlas.c -o /tmp/valwood-demo-audio-atlas-test.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_demo_audio_test.cpp engine/world_demo_audio.cpp /tmp/valwood-demo-audio-atlas-test.o -o /tmp/valwood-demo-audio-test $(LIBS)
	env SDL_AUDIODRIVER=dummy /tmp/valwood-demo-audio-test

# Analytic cube/SH math, then actual isolated offscreen capture and binding.
test-world-local-probes-cpu:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_local_probes_math_test.cpp engine/world_local_probes.cpp -o /tmp/valwood-local-probes-math-test $(LIBS)
	/tmp/valwood-local-probes-math-test

test-world-local-probes-gl:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_local_probes_gl_test.cpp engine/world_local_probes.cpp -o /tmp/valwood-local-probes-gl-test $(LIBS)
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-local-probes-gl-test

test-world-local-probes: test-world-local-probes-cpu test-world-local-probes-gl

# GPU byte/content/geometry parity for exact and orphaned instance policies.
# Exclude actual driver entry calls from the caller-module allocation guard.
test-world-instance-stream: engine/texture_cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_instance_stream_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/texture_cache.o -Wl,--wrap=glBufferData -Wl,--wrap=glBufferSubData -Wl,--wrap=glDrawElementsInstanced -Wl,--wrap=glUseProgram -Wl,--wrap=glBindBuffer -Wl,--wrap=glGetError -o /tmp/valwood-instance-stream-gl-test $(LIBS)
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-instance-stream-gl-test

.PHONY: test-world-uniform-cache test-world-demo-audio test-world-local-probes-cpu test-world-local-probes-gl test-world-local-probes test-world-instance-stream
test: test-world-uniform-cache test-world-demo-audio test-world-local-probes test-world-instance-stream

# Actual material sampler completeness, unchanged production MRT pixels and ownership.
test-world-material-samplers: engine/texture_cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_material_samplers_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/texture_cache.o -Wl,--wrap=glDrawElementsInstanced -o /tmp/valwood-material-samplers-gl-test $(LIBS)
	$(if $(filter 1,$(VALWOOD_TEST_NATIVE_GL)),,env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1) /tmp/valwood-material-samplers-gl-test
.PHONY: test-world-material-samplers
test: test-world-material-samplers

# Precomputed opaque color blocks: CPU parser checks are independent of GL.
test-world-texture-blocks-format: engine/world_texture_blocks_format.h tests/world_texture_blocks_format_test.cpp
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -pthread -I. tests/world_texture_blocks_format_test.cpp -o /tmp/valwood-texture-block-format-test
	/tmp/valwood-texture-block-format-test

test-world-model-linear-blocks-cpu: engine/texture_cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -pthread -I. tests/world_model_linear_blocks_cpu_test.cpp engine/world_model_cpu.cpp engine/texture_cache.o -o /tmp/valwood-model-linear-blocks-cpu-test
	/tmp/valwood-model-linear-blocks-cpu-test
.PHONY: test-world-model-linear-blocks-cpu
test: test-world-model-linear-blocks-cpu

# Offline-only support library; it is never linked into rpg. Cooking is explicit.
/tmp/valwood-bc7enc16.o: third_party/bc7enc16/bc7enc16.c third_party/bc7enc16/bc7enc16.h
	$(CC) -std=c99 -O3 -Wall -Wextra -Werror -Wno-unused-const-variable -Ithird_party/bc7enc16 -c $< -o $@

/tmp/valwood-cook-texture-blocks: tools/cook_world_texture_blocks.cpp tools/world_texture_blocks_encoder.h engine/world_texture_blocks_format.h engine/world_model_facade.h /tmp/valwood-bc7enc16.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tools/cook_world_texture_blocks.cpp /tmp/valwood-bc7enc16.o -o $@ $(LIBS) -pthread

/tmp/valwood-model-linear-blocks-gl-test: tests/world_model_linear_blocks_gl_test.cpp tests/world_texture_blocks_fixture.h engine/world_models.cpp engine/world_model_cpu.cpp engine/world_model_texture_blocks.h engine/world_texture_blocks_format.h engine/texture_cache.o /tmp/valwood-bc7enc16.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_model_linear_blocks_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/texture_cache.o /tmp/valwood-bc7enc16.o -Wl,--wrap=glCompressedTexImage2D -o $@ $(LIBS) -pthread

test-world-model-linear-blocks-gl: /tmp/valwood-model-linear-blocks-gl-test
	$(if $(filter 1,$(VALWOOD_TEST_NATIVE_GL)),,env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1) /tmp/valwood-model-linear-blocks-gl-test
.PHONY: test-world-model-linear-blocks-gl
test: test-world-model-linear-blocks-gl

cook-world-texture-blocks: /tmp/valwood-cook-texture-blocks
	/tmp/valwood-cook-texture-blocks --models projects/valwood_world/models --output projects/valwood_world/texture_blocks

test-world-texture-blocks-gl: engine/texture_cache.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_texture_blocks_gl_test.cpp engine/world_models.cpp engine/world_model_cpu.cpp engine/texture_cache.o -Wl,--wrap=glCompressedTexImage2D -Wl,--wrap=glTexImage2D -o /tmp/valwood-texture-blocks-gl-test $(LIBS) -pthread
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-texture-blocks-gl-test

.PHONY: test-world-texture-blocks-format test-world-texture-blocks-gl cook-world-texture-blocks
test: test-world-texture-blocks-format test-world-texture-blocks-gl

test-world-rainforest-atmosphere:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_rainforest_atmosphere_test.cpp -o /tmp/valwood-rainforest-atmosphere-test
	/tmp/valwood-rainforest-atmosphere-test
.PHONY: test-world-rainforest-atmosphere
test: test-world-rainforest-atmosphere

test-world-selection-membership:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_selection_membership_test.cpp -o /tmp/valwood-selection-membership-test
	/tmp/valwood-selection-membership-test

test-world-terrain-visibility:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_terrain_visibility_test.cpp -o /tmp/valwood-terrain-visibility-test
	/tmp/valwood-terrain-visibility-test
.PHONY: test-world-selection-membership test-world-terrain-visibility
test: test-world-selection-membership test-world-terrain-visibility

test-world-shadow-endpoint:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_shadow_endpoint_gl_test.cpp -o /tmp/valwood-shadow-endpoint-gl-test $(LIBS)
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-shadow-endpoint-gl-test $(CURDIR)
.PHONY: test-world-shadow-endpoint

test-world-imgui-submission: third_party/imgui/imgui.o third_party/imgui/imgui_draw.o third_party/imgui/imgui_tables.o third_party/imgui/imgui_widgets.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_imgui_submission_test.cpp third_party/imgui/imgui.o third_party/imgui/imgui_draw.o third_party/imgui/imgui_tables.o third_party/imgui/imgui_widgets.o -o /tmp/valwood-imgui-submission-test
	/tmp/valwood-imgui-submission-test
.PHONY: test-world-imgui-submission

# Bounded terrain banks and startup canopy field: CPU-only geometry contracts.
test-world-demo-route-polish:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_demo_route_polish_test.cpp -o /tmp/valwood-demo-route-polish-test
	/tmp/valwood-demo-route-polish-test
.PHONY: test-world-demo-route-polish
test-world-forest-shelter-gl:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_forest_shelter_gl_test.cpp -o /tmp/valwood-forest-shelter-gl-test $(LIBS)
	/tmp/valwood-forest-shelter-gl-test $(CURDIR)
test: test-world-demo-route-polish test-world-forest-shelter-gl
.PHONY: test-world-forest-shelter-gl

test-world-tavern-lighting:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_tavern_lighting_gl_test.cpp -o /tmp/valwood-tavern-lighting-gl-test $(LIBS)
	/tmp/valwood-tavern-lighting-gl-test
test: test-world-tavern-lighting
.PHONY: test-world-tavern-lighting

# Sole compensation is baked from actual affine source-pose geometry.
engine/world_village_population.o: engine/world_village_contact_data.inc

test-world-village-contact:
	python3 tools/build_village_contact_curves.py --check

.PHONY: test-world-village-contact
test: test-world-village-contact

# Directional, non-lethal training simulation (no graphics dependencies).
test-world-training-combat:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_training_combat_test.cpp engine/world_training_combat.cpp -o /tmp/valwood-training-combat-test
	/tmp/valwood-training-combat-test
.PHONY: test-world-training-combat
test: test-world-training-combat

test-world-training-station:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_castle_training_test.cpp engine/world_castle_training.cpp -o /tmp/valwood-castle-training-test $(LIBS)
	/tmp/valwood-castle-training-test
.PHONY: test-world-training-station
test: test-world-training-station

# Pure camera sweep/damping checks, with no graphics context.
test-world-follow-camera:
	$(CXX) $(CXXFLAGS) tests/world_follow_camera_test.cpp -o /tmp/valwood-follow-camera-test
	/tmp/valwood-follow-camera-test

.PHONY: test-world-follow-camera

test-world-arena-camera:
	$(CXX) $(CXXFLAGS) tests/world_arena_camera_test.cpp engine/world_training_combat.cpp -o /tmp/valwood-arena-camera-test
	/tmp/valwood-arena-camera-test

.PHONY: test-world-arena-camera

test-world-combat-motion:
	$(CXX) $(CXXFLAGS) tests/world_combat_motion_test.cpp engine/world_combat_motion.cpp engine/world_training_combat.cpp -o /tmp/valwood-combat-motion-test
	/tmp/valwood-combat-motion-test

.PHONY: test-world-combat-motion

test-world-demo-landforms:
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -I. tests/world_demo_landforms_test.c -lm -o /tmp/valwood-demo-landforms-test
	/tmp/valwood-demo-landforms-test
.PHONY: test-world-demo-landforms

test-world-castle-dragons: engine/world_atlas.o
	$(CXX) $(CXXFLAGS) tests/world_castle_dragons_test.cpp engine/world_castle_dragons.cpp engine/world_atlas.o -o /tmp/valwood-castle-dragons-test $(LIBS)
	/tmp/valwood-castle-dragons-test

test-world-dragon-lights:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_dragon_lights_test.cpp -o /tmp/valwood-dragon-lights-test
	/tmp/valwood-dragon-lights-test

test-world-dragon-breath:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_dragon_breath_cpu_test.cpp -o /tmp/valwood-dragon-breath-cpu-test $(LIBS)
	/tmp/valwood-dragon-breath-cpu-test
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_dragon_breath_gl_test.cpp engine/world_dragon_breath.cpp -o /tmp/valwood-dragon-breath-gl-test $(LIBS)
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-dragon-breath-gl-test
	env -u VALWOOD_TEST_NATIVE_GL SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 VALWOOD_TEST_PURCHASED_PARTICLES=1 /tmp/valwood-dragon-breath-gl-test

.PHONY: test-world-castle-dragons test-world-dragon-lights test-world-dragon-breath

# Persistent streamed-world authoring (CPU-only contracts).
test-world-studio:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_studio_test.cpp engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_entity_graph.cpp engine/world_shader_graph.cpp -o /tmp/valwood-world-studio-test
	/tmp/valwood-world-studio-test
.PHONY: test-world-studio
test: test-world-studio

# Compiler-generated transitive dependencies avoid rebuilding unrelated systems
# for every editor include while still tracking nested engine/model headers.

# CPU authoring math and bounded model diagnostics/import contracts.
test-world-studio-tools:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_studio_tools_test.cpp engine/world_studio_tools.cpp engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_entity_graph.cpp engine/world_shader_graph.cpp -o /tmp/valwood-studio-tools-test
	/tmp/valwood-studio-tools-test

test-world-assets:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I. tests/world_assets_test.cpp engine/world_assets.cpp engine/world_studio_tools.cpp engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_entity_graph.cpp engine/world_shader_graph.cpp -o /tmp/valwood-world-assets-test
	/tmp/valwood-world-assets-test $(CURDIR)
	python3 tests/world_asset_cooker_test.py
.PHONY: test-world-studio-tools test-world-assets
test: test-world-studio-tools test-world-assets

test-world-environment:
	$(CXX) $(CXXFLAGS) tests/world_environment_gl_test.cpp engine/world_environment.cpp engine/world_local_probes.cpp -o /tmp/valwood-environment-gl-test $(LIBS)
	/tmp/valwood-environment-gl-test $(CURDIR)
.PHONY: test-world-environment
test: test-world-environment

# Runtime authoring document, navigation and material graph correctness.
test-world-runtime-editors:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_runtime_editors_test.cpp engine/world_navigation.cpp engine/world_shader_graph.cpp engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_entity_graph.cpp -o /tmp/valwood-runtime-editors-test
	/tmp/valwood-runtime-editors-test
.PHONY: test-world-runtime-editors
test: test-world-runtime-editors

test-world-shader-graph:
	$(CXX) $(CXXFLAGS) tests/world_shader_graph_gl_test.cpp engine/world_shader_graph.cpp -o /tmp/valwood-shader-graph-gl-test $(LIBS)
	/tmp/valwood-shader-graph-gl-test $(CURDIR)
.PHONY: test-world-shader-graph

include engine/production.mk

# CPU authoring contracts for the imported surface tools.
include engine/surfaces.mk

# Expand after every module appends CPPSRCS. Earlier expansion omitted production
# modules and could link stale layouts after a shared editor header changed.
-include $(OBJS:.o=.d)

.PHONY: test-world-vfx
test-world-vfx:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_vfx_test.cpp engine/world_editor_session.cpp engine/world_studio.cpp engine/world_surface_authoring.cpp engine/world_entity_graph.cpp engine/world_shader_graph.cpp -pthread -o /tmp/valwood-vfx-test
	/tmp/valwood-vfx-test

test: test-world-vfx

.PHONY: test-world-combat-vfx
test-world-combat-vfx:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_combat_vfx_test.cpp engine/world_combat_vfx.cpp engine/world_training_combat.cpp -o /tmp/valwood-combat-vfx-test
	/tmp/valwood-combat-vfx-test

test: test-world-combat-vfx

.PHONY: test-world-combat-vfx-gl
test-world-combat-vfx-gl:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_combat_vfx_gl_test.cpp engine/world_combat_vfx.cpp engine/world_combat_vfx_render.cpp engine/world_training_combat.cpp -o /tmp/valwood-combat-vfx-gl-test $(LIBS)
	$(if $(filter 1,$(VALWOOD_TEST_NATIVE_GL)),,env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1) /tmp/valwood-combat-vfx-gl-test

.PHONY: test-world-presentation
test-world-presentation: engine/world_physics.o build/physics/libJolt.a
	$(CXX) $(PRODUCTION_CPU_FLAGS) tests/world_presentation_test.cpp engine/world_training_combat.cpp engine/world_runtime_animation.cpp engine/world_animation.cpp engine/world_runtime_demo.cpp engine/world_runtime_components.cpp engine/world_npc_memory.cpp engine/world_entity_graph.cpp engine/world_living.cpp engine/world_gameplay.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-presentation-test
	/tmp/valwood-presentation-test

test: test-world-presentation

.PHONY: test-world-presentation-voice
test-world-presentation-voice:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror -I. $(SDL_CFLAGS) tests/world_presentation_voice_test.cpp -o /tmp/valwood-presentation-voice-test $(LIBS)
	SDL_AUDIODRIVER=dummy /tmp/valwood-presentation-voice-test
