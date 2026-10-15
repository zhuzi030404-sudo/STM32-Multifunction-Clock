# 编译与下载

## 环境

本次整理使用 STM32CubeIDE 2.1.1、CubeMX 6.17.0、STM32Cube FW_F1 V1.8.7、GNU Tools for STM32 14.3.rel1。工程源码中保留了所需HAL和CMSIS文件。其他版本尚未逐一验证。

## 图形界面导入

1. 在自己电脑上安装STM32CubeIDE，选择一个与现有开发工程不同的工作区。
2. `File → Import → General → Existing Projects into Workspace`，根目录选择仓库。
3. 选中识别到的 `Watch_Cube`，完成导入。不要因为仓库名不同就只重命名某个配置文件。
4. 右键工程，`Build Configurations → Set Active → Debug`。
5. `Project → Build Project`，确认控制台无错误。工程会生成自己的Debug目录；不要复制别人的Debug目录代替构建。

## 命令行导入构建

Windows PowerShell示例：将两个路径换成自己的位置。`headless-build.bat`是CubeIDE自带的命令行构建入口，`-data`指定独立工作区。

```powershell
& 'C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE\headless-build.bat' `
  -data 'E:\STM32BuildWorkspace' `
  -import 'E:\GitHub\STM32-Multifunction-Clock' `
  -build 'Watch_Cube/Debug'
```

已有工作区请避免与另一个正在运行的IDE实例共用。源码导入后生成的编译规则应该引用当前副本，而非最初作者电脑上的绝对路径。

## 创建本机调试配置

本仓库不附带开发机的 `.launch` 文件，以免包含本地日志目录、端口或下载器序列号。

1. 正确连接ST-LINK和已供电开发板，选择工程后进入 `Run → Debug Configurations`。
2. 新建 `STM32 C/C++ Application`，工程选 `Watch_Cube`，程序选 `Debug/Watch_Cube.elf`。
3. 调试器选ST-LINK，接口选SWD（串行线调试）；连接不稳定时可从1000 kHz调试频率检查。
4. 根据接线选择复位方式。只有确实接了NRST才使用需要该信号的连接方式。
5. 确认后执行Debug。此步骤会下载并覆盖目标板程序，本文整理和编译检查不会自动执行下载。
6. 若默认停在main，按F8继续运行，红色停止按钮用于结束调试。

## CubeMX重新生成

先保存修改并备份。自定义代码应在 `USER CODE BEGIN/END` 区间内；自行编写的独立驱动文件也应保留。`makefile.defs`兼容开发环境尚未更新新增源码列表的情况；不要手动编辑Debug中自动生成的makefile来作为长期配置。
