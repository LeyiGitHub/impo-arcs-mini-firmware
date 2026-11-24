# 刷写与恢复

本目标针对云宝 ARCS Mini 语音盒子，不是 V2.1 机器狗。先确认扬声器功放 PA01、按键 PB04、左声道麦克风及 ST7789P3 单屏，显示引脚见 [屏幕适配](SCREEN.md)。核对 AP/CP 的 IPC、PSRAM 与分区布局；其他型号不能只因为芯片同属 LS26 就直接刷写。

用户提供的 Arcs-Mini2 板卡说明将侧面两个键称为 **RST 键**和**主功能键**。主功能键长按 3 秒以上用于开关机，运行后短按用于交互；RST 单击标注为关机，不能把它当作普通的应用复位来指导操作。保持 USB 连接，按住主功能键并短按、松开 RST，再松开主功能键，可进入 Recovery。进入后仍须核对下面的唯一芯片身份。

原机 Boot 可以先显示“长按功能键开机”的充电待机页，再熄屏；应用层的 USB 启动逻辑不能替代 Boot 的开机门槛。新固件启动失败时先抓取诊断，不反复尝试按键组合，也不使用板卡旧固件的多击清网或恢复出厂功能。

1. 通过唯一 ADB serial/transport 核验目标芯片和原固件。旧 firmware 用交互式 `adb -s <serial> shell` 中的 `device get_device_id` 与 `version`。先结束当前对话。
2. 在同一个交互式 shell 运行 `recovery`，确认 `adb devices -l` 出现同芯片的 `BOOT-<chip>`。旧固件可能不支持 `adb reboot recovery` 服务。
3. 从 recovery 的 ADB sync `RECV /RAW/NAND/0/1000000` 读取完整 16 MiB 闪存两次，校验长度及 SHA256 一致。保存在仓库外的私有目录（0600），其中可能有凭据。工具与协议见官方 `upstream/arcs/adb_download.sh` 和 ListenAI 文档。
4. 仅更新 CP 应用分区 `0x600000..0xA00000`；镜像须带修补后的 ARCS 头且不超过 4 MiB。不更新 boot/AP/tone/wake_word/emoji/respak，不清除 KV、Wi-Fi 或校准区域。原 AP 不兼容时先停止，不自动替换。
5. recovery 支持 raw sync 时，向精确匹配的设备执行 `adb -s BOOT-<chip> push <image> /RAW/NAND/600000`。只发送一次写入，超时后先独立读回，不能直接重发。
6. 用 `RECV /RAW/NAND/600000/<image-size-hex>` 独立读回，与镜像逐字节相同后，再完整读取闪存，确认 CP 分区外逐字节不变。然后在 recovery 交互式 shell 中运行 `reboot`。
7. 查 ADB 启动、`version`、`link`、`gadget device.health` 和 `screen`。已有配对时核验 `paired=1`；尚未配对时使用官方 BLE 流程，不把旧云端适配器的配对文件复制为新节点身份。显示、语音和连接分别验收。

同款板卡可能保留不同版本的 AP。聆思 `0.1.6` / `0.1.8` 是底层 SDK 版本，原机应用的 `3.x` 版本单独记录。核对 AP 版本、分区及模型后，用匹配的 `ARCS_BASE` 和 `--arcs-sdk-version` 构建；0.1.8 产物的目录后缀为 `-arcs018`。刷写前同时核对旁边 `impo-sdk-build.json` 的 `listenai_sdk_version`、产品、长度及 SHA256，详见 [SDK 版本对照](SDK_VERSIONS.md)。唤醒词、模型及 wrap 通过产品清单配置，不能把另一台设备的模型摘要当作本机已核验结果。

已核对的 0.1.8 AP 在启动 CP 前校验镜像 `0x180` 的两个 32 位字：PSRAM 起点和大小，须与最终构建配置一致。本板为 `0x28800000`、`0x00800000`。标准 ARCS 头相同、写入读回正确，也不能代替这项检查；缺失声明会触发 AP 的不匹配提示和约 10 秒关机。构建器已增加声明、链接和二进制校验。不要通过更换 AP 或跳过检查掩盖该错误。

部分原机 1.1.9 Recovery 保护从零地址开始的访问，完整备份会返回访问拒绝。按官方 shell 流程登录 `root`（厂商默认口令 `listenai`），执行 `upgrade enter` 临时开启 RAM 中的访问标志；只读备份完成后执行 `upgrade exit` 恢复。CP 写入期间保留 boot 保护；完整读回校验需要再次临时打开该标志，并在重启前恢复。该开关本身不擦写镜像，不增加本流程的写入分区。

运行本固件时，上游 USB 描述符可能报告固定的 `FFBBCCDDEE001122`，不能将其视为芯片唯一号。更换实物后重新核对 Recovery 的 `BOOT-<chip>` 身份、目标原机备份及 AP；串号字母的大小写以当前 ADB 实际报告为准。

原机备份与刷写账本只保存在本地；需要回退时先核对目标唯一身份、原始镜像摘要和当前分区状态，再只恢复原 CP。新固件启动后写入的 SDK 配对/Wi-Fi 状态需要单独处理，不用旧全量备份覆盖最新状态。

官方通用 `adb_download.sh` 可批量操作设备；现场不使用无目标选择的批量刷写，不运行机器狗的动作自检脚本。

## USB 不枚举时的串口恢复

本次 Mini2 在 AP 拒绝 CP 后没有 USB 更新接口；UART0 仍能由聆思官方 `tools/burn/cskburn` 进入 ROM 更新。Mac 使用 SDK 随附的 `cskburn.darwin-x64`，具体工具参数以其 `--help` 为准。先关闭串口监视器，通过 `--chip-id` 核验目标与本机两份完整备份一致；不能用 FTDI 转接器编号代替芯片编号。

```sh
cskburn.darwin-x64 -C arcs -s <port> --chip-id
cskburn.darwin-x64 -C arcs -s <port> \
  --verify 0x0:0x600000 --verify 0xa00000:0x600000 \
  --verify 0x600000:<current-image-size>
```

先比较 ROM 返回的 MD5 与该设备已保存的闪存备份，确认 Boot、AP、资源及当前 CP。只在身份、原机备份和当前分区一致时，对 `0x600000` 发送一次 CP 写入并使用 `--verify-all`；不使用整片擦除，也不刷开发 Boot。之后另起一次 `--verify`，核对新 CP 以及两个完整非 CP 区域。记录为独立 ROM 摘要校验，不称为新的全片逐字节备份。

串口启动日志使用 UART0、921600。当前已核验板级映射为转接器 TX→PA02/RX、RX→PA03/TX、GND→GND，USB 继续供电。烧录工具和日志采集分别占用串口，不能同时打开。ROM 更新完成后仍须检查实际启动、AP 看门狗接管、屏幕和唤醒；未配对的设备通过官方 Impo BLE 流程绑定，不复制另一台的配对数据。

## 镜像完整性

构建退出成功后仍须运行 `python3 tools/check_image.py upstream/arcs/build-yunbao`；构建工具也会自动执行。它拒绝空/无效归档、缺失的 GPIO、屏幕、音频等设备注册项或唤醒/连续对话符号，以及超出 CP 分区的镜像。一次构建曾生成空 GPIO 归档而未导致链接命令失败，该镜像不能使用；不得把已有 USB 和网络能力当作板级初始化成功。
