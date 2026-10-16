# Seed Map & Structure Predictions Guide

**English** | [简体中文](../zh/07-seedmap.md) | [Back to Docs Index](../README.md)

In the past, locating Strongholds, Ancient Cities, or Slime Chunks required alt-tabbing to external web tools (such as Chunkbase)—a cumbersome process that lacked live in-game positioning.

ChiyanMap embeds the native Bedrock world generation algorithm library directly into the client (powered by an optimized `cubiomes-bedrock` C engine), providing an **In-Game Native Seed Map**.

---

## 1. Seed Acquisition & Status

Click the **Seed Map** icon in the World Map toolbar to open the prediction panel:

- **Auto-Capture**: In singleplayer worlds, LAN games, or servers with seed broadcasting enabled, the mod automatically extracts the 64-bit level seed from memory when the world loads—no console commands required.
- **Manual Input**: When playing on servers that hide seeds from players, you can manually type a numeric or text seed obtained from the server owner to start full-map calculations.
- **Status Indicator**: Displays the active seed, validation status, and target generation baseline (aligned with Minecraft 1.26+ Bedrock world generation).

---

## 2. Structure Generation Predictions

The engine computes coordinates and bounding boxes for all major natural monuments deep within unexplored fog of war:

| Dimension | Structure Types | Icons & Markings |
| :--- | :--- | :--- |
| **Overworld Civilization** | Plains, Desert, Savanna, Taiga, Snowy Villages; Pillager Outposts; Igloos | Village building icons with variant badges |
| **Underground Exploration** | **Strongholds**, **Ancient Cities**, Abandoned Mineshafts | End portal icon / Sculk icon |
| **Major Monuments** | **Woodland Mansions**, **Ocean Monuments**, **Trial Chambers** | Monument silhouette icons |
| **Wilderness Ruins** | Desert Pyramids, Jungle Temples, Witch Huts, Buried Treasure, Trail Ruins | Pyramid / Chest icons |
| **Nether Dimension** | **Nether Fortresses**, **Bastion Remnants** | Blaze / Piglin snout icons |
| **The End Dimension** | **End Cities**, End Gateways | Shulker / Chorus tower icons |

### Navigation & Interaction
- **Category Filtering**: Multi-select filters allow you to isolate specific structures (e.g., displaying only *Ancient Cities* and *Strongholds*) to keep the map legible.
- **Hover Inspection**: Hovering your cursor over any structure icon displays its exact 3D coordinates, biome name, and straight-line distance from the player.

---

## 3. Slime Chunk Calculator

An essential tool for engineering automated slime farms:
- Calculates coordinates using Minecraft Bedrock's dedicated PRNG slime chunk formula.
- Toggle the **Slime Chunks** switch in the Seed Map panel.
- All eligible 16x16 chunk boundaries on both the MiniMap and World Map are overlaid with **translucent emerald green highlights**.
- Enables you to dig slime farm shafts with zero wasted blocks.

---

## 4. Unexplored Biomes Preview

- The Seed Map can compute landmass contours, ocean boundaries, and mountain elevations across tens of thousands of unvisited blocks using biome noise sampling.
- Whether searching for rare Mushroom Fields, Deep Dark caverns, or Pale Gardens, you can simply pan and zoom the World Map to scout ahead without mindless running.

---

[← Previous Chapter: 06 - World Map Freehand Drawing & Annotations](./06-drawing-annotations.md) | [Next Chapter: 08 - Hotkeys, Settings & Performance Tuning →](./08-settings-and-hotkeys.md)
