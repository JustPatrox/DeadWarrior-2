"""Prepare native resources locally from the user's own compatible SWF."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = "03aa17bc25851fb14aefbe9a7223c738f427bff22580e16ec93e5e1232b167b5"
STEPS = (
    ("build_menu_assets.py",),
    ("build_campaign_assets.py", "tutorial"),
    ("build_intermission_assets.py",),
    ("build_archidon_assets.py",),
    ("build_archidon_map.py",),
    ("build_shadow_assets.py",),
    ("build_archidon_map.py", "--shadow"),
    ("build_magic_assets.py",),
    ("build_archidon_map.py", "--magic"),
    ("build_rebels_assets.py",),
    ("build_archidon_map.py", "--level", "6"),
    ("build_giants_assets.py",),
    ("build_archidon_map.py", "--level", "7"),
    ("build_bomber_assets.py",),
    ("build_archidon_map.py", "--level", "8"),
    ("build_knight_assets.py",),
    ("build_archidon_map.py", "--level", "9"),
    ("build_dead_assets.py",),
    ("build_archidon_map.py", "--level", "10"),
    ("build_crawler_assets.py",),
    ("build_archidon_map.py", "--level", "11"),
    ("build_interaction_assets.py",),
    ("build_interface_assets.py",),
    ("build_castle_assets.py",),
    ("build_pc_button_assets.py",),
    ("build_toolbar_assets.py",),
    ("build_tooltip_assets.py",),
    ("build_wip_assets.py",),
    ("build_pause_assets.py",),
    ("refresh_actor_loadouts.py",),
)


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--swf", type=Path, required=True, help="Your own Stick War 2 SWF")
    parser.add_argument("--java-home", type=Path, help="Java 21 x64 JRE/JDK directory")
    parser.add_argument("--ffdec-home", type=Path, default=ROOT / "work/tools/ffdec")
    parser.add_argument("--check", action="store_true", help="Check prerequisites without generating files")
    parser.add_argument("--reuse-extraction", action="store_true", help="Reuse this SWF's existing work/extract")
    return parser.parse_args()


def validate(args):
    if sys.platform != "win32":
        raise ValueError("The current asset pipeline requires Windows x64.")
    args.swf = args.swf.resolve(strict=True)
    if hashlib.sha256(args.swf.read_bytes()).hexdigest() != SOURCE_SHA256:
        raise ValueError("Unsupported SWF revision. Expected SHA-256 " + SOURCE_SHA256)
    if args.java_home is None:
        env_home = os.environ.get("JAVA_HOME")
        local = list((ROOT / "work/tools").glob("*/bin/server/jvm.dll"))
        args.java_home = Path(env_home) if env_home else (local[0].parents[2] if local else None)
    if args.java_home is None:
        raise ValueError("Specify --java-home for a Java 21 x64 JRE/JDK.")
    args.java_home = args.java_home.resolve(strict=True)
    args.ffdec_home = args.ffdec_home.resolve(strict=True)
    for file in (args.java_home / "bin/java.exe", args.java_home / "bin/server/jvm.dll",
                 args.ffdec_home / "ffdec.jar"):
        if not file.is_file():
            raise ValueError("Missing tool: " + str(file))
    if not list((args.ffdec_home / "lib").glob("*.jar")):
        raise ValueError("FFDec's lib directory is missing; extract its complete ZIP.")
    for module in ("PIL", "numpy", "jpype", "imageio_ffmpeg"):
        if importlib.util.find_spec(module) is None:
            raise ValueError("Missing Python dependency; run pip install -r requirements-assets.txt")
    for file in ("art/menu/left.png", "art/menu/right.png", "assets/branding/max-games.png"):
        if not (ROOT / file).is_file():
            raise ValueError("Missing repository resource: " + file)
    if not (ROOT / "work/tools/flashplayer-sa.exe").is_file():
        raise ValueError("Original castle artwork export requires the optional Adobe standalone reference player at work/tools/flashplayer-sa.exe.")
    if args.reuse_extraction:
        inventory = json.loads((ROOT / "evidence/inventory.json").read_text(encoding="utf-8"))
        if inventory["metadata"]["sha256"] != SOURCE_SHA256:
            raise ValueError("The existing inventory belongs to a different SWF.")
        for folder in ("scripts", "images", "sounds", "fonts", "binaryData"):
            if not (ROOT / "work/extract" / folder).is_dir():
                raise ValueError("Incomplete extraction: missing " + folder)


def main():
    args = arguments()
    try:
        validate(args)
        print("Compatible SWF and preparation tools found.", flush=True)
        if args.check:
            print(f"Ready: {len(STEPS)} preparation stages. No files generated.")
            return 0
        (ROOT / "work").mkdir(exist_ok=True)
        (ROOT / "evidence/windows-interaction").mkdir(parents=True, exist_ok=True)
        environment = os.environ.copy()
        environment.update(DW2_SWF=str(args.swf), DW2_JAVA_HOME=str(args.java_home),
                           DW2_FFDEC_HOME=str(args.ffdec_home), PYTHONUTF8="1")
        if not args.reuse_extraction:
            subprocess.run([sys.executable, str(ROOT / "tools/inventory.py"), str(args.swf),
                            str(ROOT / "evidence/inventory.json")], cwd=ROOT, env=environment, check=True)
            print("Extracting original resources; details: work/extract.log", flush=True)
            with (ROOT / "work/extract.log").open("w", encoding="utf-8") as log:
                subprocess.run([str(args.java_home / "bin/java.exe"), "-Xmx5g", "-Djava.awt.headless=true",
                                "-jar", str(args.ffdec_home / "ffdec.jar"), "-onerror", "ignore",
                                "-export", "script,image,font,sound,binaryData,symbolClass,shape,morphshape",
                                str(ROOT / "work/extract"), str(args.swf)],
                               cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
        for index, step in enumerate(STEPS, 1):
            log_path = ROOT / f"work/assets-{index:02}.log"
            print(f"[{index}/{len(STEPS)}] {' '.join(step)} -> {log_path.name}", flush=True)
            with log_path.open("w", encoding="utf-8") as log:
                subprocess.run([sys.executable, str(ROOT / "tools" / step[0]), *step[1:]],
                               cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
        print("Native resources ready. Build with .\\tools\\build_windows.ps1", flush=True)
        return 0
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Asset preparation failed: {error}", file=sys.stderr)
        print("Check the indicated work/*.log file. The original SWF is never modified.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
