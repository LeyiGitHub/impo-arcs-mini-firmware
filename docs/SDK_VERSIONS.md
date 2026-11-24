# SDK 版本与固件对应关系

本项目是一套共用源码的 **Impo ARCS Mini SDK 0.1.0**，支持按 **ListenAI ARCS SDK 0.1.6 / 0.1.8** 两种底层版本构建。公共接口、产品配置、应用组件和官方 Impo 协议实现共用；聆思底层依赖、兼容配置、预编译库及固件产物分别使用。

## 版本层级

| 对象 | 当前标记 | 含义与来源 |
| --- | --- | --- |
| 本项目 SDK | `0.1.0` | `sdk/include/impo_arcs_mini.h` 的 `IMPO_ARCS_MINI_SDK_VERSION`；两个底层目标使用同一公共接口 |
| Impo 官方 Gadget SDK | 提交 `ad0e6b7527501248631569516483e4845e2e5665` 加项目补丁 | `dependencies.lock.json` 与 `patches/`；配对、Link、国内路由及语音共用 |
| 聆思官方 ARCS SDK | **`0.1.6` 或 `0.1.8`** | 开发者分别提供对应 `ARCS_BASE`；不是本项目先前刷入固件的版本号 |
| 设备保留的 Boot / AP / 原机应用 | 按每台设备读取并记录 | 决定目标兼容条件；原机应用的 `3.x` 版本不能代替 AP 所用底层版本 |
| 产品名称与唤醒资源 | 按产品 JSON 和资源清单配置 | 与底层版本独立；相同名称或唤醒词不能证明 AP、模型或镜像相同 |

## 两个构建目标

| 项目 | ListenAI **0.1.6** | ListenAI **0.1.8** |
| --- | --- | --- |
| 当前已核验设备对应关系 | 原云宝、保留 0.1.6 AP | 当前通用 Mini2、保留 0.1.8 AP |
| 底层路径示例 | `/path/to/listenai-arcs-sdk-0.1.6` | `/path/to/listenai-arcs-sdk-0.1.8` |
| 命令行标记 | `--arcs-sdk-version 0.1.6`；也是默认值 | `--arcs-sdk-version 0.1.8`；必须显式选择 |
| 本项目兼容配置 | `sdk/profiles/listenai-0.1.6.conf` | `sdk/profiles/listenai-0.1.8.conf` |
| 输出目录规则 | `upstream/arcs/build-<产品 id>/` | `upstream/arcs/build-<产品 id>-arcs018/` |
| 双核初始化适配 | 旧版初始化流程 | 使用底层 SDK 已初始化的 IPC |
| 已核验 AP 的 CP 头要求 | 使用对应旧版布局 | 在镜像 `0x180` 增加 CP PSRAM 声明，并检查实际区间 |
| 公共接口与功能源码 | 共用 | 共用；按所选版本启用兼容分支 |

构建工具读取 `ARCS_BASE/VERSION`，与显式选择不符时在准备源码前停止。底层来源和源码树分别固定在 `dependencies.lock.json` 的 `arcs_sdk_profiles`；不会把一个版本的预编译归档拿给另一个版本使用。保留原机 AP 时仍须核对该设备的 IPC、内存、分区及唤醒资源，不能仅按外壳上的 Arcs Mini 2 型号直接混刷。

## 明确版本的构建命令

下面对应目前两台已核验设备的配置，名称和唤醒词仍由各自产品 JSON 决定。

```sh
# 目标：保留 ListenAI 0.1.6 AP 的原云宝
ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.6 \
  python3 tools/build.py --docker --arcs-sdk-version 0.1.6 \
  --product products/yunbao.json
# 产物目录：upstream/arcs/build-yunbao/

# 目标：保留 ListenAI 0.1.8 AP 的当前“小聆小聆”Mini2
ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.8 \
  python3 tools/build.py --docker --arcs-sdk-version 0.1.8 \
  --product products/arcs-mini2-xiaoling.json
# 产物目录：upstream/arcs/build-arcs-mini2-xiaoling-arcs018/
```

产品配置不绑定聆思版本；同一产品源码也可按所核验的目标 AP 选择底层版本构建。上表记录现有设备的对应关系，不将“云宝”或“小聆小聆”当作选择底层版本的判断条件。

## 产物标记与验收

两个输出目录内的镜像都叫 `impo-gadget.bin`，旁边的 `impo-sdk-build.json` 包含：

- `listenai_sdk_version`：本次使用的聆思底层版本，必须与目标保留 AP 对应。
- `product`、`wake_bundle`：本次产品及唤醒资源标识。
- `image_bytes`、`image_sha256`：精确镜像长度和摘要。
- `retained_ap_required`：此固件依赖设备保留的 AP，不附带 AP 更新。

分发和保存镜像时保留各自目录及元数据，不将两份同名 `.bin` 混放。可在分发目录名中同时标记产品、`listenai-0.1.6` / `listenai-0.1.8` 和源码提交号；不要仅标“最新版”。源码包只有一份，支持两个构建目标；它不包含聆思底层 SDK、模型或固件二进制。

源码提交、主机检查、两个目标的构建、精确镜像刷写读回，以及实际屏幕、语音和打断听测分别记录。兼容性不代表每个功能已在两个目标上均经人工验收；当前功能边界见 [VALIDATION.md](VALIDATION.md)，实际更新按 [FLASHING.md](FLASHING.md) 执行。
