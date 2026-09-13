"""Inspect generated mock SVGs as XML; actual visual QA is a separate step."""
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def main():
    program = Path(sys.argv[1]).resolve()
    namespace = '{http://www.w3.org/2000/svg}'
    with tempfile.TemporaryDirectory(prefix='rpcmp-player-ui-') as temporary:
        root = Path(temporary)
        for view in ('tracker', 'keyboard', 'library'):
            for mode in ('playing', 'paused', 'empty', 'error', 'gap', 'long', 'unsupported'):
                output = root / f'{view}-{mode}.svg'
                subprocess.run([program, view, output, mode], check=True)
                tree = ET.parse(output).getroot()
                assert tree.tag == namespace + 'svg'
                assert (tree.get('width'), tree.get('height')) == ('640', '480')
                clips = {}
                texts = []
                for node in tree.iter():
                    assert node.tag in {namespace + name for name in ('svg', 'rect', 'line', 'defs', 'clipPath', 'text')}
                    assert not any('href' in key for key in node.attrib)
                    if node.tag == namespace + 'rect':
                        x, y, width, height = [int(node.get(key)) for key in ('x', 'y', 'width', 'height')]
                        assert width > 0 and height > 0 and x >= 0 and y >= 0
                        assert x + width <= 640 and y + height <= 480
                    if node.tag == namespace + 'line':
                        assert all(0 <= int(node.get(key)) < bound for key, bound in (('x1', 640), ('x2', 640), ('y1', 480), ('y2', 480)))
                    if node.tag == namespace + 'clipPath':
                        assert node.get('id') not in clips
                        clips[node.get('id')] = node.find(namespace + 'rect')
                    if node.tag == namespace + 'text':
                        clip = clips[node.get('clip-path')[5:-1]]
                        assert 0 < int(node.get('textLength')) <= int(clip.get('width'))
                        texts.append(node.text)
                assert view.upper() in texts
                if mode == 'empty':
                    assert 'No track selected' in texts and '00:00' in texts
                elif mode == 'error':
                    assert 'Track could not be loaded' in texts
                else:
                    assert '02:33' in texts
                if mode == 'paused':
                    assert 'PAUSED' in texts
                if mode == 'long':
                    assert any(node.get('data-elided') == 'true' and node.text.endswith('…') for node in tree.iter(namespace + 'text'))
                if mode == 'unsupported' and view != 'library':
                    assert 'Performance display unavailable' in texts
        repeat = root / 'repeat.svg'
        subprocess.run([program, 'tracker', repeat], check=True)
        assert repeat.read_bytes() == (root / 'tracker-playing.svg').read_bytes()
        invalid = root / 'invalid.svg'
        result = subprocess.run([program, 'invalid', invalid], capture_output=True)
        assert result.returncode == 2 and not invalid.exists()
    print('Player UI mock SVG: 21 scenarios, bounds, clipping and determinism PASS')


if __name__ == '__main__':
    main()
