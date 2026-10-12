"""Guard rails for the one-time literal migration, not a runtime translator."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from apply_author_vocabulary import translate,TOKEN

class MigrationGuards(unittest.TestCase):
    def test_whole_words(self):
        path=Path("planning_workspace.cpp")
        self.assertEqual(translate("Identidade e identidades preservadas",path),"Identidade e identidades preservadas")
        self.assertEqual(translate("Filtra entidades por nome",path),"Filtra elementos da história por nome")
        self.assertEqual(translate("Mostra entidades no projeto",path),"Mostra elementos da história no projeto")
        self.assertEqual(translate("Esta âncora",path),"Este trecho marcado")
    def test_contracts(self):
        for key in ("entity","entities","composition","narrative","writing","chronology","editorial","inde.semantic.perspective","project.example"):
            self.assertEqual(translate(key,Path("narrative_structure_workspace.cpp")),key)
    def test_lexer_boundaries(self):
        source='// entidade\nR"tag({"entity":"âncora"})tag"\n"Entidade"'
        found=list(TOKEN.finditer(source))
        self.assertEqual(len(found),3)
        self.assertTrue(found[0].group().startswith("//"))
        self.assertTrue(found[1].group().startswith('R"'))
        self.assertEqual(found[2].group(),'"Entidade"')
if __name__=="__main__":unittest.main()
