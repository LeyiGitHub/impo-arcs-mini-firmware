# IMPO + ARCS Mini 固件

基于 [Impo 官方 Gadget SDK](https://github.com/impoai/impo-gadget-sdk) 的云宝语音盒子固件工程。设备自己连接 `gadgets.impo.ai`，使用官方 BLE 配对、Noise 链路、设备命令和语音回合；不依赖原来的 Mac / ACK / LifeLike 桥接服务。

这是独立的社区适配仓库。官方 SDK 按 `dependencies.lock.json` 的固定提交获取到 `upstream/`，板级适配位于 `firmware/boards/arcs_mini_yunbao`，共享链路与语音代码不另写一份。

## 当前状态

- 云宝板级目标已实现：扬声器功放 PA01、左声道麦克风、PB04 按键。
- 短按电源键开始或停止语音回合，长按沿用 SDK 关机策略；不沿用旧 LifeLike 唤醒词。
- 不注册机器狗的电机、步态、IMU、触摸或摄像头能力；屏幕、状态灯和电池曲线尚未适配，当前运行时会黑屏。
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

先核实板型、备份完整闪存，按 [刷写和恢复说明](docs/FLASHING.md) 只更新 CP 应用。接着在 Impo 的设备设置打开开发者模式，通过 BLE 添加 `ImpoGadget-XXXXXX`。SSID、密码和设备凭据通过官方配对保存到设备；公开固件不内置凭据。配对后短按 PB04 电源键触发语音，停顿后由 Impo 处理并通过盒子扬声器回复。完整语音仍在调试，当前存在 TLS 写超时和重连问题，尚不能作为稳定可用固件。

屏幕目前不能指示联网状态。通过单个 ADB 交互 shell 读取 `pair`、`link`、`gadget device.health`：`paired=1` 表示已保存配对，`state=online` 表示已建立 Impo 链路；两者都不等于语音验收成功。不要把这些命令的原始输出发布到公开仓库。

基础命令直接来自官方 SDK：`device.health`、`device.set_name`、`speaker.say`、`speaker.play_url`、`speaker.set_volume`、`speaker.beep`、`speaker.stop`、`speaker.status`。`python3 tools/gen_manifest.py` 可生成与实际命令一致的产品清单；不借用机器狗产品 ID。

## 测试

```sh
sh tests/run.sh
ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh upstream/arcs/tests/host/run.sh
```

第一项检查真实按键策略；第二项运行官方 SDK 的配对、音频、语音、策略与链路纯逻辑测试。首次运行第二项前执行 `python3 tools/prepare_sdk.py`。

## 许可证

新增代码采用 MIT；保留的 ListenAI 板级声明采用 Apache-2.0。参见 `LICENSE`、`LICENSE-APACHE`、`NOTICE` 及下载的官方 SDK 授权文件。凭据、配对、设备日志、原机备份与用户状态禁止提交。
