"""OBJ-01 integration in the real GTK application, using fresh projects only."""
import json,os,subprocess,tempfile,unittest
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]
class NativeObjects(unittest.TestCase):
 def test_project_binding_copy_recovery_and_source_conflict(self):
  binary=BASE/'build-nativo/inde_object_native_tests'
  if not binary.is_file():self.skipTest('Compile inde_object_native_tests')
  r=subprocess.run([binary],capture_output=True,text=True,timeout=120);self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertIn('schema37: OK',r.stdout)
 def test_real_gtk_entry_math_objects_save_reopen_and_draft_guard(self):
  binary=BASE/'build-nativo/inde-especial'
  if not binary.is_file():self.skipTest('Compile o aplicativo nativo')
  if not (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):self.skipTest('É necessário um display GTK, por exemplo xvfb-run')
  with tempfile.TemporaryDirectory(prefix='inde-objects-gtk-') as folder:
   root=Path(folder);env=dict(os.environ);env['XDG_CONFIG_HOME']=str(root/'profile')
   r=subprocess.run([str(binary),'--objetos-smoke',str(root/'Objetos.inde'),str(root/'native.png')],capture_output=True,text=True,env=env,timeout=180)
   self.assertEqual(r.returncode,0,r.stdout+r.stderr);self.assertIn('owner preservation OK',r.stdout)
   report=json.loads((root/'nativo-smoke.json').read_text());self.assertTrue(report['native_window']);self.assertTrue(report['draft_close_guard']);self.assertTrue(report['save_reopen']);self.assertTrue(report['no_http']);self.assertEqual(report['equations'],3);self.assertGreater((root/'nativo-reaberto.png').stat().st_size,1000)
if __name__=='__main__':unittest.main(verbosity=2)
