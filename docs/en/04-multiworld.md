# Multiworld & Map Instance Management Guide

**English** | [简体中文](../zh/04-multiworld.md) | [Back to Docs Index](../README.md)

On large multiplayer networks, proxy clusters (BungeeCord / Velocity), minigame lobbies, and cross-server networks, players frequently suffer from **map data collision and cross-contamination**—such as jumping from a lobby to a skyblock server and having completely different worlds stitched together on the same map, corrupting caches.

**ChiyanMap's Multiworld Manager** resolves this at the architecture level by assigning completely independent **Map Instances** to each unique world and sub-server.

---

## 1. Why Multiworld Management?

- **Physical Isolation**: Each map instance maintains its own cache directory, waypoint database, vector drawing annotations, and dimensional elevation tiles.
- **Zero Configuration**: Changing servers requires no manual clearing of local files; the mod partitions data automatically.
- **Connected Roaming**: Allows players to define "Connected" relationships between sub-servers, enabling shared waypoints while preventing dangerous teleports to unlinked game modes.

---

## 2. Map Selection Modes

You can select from three matching modes in the settings panel or the Multiworld window:

### 2.1 WorldSpawn Auto-Detection Mode (Recommended)
- The system continuously reads the world's physical spawn coordinates (`spawnX`, `spawnZ`).
- Whenever you switch servers or minigames and the spawn changes, the system instantly queries for an existing instance:
  - If a matching instance exists: **Silently and seamlessly switches** to that map.
  - If visiting a new world for the first time: **Automatically creates a new map instance** named `Spawn (100, 200)`, eliminating data collisions.

### 2.2 Manual Mode
- Map selection is fully controlled by the player via the management panel.
- Ideal for advanced builders who want parallel layout planning instances on the same world.

### 2.3 Single Map Mode
- Classic minimalist mode where all explored regions are merged into the `default` instance.

---

## 3. Map Instance Lifecycle Management

Click the Multiworld icon in the World Map toolbar to open the instance control panel:

### 3.1 Creation & Renaming
- Click **New Map**, enter a custom name (e.g., *Survival SMP*, *Redstone Testing*), and the system allocates a dedicated UUID and folder.
- Rename any existing map at any time; changes take effect immediately and persist automatically.

### 3.2 Cascading Cleanup & Safe Deletion
To safeguard player assets, ChiyanMap incorporates strict deletion safety rules:
- **Core Protection**: Deleting the currently active map or the default system map is strictly prohibited.
- **Automatic Hot Backup**: Deleting a map automatically compresses and archives its data into `cache/<worldId>/maps_backup/`, allowing full recovery if deleted by mistake.
- **Deep Cascade Cleanup**: Prevents disk clutter by automatically purging all associated binary chunk files (`region_*.bin`), cave data (`cave/`), independent waypoints, folders, and vector annotations.

---

## 4. Map Connections & Teleport Warning

### 4.1 Connected Maps Roaming
In the Multiworld panel, you can establish two-way connections between maps:
- For example: Linking *Survival Overworld* and *Resource World* allows waypoints from both to be viewed in a shared navigation list.
- A map is always connected to itself.

### 4.2 Void & Suffocation Teleport Interception
When clicking a waypoint on the World Map to run a `/tp` command:
- The system checks whether your current map and the destination map are connected.
- **Unconnected Block Warning**: If attempting to teleport to an unconnected instance, a yellow confirmation modal warns you (*"The target waypoint belongs to an unlinked map instance. Blindly teleporting may cause you to fall into the void or suffocate in solid blocks"*). Explicit confirmation is required, greatly improving hardcore survival safety.

---

## 5. Storage Hierarchy Reference

Data is cleanly structured under the client's root directory:

```text
mods/ChiyanMap/
├── cache/
│   └── <World_UUID>/
│       ├── multiworld.json          # Multiworld configuration and connection matrix
│       ├── dim_0/                   # Default map Overworld binary chunks
│       ├── dim_1/                   # Default map Nether chunks
│       ├── dim_2/                   # Default map The End chunks
│       └── maps/
│           └── <Map_Instance_ID>/   # Custom map instance directory
│               ├── dim_0/           # Instance Overworld chunks
│               └── dim_0/cave/      # Instance cave data
└── waypoints/
    ├── <World_UUID>_dim0.json       # Default map waypoints
    └── <World_UUID>_<Map_ID>_*.json # Custom instance waypoints and sets
```

---

[← Previous Chapter: 03 - Waypoints & Death Points System](./03-waypoints-and-deathpoints.md) | [Next Chapter: 05 - Cave Map & Anarchy Detection →](./05-cave-and-anarchy.md)
