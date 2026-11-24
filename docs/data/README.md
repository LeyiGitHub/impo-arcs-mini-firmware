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

All tests use the exact ordinary 0.1.8 Mini2 image identified in the JSON metadata;
no diagnostic subscription is included. Runtime mode switches compare the same
image, not the original unoptimized commit against a new binary. Test volumes are
explicit conditions, not new product defaults. The ordinary firmware's runtime
sources are unchanged by the SDK example/documentation packaging changes.

Raw audio, logs, pairing details, original backups and acoustic-model binaries
stay outside this repository. See `../VOICE_LATENCY_VALIDATION.md` and
`../VOICE_LATENCY_BENCH.md` for the measurement method and acceptance boundaries.
