#!/usr/bin/env python3
"""Generate the product manifest using official command-table parsing."""
import argparse, importlib.util, json, sys
from pathlib import Path
from product import DEFAULT_PRODUCT, load_product
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sdk_manifest", ROOT / "upstream/arcs/tools/gen_manifest.py")
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
ap = argparse.ArgumentParser()
ap.add_argument("--product", type=Path, default=DEFAULT_PRODUCT)
ap.add_argument("--commands", action="append", default=[], metavar="FILE:TABLE", help="Additional official-style C command table; repeat as needed")
ap.add_argument("--events", action="append", default=[], metavar="FILE:TABLE", help="Additional official-style C event table")
a = ap.parse_args()
p = load_product(a.product)
core = ROOT / "upstream/arcs/gadget/gadget_core.c"
rows = module.table(core, "s_standard") + module.table(core, "s_speaker_cmds") + module.table(core, "s_light_cmds")
def extra(value):
    file, table = value.rsplit(":",1)
    return module.table(Path(file),table)
for value in a.commands: rows += extra(value)
commands = [module.command(row) for row in rows]
if len({c["name"] for c in commands}) != len(commands):
    raise SystemExit("Duplicate command names")
events=[]
for value in a.events:
    for name, description in extra(value):
        events.append({"name":name,"description":description,"data":None})
json.dump({"minFirmware": "0.1.0", "systemPrompt": p["instructions"], "commands": commands, "events": events}, sys.stdout, ensure_ascii=False, indent=2)
print()
