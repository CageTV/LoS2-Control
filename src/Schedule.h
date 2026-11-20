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
	enum class AddResult { Added, NoWeather, AlreadyListed, NotSaved };
	// Appends the current weather to the list and the file. NotSaved: the file could not be written, nothing was added
	AddResult AddCurrentWeather();
	enum class RemoveResult { Removed, NoWeather, NotListed, NotSaved };
	// Takes the current weather off the list and out of the file
	RemoveResult RemoveCurrentWeather();
	// Writes the list that ships with the mod back to the file and reloads it. False: the file could not be written
	bool ResetWeathers();
	// Settings were changed in the menu: re-evaluate at once
	void SettingsChanged();
	Status GetStatus();
}
