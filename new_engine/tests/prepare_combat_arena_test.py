"""Meaningful CPU checks: source saves/banks are isolated; unsafe targets refused."""
import hashlib,json,os,struct,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.dont_write_bytecode=True;sys.path.insert(0,str(ROOT/'tools'))
import prepare_combat_arena as arena
class ArenaPreparationTest(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.engine=Path(self.tmp.name)/'engine';self.engine.mkdir();self.source=self.engine/'projects/city';self.source.mkdir(parents=True);self.output=self.engine/'build/combat-arena/candidate'
  for n in ['project.valwood','world.studio','graphics.settings','city.scene','city.sky','city_showcase.json','publisher_showcase.json','playtest.route']:(self.source/n).write_text('fixture '+n)
  for n in ['models','ground','scenery','audio','shaders','ai','saves']:(self.source/n).mkdir()
  for n in ['models/asset.glb','ground/showcase.height','ground/texture.png','scenery/instances.txt','audio/sound.wav','shaders/source.glsl','ai/npc.json','saves/progress.sav']:(self.source/n).write_text('keep '+n)
  self.binary=self.engine/'fake-runtime';self.binary.write_bytes(b'fake executable');self.binary.chmod(0o755)
 def tearDown(self):self.tmp.cleanup()
 def snapshot(self,p):return {str(q.relative_to(p)):q.read_bytes() for q in p.rglob('*') if q.is_file()}
 def prepare(self):return arena.prepare(self.engine,self.source,self.output)
 def test_private_state_and_linked_banks_leave_source_bytes_unchanged(self):
  before=self.snapshot(self.source);d=self.prepare();p=self.output/'project'
  self.assertEqual(before,self.snapshot(self.source));self.assertEqual(list((p/'saves').iterdir()),[])
  self.assertTrue((p/'models').is_symlink());self.assertFalse((p/'ground').is_symlink());self.assertFalse((p/'ground/showcase.height').is_symlink())
  self.assertEqual((p/'ground/source-city.height').read_bytes(),before['ground/showcase.height'])
  for name in ['graphics.settings','world.studio','ai/npc.json','shaders/source.glsl']:
   self.assertFalse((p/name).is_symlink());self.assertEqual((p/name).stat().st_nlink,1);(p/name).write_text('local change')
  (p/'saves/progress.sav').write_bytes(b'local progress');self.assertEqual(before,self.snapshot(self.source));self.assertEqual(d['shared_bank_bytes_copied'],0)
 def test_floor_support_has_exact_samples_for_spawn_queue_and_entire_fight_area(self):
  raw=arena.flat_heightfield();magic,w,h,x,z,sx,sz=struct.unpack('<8sIIdddd',raw[:48]);values=struct.unpack('<'+str(w*h)+'f',raw[48:]);self.assertEqual(magic,b'VWHGT1\0\0');self.assertEqual(set(values),{64.})
  c=arena.contract();points=c['entry_path']+c['queue_positions']+[c['enemy_spawn'],c['player_spawn'][:3]]
  for px,py,pz in points:self.assertTrue(x<=px<=x+(w-1)*sx and z<=pz<=z+(h-1)*sz)
  self.assertEqual(len(c['queue_positions']),8);self.assertEqual(len({tuple(p) for p in c['queue_positions']}),8)
 def test_launcher_refuses_unbound_or_changed_runtime_and_generated_data(self):
  self.prepare()
  with self.assertRaisesRegex(ValueError,'not bound'):arena.validate_launch(self.output)
  arena.bind_runtime(self.output,self.binary);self.assertEqual(arena.validate_launch(self.output),str(self.binary))
  self.binary.write_bytes(b'changed')
  with self.assertRaisesRegex(ValueError,'runtime changed'):arena.validate_launch(self.output)
  self.binary.write_bytes(b'fake executable');(self.output/'project/playtest.route').write_text('changed')
  with self.assertRaisesRegex(ValueError,'data changed'):arena.validate_launch(self.output)
 def test_existing_output_and_source_output_are_rejected_without_changes(self):
  self.prepare();before=self.snapshot(self.output)
  with self.assertRaisesRegex(ValueError,'Existing output'):self.prepare()
  self.assertEqual(before,self.snapshot(self.output))
  with self.assertRaisesRegex(ValueError,'beneath'):arena.prepare(self.engine,self.source,self.source/'bad')
 def test_dotdot_cannot_escape_build_boundary(self):
  with self.assertRaisesRegex(ValueError,'beneath'):arena.prepare(self.engine,self.source,self.engine/'build/combat-arena/candidate/../../outside')
  self.assertFalse((self.engine/'build/outside').exists())
 def test_symlink_output_and_save_alias_are_rejected(self):
  parent=self.engine/'build/combat-arena';parent.mkdir(parents=True);target=parent/'real';target.mkdir();(parent/'alias').symlink_to(target,target_is_directory=True)
  with self.assertRaisesRegex(ValueError,'symlink'):arena.prepare(self.engine,self.source,parent/'alias/candidate')
  self.prepare();arena.bind_runtime(self.output,self.binary);saves=self.output/'project/saves';saves.rmdir();saves.symlink_to(self.source/'saves',target_is_directory=True)
  with self.assertRaisesRegex(ValueError,'Private saves'):arena.validate_launch(self.output)
 def blood_bank(self):
  bank=self.engine/'blood';bank.mkdir();(bank/'blood.vwb').write_bytes(b'cooked purchased source fixture')
  manifest=dict(schema='valwood.volumetric-blood-assets',version=1,ready=True,assets=[dict(file='blood.vwb',sha256=arena.sha(bank/'blood.vwb'))])
  (bank/'BLOOD_ASSETS.json').write_text(json.dumps(manifest));return bank
 def test_purchased_blood_bound_before_runtime_and_guarded_at_launch(self):
  self.prepare();bank=self.blood_bank();record=arena.bind_blood(self.output,bank)
  self.assertEqual(record['bank'],str(bank));self.assertTrue((self.output/'project/blood_assets').is_symlink())
  arena.bind_runtime(self.output,self.binary);self.assertEqual(arena.validate_launch(self.output),str(self.binary))
  (bank/'blood.vwb').write_bytes(b'changed')
  with self.assertRaisesRegex(ValueError,'blood asset changed'):arena.validate_launch(self.output)
 def test_purchased_blood_refuses_unlisted_files_and_unsafe_names(self):
  bank=self.blood_bank();manifest=bank/'BLOOD_ASSETS.json';original=manifest.read_text()
  (bank/'unlisted.bin').write_bytes(b'not cooked')
  with self.assertRaisesRegex(ValueError,'Unlisted'):arena.blood_asset_record(bank)
  (bank/'unlisted.bin').unlink()
  for name in ['../other.vwb','sub/file.vwb','sub\\file.vwb','BLOOD_ASSETS.json']:
   data=json.loads(original);data['assets'][0]['file']=name;manifest.write_text(json.dumps(data))
   with self.assertRaisesRegex(ValueError,'Unsafe'):arena.blood_asset_record(bank)
 def test_purchased_blood_refuses_symlinked_source_and_repeat_binding(self):
  self.prepare();bank=self.blood_bank();saved=(bank/'blood.vwb').read_bytes();(bank/'blood.vwb').unlink();(bank/'blood.vwb').symlink_to(self.binary)
  with self.assertRaisesRegex(ValueError,'ordinary'):arena.bind_blood(self.output,bank)
  (bank/'blood.vwb').unlink();(bank/'blood.vwb').write_bytes(saved);arena.bind_blood(self.output,bank)
  with self.assertRaisesRegex(ValueError,'already bound'):arena.bind_blood(self.output,bank)
if __name__=='__main__':unittest.main()
