# Impo ARCS Mini SDK 0.1.0

本 SDK 面向与当前云宝相同接线的 Arcs-Mini2：LS26、240×240 ST7789P3 屏幕、PB04 主功能键、PA01 扬声器功放、左声道麦克风。产品名称、平台产品 ID、对话说明、连续对话和唤醒资源由产品配置决定。`arcs_mini_yunbao` 是保留的硬件构建标识，不再决定产品名称或唤醒词。

官方 ESP32 也以 [Device SDK](https://github.com/impoai/impo-gadget-sdk/tree/main/esp32) 名义交付完整固件、通用组件、板级描述和构建配置。本项目沿用该组织方式，继续调用固定官方 SDK 的配对、Noise、语音和国内路由代码。它是社区 ARCS 适配 SDK，依赖开发者另行提供的 ListenAI SDK 0.1.6；不是一个脱离这些依赖的二进制库。

## 目录与接口

| 目录 | 用途 |
| --- | --- |
| `sdk/include/`、`sdk/src/` | 公共 C/C++ 头文件、产品 API、应用扩展及通用唤醒资源校验 |
| `firmware/boards/arcs_mini_yunbao/` | 同款硬件的引脚、屏幕、按键、功放和 AP 音频接入 |
| `products/yunbao.json` | 保持“你好云宝”的默认产品 |
| `examples/button_companion/` | 名称为 Mini、关闭本地唤醒、保留连续对话及一个自定义命令的完整应用 |
| `resources/wake/` | 唤醒资源的元数据；不包含模型或原机资源 |
| `patches/` | 固定官方 SDK 上的可复核修正 |

公共入口是 `sdk/include/impo_arcs_mini.h`：

- `impo_arcs_mini_product()`：读取本次构建的产品及唤醒配置，返回静态只读数据。
- `impo_arcs_mini_begin_conversation()`：在线且空闲时请求官方语音回合；离线或正在对话时返回 −1。
- `impo_arcs_mini_say(text)`：通过官方 `/tts` 发起播放，立即返回；不代表播放完成。
- `impo_arcs_mini_set_wake_enabled(bool)`、`impo_arcs_mini_wake_available()`：暂停/恢复已初始化的本地唤醒及读取可用性。产品关闭唤醒或初始化失败时不能凭此启用引擎。
- `impo_arcs_mini_send_event()`：沿用官方事件及 JSON 所有权约定。

接口用于任务上下文。应用的 `impo_arcs_mini_application()` 返回静态描述，可增加官方格式的 `gadget_command_t`、`gadget_event_t`，提供初始化和状态回调。`on_init` 在板级初始化中运行，此时音频和网络尚未就绪；`on_status` 在 SDK 状态任务上运行，必须快速返回。长任务放到应用自己的 FreeRTOS 任务。标准命令、协议和 `main()` 均继续由官方工程管理。

## 构建默认产品

```sh
git clone https://github.com/LeyiGitHub/impo-arcs-mini-firmware.git
cd impo-arcs-mini-firmware
export ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.6
python3 tools/build.py --docker --product products/yunbao.json
```

产物：`upstream/arcs/build-yunbao/impo-gadget.bin`。生成的产品头文件、字体、Kconfig 和元数据在 `upstream/arcs/products/yunbao/`。构建不会自动烧录设备。构建器检查生成归档的格式，发现损坏时仅删除当前构建目录中的相关产物并重建；一次执行最多额外重建一次，仍失败则停止。

## 新建同款硬件产品

复制 `examples/button_companion/` 到自己的项目目录，修改 `product.json` 和 `app.c`：

```sh
python3 tools/build.py --docker \
  --product /path/to/my-product/product.json \
  --app-dir /path/to/my-product
```

可直接构建仓库内示例：

```sh
python3 tools/build.py --docker \
  --product examples/button_companion/product.json \
  --app-dir examples/button_companion
```

示例产物位于 `upstream/arcs/build-mini-companion/impo-gadget.bin`。未传 `--app-dir` 时使用空应用；传入时，目录必须包含 `CMakeLists.txt` 并实现 `impo_arcs_mini_application()`。不复制或修改 SDK 主流程。

产品说明随清单在 Impo 开发者平台配置；编译 CP 不会自动更新云端产品定义。

产品 JSON 必须设置 `schema_version: 1`、`id`、`display_name`、`model_id`、`node_prefix`。`id`、`model_id`、`node_prefix` 为 1–32 位小写字母、数字或连字符；`id` 同时决定构建目录。可选项：

| 字段 | 含义 |
| --- | --- |
| `screen_title` | 屏幕标题，默认使用名称；须在 216 像素内 |
| `product_id` | Impo 平台上已注册的产品 ID，未注册时为 null；与本地 `id` 不同 |
| `instructions` | 给 Impo 的产品说明，最多 1500 UTF-8 字节；省略时按配置生成 |
| `continuous_dialogue` | 回答后继续收音，默认 true |
| `china_relay` | 官方大陆路由及失败回退，默认 true |
| `wake` | 关闭时为 `{"enabled":false}`；启用时还须提供 `bundle` 路径 |

构建拒绝未知字段，包括 Wi-Fi 密码、账号凭据等。开发者使用自己的 Impo 账号配对，同款设备的设备身份不会从配置或示例复制。

## 更换唤醒词

配置同时控制显示文本和引擎结果过滤。声学识别仍由 AP 模型完成：修改字符串不会训练模型，也不能让“你好云宝”模型识别另一个词。

1. 获取与当前 ACOMP/AP 兼容、包含目标唤醒词的单麦模型及 wrap。关键词字符串使用引擎实际输出的 `keyword`，可同时列出拼音和中文。
2. 从真实二进制生成清单，避免手填哈希：

```sh
python3 tools/wake_bundle.py --id xiaoban-v1 \
  --label '你好小伴' \
  --keyword 'ni3 hao3 xiao3 ban4' --keyword '你好小伴' \
  --model /private/resources/cae_esr_xiaoban.bin \
  --wrap /private/resources/wrap_xiaoban.json \
  --output /path/to/my-product/wake-bundle.json
```

3. 产品配置设置 `"wake":{"enabled":true,"bundle":"wake-bundle.json"}`，路径相对产品 JSON。显示名称可以独立于唤醒词设置。
4. 资源须预先存在于设备保留的 ROMFS 根目录，文件名与清单一致。资源写入由资源供应商的适配流程处理；本 SDK 的构建及 CP 刷写流程不安装模型，也不覆盖 AP、boot 或资源分区。若设备原 ROMFS 只有另一套模型，单刷 CP 无法完成更换。

清单格式见 `resources/wake/yunbao-preserved.json`，包含标签、1–8 个结果词，以及模型和 wrap 的文件名、SHA256。启动时逐字节计算资源摘要，检查模型头和单麦 wrap，随后交给官方 ACOMP 初始化；缺失、不匹配或引擎失败时不报告唤醒可用，保留按键对话。哈希验证只确认资源身份，不能证明模型准确率、AP 兼容性或实际唤醒效果；新词仍须人工验收。

清单可公开，模型的分发遵循其授权。源码包不附带原机模型和 wrap。

## 中文字体

SDK 自带默认状态字形及可打印 ASCII 子集。新增中文标题/唤醒提示若缺字，构建会明确报错，不能静默显示空白。提供原字体即可按产品生成子集：

```sh
python3 -m pip install -r requirements-tools.txt
python3 tools/build.py --docker --product /path/to/product.json \
  --font /path/to/NotoSansSC-variable.ttf
```

字体源为 Noto Sans SC，SHA256 必须为 `a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`。取得方式及授权见 `NOTICE`；默认产品和英文示例无需 Pillow 或源字体。屏幕当前支持 BMP 字符，不支持 emoji。`--font` 使用构建宿主的 Python，不由 Docker 容器安装 Pillow。

## 产品清单、测试和打包

```sh
python3 tools/gen_manifest.py --product products/yunbao.json
python3 tools/gen_manifest.py --product examples/button_companion/product.json \
  --commands examples/button_companion/app.c:commands
sh tests/run.sh
ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh tests/run_voice.sh
python3 tools/package_sdk.py --output /path/to/output
```

自定义应用须将命令/事件的静态 C 表通过 `--commands FILE:TABLE`、`--events FILE:TABLE` 加入产品清单；可重复传入多个表。工具不能推断任意 C 程序动态生成的描述。默认固件已提供的命令由官方表生成，不能重复注册同名命令。

打包要求所有 SDK 更改已提交，仅导出 Git 跟踪源码，附版本、提交、依赖固定及源码摘要。ListenAI SDK、模型、AP、boot、生成固件、日志和凭据均不包含。刷写仍按 [分区核验流程](FLASHING.md) 进行。

SDK 重构的构建/测试结果与设备上原有固件的实物结果分别记录在 [验收状态](VALIDATION.md)。国内中继仍有已记录的 TLS 重置问题，官方回退保持启用。
