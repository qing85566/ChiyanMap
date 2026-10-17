# Hotkeys, Settings & Performance Tuning Guide

**English** | [简体中文](../zh/08-settings-and-hotkeys.md) | [Back to Docs Index](../README.md)

ChiyanMap is engineered for deep customization. From hotkeys and visuals to low-level SQLite disk I/O throughput and render framerates, every parameter can be tuned to suit your preferences and hardware setup.

---

## 1. Hotkey Rebinding System

Click the gear icon on the World Map and switch to the **Hotkeys** tab:

### 1.1 Key Listening Workflow
- Click any action's keybind button. The button will highlight in a listening state (`[Press any key...]`);
- Press any physical key on your keyboard to instantly bind it. The new configuration saves immediately.
- Press **`Esc`** at any time to cancel rebinding.
- Click the reset/disable icon next to a keybind to clear it.

### 1.2 Modifier Key Combinations
The system supports single keys (e.g., `M`, `U`, `N`) as well as multi-key modifier combinations:
- Examples: `Ctrl + M`, `Shift + Alt + W`, `Ctrl + Shift + F`.
- Uses conflict-free low-level physical key scanning (`ReadPhysicalKeyState`). It intelligently yields during text entry while delivering instant response during gameplay.

### 1.3 Rebind Undo (Ctrl+Z)
If you make a mistake while rebinding hotkeys, press **`Ctrl + Z`** inside the Hotkeys tab to revert to your previous configuration.

---

## 2. Search Filter & Factory Reset

### 2.1 Live Fuzzy Search
The settings panel contains dozens of granular settings. To locate options quickly, use the **Live Search Bar** at the top:
- **Multilingual Search**: Type English keywords (e.g., `scale`, `radar`, `opacity`) or localized terms to filter options instantly.
- The indicator displays total matches (e.g., `Matched 4 settings`), with a one-click button to clear the query.

### 2.2 Dual-Granularity Factory Reset
- **Per-Setting Reset**: Every slider and toggle features a small reset arrow (`↺`) to restore that individual setting to its official default.
- **Global Reset**: Click "Restore Defaults" at the bottom of the panel to bring up a confirmation dialog. Confirming resets the page and refreshes GPU textures immediately.

---

## 3. Native Independent IME Support (Native IME)

In standard Bedrock ImGui overlays, Chinese/multilingual Input Method Editors (IME) often fail to trigger, or typing forces the game into a background pause state.

ChiyanMap solves this with a **Floating Host Native IME Bridge**:
- Full compatibility with Microsoft Pinyin, Sogou, WeChat Keyboard, and standard Windows IME engines.
- In waypoint naming, search filters, coordinate inputs, and drawing text boxes, the IME candidate box floats naturally alongside the cursor.
- **Anti-Pause Interception**: The system tricks the game window into maintaining active status during text entry, ensuring the game never pauses or drops frames while typing.

---

## 4. Storage Architecture & Performance Tuning

For long-term multiplayer worlds or massive maps with hundreds of thousands of explored blocks, the mod offers deep storage options:

### 4.1 SQLite WAL High-Speed Database vs. JSON Mode
- **`useSqliteDatabase` Toggle**:
  - *Enabled (Recommended)*: Activates the embedded SQLite3 engine with `WAL (Write-Ahead Logging)`, `NORMAL` synchronous mode, and `MEMORY` temporary caching.
  - Delivers extraordinary disk write throughput, ensuring **zero frame drops or stuttering** even during high-speed elytra flight or Nether ice highway travel.
  - *Disabled*: Data is saved as standard `.json` files, suitable for players who prefer manually inspecting or editing files with a text editor.

### 4.2 Fast Writes & Batch Interval
- **Fast Writes (`fastWritesEnabled`)**: Groups scattered chunk updates into an in-memory queue.
- **Batch Interval (`writeBatchIntervalMs`)**: Configurable between `100ms` and `2000ms`. Increasing the interval on lower-end systems reduces solid-state drive (SSD) write amplification and saves CPU cycles.

### 4.3 MiniMap FPS Limit
- The MiniMap updates at your monitor's full refresh rate (e.g., 144Hz / 240Hz) by default.
- When playing on laptops or integrated graphics, you can cap the MiniMap to `30 FPS` or `60 FPS` in settings to conserve battery life and reduce thermal load.

### 4.4 Runtime Directory Structure Reference
For backups, migrations, or modpack creation, the runtime data layout in `mods/ChiyanMap/` is structured as follows:

```text
mods/ChiyanMap/
├── ChiyanMap.dll                  # Core compiled mod dynamic link library
├── manifest.json / tooth.json     # LeviLamina dependencies and version metadata
├── config.json                    # User graphics, radar rules, and hotkey bindings
├── lang/*.json                    # 16 localization language dictionaries
├── heads/*.png                    # 80+ mob radar pixel head textures
├── data/
│   ├── <World_ID>_database.db     # SQLite WAL database file
│   ├── cache/<World_ID>/          # 512x512 region binary terrain chunks
│   │   ├── dim_0/                 # Overworld chunks (includes cave/ data)
│   │   ├── dim_1/ & dim_2/        # Nether and The End chunks
│   │   └── maps/<Map_ID>/         # Custom multiworld instance chunks
│   └── export/*.png               # High-resolution PNG map exports
├── waypoints/<World_ID>_*.json    # Waypoints, sets, and category folders
├── deaths/<World_ID>.json         # Death records linked to waypoints
├── drawings/<World_ID>_*.json     # Vector annotations and layer data
├── anarchy/<World_ID>_anarchy.json# Historical player modification hotspots
├── breadcrumbs/*.json             # Exploration movement trails
└── backups/*.json                 # Manual and automated snapshot archives
```

For detailed component descriptions, see the [Runtime Directory Structure Guide](../README.md#runtime-directory-structure--file-breakdown-modschiyanmap). For line-by-line annotations and parameter definitions for `config.json`, see the [Configuration File Reference Guide (09-config-json.md)](./09-config-json.md).

---

[← Previous Chapter: 07 - Seed Map & Structure Predictions](./07-seedmap.md) | [Next Chapter: 09 - Configuration File Annotated Reference →](./09-config-json.md)
