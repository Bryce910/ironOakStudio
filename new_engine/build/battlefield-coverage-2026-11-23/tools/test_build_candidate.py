#!/usr/bin/env python3
"""Small fixture tests; never compiles the engine or touches published inputs."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('candidate_builder',Path(__file__).with_name('build_candidate.py'))
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)

class BuilderTests(unittest.TestCase):
 def test_actor_exactly_once(self):
  actor='/x/runtime-native-v1/native-objects/engine/world_battlefield_actors.cpp.o'
  self.assertEqual(b.validate_objects(['c++',actor,'-o','bin']),[actor])
  for entries in ([],[actor,actor],['x.o']):
   with self.assertRaises(RuntimeError):b.validate_objects(entries)
 def test_exact_command_reroot(self):
  g=Path('/old');s=Path('/new/source-v1');o=Path('/new/runtime-native-v1')
  cmd=['g++','-O2','-I/old/source-v1/engine','/old/source-v1/engine/world_battlefield_actors.cpp','-o','/old/runtime-native-v1/a.o','-lz']
  got=b.reroot(cmd,g,s,o)
  self.assertEqual(got,['g++','-O2','-I/new/source-v1/engine','/new/source-v1/engine/world_battlefield_actors.cpp','-o','/new/runtime-native-v1/a.o','-lz'])
 def test_cache_requires_byte_identity(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);obj=root/'a.o';dep=root/'x.h';obj.write_bytes(b'object');dep.write_bytes(b'header');cmd=['cc','-c','x.c']
   record=dict(command=cmd,dependencies={str(dep):b.sha(dep)},object_sha256=b.sha(obj))
   self.assertTrue(b.cache_valid(obj,record,cmd))
   obj.write_bytes(b'changed');self.assertFalse(b.cache_valid(obj,record,cmd));obj.write_bytes(b'object')
   dep.write_bytes(b'changed');self.assertFalse(b.cache_valid(obj,record,cmd))
   self.assertFalse(b.cache_valid(obj,{**record,'object_sha256':None},cmd))
   self.assertFalse(b.cache_valid(obj,{**record,'dependencies':{}},cmd))
 def test_snapshot_catches_add_remove(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);a=root/'a.h';a.write_text('a');first=b.snapshot(root)
   (root/'new.h').write_text('new');self.assertNotEqual(first,b.snapshot(root));(root/'new.h').unlink();self.assertEqual(first,b.snapshot(root))
   a.unlink();self.assertNotEqual(first,b.snapshot(root))
 def test_dependency_rule_ignores_mp_stubs(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/'a.d';p.write_text('/out/a.o: /source/a.cpp \\\n /source/a.h\n/source/a.h:\n')
   self.assertEqual(b.depfiles(p),['/source/a.cpp','/source/a.h'])
 def test_source_dependency_mapping(self):
  self.assertEqual(b.dependency_current('/old/source-v1/e.h',Path('/new/source-v1')),Path('/new/source-v1/e.h'))
  self.assertEqual(b.dependency_current('/usr/include/e.h',Path('/new/source-v1')),Path('/usr/include/e.h'))
 def test_unbuilt_candidate_refuses_launch(self):
  n=Path(__file__).resolve().parents[1]
  spec=importlib.util.spec_from_file_location('unified_check',n/'check_unified.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  with tempfile.TemporaryDirectory() as td:
   m.N=Path(td);(m.N/'READY_INPUTS.json').write_text(json.dumps(dict(build_state='unbuilt',binary_sha256=None)))
   with self.assertRaisesRegex(RuntimeError,'unbuilt'):m.check()
 def test_plan_binds_published_generation(self):
  bound,receipt=b.load_baseline();plan,jobs,source=b.make_plan(bound,receipt)
  self.assertEqual(Path(bound['generation']).name,'battlefield-opening-2026-11-22')
  self.assertEqual(plan['baseline_object_count'],109)
  self.assertEqual(plan['battlefield_actor_objects'],1)
  self.assertFalse(plan['compilation_performed'])
  outputs=[j['output'] for j in jobs]
  self.assertEqual(len(outputs),len(set(outputs)))

if __name__=='__main__':unittest.main()
