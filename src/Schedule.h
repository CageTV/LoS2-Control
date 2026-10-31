#pragma once

namespace Schedule
{
	struct Status
	{
		float       hour{ 0.0f };
		float       onAt{ 0.0f };      // hour the lanterns light, as computed from the current mode
		float       offAt{ 0.0f };
		bool        night{ false };
		bool        weatherOn{ false };
		bool        lit{ false };
		bool        globalFound{ false };
		int         pinnedFound{ 0 };  // how many of LoS II's two control globals were found
		std::string weather;
	};

	// Data is loaded: finds the globals and reads the weather list, starts the update loop
	void Init();
	// Re-reads LoS2Control_Weathers.txt
	void ReloadWeathers();
	// Appends the current weather to the list and the file; returns false when there is none or it is already in
	bool AddCurrentWeather();
	// Settings were changed in the menu: re-evaluate at once
	void SettingsChanged();
	Status GetStatus();
}
