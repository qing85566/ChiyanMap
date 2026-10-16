# Fullscreen World Map & Image Export Guide

**English** | [简体中文](../zh/02-worldmap.md) | [Back to Docs Index](../README.md)

Press the default hotkey **`M`** to toggle the fullscreen World Map. The World Map provides a macro exploration overview, cross-dimension browsing, precise coordinate jumping, and high-resolution image export tools.

---

## 1. Viewport Navigation & Controls

### 1.1 Mouse Controls
- **Panning**: Click and drag with the **Left Mouse Button** anywhere on the map. The panning engine incorporates inertial damping filters to ensure fluid, jitter-free camera movement.
- **Zooming**: Scroll the **Mouse Wheel** to zoom. The zoom spectrum ranges from close-up block details to multi-thousand-block macro overviews, with the zoom focus always anchored to the current world coordinate beneath your cursor.
- **Center on Player**: Click the "Center on Player" button in the bottom toolbar (or press the **Spacebar**) to smoothly center the viewport on your current physical coordinates.
- **Cursor Position Memory**: The mod remembers the exact position of your cursor upon closing. Reopening the map restores your previous focus point without unwanted viewport shifts.

### 1.2 Precise Coordinate Jumping (Goto Coordinates)
The top toolbar features a dedicated coordinate input box:
- Enter target **X** and **Z** coordinates, then click **Go** or press **Enter**.
- The camera will glide across the map at a high framerate to the target destination, pulsating with a circular highlight indicator—ideal for planning long-distance travel and monument scouting.

---

## 2. Cross-Dimension Browsing & Coordinate Conversion

The dimension selector in the top bar allows seamless switching between **Overworld**, **Nether**, and **The End**:

```text
[Current Dimension: Overworld]
   └── Switch to [Nether]
         ├── Automatically loads Nether terrain cache and lava seas
         ├── Real-time calculation of corresponding Overworld portal coordinates (X*8, Z*8)
         └── Upon closing: Instantly resets back to physical Overworld without desync
```

- **Interactive Coordinate Conversion**: While browsing the Nether, the lower-right HUD dynamically calculates and displays the equivalent Overworld portal coordinates (`X*8, Z*8`). Conversely, viewing the Overworld displays the required Nether coordinates (`X/8, Z/8`), greatly simplifying Nether highway and portal network planning.
- **Dimension Memory**: Enable "Remember Last Browsed Dimension" in settings to keep focus on a specific dimension during prolonged design sessions.

---

## 3. Entity Head Radar & Collapsible Waypoint Drawer

### 3.1 Entity Avatar Hotkey Toggle (Tab Hold)
In World Map mode, all nearby players, animals, and monsters are rendered using full-color miniature head avatars or player skins:
- **Temporary Visibility Hotkey**: Hold **`Tab`** to toggle the visibility of all entity avatars.
- **Input Focus Arbitration**: Even if the coordinate input box was previously focused, pressing the entity avatar hotkey automatically releases text focus, ensuring keys are never trapped as text.

### 3.2 Collapsible Waypoint Drawer
You can manage waypoints directly on the World Map without opening a separate modal window:
- Click the left edge to expand the collapsible waypoint drawer.
- **Click to Align Camera**: Click any waypoint in the list to glide the map camera directly over its coordinates. The drawer can be configured to auto-collapse after selection, keeping the screen clutter-free.

---

## 4. Selection Box High-Resolution PNG Export

Want to turn your explored world into a crisp, high-resolution desktop wallpaper or community map? ChiyanMap includes a built-in professional-grade image export tool.

### 4.1 Selection Workflow
1. Open the fullscreen World Map (**`M`**);
2. Hold **`Shift`** (customizable) and **Left-Click + Drag** across the map;
3. A translucent highlight box will appear, with a live coordinate dimension badge in the upper right (e.g., `Selection Width: 2048 blocks, Length: 2048 blocks`);
4. Release the mouse button to bring up the **PNG Export Confirmation Panel**.

### 4.2 Export Parameters & Stitching
- **Panoramic Tile Stitching Engine**: Features a background chunk-tile baking engine. Even when selecting dozens of explored regions, the system composites block geometry, biome coloring, elevation hillshading, and water depths into a single seamless image.
- **Destination Path**: Click **Export** to generate a lossless PNG image saved to `mods/ChiyanMap/export/` in your Minecraft directory.
- **Success Notification**: When the export finishes, a green on-screen Toast notification displays the full absolute path with a one-click copy button.

---

[← Previous Chapter: 01 - MiniMap & Radar System](./01-minimap.md) | [Next Chapter: 03 - Waypoints & Death Points System →](./03-waypoints-and-deathpoints.md)
