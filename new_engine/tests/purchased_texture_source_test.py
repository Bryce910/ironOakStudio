#!/usr/bin/env python3
"""Catch Unreal BGRA storage being mistaken for exported PNG RGB channels."""
import io,json,sys
from pathlib import Path
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from audit_purchased_village_source import Package,DEFAULT_SOURCE,canonical_texture_source_png

# Independent asymmetric color/alpha fixture checks byte interpretation, not
# a filename-based ARM heuristic. Grayscale/RGBA formats must not swap twice.
pixels=np.asarray([[[11,29,211,0],[37,71,191,128]],[[83,109,167,255],[139,151,157,19]]],dtype=np.uint8)
buffer=io.BytesIO();Image.fromarray(pixels).save(buffer,format='PNG')
normalized=np.asarray(Image.open(io.BytesIO(canonical_texture_source_png(buffer.getvalue(),'TSF_BGRA8'))).convert('RGBA'))
assert np.array_equal(normalized,pixels[:,:,[2,1,0,3]])
unchanged=np.asarray(Image.open(io.BytesIO(canonical_texture_source_png(buffer.getvalue(),'TSF_RGBA8'))).convert('RGBA'))
assert np.array_equal(unchanged,pixels)
records=[]
for source,export in [('T_Wood_Trims_01_ARM','T_Tileable_Wood_Trims_01_ARM.PNG'),('T_Wall_Stone_01_ARM','T_Tileable_Wall_Stone_01_ARM.PNG')]:
    package=Package(DEFAULT_SOURCE/'Textures'/(source+'.uasset'));canonical,size=package.texture_source_png()
    expected=np.asarray(Image.open(ROOT/'assets/purchased/medieval_village/source_textures'/export).convert('RGBA'))
    actual=np.asarray(Image.open(io.BytesIO(canonical)).convert('RGBA'));raw=np.asarray(Image.open(io.BytesIO(package.raw_texture_source_png)).convert('RGBA'))
    assert package.texture_source_provenance['storage_format']=='TSF_BGRA8'
    assert np.array_equal(actual,expected) and np.array_equal(actual,raw[:,:,[2,1,0,3]])
    records.append(dict(source=source,archive_export=export,dimensions=size,canonical_export_pixel_mae=0.,raw_rgba_pixel_mae=float(np.abs(raw.astype(float)-expected).mean()),**package.texture_source_provenance))

# The real source graph routes texture sample outputs1/2/3 as R/G/B. This
# prevents fixing the extraction bug by inverting the runtime ARM contract.
for name in ['MM_POM_Master_Simple','MM_Simple']:
    p=Package(DEFAULT_SOURCE/'Materials'/(name+'.uasset'));props=p.export_properties(next(e for e in p.exports if e['name']==name))
    assert props['AmbientOcclusion']['output']==1 and props['Roughness']['output']==2 and props['Metallic']['output']==3
p=Package(DEFAULT_SOURCE/'Materials/MM_POM_3_Blend_Puddle.uasset')
props=p.export_properties(next(e for e in p.exports if e['name']=='MM_POM_3_Blend_Puddle'))
constant=props['Metallic']['expression'];value=p.export_properties(next(e for e in p.exports if e['name']==constant));assert value.get('R',0)==0
nodes={e['name']:p.export_properties(e) for e in p.exports if e['name'].startswith('MaterialExpression')}
ao=nodes['MaterialExpressionMaterialFunctionCall_16']['FunctionInputs'][0]['Input'];rough=nodes['MaterialExpressionMaterialFunctionCall_6']['FunctionInputs'][0]['Input']
assert ao['output']==1 and rough['output']==2
report=dict(schema=1,asymmetric_color_alpha_fixture_exact=True,known_source_archive_pairs=records,architecture_packing='AO/R roughness/G metallic/B',ground_graph='AO/R roughness/G constant0 metallic',source_downloads_modified=False)
(ROOT/'assets/purchased/medieval_village/source-facades/channel-source-proof.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS TextureSource BGRA: asymmetric color/alpha exact; two actual source/archive PNGs pixel-identical after canonicalization; native ARM graph connections verified')
