#!/usr/bin/env python3
"""Apply reviewed SDK patches, board files and public defaults to the pinned SDK."""
import argparse, json, os, subprocess, tempfile
import hashlib
from pathlib import Path
from product import DEFAULT_PRODUCT, load_product, generated_files
ROOT = Path(__file__).resolve().parents[1]

def install_managed(sdk, files):
    """Update our own overlays, refusing changes made outside this installer."""
    state_file = sdk / ".impo-arcs-mini-managed.json"
    state = json.loads(state_file.read_text()) if state_file.exists() else {}
    legacy = json.loads((ROOT / "tools/legacy_overlay_sha256.json").read_text())
    digest = lambda data: hashlib.sha256(data).hexdigest()
    for name, data in files.items():
        target = sdk / name
        if target.exists() and target.read_bytes() != data:
            old = digest(target.read_bytes())
            if state.get(name) != old:
                # Known public pre-SDK overlays can migrate without a Git
                # history (including source ZIP users); arbitrary edits cannot.
                if legacy.get(name) != old:
                    raise SystemExit("Managed SDK overlay has local changes: " + str(target))
    for name, data in files.items():
        target = sdk / name
        if not target.exists() or target.read_bytes() != data:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        state[name] = digest(data)
    # Old board-local parser moved into the public SDK component.
    for leaf in ["yunbao_wake_resources.c", "yunbao_wake_resources.h"]:
        target = sdk / "arcs/boards/arcs_mini_yunbao/gadget" / leaf
        if target.exists():
            name = str(target.relative_to(sdk))
            if digest(target.read_bytes()) == legacy.get(name):
                target.unlink()
                state.pop(name, None)
    state_file.write_text(json.dumps(state, sort_keys=True, indent=2) + "\n")

def prepare(sdk, product=DEFAULT_PRODUCT, font=None):
    sdk = sdk.resolve()
    profile = load_product(product)
    generated = generated_files(profile, font)
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
    files = {}
    for source, dest in [(ROOT / "firmware/boards/arcs_mini_yunbao", "arcs/boards/arcs_mini_yunbao"),
                         (ROOT / "sdk", "arcs/components/impo_arcs_mini")]:
        for file in source.rglob("*"):
            if file.is_file():
                files[str(Path(dest) / file.relative_to(source))] = file.read_bytes()
    for name, data in generated.items():
        files[f"arcs/products/{profile['id']}/{name}"] = data
    install_managed(sdk, files)
    return sdk
if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--sdk", type=Path, default=ROOT / "upstream")
    ap.add_argument("--product", type=Path, default=DEFAULT_PRODUCT)
    ap.add_argument("--font", type=Path)
    a = ap.parse_args()
    print(prepare(a.sdk, a.product, a.font))
