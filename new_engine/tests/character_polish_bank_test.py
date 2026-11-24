"""Independent checks against the actual owned, offline-cooked candidate bank."""
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import sys
import unittest

import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from character_polish_bank import BASE,DEFAULT,read_glb,attribute,view


def points(path,name='POSITION'):
    doc,blob=read_glb(path);accessor,data=attribute(doc,blob,name)
    return np.frombuffer(data,dtype='<f4',count=accessor['count']*(4 if name=='TANGENT'else 3)).reshape(accessor['count'],-1)


class ActualCharacterBank(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bank=DEFAULT;cls.manifest=json.loads((DEFAULT/'combat_motion_manifest.json').read_text())
        cls.records=cls.manifest['geometry_measurements']

    def test_complete_layout_and_backward_compatible_socket_prefix(self):
        self.assertEqual(self.manifest['pose_count'],172)
        self.assertEqual(len(self.manifest['assets']),344)
        self.assertEqual([(x['name'],x['base'],x['count'])for x in self.manifest['clips'][-6:]],
            [('hit_left',108,8),('hit_right',116,8),('hit_high',124,8),('hit_low',132,8),('hit_thrust',140,8),('death_back',148,24)])
        raw=(self.bank/'combat_weapon_sockets.bin').read_bytes()
        self.assertEqual(struct.unpack_from('<8sI',raw),(b'VWMOT172',172));self.assertEqual(len(raw),4140)
        actual=np.frombuffer(raw,dtype='<f4',offset=12).reshape(172,2,3)
        original=[]
        for line in (BASE/'world_combat_weapon_sockets.inc').read_text().splitlines():
            values=[float(x)for x in re.findall(r'(-?\d+\.\d+)f',line)]
            if len(values)==6:original.append(values)
        self.assertTrue(np.array_equal(actual[:108].reshape(108,6),np.asarray(original,dtype='<f4')))
        self.assertTrue(np.isfinite(actual).all());self.assertLess(float(np.abs(actual).max()),5)

    def test_original_attacks_guards_arms_and_topology_unchanged(self):
        for kind in ('body','arms'):
            for slot in range(108):
                name=f'combat_{kind}_{slot:03}.glb';old,old_blob=read_glb(BASE/name);new,new_blob=read_glb(self.bank/name)
                for attr in ('POSITION','NORMAL','TANGENT','TEXCOORD_0','indices'):
                    self.assertEqual(attribute(old,old_blob,attr),attribute(new,new_blob,attr),(kind,slot,attr))
            template,blob=read_glb(self.bank/f'combat_{kind}_000.glb')
            for slot in range(108,172):
                current,data=read_glb(self.bank/f'combat_{kind}_{slot:03}.glb')
                for attr in ('TEXCOORD_0','indices'):
                    self.assertEqual(attribute(template,blob,attr),attribute(current,data,attr),(kind,slot,attr))

    def test_death_is_a_full_fall_with_fixed_origin_and_ground_hold(self):
        death=[x for x in self.records if x['clip']=='death_back']
        self.assertEqual(len(death),24);self.assertEqual(self.manifest['death_seconds'],1.8)
        start,end=death[0],death[-1]
        self.assertGreater(start['bones']['Hips'][1]-end['bones']['Hips'][1],.5)
        self.assertGreater(abs(end['bones']['Hips'][2]-start['bones']['Hips'][2]),1.)
        self.assertLess(end['bounds_max'][1],.75)
        self.assertLess(end['bones']['Head'][1],.5)
        for item in death:self.assertGreaterEqual(item['bounds_min'][1],-.00001)
        a=points(self.bank/'combat_body_000.glb');b=points(self.bank/'combat_body_148.glb')
        self.assertLess(float(np.max(np.abs(a-b))),.002)

    def test_reactions_keep_both_hands_reachable_and_stationary_feet(self):
        for name,base in [('hit_left',108),('hit_right',116),('hit_high',124),('hit_low',132),('hit_thrust',140)]:
            items=[x for x in self.records if x['clip']==name]
            self.assertEqual(len(items),8)
            for item in items:
                self.assertLess(item['left_grip_error_m'],.003,name)
                self.assertLess(item['ankle_error_m'],.003,name)
                for foot in ('LeftFoot','RightFoot'):
                    self.assertLess(float(np.linalg.norm(np.asarray(item['bones'][foot])-np.asarray(items[-1]['bones'][foot]))),.01,name)
            self.assertLess(float(np.max(np.abs(points(self.bank/f'combat_body_{base+7:03}.glb')-points(self.bank/'combat_body_000.glb')))),.002,name)
        neutral=next(x for x in self.records if x['clip']=='hit_left'and x['t']==1)['bones']['Head']
        left=next(x for x in self.records if x['slot']==108)['bones']['Head']
        right=next(x for x in self.records if x['slot']==116)['bones']['Head']
        self.assertGreater(left[0]-neutral[0],.07);self.assertLess(right[0]-neutral[0],-.15)

    def test_shared_source_pbr_and_image_mime_are_correct(self):
        baseline,blob=read_glb(BASE/'combat_body_000.glb')
        albedo=view(baseline,blob,baseline['images'][0]['bufferView'])
        self.assertEqual(hashlib.sha256(albedo).hexdigest(),'8d927e617242c737a4e5497fa4311b7203d5ca87786d39f0489a4bc4cda4fcc7')
        hashes=None
        for slot in (0,75,83,108,132,148,171):
            for kind in ('body','arms'):
                doc,blob=read_glb(self.bank/f'combat_{kind}_{slot:03}.glb')
                current=[]
                for i,image in enumerate(doc['images']):
                    data=view(doc,blob,image['bufferView']);current.append(hashlib.sha256(data).hexdigest())
                    with Image.open(io.BytesIO(data))as decoded:
                        self.assertEqual(decoded.size,(2048,2048))
                        self.assertEqual(image['mimeType'],'image/jpeg'if decoded.format=='JPEG'else'image/png')
                    if i==0:self.assertEqual(data,albedo)
                if hashes is None:hashes=current
                self.assertEqual(current,hashes)
                pbr=doc['materials'][0]['pbrMetallicRoughness']
                self.assertEqual(pbr['metallicFactor'],1);self.assertEqual(pbr['roughnessFactor'],1)

    def test_actual_geometry_normals_tangents_and_original_sources_are_valid(self):
        for item in self.manifest['assets']:
            path=self.bank/item['file'];p=points(path);n=points(path,'NORMAL');t=points(path,'TANGENT')
            self.assertTrue(np.isfinite(p).all());self.assertTrue(np.isfinite(n).all());self.assertTrue(np.isfinite(t).all())
            self.assertLess(float(np.max(np.abs(np.linalg.norm(n,axis=1)-1))),.002)
            self.assertLess(float(np.max(np.abs(np.sum(n*t[:,:3],axis=1)))),.002)
        for name,digest in self.manifest['extension_source_hashes'].items():
            with Path(name).open('rb')as stream:self.assertEqual(hashlib.file_digest(stream,'sha256').hexdigest(),digest)


if __name__=='__main__':unittest.main()
