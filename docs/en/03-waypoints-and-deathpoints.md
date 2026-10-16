# Waypoints & Death Points System Guide

**English** | [简体中文](../zh/03-waypoints-and-deathpoints.md) | [Back to Docs Index](../README.md)

Waypoints and Death Points form the navigational backbone of survival and exploration. ChiyanMap delivers a structured Waypoint Sets architecture, authentic in-world 3D beacon beams, and robust deletion undo mechanisms.

---

## 1. Waypoint Manager

Press the default hotkey **`U`** to open the standalone Waypoint Manager dialog:

### 1.1 Creation & Editing
- **Create Waypoint**: Click the "New Waypoint" button. The dialog automatically pre-fills your current 3D coordinates and dimension; simply enter a name and pick a theme color.
- **Color Palette**: Offers a rich preset palette (Red, Orange, Yellow, Green, Cyan, Blue, Purple, Pink, White) with hex code and RGB fine-tuning.
- **Visibility Toggles**: Every waypoint features an individual visibility eye icon. Toggling visibility **saves immediately** to disk without requiring a manual save button.
- **Accidental Deletion Recovery (Ctrl+Z)**: If you accidentally delete a critical base or mine coordinate, press **`Ctrl + Z`** in the Waypoint Manager or World Map. The system will restore the deleted waypoint and all its attributes from an undo stack supporting up to 64 entries.

---

## 2. Waypoint Sets & Category Folders

To prevent visual clutter when managing hundreds of waypoints, ChiyanMap provides a dual-tiered organization system: **Sets** and **Folders**.

### 2.1 Waypoint Sets
Sets represent top-level isolated workspaces (e.g., *Main Base*, *Nether Transport*, *Mining Outposts*, *Exploration Ruins*):
- **Hotkey Cycling**: Press **`[`** and **`]`** (default) to cycle backward and forward between waypoint sets.
- **Smooth Toast Notifications**: Whenever you switch sets, an elegant on-screen Toast notification displays the active set name, allowing uninterrupted gameplay without opening menus.
- **Focused Filtering**: Toggle "Only Show Current Set" in settings to display only markers relevant to your immediate task.

### 2.2 Category Folders
Within each set, waypoints can be organized into sub-folders (e.g., *Iron Farms*, *Slime Farms*, *Pillager Outposts*):
- Folders can be collapsed or expanded in the management drawer.
- Batch toggle visibility or migrate entire folders between sets with one click.

---

## 3. In-Game 3D Spatial Indicators & Native Beacon Beams

ChiyanMap extends beyond 2D minimaps by projecting waypoints directly into the 3D Minecraft world.

```text
[Distant Sky]
   │
   │  ── Native 3D Rotating Beacon Beam (matches waypoint color)
   │
   ├── [Main Base (238m)] ── 3D Floating Nameplate (adaptive scale by distance)
   │
[Terrain Surface]
```

### 3.1 Native Beacon Beam Pipeline
Unlike traditional mods that draw simplistic polygon lines, ChiyanMap hooks directly into Minecraft's native block entity rendering pipeline (`LevelRendererCamera::$renderBlockEntities`):
- **Authentic Vanilla Beams**: Every active waypoint shoots an authentic vanilla beacon beam straight into the sky.
- **Rotation & Flow Dynamics**: Beams feature authentic vanilla energy flow textures and continuous axial rotation matching your waypoint's exact RGB color.
- **High Efficiency**: Rendered by the GPU vertex shader pipeline, consuming zero CPU cycles.

### 3.2 3D Spatial Distance Nameplates
- Floating billboard tags in your field of view indicate the waypoint name and linear distance (e.g., `Stronghold [1450m]`).
- Labels smoothly shrink at extreme distances to avoid blocking vision and scale up when approaching, with configurable line-of-sight occlusion testing.
- The **Maximum 3D Marker Render Distance** can be customized in settings from 64 blocks up to several thousand blocks.

---

## 4. Death Points Tracking System

Unplanned deaths during dangerous adventures can result in agonizing item loss. ChiyanMap ensures your dropped inventory is always recoverable.

### 4.1 Automatic Capture & Precision Recording
- The moment your health drops to zero, client hooks capture your exact 3D coordinates (accurate to 0.1 blocks), timestamp, and dimension.
- Automatically generates a death card named `Death YYYY-MM-DD HH:MM:SS` with a prominent crimson skull badge on both the MiniMap and World Map.

### 4.2 One-Click Conversion to Waypoint
Each death point entry in the Death Point Manager includes a **Convert to Waypoint** button:
- Converts the death coordinate into a permanent regular waypoint with a beacon beam shining over your drop site.
- **Cross-Session Persistence**: Linked internally via `convertedWaypointId`. If you later delete that converted waypoint from the Waypoint Manager, the "Convert to Waypoint" button re-enables; if you undo the deletion, it seamlessly re-locks.

### 4.3 Smart Auto-Removal & Safe Respawn Buffer
- **Proximity Cleanup**: When enabled, moving within 6 blocks of the death location automatically marks the retrieval as complete and dismisses the death marker.
- **5-Second Respawn Buffer Guard**: Includes an automated 5-second delay upon respawning, preventing premature deletion if your bed or respawn anchor is right next to your death point.

---

[← Previous Chapter: 02 - Fullscreen World Map & Export](./02-worldmap.md) | [Next Chapter: 04 - Multiworld & Map Instance Management →](./04-multiworld.md)
