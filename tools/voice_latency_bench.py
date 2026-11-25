#!/usr/bin/env python3
"""Fixed external-audio, interleaved voice bench; raw logs/audio stay private.

Uses an explicitly selected, already identified ARCS device and the official shell.
Does not flash, pair, inject PCM, retry questions, or change TTS settings.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import select
import subprocess
import tempfile
import time

ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
TURN = re.compile(r"voice_turn boot=([0-9a-f]+) epoch=(\d+) session=(\d+) overlap=(\d+) valid=([0-9a-f]+) pcm=(\d+) queued=(\d+) backlog=(\d+) dropped=(\d+) failed=(\d+) capture=(\d+) upload=(\d+) ack=(\d+) text=(\d+) tts=(\d+)")
KEYS = "boot epoch session overlap valid pcm queued backlog dropped failed capture upload ack text tts".split()


def private_write(path, data):
    path = Path(path)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix="."+path.name, delete=False) as f:
        temporary = Path(f.name)  # NamedTemporaryFile is private (0600).
        try:
            f.write(data)
            f.flush()
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def mac_volume():
    text=subprocess.check_output(["osascript","-e","get volume settings"],text=True)
    return int(re.search(r"output volume:(\d+)",text)[1]), "output muted:true" in text


def speaker_volume(sh):
    text=sh.command("gadget speaker.status",.6)
    m=re.search(r'"volume_percent":(\d+)',text)
    if not m:raise RuntimeError("Cannot read device speaker volume")
    return int(m[1])


def set_speaker_volume(sh, percent):
    sh.command('gadget speaker.set_volume {"percent":'+str(percent)+'}',.6)
    if speaker_volume(sh)!=percent:raise RuntimeError("Device speaker volume not applied")


def pending_trials(modes, cycles, completed):
    """A failed attempt is completed too: explicit resume never resubmits it."""
    for cycle in range(cycles):
        offset=cycle%len(modes)
        order=modes[offset:]+modes[:offset]
        if (cycle//len(modes))%2:order=list(reversed(order))
        for mode,pause in order:
            if (cycle+1,mode,pause) not in completed:
                yield cycle+1,mode,pause


def parse_snapshot(text):
    """Short shell lines remain usable when unrelated console logging interleaves."""
    head=re.search(r"voice phase=(\d+) boot=([0-9a-f]+) epoch=(\d+) session=(\d+) overlap=(\d+) valid=([0-9a-f]+) failed=(\d+)",text)
    body=re.search(r"voice bytes pcm=(\d+) queued=(\d+) backlog=(\d+) dropped=(\d+) upload_stack_free=(\d+)",text)
    if not head or not body:raise ValueError("Incomplete voice snapshot")
    row={k:v if k in ["boot","valid"] else int(v) for k,v in zip(
        ["phase","boot","epoch","session","overlap","valid","failed"],head.groups())}
    row.update({k:int(v) for k,v in zip(["pcm","queued","backlog","dropped","upload_stack_free"],body.groups())})
    row["stages"]={k:int(v) for k,v in re.findall(r"voice stage=(\d+) mono_ms=(\d+)",text)}
    if "1" in row["stages"]:row["capture"]=row["stages"]["1"]
    return row


def parse_reply_diagnostics(text, epoch):
    row = {}
    patterns = [
        (r"voice_sub_turn epoch=(\d+) ready=(\d+) bytes=(\d+) lines=(\d+) complete=(\d+)",
         ["ready", "bytes", "lines", "complete"]),
        (r"voice_filter_turn epoch=(\d+) missing=(\d+) not_ready=(\d+) duplicate=(\d+) parent=(\d+) state=(\d+)",
         ["missing_parent", "not_ready", "duplicate", "rejected_parent", "rejected_state"]),
    ]
    for pattern, keys in patterns:
        for match in re.finditer(pattern, text):
            if int(match[1]) == epoch:
                row.update({k: int(v) for k, v in zip(keys, match.groups()[1:])})
    return row


class Shell:
    def __init__(self, adb, serial):
        self.proc = subprocess.Popen([adb, "-s", serial, "shell"], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.raw = bytearray()
        self.events = []

    def drain(self, seconds):
        begin = len(self.raw)
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if select.select([self.proc.stdout], [], [], min(.05, max(0, end-time.monotonic())))[0]:
                b = os.read(self.proc.stdout.fileno(), 65536)
                if not b:
                    raise RuntimeError("ADB shell disconnected")
                stamp = time.monotonic()
                self.raw.extend(b)
                self.events.append((stamp, b))
        return self.clean(self.raw[begin:])

    @staticmethod
    def clean(b):
        return ANSI.sub("", bytes(b).decode(errors="replace")).replace("\0", "")

    def command(self, text, wait=.3):
        self.proc.stdin.write((text+"\r").encode())
        self.proc.stdin.flush()
        return self.drain(wait)

    def idle(self, timeout=20):
        end = time.monotonic()+timeout
        while time.monotonic() < end:
            s = self.command("wake")
            m = re.search(r"wake ready=1 enabled=1 listening=(\d+).*?error=0 dialogue=(\d+) word=([^\r\n]+)", s)
            if m and m[1] == "1" and m[2] == "0":
                return m[3].strip()
            self.drain(.5)
        raise RuntimeError("Device did not become idle with wake enabled")


def run_trial(sh, args, index, mode, pause):
    sh.idle()
    if args.speaker_volume is not None and speaker_volume(sh)!=args.speaker_volume:
        raise RuntimeError("Device volume changed; stopping to preserve user setting")
    if args.mac_output_volume is not None and mac_volume()!=(args.mac_output_volume,False):
        raise RuntimeError("Mac output changed; stopping to preserve user setting")
    if "state=online" not in sh.command("link"):
        raise RuntimeError("Device is not online; no question sent")
    configured = sh.command(f"voice_latency {mode} {pause}")
    if f"voice configured mode={mode}" not in configured:
        raise RuntimeError("Voice configuration refused; no question sent")
    start = len(sh.raw)
    event_start = len(sh.events)
    stem = args.output / f"trial-{index:03d}-{mode}-{pause}"
    rec = player = None
    result = {"index": index, "mode": mode, "endpoint_ms": pause, "ok": False}
    try:
        if args.recorder:
            rec = subprocess.Popen([str(args.recorder), str(stem)+".wav"], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if not select.select([rec.stdout], [], [], 5)[0]:
                raise RuntimeError("Acoustic recorder did not start")
            ready = rec.stdout.readline().split()
            if len(ready) != 2 or ready[0] != "READY":
                raise RuntimeError("Acoustic recorder refused capture")
            # Some macOS Python runtimes use a process-relative monotonic epoch.
            # Do not mix Swift's systemUptime origin with Python's origin.
            result["recorder_system_uptime"] = float(ready[1])
            result["record_host_mono"] = time.monotonic()
        sh.drain(.8)
        sh.command("talk", .05)
        end = time.monotonic()+6
        while time.monotonic() < end:
            if re.search(r"gadget_voice.*listening", sh.clean(sh.raw[start:])):
                break
            # Asynchronous shell/log output may interleave the human log line.
            # A fresh canonical snapshot is authoritative before any stimulus.
            try:
                if parse_snapshot(sh.command("voice_latency", .1))["phase"] == 1:
                    break
            except ValueError:
                pass
            sh.drain(.05)
        else:
            raise RuntimeError("Listening did not start; question not played")
        # The VAD first measures 500 ms of quiet. Keep the stimulus out of it.
        sh.drain(.7)
        result["stimulus_host_mono"] = time.monotonic()
        player = subprocess.Popen(["afplay", "-v", str(args.volume), str(args.stimulus)])
        while player.poll() is None:
            sh.drain(.05)
        if player.returncode:
            raise RuntimeError("Question playback failed")
        result["stimulus_end_host_mono"] = time.monotonic()
        end = time.monotonic()+args.timeout
        while time.monotonic() < end:
            rows = TURN.findall(sh.clean(sh.raw[start:]))
            if rows:
                row = rows[0]
                result.update({k: v if k in ("boot", "valid") else int(v) for k, v in zip(KEYS, row)})
                result["ok"] = result["failed"] == 0 and result["dropped"] == 0 and result["valid"] == "7f"
                break
            sh.drain(.1)
        else:
            raise RuntimeError("Voice turn timed out; result unknown, no retry")
        sh.drain(.08)  # The bounded per-turn diagnostic lines follow the summary.
        text = sh.clean(sh.raw[start:])
        result["reply_diagnostics"] = parse_reply_diagnostics(text, result["epoch"])
        result["chat_requests"] = len(re.findall(r"stream \d+: POST /chat/stream", text))
        result["tts_requests"] = len(re.findall(r"stream \d+: POST /tts", text))
        # Exclude listening beeps and support both legacy and queued playback.
        begins = re.split(r"begin (?:say|speech)\b", text, maxsplit=1)
        speech = begins[1] if len(begins) == 2 else ""
        decoded = re.search(r"speech decoded gen=(\d+) segments=(\d+)", speech)
        expected_tts = int(decoded[2]) if decoded else 1
        result["ok"] &= result["chat_requests"] == 1 and result["tts_requests"] == expected_tts
        result["answer_buffer_ready"] = bool(decoded or re.search(r"speech buffer ready:", speech))
        if decoded: result["answer_segments_decoded"] = expected_tts
        result["answer_ended"] = "ended; last 10 blocks" in speech
        result["ok"] &= result["answer_buffer_ready"] and result["answer_ended"]
        result["voice_ok"] = result["ok"]
        for stage in ["first_byte", "first_pcm"]:
            m = re.search(r"speech_timing gen=(\d+) stage="+stage+r" mono_ms=(\d+)", speech)
            if m:
                result["speech_gen"] = int(m[1])
                result[stage+"_gen"] = int(m[1])
                result[stage+"_ms"] = int(m[2])
        m = re.search(r"sound_first_write gen=(\d+) mono_ms=(\d+) result=0", speech)
        if m:
            result["first_write_ms"] = int(m[2])
            result["first_write_gen"] = int(m[1])
        if all(k in result for k in ["first_byte_gen","first_pcm_gen","first_write_gen"]):
            result["audio_timing_generation_matches"] = result["first_byte_gen"] == result["first_pcm_gen"] == result["first_write_gen"]
        # Host arrival only bounds an acoustic search window; never subtract it
        # from the device clock. Raw waveform gives both ends of the acoustic gap.
        tail = ""
        for stamp, data in sh.events[event_start:]:
            chunk = tail + sh.clean(data)
            ready = "speech buffer ready:" in chunk
            if decoded:
                ready = bool(re.search(r"sound_first_write gen="+decoded[1]+r" mono_ms=", chunk))
            if ready and "ready_host_mono" not in result:
                result["ready_host_mono"] = stamp
            if "listened " in chunk and "capture_host_mono" not in result:
                result["capture_host_mono"] = stamp
            tail = chunk[-150:]
        snapshot = sh.command("voice_latency")
        result["stages"] = {int(k): int(v) for k, v in re.findall(r"voice stage=(\d+) mono_ms=(\d+)", snapshot)}
        # Bind acoustic-tail alignment to this summary, even if automatic
        # continuous capture has already replaced the shell's live snapshot.
        for epoch, last_voice in re.findall(r"voice_capture_turn epoch=(\d+) last_voice=(\d+)", text):
            if int(epoch) == result["epoch"] and int(result["valid"],16) & 1:
                result["stages"][0] = int(last_voice)
    except KeyboardInterrupt:
        result["error"] = "Interrupted; partial trial retained, no retry"
        result["ok"] = False
    except Exception as e:
        result["error"] = str(e)
    finally:
        try:
            if player and player.poll() is None:
                player.terminate();player.wait(timeout=5)
            if rec:
                if rec.poll() is None:
                    rec.stdin.write("stop\n")
                    rec.stdin.flush()
                rec.wait(timeout=5)
                if rec.returncode:
                    raise RuntimeError("Recorder failed: "+rec.stderr.read()[:300])
        except Exception as e:
            result["record_error"] = str(e)
            result["ok"] = False
        private_write(str(stem)+".log", sh.raw[start:])
        # End the next continuous capture explicitly; never resubmit a trial.
        try:
            s = sh.command("wake")
            if "dialogue=1" in s:
                sh.command("talk")
            sh.idle()
        except Exception as e:
            result["cleanup_error"] = str(e)
            result.setdefault("error", "Cleanup failed; identify device before further trials")
            result["ok"] = False
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--adb", default="adb")
    ap.add_argument("--serial", required=True)
    ap.add_argument("--expected-boot", help="Boot identifier captured after physical device identification")
    ap.add_argument("--image-metadata", type=Path, help="Verified installed-image ledger (only public image fields copied)")
    ap.add_argument("--stimulus", required=True, type=Path, help="Fixed question, independent of /tts")
    ap.add_argument("--output", required=True, type=Path, help="Private directory, outside the repository")
    ap.add_argument("--resume", action="store_true", help="Explicitly continue only unattempted slots in the same boot/conditions")
    ap.add_argument("--recorder", type=Path, help="Compiled macos_voice_capture helper")
    ap.add_argument("--cycles", type=int, default=1)
    ap.add_argument("--modes", nargs="+", default=["batch:1200", "overlap:1200", "overlap:800"])
    ap.add_argument("--volume", type=float, default=2)
    ap.add_argument("--speaker-volume", type=int, help="Temporary device volume; restore original unless changed during test")
    ap.add_argument("--mac-output-volume", type=int, help="Temporary Mac output volume; does not unmute; restore afterwards")
    ap.add_argument("--timeout", type=float, default=180)
    args = ap.parse_args()
    if args.cycles < 1 or not args.stimulus.is_file():
        ap.error("Positive cycles and an existing stimulus are required")
    if any(v is not None and not 0<=v<=100 for v in [args.speaker_volume,args.mac_output_volume]):
        ap.error("Volume must be 0..100")
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[1]):
        ap.error("Raw audio/logs must be outside the repository")
    if args.output.exists() and any(args.output.iterdir()) and not args.resume:
        ap.error("Output is not empty; use a new directory")
    modes = []
    for value in args.modes:
        mode, pause = value.split(":")
        if mode not in ("batch", "overlap") or not 600 <= int(pause) <= 2000:
            ap.error("Modes must be batch|overlap:600..2000")
        modes.append((mode, int(pause)))
    args.output.mkdir(parents=True, mode=0o700, exist_ok=True)
    args.output.chmod(0o700)
    lock_file=os.fdopen(os.open(args.output/".bench.lock",os.O_RDWR|os.O_CREAT,0o600),"wb")
    try:
        fcntl.flock(lock_file,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:
        ap.error("Another bench is using this output directory")
    manifest={"stimulus_sha256":hashlib.sha256(args.stimulus.read_bytes()).hexdigest(),
              "modes":modes,"cycles":args.cycles,"afplay_volume":args.volume,
              "acoustic_capture":bool(args.recorder)}
    if args.image_metadata:
        meta=json.loads(args.image_metadata.read_text())
        manifest["installed_image"]={k:meta[k] for k in ["image_sha256","image_bytes","product","listenai_sdk_version","wake_bundle"] if k in meta}
    results=[]
    if args.resume:
        if not (args.output/"trials.json").is_file() or not (args.output/"manifest.json").is_file():
            ap.error("Resume needs existing trials and manifest")
        old=json.loads((args.output/"manifest.json").read_text())
        results=json.loads((args.output/"trials.json").read_text())
        normalized=json.loads(json.dumps(manifest))
        for k in ["stimulus_sha256","modes","cycles","afplay_volume","acoustic_capture","installed_image"]:
            if old.get(k)!=normalized.get(k):ap.error("Resume condition changed: "+k)
        for k,v in [("speaker_volume_percent",args.speaker_volume),("mac_output_volume_percent",args.mac_output_volume)]:
            if old.get(k)!=v:ap.error("Resume condition changed: "+k)
        if not args.expected_boot or any(r.get("boot")!=args.expected_boot for r in results if "boot" in r):
            ap.error("Resume requires the same verified boot")
        manifest=old
    completed={(r["cycle"],r["mode"],r["endpoint_ms"]) for r in results}
    if len(completed)!=len(results):ap.error("Duplicate attempted slots in existing results")
    if args.resume and "segments" not in manifest:
        manifest["segments"]=[{"first_index":1,"last_index":len(results),
                               "original_speaker_volume_percent":manifest.get("original_speaker_volume_percent"),
                               "original_mac_output_volume_percent":manifest.get("original_mac_output_volume_percent")}]
    segment={"first_index":len(results)+1}
    manifest.setdefault("segments",[]).append(segment)
    session_number=len(manifest["segments"])
    private_write(args.output/"manifest.json",(json.dumps(manifest,indent=2)+"\n").encode())
    sh = Shell(args.adb, args.serial)
    consecutive_failures = 0
    original_speaker = original_mac = None
    try:
        sh.command(""); sh.drain(1)
        initial = sh.command("voice_latency")
        if args.expected_boot and f"boot={args.expected_boot} " not in initial:
            raise RuntimeError("Boot identifier changed; identify the connected device again")
        if args.speaker_volume is not None:
            original_speaker=speaker_volume(sh)
            set_speaker_volume(sh,args.speaker_volume)
            manifest["speaker_volume_percent"]=args.speaker_volume
            manifest.setdefault("original_speaker_volume_percent",original_speaker)
            segment["original_speaker_volume_percent"]=original_speaker
        if args.mac_output_volume is not None:
            original_mac=mac_volume()
            if original_mac[1]:raise RuntimeError("Mac output is muted; no change applied")
            subprocess.run(["osascript","-e",f"set volume output volume {args.mac_output_volume}"],check=True)
            manifest["mac_output_volume_percent"]=args.mac_output_volume
            manifest.setdefault("original_mac_output_volume_percent",original_mac[0])
            segment["original_mac_output_volume_percent"]=original_mac[0]
        private_write(args.output/"manifest.json",(json.dumps(manifest,indent=2)+"\n").encode())
        for cycle,mode,pause in pending_trials(modes,args.cycles,completed):
            r = run_trial(sh, args, len(results)+1, mode, pause)
            r["cycle"] = cycle
            results.append(r)
            private_write(args.output/"trials.json", (json.dumps(results, indent=2)+"\n").encode())
            progress = {k:r[k] for k in ["index","cycle","mode","endpoint_ms","ok","failed","error"] if k in r}
            if r.get("capture") and r.get("upload"):
                progress["capture_to_queued_ms"] = r["upload"]-r["capture"]
            print(json.dumps(progress), flush=True)
            consecutive_failures = 0 if r["ok"] else consecutive_failures+1
            if r.get("error"):
                raise RuntimeError("Bench stopped after inconclusive trial; partial results saved")
            if consecutive_failures >= 2:
                raise RuntimeError("Bench stopped after two consecutive failed turns; investigate before more requests")
    finally:
        try:
            sh.command("talk") if "dialogue=1" in sh.command("wake") else None
            sh.idle(); sh.command("voice_latency batch 1200"); sh.command("wake on")
        finally:
            try:
                if original_speaker is not None and speaker_volume(sh)==args.speaker_volume:
                    set_speaker_volume(sh,original_speaker)
            finally:
                try:
                    if original_mac is not None and mac_volume()==(args.mac_output_volume,False):
                        subprocess.run(["osascript","-e",f"set volume output volume {original_mac[0]}"],check=True)
                finally:
                    sh.proc.terminate()
                    name="session.private.log" if session_number==1 else f"session-{session_number:03d}.private.log"
                    private_write(args.output/name, sh.raw)
                    segment["last_index"]=len(results)
                    private_write(args.output/"manifest.json",(json.dumps(manifest,indent=2)+"\n").encode())


if __name__ == "__main__":
    main()
