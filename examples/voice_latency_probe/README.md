# Gateway negative checks

Optional diagnostic application; normal builds exclude it. Keep the existing
product identity/wake resources and select the ListenAI SDK matching the retained
AP, as described in [FLASHING.md](../../docs/FLASHING.md). Example build:

```sh
ARCS_BASE=/path/to/listenai-sdk-0.1.8 python3 tools/build.py --docker \
  --arcs-sdk-version 0.1.8 --product products/arcs-mini2-latency-probe.json \
  --app-dir examples/voice_latency_probe
```

With the identified device online and idle, stop local wake temporarily and run
one command at a time in the official interactive shell:

```text
wake off
latency_probe positive
latency_probe cancel
latency_probe incomplete
latency_probe positive
wake on
```

Wait for each `probe mode=...` result before the next command. The two negative tests send a
WAV header and a silent sample with the JSON body deliberately left unfinished.
`cancel` cancels without transport end-of-body; `incomplete` sends end-of-body
without closing JSON. There is no spoken question and no automatic retry.

`positive` sends one complete arithmetic text question and requires an ACK plus
a completed assistant event within 30 seconds. Run it before and after the
negative checks to validate that the observer can actually see messages.

For either negative test, the separate subscription must remain healthy.
`passed=1` requires no parseable
message ACK and no observed message events during the 12-second observation;
`incomplete` must also end with an HTTP error. This is bounded gateway evidence,
not a query of persistent server history or a guarantee against events after the
observation window. Repeat successful ordinary turns separately and verify one
upload/ACK/answer per turn. Restore and reboot the normal application after diagnostics, before ordinary
voice benchmarks; do not assume that overlapping diagnostic subscriptions leave
the original voice subscription unaffected.
