# Cave Map & Anarchy Detection Guide

**English** | [简体中文](../zh/05-cave-and-anarchy.md) | [Back to Docs Index](../README.md)

Underground exploration and relic hunting on Anarchy servers represent two of the most demanding challenges in Minecraft. ChiyanMap features an integrated **Vertical Cave Slicing Engine** alongside an **Anarchy Modification History Detector**.

---

## 1. Cave Map Mode

Standard surface maps only display the uppermost solid blocks touched by sunlight. In underground mineshafts, Ancient Cities, or Nether fortresses, the top layer is solid rock, causing conventional maps to show nothing but a flat expanse of gray stone.

**Cave Mode** overcomes this limitation by evaluating a vertical slice centered on the player's current vertical elevation (Y-axis).

```text
[Surface Rock Ceiling] (Intelligently ignored)
   │
   ▼
[Air Corridor] ── Upper ceiling search
   │
   ├── Player Elevation (Y)
   │
   ▼
[Solid Floor] ── Walkway highlight (rendered as navigable corridors)
   │
[Lava Seas / Void Dropping] ── Rendered with dark shadows & cliff warnings
```

### 1.1 Core Mechanics
- **Dynamic Slice Scanning**: Centers on the player's current elevation, scanning upward for the nearest air ceiling and downward for solid floor blocks.
- **Relief Shading & Depth**: Discovered tunnels are rendered with 3D relief shadows. Corridors sloping upward or chasms dropping downward are immediately identifiable at a glance.
- **Dedicated Cave Cache**: Explored cave data is stored separately in the dimension's `cave/` directory. Enabling Cave Mode on the World Map lets you review entire mine complexes and subterranean strongholds like architectural schematics.

### 1.2 Automatic & Manual Modes
- **Automatic Transition**: When descending deep underground in the Overworld (Y < 55) or when completely occluded by overhead stone, the MiniMap seamlessly transitions into Cave Mode.
- **Manual Toggle**: Use the top toolbar icon or hotkey to force Cave Mode on/off from the surface, allowing you to scout underground pockets and Lush Caves before digging.

---

## 2. Anarchy History Analyzer

On Anarchy servers (such as 2b2t) and competitive SMPs, **tracking historical player movements, uncovering abandoned bases, finding buried stashes, and spotting traps** are critical survival skills.

ChiyanMap's **Anarchy History Analyzer** penetrates vanilla terrain to detect unnatural block configurations and historical modifications.

### 2.1 Trace Detection Dimensions
The analyzer performs pattern recognition across loaded chunks to flag:
1. **Civilization Artifacts**: Obsidian, Crying Obsidian, Ender Chests, regular Chests, Shulker Boxes, Furnaces, Crafting Tables, Bookshelves, and Signs.
2. **Modified Terrain**: Level obsidian highways, straight mining bore tunnels, and artificial bedrock holes.
3. **Explosions & Wither Scars**: Radial craters left behind by bed-bombs or End Crystal explosions.
4. **Subterranean Lighting**: Torches, Glowstone, Redstone Torches, and Sea Lanterns placed in non-standard patterns.

### 2.2 Visual Highlight Rendering
- **Chunk Heatmap Grids**: Chunks showing human activity are highlighted with translucent orange or red warning grids on both the MiniMap and World Map.
- **Persistent Evidence**: Detected modification spots are saved to disk (`AnarchyManager::SaveHighlights()`). Even after leaving the area, previously scouted player trails remain visible on the World Map, allowing you to map out enemy transit routes over time.

### 2.3 Sensitivity Configuration
Navigate to **Anarchy Settings** in the settings dialog:
- **Sensitivity Threshold**: Adjust the minimum number of man-made blocks required to trigger a highlight, filtering out random natural generation anomalies.
- **Scan Radius**: Configure how many chunks per second are scanned based on your machine's CPU budget.
- **Clear Highlights**: Quickly wipe historical markers to begin a fresh scouting run.

---

[← Previous Chapter: 04 - Multiworld & Map Instance Management](./04-multiworld.md) | [Next Chapter: 06 - World Map Freehand Drawing & Annotations →](./06-drawing-annotations.md)
