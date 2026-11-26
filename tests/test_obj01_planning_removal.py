"""Native planning deletion: transactional service and actual GTK controls."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from ambiente import native_environment
BASE=Path(__file__).resolve().parents[1]
class PlanningRemoval(unittest.TestCase):
    def test_service_deletion_guards_history_and_restart(self):
        binary=BASE/'build-nativo/inde_planning_removal_tests'
        if not binary.is_file(): self.skipTest('Compile inde_planning_removal_tests')
        r=subprocess.run([binary],capture_output=True,text=True,timeout=120)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    def test_native_independent_insertion(self):
        binary=BASE/'build-nativo/inde_native_insertion_tests'
        if not binary.is_file(): self.skipTest('Compile inde_native_insertion_tests')
        r=subprocess.run([binary],capture_output=True,text=True,timeout=120)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    def test_actual_gtk_deletion_cancel_stale_and_exit(self):
        binary=BASE/'build-nativo/inde-especial'
        if not binary.is_file(): self.skipTest('Compile aplicativo')
        if not os.environ.get('DISPLAY'): self.skipTest('Use display privado')
        with tempfile.TemporaryDirectory(prefix='inde-planning-ui-') as folder:
            root=Path(folder)
            env=native_environment(root/'profile',BASE)
            env.update(XDG_DATA_HOME=str(root/'data'),XDG_CACHE_HOME=str(root/'cache'),
                       GTK_A11Y='none',GSK_RENDERER='cairo',G_DEBUG='fatal-criticals')
            r=subprocess.run([str(binary),'--planejamento-exclusao-smoke',str(root/'Teste.inde'),
                              str(root/'planejamento.png')],env=env,capture_output=True,text=True,timeout=90)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            self.assertIn('normal exit OK',r.stdout)
            self.assertGreater((root/'planejamento.png').stat().st_size,1000)
            target=os.environ.get('INDE_TEST_ARTIFACTS')
            if target:
                import shutil
                out=Path(target);out.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(root/'planejamento.png',out/'planejamento-exclusao.png')
                shutil.copyfile(root/'planejamento-confirmacao.png',out/'planejamento-confirmacao.png')
                shutil.copytree(root/'Demo-planejamento.inde',out/'Demo-planejamento.inde')
                (out/'planejamento-exclusao.log').write_text(r.stdout+r.stderr)
if __name__=='__main__':unittest.main(verbosity=2)
