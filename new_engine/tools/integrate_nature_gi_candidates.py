#!/usr/bin/env python3
"""Stage immutable GI revisions and change only matching asset-stem fields.

Default is isolated staging. --apply is a separate loading-boundary operation;
it preserves every other world.studio byte and all placement/shader/settings
files. New revisions reuse the project's existing source-keyed texture cache.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil
from import_world_asset import cook

ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def document(path):return path.read_bytes().decode('utf-8')
def revision_copy(source,dest):
    # Keep asset_near.glb hard-linked to the unsuffixed immutable copy without
    # linking production files to a temporary candidate directory.
    links={}
    def copy(src,dst):
        st=os.stat(src);key=(st.st_dev,st.st_ino)
        if key in links:os.link(links[key],dst);return dst
        result=shutil.copy2(src,dst);links[key]=dst;return result
    return shutil.copytree(source,dest,symlinks=True,copy_function=copy)
def asset_lines(text):
    result={}
    for line in text.splitlines(keepends=True):
        m=re.match(r'^(asset\s+([A-Za-z0-9_-]+)\s+)(\S+)([^\r\n]*)(\r?\n)?$',line)
        if m:
            if m[2]in result:raise ValueError('Duplicate asset alias '+m[2])
            result[m[2]]=(m[3],m[1],m[4],m[5]or'')
    return result
def replace_stems(text,replacements):
    count=0;out=[]
    for line in text.splitlines(keepends=True):
        m=re.match(r'^(asset\s+([A-Za-z0-9_-]+)\s+)(\S+)([^\r\n]*)(\r?\n)?$',line)
        if m and m[2]in replacements:
            old,new=replacements[m[2]]
            if m[3]not in (old,new):raise ValueError('Asset changed since staging: '+m[2])
            line=m[1]+new+m[4]+(m[5]or'');count+=1
        out.append(line)
    if count!=len(replacements):raise ValueError('Missing staged asset definition')
    return ''.join(out)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--project',type=Path,default=ROOT/'projects/valwood_showcase');ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-buttress-gi/catalog.json');ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-buttress-integration');ap.add_argument('--ancestor-report',type=Path,action='append',default=[],help='Explicit earlier candidate report, so a combined candidate can validate an active ancestor revision');ap.add_argument('--apply',action='store_true');args=ap.parse_args();project=args.project.resolve();args.output.mkdir(exist_ok=True);planpath=args.output/'plan.json'
    if not args.apply:
        reports=[args.catalog.parent/'report.json',*args.ancestor_report];changed={}
        for report_path in reports:
            for f in json.loads(report_path.read_text())['files']:
                if 'candidate_mid'in f:candidate,source=Path(f['candidate_mid']).resolve(),Path(f['source_mid']).resolve()
                else:candidate,source=Path(f['candidate']).resolve(),Path(f['source']).resolve()
                if candidate in changed and changed[candidate]!=source:raise ValueError('Ambiguous candidate ancestry')
                changed[candidate]=source
        current=document(project/'world.studio');definitions=asset_lines(current);entries=[]
        stage=args.output/'staged-project';stage.mkdir(exist_ok=True)
        for record in json.loads(args.catalog.read_text()):
            files=[Path(p).resolve() for p in record['files']]
            if not any(p in changed for p in files):continue
            alias='nm_scene_'+record['name'].removeprefix('nms_');old=definitions[alias][0]
            # Imports are byte-exact units1 copies. Refuse to replace a newer
            # geometry/material revision with a candidate based on older data.
            base=[]
            for suffix,path in zip(('.glb','_mid.glb'),files):
                active=project/'models'/(old+suffix)
                ancestors=[];seen=set();ancestor=path
                while ancestor in changed:
                    if ancestor in seen:raise ValueError('Cyclic candidate ancestry')
                    seen.add(ancestor);ancestor=changed[ancestor];ancestors.append(ancestor)
                if not ancestors:ancestors=[path]
                current_hash=digest(active);original=next((p for p in ancestors if digest(p)==current_hash),None)
                if original is None:raise ValueError('Current revision differs from verified candidate ancestry: '+str(active))
                base.append(original)
            new=cook(stage,alias,(files[0],files[1],None),1.)
            revision=stage/'models'/Path(new).parent;cache=revision/'texture_blocks'
            if not cache.exists():cache.symlink_to((project/'texture_blocks').resolve(),target_is_directory=True)
            entries.append({'alias':alias,'old_stem':old,'new_stem':new,'staged_revision':str(revision.resolve()),'source_anchor':record['engine_local_anchor'],'geometry_alignment':record['lod_alignment'],'source_files':[str(p)for p in base],'candidate_files':[str(p)for p in files]})
        if not entries:raise ValueError('No candidate revisions selected')
        replacements={e['alias']:(e['old_stem'],e['new_stem'])for e in entries};preview=replace_stems(current,replacements)
        (args.output/'world.studio.preview').write_bytes(preview.encode('utf-8'));plan={'project':str(project),'base_world_sha256':digest(project/'world.studio'),'entries':entries,'only_asset_stem_fields_change':True,'shared_texture_cache':str((project/'texture_blocks').resolve()),'catalog':str(args.catalog.resolve()),'verified_ancestor_reports':[str(p.resolve())for p in args.ancestor_report]};planpath.write_text(json.dumps(plan,indent=2)+'\n');print('STAGED',len(entries),'immutable asset revisions; production unchanged',flush=True);return
    plan=json.loads(planpath.read_text())
    if Path(plan['project'])!=project:raise ValueError('Staged project differs')
    current=document(project/'world.studio');replacements={e['alias']:(e['old_stem'],e['new_stem'])for e in plan['entries']};updated=replace_stems(current,replacements)
    for entry in plan['entries']:
        source=Path(entry['staged_revision']);dest=project/'models'/Path(entry['new_stem']).parent
        if not dest.resolve().is_relative_to(project):raise ValueError('Revision escapes source project')
        if dest.exists():
            for file in source.iterdir():
                if file.is_file()and digest(file)!=digest(dest/file.name):raise ValueError('Existing immutable revision differs')
        else:dest.parent.mkdir(parents=True,exist_ok=True);revision_copy(source,dest)
    backup=args.output/'world.studio.before-apply';backup.write_bytes(current.encode('utf-8'))
    temporary=project/'world.studio.candidate-tmp';temporary.write_bytes(updated.encode('utf-8'));temporary.replace(project/'world.studio')
    (args.output/'applied.json').write_text(json.dumps({'before_sha256':hashlib.sha256(current.encode()).hexdigest(),'after_sha256':digest(project/'world.studio'),'asset_stem_replacements':len(replacements),'all_other_document_lines_preserved':True,'placements_settings_shaders_untouched':True},indent=2)+'\n');print('APPLIED',len(replacements),'asset stem replacements',flush=True)

if __name__=='__main__':main()
