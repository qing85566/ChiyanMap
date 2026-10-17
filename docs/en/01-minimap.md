# MiniMap & Advanced Radar System Guide

**English** | [简体中文](../zh/01-minimap.md) | [Back to Docs Index](../README.md)

The MiniMap is ChiyanMap's primary on-screen HUD interface, delivering high-framerate rendering, rich telemetry information, and an advanced layered entity radar.

---

## 1. Appearance & Interaction Controls

### 1.1 Shape & Orientation Modes
- **Shape Toggle**: Press **`Y`** (default) to instantly switch between **Circular** and **Square** modes.
  - *Circular Mode*: Smooth, symmetrical field of view with an elegant surrounding compass ring.
  - *Square Mode*: Offers ~27% more visible area in the corners, making it easier to monitor distant diagonal landmarks.
- **Orientation Modes**: Press **`J`** (default) to toggle how the map rotates:
  - *Follow Player Rotation*: The MiniMap always points in the direction the player is looking ("Up"), rotating smoothly in real-time.
  - *Locked to North*: The top of the map is permanently anchored to true North (-Z axis), while the player's arrow icon rotates freely inside.

### 1.2 Free Dragging & Fine Sizing
Press **`M`** to open the fullscreen World Map and click the gear icon to enter **MiniMap Settings**:
- **Position Dragging**: Directly click and drag the MiniMap window to any corner or position on your screen (Top-Left, Top-Right, Bottom-Center, etc.).
- **Interface Scale**: Stepless slider scaling from `0.5x` up to `2.5x`, providing crisp visuals across 1080p, 1440p (2K), and 4K displays.
- **View Radius**: Configure the scanned block radius (from 16 to 128 blocks) to match your personal balance between visibility and hardware performance.
- **Visibility Toggle**: Press **`N`** to instantly hide or show the MiniMap with zero impact on core game rendering.

### 1.3 Enlarge MiniMap Mode
Press the default hotkey **`X`** to activate the temporary zoom view:
- The MiniMap instantly expands and re-centers on the screen with more than 2x the standard viewing radius.
- Releasing the key (or toggling it again) immediately returns the map to its corner layout—ideal for rapid tactical orientation in caves or intense combat.

---

## 2. Smart Avoidance & Information HUD

### 2.1 Native Potion Effect Pushbox Avoidance
When the MiniMap is placed in the top-right corner, vanilla Minecraft potion status effects can collide with the map frame:
- ChiyanMap features an integrated **HUD Avoidance System** that actively detects the number of active potion status effects.
- When active effects are detected, the MiniMap smoothly shifts downward to maintain safe clearance from the potion tray, springing back automatically when the effects expire.

### 2.2 Dashboard Telemetry Items
You can configure high-value survival indicators around the MiniMap perimeter:
- **3D Coordinates & Dimension Conversion**: Displays physical X, Y, Z coordinates. When in the Overworld, it calculates the equivalent Nether portal coordinates (1:8 ratio); in the Nether, it shows the corresponding Overworld exit coordinates.
- **Biome Detection**: Reads the current biome underfoot, strips namespace clutter, and formats it cleanly in your localized language (e.g., *Cherry Grove*, *Pale Garden*, *Lush Caves*).
- **Ambient Light Sensor**: Displays separate `Sky / Block` light levels to easily verify mob-spawning conditions and ensure safe lighting coverage.
- **Dynamic Clock & Day Counter**: Reads world game ticks and converts them into 24-hour time (e.g., `12:30`) and total elapsed survival days.
- **Weather Forecast**: Evaluates local temperature and precipitation flags in real-time, displaying clear, rain, or blizzard icons.

---

## 3. Advanced Entity Radar System

Beyond terrain mapping, the MiniMap leverages client engine hooks to track nearby entities and construct a multi-layered radar.

```text
[Player Center]
   ├── Passive Mobs (Cows, Sheep, Pigs, etc.) ── Green dot / head icon
   ├── Hostile Monsters (Zombies, Skeletons, Creepers) ── Bright red dot / head icon
   ├── Neutral Mobs (Endermen, Iron Golems) ── Yellow dot
   ├── Other Players ── Dedicated highlight ring + skin avatar
   └── Tamed Pets ── Double neon-green aura + heart indicator
```

### 3.1 Depth & Height Fade
The radar rejects flat 2D projection by introducing vertical (Y-axis) distance perception:
- The **Height Threshold** can be tuned from 1.0 to 30.0 blocks in the settings panel.
- Entities on the same elevation appear at 100% opacity.
- Entities high up on cliffs or deep below in caves linearly fade and darken, accompanied by subtle directional elevation arrows (▲ / ▼) for rapid vertical differentiation.

### 3.2 Border Clamping
When surrounding entities move outside the MiniMap's visible frame while remaining within the radar's maximum detection range:
- Offers **5 distinct edge-clamping modes**.
- Entity dots smoothly slide along the inner perimeter of the map border with outward-pointing directional arrows.
- Sneaking hostile players or distant teammates are never lost from sight.

### 3.3 Wildcard Advanced Filtering
Through the radar filter bar in settings, you can configure entity whitelists and blacklists:
- **Include & Exclude Rules**: Enter entity identifiers (e.g., `zombie`, `skeleton`, `creeper`).
- **Wildcard Syntax**: Supports `*` (matches zero or more characters) and `?` (matches a single character). For instance, `*skeleton*` simultaneously matches regular Skeletons, Strays, and Wither Skeletons.
- **Comma-Separated Lists**: Specify multiple identifiers in a single line (e.g., `cow, pig, sheep, chicken`).

### 3.4 Tamed Pet Auras
By querying the engine's native `isTame` property:
- Tamed wolves, cats, horses, and parrots receive a **double neon-green glowing aura** on the radar.
- A floating pink heart icon appears above their nameplates, preventing friendly fire during chaotic team fights.

---

[Next Chapter: 02 - Fullscreen World Map & Export →](./02-worldmap.md)
