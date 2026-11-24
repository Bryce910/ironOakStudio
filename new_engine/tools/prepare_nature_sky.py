"""Convert purchased source EXR to a bounded native Radiance panorama; run via Blender CPU."""
from pathlib import Path
import bpy,numpy as np,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'assets/purchased/nature_castle/source/Assets/NatureManufacture Assets/Castle and Dungeon/Others/3D textures and Volumes/TC_Cloudy_Sky_02.exr'
out=ROOT/'assets/purchased/nature_castle/showcase/environment';out.mkdir(exist_ok=True)
i=bpy.data.images.load(str(source),check_existing=False);raw=np.empty(len(i.pixels),np.float32);i.pixels.foreach_get(raw);print('source',i.size[:],i.colorspace_settings.name,float(raw.min()),float(raw.max()))
i.scale(2048,1024);i.filepath_raw=str(out/'publisher_cloudy_sky.hdr');i.file_format='HDR';i.save()
pixels=np.empty(len(i.pixels),np.float32);i.pixels.foreach_get(pixels);rgb=pixels.reshape(1024,2048,4)[:,:,:3]
report={'source':str(source.relative_to(ROOT)),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'source_size':[4096,2048],'output':str(out/'publisher_cloudy_sky.hdr'),'output_size':[2048,1024],'output_sha256':hashlib.sha256((out/'publisher_cloudy_sky.hdr').read_bytes()).hexdigest(),'color_space':'linear HDR RGB, untonemapped; no skybox tint/exposure baked','rgb_range':[float(rgb.min()),float(rgb.max())],'skybox_rotation_degrees':154,'skybox_exposure':1,'skybox_serialized_tint_rgb':[.6698113,.6698113,.6698113],'source_ambient_intensity':1.7,'source_reflection_intensity':1,'native_expected_rotation_degrees':154,'native_hdr_cube_side':256,'native_cube_approx_bytes':6356992}
(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print('Saved',out/'publisher_cloudy_sky.hdr')
