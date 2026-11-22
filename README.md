# LoS2 Control

SKSE plugin for **Lanterns of Skyrim II + LoS II Dynamic Lantern Lighting**, the Inverse Square Lighting (Community Shaders) conversion that runs on Light Placer.
It gives the lanterns one schedule that actually drives the Light Placer lights. Picking, editing and adding lights in game is done by ISL Placer.

Skyrim SE, AE (1.5.97 to 1.7.x) and VR. The installer offers two DLLs: the current one for 1.6.1170 and newer, and the older one for VR and 1.6.1130 and below. The VR build compiles and exports the VR entry point but has not been tested in a headset.

## What it does

**Lanterns page**
- Fixed hours, or follow the sun (sunset and sunrise of the current climate, with offsets).
- "Check every": how often the clock and the weather are looked at (100 to 5000 ms, default 500).
- Gloomy weather keeps the lanterns lit by day, from a text list (`LoS2Control_Weathers.txt`). Weathers on the list are treated as "lit all day".
  - **Add the current weather...** asks first and says what it does (for weather mods that add their own fog, rain, snow or storms).
  - **Remove the current weather** takes the weather you are standing in off the list again.
  - **Reset the list to the original...** writes back the list that ships with the mod.
- Lanterns mode: follow the schedule, always on or always off (the weather option only applies while following the schedule).
- **Brightness and Distance** (0.25x to 3x): scale the intensity and the reach of the lanterns this mod controls, the ones whose Light Placer entries read `LoS2C_LightsOn`. In the SE/AE build this is applied every frame right after Light Placer's own update, so it also holds for flickering lights. The older (VR, 1.6.1130 and below) build applies it once a second only, so flickering lanterns may not keep it there.
- **Settings save themselves** a moment after you change them. **Reset everything to default...** puts every setting and the weather list back to what came with the mod.
- Keeps the Lanterns of Skyrim II MCM pinned at "Always off", because the ISL bulbs come from Light Placer.
- The schedule is a global, `LoS2C_LightsOn`, that Light Placer conditions read. Anything can use it.

The in-game light editor that earlier versions had on a second page was removed in 0.2.4. Files it already saved (`Data/LightPlacer/LoS2Control/LoS2Control_Patches.json`) keep working. Files it disabled (`*.json.hidden`) stay disabled until you rename them back by hand.

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

## Language
The menu follows the language the game runs in (`sLanguage` in Skyrim.ini), or the `sLanguage` under `[General]` in `LoS2Control.ini` if you set one there. The texts are plain files in `Data/SKSE/Plugins/LoS2Control/lang/<language>.json`, one object of `"English text": "translation"`. A missing file or a missing line leaves the English text. To translate into another language, copy `_template.json` to `<language>.json` (for example `german.json`) and translate the values; keep the `{}` and `%s` placeholders.

If you add a translation in Japanese, Chinese, Korean or Russian, turn the language on in the SKSE Menu Framework's own `SKSEMenuFramework.ini`, under `[Fonts]` (`EnableJapanese = true`, `EnableChinese`, `EnableKorean`, `EnableCyrillic`). They are off by default, and the menu then shows `?` marks.

## Building
SE/AE: alandtse's CommonLibSSE-NG, `ng.cmake` describes the plugin. VR and older: CharmedBaryon's CommonLibSSE-NG through vcpkg, `plugin/build.cmd`. Both build the same `src/`.

## License
MIT for the files written here; the DLL built with alandtse's CommonLibSSE-NG is a combined work under the GPL (the GPL text is in `LICENSE`). See `LICENSE` and `NOTICE`.
