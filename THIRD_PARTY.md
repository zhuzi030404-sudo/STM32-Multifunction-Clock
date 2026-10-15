# 第三方代码与素材来源

## 参考应用工程

- 名称：STM32-WATCH。
- 原始仓库：https://github.com/BQT0411/STM32-WATCH
- 本地参考版本提交：`be18a1645c5385f2f2739d4c0c32d4b1d3ff2795`。这是参考仓库的版本标识，不是本发布副本的提交。
- 参考范围：手表应用功能组织、RTC 和 MPU6050 的实现思路、软件总线与屏幕适配、部分图形素材及相关算法。当前没有完成逐行来源归属鉴定。
- 点阵资源重点范围：`Core/Src/dino.c` 中人物、障碍和云图案，`Core/Src/emoji.c` 中眉毛和嘴型；`oled.c` 的字体与图形相关来源也需要一并核对。
- 2026-10-15 检查本地参考资料与上游仓库页面时，未找到明确的应用工程开源许可证。未找到不等于获得授权；署名与附链接也不能替代授权确认。

移植和扩展包括 CubeMX / CubeIDE HAL 工程适配、横向图文菜单、倒计时蜂鸣提醒、光照与自动亮度、编码器输入及通信异常处理等。不能把这些扩展理解为对全部上游代码或素材拥有重新授权权利。

## 厂家与处理器支持代码

| 位置 | 原始许可说明 |
| --- | --- |
| `Drivers/STM32F1xx_HAL_Driver/LICENSE.txt` | 指向软件包许可；未随软件包获得相应条款时，文件说明采用 BSD-3-Clause |
| `Drivers/CMSIS/LICENSE.txt` | Apache License 2.0 正文 |
| `Drivers/CMSIS/Device/ST/STM32F1xx/LICENSE.txt` | 指向软件包许可；未随软件包获得相应条款时，文件说明采用 Apache-2.0 |

CMSIS 是 Arm 提供的处理器软件接口标准。上述目录的原有许可文件和源码版权声明均保留，具体适用范围以各文件及其软件包条款为准。`Core` 内自动生成或芯片模板文件也保留其原始声明。

## 发布前需要确认

1. 上游作者是否允许公开分发修改后的应用代码与图案，适用什么条款。
2. 获得的授权是否包括随附图片、字体、游戏点阵和其他素材。
3. 确认后记录可核查的授权来源、时间及适用范围，再决定自有新增代码的许可方式。

参考说明：https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository
