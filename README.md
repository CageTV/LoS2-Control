# LoS2 Control

SKSE plugin for **Lanterns of Skyrim II + LoS II Dynamic Lantern Lighting**, the Inverse Square Lighting (Community Shaders) conversion that runs on Light Placer.
It gives the lanterns one schedule that actually drives the Light Placer lights, and an in-game editor for Light Placer lights in general.

Skyrim SE, AE (1.5.97 to 1.7.x) and VR. The installer offers two DLLs: the current one for 1.6.1170 and newer, and the older one for VR and 1.6.1130 and below. The VR build compiles and exports the VR entry point but has not been tested in a headset.

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
The download is a FOMOD installer with two steps:
1. Choose the DLL: current (SE/AE 1.6.1170 and newer) or older (VR, 1.6.1130 and below). It is pre-selected from your game version.
2. `LoS2-DLL update` (keep it ticked for the lantern schedule): `LoS2-DLL.esp` with the `LoS2C_LightsOn` global and the two Light Placer json files whose conditions read it. Let it overwrite LoS II Dynamic Lantern Lighting (it replaces the 628 hard-coded weather and 19:00-07:00 conditions).

Settings are in `Data/SKSE/Plugins/LoS2Control.ini`, created on first start.

## Building
SE/AE: alandtse's CommonLibSSE-NG, `ng.cmake` describes the plugin. VR and older: CharmedBaryon's CommonLibSSE-NG through vcpkg, `plugin/build.cmd`. Both build the same `src/`.

## License
MIT for the files written here; the DLL built with alandtse's CommonLibSSE-NG is a combined work under the GPL (the GPL text is in `LICENSE`). See `LICENSE` and `NOTICE`.
