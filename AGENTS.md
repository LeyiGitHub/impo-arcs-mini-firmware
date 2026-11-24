# Firmware development

Use the pinned official SDK under upstream. Read upstream/arcs/AGENTS.md before changing firmware.
Keep Wi-Fi, BLE pairing, Noise, /link-control and voice on the official SDK path.
Make hardware changes in firmware/boards, then run tools/prepare_sdk.py.
Do not add the previous Mac/cloud integration bridge or import impo_devices.
Do not commit local.conf, tokens, Wi-Fi credentials, device logs, backups or generated images.
Select ListenAI ARCS SDK 0.1.6 or 0.1.8 explicitly to match the retained AP. Keep their build directories separate. Build and test before device writes. Preserve boot/AP/resources/KV.
Record compile, flash readback, connectivity, acoustic result and human hearing separately.

Reusable APIs belong in sdk/include and sdk/src; product identity and wake resources belong in products/ and resource descriptors. Keep hardware wiring in firmware/boards. Do not replace acoustic models by editing keyword text. Applications extend the official command/event tables through impo_arcs_mini_application(), without copying the protocol or startup.
For SDK changes run tests/run.sh with ARCS_BASE, build the default product and examples/button_companion, and keep build/flash/human acceptance separate.
