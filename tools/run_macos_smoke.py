"""Run the installed app and preserve a native backtrace after startup failure."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import time


def main():
    out = Path('out')
    checks = out / 'checks'
    checks.mkdir(parents=True, exist_ok=True)
    command = [sys.argv[1], '--smoke-test', str(checks), '--save-dir', 'out/test-save']
    started = time.time()
    with (out / 'smoke-output.log').open('w') as output:
        try:
            result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=180).returncode
        except subprocess.TimeoutExpired:
            result = 124
    print((out / 'smoke-output.log').read_text(errors='replace'), flush=True)
    if result:
        # macOS crash reports are asynchronous and can be disabled on CI. LLDB
        # supplies the failing thread even when DiagnosticReports remains empty.
        with (out / 'smoke-stacks.log').open('w') as stacks:
            debugger = ['lldb', '--batch', '-o', 'settings set target.disable-aslr false',
                        '-o', 'run', '-k', 'thread backtrace all', '-k', 'quit', '--',
                        *command]
            try:
                subprocess.run(debugger, stdin=subprocess.DEVNULL, stdout=stacks,
                               stderr=subprocess.STDOUT, timeout=200)
            except (subprocess.TimeoutExpired, OSError) as error:
                stacks.write(str(error) + '\n')
        print((out / 'smoke-stacks.log').read_text(errors='replace'), flush=True)
        reports = Path.home() / 'Library/Logs/DiagnosticReports'
        for _ in range(20):
            found = [path for path in reports.glob('DeadWarrior*')
                     if path.is_file() and path.stat().st_mtime >= started - 1]
            if found:
                for path in found:
                    shutil.copy2(path, checks / path.name)
                break
            time.sleep(.5)
    (checks / 'smoke-process.json').write_text(json.dumps({'returnCode': result}) + '\n')
    return 128 - result if result < 0 else result


if __name__ == '__main__':
    sys.exit(main())
