# 官方国内连接适配

当前固定到官方 [`ad0e6b7527501248631569516483e4845e2e5665`](https://github.com/impoai/impo-gadget-sdk/commit/ad0e6b7527501248631569516483e4845e2e5665)。相对上一版本 `0b83ffb7`，包含 VM 凭据租期复用、ESP32 WAV 结束处理及中继 TLS/Host 名称修正；ARCS 继续复用官方链路、VM API 和路由源码。此前升级包含以下三个上游变更：

- [4e687209：国内路由与自动回退](https://github.com/impoai/impo-gadget-sdk/commit/4e687209d6ab968de443c6f0dbe68a251e1b08f3)。
- [7d1f0ba：重连前刷新过期 VM 凭据](https://github.com/impoai/impo-gadget-sdk/commit/7d1f0ba1bb1a3e93df2be6230698658455c7c398)。
- [0b83ffb7：国内入口使用 TLS 透传](https://github.com/impoai/impo-gadget-sdk/commit/0b83ffb70dfbd7e1986b1e7746259335af8030f0)。必须包含此修正，不能只应用第一项或只替换域名。

## 连接行为

公开配置启用 `CONFIG_IMPO_CHINA_RELAY=y`，默认入口为官方 `gadget.itoy.cn:9443`。设备取得新 Wi-Fi 地址后，下一次请求会通过 HTTPS 查询该入口的 `/where?name=gw.itoy.cn`：国家为 `CN` 时使用国内入口，其他国家直连 `gadgets.impo.ai:443`。成功判断缓存 12 小时；判断失败暂时直连，5 分钟后重试。快速失败最多尝试三次；连续三次国内连接失败会直连 10 分钟。均使用官方 `impo_cloud_route.c` 原实现。

国内入口只透传 TLS 字节。TCP 连接目标是 `gadget.itoy.cn:9443`，TLS 的 SNI、证书主机名及业务 HTTP Host 改为 `gw.itoy.cn`，这是官方网关在 Cloudflare 上的自定义主机名，证书链必须验证成功。设备不会将凭据改发到 `/where?name=gw.itoy.cn`，该查询只判断网络地区。自定义网关域名保持原地址。

这是官方 SDK 提供的连接路径，设备仍独立执行 BLE 配对、凭据刷新、Noise 握手、注册及语音协议。无需 Mac 转发，也不增加本项目的云端桥接。升级 CP 应用时保留已有配对和 Wi-Fi，仍须执行分区备份与读回核验。

## ARCS 适配范围

`patches/0007-official-china-route.patch` 将上游路由实现直接加入 ARCS 构建，并复用其 Kconfig。ARCS Wi-Fi 的地址变化调用官方原子 reset，不在 DHCP 回调中发起网络请求。

ESP-IDF 兼容层新增 `common_name`，平台 TLS 将连接地址与认证身份分开，保持 `MBEDTLS_SSL_VERIFY_REQUIRED`。HTTP 适配保留显式 Host，避免同时发送国内入口和 Impo 两个 Host；没有显式 Host 时自动包含非默认端口。兼容层也传递官方的 HTTP 超时参数，原有平台接口继续可用。共享的 `noise_control.cpp`、`vm_api.c` 和 `impo_cloud_route.c` 使用同一份源码。

`0011-official-relay-trust-root.patch` 为精简 ARCS 证书包增加 SSL.com TLS ECC Root CA 2022，来自 macOS 系统根库，DER SHA256 为 `c32ffd9f46f936d16c3673990959434b9ad60aafbb9e7cf33654f144cc1ba143`，与 [CA 官方仓库](https://www.ssl.com/repository/) 一致。不固定叶证书、不关闭主机名或证书链校验；原有公共根继续保留。当前服务链也可经原有根交叉验证，新增根不代表旧证书包在当前网络一定无法连接。

## 验证边界

`sh tests/run_cloud.sh` 执行真实 ARCS HTTP、ESP HTTP/TLS 兼容层，替换底层网络以捕获连接参数及请求；覆盖国内地址、SNI/证书身份、单一 Host、超时参数、401 响应、直连和自定义网关。还运行上游路由测试，覆盖地区判断、缓存、失败回退、地址变化和关闭配置。`run_tls.sh` 检查真实 mbedTLS 配置的主机名及强制校验标志，并检查握手错误向上传播；这些主机测试不会访问真实账号。

2026-11-24 的 Mac 无凭据探测中，国内 `/where` 通过系统 CA 校验并返回 HTTP 200、国家 `CN`；固件内置 CA 也成功验证了该查询和 Impo 直连。以 `gadgets.impo.ai` 为 SNI 连接同一国内入口，默认 TLS 和两次 TLS 1.2 尝试均被重置。同期直连 Impo 的 TLS 1.2 认证及 HTTP 200 成功。该结果只描述当时 Mac 的网络路径，不能单独归因于服务端或运营商，也不能代替设备验收。

上一版本 `0b83ffb7` 的设备复验曾确认：成功识别大陆并选择国内入口，连续三次业务 TLS 握手被重置（`-0x0050`、errno 104），触发官方 10 分钟直连回退；随后直连完成注册和语音订阅。同期 Wi-Fi 持续关联。地区选择及回退策略已生效，国内中继的业务链路尚未通过；不能单凭这些日志将原因归于服务端或运营商。

启动栈修正版已通过屏幕、语音及连续对话人工确认，这些成功发生在直连回退路径。构建、刷写、软件回执与人工结果分别见 [验收状态](VALIDATION.md)。

新提交的 Mac 探测使用实际固件根证书包、TLS 1.2 和强制主机名校验：`/where?name=gw.itoy.cn` 返回 200/CN，连接中继且使用 `gw.itoy.cn` 为 TLS 名称与 Host 的 `/health` 返回 200。原有和新增根证书包均通过该服务当前链验证。这是宿主网络结果，尚不能代替云宝新固件启动、注册和语音验收。
