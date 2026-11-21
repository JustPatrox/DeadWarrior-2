"""Run real X11 checks with a window manager and capture startup deadlocks."""
from pathlib import Path
import re
import subprocess
import sys
import time


def capture_stacks(out, *processes):
    """Preserve the desktop side too when mapping fails before SDL starts."""
    commands=[['xwininfo', '-root', '-tree'], ['xprop', '-root', '_NET_CLIENT_LIST'],
              ['ps', '-o', 'pid,ppid,stat,wchan:30,args', '-p', ','.join(str(p.pid) for p in processes)]]
    commands.extend(['sudo', '-n', 'gdb', '--batch', '-ex', 'set pagination off',
                     '-ex', 'thread apply all bt', '-p', str(p.pid)] for p in processes)
    with (out/'smoke-stacks.log').open('w') as stacks:
        for command in commands:
            try:
                subprocess.run(command, stdin=subprocess.DEVNULL, stdout=stacks,
                               stderr=subprocess.STDOUT, timeout=25)
            except (subprocess.TimeoutExpired, OSError) as error:
                stacks.write(str(error)+'\n')
    print((out/'smoke-stacks.log').read_text(errors='replace'), flush=True)


def main():
    out = Path('out')
    out.mkdir(exist_ok=True)
    with (out / 'openbox.log').open('w') as log:
        # There is no desktop session manager inside this Xvfb test session.
        manager = subprocess.Popen(['openbox', '--sm-disable', '--sync'], stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
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
            # The EWMH property is published during startup. Also require a
            # real managed client to become viewable before launching SDL.
            # The first MapRequest can arrive while the WM still initializes,
            # despite its EWMH property already being present. Do not wait on a
            # stranded client indefinitely: issue a fresh request with a new
            # client. Require both visibility and actual WM ownership each time.
            mapped = False
            with (out/'desktop-readiness.log').open('w') as readiness:
                for attempt in range(1,6):
                    name = f'DW2DesktopProbe-{attempt}'
                    probe = subprocess.Popen(['xmessage', '-name', name, '-title', name, '-timeout', '20', 'Desktop readiness'],
                                             stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
                    state_text = clients_text = ''
                    try:
                        for _ in range(30):
                            if manager.poll() is not None:
                                raise RuntimeError('Test window manager exited during readiness check')
                            if probe.poll() is not None:
                                break
                            try:
                                state = subprocess.run(['xwininfo', '-name', name], capture_output=True, text=True, timeout=1)
                                state_text = state.stdout + state.stderr
                                clients = subprocess.run(['xprop', '-root', '_NET_CLIENT_LIST'], capture_output=True, text=True, timeout=1)
                                clients_text = clients.stdout + clients.stderr
                                match = re.search(r'Window id:\s*(0x[\da-fA-F]+)',state_text)
                                managed = {int(value,16) for value in re.findall(r'0x[\da-fA-F]+',clients_text)}
                                mapped = bool(state.returncode == 0 and 'Map State: IsViewable' in state_text and
                                              match and int(match.group(1),16) in managed)
                                if mapped:
                                    print(f'SMOKE X11 managed readiness client verified attempt={attempt}',flush=True)
                                    break
                            except subprocess.TimeoutExpired as error:
                                state_text = str(error)
                            time.sleep(.1)
                        readiness.write(f'attempt={attempt} mapped={mapped}\n{state_text}\n{clients_text}\n')
                        readiness.flush()
                        if mapped:
                            break
                        print(f'SMOKE X11 readiness attempt={attempt} was not managed/viewable; recreating client',flush=True)
                        if attempt == 5:
                            capture_stacks(out, manager, probe)
                    finally:
                        if probe.poll() is None:
                            probe.kill()
                        probe.wait(timeout=5)
                if not mapped:
                    raise RuntimeError('Test desktop could not manage/map a readiness client after five fresh requests: '+state_text+clients_text)
            print('SMOKE X11 managed client verified', flush=True)
            with (out / 'smoke-output.log').open('w') as output:
                game = subprocess.Popen([sys.argv[1], '--smoke-test', 'out/checks', '--save-dir', 'out/test-save'],
                                        stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT)
                try:
                    result = game.wait(timeout=120)
                except subprocess.TimeoutExpired:
                    print('SMOKE timed out; collecting native thread stacks', flush=True)
                    capture_stacks(out, game, manager)
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
