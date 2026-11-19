#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Builds the Impo product manifest (contracts/gadget-identity.md, ManifestInput)
for the M5Stack StackChan build (devices/sdkconfig.impo-m5stack-stackchan).
The firmware registers only command names; everything the agent reads about
them is here, and the platform checks calls against these schemas. Refuses to
build when a command here is not one the firmware registers.

usage: tools/stackchan_manifest.py [--min-firmware 0.1.0] > stackchan.manifest.json
       then impo.py manifest push stackchan.manifest.json (impo-developer-skills)
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARD = ROOT / "components/impo/boards/board_m5stack_cores3.c"
OVERLAY = ROOT / "devices/sdkconfig.impo-m5stack-stackchan"
COMMON = ROOT / "components/impo/impo_commands.c"

SYSTEM_PROMPT = """\
You are the voice and body of a StackChan: a small M5Stack desktop robot. Its face is a 2-inch \
touch screen showing Impo's avatar, on a body whose head turns left and right and tilts up and \
down. It has a speaker and microphones (the person holds the power button to talk to you, and \
your replies are spoken), a touch strip along the top of its head, and 12 coloured LEDs in its \
body. It sits on a desk near the person.

Body language: use the head the way a person would, to go with what you say: head.nod for yes \
or agreement, head.shake for no, head.look to look towards something or someone (yaw -120 to \
120 degrees, pitch 0 straight ahead to 85 looking up) and head.center to look straight ahead \
again. Keep it small and occasional: one gesture per reply at most, never a stream of moves, \
and none when the person asks for quiet. Moves return at once and take about a second.

Lights: lights.set shows one colour on all the LEDs (a name such as warm, pink, blue or green, \
or #rrggbb) at a brightness; lights.off turns them off. Use them for mood or a signal the \
person asked for, not on every reply. They stay until changed.

Touch: when the person pats or strokes its head you get head_touched. Answer as the robot \
being petted would: one short warm phrase, no questions, no lists.

Speak in short, friendly sentences. The screen shows what you say; display.show_text puts a \
short caption on it when the person wants something written down."""

EVENTS = [
    {"name": "head_touched",
     "description": "The person touched the top of the robot's head and let go: how is pat (a touch) or "
                    "stroke (a hand moved along the head). The robot's LEDs already glowed pink. Sent at "
                    "most every 5 seconds."},
]


def registered(path, name):
    """The command names in `name[] = { {"a.b", run}, ... };`."""
    src = path.read_text()
    m = re.search(re.escape(name) + r'\[\]\s*=\s*\{(.*?)\n\};', src, re.S)
    if not m:
        sys.exit(f"{name} not found in {path}")
    return re.findall(r'\{\s*"([^"]+)"', m.group(1))


def command(name, desc, req, opt):
    req = json.loads(req) if req else {}
    opt = json.loads(opt) if opt else {}
    schema = {"type": "object", "properties": {**req, **opt}}
    if req:
        schema["required"] = list(req)
    schema["additionalProperties"] = False
    assert re.fullmatch(r'[a-z][a-z0-9_]*(\.[a-z0-9_]+){0,3}', name), name
    assert 1 <= len(desc) <= 500, (name, len(desc))
    return {"name": name, "description": desc, "parameters": schema}


TIMES = '{"times":{"type":"integer","minimum":1,"maximum":5,"description":"How many times, 1 to 5; 2 if left out."}}'

# The body's commands (stackchan_head.c, listed in board_m5stack_cores3.c).
BODY = [
    ("head.look",
     "Turn and tilt the robot's head to look somewhere. Give yaw, pitch or both; what is left out "
     "stays. It eases there in about a second (or duration_ms) and returns at once.",
     None,
     '{"yaw":{"type":"number","minimum":-120,"maximum":120,"description":"Degrees: 0 straight ahead, negative to the robot\'s left, positive to its right."},'
     '"pitch":{"type":"number","minimum":0,"maximum":85,"description":"Degrees: 0 straight ahead, up to 85 looking up."},'
     '"duration_ms":{"type":"integer","minimum":100,"maximum":5000,"description":"How long the move takes, 100 to 5000."}}'),
    ("head.nod",
     "Nod the robot's head: yes, agreement or hello. Returns at once; the nod takes under a second "
     "a time.",
     None, TIMES),
    ("head.shake",
     "Shake the robot's head from side to side: no, or disbelief. Returns at once.",
     None, TIMES),
    ("head.center",
     "Turn the robot's head back to straight ahead.",
     None, None),
    ("head.status",
     "Where the head points now (yaw and pitch in degrees), whether it is moving, and whether the "
     "servos are holding it (they let go when it rests, so it can be turned by hand).",
     None, None),
    ("lights.set",
     "Light the 12 LEDs in the robot's body in one colour. They stay on until changed or turned off.",
     '{"color":{"type":"string","minLength":3,"maxLength":7,"description":"red, orange, yellow, green, cyan, blue, purple, pink, white, warm, or #rrggbb."}}',
     '{"brightness":{"type":"integer","minimum":1,"maximum":100,"description":"Percent, 1 to 100; 40 if left out."}}'),
    ("lights.off",
     "Turn off the LEDs in the robot's body.",
     None, None),
]

# Commands every board with the full UI registers (impo_commands.c COMMON).
UI = [
    ("display.show_text",
     "Show a short caption on the robot's screen, for something the person wants written down. It "
     "stays until the next thing the robot shows.",
     '{"text":{"type":"string","minLength":1,"maxLength":200,"description":"1 to 200 characters."}}',
     None),
    ("avatar.cheer",
     "Make the face on the screen cheer for a moment.",
     None, None),
    ("speaker.say",
     "Say something out loud in Impo's voice, outside a reply (a reminder, a greeting). Returns at once; "
     "the sound starts a few seconds later.",
     '{"text":{"type":"string","minLength":1,"maxLength":1000,"description":"What to say, any language."}}',
     None),
    ("speaker.set_volume",
     "Set the speaker volume.",
     '{"percent":{"type":"integer","minimum":0,"maximum":100,"description":"0 to 100."}}',
     None),
    ("speaker.stop",
     "Stop what the speaker is playing.",
     None, None),
]

# Link's own commands (main/noise_control.cpp), which every ESP32 gadget registers.
LINK = [
    ("device.health",
     "The robot's battery level, whether it is charging, uptime, memory and the Wi-Fi network.",
     None, None),
    ("device.set_name",
     "Rename the robot: the name the person calls it by. 1 to 32 characters. It reconnects under the "
     "new name a moment later.",
     '{"name":{"type":"string","minLength":1,"maxLength":32,"description":"The new name, 1 to 32 characters."}}',
     None),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-firmware", default="0.1.0")
    a = ap.parse_args()
    body = registered(BOARD, "s_commands")
    if [c[0] for c in BODY] != body:
        sys.exit(f"BODY {[c[0] for c in BODY]} differs from what {BOARD.name} registers: {body}")
    common = registered(COMMON, "COMMON")
    missing = [c[0] for c in UI if c[0] not in common]
    if missing:
        sys.exit(f"{missing} are not in {COMMON.name} COMMON")
    commands = [command(*c) for c in BODY + UI + LINK]
    names = [c["name"] for c in commands]
    assert len(names) == len(set(names)) and len(commands) <= 64
    assert len(SYSTEM_PROMPT) <= 4000, len(SYSTEM_PROMPT)
    product = re.search(r'^CONFIG_GADGET_PRODUCT_ID="([^"]+)"', OVERLAY.read_text(), re.M)
    if not product:
        sys.exit(f"no CONFIG_GADGET_PRODUCT_ID in {OVERLAY.name}")
    json.dump({"product": product.group(1), "minFirmware": a.min_firmware, "systemPrompt": SYSTEM_PROMPT, "commands": commands,
               "events": EVENTS}, sys.stdout, ensure_ascii=False, indent=2)
    print()


if __name__ == "__main__":
    main()
