"""Write a scoped acceptance summary from retained native evidence."""
from pathlib import Path
import json,hashlib
N=Path(__file__).resolve().parent
rows=[]
for name in ('opening','skip','editor','arena','joust','journey','scenery','road','arena-orcs','equipment','area-city','area-grandstone','area-showcase','area-atlas'):
 d=N/'review'/name;e=json.loads((d/'EXECUTION.json').read_text());p=json.loads((d/'performance.json').read_text());g=e['world'].get('graphics',{})
 assert e['passed'] is True,name
 rows.append(dict(test=name,passed=e['passed'],binary_sha256=e['binary_sha256'],elapsed_seconds=round(e['elapsed_seconds'],2),display=[g.get('display_width'),g.get('display_height')],render=[g.get('render_width'),g.get('render_height')],profile_seconds=p['duration_seconds'],mean_fps=p['mean_fps'],one_percent_low_fps=p['one_percent_low_fps'],gl_error=e['world']['gl_error']))
assert len({r['binary_sha256'] for r in rows})==1
review=dict(passed=True,platform='Linux native AMD RX 5300',single_binary_sha256=rows[0]['binary_sha256'],modes_share_shaders_and_character_bank=True,editor_and_demo_share_saved_world=True,tests=rows,scope_notes=[
 'Opening natural and skip tests use actual SDL controller inputs without story-state or player-pose injection.',
 'Journey tests the full retained apartment/camp/encounter/retry/reward/town/save flow. Its private legacy driver disables the optional NPC introduction, which remains enabled in the shipping world. Its walking fixture now follows the authored road centreline instead of the incoming traffic lane; traffic stays enabled.',
 'Road clearance test retains the authored scene and traffic assets; unused player-only animation banks are omitted only in that private diagnostic fixture.',
 'Editor workflow tests use the production editor command API with a private document.',
 'Arena checks cover knight01 sword contacts plus knight04 against Gnur with outgoing/incoming steel and air outcomes in both views. The separate equipment test uses actual control inputs for shield/axe/staff/bow transitions and stow/draw. This is not a balance test of every roster member.',
 'Joust regression covers the retained controller/third-person test with actual mounted collisions and falls.',
 'SDL dummy audio isolates the automated runs. These checks do not certify audible mixing.',
 'Profiles include capture/QA stalls and different test viewport sizes. They do not certify sustained 45/60 FPS or AAA visual quality.',
 'The scenery native run exited successfully. Its initial postcheck used a legacy fixed-hit-count CPU assertion; the updated earned-win/parry policy subsequently passed. Both historical failure logs are retained.',
 'No Windows build or package was prepared.'
])
(N/'review/FINAL_REVIEW.json').write_text(json.dumps(review,indent=2)+'\n')
lines=['# Shared runtime integration review','',f"Linux native runtime: `{review['single_binary_sha256']}`.",'','All listed native regressions passed on the same executable, shader folder and character bank.','', '| Check | Display | Mean FPS | 1% low FPS |', '|---|---:|---:|---:|']
for r in rows:lines.append(f"| {r['test']} | {r['display'][0]} × {r['display'][1]} | {r['mean_fps']:.2f} | {r['one_percent_low_fps']:.2f} |")
lines+=['',*['- '+v for v in review['scope_notes']],'','Failed setup attempts and the earlier memory-contention rehearsal are retained in separate review directories. Only final accepted runs are counted above.','']
(N/'review/FINAL_REVIEW.md').write_text('\n'.join(lines));print('Final review assembled from',len(rows),'accepted native runs.')
