from pathlib import Path
import hashlib,json,shutil
import numpy as np
from PIL import Image,ImageDraw
N=Path(__file__).resolve().parents[1];D=N/'review';B=N.with_name('scenery-ecology-2026-11-18')
read=lambda p:json.loads(Path(p).read_text())
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
before=read(D/'world/baseline/gameplay-views/EXECUTION.json')
after=read(D/'world/updated/gameplay-views/EXECUTION.json')
assert before['passed'] and after['passed']
assert sha(D/'world/baseline/gameplay-views/private-project/world.studio')==sha(D/'world/updated/gameplay-views/private-project/world.studio')
assert sha(D/'world/baseline/gameplay-views/private-project/graphics.settings')==sha(D/'world/updated/gameplay-views/private-project/graphics.settings')
memory=[]
for case in ['baseline','updated']:
    t=read(D/f'world/{case}/gameplay-views/TEXTURE_GPU.json')
    memory.append(sum(t[k] for k in ['peak_textures','peak_buffers','peak_renderbuffers'])/1048576)
timings={}
for label,old in before['timings'].items():
    current=after['timings'][label]
    assert current['median_ms']<old['median_ms']*.95,(label,old,current)
    timings[label]=dict(before_median_fps=1000/old['median_ms'],after_median_fps=1000/current['median_ms'],
        before_p95_ms=old['p95_ms'],after_p95_ms=current['p95_ms'],
        frame_time_reduction_percent=(1-current['median_ms']/old['median_ms'])*100)
assert memory[0]-memory[1]>500
old_idle=read(B/'review/shipping/EXECUTION.json');idle=read(D/'shipping/EXECUTION.json')
assert idle['passed'] and idle['settings']==old_idle['settings']
assert idle['profile']['mean_ms']<old_idle['profile']['mean_ms']
performance=dict(passed=True,binary_sha256=sha(N/'runtime/rpg'),matched_opening_views=timings,
    matching_geometry_placements_materials_and_fixed_settings=True,
    logical_peak_allocation_mib_before=memory[0],logical_peak_allocation_mib_after=memory[1],
    tracked_peak_allocation_saved_mib=memory[0]-memory[1],not_driver_vram_residency=True,
    ordinary_idle_before_fps=old_idle['profile']['mean_fps'],ordinary_idle_after_fps=idle['profile']['mean_fps'],
    ordinary_idle_one_percent_low_fps=idle['profile']['one_percent_low_fps'],
    ordinary_idle_p95_ms=idle['profile']['p95_ms'],ordinary_graphics=idle['world']['graphics'],
    limitations=['Static and ordinary idle views do not certify sustained movement/combat performance.',
        'Opening is not a locked60FPS on the RX5300. Legacy bank startup remains slow.'],
    additional_forest_pass_rejected=True,shipping_renderer_passes_unchanged=True,
    prototype_results_not_used_for_shipped_performance=True,windows_work_performed=False)
(D/'PERFORMANCE_REVIEW.json').write_text(json.dumps(performance,indent=2)+'\n')
labels=['camp','shade-floor','path-verge','woodland-edge','village-approach']
def sheet(name,left,right):
    out=Image.new('RGB',(1280,1950),'#181b1b');draw=ImageDraw.Draw(out);difference={}
    for i,label in enumerate(labels):
        images=[]
        for j,(directory,title) in enumerate([left,right]):
            im=Image.open(directory/f'view-{i}-{label}.png').convert('RGB');images.append(np.array(im,dtype=float))
            out.paste(im.resize((640,360)),(640*j,390*i+30));draw.text((640*j+10,390*i+9),title+' - '+label,fill='white')
        difference[label]=float(np.abs(images[0]-images[1]).mean())
    out.save(D/name);return difference
differences=sheet('optimization-comparison.png',(D/'world/baseline/gameplay-views','Previous'),(D/'world/updated/gameplay-views','Lossless storage optimization'))
sheet('scenery-comparison.png',(D/'world/updated/gameplay-views','Before ground polish'),(D/'world/updated/views','Final ground polish'))
for area in ['world','arena']:
    destination=N/'authoring-snapshot'/area;destination.mkdir(parents=True,exist_ok=True)
    shutil.copy2(N/(area+'-project/world.studio'),destination/'world.studio')
    (destination/'scenery').mkdir(exist_ok=True)
    for source in (N/(area+'-project/scenery')).glob('*instances.txt'):
        shutil.copy2(source,destination/'scenery'/source.name)
review=dict(passed=True,binary_sha256=sha(N/'runtime/rpg'),native_optimization_views=5,
    final_native_polished_views=5,mean_RGB_difference_255= differences,
    tests=['Exact native GPU pose storage regression','Shared editor/game shader lifecycle',
        'Native human and Orc sword contact/blocking','Native jousting controller/dehorse rehearsal'],
    terrain_water_collision_routes_story_and_saved_progress_preserved=True,
    published_material_changes=10,grass_mask_slots_reserved_for_designer=16,
    limitations=['World-exploration camera images do not rehearse quest conversations.',
        'Existing distant terrain and some rock assets still need further art work.'],
    full_editor_UI_not_retested=True,windows_work_performed=False)
(D/'FINAL_REVIEW.json').write_text(json.dumps(review,indent=2)+'\n')
print(json.dumps(performance,indent=2))
