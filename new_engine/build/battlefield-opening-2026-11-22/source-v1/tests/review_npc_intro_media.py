#!/usr/bin/env python3
"""Synthetic offline-import + real SDL dummy-mixer/streaming regression proof."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import wave
import struct
import math

SOURCE = Path(__file__).resolve().parents[1]
STAGE = SOURCE.parent
OUT = STAGE/'media-review'
IMPORT = SOURCE/'tools/import_npc_intro_media.py'
RUNNER = OUT/'world_npc_intro_media_test'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(command, success=True):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True, timeout=120)
    if (result.returncode == 0) != success:
        raise AssertionError(f'{command}: {result.returncode}\n{result.stdout}\n{result.stderr}')
    return result


def main():
    OUT.mkdir(exist_ok=True)
    fixture = OUT/'synthetic'
    fixture.mkdir(exist_ok=True)
    source = fixture/'supplied.mp4'
    command = ['ffmpeg', '-v', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i',
               'testsrc2=size=320x180:rate=12:duration=1.25', '-itsoffset', '0.25',
               '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=1',
               '-t', '1.25', '-c:v', 'libx264', '-threads', '2', '-pix_fmt', 'yuv420p',
               '-c:a', 'aac', '-movflags', '+faststart', source]
    run(command)
    captions = fixture/'supplied.srt'
    captions.write_text('1\n00:00:00,100 --> 00:00:00,600\nA supplied line.\nSecond line.\n\n2\n00:00:00,650 --> 00:00:01,100\nThe next line.\n')
    projects = {}
    for mode in ('video', 'voice'):
        project = fixture/mode
        project.mkdir(exist_ok=True)
        result = run([sys.executable, IMPORT, source, '--mode', mode, '--project', project, '--subtitles', captions])
        projects[mode] = {'path': project, 'result': json.loads(result.stdout)}
    video = projects['video']['result'];voice = projects['voice']['result']
    assert video['dimensions'] == [320, 180] and video['frames'] == 15
    assert video['duration'] == voice['duration'] == 1.25
    wav = Path(video['generation'])/'audio.wav'
    with wave.open(str(wav), 'rb') as clip:
        assert (clip.getnchannels(), clip.getsampwidth(), clip.getframerate(), clip.getnframes()) == (2, 2, 48000, 60000)
        pcm = struct.unpack('<' + 'h'*(clip.getnframes()*2), clip.readframes(clip.getnframes()))
    rms = lambda values: math.sqrt(sum(v*v for v in values)/len(values))
    initial_rms = rms(pcm[:int(.18*48000)*2]);voiced_rms = rms(pcm[int(.35*48000)*2:int(.55*48000)*2])
    assert initial_rms < 3 and voiced_rms > 500, (initial_rms, voiced_rms)
    raw = run([RUNNER, projects['video']['path'], projects['voice']['path'], fixture/'missing'])
    runtime = json.loads(raw.stdout)
    failures = []
    def broken(label, change):
        project = fixture/label
        if project.exists():shutil.rmtree(project)
        shutil.copytree(projects['video']['path'], project)
        generation = next(p for p in (project/'opening/npc_intro').iterdir() if p.name == Path(video['generation']).name)
        change(project, generation)
        result = run([RUNNER, '--inspect', project], success=False)
        failures.append({'case': label, 'returncode': result.returncode, 'error': result.stderr.strip()})
    broken('truncated-pack', lambda p,g: (g/'frames.pack').write_bytes((g/'frames.pack').read_bytes()[:100]))
    def invalid_jpeg(project, generation):
        lines = (generation/'frames.idx').read_text().splitlines();item = lines[-3].split();offset, size = map(int, item[-2:])
        with (generation/'frames.pack').open('r+b') as handle:handle.seek(offset);handle.write(b'X'*min(size, 40))
    broken('corrupt-late-jpeg', invalid_jpeg)
    def traversal(project, generation):
        path = project/'opening/npc_intro/media.cfg';text = path.read_text();text=text.replace(Path(video['generation']).name+'/audio.wav', '../audio.wav');path.write_text(text)
    broken('path-traversal', traversal)
    def symlink(project, generation):
        path = generation/'audio.wav';path.unlink();path.symlink_to(wav)
    broken('symlink-audio', symlink)
    def invalid_index(project, generation):
        path=generation/'frames.idx';lines=path.read_text().splitlines();parts=lines[6].split();parts[-2]='0';lines[6]=' '.join(parts);path.write_text('\n'.join(lines)+'\n')
    broken('overlapping-frame-index', invalid_index)
    def bad_wav(project, generation):
        path=generation/'audio.wav';data=bytearray(path.read_bytes());at=data.find(b'fmt ');assert at>=0;data[at+8:at+10]=struct.pack('<H',17);path.write_bytes(data)
    broken('compressed-wav-rejected', bad_wav)
    before=(projects['video']['path']/'opening/npc_intro/media.cfg').read_bytes()
    invalid=fixture/'not-video.mp4';invalid.write_bytes(b'not a movie')
    run([sys.executable, IMPORT, invalid, '--mode', 'video', '--project', projects['video']['path']], success=False)
    assert before == (projects['video']['path']/'opening/npc_intro/media.cfg').read_bytes()
    bad_captions=fixture/'overlap.srt';bad_captions.write_text('1\n00:00:00,100 --> 00:00:00,600\nOne\n\n2\n00:00:00,500 --> 00:00:01,100\nOverlap\n')
    run([sys.executable, IMPORT, source, '--mode', 'video', '--project', projects['video']['path'], '--subtitles', bad_captions], success=False)
    assert before == (projects['video']['path']/'opening/npc_intro/media.cfg').read_bytes()
    source4k=fixture/'native4k.mp4';run(['ffmpeg','-v','error','-nostdin','-y','-f','lavfi','-i','color=c=navy:size=3840x2160:rate=30:duration=0.1','-an','-c:v','libx264','-threads','2','-pix_fmt','yuv420p',source4k])
    project4k=fixture/'4k';project4k.mkdir(exist_ok=True)
    imported4k=json.loads(run([sys.executable,IMPORT,source4k,'--mode','video','--project',project4k]).stdout)
    assert imported4k['dimensions']==[3840,2160] and imported4k['audio_present'] is False
    run([RUNNER,'--inspect',project4k])
    inputs={str(p):sha(p) for p in [IMPORT,Path(__file__),SOURCE/'engine/world_npc_intro_media.h',SOURCE/'tests/world_npc_intro_media_test.cpp',RUNNER,source,captions,source4k]}
    outputs={str(p):sha(p) for mode in projects for p in projects[mode]['path'].rglob('*') if p.is_file()}
    outputs.update({str(p):sha(p) for p in project4k.rglob('*') if p.is_file()})
    report={'schema':'valwood.npc-intro-media.cpu-review.v1','passed':True,'runtime':runtime,
            'source_audio_offset_seconds':.25,'initial_silence_rms':initial_rms,'voiced_rms':voiced_rms,
            'negative_cases':failures,'failed_imports_preserve_previous_configuration':True,
            'native_4k':imported4k,'runtime_gl_used':False,'runtime_subprocesses':False,
            'inputs':inputs,'outputs':outputs}
    path=OUT/'NPC_INTRO_MEDIA_CPU_REVIEW.json';path.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'passed':True,'receipt':str(path),'runtime':runtime,'negative_cases':len(failures)},indent=2))


if __name__=='__main__':main()
