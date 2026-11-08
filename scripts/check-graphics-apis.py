"""Exercise actual Linux Vulkan/OpenGL rendering and MangoHud pacing in CI."""
import argparse
import csv
import json
import os
from pathlib import Path
import statistics
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--bin', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if os.environ.get('GITHUB_ACTIONS') != 'true':
    raise SystemExit('Live graphics diagnostics require the isolated CI runner.')
args.output.mkdir(parents=True, exist_ok=True)
checks = []
with tempfile.TemporaryDirectory(prefix='framoryx-apis-') as folder:
    root = Path(folder)
    for api, executable in [('OpenGL', 'FramoryxScene'), ('Vulkan', 'FramoryxVulkanScene')]:
        logs = root / api
        logs.mkdir()
        config=f'fps_limit=30,autostart_log=1,log_interval=0,log_duration=6,output_folder={logs},cpu_stats=0,gpu_stats=0,fps,frametime'
        env = dict(os.environ, MANGOHUD_CONFIG=config, MANGOHUD_DLSYM='1')
        report = logs / 'scene.json'
        runtime = args.bin.resolve() / 'runtime/mangohud/bin/mangohud'
        process = subprocess.run([str(runtime), '--dlsym', str(args.bin.resolve() / executable), '--seconds', '8', '--report', str(report)], env=env, capture_output=True, text=True, timeout=25)
        durations = []
        for log in logs.glob('*.csv'):
            with log.open(encoding='utf-8') as source:
                index = None
                for row in csv.reader(source):
                    if 'frametime' in row:
                        index = row.index('frametime')
                    elif index is not None and len(row) > index:
                        try:
                            value = float(row[index])
                            if 0 < value < 1000: durations.append(value)
                        except ValueError:
                            pass
        scene = json.loads(report.read_text(encoding='utf-8')) if report.exists() else {}
        median = statistics.median(durations[15:]) if len(durations) > 15 else 0
        passed = process.returncode == 0 and scene.get('valid') and scene.get('frames', 0) > 120 and len(durations) > 60 and 26 <= median <= 42
        checks.append(dict(api=api, passed=bool(passed), sceneFrames=scene.get('frames', 0), samples=len(durations), medianFrameTimeMs=median, exitCode=process.returncode, diagnostic=process.stderr[-2000:] if not passed else ''))
passed = all(item['passed'] for item in checks)
(args.output / 'graphics-apis.json').write_text(json.dumps(dict(passed=passed, checks=checks), indent=2), encoding='utf-8')
print(json.dumps(checks, indent=2))
raise SystemExit(0 if passed else 1)
