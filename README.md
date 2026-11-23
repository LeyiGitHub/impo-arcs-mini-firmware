# IMPO + ARCS Mini 固件

基于 [Impo 官方 Gadget SDK](https://github.com/impoai/impo-gadget-sdk) 的云宝语音盒子固件工程。设备自己连接 `gadgets.impo.ai`，使用官方 BLE 配对、Noise 链路、设备命令和语音回合；不依赖原来的 Mac / ACK / LifeLike 桥接服务。

这是独立的社区适配仓库。官方 SDK 按 `dependencies.lock.json` 的固定提交获取到 `upstream/`，板级适配位于 `firmware/boards/arcs_mini_yunbao`。`patches/` 记录 ARCS 音频缓冲、TLS 读取调度、TCP 小包发送和录音后上传的社区修正；共享链路与语音代码不另写一份。

## 当前状态

- 云宝板级目标已实现：扬声器功放 PA01、左声道麦克风、PB04 按键。
- 2026-11-23 已通过一次实物问答：用户短按后问“二加三等于几”，Impo 回复“五”，用户确认听到完整回答；该回合约 24 秒后开始播放。当前仍存在连接停顿与重连，不能据此宣称稳定性通过。
- 短按电源键开始或停止语音回合，长按沿用 SDK 关机策略；不沿用旧 LifeLike 唤醒词。
- ST7789P3 单屏中文状态页已刷入实机，用户确认“云宝”、眼睛表情及中文状态显示正常，方向和文字正确。同一屏幕版固件在原网络上因 TLS 上传超时重连，切到手机热点后通过一次完整问答与人工听测，短按释放到回答开始约 38 秒。网络环境会影响结果，SDK 仍有慢写，长期稳定性未通过，详见 [屏幕适配](docs/SCREEN.md) 和验收记录。
- 不注册机器狗的电机、步态、IMU、触摸或摄像头能力；独立状态灯和电池曲线尚未适配。
- 主机测试、完整编译、实机刷写、Impo 配对和真实语音验收分别记录在 [验收状态](docs/VALIDATION.md)。代码存在不表示实机通过。

## 获取和构建

```sh
git clone https://github.com/LeyiGitHub/impo-arcs-mini-firmware.git
cd impo-arcs-mini-firmware
export ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.6
python3 tools/build.py --docker
```

需要 Python 3、Git、Docker 和你有权使用的 ListenAI ARCS SDK **0.1.6**。SDK 0.1.8 的 IPC API 不兼容；构建工具会拒绝混用。工具链镜像和 Impo SDK 提交固定于 `dependencies.lock.json`。不分发 ListenAI SDK、AP、boot、唤醒资源和原机闪存。

macOS 已安装官方 RISC-V 工具链、cmake、ninja 时，可省去 `--docker`。输出为 `upstream/arcs/build-yunbao/impo-gadget.bin`。

## 配对与使用

先核实板型、备份完整闪存，按 [刷写和恢复说明](docs/FLASHING.md) 只更新 CP 应用。接着在 Impo 的设备设置打开开发者模式，通过 BLE 添加 `ImpoGadget-XXXXXX`。SSID、密码和设备凭据通过官方配对保存到设备；公开固件不内置凭据。配对后短按 PB04 电源键触发语音，停顿后由 Impo 处理并通过盒子扬声器回复。已通过真实问答与完整听测，包括屏幕版在手机热点上的一次问答；原网络仍出现 TLS 写超时、语音下载停顿和重连问题，尚不能作为稳定可用固件。

屏幕“待机”跟随 SDK 客户端在线状态。通过单个 ADB 交互 shell 读取 `pair`、`link`、`gadget device.health`：`paired=1` 表示已保存配对，`state=online` 表示已建立 Impo 链路；注册成功还应检查 `link.register acked`，语音需单独听测。`screen` 可查看显示写帧状态，`screen test` 可测试颜色；驱动回执不能代替实物验收。不要把这些命令的原始输出发布到公开仓库。

基础命令直接来自官方 SDK：`device.health`、`device.set_name`、`speaker.say`、`speaker.play_url`、`speaker.set_volume`、`speaker.beep`、`speaker.stop`、`speaker.status`、`display.show_animation`。`python3 tools/gen_manifest.py` 可生成与实际命令一致的产品清单；不借用机器狗产品 ID。

## 测试

```sh
sh tests/run.sh
ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh tests/run_tls.sh
ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh upstream/arcs/tests/host/run.sh
```

第一项获取固定 SDK、应用补丁，检查按键策略、屏幕渲染边界和音频队列的缓冲行为；第二项检查真实平台 TLS 读取函数的非阻塞、超时行为和 TCP_NODELAY 的参数与失败容错；第三项运行官方 SDK 的配对、音频、语音、策略与链路纯逻辑测试。设置了 `ARCS_BASE` 时，第一项也会运行 TLS 读取测试。

设置 `ARCS_BASE` 后，第一项还会执行真实语音任务的录音与上传测试；也可单独运行 `sh tests/run_voice.sh`。它检查网络调用发生前麦克风已停止、PCM 顺序与 WAV 长度、慢上传、静音、失败和取消期限。

## 许可证

新增代码采用 MIT；保留的 ListenAI 板级声明采用 Apache-2.0，中文位图字形采用 SIL OFL 1.1。参见 `LICENSE`、`LICENSE-APACHE`、`LICENSE-OFL`、`NOTICE` 及下载的官方 SDK 授权文件。凭据、配对、设备日志、原机备份与用户状态禁止提交。
