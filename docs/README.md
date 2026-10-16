# ChiyanMap Official Feature Guides & Documentation

**English** | [简体中文](./README_zh.md)

Welcome to the comprehensive user manual and configuration guide for **ChiyanMap**. This documentation suite is designed to help players, server administrators, and modpack creators gain a thorough understanding of every feature, hotkey interaction, internal mechanism, and performance tuning option provided by the mod.

---

## 📚 Table of Contents

| Chapter | Guide Document | Core Overview |
| :--- | :--- | :--- |
| **01** | [MiniMap & Advanced Radar System](./en/01-minimap.md) | Shape switching, smooth rotation, multi-layered entity radar, border clamping, wildcard filtering, potion pushbox avoidance, and HUD telemetry |
| **02** | [Fullscreen World Map & Image Export](./en/02-worldmap.md) | Fluid panning & zooming, precise coordinate jumping, cross-dimension browsing, selection box PNG export, entity head toggles, and waypoint drawer |
| **03** | [Waypoints & Death Points System](./en/03-waypoints-and-deathpoints.md) | Waypoint Sets management, custom category folders, native 3D beacon beams, automatic death tracking/conversion, and Ctrl+Z undo recovery |
| **04** | [Multiworld & Map Instance Management](./en/04-multiworld.md) | Physical isolation for network/sub-servers, spawn point auto-detection, instance creation/deletion, cascading cleanup, and connected roaming |
| **05** | [Cave Map & Anarchy Detection](./en/05-cave-and-anarchy.md) | Vertical depth-sliced X-ray mode, underground structure identification, player modification history tracking, and sensitivity tuning |
| **06** | [World Map Freehand Drawing & Annotations](./en/06-drawing-annotations.md) | Freehand pens, polylines, arrows, geometric shapes, text labels, multi-layer management, and full undo (`Ctrl+Z`) / redo (`Ctrl+Y`) |
| **07** | [Seed Map & Structure Predictions](./en/07-seedmap.md) | World seed auto-capture, full structure predictions (Villages, Strongholds, Ancient Cities, etc.), slime chunk overlays, and biome preview |
| **08** | [Hotkeys, Settings & Performance Tuning](./en/08-settings-and-hotkeys.md) | Single-key/combo rebinding, real-time search filters, factory reset, SQLite WAL storage engine, and multi-threaded write batching |

---

## ⚡ Default Hotkeys Overview

| Key | Action | Context |
| :--- | :--- | :--- |
| **`M`** | Toggle Fullscreen World Map | In-game anytime |
| **`U`** | Open / Close Waypoint Manager | In-game anytime |
| **`N`** | Toggle MiniMap Visibility | In-game anytime |
| **`Y`** | Toggle MiniMap Shape (Circle ⟷ Square) | In-game anytime |
| **`J`** | Toggle MiniMap Rotation (Player Rotation ⟷ Locked to North) | In-game anytime |
| **`Z`** | Temporarily Enlarge MiniMap | In-game anytime |
| **`[` / `]`** | Switch to Previous / Next Waypoint Set (Toast notification) | In-game anytime |
| **`Tab` (Hold)** | Temporarily Show / Hide Entity Heads on World Map | World Map active |
| **`Shift` + Drag** | Select Box to Export High-Resolution PNG | World Map active |
| **`Ctrl + Z`** | Undo Last Action (Drawing stroke / Hotkey rebind / Deleted waypoint recovery) | World Map / Waypoint Manager / Settings |
| **`Ctrl + Y`** | Redo Last Undone Action (Drawing mode) | Drawing mode active |
| **`Esc`** | Close Current Map Interface / Cancel Key Rebinding | Mod UI active |

> 💡 **Tip**: All hotkeys can be customized or bound to multi-key combinations (with `Ctrl`, `Shift`, `Alt` modifiers) by pressing **`M`** to open the World Map and clicking the **⚙️ (Settings)** icon in the top toolbar.

---

## 📂 Runtime Directory Structure & File Breakdown (`mods/ChiyanMap`)

When running inside the client, ChiyanMap maintains a modular, multiworld-isolated data hierarchy located under `mods/ChiyanMap/`. Below is the complete directory structure and breakdown:

```text
mods/ChiyanMap/
├── ChiyanMap.dll                  # Core binary mod dynamic link library (LeviLamina entry point)
├── manifest.json                  # LeviLamina mod manifest and version metadata
├── tooth.json                     # Tooth package manager metadata and dependencies
├── config.json                    # Global configuration (MiniMap/World Map parameters, hotkeys, radar rules)
│
├── lang/                          # Built-in localization dictionaries for 16 languages
│   ├── zh_CN.json                 # Simplified Chinese
│   ├── zh_TW.json                 # Traditional Chinese
│   ├── en_US.json                 # English (US)
│   ├── ja.json                    # Japanese
│   ├── ko.json                    # Korean
│   ├── ru.json                    # Russian
│   ├── de.json                    # German
│   ├── fr.json                    # French
│   ├── es.json                    # Spanish
│   ├── pt_BR.json                 # Portuguese (Brazil)
│   ├── it.json                    # Italian
│   ├── id.json                    # Indonesian
│   ├── th.json                    # Thai
│   ├── tr.json                    # Turkish
│   ├── uk.json                    # Ukrainian
│   └── vi.json                    # Vietnamese
│
├── heads/                         # Pixel head textures for entity radar (80+ PNG icons)
│   ├── CowFace.png                # Cow miniature face icon
│   ├── PigFace.png                # Pig miniature face icon
│   ├── CreeperFace.png            # Creeper miniature face icon
│   ├── ZombieFace.png             # Zombie miniature face icon
│   └── ...                        # Other mobs, villagers, illagers, and bosses
│
├── data/                          # Core terrain, elevation, and cache storage
│   ├── <World_ID>_database.db     # SQLite WAL database (used when SQLite mode is enabled)
│   ├── <World_ID>_database.db-wal # SQLite Write-Ahead Log (high-concurrency write buffer)
│   ├── <World_ID>_database.db-shm # SQLite shared memory index
│   │
│   ├── cache/                     # Binary map tile cache directory
│   │   └── <World_ID>/            # Data isolated strictly by World UUID
│   │       ├── multiworld.json    # Map instances and interconnected roaming manifest
│   │       ├── dim_0/             # Overworld region tiles
│   │       │   ├── region_X_Z.bin # 512x512 block compressed binary terrain data
│   │       │   └── cave/          # Underground vertical cave slice data
│   │       │       └── region_X_Z.bin
│   │       ├── dim_1/             # Nether region tiles
│   │       ├── dim_2/             # The End region tiles
│   │       └── maps/              # Custom multiworld/sub-server instance tiles
│   │           └── <Map_ID>/      # Isolated instance (e.g. spawn_0_0)
│   │               ├── dim_0/
│   │               ├── dim_1/
│   │               └── dim_2/
│   │
│   ├── export/                    # High-resolution PNG exported map images
│   │   └── map_export_*.png       # Stitched panoramic map exports
│   └── maps_backup/               # Automated backup archives generated upon map deletion
│
├── waypoints/                     # Waypoint storage directory
│   ├── <World_ID>_dim0.json       # Overworld default waypoint list
│   ├── <World_ID>_dim1.json       # Nether waypoint list
│   ├── <World_ID>_dim2.json       # The End waypoint list
│   ├── <World_ID>_folders.json    # Custom waypoint category folder configuration
│   ├── <World_ID>_sets.json       # Waypoint Sets manifest
│   └── <World_ID>_<Map_ID>_*.json # Isolated waypoints and sets for custom map instances
│
├── deaths/                        # Player death tracking directory
│   └── <World_ID>.json            # Historical death records and linked waypoint associations
│
├── drawings/                      # World Map vector drawings and annotations directory
│   ├── <World_ID>_dim0.json       # Overworld vector annotations and layer data
│   ├── <World_ID>_dim1.json       # Nether vector annotations
│   └── <World_ID>_<Map_ID>_*.json # Custom map instance annotations
│
├── anarchy/                       # Anarchy chunk modification history directory
│   └── <World_ID>_anarchy.json    # Recorded player modification heatpoints
│
├── breadcrumbs/                   # Historical player exploration movement trails
│   └── <World_ID>_breadcrumbs.json# Player movement breadcrumbs
│
└── backups/                       # Data backup and snapshot archive directory
    └── waypoints_<World>_*.json   # Manual or scheduled waypoint backup exports
```

### Detailed Component Descriptions

1. **Core Binaries & Configuration**
   - `ChiyanMap.dll`: The compiled C++23 binary module responsible for MinHook injection, DirectX 11 rendering hooks, ImGui HUD rendering, and core mapping algorithms.
   - `config.json`: Global persistent settings containing positions, scale, opacity, HUD telemetry toggles, hotkey mappings, and radar filtering rules.
   - `manifest.json` / `tooth.json`: Standard LeviLamina plugin metadata defining UUID, dependencies, and target installation directories.

2. **Localization & Atlas Assets (`lang/`, `heads/`)**
   - `lang/`: Native internationalization files covering 16 major languages. Hot-reloads in real-time when the user switches languages.
   - `heads/`: 80+ high-resolution pixel head textures (16x16 / 32x32) for vanilla mobs, villagers, illagers, and bosses used on the radar and map.

3. **Map & Cache Storage (`data/`)**
   - `data/cache/<World_ID>/`: Binary chunk tiles physically partitioned by World UUID. Each `region_<rx>_<rz>.bin` file caches a 512x512 block region of colors, heights, water, and biomes with self-healing verification.
   - `data/cache/<World_ID>/dim_0/cave/`: Vertical depth-sliced underground cave data.
   - `data/<World_ID>_database.db`: High-performance SQLite3 ACID database with Write-Ahead Logging (WAL) and shared memory cache for zero-drop rendering on high-speed elytra flight.
   - `data/export/`: Output folder for lossless high-resolution PNG exports generated via Shift-drag box selection on the World Map.

4. **Navigation & Markers (`waypoints/`, `deaths/`, `drawings/`)**
   - `waypoints/`: Per-dimension JSON arrays storing coordinates, names, RGB colors, waypoint sets, and category folders.
   - `deaths/`: Records 3D coordinates, timestamps, dimensions, and linked waypoint IDs for death points, supporting undo restoration.
   - `drawings/`: Vector annotations storing pen strokes, polylines, arrows, shapes, text labels, and layer opacities.

5. **Exploration Utilities (`anarchy/`, `breadcrumbs/`, `backups/`)**
   - `anarchy/`: Tracks player modification hotspots, man-made blocks, and altered terrain for anarchy servers.
   - `breadcrumbs/`: Records exploration movement trails for easy back-tracking.
   - `backups/`: Automated or manual snapshot backups ensuring data safety across updates.
