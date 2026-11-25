"""Check the public source tree for local/generated files and broken doc links."""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ALLOWED_ASSETS = {"assets/branding/max-games.png"}


def resolve_local_link(root, document, target):
    """Resolve a doc link while rejecting filesystem escapes from the repo."""
    repository = Path(root).resolve()
    resolved = (repository / Path(document).parent / target).resolve()
    try:
        return resolved.relative_to(repository).as_posix()
    except ValueError as error:
        raise ValueError("Local documentation link escapes the repository") from error


def main():
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode("utf-8").split("\0")
    errors = []
    paths = {name for name in tracked if name}
    for name in sorted(paths):
        path = Path(name)
        if (path.parts[0] in {"work", "build", "out", "dist", "evidence", ".vs", ".venv"}
                or (name.startswith("assets/") and name not in ALLOWED_ASSETS)
                or path.suffix.lower() in {".swf", ".fla", ".exe", ".dll", ".sav", ".log", ".pem", ".key"}
                or path.name in {"options.ini", "options.ini.tmp"}
                or "__pycache__" in path.parts or path.name.startswith(".env")):
            errors.append("Local/generated file is tracked: " + name)
        if (ROOT / name).stat().st_size > 10 * 1024 * 1024:
            errors.append("Unexpected source file larger than 10 MiB: " + name)
    for name in sorted(n for n in paths if n.endswith(".md")):
        content = (ROOT / name).read_text(encoding="utf-8")
        targets = re.findall(r"\]\(([^)\s]+)\)|\bsrc=\"([^\"]+)\"", content)
        for link, image in targets:
            target = (link or image).split("#", 1)[0]
            if not target or re.match(r"[a-z]+:", target, re.I):
                continue
            try:
                resolved = resolve_local_link(ROOT, name, target)
            except ValueError:
                errors.append(f"Local link escapes repository in {name}: {target}")
                continue
            if resolved not in paths or not (ROOT / resolved).is_file():
                errors.append(f"Broken local link in {name}: {target}")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Public source tree checked: {len(paths)} files; no game extraction or build products.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
