import tempfile,unittest
from pathlib import Path
from make_catalog import build
class CatalogTests(unittest.TestCase):
    def test_names_and_addresses(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'01').mkdir();(root/'MP3').mkdir()
            (root/'01'/'001 - Morning.mp3').touch();(root/'MP3'/'0123 - Evening.wav').touch()
            tracks=build(root)
            self.assertEqual([(t['folder'],t['track'],t['title']) for t in tracks],[(1,1,'Morning'),(0,123,'Evening')])
    def test_reject_duplicates(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'01').mkdir()
            for title in ('001 First.mp3','001 Second.wav'):(root/'01'/title).touch()
            with self.assertRaises(ValueError):build(root)
    def test_utf8_limit_and_invalid_numbers(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'02').mkdir()
            (root/'02'/('001 '+('آرام'*20)+'.mp3')).touch();(root/'02'/'999 invalid.mp3').touch()
            tracks=build(root);self.assertEqual(len(tracks),1)
            self.assertLessEqual(len(tracks[0]['title'].encode()),63)
if __name__=='__main__':unittest.main()
