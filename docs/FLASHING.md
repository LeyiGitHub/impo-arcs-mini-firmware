# 刷写与恢复

本目标针对云宝 ARCS Mini 语音盒子，不是 V2.1 机器狗。先确认扬声器功放 PA01、按键 PB04 和左声道麦克风，核对 AP/CP 的 IPC、PSRAM 与分区布局；其他型号不能只因为芯片同属 LS26 就直接刷写。

1. 通过唯一 ADB serial/transport 核验目标芯片和原固件。旧 firmware 用交互式 `adb -s <serial> shell` 中的 `device get_device_id` 与 `version`。先结束当前对话。
2. 在同一个交互式 shell 运行 `recovery`，确认 `adb devices -l` 出现同芯片的 `BOOT-<chip>`。旧固件可能不支持 `adb reboot recovery` 服务。
3. 从 recovery 的 ADB sync `RECV /RAW/NAND/0/1000000` 读取完整 16 MiB 闪存两次，校验长度及 SHA256 一致。保存在仓库外的私有目录（0600），其中可能有凭据。工具与协议见官方 `upstream/arcs/adb_download.sh` 和 ListenAI 文档。
4. 仅更新 CP 应用分区 `0x600000..0xA00000`；镜像须带修补后的 ARCS 头且不超过 4 MiB。不更新 boot/AP/tone/wake_word/emoji/respak，不清除 KV、Wi-Fi 或校准区域。原 AP 不兼容时先停止，不自动替换。
5. recovery 支持 raw sync 时，向精确匹配的设备执行 `adb -s BOOT-<chip> push <image> /RAW/NAND/600000`。只发送一次写入，超时后先独立读回，不能直接重发。
6. 用 `RECV /RAW/NAND/600000/<image-size-hex>` 独立读回，与镜像逐字节相同后，再完整读取闪存，确认 CP 分区外逐字节不变。然后在 recovery 交互式 shell 中运行 `reboot`。
7. 查 ADB 启动、`version`、`link`、`gadget device.health`。通过官方 BLE 流程重新配对，不把旧云端适配器的配对文件复制为新节点身份。

原机备份与刷写账本只保存在本地；需要回退时先核对目标唯一身份、原始镜像摘要和当前分区状态，再只恢复原 CP。新固件启动后写入的 SDK 配对/Wi-Fi 状态需要单独处理，不用旧全量备份覆盖最新状态。

官方通用 `adb_download.sh` 可批量操作设备；现场不使用无目标选择的批量刷写，不运行机器狗的动作自检脚本。
