#include "Schedule.h"

#include "Editor.h"
#include "Settings.h"

namespace Schedule
{
	namespace
	{
		constexpr auto kTick = 250ms;

		std::mutex            weatherLock;
		std::set<std::string> weathers;  // lower case EditorIDs
		std::mutex            statusLock;
		Status                status;
		std::atomic<bool>     running{ false };

		RE::TESGlobal*      lightsGlobal{ nullptr };
		RE::TESGlobal*      losToggle{ nullptr };     // Lanterns of Skyrim II: "Always on"
		RE::TESGlobal*      losToggleOff{ nullptr };  // Lanterns of Skyrim II: "Always off"
		std::string         lastGlobalName;
		bool                warnedGlobal{ false };
		std::optional<bool> lastLit;

		std::string Lower(std::string a_s)
		{
			std::ranges::transform(a_s, a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		std::string Trim(const std::string& a_s)
		{
			const auto b = a_s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) {
				return {};
			}
			return a_s.substr(b, a_s.find_last_not_of(" \t\r\n") - b + 1);
		}

		void FindGlobals()
		{
			const auto& s = Settings::Get();
			if (!lightsGlobal || lastGlobalName != s.lightsGlobal) {
				lastGlobalName = s.lightsGlobal;
				lightsGlobal = RE::TESForm::LookupByEditorID<RE::TESGlobal>(lastGlobalName);
				if (lightsGlobal) {
					logger::info("Found the lights global {} ({:08X})", lastGlobalName, lightsGlobal->GetFormID());
					warnedGlobal = false;
				} else if (!warnedGlobal) {
					logger::warn("GlobalVariable {} not found: add it to LoS2-DLL.esp (Light Placer conditions read it)", lastGlobalName);
					warnedGlobal = true;
				}
			}
			if (!losToggle) {
				losToggle = RE::TESForm::LookupByEditorID<RE::TESGlobal>("LoS_TOGGLE");
			}
			if (!losToggleOff) {
				losToggleOff = RE::TESForm::LookupByEditorID<RE::TESGlobal>("LoS_TOGGLEOFF");
			}
		}

		void SetGlobal(RE::TESGlobal* a_global, float a_value)
		{
			if (a_global && a_global->value != a_value) {
				a_global->value = a_value;
			}
		}

		// Is hour a_h inside the span that starts at a_on and ends at a_off (may wrap midnight)?
		bool InSpan(float a_h, float a_on, float a_off)
		{
			if (a_on <= a_off) {
				return a_h >= a_on && a_h < a_off;
			}
			return a_h >= a_on || a_h < a_off;
		}

		float Wrap24(float a_h)
		{
			a_h = std::fmod(a_h, 24.0f);
			return a_h < 0.0f ? a_h + 24.0f : a_h;
		}

		// Runs on the main thread
		void Update()
		{
			const auto calendar = RE::Calendar::GetSingleton();
			const auto sky = RE::Sky::GetSingleton();
			if (!calendar || !sky) {
				return;
			}
			const auto& s = Settings::Get();
			FindGlobals();

			Status st;
			st.hour = calendar->GetHour();

			if (s.mode == Settings::kSunRelative && sky->currentClimate) {
				// the climate stores these in 10 minute steps
				const auto& t = sky->currentClimate->timing;
				st.onAt = Wrap24(static_cast<float>(t.sunset.begin) / 6.0f + s.sunsetOffsetMinutes / 60.0f);
				st.offAt = Wrap24(static_cast<float>(t.sunrise.end) / 6.0f + s.sunriseOffsetMinutes / 60.0f);
			} else {
				st.onAt = s.onHour;
				st.offAt = s.offHour;
			}
			st.night = InSpan(st.hour, st.onAt, st.offAt);

			if (sky->currentWeather) {
				const char* edid = sky->currentWeather->GetFormEditorID();
				st.weather = edid ? edid : "";
				if (s.weatherKeepsOn && !st.weather.empty()) {
					std::scoped_lock l(weatherLock);
					st.weatherOn = weathers.contains(Lower(st.weather));
				}
			}

			st.lit = s.alwaysOn || (!s.alwaysOff && (st.night || st.weatherOn));
			st.globalFound = lightsGlobal != nullptr;
			st.pinnedFound = (losToggle ? 1 : 0) + (losToggleOff ? 1 : 0);

			SetGlobal(lightsGlobal, st.lit ? 1.0f : 0.0f);
			if (s.pinLoS2) {
				// saved globals come back with every load and the old MCM can flip them, so this is checked every tick
				SetGlobal(losToggle, 0.0f);
				SetGlobal(losToggleOff, 1.0f);
			}

			if (lastLit != st.lit) {
				lastLit = st.lit;
				logger::info("Lanterns {} (hour {:.2f}, on {:.2f}, off {:.2f}, weather {}{})", st.lit ? "ON" : "OFF", st.hour, st.onAt, st.offAt,
					st.weather, st.weatherOn ? " [listed]" : "");
			}

			std::scoped_lock l(statusLock);
			status = st;
		}

		void Tick()
		{
			if (running.load()) {
				Update();
				Editor::Tick();
			}
		}
	}

	void ReloadWeathers()
	{
		std::set<std::string> list;
		std::ifstream         in(Settings::kWeatherPath);
		for (std::string line; std::getline(in, line);) {
			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#') {
				continue;
			}
			list.insert(Lower(line));
		}
		std::scoped_lock l(weatherLock);
		weathers = std::move(list);
		logger::info("{} weathers keep the lanterns on by day ({})", weathers.size(), Settings::kWeatherPath);
	}

	bool AddCurrentWeather()
	{
		std::string name;
		{
			std::scoped_lock l(statusLock);
			name = status.weather;
		}
		if (name.empty()) {
			return false;
		}
		{
			std::scoped_lock l(weatherLock);
			if (!weathers.insert(Lower(name)).second) {
				return false;
			}
		}
		std::ofstream out(Settings::kWeatherPath, std::ios::app);
		out << '\n' << name;
		logger::info("Added weather {} to the list", name);
		return true;
	}

	void SettingsChanged()
	{
		lastLit.reset();
	}

	Status GetStatus()
	{
		std::scoped_lock l(statusLock);
		return status;
	}

	void Init()
	{
		ReloadWeathers();
		if (running.exchange(true)) {
			return;
		}
		// the worker thread only wakes the main thread; the work itself runs there
		std::thread([]() {
			while (true) {
				std::this_thread::sleep_for(kTick);
				if (const auto task = SKSE::GetTaskInterface()) {
					task->AddTask(Tick);
				}
			}
		}).detach();
		logger::info("Schedule running every {} ms", kTick.count());
	}
}
