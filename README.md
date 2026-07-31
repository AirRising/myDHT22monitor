# 基于 I.MX6ULL 的工业级物联网边缘网关 (Huawei Cloud IoT)

![C](https://img.shields.io/badge/Language-C-blue.svg) ![Platform](https://img.shields.io/badge/Platform-Embedded_Linux-lightgrey.svg) ![Arch](https://img.shields.io/badge/Arch-ARM_Cortex--A7-orange.svg) ![Cloud](https://img.shields.io/badge/Cloud-Huawei_IoTDA-red.svg)

> **项目状态：** 已完工 (Completed)
> **核心技术：** Linux 嵌入式应用开发、交叉编译、MQTT 协议、Rootfs 定制、SSL/TLS 加密认证

## 1. 项目背景

本项目基于正点原子阿尔法 I.MX6ULL 开发板构建，是一个端到端的工业物联网边缘网关系统。旨在构建一个高可靠的工业级物联网（IoT）闭环系统。通过 I.MX6ULL 边缘开发板采集 DHT22 传感器的温湿度数据，应用层利用华为云 IoT Device SDK (C语言) 进行 JSON 报文封装，并解决复杂的架构依赖与证书校验问题，最终通过 MQTTS 加密链路将数据直连直报至华为云 IoTDA 平台。

项目打通了从**硬件感知、交叉编译、根文件系统定制到云端安全通信**的完整嵌入式应用开发闭环，解决了工业落地场景中的诸多系统级工程问题。

## 2. 核心技术栈

* **硬件平台：** NXP i.MX6ULL (Cortex-A7)
* **操作系统：** 深度定制的嵌入式 Linux 根文件系统 (Rootfs)，集成 `wpa_supplicant`、`ntpdate`、`OpenSSH`
* **应用编程：** C 语言、华为云 IoT SDK、Paho-MQTT、OpenSSL 动态库集成
* **开发环境：** Ubuntu、交叉编译 (arm-linux-gnueabihf-gcc)、定制化 Makefile 构建系统

### 第三方开源库依赖

| 库名称 | 作用 | 平台适配 |
| :--- | :--- | :--- |
| **Huawei Cloud IoT SDK** | 云端物模型数据封装与上报 | C 语言源码级集成 |
| **Paho-MQTT-C** | 异步 MQTT 协议栈实现 | 交叉编译为 ARM 架构 `.so` |
| **OpenSSL** | 提供 SSL/TLS 底层加密认证 | 交叉编译为 ARM 架构 `.so` |
| **Zlib / Libcurl** | 数据压缩与基础网络传输 | 交叉编译为 ARM 架构 `.so` |

## 3. 系统架构

系统采用“传感器感知 - 边缘网关处理 - 云平台管理”的直连架构，减少了中间路由节点的延迟与故障率。

* **感知层：** DHT22 传感器连接至 I.MX6ULL，负责环境温湿度原始数据的物理采集。
* **边缘网关层 (ARM 应用侧)：**
  * **网络与安全：** 负责无线连网 (`wlan0`)、系统时钟同步（满足云端证书校验）、解析并加载 `rootcert.pem` 根证书。
  * **协议转化：** 运行 `mqttV5_test` 主应用，调用交叉编译的 `libpaho-mqtt3as.so` 与 `libssl.so`，建立安全的 8883 端口双向加密通信。
* **平台层 (华为云 IoTDA)：** 负责设备在线鉴权、物模型数据影子存储及可视化历史曲线展示。

## 4. 核心技术攻关与工程亮点

### 4.1 跨平台架构迁移与动态库依赖重构 (Architecture Mismatch 解决)

针对宿主机环境与目标板架构混淆导致的 `libz.so: file not recognized` 及 `undefined reference` 报错，通过底层 ELF 文件属性审查（`file` 工具），在 Ubuntu 上重新独立交叉编译了 OpenSSL、Paho-MQTT 等全套第三方库。确保所有产物严格符合 `ELF 32-bit LSB shared object, ARM, EABI5` 规范，彻底打通链接期依赖。

### 4.2 深度重构 Makefile 构建系统

面对华为云 SDK 庞大且复杂的源码目录，深度定制并重构了 Makefile：

* 强行指定交叉编译器变量（`CC = arm-linux-gnueabihf-gcc`），屏蔽宿主机环境变量干扰。
* 优化 `LDFLAGS` 与 `CFLAGS`，模块化管理多重测试目标，通过 `make mqttV5_test` 实现应用层工程的精准构建。

### 4.3 根文件系统 (Rootfs) 深度定制与环境固化

为保障网关系统脱离开发环境后仍能独立、稳定运行，对嵌入式根文件系统进行了定制打包：

* **网络与时间固化**：集成 `wpa_supplicant` 和 DHCP 客户端实现开机自动联网；集成 `ntpdate` 并在自启脚本 `/etc/init.d/rcS` 中加入时钟同步逻辑，解决冷启动时间重置引发的华为云 MQTTS 证书校验失败问题。
* **依赖环境固化**：将交叉编译的第三方 `.so` 库批量植入 `/usr/lib`，并在 `/etc/profile` 配置全局动态库寻址路径。
* **安全根证书部署**：将华为云 `rootcert.pem` CA 证书打包至系统级证书库，建立完整的 TLS 信任链。
* **系统瘦身**：使用 `arm-linux-gnueabihf-strip` 工具剥离所有库文件与可执行文件的调试符号，大幅缩减 Rootfs 镜像体积，适应受限的 Flash 存储空间。

---

## 5. 核心工程目录结构

```text
├── conf/                     # 系统配置文件及华为云 CA 根证书 (rootcert.pem)
├── demos/                    # 应用层业务代码存放区
│   └── device_demo/
│       └── mqttV5_test.c     # 核心主程序：包含 DHT22 数据读取与 MQTT 报文组装
├── include/                  # SDK 及第三方库头文件集合
├── lib/                      # 已适配 ARM 架构的动态链接库 (.so)
├── src/                      # 华为云 Device SDK 核心源代码
├── Makefile                  # 工程自动化构建脚本
└── mqttV5_test               # 构建生成的最终 ARM 架构可执行二进制文件
```

## 6. 硬件连接

| 传感器引脚 | I.MX6ULL 引脚 | 说明 |
| :--- | :--- | :--- |
| VCC | 3.3V | 电源供电[cite: 2] |
| DATA | GPIO 引脚 | 需要 10kΩ 上拉电阻[cite: 2] |
| GND | GND | 接地[cite: 2] |

## 7. 数据格式

**上报华为云 IoTDA 的标准物模型 JSON 示例：**

```json
{
  "services": [{
    "service_id": "Data",
    "properties": {
      "temperature": 26.5,
      "humidity": 62.1
    }
  }]
}
```

*(数据格式严格遵循华为云 Profile 定义，由 C 语言应用层 cJSON 库动态拼装完成)*

## 8. 实际部署与运行

网关程序的部署与启动全流程：

Bash

```
# 1. 跨网段推送程序与证书至板端 (需配置虚拟机桥接模式)
scp mqttV5_test root@192.168.x.x:/home/root/
scp -r conf/ root@192.168.x.x:/home/root/

# 2. 板端赋予执行权限并校准时间 (若未固化ntp)
chmod +x /home/root/mqttV5_test
date -s "2026-07-31 17:50:00"

# 3. 运行网关主程序
./mqttV5_test
```

*(在华为云 IoTDA 控制台添加对应产品与设备后即可观察在线状态)*

## 9. 项目成果展示

* **实时性：** 稳定实现温湿度高频上报。
* **安全性：** 完整实现了基于华为云根证书的 TLS 加密握手，保证了工业边缘节点数据传输的防窃听与防篡改。
* **稳定性：** Paho-MQTT 协议栈配置了 KeepAlive 机制，支持弱网环境下的自动重连重发，彻底解决边缘侧网络抖动导致的数据丢失问题。
