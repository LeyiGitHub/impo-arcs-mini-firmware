# Firmware development

Use the pinned official SDK under upstream. Read upstream/arcs/AGENTS.md before changing firmware.
Keep Wi-Fi, BLE pairing, Noise, /link-control and voice on the official SDK path.
Make hardware changes in firmware/boards, then run tools/prepare_sdk.py.
Do not add the previous Mac/cloud integration bridge or import impo_devices.
Do not commit local.conf, tokens, Wi-Fi credentials, device logs, backups or generated images.
Use ARCS SDK 0.1.6. Build and test before device writes. Preserve boot/AP/resources/KV.
Record compile, flash readback, connectivity, acoustic result and human hearing separately.
