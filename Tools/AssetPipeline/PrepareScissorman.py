"""공식 SCISSORMAN GLB를 엔진용 외부 glTF로 준비한다. 원본은 수정하지 않는다."""
import argparse
import bisect
import math
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / 'Scripts'))
from gltf_asset import Document, MergeAnimation, Validate, WriteJSON

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('--output', type=Path, default=Path('Game/Content/BabyDoll/Scissorman'))
args = parser.parse_args()
blob = args.source.read_bytes()
assert blob[:4] == b'glTF' and struct.unpack_from('<I', blob, 4)[0] == 2
json_size = struct.unpack_from('<I', blob, 12)[0]
data = json.loads(blob[20:20 + json_size])
binary_offset = 20 + json_size
binary_size = struct.unpack_from('<I', blob, binary_offset)[0]
binary = blob[binary_offset + 8:binary_offset + 8 + binary_size]
out = args.output
out.mkdir(parents=True, exist_ok=True)
data['buffers'][0]['uri'] = 'Scissorman.bin'
(out / 'Scissorman.bin').write_bytes(binary)
for index, image in enumerate(data.get('images', [])):
    view = data['bufferViews'][image.pop('bufferView')]
    begin = view.get('byteOffset', 0)
    image['uri'] = f'Texture{index}.png'
    image.pop('mimeType', None)
    (out / image['uri']).write_bytes(binary[begin:begin + view['byteLength']])
path = out / 'Scissorman.gltf'
WriteJSON(path, data)
source = Document(path)
for animation in source.data['animations']:
    for sampler in animation['samplers']:
        if sampler.get('interpolation') == 'STEP':
            values = source.Read(sampler['output'])
            times = [row[0] for row in source.Read(sampler['input'])]
            # 원본 STEP 의미로 30Hz 샘플링한 뒤 엔진이 지원하는 LINEAR 키로 베이크한다.
            duration = times[-1] - times[0]
            baked_times = [times[0] + i / 30 for i in range(math.ceil(duration * 30))] + [times[-1]]
            sampled = [values[max(0, bisect.bisect_right(times, t) - 1)] for t in baked_times]
            kind = source.data['accessors'][sampler['output']]['type']
            sampler['input'] = source.Append([[t] for t in baked_times], 'SCALAR')
            sampler['output'] = source.Append(sampled, kind)
            sampler['interpolation'] = 'LINEAR'
target = Document(path)
target.data['animations'] = []
for animation in source.data['animations']:
    MergeAnimation(target, source, animation, animation['name'], sampleRate=30)
target.Save()
profile = json.loads((Path(__file__).parent / 'Profiles/SeonEngine.json').read_text())
report = Validate(path, profile)
WriteJSON(out / 'ImportReport.json', report)
WriteJSON(out / 'Source.json', {
    'author': 'Goth Scarecrow',
    'source': 'https://goth-scarecrow.itch.io/scissorman',
    'license': 'CC0',
    'originalSHA256': hashlib.sha256(blob).hexdigest(),
    'preparation': 'External PNG/bin extraction; STEP sampled with hold at 30Hz; LINEAR resampling',
})
print(json.dumps(report, indent=2))
