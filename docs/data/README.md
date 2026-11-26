# Public voice measurements

These files are fixed-field exports of private, real-microphone/real-speaker bench
runs, not device logs or audio. They retain every attempted question, including
failures. Percentiles use only successful turns with valid acoustic alignment and
answer signal. The metric is an automated acoustic proxy, not human listening.

- `voice-latency-20261124-short-60.json`: the original 30 attempts per mode, one
  overlap TLS failure retained. The two log segments stayed in the same boot.
- `voice-latency-20261124-short-repeat-8.json`: four new pairs on a later boot;
  all eight questions completed, no capture drops, all acoustic samples accepted.
  Do not silently pool these with the original run. Before this repeat began,
  and again after it completed during volume restoration, USB disconnected and
  the boot identifier changed. No new question was sent in the first disconnect.
  The cause has not been established; a subsequent read/set/read restored the
  original 20% volume without another reboot. These operational events limit any
  stability claim even though the eight question turns passed.

- `voice-latency-20261124-medium-stopped-3.json`: the 6.321-second fixed
  arithmetic question, planned four pairs. One batch turn passed. The first
  overlap turn queued the full body and received ACK, but timed out after
  120 seconds without a complete reply. The next planned overlap turn lost USB
  and rebooted; its result is unknown. Testing stopped and no question was
  retried. Five remaining medium slots and all eight planned pause900 slots
  were not attempted. No overlap percentile or long-input improvement can be
  claimed. The exporter conservatively includes the unknown result in the
  non-success count; it is not a proven gateway rejection.

- `voice-latency-20261125-medium-8.json`: first reply-diagnostics candidate,
  four pairs, 8/8 complete and acoustically valid. Small exploratory sample.
- `voice-latency-20261125-short-stopped-3.json`: same candidate, two successes
  followed by a partial-upload TLS failure. One planned slot was not attempted.
- `voice-latency-20261125-tls-pause900-4.json`: second candidate with peer-alert
  diagnostics; two pairs retain the fixed 900 ms internal pause, 4/4 complete.
- `voice-latency-20261125-tls-medium-stopped-2.json`: second candidate, batch
  success then overlap partial-upload failure. Peer reported fatal alert 20
  (`bad_record_mac`); no ACK/TTS. Six planned slots were not attempted.
- `voice-latency-20261125-writeguard-medium-4.json`: third candidate with the
  platform zero-write guard, before the send-BIO guard; four complete turns.
- `voice-latency-20261125-writeguard-short-unknown-1.json`: a subsequent short
  question lost USB after playback of the stimulus. The boot changed and no
  complete turn summary exists. The outcome is unknown, not a proven gateway
  rejection. The remaining planned question was not attempted.

- `voice-latency-20261125-writeguard-medium-4.json`: an intermediate candidate
  with only the platform zero-return guard, before the send-BIO guard; four
  medium turns completed with valid acoustic alignment and zero capture drops.
- `voice-latency-20261125-writeguard-short-unknown-1.json`: the following short
  question lost USB after capture and stimulus playback began. Boot identity
  changed; the result is unknown and retained. One remaining slot was not sent.
  The late reset query had no usable retained record. This is not a successful
  batch acceptance or proof of a gateway rejection.

Keep each candidate and stimulus separate. A percentile among successful turns
does not cancel a failed attempt. The November 25 results and unresolved TLS
failure are detailed in `../VOICE_LATENCY_PROGRESS_20261125.md`.

- `voice-latency-20261125-bio-batch-2.json`: the installed candidate includes
  the send-BIO zero-return guard. Two batch short-question recovery checks
  completed, with valid acoustic alignment and zero capture drops. This image
  has no new overlap comparison or long-term stability acceptance.

All tests use the exact ordinary 0.1.8 Mini2 image identified in the JSON metadata;
no diagnostic subscription is included. Runtime mode switches compare the same
image, not the original unoptimized commit against a new binary. Test volumes are
explicit conditions, not new product defaults. The ordinary firmware's runtime
sources for the November 24 datasets are unchanged by the SDK example and
documentation packaging changes.

Raw audio, logs, pairing details, original backups and acoustic-model binaries
stay outside this repository. See `../VOICE_LATENCY_VALIDATION.md` and
`../VOICE_LATENCY_BENCH.md` for the measurement method and acceptance boundaries.

Playback-only measurements use a different metric and must not be pooled with
question-to-answer voice datasets:

- `playback-20261125-matched-30.json`: same image/boot, ten interleaved cycles
  of complete, adaptive/full and adaptive/sentence. All 30 played completely;
  two adaptive acoustic samples were rejected, no ingress overflow. One sentence
  trial had an 879 ms rebuffer. This is why the device uses adaptive/full.
- `playback-20261125-exploration-9.json`: separate preceding exploration on the
  same candidate, with the coarser device-delta acoustic window. Do not pool it
  with the host-first-write anchored matched run. All nine played completely;
  one adaptive acoustic sample was rejected.

The initial overlong-shell tool runs sent no TTS. A reboot during their cleanup
remains unexplained. See `../PLAYBACK_OPTIMIZATION.md` for exact acceptance and
the later final-image verification; the matched image is identified in each JSON.

- `playback-20261125-short-6.json`: separate short-text exploration, two trials
  per mode, all six completed. The adaptive/800 mode had one 431 ms rebuffer;
  three acoustic samples were rejected. The device candidate was subsequently
  raised to 1200 ms and tested separately.

- `playback-20261125-release-matched-20.json`: exact installed final image,
  ten alternating complete/adaptive-1200 cycles. 20/20 complete, zero underruns
  or ingress overflows; acoustic acceptance 7/10 and 8/10 respectively.
- `playback-20261125-release-short-8.json`: same final image, separate short text,
  four attempts per mode, 8/8 complete with zero underruns/overflow. Acoustic
  acceptance 2/4 and 4/4. Do not pool text lengths or earlier candidates.

- `playback-20261125-final-acceptance.json`: final-image readback, fetch/play
  cancellation, retained metrics and synthetic wake interruption. This does not
  represent human listening or wake accuracy.

- `voice-latency-20261125-playback-timeout-1.json`: one full voice question
  on the final playback image timed out after upload, ACK and final text. No TTS
  audio body was observed; the firmware performed its existing single pre-audio
  TTS retry. The question was not resent, and the second planned slot was not
  attempted. After a controlled reboot, pairing and Wi-Fi/IP remained, but DNS
  and clock sync failed and the device stayed offline. The device eventually synced time and reconnected automatically, about 236
  seconds after reboot; recovery questions use a separate new corpus. Do not pool this failure with playback-only speed measurements.

- `voice-latency-20261125-playback-recovery-2.json`: exact final image after the
  controlled reboot and automatic network recovery, a separate greeting corpus.
  Two complete batch question/reply/TTS turns, no capture drops. No controlled
  end-to-end speed claim. A preceding console-listening guard refusal played no
  question and is retained privately as an invalid tool run.

- `voice-full-chain-20261125-analysis.json`: derived interval analysis of the
  two recovery turns, the separate final-image timeout, and the playback-only
  matched comparison. No new device requests were sent for this analysis.
  Device intervals end at driver acceptance; acoustic intervals use recorded
  source/answer energy. Their difference is not calibrated speaker latency.
  The timeout and recovered-network successes use different stimuli and are
  not pooled. See `../VOICE_FULL_CHAIN_ANALYSIS_20261125.md` for definitions,
  transport backpressure evidence and limits without cloud logs.

- `reply-probe-20261125-explain-1.json`, `reply-probe-20261125-story-1.json`,
  `reply-probe-20261125-short-1.json`: three separate observation-only runs on
  one probe image. All observed appended text equals the final text. First
  complete-sentence leads are 497/833/1090 ms, scheduling opportunities rather
  than measured playback gains. A second planned explanation was refused by
  the volume guard before stimulus playback. Mac volume differs across corpora.
- `reply-stream-20261125-exploration-8.json`: eight interleaved full/incremental
  questions on the earlier, aggressive streaming image. All completed; one
  1308 ms underrun, two 15-second capture limits, only three acoustic samples
  valid. Only three complete delta-probe records survived shell/log interleaving;
  missing fields remain missing. No dependable end-to-end speedup is claimed.
- `reply-stream-20261125-final-acceptance.json`: subsequent coalesced image,
  initial host tests/builds, CP readback, preservation and online/configuration
  snapshot before the user authorized voice testing. Zero questions were sent
  during that snapshot. The later voice corpus is separate, below.
- `reply-stream-20261125-coalesced-validation.json`: same coalesced image and
  boot, four interleaved full/incremental pairs, 8/8 complete with zero underruns.
  All four incremental turns issue two TTS requests, submitting 400–1095 ms
  before final text. Five acoustic detections pass; a ±350 ms input tail check
  leaves four clean samples and one complete pair (16.593/15.513 seconds).
  First two attempts lack capture alignment, one fails noise detection, and
  one has 1065 ms of extra input tail. All retained. A host snapshot binding
  fix applies to the last six attempts. Full probe summaries remain incomplete
  in all eight; the mode/segment rows are complete. No stable speedup is
  established, and full remains the default. See
  `../INCREMENTAL_REPLY_VALIDATION_20261125.md`.

These three image stages are distinct. Do not pool earlier opportunities or
queue behavior with final-image acoustic acceptance. See
`../INCREMENTAL_REPLY_20261125.md` for thresholds, exact hashes and limitations.

- `voice-optimization-review-20261125.json`: offline review of the same eight
  final-image trials. One full-mode example sums exactly to 16.371 seconds of
  device intervals; acoustic gap is independently 16.593 seconds. First-delta
  waiting and final-text tail ranges are measured. Tail-trim byte savings and
  a shorter continuous-turn wait are explicitly hypothetical, with no measured
  wall-clock gain or firmware change. See `../VOICE_OPTIMIZATION_REVIEW_20261125.md`.

- `cloud-streaming-input-audit-20261125.json`: current public official SDK/client
  commits, 19 source files verified against their Git tree blob hashes, deployed
  public gateway documentation, and interval ranges recomputed from the eight
  existing coalesced-image trials. No verified callable realtime ASR contract
  was found; production backend internals remain unknown. Proposed routes and
  events are marked as proposals. No authenticated voice probe, SDK runtime
  change, flash or new speedup measurement was performed. See
  `../CLOUD_STREAMING_INPUT_AUDIT_20261125.md` for the cloud implementation and
  pre-commit recognition acceptance requirements.

- `cross-device-latency-attribution-20261125.json`: offline attribution for the
  perceived Xiaozhou/Yunbao/Xiaoling difference. Recomputes all device intervals
  and within-cycle differences from the existing twenty medium-text and eight
  short-text playback trials. Earlier playback accounts for the demonstrated
  policy gain; this is not a matched three-device voice comparison. Archived
  device configurations and historical unpaired baselines are kept separate
  from live state and from the later coalesced firmware image. See the final
  section of `../VOICE_OPTIMIZATION_REVIEW_20261125.md`.

## Source audit and implementation checks

- `impo-cloud-optimization-20261126.json`: refreshed official commit/release
  inventory, three local patches, host checks and separate ListenAI 0.1.6/0.1.8
  candidate build metadata. This includes the original Yunbao 0.1.6 CP flash,
  independent readback, basic live-cloud/playback/wake checks and one acoustic
  trial per tail setting. It does not establish stable latency improvement,
  human hearing, fleet acceptance or resolution of the existing heap faults. See
  `../IMPO_CLOUD_OPTIMIZATION_20261126.md` for the implementation and rollout gates.
