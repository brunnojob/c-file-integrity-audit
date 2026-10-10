import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

def run(argv, expected=0):
    result = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, timeout=30)
    if result.returncode != expected:
        raise RuntimeError(result.stderr + result.stdout)
    return result.stdout

with tempfile.TemporaryDirectory() as temp:
    data = Path(temp) / 'data'
    data.mkdir()
    (data / 'sensor.csv').write_text('1,42\n')
    manifest = Path(temp) / 'manifest.txt'
    manifest.write_text(run(['build/integrity', 'snapshot', str(data)]))
    healthy = json.loads(run(['build/integrity', 'verify', str(data), str(manifest)]))
    assert healthy['valid'] and healthy['changed'] == 0
    (data / 'sensor.csv').write_text('1,99\n')
    changed = json.loads(run(['build/integrity', 'verify', str(data), str(manifest)], 1))
    assert not changed['valid'] and changed['changed'] == 1
    print(json.dumps({'baseline': healthy, 'tampered': changed}, sort_keys=True))
