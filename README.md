# ChiyanMap

**English** | [简体中文](./README_zh.md)

[![Release](https://img.shields.io/badge/Release-v0.5.5--mc26.51-red)](https://github.com/qing85566/ChiyanMap/releases)
[![Platform](https://img.shields.io/badge/Platform-Minecraft%20Bedrock%20(Windows)-green)](https://www.minecraft.net/)
[![Framework](https://img.shields.io/badge/Loader-LeviLamina-blue)](https://github.com/LiteLDev/LeviLamina)
[![Language](https://img.shields.io/badge/C%2B%2B-23-orange)](https://en.cppreference.com/w/cpp/23)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](./LICENSE)

**ChiyanMap** is a high-performance, client-side map and radar mod designed exclusively for **Minecraft Bedrock Edition (Windows)**. Built on the **LeviLamina** native framework and deeply integrated into the DirectX 11 / 11On12 rendering pipeline, it runs purely within the client—requiring zero server plugins for a buttery-smooth real-time HUD map experience.

> ⚠️ **Disclaimer**: All code in this mod was generated and written by **AI (Artificial Intelligence)**. By using this mod, you acknowledge this and assume all associated risks.

---

## 🌟 Key Features

- **🗺️ Real-Time MiniMap**
  - Seamless toggle between circular and square shapes, free dragging, scaling, and view distance radius adjustment.
  - Smooth player-direction rotation or locked to North; smart potion HUD push-box avoidance.
  - Multi-distance layered entity radar (passive mobs, players, hostile mobs, and tamed pets with glowing auras).

- **🌍 Fullscreen World Map**
  - Ultra-smooth panning and zooming, instant coordinate jumping, and cross-dimension viewport roaming.
  - Selection box export of high-resolution map PNGs (with customizable hotkey).
  - Integrated Seed Map supporting structure distribution, slime chunk, and biome predictions.

- **📍 Waypoints & Death Points System**
  - Waypoint Sets management, category drawer filtering, custom colors, and icons.
  - In-game 3D beacon beams and distance HUD indicators; hotkey set switching with smooth on-screen toast notifications.
  - High-precision automatic death location recording, one-click conversion to permanent waypoints, and `Ctrl+Z` undo restoration for accidental deletions.

- **🌌 Multiworld & Instance Roaming**
  - Separate isolation and interconnected roaming between server worlds and custom map instances, automatically detecting spawn changes.
  - Deep cascade cleanup upon instance deletion—zero leftover orphaned files.

- **⛏️ Cave Map & Anarchy Detection**
  - Vertical depth-sliced cave X-ray mode reveals underground layouts at a glance.
  - Anarchy chunk history modification detection to identify player structures and terrain modifications.

- **✏️ Drawing & Annotation System**
  - Freehand drawing on the fullscreen map with pens, polylines, arrows, rectangles, circles, text labels, and layer management.
  - Full undo (`Ctrl+Z`) and redo (`Ctrl+Y`) support.

- **⚡ Ultimate Performance & Stability**
  - Multi-threaded asynchronous layered disk writes and self-healing in-memory cache management.
  - Full-pipeline exception barrier across DXGI Present—0 crashes, 0 frame drops.
  - Native independent IME input host and 16 built-in localization languages.

---

## ⌨️ Default Hotkeys

| Key | Action | Key | Action |
| :--- | :--- | :--- | :--- |
| **`M`** | Toggle Fullscreen World Map | **`U`** | Open Waypoint Manager |
| **`N`** | Toggle MiniMap Visibility | **`Y`** | Toggle MiniMap Shape (Circle / Square) |
| **`J`** | Toggle MiniMap Rotation (Player / North) | **`Shift` + Drag** | Select Box to Export PNG on World Map |
| **`Tab` (Hold)** | Temporarily Show/Hide Entity Heads on Map | **`Ctrl + Z`** | Undo Action / Restore Deleted Waypoint |

> 💡 All hotkeys can be rebound or combined in the settings panel (gear icon on the world map).

---

## 📦 Installation

1. Ensure [LeviLamina](https://github.com/LiteLDev/LeviLamina) is installed for your Minecraft Bedrock client version.
2. Download the latest `ChiyanMap.zip` from [Releases](https://github.com/qing85566/ChiyanMap/releases).
3. Extract the `ChiyanMap` folder into your client's `mods/` directory (e.g., `mods/ChiyanMap`).
4. Launch the game.

---

## 🛠️ Building from Source

### Prerequisites
- **Operating System**: Windows 10 / 11 (x64)
- **Compiler**: [Visual Studio 2022](https://visualstudio.microsoft.com/) (Desktop development with C++, MSVC v143+)
- **Build Tool**: [XMake](https://xmake.io/) (3.0.0+)
- **Standard**: C++23

### Build Steps
```powershell
# 1. Clone repository
git clone https://github.com/qing85566/ChiyanMap.git
cd ChiyanMap

# 2. Configure and build (Release)
xmake f -m release
xmake

# Compiled binaries will be generated in bin/ChiyanMap/
```

---

## 📄 License

This project is licensed under the [GNU General Public License v3.0 (GPL-3.0)](./LICENSE).
