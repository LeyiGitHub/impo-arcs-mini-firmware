# Bounded voice notes on slow ARCS links

Network upload used to block the task while it was still recording. In one
physical test a 512-byte TLS write took 5,992 ms, beyond the old 5 s request
queue wait, and the recording/upload failed before a reply could arrive.

The microphone now captures the complete note before any request is opened.
It retains the existing VAD maximum of 15 s, writes exact RIFF/data lengths,
and uses the native `/chat/stream`, `/chat/subscribe`, and `/tts` paths. Silence,
dropped microphone bytes, and a microphone timeout are rejected. Upload has a
120 s budget checked between chunks; each body send shares one 20 s queue
wait budget. A network failure ends the current note without replaying it on
a new session. The reply wait is 120 s.

The note buffer is allocated once in PSRAM. Reducing the microphone jitter
queue from 10 s to 1 s gives a net PSRAM increase of about 192 KB; internal
SRAM does not gain a large static allocation.

Short TTS is buffered in the existing 30 s sound ring until its declared WAV
PCM is complete. Long speech starts at 25 s of buffered audio to leave room
for the producer. Failure before playback clears the unplayed phrase; local
beeps keep their immediate playback path. A loopback self-test waits up to
120 s for its question and skips recording if speech never becomes ready.

Run `ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh arcs/tests/host/run_buffered_voice.sh`
from the repository root. Tests execute the real sound output task and voice
`listen()` with deterministic audio/network stubs. They cover PCM order, WAV
sizes, delayed playback, cancellation, truncated downloads, long replies,
6 s-per-chunk upload, silence, failure, and deadlines.

With the companion ARCS TLS changes and ARCS Mini board adapter, a person
asked a short arithmetic question and confirmed a complete spoken answer
(about 24 s to playback). The screen build also completed one conversation
on a phone hotspot (about 38 s). The original network still produced upload
timeouts, slow writes, and reconnects. These are individual physical acceptance
results, not long-term stability or a hardware regression pass for other boards.
