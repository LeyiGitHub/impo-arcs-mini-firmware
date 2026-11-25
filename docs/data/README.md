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
