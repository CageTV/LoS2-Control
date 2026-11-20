#include "Settings.h"

Settings& Settings::Get()
{
	static Settings instance;
	return instance;
}

void Settings::Clamp()
{
	mode = std::clamp(mode, 0, 1);
	checkIntervalMs = std::clamp(checkIntervalMs, 100, 5000);
	onHour = std::clamp(onHour, 0.0f, 24.0f);
	offHour = std::clamp(offHour, 0.0f, 24.0f);
	sunsetOffsetMinutes = std::clamp(sunsetOffsetMinutes, -240.0f, 240.0f);
	sunriseOffsetMinutes = std::clamp(sunriseOffsetMinutes, -240.0f, 240.0f);
	lightBrightness = std::clamp(lightBrightness, 0.25f, 3.0f);
	lightDistance = std::clamp(lightDistance, 0.25f, 3.0f);
	if (alwaysOn && alwaysOff) {
		alwaysOff = false;
	}
	lightsGlobal[sizeof(lightsGlobal) - 1] = '\0';
	language[sizeof(language) - 1] = '\0';
}

void Settings::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	if (ini.LoadFile(kPath) >= 0) {
		const auto getBool = [&](const char* s, const char* k, bool& v) { v = ini.GetBoolValue(s, k, v); };
		const auto getFloat = [&](const char* s, const char* k, float& v) { v = static_cast<float>(ini.GetDoubleValue(s, k, v)); };

		mode = static_cast<int>(ini.GetLongValue("Schedule", "iMode", mode));
		getFloat("Schedule", "fOnHour", onHour);
		getFloat("Schedule", "fOffHour", offHour);
		getFloat("Schedule", "fSunsetOffsetMinutes", sunsetOffsetMinutes);
		getFloat("Schedule", "fSunriseOffsetMinutes", sunriseOffsetMinutes);
		getBool("Schedule", "bWeatherKeepsOn", weatherKeepsOn);
		getBool("Schedule", "bAlwaysOn", alwaysOn);
		getBool("Schedule", "bAlwaysOff", alwaysOff);
		checkIntervalMs = static_cast<int>(ini.GetLongValue("Schedule", "iCheckIntervalMs", checkIntervalMs));

		if (const char* l = ini.GetValue("General", "sLanguage", language); l) {
			strncpy_s(language, l, _TRUNCATE);
		}
		getFloat("Lights", "fBrightness", lightBrightness);
		getFloat("Lights", "fDistance", lightDistance);

		getBool("LoS2", "bPinLanternsOfSkyrimMCM", pinLoS2);
		const char* g = ini.GetValue("LoS2", "sLightsGlobal", lightsGlobal);
		if (g && *g) {
			strncpy_s(lightsGlobal, g, _TRUNCATE);
		}
	}
	debugLog = ini.GetBoolValue("Log", "Debug", debugLog);
	Clamp();
}

void Settings::Save() const
{
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(kPath);  // keep whatever else is in there

	ini.SetLongValue("Schedule", "iMode", mode, "; 0 = fixed hours (fOnHour / fOffHour), 1 = follow the climate's sunset and sunrise plus the offsets below");
	ini.SetDoubleValue("Schedule", "fOnHour", onHour);
	ini.SetDoubleValue("Schedule", "fOffHour", offHour);
	ini.SetDoubleValue("Schedule", "fSunsetOffsetMinutes", sunsetOffsetMinutes);
	ini.SetDoubleValue("Schedule", "fSunriseOffsetMinutes", sunriseOffsetMinutes);
	ini.SetBoolValue("Schedule", "bWeatherKeepsOn", weatherKeepsOn, "; keep the lanterns lit by day during the weathers in LoS2Control_Weathers.txt");
	ini.SetBoolValue("Schedule", "bAlwaysOn", alwaysOn);
	ini.SetBoolValue("Schedule", "bAlwaysOff", alwaysOff);
	ini.SetLongValue("Schedule", "iCheckIntervalMs", checkIntervalMs, "; how often (milliseconds, 100 - 5000) the clock and the weather are checked. Raise it if another weather mod needs the room; lights only change at a transition anyway");

	ini.SetValue("General", "sLanguage", language, "; language of the menu. Empty = the language the game runs in. A name such as japanese reads Data/SKSE/Plugins/LoS2Control/lang/<name>.json; english is the built-in text");
	ini.SetDoubleValue("Lights", "fBrightness", lightBrightness, "; multiplies the brightness (intensity) of the lanterns this mod controls. 1 = as the light files say. 0.25 - 3");
	ini.SetDoubleValue("Lights", "fDistance", lightDistance, "; multiplies how far their light reaches (radius). 1 = as the light files say. 0.25 - 3");

	ini.SetBoolValue("LoS2", "bPinLanternsOfSkyrimMCM", pinLoS2, "; keep Lanterns of Skyrim II at Always off so only the Light Placer bulbs light the lanterns");
	ini.SetValue("LoS2", "sLightsGlobal", lightsGlobal, "; GlobalVariable (in LoS2-DLL.esp) that the Light Placer JSON reads: 1 = lit, 0 = off");

	ini.SetBoolValue("Log", "Debug", debugLog);

	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(kPath).parent_path(), ec);
	if (ini.SaveFile(kPath) < 0) {
		logger::error("Could not write {}", kPath);
	}
}

namespace
{
	std::atomic<std::int64_t> changedAtMs{ 0 };

	std::int64_t NowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + 1;
	}
}

void Settings::MarkChanged()
{
	changedAtMs = NowMs();
}

void Settings::SaveIfDue()
{
	auto t = changedAtMs.load();
	if (t == 0 || NowMs() - t < 1500) {
		return;
	}
	if (changedAtMs.compare_exchange_strong(t, 0)) {
		Settings copy = Get();
		copy.Clamp();
		copy.Save();
		logger::info("Settings saved");
	}
}
