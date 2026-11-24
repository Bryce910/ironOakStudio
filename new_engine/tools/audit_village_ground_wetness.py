#!/usr/bin/env python3
"""CPU-check actual city puddle GLSL and measure its prepared source PBR pages."""
from pathlib import Path
import argparse, hashlib, json, subprocess, tempfile
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def block(text, signature):
    start = text.index(signature)
    begin = text.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    shader = ROOT/'projects/valwood_city_showcase/shaders/world_foliage.fs.glsl'
    text = shader.read_text()
    height = block(text, 'float source_height_weight(')
    prepare = block(text, 'void source_ground_prepare(')
    arm = block(block(text, 'vec3 facade_arm('), 'if(uFacadeMode==4)')
    arm = arm[arm.index('{'):]
    # Compile the production function bodies directly, with CPU sampler stubs.
    # This validates the new GLSL math without creating a GL/GPU context.
    cpp = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
struct vec2 {};
struct vec3 {float x,y,z;vec3(float a,float b,float c):x(a),y(b),z(c){}};
struct vec4 {float r,g,b,a;};
struct RGB {float r,g,b;} vSourcePaint{1,1,1};
vec3 sourceLayerWeights(1,0,0),sourceColorWeights(1,0,0),uFacadeUVScale(15,25,.06523899734020233f);
float sourcePuddle=0,sourceColorPuddle=0,uWetness=0;
vec4 sourceLayerA,sourceLayerB,sourceLayerC;
vec4 pages[]={{.5,.5,.6,.7},{.5,.5,.8,.8},{.5,.5,.75,.9},{.4,0,0,1}};
const int uNormal=0,uFacadeAux0=1,uFacadeAux2=2,uEmissive=3;
float clamp(float v,float a,float b){return std::clamp(v,a,b);}
float mix(float a,float b,float w){return a*(1-w)+b*w;}
vec4 facade_sample(int i,float,vec2,vec2,vec2){return pages[i];}
''' + height + '\n' + prepare + '\nvec3 groundARM()' + arm + r'''
int main(){
 unsigned checked=0;
 for(int h=0;h<=256;h++)for(float red:{0.f,.5f,1.f})
 for(float green:{0.f,.5f,1.f})for(float blue:{0.f,.5f,1.f}){
   pages[3].r=h/256.f;vSourcePaint={red,green,blue};uWetness=0;
   source_ground_prepare({},{},{});const vec3 dry=groundARM();
   assert(sourcePuddle==0&&sourceColorPuddle==0&&dry.z==0);
   const float full=source_height_weight(pages[3].r,.699999988079f*blue,0);
   const float fullColor=source_height_weight(.215860500114f,.699999988079f*blue,0);
   for(float wet:{-1.f,0.f,.25f,.5f,1.f,2.f}){
     uWetness=wet;source_ground_prepare({},{},{});vec3 value=groundARM();
     const float w=clamp(wet,0,1);
     assert(std::abs(sourcePuddle-full*w)<1e-6f);
     assert(std::abs(sourceColorPuddle-fullColor*w)<1e-6f);
     assert(std::abs(value.y-mix(dry.y,uFacadeUVScale.z,full*w))<1e-6f);
     assert(std::isfinite(value.y)&&value.y>=uFacadeUVScale.z&&value.y<=1&&value.z==0);
     checked++;
   }
 }
 std::printf("PASS actual production GLSL math: %u ground cases; dry puddle/color zero; wet endpoints exact; finite bounded roughness; dielectric metallic zero.\n",checked);
}
'''
    with tempfile.TemporaryDirectory(prefix='city-ground-wetness-') as directory:
        source = Path(directory)/'check.cpp'
        exe = Path(directory)/'check'
        source.write_text(cpp)
        subprocess.run(['g++', '-std=c++17', '-O2', str(source), '-o', str(exe)], check=True)
        check = subprocess.run([str(exe)], text=True, capture_output=True, check=True).stdout
    (args.output/'cpu-check.log').write_text(check)
    materials = json.loads((ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json').read_text())
    row = materials['MI_POM_Ground']

    def sample(path, tiling):
        pixels = np.asarray(Image.open(path).convert('RGBA'), dtype=np.float64)/255
        uv = ((np.arange(512)+.5)/512*tiling) % 1
        return pixels[np.floor(uv[:, None]*pixels.shape[0]).astype(int),
                      np.floor(uv[None, :]*pixels.shape[1]).astype(int)]

    def height_weight(h, phase, contrast):
        return np.clip((h+2*phase-1)*(1+2*contrast)-contrast, 0, 1)

    h = sample(row['emissive'], 25)[:, :, 0]
    h = np.where(h <= .04045, h/12.92, ((h+.055)/1.055)**2.4)
    a = sample(row['normal'], 15)
    c = sample(row['aux'][2], 15)
    red = height_weight(h, 1, -.047619000077)
    green = height_weight(h, 1, -.300000011921)
    dry = a[:, :, 2]*(1-red)*(1-green)+c[:, :, 2]*(red*(1-green)+green)
    source_puddle = height_weight(h, .699999988079, 0)
    metric = {}
    for wet in [0, .25, .5, 1]:
        puddle = source_puddle*wet
        rough = dry*(1-puddle)+row['facade_scale'][2]*puddle
        metric[str(wet)] = dict(puddle_mean=float(puddle.mean()),
            roughness_percentiles=dict(zip(['min', 'p05', 'p25', 'median', 'p75', 'p95', 'max'],
                np.percentile(rough, [0, 5, 25, 50, 75, 95, 100]).tolist())),
            roughness_below_point2_fraction=float(np.mean(rough < .2)))
    report = dict(shader=str(shader.resolve()), shader_sha256=hashlib.sha256(shader.read_bytes()).hexdigest(),
        cpu_check=check.strip(), samples=512**2,
        sampling='Representative source UV0 unit tile, original 15/25 repeats, nearest CPU texels; source ground RGB is white. Before pixel footprint filtering/global weather treatment.',
        weather_contract='Existing viewer sends uWetness=(.20+.64*rain)*rainforest_weight; captured town rainforest_weight=0, so dry town receives0. Both puddle branches clamp this existing uniform to0..1.',
        source_puddle_before_gate=dict(mean=float(source_puddle.mean()), median=float(np.median(source_puddle))),
        weather_metrics=metric,
        preserved=['Original source layers, height data/colorspace, normal detail, source roughness and AO channels',
                   'Generic wetness response, normal/specular footprint filtering, metallic0, lighting/reflections',
                   'Full source puddle response at uWetness>=1'],
        native_shader_compile='Pending parent native launch; no GPU context was created by this CPU audit.')
    (args.output/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print(check.strip())
    print(json.dumps(metric, indent=2))


if __name__ == '__main__':
    main()
