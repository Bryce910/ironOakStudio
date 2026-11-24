# Connected surface authoring CPU contracts; native graphics are explicit.
SURFACE_CPU_FLAGS = -std=c++17 -O2 -Wall -Wextra -Werror -I.
test-world-surfaces:
	$(CXX) $(SURFACE_CPU_FLAGS) tests/world_surface_authoring_test.cpp engine/world_surface_authoring.cpp -o /tmp/valwood-surface-authoring-test
	/tmp/valwood-surface-authoring-test
	$(CXX) $(SURFACE_CPU_FLAGS) tests/world_surface_images_test.cpp engine/world_surface_images.cpp -o /tmp/valwood-surface-images-test
	/tmp/valwood-surface-images-test
	$(CXX) $(SURFACE_CPU_FLAGS) tests/world_surface_field_test.cpp engine/world_surface_field.cpp engine/world_surface_authoring.cpp -o /tmp/valwood-surface-field-test
	/tmp/valwood-surface-field-test
	$(CXX) $(SURFACE_CPU_FLAGS) tests/world_surface_import_test.cpp engine/world_surface_import.cpp -o /tmp/valwood-surface-import-test
	/tmp/valwood-surface-import-test
.PHONY: test-world-surfaces

surface-gl-contract:
	$(CXX) $(CXXFLAGS) -Wno-unused-function tests/world_surface_gl_test.cpp engine/world_surface_authoring.cpp engine/world_surface_images.cpp engine/world_shader_graph.cpp -o /tmp/valwood-surface-gl-test $(LIBS)
.PHONY: surface-gl-contract
