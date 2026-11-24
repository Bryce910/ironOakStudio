#!/usr/bin/env python3
"""Actual pure visual admission behavior; no renderer, SDL, or GPU context.

The host adapter is included after Studio, so extract only its marked pure
admission boundary and compile that exact source with the real document types.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AdmissionTest(unittest.TestCase):
    def test_renderer_boundaries_preserve_authored_inputs(self):
        source = (ROOT / "engine/world_runtime_visuals.inc").read_text()
        begin = source.index("// Begin CPU authoring admission.")
        end = source.index("// End CPU authoring admission.", begin)
        helper = source[begin:end]
        fixture = r'''
#include "engine/world_studio.h"
#include "engine/world_runtime_demo.h"
#include <cassert>
#include <iostream>
HELPER
int main(){
    world_studio::Document scene;
    world_runtime::Document components;
    world_runtime_demo::Seed seed;
    std::vector<world_runtime::Entity> entities;
    std::string error="stale diagnostic";
    const auto add_seed=[&](world_runtime::Id id,bool crate){
        world_runtime_demo::Visual visual;visual.entity=id;
        visual.kind=crate?world_runtime_demo::VisualKind::Crate:world_runtime_demo::VisualKind::CastleKnight;
        seed.visuals.push_back(visual);world_runtime::Entity entity;entity.id=id;entities.push_back(entity);
    };
    add_seed(1000,true);add_seed(1001,true);
    for(unsigned i=0;i<6;i++)add_seed(2000+i,false);
    assert(world_live_visuals_validate(scene,components,seed,entities,error)&&error.empty());
    for(unsigned i=0;i<30;i++){
        world_entity::Node node;node.id=i+1;node.binding={world_entity::Kind::Placement,i};
        node.visible=i%2==0;scene.entities.nodes.push_back(node);
        world_runtime::PhysicsBinding binding;binding.entity=node.id;binding.body=node.id;components.physics.push_back(binding);
    }
    // Two seed crates + thirty authored models fill exactly thirty-two slots.
    assert(world_live_visuals_validate(scene,components,seed,entities,error));
    const auto before=components.physics.size();const auto before_nodes=scene.entities.nodes.size();
    world_entity::Node hidden;hidden.id=31;hidden.visible=false;hidden.binding={world_entity::Kind::SourceModel,9};scene.entities.nodes.push_back(hidden);
    world_runtime::PhysicsBinding binding;binding.entity=31;binding.body=31;components.physics.push_back(binding);
    assert(!world_live_visuals_validate(scene,components,seed,entities,error));
    assert(error.find("33 rigid")!=std::string::npos);
    assert(components.physics.size()==before+1&&scene.entities.nodes.size()==before_nodes+1&&!scene.entities.nodes.back().visible);
    // Unavailable seed visuals do not consume renderers. Re-enabling them
    // requires configuration/prevalidation again, with the complete candidate.
    entities.erase(entities.begin());assert(world_live_visuals_validate(scene,components,seed,entities,error));
    scene.entities.nodes.clear();components.physics.clear();
    // Six seed actors plus fifty-eight potential edited actors fill sixty-four.
    for(unsigned i=0;i<58;i++){
        world_entity::Node node;node.id=4000+i;node.visible=false;node.binding={world_entity::Kind::Actor,i};scene.entities.nodes.push_back(node);
        world_runtime::LivingBinding living;living.entity=node.id;living.actor=node.id;components.living.push_back(living);
    }
    assert(world_live_visuals_validate(scene,components,seed,entities,error));
    world_entity::Node actor;actor.id=5000;actor.binding={world_entity::Kind::Actor,0};scene.entities.nodes.push_back(actor);
    world_runtime::LivingBinding living;living.entity=actor.id;living.actor=actor.id;components.living.push_back(living);
    assert(!world_live_visuals_validate(scene,components,seed,entities,error)&&error.find("65 actor")!=std::string::npos);
    // Referential validity is owned by Runtime::configure, not guessed by the
    // render admission layer. Non-renderable markers consume no visual slots.
    components.living.clear();scene.entities.nodes.clear();
    world_runtime::PhysicsBinding absent;absent.entity=99999;absent.body=99999;components.physics.push_back(absent);
    assert(world_live_visuals_validate(scene,components,seed,entities,error)&&error.empty());
    std::cout<<"world_runtime_visuals: actual 32/33 rigid and 64/65 actor admission, hidden activation, seed counts, and input preservation passed\n";
}
'''.replace("HELPER", helper)
        with tempfile.TemporaryDirectory(prefix="valwood-visual-admission-") as folder:
            path = Path(folder)
            (path / "test.cpp").write_text(fixture)
            compile_result = subprocess.run(
                ["g++", "-std=c++17", "-O0", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT),
                 str(path / "test.cpp"), "-o", str(path / "test")],
                text=True, capture_output=True, timeout=30)
            self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
            run = subprocess.run([str(path / "test")], text=True, capture_output=True, timeout=5)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("32/33 rigid and 64/65 actor admission", run.stdout)


if __name__ == "__main__":
    unittest.main()
