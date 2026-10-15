# STM32 多功能交互时钟与环境感知系统

基于 STM32F103C8T6 的模块化嵌入式实践项目。在参考手表工程基础上，移植到 STM32CubeMX / STM32CubeIDE，并扩展倒计时蜂鸣提醒、相对光照检测、自动亮度和旋转编码器菜单输入。

> **发布准备状态**：参考工程及部分点阵素材的再发布授权尚待确认；当前仅作私有备份与整理验证，不代表已经取得完整开源授权。请先阅读 [来源说明](THIRD_PARTY.md) 和 [许可证状态](LICENSE_STATUS.md)，不要直接给全仓库添加统一许可证。

## 功能

| 类别 | 内容 |
| --- | --- |
| 时间管理 | 数字 / 指针时钟、日期显示、时间设置、秒表、倒计时与蜂鸣提醒 |
| 界面交互 | 横向图标与中文菜单、按键长短按、旋钮切换菜单、息屏及按键唤醒 |
| 运动感知 | 六轴原始数据、姿态角、电子水平仪、参考归零 |
| 环境亮度 | 光敏采样、相对光照百分比、四档自动亮度、手动调节 |
| 辅助功能 | 外接灯控制、小恐龙游戏、六种动态表情 |
| 驱动处理 | 按键消抖、采样平滑、阈值回差、显示变化区域刷新与通信失败恢复 |

当前使用最小系统板和外接模块搭建原型，没有独立手表电路板、电池管理或无线通信。息屏只关闭显示输出；主页电量是占位信息。光照百分比未标定为实际照度，偏航角会漂移，不能用作指南针。

## 亮度界面预览

![亮度页面模拟预览](docs/images/brightness-simulation.png)

上图为界面模拟图，**不是开发板实拍或实测采样结果**。最终实物照片与完整演示视频待补充。

## 开发环境

- 主控：STM32F103C8T6，64 KB Flash、20 KB RAM。
- STM32CubeIDE 2.1.1；STM32CubeMX 6.17.0。
- STM32Cube FW_F1 V1.8.7；GNU Tools for STM32 14.3.rel1。
- C 语言、HAL（硬件抽象库）、软件 I²C（双线通信）、ADC（模数转换器）、RTC（实时时钟）、外部中断。
- 系统时钟 72 MHz，RTC 使用外部 32.768 kHz 晶振，ADC 时钟 12 MHz。

## 快速开始

1. 按 [硬件接线](docs/WIRING.md) 连接模块，断电接线并确认供电与共地。
2. 启动 STM32CubeIDE，使用 `File → Import → General → Existing Projects into Workspace` 选择本目录。工程内部名称保持为 `Watch_Cube`，仓库文件夹名称可以不同。
3. 选择 `Debug` 配置并执行 `Project → Build Project`。仓库不提供本机 `Debug` 目录，开发环境会重新生成。
4. 根据 [编译与下载说明](docs/BUILD.md) 在自己的电脑上创建调试配置。下载会改写芯片程序，应先确认目标板和已有固件。
5. 参考 [操作说明](docs/USAGE.md) 使用各页面。

## 项目结构

```text
Core/                  应用代码与芯片启动代码
Drivers/               厂家驱动及其原始许可证
docs/                  接线、操作、构建、架构和验证说明
Watch_Cube.ioc         CubeMX 引脚与外设配置
.project / .cproject   CubeIDE 工程配置
makefile.defs          新增模块的构建兼容配置
STM32F103C8TX_FLASH.ld  芯片程序与内存布局
THIRD_PARTY.md         参考工程及第三方内容来源
LICENSE_STATUS.md     当前授权状态
```

## 从哪里读代码

先读 `led.c → key.c → main.c 的 main() → menu.c`，再读计时、光照、编码器、显示和运动模块。项目新增应用代码带有中文说明；系统及厂家代码保留原有内容。

- [软件架构与关键实现](docs/ARCHITECTURE.md)
- [验证记录与限制](docs/VALIDATION.md)
- [发布前检查](docs/PUBLISHING.md)

## 项目来源

参考 [BQT0411/STM32-WATCH](https://github.com/BQT0411/STM32-WATCH)。本项目以移植与扩展为定位，基础功能、既有算法或图案不声明为全部原创。厂家驱动的授权与参考应用工程的授权分别处理，详见 [THIRD_PARTY.md](THIRD_PARTY.md)。
