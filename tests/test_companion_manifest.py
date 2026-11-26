import json
from pathlib import Path
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]
class ManifestTests(unittest.TestCase):
    def manifest(self,*args):
        return json.loads(subprocess.check_output(['python3',str(ROOT/'tools/gen_manifest.py'),*args],cwd=ROOT))
    def test_default_matches_firmware_extension(self):
        m=self.manifest()
        self.assertEqual(len(m['commands']),21)
        self.assertEqual(len(m['events']),5)
        self.assertEqual(m['minFirmware'],'0.2.3')
        commands={c['name']:c for c in m['commands']}
        self.assertIn('boot_id',commands['timer.cancel']['parameters']['required'])
        self.assertIn('name',commands['device.set_name']['parameters']['required'])
        self.assertIn('url',commands['display.show_image']['parameters']['required'])
        self.assertIn('camera.capture',commands)
        for name in ['speaker.say','speaker.play_url','speaker.beep','speaker.play_effect','timer.start']:
            self.assertEqual(commands[name]['parameters']['properties']['notify_event']['type'],'boolean')
            self.assertNotIn('notify_event',commands[name]['parameters'].get('required',[]))
        for c in m['commands']:
            self.assertFalse(c['parameters']['additionalProperties'])
        for e in m['events']:
            self.assertIn('boot_id',e['data']['required'])
    def test_custom_application_does_not_claim_default_commands(self):
        m=self.manifest('--no-companion','--commands','examples/button_companion/app.c:commands')
        self.assertEqual(len(m['commands']),11)
        self.assertEqual(m['events'],[])
        self.assertIn('companion.info',{c['name'] for c in m['commands']})

if __name__=='__main__':unittest.main()
