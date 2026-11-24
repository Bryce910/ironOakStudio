"""CPU-only delivered-project selection; never starts a game or Windows process."""
from pathlib import Path
import sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from windows_artist_actions import editor_command

class ArtistLaunchTest(unittest.TestCase):
    def test_delivered_project_defaults_and_explicit_view(self):
        with tempfile.TemporaryDirectory(prefix='valwood-artist-launch-')as name:
            root=Path(name);project=root/'project';project.mkdir()
            self.assertEqual(editor_command(root,[])[-2:],['--scenic','castle-finished-arcade'])
            (project/'jungle_expansion.json').write_text('{}')
            self.assertEqual(editor_command(root,[])[-1],'rainforest-milestone')
            (project/'stronghold.json').write_text('{}')
            self.assertEqual(editor_command(root,[])[-1],'stronghold')
            (project/'publisher_showcase.json').write_text('{}')
            self.assertEqual(editor_command(root,[])[-1],'stronghold-town')
            (project/'city_showcase.json').write_text('{}')
            self.assertEqual(editor_command(root,[])[-1],'city-walk')
            command=editor_command(root,['--scenic','city-camera-11'])
            self.assertEqual(command.count('--scenic'),1)
            self.assertEqual(command[-1],'city-camera-11')

if __name__=='__main__':unittest.main()
