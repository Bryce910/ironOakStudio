"""Exercise water-only publication rejection paths in isolated directories."""
from pathlib import Path
import importlib.util
import json
import tempfile
import unittest
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent

class ShaderScopeTests(unittest.TestCase):
    def setUp(self):
        spec=importlib.util.spec_from_file_location('river_swim_shader_scope_test',HERE/'check_unified.py')
        self.c=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.c)
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        root=Path(self.tmp.name);self.c.N=root/'candidate';self.c.BASE=root/'base'
        for generation in (self.c.N,self.c.BASE):
            for folder in ('review','shared/shaders','source-v1/shaders'):(generation/folder).mkdir(parents=True)
            for name in ('world_water.fs.glsl','world_water.vs.glsl','world_material_response.glsl'):
                for folder in ('shared/shaders','source-v1/shaders'):(generation/folder/name).write_text('baseline '+name+'\n')
        for folder in ('shared/shaders','source-v1/shaders'):
            (self.c.N/folder/'world_water.fs.glsl').write_text('reviewed water derivative\n')
        self.scope=dict(schema='valwood.river-swim.shader-change-scope.v1',reviewed=True,root_freeze_authorized=True,
                        shader_changes=['world_water.fs.glsl'],
                        baseline_sha256={'world_water.fs.glsl':self.c.sha(self.c.BASE/'shared/shaders/world_water.fs.glsl')},
                        candidate_sha256={'world_water.fs.glsl':self.c.sha(self.c.N/'shared/shaders/world_water.fs.glsl')})
        self.write_scope()
    def write_scope(self):
        (self.c.N/'review/SHADER_CHANGE_SCOPE.json').write_text(json.dumps(self.scope))
    def rejected(self):
        with self.assertRaises(RuntimeError):self.c.shader_scope()
    def test_exact_owned_derivative_passes(self):self.c.shader_scope()
    def test_unreviewed_rejected(self):
        self.scope['reviewed']=False;self.write_scope();self.rejected()
    def test_missing_root_freeze_rejected(self):
        self.scope['root_freeze_authorized']=False;self.write_scope();self.rejected()
    def test_source_shared_mismatch_rejected(self):
        (self.c.N/'source-v1/shaders/world_water.fs.glsl').write_text('other final source');self.rejected()
    def test_unrelated_shader_change_rejected(self):
        (self.c.N/'shared/shaders/world_water.vs.glsl').write_text('unreviewed vertex');self.rejected()
    def test_extra_shader_rejected(self):
        (self.c.N/'shared/shaders/extra.glsl').write_text('unreviewed file');self.rejected()
    def test_missing_shader_rejected(self):
        (self.c.N/'shared/shaders/world_water.vs.glsl').unlink();self.rejected()
    def test_no_water_delta_rejected(self):
        p=self.c.N/'shared/shaders/world_water.fs.glsl';p.write_bytes((self.c.BASE/'shared/shaders/world_water.fs.glsl').read_bytes());self.rejected()
    def test_water_alias_rejected(self):
        p=self.c.N/'shared/shaders/world_water.fs.glsl';p.unlink();p.symlink_to(self.c.N/'source-v1/shaders/world_water.fs.glsl');self.rejected()
    def test_source_alias_rejected(self):
        p=self.c.N/'source-v1/shaders/world_water.fs.glsl';p.unlink();p.symlink_to(self.c.N/'shared/shaders/world_water.fs.glsl');self.rejected()
    def test_changed_baseline_rejected(self):
        (self.c.BASE/'source-v1/shaders/world_water.fs.glsl').write_text('unexpected baseline');self.rejected()
    def test_changed_pin_rejected(self):
        self.scope['candidate_sha256']['world_water.fs.glsl']='0'*64;self.write_scope();self.rejected()
    def test_scope_expansion_rejected(self):
        self.scope['shader_changes'].append('world_water.vs.glsl');self.write_scope();self.rejected()

if __name__=='__main__':unittest.main(verbosity=2)
