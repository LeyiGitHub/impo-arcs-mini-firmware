# M5Stack CoreS3

The [CoreS3](https://docs.m5stack.com/en/core/CoreS3): an ESP32-S3 with a 2"
320×240 touch screen, speaker and microphones, a GC0308 camera, a BMI270
motion sensor and a battery behind an AXP2101.

    tools/impo/board.sh build cores3 && tools/impo/board.sh flash cores3

| File | What it is |
| --- | --- |
| `board.c` | the board under Impo's UI: display, touch, audio, power (M5Stack's BSP), and what it adds to the gadget's commands |
| `camera.c` | the GC0308 for `camera.capture`, set up as M5Stack's StackChan firmware sets it up |
| `imu.cpp` | the BMI270 with M5Stack's driver: `imu.read` and the `shaken` event |
| `m5stack_cores3.h` | how a body the CoreS3 sits in plugs in |
| `vendor/` | M5Stack's code, unchanged (`vendor/README.md`) |

## A body

The CoreS3 is also the head of other M5Stack products. Such a device is its
own folder with `"base": "m5stack-cores3"` in `device.json`; its component
selects `M5STACK_CORES3_BODY` and defines `m5stack_cores3_body`
(`m5stack_cores3.h`): its name, a `start` the board calls once the CoreS3 is up
and the 5 V output is on, and the commands and features it adds after the
CoreS3's own. `../m5stack-stackchan` is one.

## Memory

The camera's DVP driver needs a 32 KB block of internal DMA memory while it
streams. `camera.c` starts the stream with the board, before Wi-Fi and BLE take
internal RAM, and runs it from then on, as M5Stack's firmware does; frames are
dropped, not copied, between photos. The 32 KB data cache (`sdkconfig`, as in
M5Stack's firmware) leaves room for it.
