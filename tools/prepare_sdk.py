#!/usr/bin/env python3
"""Apply reviewed SDK patches, board files and public defaults to the pinned SDK."""
import argparse, json, os, shutil, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def prepare(sdk):
    sdk = sdk.resolve()
    expected = json.loads((ROOT / "dependencies.lock.json").read_text())["impo_gadget_sdk"]["commit"]
    if not sdk.exists():
        subprocess.run(["git", "init", str(sdk)], check=True)
        subprocess.run(["git", "-C", str(sdk), "remote", "add", "origin", "https://github.com/impoai/impo-gadget-sdk.git"], check=True)
        subprocess.run(["git", "-C", str(sdk), "fetch", "--depth", "1", "origin", expected], check=True)
        subprocess.run(["git", "-C", str(sdk), "checkout", "--detach", expected], check=True)
    actual = subprocess.check_output(["git", "-C", str(sdk), "rev-parse", "HEAD"], text=True).strip()
    if actual != expected:
        raise SystemExit("SDK revision mismatch: inspect the pinned upstream checkout")
    patches = sorted((ROOT / "patches").glob("*.patch"))
    # Calculate the expected patched tree without changing the user's Git index.
    with tempfile.TemporaryDirectory() as temp:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(temp) / "index"))
        git = ["git", "-C", str(sdk)]
        subprocess.run([*git, "read-tree", "HEAD"], env=env, check=True)
        for patch in patches:
            subprocess.run([*git, "apply", "--cached", str(patch)], env=env, check=True)
        expected_tree = subprocess.check_output([*git, "write-tree"], env=env, text=True).strip()
    if subprocess.run([*git, "diff", "--quiet", expected_tree, "--", "arcs", "esp32"]).returncode:
        if subprocess.run([*git, "diff", "--quiet", "HEAD", "--", "arcs", "esp32"]).returncode:
            raise SystemExit("SDK changes differ from this project's patches; inspect before building")
        for patch in patches:
            subprocess.run([*git, "apply", str(patch)], check=True)
    source = ROOT / "firmware/boards/arcs_mini_yunbao"
    dest = sdk / "arcs/boards/arcs_mini_yunbao"
    for file in source.rglob("*"):
        if file.is_file():
            target = dest / file.relative_to(source)
            if target.exists() and target.read_bytes() != file.read_bytes():
                raise SystemExit("Board destination has local changes: " + str(target))
    shutil.copytree(source, dest, dirs_exist_ok=True)
    conf = sdk / "arcs/app/yunbao.conf"
    data = (ROOT / "configs/yunbao.conf").read_bytes()
    if conf.exists() and conf.read_bytes() != data:
        raise SystemExit("Public build config has local changes: " + str(conf))
    conf.write_bytes(data)
    return sdk
if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--sdk", type=Path, default=ROOT / "upstream")
    print(prepare(ap.parse_args().sdk))
