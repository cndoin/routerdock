<div align="center">

![RouterDock — Your router. Within reach.](docs/assets/brand.svg)

**把 OpenWrt 的实时状态与常用操作，放到桌面小屏上。**

[![Checks](https://github.com/cndoin/routerdock/actions/workflows/checks.yml/badge.svg)](https://github.com/cndoin/routerdock/actions)
[![Release](https://img.shields.io/github/v/release/cndoin/routerdock?color=45bfff)](https://github.com/cndoin/routerdock/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-8b5cf6.svg)](LICENSE)
![ESP32](https://img.shields.io/badge/ESP32-320%C3%97240-1c3046)
![Local first](https://img.shields.io/badge/Local-first-58cfac)

[English](README.md) · **简体中文**

[开始使用](docs/INSTALL.md) · [硬件引脚](docs/HARDWARE.md) · [兼容范围](docs/COMPATIBILITY.md) · [参与开发](CONTRIBUTING.md)

</div>

<p align="center"><img src="docs/assets/device-overview.png" alt="ESP32 实际画布：资源占用、上下行流量、趋势与连接数" width="640" /></p>

RouterDock 是一个 ESP32 触控终端：抬眼看路由器状态，点一下切节点、测延迟或重启服务。不需要云端，不需要每次打开 LuCI，也不需要把 root 密码写进固件。

上图来自真实开发板与路由器的实时数据。顶部品牌图为示意插画；后台截图使用真实页面代码与演示网络配置，不包含个人凭据。

## 五个面板，放下常用信息

| 面板 | 功能 |
| --- | --- |
| **总览** | CPU、内存、温度、WAN 实时上下行、双线趋势、Clash 连接数与运行时长；点击连接数进入详情。 |
| **节点** | 策略组与节点翻页、Selector 切节点、本页批量测延迟、单节点重测、超时提示。 |
| **UA3F** | 服务状态、当前保留的连接/放行/改写记录、改写比例与最近目标。 |
| **流量** | 实时速率、累计计数、短期曲线，明确标注 Clash API 或网卡来源。 |
| **系统** | 路由器与设备信息、进程概览、服务操作、触摸校准与设备后台入口。 |

联网取数与触摸绘制分开运行。取数失败会保留旧值并标记过期；没有 OpenClash 或 UA3F，也能使用基础系统监控。

## 用浏览器配置，不再修改源码

![设备后台，演示配置](docs/assets/settings-desktop.png)

后台支持扫描并选择 **2.4GHz Wi-Fi**、手动输入隐藏网络、填写路由器地址与端口、设置设备令牌、修改刷新间隔和亮度，以及更改后台密码。

1. 首次启动，屏幕显示独立的 `RouterDock-xxxxxx` 热点和随机密码。
2. 手机或电脑连接该热点，打开 `http://192.168.4.1`。
3. 用户名为 `admin`，密码就是设备屏幕显示的密码。
4. 选择 Wi-Fi，填写路由器地址和安装器给出的设备令牌，保存后自动重启连接。

可以先只保存 Wi-Fi，联网后再填写路由器令牌。已联网时，访问设备的局域网 IP 即可进入后台；地址可在“系统 → 设备后台 / 配网”里查看。支持 mDNS 的网络也可以使用屏幕对应的 `routerdock-xxxxxx.local`。

**换路由器无需重新编译**：在新路由器安装采集端，把新地址和令牌填入设备后台即可。设置保存在设备 NVS 中，断电保留。

<details>
<summary>手机后台预览</summary>

<img src="docs/assets/settings-mobile.png" alt="手机端设备后台，演示数据" width="390" />

</details>

## 安装

### 路由器端

从 [Releases](https://github.com/cndoin/routerdock/releases/latest) 下载 `routerdock-agent.tar.gz`，上传到路由器 `/tmp`，以 root 执行：

```sh
mkdir -p /tmp/routerdock
tar -xzf /tmp/routerdock-agent.tar.gz -C /tmp/routerdock
sh /tmp/routerdock/install.sh
```

安装器会检查依赖、安装受令牌保护的 CGI 接口并打印设备令牌。它不会重启路由器，不改 OpenClash/UA3F 配置，不添加防火墙放行规则。依赖安装和源码部署见 [安装说明](docs/INSTALL.md)。

### ESP32 端

使用 Release 中的烧录包，或通过 PlatformIO 编译：

```sh
git clone https://github.com/cndoin/routerdock.git
cd routerdock
python scripts/embed_portal.py
pio run -d firmware
pio run -d firmware -t upload --upload-port YOUR_SERIAL_PORT
```

默认板型为 **ESP32-32E / E32R28T 类 2.8 寸 ILI9341 + XPT2046**。外观相近的 CYD 板可能引脚不同，请先核对 [硬件表](docs/HARDWARE.md)。路由器是 ARM 还是 x86，与显示板的引脚兼容是两个独立问题。

## 兼容与统计口径

- 采集端使用 Lua、Linux `/proc`、`/sys`、`ubus`、`uci` 与 `curl`，源码不依赖 CPU 架构。目前**仅完成一台 x86 iStoreOS 24.10.5 路由器的实机验证**；其他 ARM/MIPS 与定制固件仍需测试，不能当作已经验证。
- 当前插件实测：OpenClash 0.48.002 / Mihomo、UA3F 3.7.0。插件可选。自定义 WAN/LAN 逻辑接口、控制器地址和密钥在 `/etc/config/routerdock` 设置。
- 首页流量来自**单个 WAN 网卡的内核字节计数**；流量页优先使用 Clash 累计量，失败后回退网卡并标注来源。两个统计层次的数值可能不同。
- 节点测速为固定 URL 的请求延迟，单位 ms，**不是下载带宽测速**，不会切换所选节点。
- UA3F 统计是**当前保留日志的记录数**，日志轮换后可能减少，不是 DNS 广告拦截，也不是服务启动以来的累计总量。
- 历史按需采样，存放路由器 `/tmp`；重启、计数器重置或统计来源变化后重新开始。不包含月账单数据库。
- 进程 CPU 为生命周期平均值；没有适用温度传感器时显示不可用。

完整范围见 [兼容与验收记录](docs/COMPATIBILITY.md)。

## 找回配网与触摸校准

- 按住 **BOOT 四秒**，打开配网热点并显示后台密码。
- Wi-Fi 连接失败超过 45 秒，也会进入配网模式；仍会继续尝试连接原网络。
- 已联网时热点开放十分钟后关闭，需要时重新进入即可。
- 触摸偏移时，在“系统 → 触摸校准”依次按十字。校准保存在 NVS 中，正常应用固件升级不会清除它。

## 本地使用，凭据留在本地

后台需要登录，写入接口有 CSRF 检查；密码、令牌不会通过设置 API 回显，空白密码字段保留原值。热点密码每台设备独立生成，可在后台更改。

设备后台与采集端目前都使用 **HTTP**，只适合可信局域网，不要映射到公网。NVS 和路由器令牌文件未由本项目加密，持有设备、串口或路由器管理员权限的人仍能读取配置。设备令牌可调用节点切换与服务操作，请妥善保管。Clash 控制器密钥只留在路由器端；设备不需要路由器 root 密码。

## 开发

```sh
python scripts/embed_portal.py
python scripts/check_public.py
npm ci
npx playwright install chromium
npm test
pio run -d firmware
```

后台 HTML 直接嵌入固件，不需要 CDN、额外文件系统烧录或托管网站。CI 检查 Lua/Shell 语法、采集器夹具、后台交互和固件编译。

[架构](docs/ARCHITECTURE.md) · [接口](docs/API.md) · [贡献指南](CONTRIBUTING.md) · [安全说明](SECURITY.md)

## 许可与致谢

[MIT](LICENSE)。LovyanGFX、ArduinoJson、Espressif Arduino core 遵循各自上游许可，通过构建依赖获取，不在本仓库打包第三方源码。

视觉方向参考 [CYD-Dashboard](https://github.com/ange007/CYD-Dashboard) 与 [Nebula Monitor](https://github.com/Tech-Tweakers/nebula-monitor)。当前界面代码与矢量品牌图为本项目原创，没有复制这两个项目的代码或图片。
