"""Extract prepared assets or package a native installation for a release."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import zipfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action",choices=("assets","pack"))
    parser.add_argument("--archive",type=Path)
    parser.add_argument("--prefix",type=Path,default=Path("out/package"))
    parser.add_argument("--name")
    args=parser.parse_args()
    if args.action=="assets":
        with zipfile.ZipFile(args.archive) as archive:
            entries=[info for info in archive.infolist() if info.filename.startswith("assets/")]
            if not entries: raise ValueError("No prepared assets in the seed archive")
            for info in entries:
                path=Path(info.filename)
                if ".." in path.parts or path.is_absolute(): raise ValueError("Unsafe archive member")
                archive.extract(info,Path.cwd())
        if not Path("assets/menu/textures.tsv").is_file(): raise ValueError("Incomplete prepared assets")
        print(f"Prepared assets: {len(entries)} files")
        return
    out=Path("out/releases");out.mkdir(parents=True,exist_ok=True)
    if args.name.startswith("macOS"):
        app=args.prefix/"DeadWarrior 2.app"
        # No Developer ID credential is required for an ad-hoc signature. This
        # verifies bundle integrity but does not provide Apple notarization.
        subprocess.run(["codesign","--force","--deep","--sign","-",str(app)],check=True)
        subprocess.run(["codesign","--verify","--deep","--strict",str(app)],check=True)
        file=out/f"DeadWarrior-2-{args.name}.zip"
        subprocess.run(["ditto","-c","-k","--sequesterRsrc","--keepParent",str(app),str(file)],check=True)
    elif args.name.startswith("Linux"):
        file=out/f"DeadWarrior-2-{args.name}.tar.gz"
        with tarfile.open(file,"w:gz") as archive: archive.add(args.prefix,arcname="DeadWarrior-2")
    else:
        file=out/f"DeadWarrior-2-{args.name}.zip"
        with zipfile.ZipFile(file,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
            for entry in sorted(args.prefix.rglob("*")):
                if entry.is_file(): archive.write(entry,entry.relative_to(args.prefix))
    digest=hashlib.sha256(file.read_bytes()).hexdigest()
    file.with_suffix(file.suffix+".sha256").write_text(f"{digest}  {file.name}\n")
    print(file,digest)


if __name__=="__main__":main()
