import assert from 'node:assert/strict';
import {mkdtemp,writeFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {blank,example,validate,upgrade,blankAppearance,recall,compile,makeMemory,descendants,arrange,LIMIT,APPEARANCE_NUMBERS,APPEARANCE_TEXT} from '../tools/npc_memory_editor/dist/memory.mjs';
const clone=structuredClone;
assert.deepEqual(validate(blank()),[]);const d=example();assert.deepEqual(validate(d),[]);assert.equal(d.memories.length,7);
const legacy=clone(d);legacy.version=1;delete legacy.profile.appearance;
const migrated=upgrade(legacy);assert.equal(migrated.version,2);assert.deepEqual(migrated.profile.appearance,blankAppearance());assert.equal(legacy.version,1);assert(!Object.hasOwn(legacy.profile,'appearance'));assert.deepEqual(migrated.memories,legacy.memories);assert.deepEqual(migrated.layout,legacy.layout);
assert.deepEqual(recall(migrated,['player'],72),recall(legacy,['player'],72));assert(compile(legacy).startsWith('VALWOOD_NPC_MEMORY 2\n'));
for(const mutate of [x=>x.profile.appearance=null,x=>delete x.profile.appearance.height_cm,x=>x.profile.appearance.extra='unknown',x=>x.profile.appearance.height_cm=0,x=>x.profile.appearance.height_cm=1001,x=>x.profile.appearance.weight_kg=-1,x=>x.profile.appearance.age_years=NaN,x=>x.profile.appearance.age_years=10001,x=>x.profile.appearance.height_cm='180',x=>x.profile.appearance.hair_thickness='thick',x=>x.profile.appearance.hair_density='lots',x=>x.profile.appearance.eye_color='\ud800',x=>x.profile.appearance.distinguishing_features='🙂'.repeat(129)]){const bad=clone(d);mutate(bad);assert(validate(bad).length>0);assert.throws(()=>upgrade(bad));assert.throws(()=>compile(bad));}
const legacyExtra=clone(legacy);legacyExtra.profile.appearance=blankAppearance();assert.throws(()=>upgrade(legacyExtra));
const changedAppearance=clone(d);changedAppearance.profile.appearance=blankAppearance();assert.deepEqual(recall(changedAppearance,['player'],72),recall(d,['player'],72));
for(const mutate of [x=>x.memories.push(null),x=>x.memories[0].parent_id='rescue',x=>x.memories[0].parent_id='missing',x=>x.memories[0].confidence=NaN,x=>x.memories[0].id='core',x=>x.memories[0].tags=['A'],x=>x.memories[0].tags=['a','a'],x=>x.layout.positions.core.x=1,x=>x.profile.actor_id=-1,x=>x.profile.name='🙂'.repeat(17)]){const bad=clone(d);mutate(bad);assert(validate(bad).length>0);assert.throws(()=>compile(bad));}
assert.equal(descendants(d,'family').size,3);const layout=clone(d);layout.layout.positions.rescue.x+=1000;assert.deepEqual(recall(layout,['player'],72).map(x=>x.score),recall(d,['player'],72).map(x=>x.score));
const malformedUnicode=clone(d);malformedUnicode.profile.name='\ud800';assert(validate(malformedUnicode).length>0);
assert.equal(recall(d,['player','jungle','bandits'],72)[0].memory.id,'rescue');const disabled=clone(d);disabled.memories.find(x=>x.id==='family').enabled=false;assert(!recall(disabled,['player'],72).length);
const future=clone(d);future.memories.find(x=>x.id==='family').occurred_hour=100;assert(!recall(future,['player'],72).length);
const large=blank();for(let i=0;i<LIMIT;i++){const m=makeMemory('m_'+i);large.memories.push(m);large.layout.positions[m.id]={x:0,y:0,z:0};}assert.deepEqual(validate(arrange(large)),[]);assert.equal(recall(large,[],0).length,8);
for(let i=0;i<LIMIT;i++)for(let j=0;j<i;j++){const a=large.layout.positions['m_'+i],b=large.layout.positions['m_'+j];assert(Math.hypot(a.x-b.x,a.y-b.y)>=70);}
const binary=process.argv[2];assert(binary,'Pass the C++ memory test executable');const temp=await mkdtemp(join(tmpdir(),'valwood-memory-'));
try{
 const file=join(temp,'example.vwmemory'),bad=join(temp,'bad.vwmemory');await writeFile(file,compile(d));await writeFile(bad,'VALWOOD_NPC_MEMORY 999\n');
 let run=spawnSync(binary,[file,bad],{encoding:'utf8'});assert.equal(run.status,0,run.stderr);process.stdout.write(run.stdout);
 // Use the native loader to compare every exported attribute, including blanks,
 // fractional measurements, age zero, and escaped/multiline Unicode text.
 const physical=clone(d);Object.assign(physical.profile.appearance,{age_years:0,height_cm:182.25,weight_kg:83.125,facial_hair:'Short beard',distinguishing_features:'A "silver" scar\\tattoo.\nÉtoile 🌿'});
 const quote=s=>'"'+s.replaceAll('\\','\\\\').replaceAll('"','\\"')+'"';
 for(const doc of [d,physical,blank(),migrated]){
  await writeFile(file,compile(doc));run=spawnSync(binary,['--appearance',file],{encoding:'utf8'});assert.equal(run.status,0,run.stderr);
  const lines=run.stdout.split('\n'),a=doc.profile.appearance;
  Object.keys(APPEARANCE_NUMBERS).forEach((k,i)=>assert.equal(lines[i]==='unset'?null:Number(lines[i]),a[k]));
  assert.equal(lines.slice(3).join('\n'),APPEARANCE_TEXT.map(k=>quote(a[k])).join('\n')+'\n');
 }
 // Version 1 engine files remain readable and keep attributes unspecified.
 const v1=compile(migrated).replace('VALWOOD_NPC_MEMORY 2','VALWOOD_NPC_MEMORY 1').split('\n').filter(line=>!line.startsWith('APPEARANCE ')).join('\n');await writeFile(file,v1);run=spawnSync(binary,['--appearance',file],{encoding:'utf8'});assert.equal(run.status,0,run.stderr);assert(run.stdout.startsWith('unset\nunset\nunset\n'));
 const variants=[d,large,disabled,future];const decayed=clone(d);decayed.memories[2].body='A "promise" in a\\wooden room.\nMara’s memory of Valdoa — café.';decayed.memories[2].half_life_hours=24;variants.push(decayed);
 let checked=0;
 for(const [i,doc]of variants.entries()){
  const path=join(temp,'variant_'+i+'.vwmemory');await writeFile(path,compile(doc));
  for(const hour of [0,24,72,150])for(const tags of [[],['family'],['player','jungle','bandits'],['fire'],['tavern','grain'],['family','family']])for(const publicOnly of [false,true]){
   const expected=recall(doc,tags,hour,publicOnly);run=spawnSync(binary,['--query',path,String(hour),...tags,...(publicOnly?['--public']:[])],{encoding:'utf8'});assert.equal(run.status,0,run.stderr);
   const actual=run.stdout.trim()?run.stdout.trim().split('\n').map(line=>line.split('\t')):[];assert.equal(actual.length,expected.length);
   for(let j=0;j<actual.length;j++){assert.equal(actual[j][0],expected[j].memory.id);assert(Math.abs(Number(actual[j][1])-expected[j].score)<1e-12);assert.equal(Number(actual[j][2]),expected[j].matched.length);}checked++;
  }
 }
 for(const contents of [compile(d)+'junk',compile(d).replace('MEMORIES 7','MEMORIES 9999999'),compile(d).replace('"family" "core"','"family" "rescue"'),compile(d).replace('APPEARANCE 38 168 67','APPEARANCE 38 0 67'),compile(d).replace('APPEARANCE 38 168 67','APPEARANCE -2 168 67'),compile(d).replace('"coarse" "dense"','"invalid" "dense"'),compile(d).replace('APPEARANCE','BROKEN'), 'x'.repeat(2*1024*1024+1)]){await writeFile(bad,contents);run=spawnSync(binary,['--query',bad,'72'],{encoding:'utf8'});assert.notEqual(run.status,0);}
 console.log('PASS physical attributes: v1 project migration and engine compatibility, unspecified values, measurement/text validation, complete native round trip and appearance-independent recall');
 console.log(`PASS browser domain/export: blank core,256-node layout, invalid imports, geometry-independent recall and${checked} exact JS/C++ parity queries including Unicode, escaping, decay and privacy`);
}finally{await rm(temp,{recursive:true,force:true});}
