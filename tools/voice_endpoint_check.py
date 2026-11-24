#!/usr/bin/env python3
"""Measure VAD completeness on a fixed external stimulus, cancelling before upload completion.

Use only on an already physically identified device. No automatic retry. The
same source waveform is played completely even when the device ends capture early.
"""
import argparse
import json
from pathlib import Path
import re
import select
import subprocess
import time
from voice_latency_bench import Shell, private_write, parse_snapshot
from analyze_voice_bench import read_wav, measure


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--adb",default="adb")
    ap.add_argument("--serial",required=True)
    ap.add_argument("--expected-boot",required=True)
    ap.add_argument("--stimulus",required=True,type=Path)
    ap.add_argument("--recorder",required=True,type=Path)
    ap.add_argument("--output",required=True,type=Path)
    ap.add_argument("--pauses",nargs="+",type=int,default=[1200,800])
    ap.add_argument("--volume",type=float,default=4)
    args=ap.parse_args()
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[1]) or args.output.exists():
        ap.error("Use a new private directory outside the repository")
    if any(not 600<=p<=2000 for p in args.pauses):ap.error("Pause must be 600..2000")
    args.output.mkdir(parents=True,mode=0o700)
    reference,ref_rate=read_wav(args.stimulus)
    sh=Shell(args.adb,args.serial);results=[]
    try:
        sh.command("");sh.drain(1)
        if f"boot={args.expected_boot} " not in sh.command("voice_latency"):
            raise RuntimeError("Boot identifier changed; identify device again")
        for i,pause in enumerate(args.pauses,1):
            sh.idle()
            if "voice configured" not in sh.command(f"voice_latency batch {pause}"):
                raise RuntimeError("Configuration refused")
            off=len(sh.raw);rec=None;player=None;row={"endpoint_only":True,"endpoint_ms":pause,"ok":False}
            stem=args.output/f"endpoint-{i:02d}-{pause}"
            try:
                rec=subprocess.Popen([str(args.recorder),str(stem)+".wav"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
                if not select.select([rec.stdout],[],[],5)[0] or not rec.stdout.readline().startswith("READY "):
                    raise RuntimeError("Acoustic recorder not ready")
                row["record_host_mono"]=time.monotonic();sh.drain(.8)
                sh.command("talk",.05);end=time.monotonic()+5
                while not re.search(r"gadget_voice.*listening",sh.clean(sh.raw[off:])):
                    if time.monotonic()>end:raise RuntimeError("Listening not started")
                    sh.drain(.02)
                sh.drain(.7);row["stimulus_host_mono"]=time.monotonic()
                player=subprocess.Popen(["afplay","-v",str(args.volume),str(args.stimulus)])
                end=time.monotonic()+20
                while time.monotonic()<end:
                    sh.drain(.02)
                    if "listened " in sh.clean(sh.raw[off:]):
                        row["capture_host_mono"]=time.monotonic()
                        # Batch is deliberate: stop before the full recording is
                        # queued. Absence of ACK/full-body stage is checked below.
                        sh.command("talk",.02)
                        break
                else:raise RuntimeError("No capture end")
                while player.poll() is None:sh.drain(.02)
                end=time.monotonic()+5
                while time.monotonic()<end:
                    snapshot=parse_snapshot(sh.command("voice_latency"))
                    if snapshot["phase"] in (0,7) and int(snapshot["valid"],16)&0x80:
                        row.update(snapshot);break
                    sh.drain(.02)
                else:raise RuntimeError("Capture cancellation did not finish")
                flags=int(row["valid"],16)
                row["tts_requests"]=len(re.findall(r"POST /tts",sh.clean(sh.raw[off:])))
                row["ok"]=(row["dropped"]==0 and (flags&3)==3 and (flags&0x18)==0
                           and bool(flags&0x80) and row["tts_requests"]==0)
            except Exception as e:row["error"]=str(e)
            finally:
                if player and player.poll() is None:player.terminate();player.wait(timeout=5)
                if rec and rec.poll() is None:
                    rec.stdin.write("stop\n");rec.stdin.flush();rec.wait(timeout=5)
                private_write(str(stem)+".log",sh.raw[off:])
            try:
                x,rate=read_wav(str(stem)+".wav")
                row["acoustic"]=measure(row,x,rate,reference,ref_rate)
            except Exception as e:row["acoustic"]={"acoustic_valid":False,"error":str(e)}
            results.append(row)
            private_write(args.output/"results.json",(json.dumps(results,indent=2)+"\n").encode())
            print(json.dumps({k:row[k] for k in ["endpoint_ms","ok","acoustic"]}),flush=True)
            if row.get("error") or not row["ok"]:
                raise RuntimeError("Endpoint trial inconclusive; no retry")
    finally:
        try:
            if "dialogue=1" in sh.command("wake"):sh.command("talk");sh.drain(2)
            sh.idle();sh.command("voice_latency batch 1200");sh.command("wake on")
        finally:
            sh.proc.terminate();private_write(args.output/"session.private.log",sh.raw)


if __name__=="__main__":main()
