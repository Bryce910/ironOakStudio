import {blank,example,validate,upgrade,compile,recall,makeMemory,descendants,arrange,KINDS,SOURCES,COLORS,LIMIT,HAIR_THICKNESS,HAIR_DENSITY} from './memory.mjs';
const $=id=>document.getElementById(id),NS='http://www.w3.org/2000/svg',KEY='valwood.memory-studio.v1';
let doc=blank(),selected='core',back=[],forward=[],drag=null,frame=0,last=0,saveTimer=0,appearanceOpen=true;
const view={mode:'brain',yaw:.32,pitch:.12,zoom:1,panX:0,panY:0,w:800,h:650,animate:!matchMedia('(prefers-reduced-motion: reduce)').matches};
let nodeEls=new Map(),edgeEls=[],signalEls=[],positions=new Map();
const copy=x=>structuredClone(x),tags=s=>[...new Set(s.split(/[,\n]/).map(t=>t.trim().toLowerCase().replace(/\s+/g,'-')).filter(Boolean))];
function toast(text){$('toast').textContent=text;$('toast').classList.add('show');clearTimeout(toast.timer);toast.timer=setTimeout(()=>$('toast').classList.remove('show'),4200);}
try{const stored=localStorage.getItem(KEY);if(stored){const value=JSON.parse(stored);if(!validate(value).length)doc=upgrade(value);else toast('The saved draft is invalid. A new core is ready; the previous browser draft has been preserved until you edit.');}}catch{toast('Browser draft storage is unavailable. Use Save project to keep your work.');}
function checkpoint(){back.push(copy(doc));if(back.length>32)back.shift();forward=[];}
function changed(rebuild=true,inspect=true){
 const errors=validate(doc);$('export').disabled=errors.length>0;$('save').disabled=errors.length>0;$('status').textContent=errors[0]||'Draft saved in this browser';$('status').classList.toggle('error',!!errors.length);
 $('undo').disabled=!back.length;$('redo').disabled=!forward.length;
 clearTimeout(saveTimer);if(!errors.length)saveTimer=setTimeout(()=>{try{localStorage.setItem(KEY,JSON.stringify(doc));}catch{$('status').textContent='Storage full — save a project file';}},350);
 if(rebuild)renderGraph();renderList();if(inspect)renderInspector();
 $('npc-name').textContent=doc.profile.name;$('npc-role').textContent=doc.profile.role||'Start with one core. Grow a mind.';$('node-count').textContent=doc.memories.length;
 $('empty-guide').hidden=doc.memories.length>0;$('add').disabled=doc.memories.length>=LIMIT;$('parent-hint').textContent='from '+(selected==='core'?'Core profile':doc.memories.find(m=>m.id===selected)?.title||'Core profile');
}
function select(id){selected=id;changed(false,true);for(const [key,el]of nodeEls)el.classList.toggle('selected',key===selected);}
function renderList(){const el=$('branch-list');el.replaceChildren();const filter=$('search').value.toLowerCase();for(const m of doc.memories){if(filter&&!`${m.title} ${m.body} ${m.tags.join(' ')}`.toLowerCase().includes(filter))continue;const b=document.createElement('button');b.className='branch-item'+(selected===m.id?' selected':'');const dot=document.createElement('span');dot.className='type-dot';dot.style.background=COLORS[m.kind];const text=document.createElement('span');text.append(document.createTextNode(m.title));const small=document.createElement('small');small.textContent=m.kind+(m.enabled?'':' · disabled');text.append(small);b.append(dot,text);b.onclick=()=>select(m.id);el.append(b);}if(!doc.memories.length){const p=document.createElement('p');p.className='help';p.textContent='Your first memory becomes a branch from the core. Each branch can grow more memories of its own.';el.append(p);}}
function field(form,label,key,obj,{type='text',max,min,step,options,help,optional=false}={}){
 const l=document.createElement('label');l.textContent=label;let input;
 if(options){input=document.createElement('select');for(const [value,name]of options){const o=document.createElement('option');o.value=value;o.textContent=name;input.append(o);}}
 else input=document.createElement(type==='textarea'?'textarea':'input');if(type!=='textarea'&&!options)input.type=type;
 if(min!==undefined)input.min=min;if(max!==undefined){if(type==='number'||type==='range')input.max=max;else input.maxLength=max;}if(step!==undefined)input.step=step;
 input.id='field-'+key;input.value=Array.isArray(obj[key])?obj[key].join(', '):(obj[key]??'');if(optional)input.placeholder='Unspecified';l.append(input);form.append(l);
 if(type==='range'){const out=document.createElement('output');out.textContent=Number(obj[key]).toFixed(2);l.insertBefore(out,input);out.style.float='right';input.addEventListener('input',()=>out.textContent=Number(input.value).toFixed(2));}
 let editing=false;input.addEventListener('focus',()=>editing=false);
 const commit=()=>{if(!editing){checkpoint();editing=true;}if(type==='number'||type==='range')obj[key]=input.value===''?(optional?null:NaN):Number(input.value);else if(key==='tags')obj[key]=tags(input.value);else if(key==='traits')obj[key]=input.value.split(',').map(x=>x.trim()).filter(Boolean);else obj[key]=input.value;changed(true,false);};
 input.addEventListener('input',commit);input.addEventListener('change',commit);
 if(help){const p=document.createElement('p');p.className='help';p.textContent=help;form.append(p);}return input;
}
function checkbox(form,label,key,obj){const l=document.createElement('label');l.className='check';const input=document.createElement('input');input.type='checkbox';input.checked=obj[key];input.id='field-'+key;input.onchange=()=>{checkpoint();obj[key]=input.checked;changed(true,false);};l.append(input,document.createTextNode(label));form.append(l);}
function appearanceFields(form){
 const section=document.createElement('details');section.className='appearance-fields';section.open=appearanceOpen;section.ontoggle=()=>appearanceOpen=section.open;
 const heading=document.createElement('summary');heading.textContent='Physical attributes';section.append(heading);form.append(section);
 const note=document.createElement('p');note.className='help';note.textContent='Export this profile, then use Living world → NPC memory → Apply profile appearance in the engine. Height, cosmetic build/weight, and compatible hair materials can change in 3D. Other details stay descriptive until the model supports them. Leave unknown values unspecified.';section.append(note);
 const a=doc.profile.appearance;
 const pair=()=>{const row=document.createElement('div');row.className='two-cols';section.append(row);return row;};
 let row=pair();field(row,'Height (cm)','height_cm',a,{type:'number',min:1,max:1000,step:.1,optional:true});field(row,'Weight (kg)','weight_kg',a,{type:'number',min:.1,max:10000,step:.1,optional:true});
 row=pair();field(row,'Age (years)','age_years',a,{type:'number',min:0,max:10000,step:.1,optional:true});field(row,'Body build','build',a,{max:64});
 row=pair();field(row,'Skin tone','skin_tone',a,{max:64});field(row,'Eye color','eye_color',a,{max:64});
 row=pair();const hair=field(row,'Hair color','hair_color',a,{max:64});field(row,'Hair style','hair_style',a,{max:64});
 hair.placeholder='Dark brown or #RRGGBB';hair.setAttribute('list','hair-colors');const colors=document.createElement('datalist');colors.id='hair-colors';for(const color of ['Black','Dark brown','Brown','Light brown','Blonde','Red','Auburn','Gray','Silver','White']){const option=document.createElement('option');option.value=color;colors.append(option);}section.append(colors);
 field(section,'Hair texture (straight, wavy, curly…)','hair_texture',a,{max:64});
 row=pair();const options=values=>values.map(v=>[v,v?v[0].toUpperCase()+v.slice(1):'Unspecified']);field(row,'Hair strand thickness','hair_thickness',a,{options:options(HAIR_THICKNESS)});field(row,'Hair density','hair_density',a,{options:options(HAIR_DENSITY)});
 const help=document.createElement('p');help.className='help';help.textContent='On compatible alpha hair cards, these adjust existing strand coverage; they do not generate new strands or hairstyles. Solid separate hair supports color and baldness. The engine reports support for your selected model.';section.append(help);
 field(section,'Facial hair','facial_hair',a,{max:64});field(section,'Distinguishing features','distinguishing_features',a,{type:'textarea',max:512,help:'Scars, freckles, tattoos, or other appearance details.'});
}
function renderInspector(){
 const form=$('inspector');form.replaceChildren();const m=doc.memories.find(m=>m.id===selected);const heading=document.createElement('h2');heading.textContent=m?'Memory details':'Core profile';form.append(heading);
 const note=document.createElement('p');note.className='sub';note.textContent=m?'A connected experience, belief, or intention.':'The stable identity at the center of this mind.';form.append(note);
 if(!m){selected='core';field(form,'Name','name',doc.profile,{max:64});field(form,'Role / occupation','role',doc.profile,{max:64});field(form,'Faction','faction',doc.profile,{max:64});appearanceFields(form);field(form,'Profile & backstory','summary',doc.profile,{type:'textarea',max:2048});field(form,'Personality traits, separated by commas','traits',doc.profile);field(form,'Stable NPC ID','npc_id',doc.profile,{max:48});field(form,'Engine actor ID','actor_id',doc.profile,{type:'number',min:0,max:1e9,step:1,help:'0 keeps this profile unbound. Use the persistent actor ID from the engine’s Living world system to bind it explicitly.'});return;}
 const badge=document.createElement('span');badge.className='badge';badge.textContent=m.id;form.append(badge);
 field(form,'Memory title','title',m,{max:96});field(form,'What this NPC remembers','body',m,{type:'textarea',max:2048});field(form,'Category','kind',m,{options:KINDS.map(x=>[x,x[0].toUpperCase()+x.slice(1)])});
 const blocked=descendants(doc,m.id);field(form,'Parent memory','parent_id',m,{options:[['core','Core profile'],...doc.memories.filter(n=>!blocked.has(n.id)).map(n=>[n.id,n.title])],help:'Branches inherit their ancestors’ trigger tags. A disabled branch also disables recall of its children.'});
 field(form,'Situation / trigger tags','tags',m,{help:'Comma-separated keywords, such as player, jungle, bandits. Tag matches are exact; this version does not infer synonyms.'});
 field(form,'Importance','importance',m,{type:'range',min:0,max:1,step:.01});field(form,'Confidence in this memory','confidence',m,{type:'range',min:0,max:1,step:.01});field(form,'Emotional tone · negative to positive','emotion',m,{type:'range',min:-1,max:1,step:.01});
 field(form,'Source','source',m,{options:SOURCES.map(x=>[x,x[0].toUpperCase()+x.slice(1)])});
 field(form,'Remembered at game hour','occurred_hour',m,{type:'number',min:0,max:1e9,step:1});field(form,'Decay half-life, in game hours','half_life_hours',m,{type:'number',min:0,max:1e9,step:1,help:'0 means permanent. A half-life reduces recall weight as game time passes; it does not delete the memory.'});
 checkbox(form,'Available for recall','enabled',m);checkbox(form,'May be shared in conversation','shareable',m);
 field(form,'Neural view depth','z',doc.layout.positions[m.id],{type:'range',min:-600,max:600,step:1,help:'Position and depth affect the picture only. They never change the memory’s meaning or importance.'});
 const remove=document.createElement('button');remove.className='danger';remove.id='delete-branch';remove.textContent=`Delete branch (${blocked.size})`;remove.onclick=()=>{checkpoint();doc.memories=doc.memories.filter(n=>!blocked.has(n.id));for(const id of blocked)delete doc.layout.positions[id];selected='core';changed();toast('Branch removed. Undo restores it.');};form.append(remove);
}
function svg(name,attrs={},parent){const e=document.createElementNS(NS,name);for(const [k,v]of Object.entries(attrs))e.setAttribute(k,String(v));if(parent)parent.append(e);return e;}
function renderGraph(){
 $('nodes').replaceChildren();$('edges').replaceChildren();$('signals').replaceChildren();$('rings').replaceChildren();nodeEls=new Map();edgeEls=[];signalEls=[];
 if(view.mode==='map')for(const r of [180,325,470])svg('circle',{r,class:'orbit-ring'},$('rings'));
 const records=[{id:'core',title:doc.profile.name,kind:'core',importance:1},...doc.memories];
 for(const m of doc.memories){const path=svg('path',{fill:'none',stroke:COLORS[m.kind],'stroke-width':1.1,opacity:.4},$('edges'));edgeEls.push({memory:m,path});}
 for(let i=0;i<Math.min(edgeEls.length,24);i++)signalEls.push(svg('circle',{r:2.3,fill:COLORS[edgeEls[i].memory.kind]},$('signals')));
 for(const m of records){
  const core=m.id==='core',color=core?'#b6e8d2':COLORS[m.kind]||'#b6e8d2',r=core?55:10+15*(m.importance||0);
  const g=svg('g',{class:'node'+(m.id===selected?' selected':''),tabindex:0,role:'button','data-node-id':m.id,'aria-label':core?'Core profile: '+m.title:m.kind+': '+m.title},$('nodes'));
  svg('circle',{r:r+11,fill:color,opacity:core?.22:.16,filter:'url(#glow)',...(core?{'data-core-halo':'true'}:{})},g);
  svg('circle',{r:r+7,fill:'none',stroke:color,'stroke-opacity':.22,'stroke-width':1},g);
  svg('circle',{class:'node-ring',r,fill:core?'url(#core-fill)':'#112622',stroke:color,'stroke-opacity':.75},g);
  svg('circle',{r:core?6:3,fill:color},g);
  if(core){const t=svg('text',{y:28,class:'node-core-text'},g);t.textContent='CORE';}
  const title=svg('text',{y:r+26,class:'node-label'},g);const words=m.title.length>32?m.title.slice(0,29)+'…':m.title;title.textContent=words;
  const kind=svg('text',{y:r+42,class:'node-kind'},g);kind.textContent=core?'NPC PROFILE':m.kind.toUpperCase();
  const info=svg('title',{},g);info.textContent=core?doc.profile.summary:m.body;
  g.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();select(m.id);}};nodeEls.set(m.id,g);
 }
 draw(0);
}
function project(p){
 if(view.mode==='map')return {x:p.x*view.zoom+view.panX,y:p.y*view.zoom+view.panY,s:1};
 const c=Math.cos(view.yaw),s=Math.sin(view.yaw),cp=Math.cos(view.pitch),sp=Math.sin(view.pitch);
 const x=c*p.x+s*p.z,z=-s*p.x+c*p.z,y=cp*p.y-sp*z,depth=sp*p.y+cp*z;
 const perspective=1100/Math.max(350,1100+depth);return {x:x*perspective*view.zoom+view.panX,y:y*perspective*view.zoom+view.panY,s:Math.max(.55,Math.min(1.25,perspective))};
}
function draw(t){
 const halo=$('nodes').querySelector('[data-core-halo]');if(halo){const pulse=view.animate?Math.sin(t*.002):0;halo.setAttribute('r',66+5*pulse);halo.setAttribute('opacity',.22+.055*pulse);}
 $('rings').setAttribute('transform',`translate(${view.panX} ${view.panY}) scale(${view.zoom})`);
 positions=new Map();for(const [id,el]of nodeEls){const p=project(doc.layout.positions[id]||{x:0,y:0,z:0});positions.set(id,p);el.setAttribute('transform',`translate(${p.x} ${p.y}) scale(${p.s*Math.sqrt(view.zoom)})`);const m=doc.memories.find(n=>n.id===id);el.style.opacity=m&&!m.enabled?'.32':String(Math.min(1,.5+.5*p.s));}
 edgeEls.forEach(({memory:m,path},i)=>{const a=positions.get(m.parent_id),b=positions.get(m.id);if(!a||!b)return;const dx=b.x-a.x,dy=b.y-a.y,len=Math.hypot(dx,dy)||1,bend=Math.min(34,len*.12),cx=(a.x+b.x)/2-dy/len*bend,cy=(a.y+b.y)/2+dx/len*bend;path.setAttribute('d',`M${a.x},${a.y} Q${cx},${cy} ${b.x},${b.y}`);path.setAttribute('stroke-width',selected===m.id||selected===m.parent_id?'1.8':'1');path.style.opacity=m.enabled?(selected===m.id?'.9':'.35'):'.12';
  if(signalEls[i]){const q=(t*.00016+i*.173)%1,u=1-q;signalEls[i].setAttribute('cx',u*u*a.x+2*u*q*cx+q*q*b.x);signalEls[i].setAttribute('cy',u*u*a.y+2*u*q*cy+q*q*b.y);signalEls[i].style.opacity=view.animate&&m.enabled?'.9':'0';}
 });
}
function tick(t){if(view.animate&&!document.hidden){if(t-last>33){if(!drag&&view.mode==='brain')view.yaw+=Math.sin(t*.0005)*.00005*Math.min(100,t-last);draw(t);last=t;}frame=requestAnimationFrame(tick);}else frame=0;}
function start(){if(view.animate&&!document.hidden&&!frame){last=performance.now();frame=requestAnimationFrame(tick);} $('motion').setAttribute('aria-pressed',String(view.animate));$('motion').textContent=view.animate?'◉ Animate':'○ Paused';}
function fit(){view.panX=view.panY=0;view.zoom=1;const p=Object.values(doc.layout.positions).map(project);const x=Math.max(220,...p.map(v=>Math.abs(v.x)+100)),y=Math.max(200,...p.map(v=>Math.abs(v.y)+100));view.zoom=Math.min(1.3,view.w/(x*2+40),view.h/(y*2+120));draw(0);}
new ResizeObserver(([entry])=>{view.w=entry.contentRect.width;view.h=entry.contentRect.height;$('network').setAttribute('viewBox',`${-view.w/2} ${-view.h/2} ${view.w} ${view.h}`);fit();}).observe($('network'));
function pointer(e){const r=$('network').getBoundingClientRect();return {x:e.clientX-r.left-r.width/2,y:e.clientY-r.top-r.height/2};}
$('network').addEventListener('pointerdown',e=>{if(e.button!==0&&e.button!==1)return;const g=e.target.closest('[data-node-id]');const id=g?.getAttribute('data-node-id');if(id)select(id);const p=pointer(e);drag={id:id==='core'?null:id,x:p.x,y:p.y,moved:false,pan:e.shiftKey||e.button===1};$('network').setPointerCapture(e.pointerId);e.preventDefault();});
$('network').addEventListener('pointermove',e=>{if(!drag)return;const p=pointer(e),dx=p.x-drag.x,dy=p.y-drag.y;if(!drag.moved&&Math.hypot(dx,dy)<2)return;if(!drag.moved&&drag.id)checkpoint();drag.moved=true;
 if(drag.id){const pos=doc.layout.positions[drag.id],screen=positions.get(drag.id),scale=view.zoom*(screen?.s||1),cx=dx/scale,cy=dy/scale;if(view.mode==='brain'){const c=Math.cos(view.yaw),s=Math.sin(view.yaw),cp=Math.cos(view.pitch),sp=Math.sin(view.pitch);pos.x+=c*cx-s*sp*cy;pos.y+=cp*cy;pos.z+=s*cx+c*sp*cy;}else{pos.x+=cx;pos.y+=cy;}for(const k of ['x','y','z'])pos[k]=Math.max(-100000,Math.min(100000,pos[k]));}
 else if(view.mode==='brain'&&!drag.pan){view.yaw+=dx*.007;view.pitch=Math.max(-1.2,Math.min(1.2,view.pitch+dy*.006));}else{view.panX+=dx;view.panY+=dy;}
 drag.x=p.x;drag.y=p.y;draw(performance.now());});
function endDrag(){if(!drag)return;const changedPosition=drag.id&&drag.moved;drag=null;if(changedPosition)changed(false,true);}
$('network').addEventListener('pointerup',endDrag);$('network').addEventListener('pointercancel',endDrag);
$('network').addEventListener('wheel',e=>{e.preventDefault();view.zoom=Math.max(.18,Math.min(3,view.zoom*Math.exp(-e.deltaY*.001)));draw(performance.now());},{passive:false});
$('brain-mode').onclick=()=>mode('brain');$('map-mode').onclick=()=>mode('map');function mode(m){view.mode=m;$('brain-mode').classList.toggle('active',m==='brain');$('map-mode').classList.toggle('active',m==='map');$('gesture-help').textContent=m==='brain'?'Drag space to orbit · Drag a dot to move it · Scroll to zoom':'Drag dots to move · Drag space to pan · Scroll to zoom';renderGraph();fit();}
$('motion').onclick=()=>{view.animate=!view.animate;start();draw(0);};document.addEventListener('visibilitychange',start);matchMedia('(prefers-reduced-motion: reduce)').addEventListener('change',e=>{if(e.matches){view.animate=false;start();draw(0);}});
$('fit').onclick=fit;$('zoom-in').onclick=()=>{view.zoom=Math.min(3,view.zoom*1.25);draw(performance.now());};$('zoom-out').onclick=()=>{view.zoom=Math.max(.18,view.zoom/1.25);draw(performance.now());};$('arrange').onclick=()=>{checkpoint();arrange(doc);changed();fit();};$('search').oninput=renderList;$('core-select').onclick=()=>select('core');
$('add').onclick=()=>{if(doc.memories.length>=LIMIT)return;checkpoint();const id='m_'+crypto.randomUUID().replaceAll('-','').slice(0,20),m=makeMemory(id,selected,$('new-kind').value);m.title=$('new-kind').selectedOptions[0].text+' memory';doc.memories.push(m);const parent=doc.layout.positions[selected],angle=(doc.memories.length*2.4)-1.57,dist=selected==='core'?190:145;doc.layout.positions[id]={x:parent.x+Math.cos(angle)*dist,y:parent.y+Math.sin(angle)*dist,z:parent.z+Math.sin(angle*1.7)*60};selected=id;changed();};
function replace(next){checkpoint();doc=next;selected='core';view.yaw=.32;view.pitch=.12;$('recall-results').replaceChildren();changed();fit();}
$('new').onclick=()=>{replace(blank());toast('New core created. Undo restores the previous project.');};$('example').onclick=()=>replace(example());
$('undo').onclick=()=>{if(!back.length)return;forward.push(copy(doc));doc=back.pop();selected='core';changed();};$('redo').onclick=()=>{if(!forward.length)return;back.push(copy(doc));doc=forward.pop();selected='core';changed();};
function download(name,body,type){const url=URL.createObjectURL(new Blob([body],{type})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
$('save').onclick=()=>{const e=validate(doc);if(e.length)return toast(e[0]);download(doc.profile.npc_id+'.vwmemory.json',JSON.stringify(doc,null,2)+'\n','application/json');toast('Project saved. This file preserves your editable layout.');};
$('export').onclick=()=>{try{download(doc.profile.npc_id+'.vwmemory',compile(doc),'text/plain;charset=utf-8');toast('Engine memory exported. Load it in Valwood → Living world → NPC memory. Save a project file too if you want to keep editing.');}catch(e){toast(e.message);}};
$('import').onclick=()=>$('file').click();$('file').onchange=async()=>{const f=$('file').files[0];if(!f)return;try{if(f.size>2*1024*1024)throw new Error('Project exceeds the 2 MiB file limit.');const next=upgrade(JSON.parse(await f.text()));replace(next);toast('Project opened.');}catch(e){toast('Import rejected: '+e.message);}finally{$('file').value='';}};
$('recall').onclick=()=>{const out=$('recall-results');out.replaceChildren();try{const results=recall(doc,tags($('query').value),$('hour').valueAsNumber);if(!results.length){const p=document.createElement('p');p.className='help';p.textContent='No enabled memories match this situation at this game hour.';out.append(p);}for(const r of results){const b=document.createElement('button');b.className='recall-result';b.textContent=r.memory.title;const why=document.createElement('small');why.textContent=`Score ${r.score.toFixed(3)} · ${r.memory.source} · confidence ${Math.round(r.memory.confidence*100)}% · ${r.memory.shareable?'shareable':'private'}`;const match=document.createElement('small');match.textContent=r.matched.length?'Matched: '+r.matched.join(', '):'General recall by importance';b.append(why,match);b.onclick=()=>select(r.memory.id);out.append(b);}}catch(e){toast(e.message);}};
document.addEventListener('keydown',e=>{if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='s'){e.preventDefault();$('save').click();}if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='z'&&!['INPUT','TEXTAREA','SELECT'].includes(e.target.tagName)){e.preventDefault();$(e.shiftKey?'redo':'undo').click();}});
changed();start();
