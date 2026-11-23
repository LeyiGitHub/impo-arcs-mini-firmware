#!/usr/bin/env python3
"""Derive the Yunbao manifest from the official command tables and this board."""
import importlib.util, json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sdk_manifest", ROOT / "upstream/arcs/tools/gen_manifest.py")
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
core = ROOT / "upstream/arcs/gadget/gadget_core.c"
rows = module.table(core, "s_standard") + module.table(core, "s_speaker_cmds") + module.table(core, "s_light_cmds")
prompt = module.instructions(ROOT / "firmware/boards/arcs_mini_yunbao/gadget/board_yunbao.c")
json.dump({"minFirmware": "0.1.0", "systemPrompt": prompt, "commands": [module.command(row) for row in rows], "events": []}, __import__("sys").stdout, ensure_ascii=False, indent=2)
print()
