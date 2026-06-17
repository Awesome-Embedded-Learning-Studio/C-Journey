# 项目：MCU 嵌入式实践（8051）

> 对应 [ROADMAP](../../ROADMAP.md) · **阶段 6：嵌入式 C 与硬件相关开发**
> 📖 先读知识点：[裸机 C 入门：在 8051 上写一个会跑的时钟](../../documents/06-embedded/0-8051-bare-metal-basics.md)

基于 8051 单片机的小型嵌入式项目，练习寄存器操作、外设驱动（GPIO / Timer / LCD / 按键）与裸机程序组织。

## 项目

| 目录 | 内容 |
|---|---|
| `Timer/` | 基于 Timer0 + LCD1602 的时钟显示（含按键调时） |
| `LEDMatrix/` | LED 点阵驱动 |
| `GamesForMCU51/` | 单片机小游戏（按键 / 蜂鸣器 / 逻辑） |
| `LIBS/` | 公共驱动库 |

每个工程含 Keil 工程文件（`.uvproj` / `.uvopt`）与 `STARTUP.A51` 启动文件。

## 如何使用

- 用 **Keil µVision** 打开对应目录的 `.uvproj` 进行编译与烧录
- 重点关注：寄存器位操作、中断服务程序（ISR）、`volatile` 的使用

## 学习要点（对照阶段 6）

- 裸机程序结构、中断向量表、启动文件
- `volatile` 与内存映射寄存器
- GPIO / Timer / LCD1602 / 按键消抖的驱动分层

---
*整理自 2023–2024 学习存档。原 Keil 编译输出（Objects / Listings）已清理。*
