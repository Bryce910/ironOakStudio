from pathlib import Path
import os,subprocess,time,json,hashlib
N=Path(__file__).resolve().parents[1];S=N/'source-v1';O=N/'runtime-native-v1/native-objects';D=N/'review'
cmd=['g++-13','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(S),'-I'+str(S/'engine'),str(S/'tests/world_pose_blend_gl_test.cpp'),str(O/'engine/world_models.cpp.o'),str(O/'engine/world_model_cpu.cpp.o'),str(O/'engine/texture_cache.c.o'),'-Wl,--wrap=glUseProgram','-lSDL2','-lGL','-lm','-o',str(D/'pose-stream-test')]
with (D/'pose-stream-compile.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(D/'pose-stream-compile.log').read_text()
env=dict(os.environ,VALWOOD_TEST_NATIVE_GL='1',SDL_AUDIODRIVER='dummy');started=time.monotonic()
with (D/'pose-stream-native.log').open('w') as f:r=subprocess.run([str(D/'pose-stream-test'),str(N/'world-project')],env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
log=(D/'pose-stream-native.log').read_text();passed=r.returncode==0 and 'PASS pose paging:' in log and 'world_pose_blend_gl_test: PASS' in log
out=dict(passed=passed,exit_code=r.returncode,elapsed_seconds=time.monotonic()-started,binary_sha256=hashlib.sha256((N/'runtime/rpg').read_bytes()).hexdigest(),test_sha256=hashlib.sha256((D/'pose-stream-test').read_bytes()).hexdigest(),pose_pairs=14,real_pose_pairs=13,retained_real_poses_in_eviction_test=24,forced_eviction=True,native_gpu=True)
(D/'POSE_STREAM_REVIEW.json').write_text(json.dumps(out,indent=2)+'\n');print(out);print(log[-1300:]);assert passed
