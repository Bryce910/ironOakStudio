export const SCHEMA='valwood.npc-memory';
export const LIMIT=256;
export const KINDS=['event','relationship','belief','goal','knowledge','skill'];
export const SOURCES=['authored','witnessed','hearsay'];
export const HAIR_THICKNESS=['','fine','medium','coarse'];
export const HAIR_DENSITY=['','bald','sparse','medium','dense'];
export const APPEARANCE_NUMBERS={age_years:[0,10000],height_cm:[1,1000],weight_kg:[.1,10000]};
export const APPEARANCE_TEXT=['build','skin_tone','eye_color','hair_color','hair_style','hair_texture','hair_thickness','hair_density','facial_hair','distinguishing_features'];
export function blankAppearance(){return {...Object.fromEntries(Object.keys(APPEARANCE_NUMBERS).map(k=>[k,null])),...Object.fromEntries(APPEARANCE_TEXT.map(k=>[k,'']))};}
export const COLORS={event:'#e0aa68',relationship:'#db92af',belief:'#ac9ee8',goal:'#80bbdf',knowledge:'#77c9b0',skill:'#b6ca7b'};
const ident=/^[a-z][a-z0-9_-]{0,47}$/;
const tagPattern=/^[a-z0-9][a-z0-9_-]{0,31}$/;
const bytes=s=>new TextEncoder().encode(s).length;
const finite=(v,a,b)=>typeof v==='number'&&Number.isFinite(v)&&v>=a&&v<=b;
const text=(v,max,required=false)=>{
 if(typeof v!=='string'||bytes(v)>max||(required&&!v.trim().length)||/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f]/.test(v))return false;
 for(let i=0;i<v.length;i++){const c=v.charCodeAt(i);if(c>=0xd800&&c<=0xdbff){const next=v.charCodeAt(++i);if(!(next>=0xdc00&&next<=0xdfff))return false;}else if(c>=0xdc00&&c<=0xdfff)return false;}return true;
};
const keys=(obj,list)=>obj&&typeof obj==='object'&&!Array.isArray(obj)&&Object.keys(obj).every(k=>list.includes(k))&&list.every(k=>Object.hasOwn(obj,k));
export function validate(d){
 const errors=[];const bad=s=>errors.push(s);
 if(!d||d.schema!==SCHEMA||![1,2].includes(d.version))return ['Unsupported memory document. Expected valwood.npc-memory version 1 or 2.'];
 if(!keys(d,['schema','version','profile','memories','layout']))bad('Document fields do not match this version.');
 const p=d.profile;
 if(!keys(p,['npc_id','actor_id','name','role','faction','summary','traits',...(d.version===2?['appearance']:[])]))return ['Profile fields are incomplete or unknown.'];
 if(!ident.test(p.npc_id))bad('NPC ID must start with a lowercase letter and use at most 48 letters, digits, underscores or hyphens.');
 if(!Number.isInteger(p.actor_id)||!finite(p.actor_id,0,1000000000))bad('Actor ID must be 0 (unbound) or a positive engine actor ID.');
 for(const k of ['name','role','faction'])if(!text(p[k],64,k==='name'))bad(`Profile ${k} must fit 64 UTF-8 bytes.`);
 if(!text(p.summary,2048))bad('Profile description must fit 2048 UTF-8 bytes.');
 if(!Array.isArray(p.traits)||p.traits.length>8||p.traits.some(x=>!text(x,48,true)))bad('Use at most 8 traits of 48 UTF-8 bytes each.');
 if(d.version===2){
  const a=p.appearance;
  if(!keys(a,[...Object.keys(APPEARANCE_NUMBERS),...APPEARANCE_TEXT]))bad('Physical attributes are incomplete or contain unknown fields.');
  else {
   for(const [k,[min,max]]of Object.entries(APPEARANCE_NUMBERS))if(a[k]!==null&&!finite(a[k],min,max))bad(`${k.replaceAll('_',' ')} must be unspecified or between ${min} and ${max}.`);
   for(const k of APPEARANCE_TEXT)if(!text(a[k],k==='distinguishing_features'?512:64))bad(`Physical attribute ${k.replaceAll('_',' ')} exceeds its UTF-8 text budget.`);
   if(!HAIR_THICKNESS.includes(a.hair_thickness))bad('Choose a valid hair strand thickness.');
   if(!HAIR_DENSITY.includes(a.hair_density))bad('Choose a valid hair density.');
  }
 }
 if(!Array.isArray(d.memories)||d.memories.length>LIMIT)return [...errors,`Use at most ${LIMIT} memories per NPC.`];
 const ids=new Set(['core']);const map=new Map();
 for(const m of d.memories){
  if(!keys(m,['id','parent_id','kind','title','body','tags','importance','confidence','emotion','source','shareable','occurred_hour','half_life_hours','enabled'])){bad('A memory has incomplete or unknown fields.');continue;}
  if(typeof m.id!=='string'||!ident.test(m.id)||ids.has(m.id))bad('Memory IDs must be valid and unique.');
  ids.add(m.id);map.set(m.id,m);
  if(!KINDS.includes(m.kind)||!SOURCES.includes(m.source))bad('Unknown memory category or source.');
  if(!text(m.title,96,true)||!text(m.body,2048))bad('Each memory needs a title (96 UTF-8 bytes maximum) and a description of at most 2048 UTF-8 bytes.');
  if(!Array.isArray(m.tags)||m.tags.length>8||m.tags.some(t=>typeof t!=='string'||!tagPattern.test(t))||new Set(m.tags).size!==m.tags.length)bad('Use up to 8 unique lowercase trigger tags per memory.');
  for(const k of ['importance','confidence'])if(!finite(m[k],0,1))bad(`${m.title}: ${k} must be between 0 and 1.`);
  if(!finite(m.emotion,-1,1))bad(`${m.title}: emotion must be between -1 and 1.`);
  if(!finite(m.occurred_hour,0,1e9)||!finite(m.half_life_hours,0,1e9))bad('Memory times must be finite game hours between 0 and 1 billion.');
  if(typeof m.enabled!=='boolean'||typeof m.shareable!=='boolean')bad('Enabled and shareable values must be booleans.');
 }
 for(const m of d.memories){
  if(!m||typeof m.id!=='string'||!map.has(m.id))continue;
  let id=m.id;const visited=new Set();let depth=0;
  while(id!=='core'){
   if(visited.has(id)){bad('Memory branches cannot contain a cycle.');break;}
   visited.add(id);const n=map.get(id);
   if(!n){bad('Every memory must connect to the core through an existing parent.');break;}
   id=n.parent_id;if(++depth>16){bad('Memory branches may be at most 16 levels deep.');break;}
  }
 }
 if(!d.layout||!keys(d.layout,['positions'])||!d.layout.positions||typeof d.layout.positions!=='object'||Array.isArray(d.layout.positions))bad('Missing editor positions.');
 else {
  for(const [id,pos] of Object.entries(d.layout.positions))if(!ids.has(id)||!keys(pos,['x','y','z'])||!['x','y','z'].every(k=>finite(pos[k],-100000,100000)))bad('Invalid editor position.');
  for(const id of ids)if(!Object.hasOwn(d.layout.positions,id))bad('Each node needs an editor position.');
  const c=d.layout.positions.core;if(c&&(c.x!==0||c.y!==0||c.z!==0))bad('The core must remain at the origin.');
 }
 return [...new Set(errors)];
}
// Validate the old document before migrating so unknown fields are never silently dropped.
export function upgrade(d){const errors=validate(d);if(errors.length)throw new Error(errors[0]);if(d.version===2)return d;const next=structuredClone(d);next.version=2;next.profile.appearance=blankAppearance();return next;}
export function blank(){return {schema:SCHEMA,version:2,profile:{npc_id:'npc_new',actor_id:0,name:'New NPC',role:'',faction:'',summary:'',traits:[],appearance:blankAppearance()},memories:[],layout:{positions:{core:{x:0,y:0,z:0}}}};}
export function makeMemory(id,parent_id='core',kind='event'){return {id,parent_id,kind,title:'New memory',body:'',tags:[],importance:.7,confidence:1,emotion:0,source:'authored',shareable:false,occurred_hour:0,half_life_hours:0,enabled:true};}
export function descendants(d,id){const found=new Set([id]);let more=true;while(more){more=false;for(const m of d.memories)if(found.has(m.parent_id)&&!found.has(m.id)){found.add(m.id);more=true;}}return found;}
export function arrange(d){
 const walk=(parent,angle,spread,depth,base=180)=>{
  const children=d.memories.filter(m=>m.parent_id===parent);
  children.forEach((m,i)=>{
   let a=angle+(i-(children.length-1)/2)*spread/Math.max(1,children.length),rootRadius=base;
   if(parent==='core'&&children.length>12){let ring=0,slot=i;while(slot>=12+10*ring){slot-=12+10*ring;ring++;}a=-Math.PI/2+slot*Math.PI*2/(12+10*ring)+ring*.19;rootRadius=180+ring*145;}
   const r=rootRadius+depth*145;
   d.layout.positions[m.id]={x:Math.cos(a)*r,y:Math.sin(a)*r,z:Math.sin(a*2+depth*.8)*85};
   walk(m.id,a,spread/Math.max(1,children.length)*.85,depth+1,rootRadius);
  });
 };walk('core',-Math.PI/2,Math.PI*2,0);return d;
}
export function example(){
 const d=blank();d.profile={...d.profile,npc_id:'npc_mara_fenwick',actor_id:0,name:'Mara Fenwick',role:'Tavern keeper',faction:'Valdoa townsfolk',summary:'Keeps a warm tavern near the castle market. Generous with travelers, fiercely protective of family, and wary of the eastern jungle road.',traits:['observant','protective','hospitable']};
 Object.assign(d.profile.appearance,{age_years:38,height_cm:168,weight_kg:67,build:'Sturdy',skin_tone:'Warm olive',eye_color:'Hazel',hair_color:'Dark brown',hair_style:'Loose braid',hair_texture:'Wavy',hair_thickness:'coarse',hair_density:'dense',distinguishing_features:'A small scar on her left eyebrow.'});
 const add=(id,parent,kind,title,body,tags,importance,emotion=0,source='authored',confidence=1,shareable=false)=>d.memories.push({...makeMemory(id,parent,kind),title,body,tags,importance,emotion,source,confidence,shareable});
 add('family','core','relationship','Family comes first','Her family is her strongest connection to Valdoa.',['family','valdoa'],.95,.8);
 add('brother','family','event','A brother on the road','Her brother travels through the jungle to deliver grain.',['brother','jungle','grain'],.9,.6);
 add('rescue','brother','event','You brought him home','The player escorted her brother through a dangerous stretch of forest.',['player','help','jungle'],.95,.9,'witnessed',1,true);
 add('trade','core','knowledge','Life at the tavern','The tavern connects merchants, guards, and travelers.',['tavern','market'],.75,0);
 add('supply','trade','goal','Keep the larder full','She hopes to secure a reliable grain delivery before winter.',['grain','winter','trade'],.85,.2,'authored',1,true);
 add('rumor','trade','belief','Trouble beyond the creek','A traveler claimed bandits are gathering near Old Tanthoon. It has not been independently confirmed.',['bandits','old-tanthoon','jungle'],.75,-.7,'hearsay',.45,true);
 add('past','core','event','The night of the fire','A torch fire once damaged her first inn.',['fire','torch','danger'],.9,-.9,'witnessed',1,false);
 return arrange(d);
}
export function recall(d,tags,hour=0,shareableOnly=false){
 const errors=validate(d);if(errors.length)throw new Error(errors[0]);
 if(!finite(hour,0,1e9))throw new Error('Recall time must be a finite game hour.');
 tags=[...new Set(tags)];if(tags.length>16||tags.some(t=>!tagPattern.test(t)))throw new Error('Use up to 16 valid context tags.');
 const map=new Map(d.memories.map(m=>[m.id,m]));const output=[];
 for(const m of d.memories){
  const inherited=new Set();let current=m,active=true;
  while(current){active&&=current.enabled&&current.occurred_hour<=hour;for(const t of current.tags)inherited.add(t);current=map.get(current.parent_id);}
  if(!active||(shareableOnly&&!m.shareable))continue;
  const matched=tags.filter(t=>inherited.has(t));if(tags.length&&!matched.length)continue;
  const relevance=tags.length?Math.min(1,matched.length/Math.min(3,tags.length)):0;
  const decay=m.half_life_hours?Math.pow(2,-(hour-m.occurred_hour)/m.half_life_hours):1;
  const score=(.5*m.importance+.2*m.confidence+.1*Math.abs(m.emotion)+.2*relevance)*decay;
  output.push({memory:m,score,matched,decay});
 }
 return output.sort((a,b)=>b.score-a.score||(a.memory.id<b.memory.id?-1:a.memory.id>b.memory.id?1:0)).slice(0,8);
}
const quote=s=>'"'+s.replaceAll('\\','\\\\').replaceAll('"','\\"')+'"';
export function compile(d){
 d=upgrade(d);const p=d.profile,a=p.appearance;
 const lines=['VALWOOD_NPC_MEMORY 2','PROFILE '+[quote(p.npc_id),p.actor_id,quote(p.name),quote(p.role),quote(p.faction),quote(p.summary)].join(' '),'TRAITS '+[p.traits.length,...p.traits.map(quote)].join(' '),'APPEARANCE '+[...Object.keys(APPEARANCE_NUMBERS).map(k=>a[k]??-1),...APPEARANCE_TEXT.map(k=>quote(a[k]))].join(' '),'MEMORIES '+d.memories.length];
 for(const m of d.memories)lines.push('MEMORY '+[quote(m.id),quote(m.parent_id),KINDS.indexOf(m.kind),quote(m.title),quote(m.body),m.importance,m.confidence,m.emotion,SOURCES.indexOf(m.source),Number(m.shareable),m.occurred_hour,m.half_life_hours,Number(m.enabled),m.tags.length,...m.tags.map(quote)].join(' '));
 lines.push('END');return lines.join('\n')+'\n';
}
