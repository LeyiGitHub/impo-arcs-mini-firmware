#!/usr/bin/env python3
"""Analyze optional acoustic bench WAVs. Requires numpy; never opens raw logs.

The metric is fixed-stimulus last active frame to first detected answer energy.
It is an automated acoustic proxy, not human hearing or calibrated sound pressure.
All thresholds and rejected measurements are retained in the output.
"""
import argparse
import json
from pathlib import Path
import wave
import numpy as np


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        if w.getsampwidth() != 2 or w.getnchannels() != 1:
            raise ValueError("Expected mono PCM16 WAV")
        return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float), w.getframerate()


def match_reference(recording, reference):
    """Normalized FFT correlation, valid offsets only; DC is removed first."""
    x = recording - np.mean(recording)
    y = reference - np.mean(reference)
    if len(x) < len(y) or np.sum(y*y) == 0:
        raise ValueError("Recording shorter than reference, or silent reference")
    size = 1 << (len(x)+len(y)-1).bit_length()
    corr = np.fft.irfft(np.fft.rfft(x, size)*np.fft.rfft(y[::-1], size), size)
    corr = corr[len(y)-1:len(x)]
    power = np.concatenate(([0.], np.cumsum(x*x)))
    power = power[len(y):]-power[:-len(y)]
    score = corr / np.sqrt(np.maximum(power*np.sum(y*y), 1e-12))
    index = int(np.argmax(score))
    return index, float(score[index])


def measure(row, recording, rate, reference, ref_rate):
    # Downsample to 8 kHz for correlation; all reported waveform times use this
    # common recording timeline, never subtraction of device and host clocks.
    target = 8000
    x = np.interp(np.arange(0, len(recording)/rate, 1/target), np.arange(len(recording))/rate, recording)
    y = np.interp(np.arange(0, len(reference)/ref_rate, 1/target), np.arange(len(reference))/ref_rate, reference)
    expected = row["stimulus_host_mono"]-row["record_host_mono"]
    lo = max(0, int((expected-.3)*target))
    hi = min(len(x), int((expected+len(y)/target+1.5)*target))
    offset, score = match_reference(x[lo:hi], y)
    stimulus_start = (lo+offset)/target
    frame = 80  # 10 ms
    y_energy = np.sqrt(np.mean(y[:len(y)//frame*frame].reshape(-1,frame)**2,axis=1))
    active = np.flatnonzero(y_energy > max(10., float(np.max(y_energy))*.05))
    if not len(active):
        raise ValueError("No active reference speech")
    stimulus_end = stimulus_start + (int(active[-1])+1)*.01
    if row.get("endpoint_only"):
        m={"reference_correlation":score,"stimulus_last_active_s":stimulus_end,
           "acoustic_valid":False,"stimulus_tail_seen":False}
        if "capture_host_mono" in row and "0" in row.get("stages",{}):
            last_voice=row["capture_host_mono"]-row["record_host_mono"]-(row["capture"]-row["stages"]["0"])/1000
            m["vad_last_voice_relative_to_stimulus_end_ms"]=round((last_voice-stimulus_end)*1000)
            m["stimulus_tail_seen"]=last_voice >= stimulus_end-.35
            m["acoustic_valid"]=score>=.15 and m["stimulus_tail_seen"]
        return m
    ready = row["ready_host_mono"]-row["record_host_mono"]
    energy = np.sqrt(np.mean(x[:len(x)//frame*frame].reshape(-1,frame)**2,axis=1))
    quiet = energy[max(0,int((ready-.9)*100)):max(1,int((ready-.25)*100))]
    if not len(quiet):
        raise ValueError("No pre-answer noise window")
    noise = max(1.,float(np.percentile(quiet,95)))
    threshold = max(8., noise*3)
    a=max(0,int((ready-.05)*100)); b=min(len(energy),int((ready+1.5)*100))
    candidates=[i for i in range(a,max(a,b-2)) if np.all(energy[i:i+3]>threshold)]
    if not candidates:
        raise ValueError("No sustained answer above background noise")
    answer_start=candidates[0]*.01
    snr=float(np.max(energy[a:b]))/noise
    m={"reference_correlation":score,"noise_rms95":noise,"answer_threshold_rms":threshold,
       "answer_peak_to_noise":snr,"stimulus_last_active_s":stimulus_end,
       "answer_first_active_s":answer_start,"acoustic_gap_ms":round((answer_start-stimulus_end)*1000),
       "acoustic_valid":score>=.15 and snr>=4 and answer_start>stimulus_end}
    if "capture_host_mono" in row and "0" in row.get("stages",{}):
        last_voice=row["capture_host_mono"]-row["record_host_mono"]-(row["capture"]-row["stages"]["0"])/1000
        m["vad_last_voice_relative_to_stimulus_end_ms"]=round((last_voice-stimulus_end)*1000)
        m["stimulus_tail_seen"]=last_voice >= stimulus_end-.35
        m["acoustic_valid"] &= m["stimulus_tail_seen"]
    else:
        m["acoustic_valid"]=False
        m["missing_capture_alignment"]=True
    return m


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("directory",type=Path)
    ap.add_argument("--reference",type=Path,required=True,help="PCM16 WAV of fixed stimulus")
    args=ap.parse_args()
    reference,rate=read_wav(args.reference)
    rows=json.loads((args.directory/"trials.json").read_text())
    for r in rows:
        try:
            wav=args.directory/f"trial-{r['index']:03d}-{r['mode']}-{r['endpoint_ms']}.wav"
            x,sr=read_wav(wav)
            r["acoustic"]=measure(r,x,sr,reference,rate)
        except Exception as e:
            r["acoustic"]={"acoustic_valid":False,"error":str(e)}
    groups={}
    for r in rows:
        k=f"{r['mode']}:{r['endpoint_ms']}"
        groups.setdefault(k,[]).append(r)
    summary={}
    for k,group in groups.items():
        accepted=[r for r in group if r["ok"] and r["acoustic"]["acoustic_valid"]]
        gaps=[r["acoustic"]["acoustic_gap_ms"] for r in accepted if "acoustic_gap_ms" in r["acoustic"]]
        summary[k]={"attempted":len(group),"voice_failed":sum(not r.get('voice_ok',r['ok']) for r in group),
                    "bench_errors":sum(bool(r.get('error') or r.get('record_error')) for r in group),
                    "acoustic_valid":len(accepted),"acoustic_rejected":len(group)-len(accepted)}
        if gaps:
            summary[k].update(p50_ms=round(float(np.median(gaps))),p95_ms=round(float(np.percentile(gaps,95))))
        components={}
        for r in group:
            if not r["ok"]:continue
            stages=r.get("stages",{})
            values={}
            if "0" in stages:
                values["last_voice_to_capture_ms"]=r["capture"]-stages["0"]
            for name,a,b in [("capture_to_queued_ms","capture","upload"),
                             ("queued_to_ack_ms","upload","ack"),
                             ("ack_to_text_ms","ack","text"),
                             ("tts_to_first_pcm_ms","tts","first_pcm_ms"),
                             ("first_pcm_to_driver_write_ms","first_pcm_ms","first_write_ms")]:
                if r.get(a) and r.get(b):values[name]=r[b]-r[a]
            for name,value in values.items():components.setdefault(name,[]).append(value)
        summary[k]["device_components_p50_ms"]={name:round(float(np.median(values))) for name,values in components.items()}
    paired={}
    if "batch:1200" in groups and "overlap:1200" in groups:
        a={r.get("cycle"):r for r in groups["batch:1200"] if r["ok"] and r["acoustic"]["acoustic_valid"]}
        b={r.get("cycle"):r for r in groups["overlap:1200"] if r["ok"] and r["acoustic"]["acoustic_valid"]}
        deltas=[b[c]["acoustic"]["acoustic_gap_ms"]-a[c]["acoustic"]["acoustic_gap_ms"] for c in a.keys()&b.keys() if c is not None]
        if deltas:
            paired={"pairs":len(deltas),"median_overlap_minus_batch_ms":round(float(np.median(deltas)))}
            if len(deltas)>=10:
                rng=np.random.default_rng(2411)
                boot=np.median(rng.choice(deltas,size=(5000,len(deltas)),replace=True),axis=1)
                paired["paired_median_bootstrap95_ms"]=[round(float(x)) for x in np.percentile(boot,[2.5,97.5])]
    connections={}
    for r in rows:
        label=str(r.get("session","unknown"))
        key=f"{r['mode']}:{r['endpoint_ms']}"
        connections.setdefault(label,{}).setdefault(key,[]).append(r)
    by_connection={}
    for label,groups_in_connection in connections.items():
        by_connection[label]={}
        for key,group in groups_in_connection.items():
            gaps=[r["acoustic"]["acoustic_gap_ms"] for r in group if r["ok"] and r["acoustic"]["acoustic_valid"] and "acoustic_gap_ms" in r["acoustic"]]
            by_connection[label][key]={"attempted":len(group),"failed":sum(not r['ok'] for r in group)}
            if gaps:by_connection[label][key]["p50_ms"]=round(float(np.median(gaps)))
    output={"metric":"fixed source last active frame to detected answer energy, automated acoustic proxy",
            "summary":summary,"paired":paired,"by_subscription_generation":by_connection,"trials":rows}
    path=args.directory/"analysis.json"
    path.write_text(json.dumps(output,indent=2)+"\n");path.chmod(0o600)
    print(json.dumps(summary,indent=2))


if __name__=="__main__":
    main()
