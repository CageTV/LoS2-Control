# LoS2 Control

SKSE plugin for **Lanterns of Skyrim II + LoS II Dynamic Lantern Lighting**, the Inverse Square Lighting (Community Shaders) conversion that runs on Light Placer.
It gives the lanterns one schedule that actually drives the Light Placer lights, and an in-game editor for Light Placer lights in general.

SE and AE. (No VR build yet.)

## What it does

**Lanterns page**
- Fixed hours, or follow the sun (sunset and sunrise of the current climate, with offsets).
- Gloomy weather keeps the lanterns lit by day, from a text list (`LoS2Control_Weathers.txt`). "Add the current weather" adds modded weathers.
- Always on / always off.
- Keeps the Lanterns of Skyrim II MCM pinned at "Always off", because the ISL bulbs come from Light Placer.
- The schedule is a global, `LoS2C_LightsOn`, that Light Placer conditions read. Anything can use it.

**Light editor page**
- Pick the light you are looking at (any light that Light Placer, ISL Helper or the game placed) and change radius, size, cutoff, fade, colour and Community Shaders flags live. Optional: apply to every loaded copy of that object.
- Lists the Light Placer json entries that define the object, and warns when two entries light the same node.
- **Write it as Light Placer json:** saves your values to `Data/LightPlacer/LoS2Control/LoS2Control_Patches.json`, as always on or on the lantern schedule.
- **Turn off for this object:** conflict resolution. Takes the object out of another json entry in a rewritten copy of the file. The first version of every changed file is kept in `Data/SKSE/Plugins/LoS2Control/backups` and "Put back" restores it.
- **Add a light to this object:** for models that have no light. Attach to a node or a point, choose a base light record, save.

Light Placer reads its files once at game start: json written from the editor shows after a restart. ISL Helper skips any light that a Light Placer file names, so lights written here are left alone by it.

## Requirements
- Skyrim SE / AE, SKSE, Address Library
- [Light Placer](https://www.nexusmods.com/skyrimspecialedition/mods/127557)
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) (the menus)
- powerofthree's Tweaks (EditorID lookup)
- Lanterns of Skyrim II and LoS II Dynamic Lantern Lighting (updated plugin, see below), ISL Helper SKSE, Base Object Swapper

## Install
The release zip has two folders. Install both as mods:
1. `LoS2 Control` - the DLL, weather list and logo.
2. `LoS2-DLL update` - `LoS2-DLL.esp` with the `LoS2C_LightsOn` global and the two Light Placer json files whose conditions read it. Let it overwrite LoS II Dynamic Lantern Lighting (it replaces the 628 hard-coded weather and 19:00-07:00 conditions).

Settings are in `Data/SKSE/Plugins/LoS2Control.ini`, created on first start.

## Building
CommonLibSSE-NG (alandtse fork), CMake, MSVC. `ng.cmake` describes the plugin.

## License
GPL-3.0, see `LICENSE` and `NOTICE`.
