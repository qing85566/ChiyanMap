# World Map Freehand Drawing & Annotations Guide

**English** | [简体中文](../zh/06-drawing-annotations.md) | [Back to Docs Index](../README.md)

To support clan war planning, faction boundary zoning, redstone routing schematics, and exploration notes, ChiyanMap embeds a professional **Vector Drawing & Annotation System** directly into the fullscreen World Map.

Annotations are anchored directly to **Minecraft world physical coordinates**. Whether zooming in for block-level inspections or zooming out to a continental overview, drawings and labels remain anchored to their exact terrain blocks.

---

## 1. Activating Drawing Mode

1. Press **`M`** to open the fullscreen World Map;
2. Click the **Pencil Icon (✏️)** in the top toolbar to enter drawing mode;
3. The vector toolkit and the floating layer manager panel will appear.

---

## 2. Drawing Toolkit Overview

| Tool Name | Tool Identifier | Usage & Best Applications |
| :--- | :--- | :--- |
| **Freehand Pen** | `Pen` | Click and drag to draw smooth continuous curves; ideal for winding trails, river routes, or organic sketches. |
| **Straight Line** | `Line` | Click start point and drag to end point; draws crisp linear vectors for railway tracks and Nether ice highways. |
| **Tactical Arrow** | `Arrow` | Drags to create directional arrows; perfect for coordinating team troop movements and evacuation routes. |
| **Rectangle** | `Rect` | Drags diagonally to form a rectangle with outline or solid fill; commonly used for clan borders and building plots. |
| **Radius Circle** | `Circle` | Drags outward from the center point; useful for village raid boundaries, beacon coverage, and mob spawning rings. |
| **Text Label** | `Text` | Click anywhere on the map to place a text tag with customizable font sizing and persistent world anchoring. |
| **Eraser** | `Eraser` | Features pixel-tolerance collision detection; click any element to instantly erase it with full undo support. |

### Style Properties
On the right side of the toolkit, you can adjust active drawing properties:
- **Pen Color (RGBA)**: Hex color picker with transparency alpha slider.
- **Line Thickness**: Adjustable from `1px` hair-lines to bold `16px` vectors.
- **Fill Toggle & Fill Color**: When enabled, rectangles and circles render an independent translucent interior fill for professional map zone styling.

---

## 3. Multi-Layer Management System

To keep complex projects organized, ChiyanMap incorporates a Photoshop-style multi-layer architecture:

```text
[Layer Manager Panel]
   ├── 📂 Layer 1: [Territory Claims] ── 👁️ Visible | 🔒 Locked | Theme: Blue
   ├── 📂 Layer 2: [Redstone & Rail Hub] ── 👁️ Visible | 🔓 Editable | Theme: Orange
   └── 📂 Layer 3: [Tactical Infiltration] ── 🙈 Hidden | 🔓 Editable | Theme: Red
```

- **Add & Rename Layers**: Create unlimited layers with custom names (e.g., *Bunker Layout*, *Mining Tunnels*).
- **Visibility Toggle (Eye Icon)**: Batch show or hide all annotations belonging to a layer.
- **Layer Lock (Lock Icon)**: Protect completed layouts from accidental edits or erasures while drawing on other layers.
- **Layer Opacity**: Independent opacity slider per layer (`0.05` to `1.0`).
- **Clear Layer**: Quickly wipe all annotations on the active layer; automatically logged into the undo history.

---

## 4. Full Undo & Redo Support

Mistakes happen during drafting. The drawing system features full undo and redo capabilities matching desktop vector software:

- **Undo**: Press **`Ctrl + Z`** at any point in drawing mode to instantly revert the last stroke, shape addition, erasure, or layer clear (up to 60 steps).
- **Redo**: Press **`Ctrl + Y`** (or **`Ctrl + Shift + Z`**) to reapply undone actions.

---

## 5. Storage & Persistence

All vector drawings are serialized as lightweight, human-readable JSON files:

- **File Path**: `mods/ChiyanMap/drawings/<World_ID>_<Map_ID>_dim<Dimension_ID>.json`
- **Data Integrity**: Atomic file writing executes on world exit, dimension shifts, and map instance switches to protect against crashes or sudden power loss.

---

[← Previous Chapter: 05 - Cave Map & Anarchy Detection](./05-cave-and-anarchy.md) | [Next Chapter: 07 - Seed Map & Structure Predictions →](./07-seedmap.md)
