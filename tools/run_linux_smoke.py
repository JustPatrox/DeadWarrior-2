"""Run real X11 checks with a window manager and capture startup deadlocks."""
from pathlib import Path
import subprocess
import sys
import time


def main():
    out = Path('out')
    out.mkdir(exist_ok=True)
    with (out / 'openbox.log').open('w') as log:
        manager = subprocess.Popen(['openbox'], stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
        try:
            diagnostic = ''
            for _ in range(50):
                if manager.poll() is not None:
                    raise RuntimeError('Test window manager exited during startup')
                try:
                    state = subprocess.run(['xprop', '-root', '_NET_SUPPORTING_WM_CHECK'], capture_output=True, text=True, timeout=1)
                    diagnostic = state.stdout + state.stderr
                    if state.returncode == 0 and 'window id #' in state.stdout:
                        break
                except subprocess.TimeoutExpired as error:
                    diagnostic = str(error)
                time.sleep(.1)
            else:
                raise RuntimeError('Test window manager did not become ready: ' + diagnostic)
            print('SMOKE X11 window manager ready', flush=True)
            with (out / 'smoke-output.log').open('w') as output:
                game = subprocess.Popen([sys.argv[1], '--smoke-test', 'out/checks', '--save-dir', 'out/test-save'],
                                        stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT)
                try:
                    result = game.wait(timeout=120)
                except subprocess.TimeoutExpired:
                    print('SMOKE timed out; collecting native thread stacks', flush=True)
                    with (out / 'smoke-stacks.log').open('w') as stacks:
                        for command in (["xwininfo", "-root", "-tree"],
                                        ['sudo', '-n', 'gdb', '--batch', '-ex', 'set pagination off',
                                         '-ex', 'thread apply all bt', '-p', str(game.pid)]):
                            try:
                                subprocess.run(command, stdin=subprocess.DEVNULL, stdout=stacks,
                                               stderr=subprocess.STDOUT, timeout=25)
                            except (subprocess.TimeoutExpired, OSError) as error:
                                stacks.write(str(error) + '\n')
                    result = 124
                finally:
                    if game.poll() is None:
                        game.kill()
                    game.wait(timeout=10)
            print((out / 'smoke-output.log').read_text(errors='replace'), flush=True)
            if (out / 'smoke-stacks.log').exists():
                print((out / 'smoke-stacks.log').read_text(errors='replace'), flush=True)
            return result
        finally:
            if manager.poll() is None:
                manager.terminate()
                try:
                    manager.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    manager.kill()
                    manager.wait(timeout=5)


if __name__ == '__main__':
    sys.exit(main())
