#!/usr/bin/env python3
"""Native SDL rehearsal of arena configuration, combat, combos and saved coins.

All state changes come from ordinary gameplay controls. Authoring, settings,
model banks and user saves stay protected in a private runtime copy.
"""
import argparse
import fcntl
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import time

from PIL import Image, ImageStat
from benchmark_graphics_settings import native_environment
from check_opening_prologue import isolate_project, mutable_snapshot
from check_playtest_slices import timings
from prepare_combat_arena import blood_asset_record

ROOT = Path(__file__).resolve().parents[1]
CAPTURES = {'arena-eight', 'arena-defeat', 'arena-retry', 'bow-first-draw', 'bow-third-draw',
            'arrow-flight', 'arrow-hit', 'queue-progress', 'arena-victory', 'arena-reset',
            'body-hit', 'metal-block', 'metal-parry', 'blood-fluid', 'blood-paused', 'blood-stain',
            'gore-impact', 'finisher-blood', 'blood-persistent', 'blood-settled',
            'defeat-fall', 'defeat-grounded', 'arena-setup', 'arena-one-setup',
            'combo-first', 'combo-second', 'combo-third', 'combo-held', 'arena-one-victory',
            'arena-many-setup', 'arena-many-active', 'arena-many-defeat', 'arena-lock-on'}
CAPTURE_MEANINGS = {
    'arena-one-setup': 'One-opponent setup-to-fight commit: the rendered setup menu shows 1/1 and +10; metadata is read after its Start handler commits a fresh entering opponent in the same UI frame.',
    'arena-many-setup': 'Eight-opponent setup-to-fight commit: the rendered setup menu shows 8/8 and +80; metadata is read after its Start handler commits eight fresh entering opponents in the same UI frame.',
}
CHECKS = {
    'setup opens before combat with eight total and one simultaneous',
    'focused setup Start allocates a fresh run without awarding coins',
    'eight visible challengers and grounded third-person start',
    'exclusive real SDL virtual gamepad',
    'purchased blood source bank is loaded',
    'polished fighter loads directional reactions and real collapse',
    'keyboard 2 equips bow',
    'keyboard 1 equips sword',
    'controller D-pad right equips bow',
    'controller D-pad left equips sword',
    'pause freezes real arena combat',
    'ordinary Escape resumes arena',
    'unguarded enemy attacks cause real defeat',
    'real defeat leaves the persistent purse unchanged',
    'focused result button retries all eight with a new run receipt',
    'controller camera enters first-person bow',
    'fresh RT physically draws bow',
    'LT steadies bow aim without sword guard',
    'bow pause cancels draw and freezes fight',
    'held bow trigger cannot fire or redraw after resume',
    'controller camera preserves third-person drawn bow',
    'charged physical arrow hits the active knight',
    'physical arrow produces blood at its resolved body contact',
    'purchased volumetric mesh animates at body contact',
    'pause freezes purchased fluid and floor stains',
    'guarded sword contact produces iron sparks without body damage',
    'sword body contact produces blood and a damage cue',
    'timed parry produces iron sparks and a steel cue',
    'directional body damage produces crossed heavy fluid sprays',
    'finishing body hit produces a heavier blood burst',
    'blood floor stains persist into the next challenger',
    'defeated fighter performs a grounded collapse',
    'fallen fighter remains grounded while the next challenger enters',
    'blood settles within one second while ground splatter remains',
    'ground blood remains anchored and fully visible after thirty seconds',
    'eight sequential real duels won with bow hit sword contacts and parries',
    'eight-opponent victory saves exactly eighty arena coins',
    'heavy blood sprays and finishers remain bounded through eight fights',
    'focused repeat starts a fresh eight-knight arena',
    'fresh run clears purchased fluid and floor stains',
    'all original combat checkpoints and setup captured',
    'ending a fresh run opens settings without another reward',
    'ordinary menu navigation selects one total and one simultaneous',
    'one-opponent selection starts exactly one real challenger',
    'first fresh flick commits a paid animated combo swing',
    'fresh recovery flick links a second paid animated swing',
    'third fresh recovery flick completes a three-swing paid combo',
    'held third gesture cannot create a fourth combo swing',
    'one-opponent real victory persists exactly ten more coins',
    'repeated result frames and disk reload cannot duplicate coins',
    'repeat allocates a unique receipt while preserving the saved purse',
    'ordinary menu navigation selects eight simultaneous opponents',
    'controller A starts eight genuinely active independent opponents',
    'eight-opponent camera keeps the player centered without automatic enemy framing',
    'controller RB locks an enemy without starting a thrust',
    'explicit lock keeps one target and retains the normal player camera',
    'second RB releases the target without a camera snap or attack',
    'independent simultaneous opponents inflict a genuine shared-player defeat',
    'simultaneous defeat awards no coins and preserves both saved victories',
    'all configured combat combo and purse checkpoints captured',
}


def sha(path):
    with Path(path).open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()


def finite_number(value, minimum=0):
    return type(value) in (int, float) and math.isfinite(value) and value >= minimum


def qa_failures(world):
    errors = []; qa = world.get('arena_qa', {})
    camera=world.get('arena_camera', {})
    for key,minimum in [('player_follow_frames',100),('multiple_opponent_frames',30),('locked_frames',20),('lock_toggles',2)]:
        if type(camera.get(key)) is not int or camera[key]<minimum: errors.append('missing actual camera evidence: '+key)
    for key in ('maximum_heading_error','maximum_lateral_error','maximum_pitch_error'):
        if not finite_number(camera.get(key)) or camera[key]>.0001: errors.append('arena camera left player framing: '+key)
    if qa.get('enabled') is not True or qa.get('passed') is not True:
        errors.append('arena SDL rehearsal incomplete')
    for key, minimum in {'checks': len(CHECKS), 'events': 100, 'loss_hits': 5, 'total_hits': 8, 'total_parries': 1}.items():
        if type(qa.get(key)) is not int or qa[key] < minimum: errors.append('missing observed arena '+key)
    labels = qa.get('check_labels')
    if not isinstance(labels, list) or set(labels) != CHECKS or len(labels) != len(CHECKS): errors.append('required configured combat behavior missing or duplicated')
    if qa.get('wallet_disk_verified') is not True or type(qa.get('initial_purse')) is not int or qa.get('final_purse') != qa.get('initial_purse', -1)+90:
        errors.append('two genuine victories did not persist exactly ninety coins')
    if qa.get('combo_pose_witnesses') != 7: errors.append('three animated paid combo witnesses missing')
    if type(qa.get('concurrent_damage_sources')) is not int or not 2 <= qa['concurrent_damage_sources'] <= 8:
        errors.append('independent simultaneous enemy damage missing')
    if qa.get('final_total') != 8 or qa.get('final_simultaneous') != 8: errors.append('eight concurrent configuration not exercised')
    names = qa.get('capture_names')
    if not isinstance(names, list) or set(names) != CAPTURES or len(names) != len(CAPTURES) or qa.get('captures') != len(CAPTURES):
        errors.append('actual arena checkpoint captures missing')
    for key in ('combat_state_mutations', 'player_pose_writes', 'story_state_writes'):
        if qa.get(key) != 0: errors.append('QA bypassed ordinary gameplay: '+key)
    for key in ('gl_error', 'remaining_texture_references'):
        if world.get(key) != 0: errors.append(key)
    if world.get('asset_streaming', {}).get('failed') != 0: errors.append('streaming failed')
    feedback = world.get('combat_feedback', {})
    for key in ('body_bursts', 'metal_bursts', 'body_audio', 'metal_audio'):
        if type(feedback.get(key)) is not int or feedback[key] < 1: errors.append('missing combat feedback: '+key)
    if feedback.get('cut_windup_seconds') != .18 or feedback.get('cut_cycle_seconds') != .51:
        errors.append('faster sword timing missing')
    if feedback.get('parry_window_seconds') != .24: errors.append('fair parry window changed')
    blood = world.get('volumetric_blood', {})
    if blood.get('enabled') is not True or blood.get('provenance_loaded') is not True:
        errors.append('purchased blood source not loaded')
    for key in ('variants', 'source_frames', 'source_vertices', 'source_triangles', 'geometry_bytes',
                'texture_bytes', 'mesh_draws', 'stain_draws', 'frame_transitions', 'spawned', 'resets'):
        if type(blood.get(key)) is not int or blood[key] < 1: errors.append('missing purchased blood evidence: '+key)
    for key, maximum in [('peak_meshes', 8), ('peak_stains', 128)]:
        if type(blood.get(key)) is not int or not 1 <= blood[key] <= maximum: errors.append('blood budget exceeded or unused: '+key)
    if blood.get('failures') != 0: errors.append('purchased blood render failure')
    if type(blood.get('counter_rebases')) is not int or blood['counter_rebases'] < 0: errors.append('invalid blood counter rebase evidence')
    intensity = world.get('combat_blood_intensity', {})
    if intensity.get('fluid_scale') != .48 or intensity.get('initial_particles', 0) < 32:
        errors.append('stronger source-fluid scale or initial blood burst missing')
    for key in ('impacts', 'secondary_sprays', 'secondary_draws', 'finisher_draws', 'initial_finisher_bursts'):
        if type(intensity.get(key)) is not int or intensity[key] < 1: errors.append('missing stronger blood evidence: '+key)
    if intensity.get('impacts') != intensity.get('secondary_sprays'):
        errors.append('second source-fluid lobe missing from a body hit')
    if intensity.get('finisher_events', 0) < 8 or intensity.get('max_strength', 0) < 1.5:
        errors.append('actual finishing-hit blood weights missing')
    persistence = world.get('ground_blood_persistence', {})
    if persistence.get('enabled') is not True or persistence.get('witness_checked') is not True:
        errors.append('persistent ground blood witness not verified')
    if type(persistence.get('capacity')) is not int or persistence['capacity'] != 128:
        errors.append('persistent ground blood capacity incorrect')
    if not finite_number(persistence.get('witness_age'), 30):
        errors.append('ground blood not observed beyond the old lifetime')
    for key in ('witness_serial', 'stains_spawned'):
        if type(persistence.get(key)) is not int or persistence[key] < 1:
            errors.append('missing persistent ground blood evidence: '+key)
    if type(persistence.get('stains_replaced')) is not int or persistence['stains_replaced'] != 0:
        errors.append('ground blood replaced during eight-fight rehearsal')
    motion = world.get('combat_blood_motion', {})
    if motion.get('playback_rate') != 3 or motion.get('gravity') != 9.81:
        errors.append('faster blood playback or physical gravity missing')
    maximum = motion.get('maximum_mesh_seconds')
    if not finite_number(maximum) or not 0 < maximum <= 1:
        errors.append('airborne blood lifetime exceeds one second or is missing')
    mist = motion.get('mist_lifetime')
    if not finite_number(mist) or mist > .12:
        errors.append('short blood mist lifetime missing')
    if motion.get('settled_checked') is not True or not finite_number(motion.get('settle_elapsed'), 1):
        errors.append('airborne blood settling not observed after one second')
    if type(motion.get('settle_stains')) is not int or not 1 <= motion['settle_stains'] <= 128:
        errors.append('settled blood leaves no bounded persistent floor splatter')
    character = world.get('character_polish', {})
    for key in ('enabled', 'keep_defeated', 'fall_checked', 'grounded_checked'):
        if character.get(key) is not True: errors.append('character polish not observed: '+key)
    for key, expected in {'pose_assets': 344, 'reaction_assets': 40, 'death_assets': 24, 'fallen_peak': 8}.items():
        if type(character.get(key)) is not int or character[key] != expected:
            errors.append('character asset or fallen-body count incorrect: '+key)
    for key in ('death_starts', 'directional_reactions'):
        if type(character.get(key)) is not int or character[key] < 8:
            errors.append('eight real fighter reactions or defeats missing: '+key)
    if not finite_number(character.get('grounded_age'), 1.8):
        errors.append('grounded fighter not observed after collapse')
    return errors


def capture_failures(name, state):
    if not isinstance(state,dict):return ['invalid capture state: '+name]
    camera_errors=[]
    for key in ('camera_heading_error','camera_lateral_error','camera_pitch_error'):
        if not finite_number(state.get(key)) or state[key]>.0001: camera_errors.append('player camera framing incorrect: '+name+': '+key)
    if name=='arena-lock-on' and (state.get('target_locked') is not True or type(state.get('lock_target')) is not int or not 0<=state['lock_target']<8 or state.get('current')!=state['lock_target']):
        camera_errors.append('explicit rendered target lock missing')
    if name=='arena-many-active' and state.get('target_locked') is not False: camera_errors.append('default camera was automatically locked')
    if not isinstance(state, dict) or state.get('name') != name or type(state.get('frame')) is not int or state['frame'] <= 0:
        return ['capture name/frame invalid: '+name]
    errors = camera_errors
    if state.get('blood_loaded') is not True or state.get('blood_variants', 0) < 1 or state.get('blood_failures') != 0:
        errors.append('purchased source unavailable in capture: '+name)
    if state.get('blood_persistent_mode') is not True or type(state.get('blood_stain_capacity')) is not int or state['blood_stain_capacity'] != 128:
        errors.append('persistent ground blood mode/capacity missing: '+name)
    fade = state.get('blood_minimum_stain_fade')
    if not finite_number(fade) or fade > 1:
        errors.append('rendered ground blood fade missing/invalid: '+name)
    if not finite_number(state.get('blood_oldest_stain_age')):
        errors.append('rendered ground blood age missing/invalid: '+name)
    drawn = state.get('blood_persistent_stains_drawn')
    if type(drawn) is not int or not 0 <= drawn <= 128:
        errors.append('rendered persistent ground blood count missing/invalid: '+name)
    if state.get('blood_playback_rate') != 3:
        errors.append('faster purchased fluid playback missing: '+name)
    maximum = state.get('blood_max_air_seconds')
    if not finite_number(maximum) or not 0 < maximum <= 1:
        errors.append('bounded airborne blood duration missing: '+name)
    if type(state.get('blood_staged_meshes')) is not int or not 0 <= state['blood_staged_meshes'] <= 8:
        errors.append('live blood mesh count missing/invalid: '+name)
    if not finite_number(state.get('blood_settle_elapsed')):
        errors.append('blood settling clock missing/invalid: '+name)
    if state.get('motion_polished') is not True:
        errors.append('polished fighter motion unavailable in capture: '+name)
    for key, expected in {'motion_pose_assets': 344, 'reaction_assets': 40, 'death_assets': 24}.items():
        if type(state.get(key)) is not int or state[key] != expected:
            errors.append('polished fighter asset count incorrect: '+name+': '+key)
    if type(state.get('fallen_count')) is not int or not 0 <= state['fallen_count'] <= 8:
        errors.append('fallen fighter count missing/invalid: '+name)
    if not finite_number(state.get('first_defeat_age')):
        errors.append('fighter collapse age missing/invalid: '+name)
    phase = state.get('first_defeat_phase')
    if not finite_number(phase) or phase > 1:
        errors.append('fighter collapse phase missing/invalid: '+name)
    if type(state.get('first_defeat_anchored')) is not bool:
        errors.append('fighter collapse anchor evidence missing/invalid: '+name)
    counts = [state.get(key) for key in ('waiting', 'entering', 'active', 'yielded', 'unused')]
    total, simultaneous = state.get('total_opponents'), state.get('simultaneous_opponents')
    counts_valid = all(type(value) is int and 0 <= value <= 8 for value in counts)
    if not counts_valid or sum(counts) != 8:
        errors.append('bounded configured actor roster missing: '+name)
    if type(total) is not int or type(simultaneous) is not int or not 1 <= simultaneous <= total <= 8:
        errors.append('configured opponent limits invalid: '+name)
    elif counts_valid and (state.get('unused') != 8-total or state['active']+state['entering'] > simultaneous):
        errors.append('actual roster exceeds configured opponent limits: '+name)
    for key in ('purse', 'initial_purse', 'run_id', 'saved_reward', 'attack_serial', 'combo_links', 'combo_count'):
        if type(state.get(key)) is not int or state[key] < 0: errors.append('configured run evidence missing: '+name+': '+key)
    if type(state.get('reward_pending')) is not bool or type(state.get('combo_pending')) is not bool:
        errors.append('configured run save/input state missing: '+name)
    if type(state.get('draft_total')) is not int or type(state.get('draft_simultaneous')) is not int or not 1 <= state['draft_simultaneous'] <= state['draft_total'] <= 8:
        errors.append('setup selection out of bounds: '+name)
    elif state.get('payout_preview') != 10*state['draft_total']: errors.append('setup payout preview incorrect: '+name)
    if name in {'arena-eight', 'arena-retry', 'arena-reset'}:
        if state.get('defeated') != 0 or state.get('yielded') != 0:
            errors.append('fresh eight actor queue not shown: '+name)
        if state.get('blood_meshes') != 0 or state.get('blood_stains') != 0:
            errors.append('blood was not cleared on fresh arena: '+name)
    if name == 'arena-defeat' and (state.get('state') != 5 or state.get('health') != 0):
        errors.append('real defeat missing from capture')
    if name in {'bow-first-draw', 'bow-third-draw'}:
        if (state.get('weapon') != 1 or state.get('drawing') is not True or
                state.get('third_person') is not (name == 'bow-third-draw') or
                type(state.get('draw')) not in (int, float) or not math.isfinite(state['draw']) or not .65 <= state['draw'] <= 1):
            errors.append('drawn bow/camera mismatch: '+name)
    if name == 'arrow-flight' and (state.get('shots', 0) < 1 or state.get('flying_arrows', 0) < 1):
        errors.append('actual flying arrow missing')
    if name == 'arrow-hit' and (state.get('shots', 0) < 1 or state.get('arrow_hits', 0) < 1):
        errors.append('physical arrow hit missing')
    if name in {'body-hit', 'arrow-hit'}:
        if state.get('body_bursts', 0) < 1 or state.get('blood_particles', 0) < 1 or state.get('material') != 1:
            errors.append('body blood effect missing: '+name)
        if state.get('body_audio', 0) < 1: errors.append('body impact sound missing: '+name)
        if state.get('blood_meshes', 0) < 1 or state.get('blood_mesh_draws', 0) < 1:
            errors.append('purchased body fluid missing: '+name)
        if state.get('blood_particles', 0) < 20: errors.append('initial blood burst too sparse: '+name)
    if name in {'gore-impact', 'finisher-blood'}:
        if (state.get('blood_scale') != .48 or state.get('blood_meshes', 0) < 2 or
                state.get('blood_secondary_draws', 0) < 1):
            errors.append('crossed heavy blood sprays missing: '+name)
    if name == 'finisher-blood' and (state.get('blood_live_finishers', 0) < 1 or
            state.get('blood_finisher_draws', 0) < 1 or state.get('blood_max_strength', 0) < 1.5):
        errors.append('actual finishing-hit fluid missing')
    if name in {'blood-fluid', 'blood-paused'}:
        if state.get('blood_meshes', 0) < 1 or state.get('blood_frame', 0) < 1 or state.get('blood_frame_transitions', 0) < 1:
            errors.append('purchased fluid animation missing: '+name)
        if state.get('blood_paused') is not (name == 'blood-paused'):
            errors.append('purchased fluid pause mismatch: '+name)
    if name == 'blood-stain' and (state.get('blood_stains', 0) < 1 or state.get('blood_stain_draws', 0) < 1 or state.get('defeated', 0) < 1):
        errors.append('persistent floor stain missing')
    if name == 'blood-persistent':
        if type(state.get('blood_witness_serial')) is not int or state['blood_witness_serial'] < 1:
            errors.append('original ground blood witness serial missing')
        if state.get('blood_witness_anchored') is not True:
            errors.append('original ground blood moved or disappeared')
        if not finite_number(state.get('blood_witness_age'), 30) or not finite_number(state.get('blood_oldest_stain_age'), 30):
            errors.append('ground blood witness/render age below thirty seconds')
        if fade != 1 or type(drawn) is not int or drawn < 1:
            errors.append('old ground blood not drawn at full opacity')
        if type(state.get('defeated')) is not int or state['defeated'] < 4:
            errors.append('ground blood persistence not observed across four challengers')
    if name == 'blood-settled':
        if any(type(state.get(key)) is not int or state[key] != 0 for key in ('blood_particles', 'blood_meshes', 'blood_staged_meshes')):
            errors.append('airborne blood remains after its settling deadline')
        if (type(state.get('blood_stains')) is not int or state['blood_stains'] < 1 or
                type(drawn) is not int or drawn < 1 or fade != 1):
            errors.append('settled blood leaves no fully visible floor splatter')
        if not finite_number(state.get('blood_settle_elapsed'), 1) or type(state.get('defeated')) is not int or state['defeated'] != 1:
            errors.append('settled blood not observed in the first completed duel')
    if name == 'body-hit' and (state.get('contact_kind') != 1 or state.get('damage_cue', 0) <= 0):
        errors.append('body damage cue missing')
    if name in {'metal-block', 'metal-parry'}:
        if state.get('metal_bursts', 0) < 1 or state.get('metal_particles', 0) < 1 or state.get('material') != 2:
            errors.append('metal spark effect missing: '+name)
        if state.get('contact_kind') != (4 if name == 'metal-parry' else 3) or state.get('damage_cue') != 0:
            errors.append('steel contact cue incorrect: '+name)
        if state.get('metal_audio', 0) < 1: errors.append('metal impact sound missing: '+name)
    if name == 'queue-progress' and not 1 <= state.get('defeated', 0) <= 7:
        errors.append('intermediate sequential progress missing')
    if name == 'arena-victory' and (state.get('state') != 4 or state.get('defeated') != 8 or state.get('yielded') != 8
                                    or state.get('health', 0) <= 0 or state.get('arrow_hits', 0) < 1):
        errors.append('eight actual victories with retained bow hit missing')
    if name == 'arena-setup' and (state.get('ui_phase') != 0 or state.get('state') != 0 or state.get('draft_total') != 8 or state.get('draft_simultaneous') != 1):
        errors.append('initial setup was bypassed')
    if name in CAPTURE_MEANINGS:
        selected, purse_gain = (1, 80) if name == 'arena-one-setup' else (8, 90)
        # The UI builds its setup draw list, then Start mutates the arena inside
        # that same draw. The JSON snapshot follows the handler. This must be
        # an exact fresh setup->Entering commit, never a later fighting frame.
        expected = dict(ui_phase=1, state=1, draft_total=selected,
            draft_simultaneous=selected, payout_preview=10*selected,
            total_opponents=selected, simultaneous_opponents=selected,
            active=0, entering=selected, waiting=0, yielded=0, defeated=0,
            unused=8-selected, health=100, stamina=100, weapon=0,
            shots=0, arrow_hits=0, flying_arrows=0, attack_serial=0,
            combo_links=0, combo_count=0, saved_reward=0)
        if (any((type(state.get(key)) not in (int, float) if key in {'health', 'stamina'} else type(state.get(key)) is not int) or state[key] != value for key, value in expected.items())
                or type(state.get('run_id')) is not int or state['run_id'] <= 0
                or state.get('reward_pending') is not False or state.get('drawing') is not False
                or type(state.get('initial_purse')) is not int
                or state.get('purse') != state.get('initial_purse', -1)+purse_gain):
            errors.append('fresh configured setup-to-fight commit evidence missing: '+name)
    if name in {'combo-first', 'combo-second', 'combo-third', 'combo-held'}:
        swing = {'combo-first': 1, 'combo-second': 2, 'combo-third': 3, 'combo-held': 3}[name]
        if total != 1 or simultaneous != 1 or state.get('attack_serial') != swing or state.get('combo_links') != swing-1:
            errors.append('fresh three-swing combo serial/link sequence missing: '+name)
        if not finite_number(state.get('stamina')) or (name != 'combo-held' and state['stamina'] > 100-16*swing+.1):
            errors.append('combo stamina was not paid per committed swing: '+name)
        if name != 'combo-held' and state.get('player_pose', 0) == 0: errors.append('animated combo pose absent: '+name)
        if name == 'combo-held' and state.get('combo_pending') is not False: errors.append('held third swing queued an extra attack')
    if name in {'arena-victory', 'arena-one-victory'}:
        expected_total, reward, gain = (8, 80, 80) if name == 'arena-victory' else (1, 10, 90)
        if total != expected_total or state.get('defeated') != expected_total or state.get('state') != 4 or state.get('reward_pending') is not False or state.get('saved_reward') != reward or state.get('purse') != state.get('initial_purse', -1)+gain:
            errors.append('genuine victory receipt/purse mismatch: '+name)
    if name == 'arena-many-active' and (total != 8 or simultaneous != 8 or state.get('active') != 8 or state.get('entering') != 0):
        errors.append('eight independent active opponents absent')
    if name == 'arena-many-defeat' and (total != 8 or simultaneous != 8 or state.get('state') != 5 or state.get('health') != 0 or state.get('concurrent_damage_sources', 0) < 2 or state.get('purse') != state.get('initial_purse', -1)+90 or state.get('saved_reward') != 0):
        errors.append('shared-player simultaneous defeat/purse evidence missing')
    if name in {'defeat-fall', 'defeat-grounded'}:
        if type(state.get('fallen_count')) is not int or not 1 <= state['fallen_count'] <= 8:
            errors.append('defeated fighter absent from collapse capture: '+name)
        if state.get('first_defeat_anchored') is not True:
            errors.append('defeated fighter moved from its resolved defeat position: '+name)
    if name == 'defeat-fall':
        age = state.get('first_defeat_age')
        if not finite_number(age, .75) or age > 1.3:
            errors.append('falling fighter captured outside collapse interval')
        if not finite_number(phase) or not 0 < phase < 1:
            errors.append('falling fighter is not in an actual intermediate pose')
    if name == 'defeat-grounded':
        if not finite_number(state.get('first_defeat_age'), 1.8) or phase != 1:
            errors.append('fighter has not completed its grounded collapse')
        if type(state.get('current')) is not int or not 1 <= state['current'] <= 7:
            errors.append('grounded fighter not retained for the next challenger')
    return errors


def isolate(source, target):
    inputs = isolate_project(source, target)
    for name in ('arena.config', 'playtest.route', 'CHARACTER_POLISH.json'):
        path = source/name
        if path.is_symlink() or not path.is_file() or not 8 <= path.stat().st_size <= 1024*1024:
            raise ValueError('Missing bounded arena runtime input: '+name)
        inputs[name] = sha(path); shutil.copyfile(path, target/name)
    if (target/'opening.config').exists():
        (target/'opening.config').unlink(); inputs.pop('opening.config', None)
    bank = source/'arena_assets'
    if not bank.is_dir(): raise ValueError('Prepared arena project lacks arena_assets')
    (target/'arena_assets').symlink_to(bank.resolve(strict=True), target_is_directory=True)
    for kind in ('body', 'arms'):
        for pose in range(3):
            name = f'arena_assets/bow_{kind}_{pose:03d}.glb'
            if not (source/name).is_file(): raise ValueError('Incomplete owned bow bank')
            inputs[name] = sha(source/name)
    blood = blood_asset_record(source/'blood_assets')
    (target/'blood_assets').symlink_to(blood['bank'], target_is_directory=True)
    for name, digest in blood['files_sha256'].items(): inputs['blood_assets/'+name] = digest
    from character_polish_assets import character_asset_record
    character = character_asset_record(source/'models'/'longsword_mocap')
    for name, digest in character['files_sha256'].items(): inputs['models/longsword_mocap/'+name] = digest
    return inputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scale', type=float, default=.8)
    parser.add_argument('--timeout', type=int, default=500)
    args = parser.parse_args(); binary = args.binary.resolve(strict=True); source = args.project.resolve(strict=True); out = args.output.resolve()
    if not binary.is_file() or not source.is_dir() or out == source or out.is_relative_to(source) or source.is_relative_to(out):
        parser.error('Supply an existing runtime/project and separate fresh evidence directory')
    if not .4 <= args.scale <= 1 or not 60 <= args.timeout <= 900: parser.error('Scale must be 0.4–1; timeout 60–900 seconds')
    out.mkdir(parents=True, exist_ok=False); before = mutable_snapshot(source); binary_before = sha(binary)
    project = out/'project'; inputs = isolate(source, project); captures = out/'captures'; captures.mkdir()
    profile = out/'profile.json'; final = out/'final.bmp'
    env, removed = native_environment(); env.pop('VALWOOD_SHOWCASE', None); env.pop('VALWOOD_CAPTURE_VIEW', None)
    env.update(VALWOOD_PRESENTATION='1', VALWOOD_PLAYTEST_SLICE='city', VALWOOD_COMBAT_ARENA='1',
               VALWOOD_QA_ARENA='1', VALWOOD_QA_ARENA_CAPTURE_DIR=str(captures),
               VALWOOD_QA_FRAME_TIMES_ONLY='1', SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
    command = [str(binary), '--project', str(project), '--play', '--scenic', 'city-walk', '--frames', '15000',
               '--warmup', '120', '--width', '1280', '--height', '720', '--auto-quality', 'off',
               '--render-scale', str(args.scale), '--profile', str(profile), '--capture', str(final)]
    (out/'command.json').write_text(json.dumps(dict(command=command, removed_environment=removed,
        environment={name:value for name,value in env.items() if name.startswith('VALWOOD_')}), indent=2)+'\n')
    (out/'source-before.json').write_text(json.dumps(before, indent=2)+'\n')
    (out/'isolated-inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
    started = time.monotonic()
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with (out/'native.log').open('w') as log:
            try: code = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout).returncode
            except subprocess.TimeoutExpired: code = -1
    world_path = Path(str(profile)+'.world.json'); world = json.loads(world_path.read_text()) if world_path.is_file() else {}
    metrics = json.loads(profile.read_text()) if profile.is_file() else {}; errors = qa_failures(world)
    if code != 0: errors.append('native process exit '+str(code))
    renderer = str(metrics.get('renderer', ''))
    if not renderer or any(word in renderer.lower() for word in ('llvmpipe', 'softpipe', 'software', 'swrast')):
        errors.append('native GPU renderer missing')
    images = {}
    for name in sorted(CAPTURES | {'final'}):
        path = final if name == 'final' else captures/(name+'.bmp')
        if not path.is_file(): errors.append('rendered capture missing: '+name); continue
        image = Image.open(path).convert('RGB'); target = out/'final.png' if name == 'final' else captures/(name+'.png')
        if image.size != (1280, 720) or max(ImageStat.Stat(image).var) < 8: errors.append('blank/invalid image: '+name)
        image.save(target); images[name] = dict(path=str(target), sha256=sha(target))
        if name != 'final':
            state_path = captures/(name+'.json')
            try: state = json.loads(state_path.read_text())
            except (OSError, ValueError): errors.append('capture state missing: '+name)
            else:
                errors += capture_failures(name, state); images[name].update(state=state, state_sha256=sha(state_path))
                if name in CAPTURE_MEANINGS: images[name]['meaning'] = CAPTURE_MEANINGS[name]
    unchanged = before == mutable_snapshot(source)
    inputs_unchanged = all((project/name).is_file() and sha(project/name) == value for name,value in inputs.items())
    binary_unchanged = sha(binary) == binary_before
    if not unchanged: errors.append('source project or user saves changed')
    if not inputs_unchanged: errors.append('isolated inputs or shared weapon/blood banks changed')
    if not binary_unchanged: errors.append('runtime changed during rehearsal')
    result = dict(passed=not errors, failures=errors, exit=code, elapsed_seconds=round(time.monotonic()-started, 2),
        binary=str(binary), binary_sha256=binary_before, source_project=str(source), isolated_project=str(project),
        renderer=renderer, arena_qa=world.get('arena_qa'), ground_blood_persistence=world.get('ground_blood_persistence'),
        combat_blood_motion=world.get('combat_blood_motion'),
        captures=images, source_state_unchanged=unchanged,
        isolated_inputs_unchanged=inputs_unchanged, binary_unchanged=binary_unchanged,
        timing=timings(profile.with_suffix('.json.csv')),
        scope='Actual SDL virtual controller and ordinary keyboard actions; setup and result navigation, real defeat/retry, physical arrows, eight sequential victories, a paid three-cut combo and one-opponent victory, exactly ninety saved coins, and eight concurrent independent attackers causing defeat. Configured setup captures show the same-frame setup-to-fight commit. No health/player/AI/progress field writes; physical controller hardware and sustained whole-game performance not certified.')
    if result['timing']:
        result['timing']['scope'] = 'Bounded arena workflow including loading and effect captures; not sustained whole-game performance.'
    (out/'results.json').write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    print(json.dumps(dict(passed=result['passed'], failures=errors, checks=world.get('arena_qa', {}).get('checks'), result=str(out/'results.json')), indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__': raise SystemExit(main())
