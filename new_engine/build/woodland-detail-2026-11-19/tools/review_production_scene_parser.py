"""Run the production placement-parser loops with production atlas/water state."""
from pathlib import Path
import json
import re
import subprocess


def audit(n, baseline, sha):
    out = n/'review/forest-scene'
    source_root = n/'source-v1'
    path = source_root/'engine/world_scenery.cpp'
    text = path.read_text()
    def block(content, start):
        begin = content.index(start)
        brace = content.index('{', begin)
        depth = 0
        for i in range(brace, len(content)):
            if content[i] == '{': depth += 1
            elif content[i] == '}': depth -= 1
            if depth == 0: return content[begin:i+1]+(';' if start.startswith('struct ') else '')
        raise ValueError(start)
    constants = '\n'.join(re.search(r'constexpr std::size_t '+name+r'[^;]+;', text)[0]
                          for name in ['record_cap', 'source_byte_cap', 'legacy_record_bytes'])
    registry = text[text.index('enum class Grounding'):text.index('struct Placement {')]
    parser = block(text, 'for(std::size_t source=0;source<file_count;source++)')
    water_path = source_root/'engine/world_studio_runtime_view.inc'
    height_path = source_root/'engine/world_studio_view.inc'
    callbacks = 'struct NativeCallbacks {\n'+block(height_path.read_text(), 'static float height(')+'\n'
    callbacks += block(water_path.read_text(), 'static bool water_sample(')+'\n};\n'
    # Only the hosting scene container and error return are adapted. The
    # registry, configure transaction, every parsing/validation loop, and the
    # height/water callbacks below are copied byte-for-byte from production.
    code = '''#include "engine/world_scenery.h"
#include "engine/world_studio.h"
#include "engine/world_atlas.h"
#include "engine/world_pond_field.h"
#include "engine/world_surface_field.h"
#include "engine/world_transform_math.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
constexpr double pi=3.14159265358979323846;
'''+constants+'\n'+registry+'\n'
    code += block(text, 'struct Placement {')+'\n'+block(text, 'struct Source {')+'\n'
    code += block(text, 'void error_text(')+'\n'+block(text, 'bool world_scenery_configure(')+'\n'
    code += callbacks
    code += '''struct ParseScene {std::vector<Placement> placements;std::vector<Source> sources;WorldSceneryStats stats;};
bool validate(const char*const*paths,std::size_t file_count,std::string&error,std::size_t&rows){
ParseScene host;auto*scene=&host;std::size_t maximum_records=record_cap;
std::array<std::size_t,alias_count> counts={};auto fail=[&](const std::string&why){error=why;return false;};
'''+parser+'''
rows=scene->placements.size();return true;}
int main(int argc,char**argv){if(argc!=3)return 2;namespace fs=std::filesystem;const fs::path project=fs::absolute(argv[1]);
world_studio::Document doc;std::string error;char why[512]={};
if(!world_atlas_init()||world_atlas_load_height_region((project/"ground/showcase.height").c_str(),why,sizeof why)!=1||
world_atlas_load_fallback_height_region((project/"ground/countryside.height").c_str(),why,sizeof why)<0||
!world_studio::load((project/"world.studio").c_str(),doc,error)||!world_pond_field::prepare(doc,{world_atlas_base_height},error)){std::cerr<<why<<' '<<error;return 2;}
world_atlas_set_height_edit(NativeCallbacks::height,&doc);world_atlas_set_water_edit(NativeCallbacks::water_sample,&doc);
std::vector<WorldSceneryDefinition> defs;for(const auto&a:doc.assets)defs.push_back({a.alias.c_str(),a.stem.c_str(),a.lods,a.collision,a.vegetation,a.distance,a.split});
if(!world_scenery_configure(defs.data(),defs.size(),why,sizeof why)){std::cerr<<why;return 2;}
std::vector<std::string> names;for(auto&e:fs::directory_iterator(project/"scenery"))if(e.is_regular_file()&&e.path().extension()==".txt")names.push_back(e.path().string());std::sort(names.begin(),names.end());
std::vector<const char*> paths;for(auto&s:names)paths.push_back(s.c_str());std::size_t rows=0;
bool valid=validate(paths.data(),paths.size(),error,rows);std::string final_error=error;const char*negative[]={argv[2]};std::size_t ignored=0;error.clear();
bool rejected=!validate(negative,1,error,ignored)&&error.find("root must be above local water on dry terrain")!=std::string::npos;
std::cout<<"{\\"passed\\":"<<(valid&&rejected?"true":"false")<<",\\"all_final_scene_rows_valid\\":"<<(valid?"true":"false")
<<",\\"final_scene_files\\":"<<names.size()<<",\\"final_scene_rows\\":"<<rows<<",\\"final_error\\":"<<std::quoted(final_error)
<<",\\"native_shoreline_failure_negative_control_rejected\\":"<<(rejected?"true":"false")<<",\\"negative_control_error\\":"<<std::quoted(error)<<"}\\n";
world_atlas_set_height_edit(nullptr,nullptr);world_atlas_set_water_edit(nullptr,nullptr);world_atlas_shutdown();return valid&&rejected?0:1;}
'''
    cpp = out/'production-scene-parser.cpp'
    cpp.write_text(code)
    binary = cpp.with_suffix('')
    object_root = baseline/'runtime-native-v1/native-objects/engine'
    names = ['world_studio.cpp', 'world_surface_authoring.cpp', 'world_surface_field.cpp',
             'world_entity_graph.cpp', 'world_shader_graph.cpp', 'world_authoring_identity.cpp',
             'world_pond_field.cpp', 'world_terrain_authoring.cpp', 'world_atlas.c']
    objects = [object_root/(name+'.o') for name in names]
    inputs = {str(p): sha(p) for p in objects+[Path(__file__), path, height_path, water_path]}
    inputs[str(n/'world-project/world.studio')] = sha(n/'world-project/world.studio')
    for p in (n/'world-project/scenery').glob('*.txt'): inputs[str(p)] = sha(p)
    for name in ['showcase.height', 'countryside.height']:
        p = n/'world-project/ground'/name
        if p.is_file(): inputs[str(p)] = sha(p)
    negative = out/'parser-negative-authored_instances.txt'
    inputs[str(negative)] = sha(negative)
    command = ['g++', '-std=c++17', '-O2', '-I'+str(source_root), str(cpp)]+list(map(str, objects))+['-pthread', '-o', str(binary)]
    built = subprocess.run(command, capture_output=True, text=True)
    (out/'production-scene-parser-build.log').write_text(built.stdout+built.stderr)
    assert built.returncode == 0, built.stderr
    run = [str(binary), str(n/'world-project'), str(negative)]
    result = subprocess.run(run, capture_output=True, text=True)
    (out/'production-scene-parser-raw.json').write_text(result.stdout)
    (out/'production-scene-parser-run.log').write_text(result.stderr)
    raw = json.loads(result.stdout)
    unchanged = all(Path(p).is_file() and sha(p) == h for p, h in inputs.items())
    report = dict(schema='valwood.production-placement-parser-cpu-review.v1',
                  passed=result.returncode == 0 and raw['passed'] and unchanged,
                  gpu_used=False, authored_or_production_files_modified=False,
                  inputs_sha256=inputs, inputs_unchanged=unchanged, build_command=command, run_command=run,
                  generated_adapter_sha256=sha(cpp), helper_binary_sha256=sha(binary),
                  raw=raw, scope='Exact production registry/configure and full placement parsing/validation loops, with production atlas init, primary/fallback terrain, pond/river field preparation and exact Studio height/water callbacks. All final authored files are parsed together.',
                  limitations=['The parser-only hosting adapter stops before GPU model loading, render grouping and upload.',
                               'Native game startup remains the independent full-loader check.'])
    (out/'PRODUCTION_SCENE_PARSER_REVIEW.json').write_text(json.dumps(report, indent=2)+'\n')
    return report
