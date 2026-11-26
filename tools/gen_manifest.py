#!/usr/bin/env python3
"""Generate the product manifest using official command-table parsing."""
import argparse, importlib.util, json, re, sys
from pathlib import Path
from product import DEFAULT_PRODUCT, load_product
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sdk_manifest", ROOT / "upstream/arcs/tools/gen_manifest.py")
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
ap = argparse.ArgumentParser()
ap.add_argument("--product", type=Path, default=DEFAULT_PRODUCT)
ap.add_argument("--commands", action="append", default=[], metavar="FILE:TABLE", help="Additional official-style C command table; repeat as needed")
ap.add_argument("--events", action="append", default=[], metavar="FILE:TABLE", help="Additional official-style C event table")
ap.add_argument("--no-companion", action="store_true", help="Custom --app-dir replaces the default companion application")
ap.add_argument("--instructions",type=Path,help="Shared-product instructions, independent of per-device names and wake words")
a = ap.parse_args()
p = load_product(a.product)
core = ROOT / "upstream/arcs/gadget/gadget_core.c"
rows = module.table(core, "s_standard") + module.table(core, "s_speaker_cmds") + module.table(core, "s_light_cmds")
rows += module.table(core, "s_camera_cmds")
companion = ROOT / "sdk/src/impo_arcs_companion.c"
if not a.no_companion:
    rows += module.table(companion, "companion_commands")
def extra(value):
    file, table = value.rsplit(":",1)
    return module.table(Path(file),table)
for value in a.commands: rows += extra(value)
commands = [module.command(row) for row in rows]
if len({c["name"] for c in commands}) != len(commands):
    raise SystemExit("Duplicate command names")
events=[]
event_rows = module.table(companion,"companion_events") if not a.no_companion else []
for value in a.events: event_rows += extra(value)
for name, description in event_rows:
    id_name = "timer_id" if name=="timer_fired" else "press_count" if name=="button_pressed" else "playback_id"
    schema={"type":"object","properties":{"boot_id":{"type":"string"},id_name:{"type":"integer"}},"required":["boot_id",id_name]}
    events.append({"name":name,"description":description,"data":schema if name in {"timer_fired","button_pressed","playback_completed","playback_interrupted","playback_failed"} else None})
if len({e["name"] for e in events})!=len(events): raise SystemExit("Duplicate event names")
prompt=a.instructions.read_text().strip() if a.instructions else p["instructions"]
if not prompt or len(prompt)>4000 or '\0' in prompt: raise SystemExit("Instructions must be 1..4000 characters without NUL")
version=re.search(r'#define IMPO_ARCS_MINI_SDK_VERSION "([^"\n]+)"',(ROOT/"sdk/include/impo_arcs_mini.h").read_text())[1]
manifest={"minFirmware": version, "systemPrompt": prompt, "commands": commands, "events": events}
if not a.no_companion: manifest["scenarios"]=json.loads((ROOT/"products/companion-scenarios.json").read_text())
json.dump(manifest, sys.stdout, ensure_ascii=False, indent=2)
print()
