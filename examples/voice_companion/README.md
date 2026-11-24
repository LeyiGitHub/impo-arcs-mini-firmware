# Voice companion

Minimal application using the public voice SDK. It starts conversation with the
physical PB04 button and has no acoustic-model dependency. The product explicitly
opts into overlap upload with a 1200 ms endpoint; the SDK-wide default stays batch.
Pair through the normal Impo BLE flow, then press the button, wait for the listening
prompt and speak. The existing `/tts` endpoint and voice are unchanged.

```sh
ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.6 \
  python3 tools/build.py --docker --arcs-sdk-version 0.1.6 \
  --product examples/voice_companion/product.json \
  --app-dir examples/voice_companion
```

For a verified retained 0.1.8 AP, select the corresponding `ARCS_BASE` and
`--arcs-sdk-version 0.1.8`. Output directories are separate. This example is
cross-built; its exact image has not been flashed. Physical latency evidence is
from the ordinary Mini2 product identified in `docs/VOICE_LATENCY_VALIDATION.md`.

`companion.voice_status` exposes phase and latest-turn counters without audio,
answer text or pairing credentials. Before the first turn, counters may be zero;
`valid_stages` identifies recorded timestamps. The configured default is separate
from the latest snapshot's turn-specific mode.

Switch to batch by changing `voice.upload_mode` in the product and rebuilding, or
call `impo_arcs_mini_configure_voice()` after ending the current continuous session.
A submitted request is never automatically retried or resent as batch.

For a product manifest including this application command:

```sh
python3 tools/gen_manifest.py --product examples/voice_companion/product.json \
  --commands examples/voice_companion/app.c:commands
```

See `docs/SDK.md` for the API and compatible wake-resource configuration. Product
labels alone cannot change an acoustic wake word.
