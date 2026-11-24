#!/usr/bin/env python3
"""Export the committed SDK source; excludes local SDKs, resources and firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile
from product import DEFAULT_PRODUCT, load_product, generated_files
ROOT = Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser()
ap.add_argument("--output",type=Path,default=ROOT/"build-sdk-package")
a=ap.parse_args()
def git(*args):return subprocess.check_output(["git","-C",str(ROOT),*args])
if git("status","--porcelain"):
    raise SystemExit("Commit SDK changes before packaging a reproducible source archive")
version=json.loads(generated_files(load_product(DEFAULT_PRODUCT))["product.manifest.json"])["sdk_version"]
head=git("rev-parse","HEAD").decode().strip()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
archive=out/f"impo-arcs-mini-sdk-{version}.zip"
prefix=f"impo-arcs-mini-sdk-{version}/"
subprocess.run(["git","-C",str(ROOT),"archive","--format=zip",f"--prefix={prefix}","-o",str(archive),head],check=True)
files=[x.decode() for x in git("ls-files","-z").split(b"\0") if x]
with zipfile.ZipFile(archive) as z:
    assert {x.removeprefix(prefix) for x in z.namelist() if not x.endswith("/")}==set(files)
    for name in files:assert z.read(prefix+name)==git("show",head+":"+name),name
manifest={"sdk_version":version,"commit":head,"official_dependencies":json.loads((ROOT/"dependencies.lock.json").read_text()),
          "files":len(files),"source_sha256":hashlib.sha256(archive.read_bytes()).hexdigest(),
          "excludes":["ListenAI SDK","acoustic models","AP","boot","firmware binaries","credentials","device logs","flash backups"]}
(out/"impo-arcs-mini-sdk-package.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n")
print(archive)
