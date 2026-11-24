#!/usr/bin/env python3
"""Compare original purchased fluid mesh coverage using CPU projections only."""
import argparse
import json
import math
from pathlib import Path
import re

import numpy as np
from PIL import Image, ImageDraw

import cook_volumetric_blood as source

NAMES = ('Blood1','Blood2','Blood2_Left','Blood2_Right','Blood3','Blood4','Blood5','Blood6','Blood7','Blood8','Blood9')


def audit(bank, inventory, output):
    bank, output = Path(bank), Path(output)
    output.mkdir(parents=True, exist_ok=False)
    report = json.loads(Path(inventory).read_text())
    entries = {item['guid']:item for item in report['entries']}
    used = {}
    def read(guid):
        item = entries[guid]
        path = bank/item['path']
        if source.sha256_file(path) != item['source_sha256']:
            raise ValueError('Changed purchased source')
        used[item['path']] = item['source_sha256']
        return path
    results = []
    for name in NAMES:
        prefab = bank/source.SOURCE_PREFIX/'Prefabs'/f'{name}.prefab'
        docs = source.documents(prefab.read_text(encoding='utf-8-sig'))
        mesh = next(body for kind,body in docs.values() if kind==33 and 'fileID: 4300000' in body)
        owner = source.owner(mesh)
        renderer = next(body for kind,body in docs.values() if kind==23 and source.owner(body)==owner)
        animation = next(body for kind,body in docs.values() if kind==114 and source.owner(body)==owner)
        transforms = {key:body for key,(kind,body) in docs.items() if kind==4}
        def matrix(key):
            body = transforms[key]
            parent = int(re.search(r'm_Father: \{fileID: (-?\d+)\}',body)[1])
            local = source.transform_matrix(body)
            return matrix(parent)@local if parent else local
        transform = matrix(next(key for key,body in transforms.items() if source.owner(body)==owner))
        material_path = read(re.search(r'guid: ([0-9a-f]{32}), type: 2',renderer)[1])
        material = material_path.read_text(encoding='utf-8-sig')
        uv, _ = source.mesh_addressing(read(source.reference(mesh,'m_Mesh')))
        pixels = source.read_exr(read(source.texture_reference(material,'_posTex')))
        count = int(re.search(r'FramesCount: (\S+)',animation)[1])+1
        duration = float(re.search(r'TimeLimit: (\S+)',animation)[1])
        minimum,maximum = source.float_property(material,'_boundingMin'),source.float_property(material,'_boundingMax')
        offset = source.field_vector(material,'- _HeightOffset','rgb')
        linear = np.diag([1.,1.,-1.])@transform[:3,:3]
        translation = np.diag([1.,1.,-1.])@transform[:3,3]
        def frame(age):
            value = age/duration*(count-1)
            time = math.ceil(-(value+1.1))/count+1/count
            positions = source.gamma(source.sample_vat(pixels,uv,time))*(maximum-minimum)+minimum
            positions[:,0] *= -1
            return (positions[:,[0,2,1]]+offset)@linear.T+translation
        direction = frame(duration*.15).mean(0)
        angle = -math.atan2(float(direction[0]),float(direction[2]))
        ca,sa = math.cos(angle),math.sin(angle)
        canonical = np.array([[ca,0,sa],[0,1,0],[-sa,0,ca]])
        frames = {str(t):frame(t)@canonical.T for t in (.1,.3,.7)}
        areas = {}
        for age,positions in frames.items():
            tri = positions.reshape(-1,3,3)
            cross = np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])
            areas[age] = {'surface_area_m2':float(np.linalg.norm(cross,axis=1).sum()*.5),
                          'largest_triangle_m2':float(np.linalg.norm(cross,axis=1).max()*.5),
                          'head_on_projected_area_m2':float(np.abs(cross[:,2]).sum()*.5),
                          'side_projected_area_m2':float(np.abs(cross[:,0]).sum()*.5),
                          'bounds':[positions.min(0).tolist(),positions.max(0).tolist()]}
        # Same camera scale for every candidate: actual filled source triangles.
        image = Image.new('RGB',(1200,900),'#151b22')
        draw = ImageDraw.Draw(image)
        coverage = {}
        for row,age in enumerate((.1,.3,.7)):
            for col,degrees in enumerate((0,45,90)):
                radians = math.radians(degrees)
                positions = frames[str(age)]*.48
                horizontal = positions[:,0]*math.cos(radians)+positions[:,2]*math.sin(radians)
                depth = positions[:,2]*math.cos(radians)-positions[:,0]*math.sin(radians)
                xy = np.column_stack((col*400+145+horizontal*140,row*300+125-positions[:,1]*140))
                tile = Image.new('1',(400,300));paint=ImageDraw.Draw(tile)
                for index in np.argsort(-depth.reshape(-1,3).mean(1)):
                    triangle = xy.reshape(-1,3,2)[index]
                    points = [tuple(point) for point in triangle]
                    draw.polygon(points,fill='#a71925')
                    paint.polygon([(x-col*400,y-row*300) for x,y in points],fill=1)
                coverage[f'{age}s-{degrees}deg'] = int(np.count_nonzero(np.asarray(tile)))
                draw.text((col*400+12,row*300+12),f'{name} {age:.1f}s view{degrees} scale.48',fill='white')
                draw.ellipse((col*400+142,row*300+122,col*400+148,row*300+128),fill='white')
                draw.text((col*400+12,row*300+280),'CPU filled mesh; not a game screenshot',fill='#9ca9b5')
        image.save(output/f'{name}-filled-projections.png')
        results.append({'name':name,'triangles':len(uv)//3,'frames':count,'duration':duration,'area':areas,'projected_coverage_pixels':coverage})
    document = {'archive_sha256':report['sha256'],'source_data_unchanged':True,'purchased_code_executed':False,'gpu_used':False,
                'presentation_scale_for_comparison':.48,'projection_pixels_per_meter':140,'effects':results,'used_source_sha256':used}
    (output/'candidate-audit.json').write_text(json.dumps(document,indent=2)+'\n')
    return document


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-bank',type=Path,required=True)
    parser.add_argument('--inventory',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    result=audit(args.source_bank,args.inventory,args.output)
    for item in result['effects']:
        print(item['name'],item['triangles'],item['frames'],item['area']['0.3'],item['projected_coverage_pixels']['0.3s-45deg'])
