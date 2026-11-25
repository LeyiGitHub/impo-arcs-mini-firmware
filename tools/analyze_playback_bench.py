#!/usr/bin/env python3
"""Export numeric playback measurements; raw speech, identities and logs stay private."""
import argparse,json,hashlib
from pathlib import Path
import numpy as np
from analyze_voice_bench import read_wav

def acoustic(row,path):
    x,rate=read_wav(path);frame=rate//100;x=x[:len(x)//frame*frame]
    energy=np.sqrt(np.mean(x.reshape(-1,frame)**2,axis=1))
    start=row['command_host_mono']-row['record_host_mono']
    quiet=energy[:max(1,int((start-.15)*100))];noise=max(1.,float(np.percentile(quiet,95)))
    threshold=max(8.,noise*4)
    if row.get('first_write_host_mono'):
        ready=row['first_write_host_mono']-row['record_host_mono'];anchor='host_first_write'
    elif row.get('first_write_ms') and row.get('request_ms'):
        ready=start+(row['first_write_ms']-row['request_ms'])/1000;anchor='device_delta_window'
    else:return {'valid':False}
    first=max(0,int((ready-.10)*100));last=min(len(energy)-3,int((ready+1.5)*100))
    active=[i for i in range(first,max(first,last)) if np.all(energy[i:i+3]>threshold)]
    if not active:return {'valid':False}
    peak=float(np.max(energy))/noise
    return {'valid':peak>=4,'anchor':anchor,'command_to_sound_ms':round((active[0]*.01-start)*1000),'peak_to_noise':round(peak,2)}
def export(root):
    manifest=json.loads((root/'manifest.json').read_text());rows=json.loads((root/'trials.json').read_text());public=[]
    allowed='index mode cycle ok generation failed cancelled adaptive sentence played_samples queued_samples underruns underrun_ms prefill_ms ingress_high_water ingress_overflows received_bytes worker_stack_free segments_decoded segments_requested tts_requests'.split()
    for row in rows:
        r={k:row[k] for k in allowed if k in row};r['bench_error']=bool(row.get('error') or row.get('record_error'))
        for key in ['first_body_ms','first_pcm_ms','first_write_ms','last_write_ms']:
            if row.get(key) and row.get('request_ms'):r[key.replace('_ms','_from_request_ms')]=row[key]-row['request_ms']
        try:r['acoustic']=acoustic(row,root/f"trial-{row['index']:03d}-{row['mode']}.wav")
        except Exception:r['acoustic']={'valid':False}
        public.append(r)
    summary={}
    for mode in manifest['modes']:
        group=[r for r in public if r['mode']==mode];valid=[r['acoustic']['command_to_sound_ms'] for r in group if r['ok'] and r['acoustic']['valid']]
        summary[mode]={'attempts':len(group),'successes':sum(r['ok'] for r in group),'acoustic_valid':len(valid),'underruns':sum(r.get('underruns',0) for r in group),'failed_or_unknown':sum(not r['ok'] for r in group)}
        if valid:summary[mode].update(p50_ms=round(float(np.median(valid))),p95_ms=round(float(np.percentile(valid,95))))
    image=manifest['image']
    return {'schema_version':1,'metric':'official TTS command to detected answer energy; automated acoustic proxy','text_sha256':hashlib.sha256(manifest['text'].encode()).hexdigest(),'test_volume':manifest['test_volume'],'configured_prefill_ms':manifest.get('prefill_ms',800),'image':{k:image[k] for k in ['image_sha256','image_bytes','product','listenai_sdk_version'] if k in image},'summary':summary,'trials':public}
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('directory',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    result=export(a.directory);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result['summary'],indent=2))
