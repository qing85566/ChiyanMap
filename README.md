# ChiyanMap（赤焰地图）

[![Release](https://img.shields.io/github/v/release/qing85566/ChiyanMap?color=red&label=Release)](https://github.com/qing85566/ChiyanMap/releases)
[![Platform](https://img.shields.io/badge/Platform-Minecraft%20Bedrock%20(Windows)-green)](https://www.minecraft.net/)
[![Framework](https://img.shields.io/badge/Loader-LeviLamina-blue)](https://github.com/LiteLDev/LeviLamina)
[![Language](https://img.shields.io/badge/C%2B%2B-23-orange)](https://en.cppreference.com/w/cpp/23)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](./LICENSE)

**ChiyanMap** 是一款专为 **Minecraft 基岩版 (Windows)** 打造的高性能客户端渲染向地图与雷达模组。基于 **LeviLamina** 原生框架与 DirectX 11 / 11On12 渲染管线深度集成，纯客户端运行，无需服务器安装任何插件即可享受丝滑的 HUD 实时地图体验。

> ⚠️ **声明**：本模组全部代码均由 **AI（人工智能）编写生成**。使用即代表知情并自行承担相关风险。

---

## 🌟 核心特性

- **🗺️ 实时小地图 (MiniMap)**
  
  - 支持圆形 / 方形无缝切换、自由拖拽定位、缩放与显示半径调节。
  - 玩家朝向平滑旋转或固定正北；智能药水效果避让与 HUD 堆叠防护。
  - 内置多距离分层实体雷达（生物、玩家、怪物、已驯服宠物专属光环）。

- **🌍 全屏大地图 (World Map)**
  
  - 高流畅度平滑缩放、坐标即时跳转、多维度视角无缝漫游。
  - 框选高清导出地图 PNG 图片（支持自定义快捷键）。
  - 内置种子地图（Seed Map），支持结构分布、史莱姆区块与群系实时预测。

- **📍 路径点与死亡点系统 (Waypoints & Death Points)**
  
  - 支持路径点集合（Sets）管理、分类抽屉筛选、自定义颜色与图标。
  - 原版 3D 贯天信标光柱与屏幕距离指示器；支持快捷键切换集合并弹出平滑 Toast。
  - 死亡位置自动高精度记录，支持一键转为永久路径点，误删支持 `Ctrl+Z` 撤销恢复。

- **🌌 多世界与实例漫游 (Multiworld)**
  
  - 支持跨服/子服地图实例独立隔离与互联漫游，自动感知世界出生点变更。
  - 实例删除级联清理，不留磁盘孤儿文件。

- **⛏️ 洞穴地图与无规则探测 (Cave & Anarchy)**
  
  - 垂直深度切片洞穴透视，地下结构一目了然。
  - 无规则（Anarchy）区块历史修改痕迹探测，协助识别建筑痕迹与地形变动。

- **✏️ 涂鸦与标注系统 (Drawing & Annotations)**
  
  - 大地图自由涂鸦，支持画笔、折线、箭头、矩形、圆形、文本标记及图层管理。
  - 完整支持撤销 (`Ctrl+Z`) 与重做 (`Ctrl+Y`)。

- **⚡ 极致性能与稳定性**
  
  - 多线程分层异步磁盘写盘，内存缓存自愈管理。
  - DXGI Present 呈现层全链路异常容灾屏障，0 崩溃、0 掉帧。
  - 内置 16 种语言本地化支持与原生独立输入法 (IME) 输入支持。

---

## ⌨️ 常用快捷键（默认）

| 按键             | 功能                | 按键               | 功能                 |
|:-------------- |:----------------- |:---------------- |:------------------ |
| **`M`**        | 打开 / 关闭全屏大地图      | **`U`**          | 打开路径点管理器           |
| **`N`**        | 切换小地图显示开关         | **`Y`**          | 切换小地图形状（圆形 / 方形）   |
| **`J`**        | 切换小地图朝向（旋转 / 锁定北） | **`Shift` + 拖拽** | 大地图框选区域导出 PNG      |
| **`Tab` (长按)** | 大地图临时显隐实体头像       | **`Ctrl + Z`**   | 撤销上一步操作 / 恢复已删除路径点 |

> 💡 所有快捷键均可在设置面板（大地图齿轮图标）中自由重绑定与组合绑定。

---

## 📦 安装方法

1. 确保已安装 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 对应版本的 Minecraft 基岩版客户端。
2. 前往 [Releases](https://github.com/qing85566/ChiyanMap/releases) 下载最新版本的 `ChiyanMap.zip`。
3. 将解压出的 `ChiyanMap` 文件夹放置于客户端 `mods/` 目录下（如 `mods/ChiyanMap`）。
4. 启动游戏即可生效。

---

## 🛠️ 源码构建

### 环境要求

- **操作系统**：Windows 10 / 11 (x64)
- **编译工具**：[Visual Studio 2022](https://visualstudio.microsoft.com/)（C++ 桌面开发环境，MSVC v143+）
- **构建工具**：[XMake](https://xmake.io/) (3.0.0+)
- **语言标准**：C++23

### 构建步骤

```powershell
# 1. 克隆仓库
git clone https://github.com/qing85566/ChiyanMap.git
cd ChiyanMap

# 2. 配置并构建（Release）
xmake f -m release
xmake

# 编译产物位于 bin/ChiyanMap/
```

---

## 📄 开源许可证

本项目基于 [GNU General Public License v3.0 (GPL-3.0)](./LICENSE) 协议开源。
