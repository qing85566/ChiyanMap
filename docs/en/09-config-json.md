# Configuration File Annotated Reference Guide (config.json)

**English** | [简体中文](../zh/09-config-json.md) | [Back to Docs Index](../README.md)

`mods/ChiyanMap/config.json` serves as the global persistent configuration file for the mod. Whenever the game launches, settings are modified via the UI, or options are toggled using hotkeys, the system automatically synchronizes state to and from this file.

This document provides a **complete parameter structure, an annotated JSONC sample, data types, valid ranges, and detailed descriptions for every key**, allowing players and modpack creators to customize defaults, troubleshoot issues, and fine-tune performance.

---

## 1. Annotated JSON Template (JSONC)

> 💡 **Notice**: The real `config.json` file uses standard JSON format (which does not support `//` comments). When editing manually with a text editor, remove all comments and trailing commas, or use the tables below for reference.

```jsonc
{
    // ==========================================
    // 1. Language & Global UI
    // ==========================================
    "language": "en_US",                // Active localization language code (en_US, zh_CN, zh_TW, ja, ko, ru, de, fr, es, pt_BR, it, id, th, tr, uk, vi)
    "globalUIScale": 1.0,               // Global UI scaling factor (float, 0.5 ~ 2.5)
    "uiTextScale": 1.0,                 // Text font scaling baseline
    "openingAnimation": true,           // Play smooth opening fade/expansion animation when toggling the World Map

    // ==========================================
    // 2. MiniMap Display & Layout
    // ==========================================
    "showMiniMap": true,                // Master toggle for MiniMap visibility (Hotkey: N)
    "isSquareMap": false,               // MiniMap geometry (false: Circular, true: Square, Hotkey: Y)
    "rotateMiniMap": true,              // MiniMap rotation mode (true: Follow player heading, false: Locked to North, Hotkey: J)
    "miniMapScale": 1.0,                // MiniMap overall window scale factor (0.5 ~ 2.5)
    "miniMapZoomRadius": 64,            // MiniMap scanned visible block radius (16 ~ 128 blocks)
    "minimapDoubleMinZoom": false,      // Double the minimum zoom level for expanded coverage
    "transparentMinimapBackground": false, // Use transparent background for unexplored fog-of-war areas
    "miniMapOffsetX": 16,               // Horizontal pixel offset relative to screen anchor
    "miniMapOffsetY": 16,               // Vertical pixel offset relative to screen anchor
    "miniMapFrameStyle": 0,             // Border frame style (0: Metallic texture frame, 1: Minimalist flat frame, 2: Frameless)
    "miniMapFrameColor": [0.0, 0.0, 0.0, 0.8], // Custom frame RGBA color array (0.0 ~ 1.0 per channel)
    "minimapFpsLimit": 0,               // MiniMap independent framerate limit (0: Uncapped/vsync, or set to 30/60 to conserve power)
    "enlargeMinimapToggle": false,      // Configure Hotkey X enlarge mode to toggle on/off instead of hold-to-zoom
    "keepEnlargedMinimapRotation": false, // Force MiniMap rotation to persist when enlarged

    // ==========================================
    // 3. Compass & Player Indicator
    // ==========================================
    "showCompass": true,                // Display cardinal direction letters (N, E, S, W) around MiniMap frame
    "compassScale": 1.0,                // Scale factor for compass labels and indicator needle
    "compassLocation": 0,               // Compass docking position (0: Embedded top, 1: Outer perimeter)
    "northCompassColorMode": 0,         // North pointer color mode (0: Classic crimson red, 1: Custom RGBA)
    "northCompassCustomColor": [1.0, 0.0, 0.0, 1.0], // Custom RGBA color for the North indicator needle
    "playerArrowColor": 0,              // Center player arrow color mode (0: Pure white, 1: Team color, 2: Red)
    "playerArrowScale": 1.0,            // Player arrow size scale factor

    // ==========================================
    // 4. HUD Telemetry
    // ==========================================
    "infoShowCoords": true,             // Display current physical X, Y, Z coordinates
    "infoShowNetherCoords": true,       // Display real-time 1:8 converted portal coordinates for Nether/Overworld
    "infoShowFacing": true,             // Display text compass heading (North, South, East, West, etc.)
    "infoShowAngles": false,            // Display exact Yaw and Pitch rotation angles in degrees
    "infoShowBiome": true,              // Display localized biome name currently underfoot
    "infoShowWeather": true,            // Display weather forecast icon (Clear, Rain, Thunderstorm, Snow)
    "infoShowTime": true,               // Display in-game clock time (e.g., 12:30)
    "timeFormat24h": true,              // Time format (true: 24-hour military format, false: 12-hour AM/PM)
    "infoShowLight": true,              // Display Sky and Block illumination levels (Sky / Block)
    "infoShowChunkCoords": false,       // Display in-chunk relative coordinates (0~15) and chunk coordinates
    "infoShowBackground": true,         // Render translucent dark backing card behind telemetry text
    "pushboxBossHealth": true,          // Automatically shift MiniMap downward when Wither/Ender Dragon health bars appear
    "pushboxPotionEffects": true,       // Automatically shift MiniMap downward when active potion effect icons appear in top-right

    // ==========================================
    // 5. Entity Radar System
    // ==========================================
    "showRadar": true,                  // Master toggle for entity radar
    "radarShowPlayers": true,           // Radar detects other players
    "radarShowHostile": true,           // Radar detects hostile monsters (Zombies, Skeletons, Creepers, etc.)
    "radarShowFriendly": true,          // Radar detects passive/friendly animals (Cows, Sheep, Villagers, etc.)
    "radarShowItems": false,            // Radar detects dropped item entities on the ground
    "radarOverFrameMode": 1,            // Entity edge clamping mode when beyond radar frame (0: Disabled, 1: Clamped arrow, 2: Clamped dot, etc.)
    "radarHighlightTamed": true,        // Render glowing double-layer neon green aura and heart icon on tamed pets
    "radarDepthFadeStart": 8.0,         // Vertical height difference threshold before entities begin fading (blocks)
    "radarHeightIndicators": true,      // Render elevation indicator triangles (▲ / ▼) for entities above or below
    "entityDepth": true,                // Enable vertical distance awareness
    "entityHeightLimit": 30.0,          // Maximum vertical difference before entities fade out entirely (blocks)
    "alwaysShowNametags": false,        // Force-render entity nameplates on radar regardless of distance
    "radarFilterInclude": "",           // Wildcard inclusion whitelist (empty means all, e.g., "*skeleton*,*zombie*")
    "radarFilterExclude": "",           // Wildcard exclusion blacklist (filters noise mobs, e.g., "bat,squid")

    // ==========================================
    // 6. World Map Display & Controls
    // ==========================================
    "bigMapShowEntities": true,         // Render entity and player head avatars on the World Map
    "bigMapShowMarkers": true,          // Render waypoint icons on the World Map
    "showChunkGrid": false,             // Render F3+G style 16x16 chunk boundary grid on the World Map
    "bigMapShowHoverBox": true,         // Display tooltip popup when hovering over terrain or icons
    "bigMapWaypointDrawerOpen": false,  // Default collapsed/expanded state of the left-side waypoint drawer
    "bigMapCloseDrawerAfterHop": true,  // Automatically collapse drawer after clicking a waypoint to center camera
    "bigMapFollowPlayer": false,        // Automatically pan map camera to follow player movement
    "bigMapWaypointScale": 1.0,         // Scale factor for waypoint icons on the World Map
    "minZoomForLocalWaypoints": 0.5,    // Minimum zoom level required before rendering dense local waypoints
    "renderWaypointBackgrounds": true,  // Render dark background label cards behind waypoint names
    "declutterWaypointLabels": true,    // Prevent overlapping waypoint text labels by decluttering
    "autoHideLabelsOnFarZoom": true,    // Automatically hide small waypoint labels when zoomed far out
    "bigMapEntityScale": 1.0,           // Scale factor for entity head avatars on the World Map
    "bigMapShowDisabledWaypoints": false, // Render disabled waypoints in grayscale on the World Map
    "showWaypointDistance": true,       // Display straight-line physical distance in meters below waypoint labels
    "showSlimeChunks": false,           // Overlay translucent emerald green highlight grid on Bedrock slime chunks
    "bigMapCrossDimCoords": true,       // Display converted Overworld/Nether portal coordinates in bottom-right HUD
    "persistBigMapDimension": false,    // Remember last browsed dimension across map sessions
    "lastViewedDimensionId": 0,         // Last browsed dimension ID (0: Overworld, 1: Nether, 2: The End)
    "showZoomButtons": true,            // Display on-screen +/- zoom control buttons on the World Map

    // ==========================================
    // 7. Waypoints & 3D Beams
    // ==========================================
    "showWaypointsOnMinimap": true,     // Render waypoints on the MiniMap
    "preferOverworldWaypoints": false,  // When in Nether, prioritize rendering corresponding Overworld waypoints
    "showDrawingsOnMinimap": true,      // Render World Map freehand vector drawings on the MiniMap
    "onlyShowCurrentWaypointSet": false,// Only render waypoints belonging to the currently active set
    "currentWaypointSet": "default",    // Name of the currently active waypoint set (Default: default)
    "autoRemoveDeathpoints": false,     // Automatically remove death point marker when retrieving items (within 6 blocks)
    "show3DWaypoints": true,            // Render in-world 3D floating billboard nameplates
    "show3DWaypointBeams": true,        // Project in-world 3D beacon beams into the sky
    "useNativeBeaconBeams": true,       // Hook into native block entity shader pipeline for authentic rotating energy beams
    "show3DWaypointLabels": true,       // Display text names on 3D floating waypoint nameplates
    "show3DWaypointDistance": true,     // Display distance in meters on 3D floating nameplates (e.g., [120m])
    "waypoint3DClampToScreen": true,    // Clamp indicators to screen edge when facing away from target
    "waypoint3DMaxDistance": 2048.0,    // Maximum render distance for in-world 3D markers (blocks)
    "waypoint3DBeamOpacity": 1.0,       // Opacity multiplier for 3D beacon beams (0.1 ~ 1.0)
    "waypoint3DBeamRadius": 1.0,        // Thickness radius of 3D beacon beams
    "waypoint3DScale": 1.0,             // Overall size scale for 3D billboard markers
    "show3DDeathpoints": true,          // Project a dedicated red beacon beam at the most recent death location

    // ==========================================
    // 8. Cave Mode Settings
    // ==========================================
    "caveModeType": 0,                  // Active dimension cave mode (0: Off, 1: Layered scan, 2: Full depth)
    "defaultCaveModeType": 1,           // Default cave mode type applied when entering unconfigured dimensions
    "dimCaveModeType0": 0,              // Overworld cave mode override
    "dimCaveModeType1": 1,              // Nether cave mode override
    "dimCaveModeType2": 0,              // The End cave mode override
    "caveTopYAuto": true,               // Dynamically compute cave ceiling Y level based on player elevation
    "caveTopY": 64,                     // Manual fixed top ceiling Y coordinate for cave mode
    "caveDepth": 32,                    // Number of vertical block layers scanned downward in cave mode
    "legibleCaveMaps": true,            // High-contrast shading enhancement for ambiguous underground blocks
    "autoCaveMode": 1,                  // Overhead solid ceiling grid size required to automatically enter cave mode (0: Disabled)
    "caveModeToggleTimer": 1000,        // Anti-flicker delay (ms) before switching cave mode under broken roofs
    "caveAllowedOverworld": true,       // Allow cave mode in the Overworld
    "caveAllowedNether": true,          // Allow cave mode in the Nether
    "caveAllowedEnd": true,             // Allow cave mode in The End

    // ==========================================
    // 9. PNG Export Settings
    // ==========================================
    "exportForceFullMap": false,        // Force-export entire explored world regions rather than viewport selection
    "exportMultipleImages": false,      // Split massive selections into individual region tiles
    "exportOpenFolder": true,           // Automatically open export directory in Windows File Explorer upon completion
    "exportScaleDownSquare": 1,         // Super-sampling downscale ratio (1 is 1:1 lossless)
    "exportNighttime": false,           // Render exported map using night lighting conditions

    // ==========================================
    // 10. Terrain Shading & Baking
    // ==========================================
    "terrainSlopes": true,              // Render elevation slope hillshading for 3D terrain relief
    "terrainDepth": true,               // Render water and transparent fluid depth gradients
    "adjustHeightForShortBlocks": true, // Correct shadow heights for non-full blocks (slabs, stairs, carpets)
    "blockColorMode": 0,                // Block color sampling mode (0: Texture average, 1: Flat chart)
    "biomeColorsInVanilla": true,       // Apply vanilla temperature and rainfall color blending to grass and foliage
    "mapLighting": true,                // Bake block light levels and natural sky shadows into map tiles
    "displayStainedGlass": true,        // Render stained glass using accurate tinted RGBA transparency
    "loadFlowers": true,                // Render flowers, tall grass, and saplings on map tiles
    "biomeBlending": true,              // Perform 3x3 smooth color blending across biome transitions
    "loadNewChunks": true,              // Continuously scan and cache newly generated chunks
    "updateChunks": true,               // Dynamically re-bake tiles when player builds or breaks blocks
    "mapWritingDistance": 5,            // Background disk writing radius around the player (chunks)
    "ignoreHeightmaps": false,          // Bypass vanilla heightmaps to perform exhaustive full-layer scanning
    "multiLevelPyramid": true,          // Maintain multi-level MIP pyramid textures for ultra-smooth zoom
    "maxLoadedRegions": 64,             // Maximum number of 512x512 region tiles kept in RAM (LRU eviction)

    // ==========================================
    // 11. Special Grids & World Borders
    // ==========================================
    "showRegionGrid": false,            // Render 512x512 block Region file boundary lines on World Map
    "showRegionGridLabel": true,        // Render r.X.Z region file index labels inside grid cells
    "regionGridColor": [1.0, 0.0, 0.0, 0.8], // Region grid line RGBA color
    "regionGridThickness": 1.5,         // Region grid line thickness
    "showMapArtGrid": false,            // Render 128x128 block Map Art boundary lines
    "showMapArtGridLabel": true,        // Render Map Art slot index labels
    "mapArtGridColor": [1.0, 1.0, 0.0, 0.8], // Map Art grid line RGBA color
    "mapArtGridThickness": 1.0,         // Map Art grid line thickness
    "showSpawnChunks": false,           // Highlight permanently loaded world spawn chunks
    "spawnChunksAutoDetect": true,      // Automatically track world spawn coordinate changes
    "customSpawnX": 0,                  // Manually specified spawn coordinate X
    "customSpawnZ": 0,                  // Manually specified spawn coordinate Z
    "spawnEntityRadiusChunks": 9,       // Core entity-ticking spawn chunk radius (default: 9 chunks)
    "spawnLazyRadiusChunks": 12,        // Lazy-ticking chunk radius (default: 12 chunks)
    "spawnBorderRadiusChunks": 16,      // Chunk unload border radius
    "showSpawnChunksOnMinimap": false,  // Display spawn chunk boundaries on the MiniMap
    "showWorldBorder": false,           // Render world border wall boundaries
    "worldBorderCenterX": 0,            // World border center X coordinate
    "worldBorderCenterZ": 0,            // World border center Z coordinate
    "worldBorderSize": 60000000,        // World border diameter side length (blocks)
    "worldBorderColor": [0.0, 0.8, 1.0, 0.8], // World border line RGBA color
    "showWorldBorderWarning": true,     // Display on-screen HUD warning when approaching border
    "persistentBreadcrumbsEnabled": false, // Enable continuous tracking of player movement trails
    "maxBreadcrumbPoints": 1000,        // Maximum number of trail points retained in memory
    "breadcrumbMinDistance": 8.0,       // Minimum movement distance before logging a new breadcrumb point (blocks)
    "breadcrumbColor": [1.0, 0.5, 0.0, 0.8], // Breadcrumb trail line RGBA color
    "showFootsteps": false,             // Render tiny footprint icons along movement paths on MiniMap

    // ==========================================
    // 12. Anarchy Detection & Highways
    // ==========================================
    "anarchyLiquidNewChunks": false,     // Detect unnatural flowing liquids to identify newly loaded chunks
    "anarchyLiquidNewChunksInverse": false, // Inverse filter
    "anarchyLiquidNewChunksOnlyAboveY0": true, // Restrict new chunk detection to Y > 0
    "anarchyLiquidNewChunksColor": [0.0, 1.0, 1.0, 0.5], // New chunk highlight RGBA color
    "anarchyLiquidNewChunksInverseColor": [1.0, 0.0, 0.0, 0.5],
    "anarchyPaletteNewChunks": false,    // Detect new chunks based on block palette format versions
    "anarchyPaletteNewChunksInverse": false,
    "anarchyPaletteNewChunksColor": [0.2, 0.8, 0.2, 0.5],
    "anarchyPaletteNewChunksInverseColor": [0.8, 0.2, 0.2, 0.5],
    "anarchyOldChunks": false,          // Identify and highlight pre-1.18 terrain chunks
    "anarchyOldChunksInverse": false,
    "anarchyOldChunksMode": 0,          // Old chunk detection algorithm (0: Default, 1: Strict)
    "anarchyOldChunksBedrockBlending": true, // Detect bedrock blending seams between new and old chunks
    "anarchyOldChunksColor": [1.0, 0.8, 0.0, 0.5],
    "anarchyOldChunksInverseColor": [0.5, 0.5, 0.5, 0.5],
    "anarchyOldChunksBlendedColor": [1.0, 0.2, 0.8, 0.5],
    "anarchyOldBiomes": false,          // Identify legacy world generation biomes
    "anarchyOldBiomesColor": [0.6, 0.3, 0.8, 0.5],
    "anarchyOldBiomesSeed": 0,          // Baseline seed used for legacy biome comparison
    "anarchyPortals": false,            // Detect and mark active Nether portal coordinates
    "anarchyPortalsColor": [0.8, 0.1, 1.0, 0.8],
    "anarchyPortalSkip": false,         // Ignore portals within dense clusters
    "anarchyPortalSkipRadius": 16,
    "anarchyPortalSkipColor": [0.4, 0.4, 0.4, 0.5],
    "anarchyHighways": false,           // Render projected highway network lines for anarchy servers
    "anarchyHighwaysMode": 0,           // Highway projection mode (0: Orthogonal & Diagonal axes, 1: Full rings)
    "anarchyHighwaysMainAxes": true,    // Render X/Z orthogonal primary axes (X=0, Z=0)
    "anarchyHighwaysDiagAxes": true,    // Render 45-degree diagonal axes (X=Z, X=-Z)
    "anarchyHighwaysRingRoads": false,  // Render square ring highways (e.g., 1k, 5k, 10k, 25k, 50k)
    "anarchyHighwaysDiamondRoads": false, // Render diamond diagonal ring roads
    "anarchyHighwaysGridRoads": false,  // Render secondary highway grid roads
    "anarchyHighwaysRingInterval": 5000,// Default interval distance between ring roads (blocks)
    "anarchyHighwaysDiamondInterval": 5000,
    "anarchyHighwaysGridSpacing": 1000,
    "anarchyHighwaysMaxCoord": 30000000,// Maximum coordinate reach for highway lines
    "anarchyHighwaysCustomRingsStr": "1000,5000,10000,25000,50000,100000", // Comma-separated list of custom ring radii
    "anarchyHighwaysDimOverworld": true,// Display highways in the Overworld
    "anarchyHighwaysDimNether": true,   // Display highways in the Nether
    "anarchyHighwaysDimEnd": false,     // Display highways in The End
    "anarchyHighwaysWidth": 3,          // Highway rendered line width (blocks)
    "anarchyHighwaysColor": [0.2, 0.5, 1.0, 0.8], // Primary highway line RGBA color
    "anarchyHighwaysSecondaryColor": [0.5, 0.8, 1.0, 0.5], // Secondary highway line color
    "anarchyHighwaysShowLabels": true,  // Render distance milestone labels on highways (e.g., "+X 10k")
    "anarchyLavaColumns": false,        // Detect towering lava columns (Lava Casts / grief ruins)
    "anarchyLavaColumnsMinHeight": 20,  // Minimum height threshold for lava cast detection
    "anarchyLavaColumnsAlphaShift": 0.2,
    "anarchyLavaColumnsAlphaStep": 0.1,
    "anarchyLavaColumnsColor": [1.0, 0.3, 0.0, 0.8],
    "transparentObsidianRoof": false,   // Render obsidian ceiling above Nether bedrock roof as transparent
    "transparentObsidianRoofY": 128,    // Starting Y level for obsidian transparency
    "roofObsidianDarkening": 0.5,       // Obsidian ceiling darkening factor
    "roofSnowOpacity": 0.5,             // Snow layer opacity on roof
    "anarchySaveLoadToDisk": true,      // Persist discovered anarchy modification spots to disk

    // ==========================================
    // 13. Seed Map Engine
    // ==========================================
    "seedMap": {
        "enabled": false,               // Master toggle for Seed Map
        "searchMode": 0,                // Calculation scope mode (0: Follow player viewport, 1: Fixed center)
        "dimension": 0,                 // Target calculation dimension (0: Overworld, 1: Nether, 2: The End)
        "manualCenterX": 0,             // Manual mode center coordinate X
        "manualCenterZ": 0,             // Manual mode center coordinate Z
        "manualRadius": 2048,           // Manual mode radius (blocks)
        "enabledLayers": [              // Layer toggle boolean array for structure types
            true,                       // 0: Villages
            true,                       // 1: Pillager Outposts
            true,                       // 2: Ancient Cities
            true,                       // 3: Strongholds
            true,                       // 4: Woodland Mansions
            true,                       // 5: Ocean Monuments
            true,                       // 6: Trial Chambers
            true,                       // 7: Desert / Jungle Temples
            true,                       // 8: Nether Fortresses
            true,                       // 9: Bastion Remnants
            true                        // 10: End Cities
        ]
    },
    "manualSeeds": {                    // Per-world manual level seed cache mapping
        // "<World_UUID>": "123456789012345"
    },

    // ==========================================
    // 14. Database & Disk I/O Performance
    // ==========================================
    "useSqliteDatabase": true,          // Enable high-performance SQLite3 WAL database backend (Recommended: true)
    "fastWritesEnabled": true,          // Enable in-memory batch write coalescing queue
    "writeBatchIntervalMs": 500,        // Batch flush interval (ms, 100 ~ 2000)
    "teleportFailNotifier": true,       // Pop alert modal when cross-server or cross-map teleport is rejected or timed out

    // ==========================================
    // 15. Hotkey Bindings Map
    // Each binding stores virtual key code, modifier bitmask, and boolean helper flags
    // ==========================================
    "hotkeys": {
        "openBigMap": { "key": 77, "modifiers": 0 },                 // M key: Toggle World Map
        "openWaypointMgr": { "key": 85, "modifiers": 0 },            // U key: Open Waypoint Manager
        "openDeathPointMgr": { "key": 0, "modifiers": 0 },           // 0 (Unbound): Open Death Points dialog
        "toggleMinimap": { "key": 78, "modifiers": 0 },              // N key: Toggle MiniMap visibility
        "toggleMinimapShape": { "key": 89, "modifiers": 0 },         // Y key: Toggle Circle / Square shape
        "toggleMinimapRot": { "key": 74, "modifiers": 0 },           // J key: Toggle Player Rotation / Locked North
        "holdEntities": { "key": 9, "modifiers": 0 },                // Tab key: Temporarily show entity heads on World Map
        "toggleSeedMap": { "key": 0, "modifiers": 0 },               // Toggle Seed Map
        "enlargeMinimap": { "key": 88, "modifiers": 0 },             // X key: Temporary MiniMap zoom
        "centerCamera": { "key": 32, "modifiers": 0 },               // Spacebar: Center World Map camera on player
        "selectExportArea": { "key": 16, "modifiers": 2, "shift": true }, // Shift key: Box selection PNG export
        "toggleSlimeChunks": { "key": 0, "modifiers": 0 },           // Toggle Slime Chunks overlay
        "switchWaypointSet": { "key": 221, "modifiers": 0 },         // ] key: Cycle to next waypoint set
        "toggleHighways": { "key": 0, "modifiers": 0 },              // Toggle highway lines
        "toggleAnarchySettings": { "key": 0, "modifiers": 0 },       // Open Anarchy settings
        "toggleDrawingMode": { "key": 0, "modifiers": 0 },           // Toggle World Map drawing mode
        "toggleDoubleMinZoom": { "key": 0, "modifiers": 0 },         // Toggle doubled minimum zoom
        "toggleTransBg": { "key": 0, "modifiers": 0 },               // Toggle transparent MiniMap background
        "minimapZoomIn": { "key": 187, "modifiers": 0 },             // = key: Zoom in MiniMap
        "minimapZoomOut": { "key": 189, "modifiers": 0 },            // - key: Zoom out MiniMap
        "togglePreferOwWaypoints": { "key": 0, "modifiers": 0 },     // Toggle prefer Overworld waypoints in Nether
        "quickCreateWaypoint": { "key": 66, "modifiers": 0 },        // B key: Quick-create waypoint at current position
        "toggleViewDistance": { "key": 0, "modifiers": 0 },          // Toggle view distance circle
        "toggleSimulationDistance": { "key": 0, "modifiers": 0 },    // Toggle simulation distance circle
        "toggleBigMapUI": { "key": 70, "modifiers": 0 },             // F key: Toggle clean immersive World Map mode
        "toggleFollowPlayer": { "key": 0, "modifiers": 0 }           // Toggle camera follow player
    }
}
```

---

## 2. Key Settings Quick-Reference

### 2.1 Performance & I/O Settings

| Parameter | Type | Default | Recommended | Description |
| :--- | :--- | :--- | :--- | :--- |
| `useSqliteDatabase` | `boolean` | `true` | **`true`** | Enables SQLite3 WAL storage engine for zero frame drops during high-speed elytra flight. |
| `fastWritesEnabled` | `boolean` | `true` | **`true`** | Batches tile disk writes into memory queues, reducing SSD write amplification. |
| `writeBatchIntervalMs` | `integer` | `500` | `200`~`1000` | Disk flush interval in milliseconds. Set higher on low-power devices. |
| `minimapFpsLimit` | `integer` | `0` | `0` / `60` | Caps MiniMap render framerate. Recommended `60` for laptop battery life. |
| `maxLoadedRegions` | `integer` | `64` | `32`~`128` | Maximum 512x512 region tiles cached in RAM before LRU eviction. |
| `multiLevelPyramid` | `boolean` | `true` | **`true`** | Precomputes multi-level MIP pyramid textures for fluid macro zooming. |

### 2.2 Visual & MiniMap Controls

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `miniMapScale` | `float` | `1.0` | MiniMap window size scale factor (`0.5` to `2.5`). |
| `miniMapZoomRadius` | `integer` | `64` | MiniMap visible block scan radius (`16` to `128` blocks). |
| `isSquareMap` | `boolean` | `false` | `false` for Circular MiniMap, `true` for Square MiniMap. |
| `rotateMiniMap` | `boolean` | `true` | `true` to follow player heading, `false` to lock to North. |
| `transparentMinimapBackground` | `boolean` | `false` | When `true`, unexplored fog and void areas render completely transparent. |

### 2.3 Navigation & 3D Beacon Beams

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `useNativeBeaconBeams` | `boolean` | `true` | Hooks into native block entity rendering pipeline for authentic rotating energy beams. |
| `show3DWaypoints` | `boolean` | `true` | Renders in-world floating 3D billboard nameplates. |
| `waypoint3DMaxDistance` | `float` | `2048.0` | Maximum distance (blocks) before in-world 3D markers fade out. |
| `autoRemoveDeathpoints` | `boolean` | `false` | Automatically removes death marker after approaching within 6 blocks to retrieve items. |

---

## 3. Manual Editing & Best Practices

1. **Hot-Reloading**: Modifications in-game save immediately to `config.json`. If editing manually with an external text editor, ensure the game is **completely closed** before saving to avoid memory state overwriting your disk edits.
2. **Syntax Validation**: Ensure valid JSON syntax (no trailing commas, properly closed brackets). If syntax errors are detected upon launch, the system automatically archives the corrupted file and regenerates a fresh factory default configuration.

---

[← Previous Chapter: 08 - Hotkeys, Settings & Performance Tuning](./08-settings-and-hotkeys.md) | [Back to Docs Index](../README.md)
