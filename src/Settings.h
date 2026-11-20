#pragma once

// Settings live in Data/SKSE/Plugins/LoS2Control.ini. The weather list is LoS2Control_Weathers.txt next to it.
struct Settings
{
	enum Mode : int { kFixedHours = 0, kSunRelative = 1 };

	int   mode{ kFixedHours };
	float onHour{ 19.0f };                 // lanterns on from this hour ...
	float offHour{ 7.0f };                 // ... until this one (fixed-hours mode)
	float sunsetOffsetMinutes{ 0.0f };     // sun mode: minutes after the climate's sunset begins (negative = earlier)
	float sunriseOffsetMinutes{ 0.0f };    // sun mode: minutes after the climate's sunrise ends
	bool  weatherKeepsOn{ true };          // fog, rain, storm, snow, ash keep the lanterns lit by day
	bool  alwaysOn{ false };
	bool  alwaysOff{ false };

	bool  pinLoS2{ true };                 // keep Lanterns of Skyrim II's own MCM at "Always off" (the ISL bulbs do the lighting)
	char  lightsGlobal[64]{ "LoS2C_LightsOn" };  // GlobalVariable in LoS2-DLL.esp that Light Placer conditions read
	int   checkIntervalMs{ 500 };          // how often the schedule looks at the clock and the weather (100 - 5000)
	bool  debugLog{ false };

	char  language[32]{ "" };               // [General] sLanguage: empty = follow the game's language, or a name such as "japanese" (file lang/<name>.json)
	float lightBrightness{ 1.0f };         // multiplies the intensity (fade) of the lanterns this mod controls: 0.25 - 3
	float lightDistance{ 1.0f };           // multiplies their radius: 0.25 - 3

	static Settings& Get();
	static constexpr const char* kPath = "Data/SKSE/Plugins/LoS2Control.ini";
	static constexpr const char* kWeatherPath = "Data/SKSE/Plugins/LoS2Control_Weathers.txt";
	void Load();
	void Save() const;
	void Clamp();
	// A setting was changed in the menu: it is written to the ini a moment after the last change (the settings were lost when the game closed before "Save now" was pressed)
	static void MarkChanged();
	// Called every tick on the main thread: writes the ini once the last change is old enough
	static void SaveIfDue();
};
