# ARCS Mini device adaptation

`arcs_mini_voice` adapts a stationary ARCS Mini voice device with a single
240 x 240 ST7789P3 screen. It uses the SDK's BLE pairing, Wi-Fi, verified TLS,
Noise link, standard speaker commands, and native Impo voice processing.
It does not register the existing robot dog's actuators or sensors.

## Tested wiring

| Part | Configuration |
| --- | --- |
| Speaker amplifier | PA01, high enables |
| Microphone | codec ADC left channel, 30 dB |
| Power latch / power key | PB03 / PB04, active-low key |
| Screen backlight | PA21, PWM0 channel 1, 2 kHz, 70 percent |
| Screen CS / DC | PA22 / PA23 |
| SPI0 MOSI / clock | PA24 / PA25, 20 MHz |
| Screen reset | PB09 |
| Screen transfer | RGB565, offset 0, DMA channel 0, TE disabled |

Confirm the panel and pin mapping for another unit before flashing. This
target is not a general image for every product sold as ARCS Mini.

## Build and use

Use a licensed ListenAI ARCS SDK 0.1.6 installation and the toolchain described
in `../../README.md`. SDK 0.1.8 has incompatible IPC APIs.

```sh
cd arcs
export ARCS_BASE=/path/to/arcs-sdk-0.1.6
./build.sh -S app -B build-mini -t mkhdr -DBOARD=arcs_mini_voice
```

The board overlay supplies public identity and driver defaults. It contains
no Wi-Fi network, pairing credentials, or product ID. Pair from Impo's device
settings with Developer mode enabled, using `ImpoGadget-XXXXXX` over BLE.
A debounced short click starts/stops an SDK voice turn; the initial power-on
hold is ignored and the SDK retains its 3 s power-off policy.

Only update the CP app partition at 0x600000 after backing up the full flash
and verifying the board/AP compatibility. Keep AP, boot, resources, KV,
calibration, pairing, and Wi-Fi state. This PR includes neither vendor
images nor a one-command flash script for unknown hardware.

The low-priority screen task renders SDK connection and voice states in a
PSRAM frame buffer. The title is ARCS MINI; status labels are Chinese.
`screen` reports initialization, frame count and write errors. `screen test`
shows RGB565 color blocks for 15 s. `display.show_animation` restores the UI.
Screen initialization failure does not stop audio initialization. The
rotation scratch limit is reduced because this UI never uses SDK rotation.

## Validation and limits

Before upstream packaging, the same board logic/display driver was built with
the companion transport and buffered-voice fixes, flashed to one device, and
independently read back. The CP app matched and all other flash bytes were
unchanged. The user confirmed the screen's text and direction and heard a
complete native Impo answer. One screen-build conversation succeeded on a
phone hotspot (about 38 s to playback); on the original network, Wi-Fi stayed
associated while TLS writes timed out and the session reconnected.

The upstream adaptation changes identity/symbol names and the screen title.
Host checks and cross-compilation passed for this version (ListenAI SDK
0.1.6; app 1790128 bytes; internal SRAM 150,896 / 167,936 bytes). The
image SHA256 is `1d3f98e047be25836bd72957853c24c1786a88084a826c03276dcbcab2983c24`. It has not been flashed
again as part of PR preparation. Prior hearing/display results apply
to that earlier image, not to a new physical regression of this branch alone.
The voice tests above used companion patches and do not establish reliable
conversation with the upstream voice/transport implementation by itself.

Host checks: `sh arcs/tests/host/run_arcs_mini.sh` from repository root. They
cover power-on hold, debounce, click, long hold, uptime wrap, screen bounds,
all SDK states, blink phases, colors, and malformed sizes. The screen test
accepts an optional existing directory for PPM previews.

Battery capacity/voltage calibration, an independent status LED, sustained
network reliability, and hardware tests on other ARCS Mini variants remain
unverified. Status callbacks do not replace server acknowledgements or human
hearing. No raw device logs, network names, unique IDs, or credentials are
published.

## Font licensing

The checked-in ARCS Mini Status 24 glyphs are derived from Google Fonts Noto
Sans SC and remain under SIL OFL 1.1 (`LICENSE-OFL`). Regenerate using Pillow
11.3.0 and `arcs/tools/gen_arcs_mini_font.py`; the generator checks the source
font SHA256. Font generation is not needed for the normal firmware build.
