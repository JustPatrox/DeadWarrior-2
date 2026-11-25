"""Verify native intro playback through isolated, owned Windows clients."""
import argparse
import configparser
import csv
import ctypes as ct
from ctypes import wintypes as wt
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import threading
import time
import wave

import numpy as np
from PIL import Image
from native_client import capture_from_game
from reference_projector import user, window

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ.get('DW2_TEST_EVIDENCE', ROOT/'work/intro-window-evidence'))


class Client:
    def __init__(self, executable, width, height, mode, frame_timing=False, fps_limit=None):
        self.width, self.height = width, height
        self.prefix = f'{width}x{height}-{mode}'
        self.mode = mode
        self.folder = OUT/'clients'/f'{self.prefix}-{time.time_ns()}'
        self.folder.mkdir(parents=True)
        self.exe = self.folder/'DeadWarrior 2.exe'
        shutil.copyfile(executable, self.exe)
        # Keep each product log separate. Only this newly created directory
        # receives a junction; its target and the supplied installation are read-only.
        subprocess.run(['cmd', '/c', 'mklink', '/J', str(self.folder/'assets'),
                        str((executable.parent/'assets').resolve())],
                       check=True, capture_output=True, text=True)
        self.save = self.folder/'save'
        self.save.mkdir()
        if fps_limit is not None:
            (self.save/'options.ini').write_text(
                '[Display]\nmode=windowed\nresolution=auto\n'
                f'fps_limit={fps_limit}\nvsync=0\nshow_fps=0\nmouse_lock=0\naspect_mode=dynamic\n',
                encoding='utf-8')
        self.sentinel = self.save/'unrelated.txt'
        self.sentinel.write_text('preserve', encoding='utf-8')
        self.capture = self.folder/'capture.png'
        self.frame_timing = self.folder/'frame-timing.tsv' if frame_timing else None
        self.process = None
        self.hwnd = None
        self.gpu_control = self.gpu_session = None
        self.gpu_changed = threading.Condition()
        self.gpu_latest = None
        self.gpu_serial = 0
        self.gpu_closed = False

    def __enter__(self):
        arguments = [self.exe, '--save-dir', self.save,
                     '--capture-client', self.capture, '--magenta']
        if self.frame_timing is not None:
            arguments += ['--frame-timing', self.frame_timing]
        self.process = subprocess.Popen(arguments)
        def find():
            self.hwnd = next((h for h, c in window(self.process.pid)
                              if c == 'DeadWarrior2Native'), None)
            return bool(self.hwnd)
        try:
            self.wait(find, 25)
            self.resize(self.width, self.height)
            if self.mode == 'full':
                self.prepare_gpu()
        except Exception:
            self.__exit__()
            raise
        return self

    def __exit__(self, *unused):
        self.stop_gpu()
        if self.process is not None and self.process.poll() is None:
            if self.hwnd:
                user.PostMessageW(self.hwnd, 0x10, 0, 0)
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                # This handle belongs to the client launched above, never an
                # existing player process or a process selected by its name.
                self.process.terminate()
                self.process.wait(timeout=10)
        log = self.folder/'lastest.log'
        if log.exists():
            shutil.copyfile(log, OUT/f'{self.prefix}-lastest.log')

    def wait(self, check, seconds=20):
        until = time.monotonic()+seconds
        while time.monotonic() < until:
            if self.process.poll() is not None:
                raise AssertionError(f'Owned intro client exited: {self.process.returncode}')
            result = check()
            if result:
                return result
            time.sleep(.025)
        raise AssertionError(f'Intro condition timed out: page={self.query(28)} '
                             f'playing={self.query(35)} frame={self.query(34)}')

    def query(self, number):
        return user.SendMessageW(self.hwnd, 0x8000+number, 0, 0)

    def key(self, code):
        user.SendMessageW(self.hwnd, 0x100, code, 0)
        user.SendMessageW(self.hwnd, 0x101, code, 0)

    def size(self):
        bounds = wt.RECT()
        user.GetClientRect(self.hwnd, ct.byref(bounds))
        return bounds.right, bounds.bottom

    def resize(self, width, height):
        user.SetWindowLongW(self.hwnd, -16,
                           (user.GetWindowLongW(self.hwnd, -16) & ~0xcf0000) | 0x80000000)
        user.SetWindowPos(self.hwnd, 0, 0, 0, width, height, 0x34)
        assert self.size() == (width, height), self.size()

    def click(self, x, y):
        width, height = self.size()
        vw, vh = self.query(23)/100, self.query(24)/100
        scale = min(width/vw, height/vh)
        px = round(width/2+(x-425)*scale)
        py = round((height-vh*scale)/2+y*scale)
        for message, button in ((0x200, 0), (0x201, 1), (0x202, 0)):
            user.SendMessageW(self.hwnd, message, button, (py << 16) | (px & 65535))

    def prepare_gpu(self):
        if self.gpu_control is not None:
            return
        from windows_capture import WindowsCapture
        with self.gpu_changed:
            self.gpu_latest = None
            self.gpu_serial = 0
            self.gpu_closed = False
        session = WindowsCapture(cursor_capture=False, draw_border=False, window_hwnd=self.hwnd)
        @session.event
        def on_frame_arrived(frame, control):
            pixels = frame.frame_buffer.copy()
            with self.gpu_changed:
                self.gpu_latest = pixels
                self.gpu_serial += 1
                self.gpu_changed.notify_all()
        @session.event
        def on_closed():
            with self.gpu_changed:
                self.gpu_closed = True
                self.gpu_changed.notify_all()
        self.gpu_session = session
        self.gpu_control = session.start_free_threaded()

    def stop_gpu(self):
        if self.gpu_control is not None:
            self.gpu_control.stop()
            self.gpu_control = self.gpu_session = None

    def gpu_frame(self):
        self.prepare_gpu()
        with self.gpu_changed:
            serial = self.gpu_serial
            if not self.gpu_changed.wait_for(lambda: self.gpu_serial > serial or self.gpu_closed, timeout=3):
                raise AssertionError('No fresh Windows Graphics Capture frame from the owned HWND')
            if self.gpu_closed or self.gpu_latest is None:
                raise AssertionError('Owned HWND graphics capture closed before a frame arrived')
            pixels = self.gpu_latest
        image = Image.fromarray(pixels[:, :, 2::-1])
        assert image.size == self.size(), (image.size, self.size())
        self.stop_gpu()
        return image

    def shot(self, label, actual_gpu=False):
        before = self.query(34)
        image = self.gpu_frame() if actual_gpu else capture_from_game(self.hwnd, self.capture)
        after = self.query(34)
        image.save(OUT/f'{self.prefix}-{label}.png')
        pixels = np.asarray(image)
        assert not np.any(np.all(pixels == [255, 0, 255], axis=2)), label
        width, height = image.size
        central = pixels[height//5:height*4//5, width//5:width*4//5]
        return dict(label=label, frameBefore=before, frameAfter=after,
                    captureRoute='windows-graphics-capture-hwnd' if actual_gpu else 'diagnostic-software-render',
                    centralSHA256=hashlib.sha256(central.tobytes()).hexdigest())

    def begin_intro(self, pause_pending=False):
        self.wait(lambda: self.query(28) == 1)
        time.sleep(1.6)
        for point, page in (((161, 664), 2), ((437, 664), 3)):
            self.click(*point)
            self.wait(lambda: self.query(28) == page)
            time.sleep(1.3)
        self.click(154, 668)
        if pause_pending:
            assert self.query(28) == 3 and not self.query(35)
            self.key(0x2d)
            self.wait(lambda: self.query(21) & 2)
            time.sleep(1)
            assert self.query(28) == 3 and not self.query(35), 'Difficulty advanced behind PC settings'
            self.key(0x2d)
        self.wait(lambda: self.query(35) == 1 and self.query(34) >= 5)
        assert self.query(28) == 4 and self.query(22) == -1

    def tutorial_after_intro(self):
        self.wait(lambda: self.query(22) >= 1 and self.query(35) == 0)
        assert self.query(34) == 0 and self.query(28) == 5
        self.shot('tutorial')
        assert self.sentinel.read_text(encoding='utf-8') == 'preserve'
        log = (self.folder/'lastest.log').read_text(encoding='utf-8')
        assert 'INTRO started fps=30 first=3 last=3235' in log and 'INTRO stopped' in log
        assert log.count('MISSION start level=1 ') == 1
        assert 'AUDIO music=orderInGame' in log
        assert 'RENDER backend=Direct2D' in log
        for message in ('INTRO resources unavailable', 'AUDIO ERROR', 'AUDIO MISSING', 'FATAL'):
            assert message not in log, message
        return log


def check(executable, width, height, mode):
    with Client(executable, width, height, mode) as client:
        client.begin_intro(pause_pending=mode == 'full')
        report = dict(resolution=[width, height], mode=mode, autoplay=True,
                      isolatedSaveAndLog=True, captures=[], executablePath=str(client.exe.resolve()),
                      executableSHA256=hashlib.sha256(client.exe.read_bytes()).hexdigest(),
                      frameManifestSHA256=hashlib.sha256((client.folder/'assets/intro/frame-hashes.json').read_bytes()).hexdigest())
        if mode == 'full':
            report['settingsFreezePendingDifficulty'] = True
            client.wait(lambda: client.query(34) >= 12)
            report['captures'].append(client.shot('source-12-gpu', actual_gpu=True))
            with wave.open(str(executable.parent/'assets/intro/music.wav'), 'rb') as source:
                sample_rate = source.getframerate()
            client.wait(lambda: client.query(32) > 0 and client.query(34) >= 12)
            start = time.monotonic()
            first_frame, first_audio = client.query(34), client.query(32)
            time.sleep(1.2)
            last_frame, last_audio = client.query(34), client.query(32)
            elapsed = time.monotonic()-start
            measured = (last_frame-first_frame)/elapsed
            assert 24 <= measured <= 36, measured
            assert abs((last_frame-first_frame)-(last_audio-first_audio)*30/sample_rate) <= 3
            report['measuredSourceFPS'] = measured
            report['audioClockMatchesFrames'] = True
            client.click(425, 350)
            after_click = client.query(34)
            client.wait(lambda: client.query(34) > after_click)
            assert client.query(35) == 1 and client.query(28) == 4 and not client.query(18)
            report['centerClickKeepsMoviePlaying'] = True

            client.key(0x2d)
            client.wait(lambda: client.query(21) & 2)
            time.sleep(.2)
            paused_frame, paused_audio = client.query(34), client.query(32)
            time.sleep(1)
            assert client.query(34) == paused_frame and client.query(32) == paused_audio
            report['insertFreezesFrameAndAudio'] = True
            report['captures'].append(client.shot('paused-settings'))
            # Change the real resolution setting without leaving windowed mode.
            old_size = client.size()
            client.key(0x24)  # Home: Windowed row.
            client.key(0x26)  # Up: Resolution row.
            presses = 2 if width == 1280 else 1
            for unused in range(presses):
                client.key(0x27)
            client.key(0x0d)
            client.wait(lambda: client.size() != old_size)
            options = configparser.ConfigParser()
            options.read(client.save/'options.ini')
            expected = '1600x900' if presses == 2 else '1280x720'
            assert options['Display']['resolution'] == expected
            assert client.query(34) == paused_frame and client.query(32) == paused_audio
            report['resolutionChangesDuringPause'] = True
            report['captures'].append(client.shot('resized-settings'))
            for unused in range(presses):
                client.key(0x25)
            client.key(0x0d)
            client.resize(width, height)
            client.key(0x2d)
            client.wait(lambda: not client.query(21) & 2 and client.query(34) > paused_frame and client.query(32) > paused_audio)
            report['insertResumesFrameAndAudio'] = True

            wanted = [100, 900, 2640, 2800, 2921, 3200]
            until = time.monotonic()+125
            while time.monotonic() < until and client.query(35):
                frame = client.query(34)
                if wanted and wanted[0] == 2921 and frame >= 2880:
                    client.prepare_gpu()
                if wanted and frame >= wanted[0]:
                    requested = wanted.pop(0)
                    if requested == 2921:
                        report['captures'].append(client.shot('source-2921-gpu', actual_gpu=True))
                    report['captures'].append(client.shot(f'source-{requested}'))
                    print(f'{client.prefix}: requested={requested}, current={frame}', flush=True)
                time.sleep(.025)
            assert not client.query(35) and not wanted, wanted
            log = client.tutorial_after_intro()
            assert 'INTRO completed source_frame=3235' in log
            frames = [shot['centralSHA256'] for shot in report['captures'] if shot['label'].startswith('source-')]
            assert len(set(frames)) >= 4, 'Movie frames did not visibly animate'
            report['automaticEndStartsTutorial'] = True
        else:
            client.wait(lambda: client.query(34) >= 100)
            if mode == 'escape':
                old_frame = client.query(34)
                client.click(434, 639)  # Queue the original delayed Skip action.
                client.click(807-(425-client.query(23)/200), 18)
                client.wait(lambda: client.query(28) == 1 and not client.query(35))
                time.sleep(1)
                assert client.query(28) == 1 and client.query(22) == -1 and client.query(34) == 0
                report['backCancelsPendingSkip'] = True
                client.begin_intro()
                assert 5 <= client.query(34) < min(old_frame, 40)
                report['reentryRestartsIntroClock'] = True
                client.wait(lambda: client.query(34) >= 40)
            report['captures'].append(client.shot('before-skip'))
            if mode == 'escape':
                client.key(0x1b)
            else:
                client.click(434, 639)
            log = client.tutorial_after_intro()
            assert 'INTRO completed source_frame=3235' not in log
            report['escapeStartsTutorial' if mode == 'escape' else 'originalSkipStartsTutorial'] = True
        report['noResidualIntro'] = True
        report['noMagenta'] = True
        report['direct2DPlaybackObserved'] = True
    (OUT/f'{width}x{height}-{mode}-checks.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(report, flush=True)
    return report


def performance(executable, width, height):
    """Measure presentation intervals without PNG captures or display changes."""
    with wave.open(str(executable.parent/'assets/intro/music.wav'), 'rb') as source:
        sample_rate = source.getframerate()
    samples = []
    with Client(executable, width, height, 'performance', frame_timing=True) as client:
        client.begin_intro()
        client.wait(lambda: client.query(32) > 0)
        start = time.monotonic()
        while time.monotonic()-start < 20:
            unix_time = time.time()
            frame, audio = client.query(34), client.query(32)
            samples.append(dict(unix_time=unix_time, elapsed_measurement_s=time.monotonic()-start,
                                source_frame=frame, audio_samples=audio))
            assert frame >= 3 and audio > 0
            time.sleep(.05)
        assert client.query(35) == 1 and client.query(28) == 4
    assert client.process.returncode == 0
    with (OUT/f'{client.prefix}-samples.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=samples[0].keys())
        writer.writeheader(); writer.writerows(samples)
    shutil.copyfile(client.frame_timing, OUT/f'{client.prefix}-raw.tsv')
    with client.frame_timing.open(encoding='utf-8') as stream:
        frames = list(csv.DictReader(stream, delimiter='\t'))
    cumulative = np.cumsum([float(row['interval_ms']) for row in frames])/1000
    # Native hitch messages are stamped immediately after that presentation.
    # Match a unique row to align the CSV clock, without adding a flush/query
    # or diagnostic capture during the measured interval.
    log = (client.folder/'lastest.log').read_text(encoding='utf-8')
    anchor = None
    for stamp, render, interval in re.findall(
            r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) RENDER hitch render_ms=([\d.]+) interval_ms=([\d.]+)', log, re.M):
        matches = [i for i, row in enumerate(frames)
                   if abs(float(row['render_ms'])-float(render)) < .02
                   and abs(float(row['interval_ms'])-float(interval)) < .02]
        if len(matches) == 1:
            anchor = datetime.strptime(stamp, '%Y-%m-%d %H:%M:%S.%f').timestamp()-cumulative[matches[0]]
            break
    assert anchor is not None, 'Cannot align native timing rows to the sampled intro clock'
    origin = float(np.median([s['unix_time']-s['audio_samples']/sample_rate for s in samples]))
    end = samples[-1]['unix_time']
    selected = []
    for index, row in enumerate(frames):
        stamp = anchor+cumulative[index]
        if origin <= stamp <= end:
            selected.append(dict(elapsed_intro_s=stamp-origin,
                                 interval_ms=float(row['interval_ms']), render_ms=float(row['render_ms']),
                                 gpu=int(row['gpu']), scene=row['scene']))
    assert selected and max(row['elapsed_intro_s'] for row in selected) >= 19
    with (OUT/f'{client.prefix}-frames.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=selected[0].keys())
        writer.writeheader(); writer.writerows(selected)
    def distribution(rows):
        intervals = np.asarray([row['interval_ms'] for row in rows])
        renders = np.asarray([row['render_ms'] for row in rows])
        assert len(intervals)
        return dict(presentations=len(rows),
                    intervalMs=dict(zip(('p50', 'p95', 'p99'), map(float, np.percentile(intervals, [50, 95, 99])))),
                    renderMs=dict(zip(('p50', 'p95', 'p99'), map(float, np.percentile(renders, [50, 95, 99])))),
                    intervalsOver50Ms=int(np.sum(intervals > 50)), intervalsOver100Ms=int(np.sum(intervals > 100)),
                    acceleratedPresentations=sum(row['gpu'] for row in rows))
    report = dict(resolution=[width, height], noPngCaptures=True, noDisplayChangesDuringMeasurement=True,
                  sampleCount=len(samples), sampledSeconds=samples[-1]['elapsed_measurement_s'],
                  sourceFrames=[samples[0]['source_frame'], samples[-1]['source_frame']],
                  warmupFirst2Seconds=distribution([row for row in selected if row['elapsed_intro_s'] < 2]),
                  followingSeconds=distribution([row for row in selected if row['elapsed_intro_s'] >= 2]),
                  alignment='Unique native hitch row plus cumulative presentation intervals; movie origin from audio samples.')
    (OUT/f'{client.prefix}-checks.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(report, flush=True)
    return report


def rate_check(executable, width, height, limit):
    """Confirm presentation limits do not change the movie/audio time rate."""
    with wave.open(str(executable.parent/'assets/intro/music.wav'), 'rb') as source:
        sample_rate = source.getframerate()
    samples = []
    with Client(executable, width, height, f'rate-{limit}', frame_timing=True, fps_limit=limit) as client:
        client.begin_intro()
        client.wait(lambda: client.query(34) >= 12 and client.query(32) > 0)
        options = configparser.ConfigParser()
        options.read(client.save/'options.ini')
        assert options['Display']['fps_limit'] == str(limit)
        start = time.monotonic()
        while time.monotonic()-start < 5:
            samples.append(dict(elapsed_s=time.monotonic()-start,
                                source_frame=client.query(34), audio_samples=client.query(32)))
            time.sleep(.05)
        assert client.query(35) == 1 and client.query(28) == 4
    assert client.process.returncode == 0 and not list(client.folder.glob('*.png'))
    elapsed = samples[-1]['elapsed_s']-samples[0]['elapsed_s']
    frames = samples[-1]['source_frame']-samples[0]['source_frame']
    audio = samples[-1]['audio_samples']-samples[0]['audio_samples']
    source_fps = frames/elapsed
    assert 27 <= source_fps <= 33, (limit, source_fps)
    difference = abs(frames-audio*30/sample_rate)
    assert difference <= 3, (limit, frames, audio, difference)
    with client.frame_timing.open(encoding='utf-8') as stream:
        timing = list(csv.DictReader(stream, delimiter='\t'))
    assert timing and all(int(row['fps_limit']) == limit for row in timing)
    with (OUT/f'{client.prefix}-samples.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=samples[0].keys())
        writer.writeheader(); writer.writerows(samples)
    shutil.copyfile(client.frame_timing, OUT/f'{client.prefix}-raw.tsv')
    report = dict(resolution=[width, height], fpsLimit=limit, sampledSeconds=elapsed,
                  sourceFramesAdvanced=frames, audioSamplesAdvanced=audio, measuredSourceFPS=source_fps,
                  sourceVsAudioFrameDifference=difference, sourceClockRemains30Hz=True,
                  requestedLimitUsedByRenderer=True, noPngCaptures=True)
    (OUT/f'{client.prefix}-checks.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(report, flush=True)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('width', type=int)
    parser.add_argument('height', type=int)
    parser.add_argument('--mode', choices=('all', 'full', 'escape', 'skip', 'performance', 'rates'), default='all')
    parser.add_argument('--limits', type=int, nargs='+', choices=(30, 60, 120, 240, 0), default=(30, 60, 120, 240, 0))
    args = parser.parse_args()
    if 'DW2_TEST_EXE' not in os.environ:
        raise SystemExit('Set DW2_TEST_EXE to the isolated test stage; never use the player installation.')
    executable = Path(os.environ['DW2_TEST_EXE']).resolve()
    assert executable != (ROOT/'out/windows-x64/DeadWarrior 2.exe').resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    modes = ('full', 'escape', 'skip') if args.mode == 'all' else (args.mode,)
    if args.mode == 'rates':
        reports = [rate_check(executable, args.width, args.height, limit) for limit in args.limits]
        (OUT/f'{args.width}x{args.height}-rates-checks.json').write_text(json.dumps(reports, indent=2)+'\n', encoding='utf-8')
    else:
        reports = [performance(executable, args.width, args.height) if mode == 'performance'
                   else check(executable, args.width, args.height, mode) for mode in modes]
    if args.mode not in ('performance', 'rates'):
        (OUT/f'{args.width}x{args.height}-checks.json').write_text(json.dumps(reports, indent=2)+'\n', encoding='utf-8')
