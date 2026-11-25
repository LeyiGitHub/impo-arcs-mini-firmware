#!/usr/bin/env python3
"""Interleaved official /tts playback checks; raw acoustic captures remain private."""
import argparse,json,re,subprocess,time,select
from pathlib import Path
from voice_latency_bench import Shell,private_write,speaker_volume,set_speaker_volume

def snapshot(text):
    keys=[(r'playback gen=(\d+) active=(\d+) failed=(\d+) cancelled=(\d+) adaptive=(\d+) sentence=(\d+)', 'generation active failed cancelled adaptive sentence'),
          (r'playback samples played=(\d+) queued=(\d+) underruns=(\d+) gap_ms=(\d+) prefill_ms=(\d+)', 'played_samples queued_samples underruns underrun_ms prefill_ms'),
          (r'playback ingress high=(\d+) overflow=(\d+) bytes=(\d+) stack=(\d+) segments=(\d+)/(\d+)', 'ingress_high_water ingress_overflows received_bytes worker_stack_free segments_decoded segments_requested'),
          (r'playback timing request=(\d+) body=(\d+) pcm=(\d+) first=(\d+) last=(\d+)', 'request_ms first_body_ms first_pcm_ms first_write_ms last_write_ms')]
    result={}
    for pattern,names in keys:
        m=re.search(pattern,text)
        if not m:raise ValueError('Incomplete playback snapshot')
        result.update(zip(names.split(),map(int,m.groups())))
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--adb',default='adb');ap.add_argument('--serial',required=True)
    ap.add_argument('--expected-boot',required=True);ap.add_argument('--image-metadata',type=Path,required=True)
    ap.add_argument('--text-file',type=Path,required=True);ap.add_argument('--recorder',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True);ap.add_argument('--cycles',type=int,default=3)
    ap.add_argument('--speaker-volume',type=int)
    ap.add_argument('--prefill-ms',type=int,default=800)
    ap.add_argument('--modes',nargs='+',default=['complete','adaptive','sentence']);args=ap.parse_args()
    if not 600<=args.prefill_ms<=2000 or args.cycles<1:ap.error('prefill 600..2000 and positive cycles required')
    if any(m not in ['complete','adaptive','sentence'] for m in args.modes):ap.error('Unknown mode')
    repo=Path(__file__).resolve().parents[1];out=args.output.resolve()
    if out==repo or repo in out.parents:raise SystemExit('Raw evidence must stay outside repo')
    if out.exists():raise SystemExit('Use a fresh directory; never replace failed trials')
    out.mkdir(parents=True,mode=0o700)
    text=args.text_file.read_text().strip();assert text
    say_command='gadget speaker.say '+json.dumps({'text':text},ensure_ascii=False,separators=(',',':'))
    if len(say_command.encode())>=160:raise SystemExit('Use a command shorter than the conservative 160-byte bench limit')
    sh=Shell(args.adb,args.serial);rows=[];original=None
    try:
        sh.command('');sh.drain(.5)
        assert f'boot={args.expected_boot} ' in sh.command('voice_latency')
        assert 'state=online' in sh.command('link');sh.idle()
        original=speaker_volume(sh)
        if args.speaker_volume is not None:set_speaker_volume(sh,args.speaker_volume)
        sh.command('wake off')
        private_write(out/'manifest.json',json.dumps({'image':json.loads(args.image_metadata.read_text()),'text':text,'cycles':args.cycles,'modes':args.modes,'original_volume':original,'test_volume':args.speaker_volume if args.speaker_volume is not None else original,'boot':args.expected_boot,'prefill_ms':args.prefill_ms}).encode())
        for cycle in range(args.cycles):
            order=args.modes[cycle%len(args.modes):]+args.modes[:cycle%len(args.modes)]
            for mode in order:
                assert 'state=online' in sh.command('link')
                assert f'boot={args.expected_boot} ' in sh.command('voice_latency')
                assert '"status":"idle"' in sh.command('gadget speaker.status')
                policy='complete' if mode=='complete' else 'adaptive';segment='sentence' if mode=='sentence' else 'full'
                configured=sh.command(f'playback {policy} {args.prefill_ms} {segment} apply');assert 'playback busy' not in configured
                before=snapshot(configured)['generation'];index=len(rows)+1;stem=out/f'trial-{index:03d}-{mode}'
                rec=None;start=len(sh.raw);event_start=len(sh.events);row={'index':index,'mode':mode,'cycle':cycle+1,'ok':False}
                try:
                    rec=subprocess.Popen([str(args.recorder),str(stem)+'.wav'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                    assert select.select([rec.stdout],[],[],5)[0];assert rec.stdout.readline().startswith('READY ')
                    row['record_host_mono']=time.monotonic();sh.drain(.7);row['command_host_mono']=time.monotonic()
                    response=sh.command(say_command,.2)
                    if 'too long' in response or '"ok":false' in response:raise RuntimeError('Speaker command rejected; no retry')
                    end=time.monotonic()+150;started=False
                    while time.monotonic()<end:
                        s=snapshot(sh.command('playback',.2));started|=s['generation']!=before
                        if started and s['failed']:row.update(s);break
                        status=sh.command('gadget speaker.status',.15)
                        if started and not s['active'] and '"status":"idle"' in status:
                            row.update(s);row['ok']=s['played_samples']>0 and s['segments_decoded']==s['segments_requested'] and not s['ingress_overflows'];break
                        private_write(out/'progress.private.log',sh.raw)
                        sh.drain(.1)
                    else:raise RuntimeError('Playback timeout, no retry')
                    tail=''
                    for stamp,chunk in sh.events[event_start:]:
                        combined=tail+sh.clean(chunk)
                        if re.search(r'sound_first_write gen='+str(row.get('generation',-1))+r' mono_ms=',combined):
                            row.setdefault('first_write_host_mono',stamp)
                        tail=combined[-150:]
                    row['tts_requests']=len(re.findall(r'POST /tts',sh.clean(sh.raw[start:])))
                except Exception as e:row['error']=str(e);row['ok']=False
                finally:
                    if rec:
                        if rec.poll() is None:rec.stdin.write('stop\n');rec.stdin.flush()
                        rec.wait(timeout=5)
                        wav_path=Path(str(stem)+'.wav')
                        if wav_path.exists():wav_path.chmod(0o600)
                        if rec.returncode:row['record_error']='Recorder failed';row['ok']=False
                    private_write(str(stem)+'.log',sh.raw[start:]);rows.append(row)
                    private_write(out/'trials.json',json.dumps(rows,indent=2).encode())
                print(json.dumps({k:row[k] for k in ['index','mode','ok','underruns','underrun_ms','segments_requested','error'] if k in row}),flush=True)
                if not row['ok']:raise RuntimeError('Stopped after failed/inconclusive trial; retained, no retry')
    finally:
        try:
            sh.command('gadget speaker.stop');sh.command('wake on')
            if args.speaker_volume is not None and original is not None and speaker_volume(sh)==args.speaker_volume:set_speaker_volume(sh,original)
        finally:
            private_write(out/'session.private.log',sh.raw);sh.proc.terminate();sh.proc.wait(timeout=5)
if __name__=='__main__':main()
