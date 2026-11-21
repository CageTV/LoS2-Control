#include "Menu.h"

#include "Editor.h"
#include "Lang.h"
#include "Schedule.h"
#include "Settings.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include "SKSEMenuFramework.h"
#pragma warning(pop)

namespace Menu
{
	namespace
	{
		using namespace ImGuiMCP;

		std::string Clock(float a_hour)
		{
			const int mins = static_cast<int>(std::lround(a_hour * 60.0f)) % 1440;
			return std::format("{:02}:{:02}", mins / 60, mins % 60);
		}

		constexpr const char* kLogoPath = "Data\\Interface\\LoS2Control\\los2_logo.png";
		constexpr float       kLogoAspect = 119.0f / 459.0f;

		void Logo()
		{
			static ImTextureID tex = SKSEMenuFramework::LoadTexture(kLogoPath);
			if (!tex) {
				return;
			}
			const float avail = GetContentRegionAvail().x;
			const float w = std::min(avail, 360.0f);
			SetCursorPosX(GetCursorPosX() + (avail - w) * 0.5f);
			Image(tex, ImVec2(w, w * kLogoAspect));
			Spacing();
		}

		void Tip(const char* a_text)
		{
			if (IsItemHovered()) {
				SetTooltip("%s", Lang::T(a_text));
			}
		}

		// A modal question with two buttons. The texts are translated here. True on the frame the person pressed the yes button.
		bool ConfirmPopup(const char* a_title, const char* a_body, const char* a_yes)
		{
			bool yes = false;
			if (BeginPopupModal(Lang::T(a_title), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				PushTextWrapPos(460.0f);
				Text("%s", Lang::T(a_body));
				PopTextWrapPos();
				Spacing();
				if (Button(Lang::T(a_yes))) {
					yes = true;
					CloseCurrentPopup();
				}
				SameLine();
				if (Button(Lang::T("Cancel"))) {
					CloseCurrentPopup();
				}
				EndPopup();
			}
			return yes;
		}

		void __stdcall RenderSettings()
		{
			auto& s = Settings::Get();
			bool  c = false;

			Logo();

			SeparatorText(Lang::T("When the lanterns are lit"));
			c |= Combo(Lang::T("Mode"), &s.mode, Lang::Items("Fixed hours|Follow the sun").c_str());
			Tip("Fixed hours uses the two times below. Follow the sun uses the current climate's sunset and sunrise.");
			if (s.mode == Settings::kFixedHours) {
				c |= SliderFloat(Lang::T("Lit from"), &s.onHour, 12.0f, 24.0f, "%.2f h");
				c |= SliderFloat(Lang::T("Off at"), &s.offHour, 0.0f, 12.0f, "%.2f h");
			} else {
				c |= SliderFloat(Lang::T("After sunset begins"), &s.sunsetOffsetMinutes, -180.0f, 180.0f, "%+.0f min");
				c |= SliderFloat(Lang::T("After sunrise ends"), &s.sunriseOffsetMinutes, -180.0f, 180.0f, "%+.0f min");
			}
			int lanternMode = s.alwaysOn ? 1 : (s.alwaysOff ? 2 : 0);
			if (Combo(Lang::T("Lanterns"), &lanternMode, Lang::Items("Follow the schedule|Always on|Always off").c_str())) {
				s.alwaysOn = lanternMode == 1;
				s.alwaysOff = lanternMode == 2;
				c = true;
			}
			Tip("Follow the schedule: the hours or sun above, plus gloomy weather if ticked. Always on / Always off ignore the schedule and the weather.");
			BeginDisabled(lanternMode != 0);
			c |= Checkbox(Lang::T("Gloomy weather keeps them lit by day"), &s.weatherKeepsOn);
			EndDisabled();
			Tip("In the weathers on the gloomy list (fog, rain, storm, snow, ash; LoS2Control_Weathers.txt) the lanterns stay lit all day. Only used while the lanterns follow the schedule.");
			c |= SliderInt(Lang::T("Check every"), &s.checkIntervalMs, 100, 5000, "%d ms");
			Tip("How often the clock and the weather are looked at. Lanterns only change at a transition, so a longer time is fine. Raise it if another weather mod needs the room.");

			SeparatorText(Lang::T("Lantern light"));
			c |= SliderFloat(Lang::T("Brightness"), &s.lightBrightness, 0.25f, 3.0f, "%.2fx");
			Tip("How bright the lanterns shine. 1.00x is what the light files say. Applies to the lanterns this mod controls, and shows within a second.");
			c |= SliderFloat(Lang::T("Distance"), &s.lightDistance, 0.25f, 3.0f, "%.2fx");
			Tip("How far the light reaches. 1.00x is what the light files say. Applies to the lanterns this mod controls, and shows within a second.");
			if (Button(Lang::T("Reset brightness and distance"))) {
				s.lightBrightness = 1.0f;
				s.lightDistance = 1.0f;
				c = true;
			}

			SeparatorText(Lang::T("Gloomy weathers"));
			TextWrapped(Lang::T("In these weathers the lanterns stay lit all day, the same as at night."));
			static std::string addNote;
			const auto         now = Schedule::GetStatus();
			if (Button(Lang::T("Add the current weather..."))) {
				if (now.weather.empty()) {
					addNote = Lang::T("No weather was found to add.");
				} else {
					addNote.clear();
					OpenPopup(Lang::T("Keep the lanterns lit in this weather?"));
				}
			}
			Tip("Press it while the weather you want to add is active. A weather mod's own fog, rain, snow or storm weathers are not on the list until you add them.");
			SameLine();
			if (Button(Lang::T("Reload the list"))) {
				Schedule::ReloadWeathers();
				addNote = Lang::T("List reloaded from the file.");
			}
			Tip("Reads LoS2Control_Weathers.txt again. A weather that was never saved to the file drops out.");
			if (Button(Lang::T("Remove the current weather"))) {
				switch (Schedule::RemoveCurrentWeather()) {
				case Schedule::RemoveResult::Removed:
					addNote = Lang::F("{} removed from the list.", now.weather);
					break;
				case Schedule::RemoveResult::NotListed:
					addNote = Lang::F("{} is not on the list.", now.weather);
					break;
				case Schedule::RemoveResult::NotSaved:
					addNote = Lang::F("{} could not be removed: LoS2Control_Weathers.txt could not be written.", now.weather);
					break;
				default:
					addNote = Lang::T("No weather was found to remove.");
					break;
				}
			}
			Tip("Takes the weather that is active now off the list, so the lanterns follow the schedule again in it. For a weather you added by mistake.");
			SameLine();
			if (Button(Lang::T("Reset the list to the original..."))) {
				OpenPopup(Lang::T("Reset the weather list?"));
			}
			Tip("Puts the list back to the one that came with LoS2 Control. Every weather you added is removed.");
			if (ConfirmPopup("Reset the weather list?", "The list goes back to the one that came with LoS2 Control. Every weather you added is removed, and the lanterns follow the schedule again in those weathers.", "Reset the list")) {
				addNote = Schedule::ResetWeathers() ? Lang::T("The weather list is back to the original.") : Lang::T("LoS2Control_Weathers.txt could not be written.");
			}
			if (!addNote.empty()) {
				TextWrapped("%s", addNote.c_str());
			}
			if (BeginPopupModal(Lang::T("Keep the lanterns lit in this weather?"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				PushTextWrapPos(460.0f);
				Text(Lang::T("Current weather: %s"), now.weather.c_str());
				Spacing();
				Text(Lang::T("Every time this weather is active, the lanterns will stay lit all day, not only at night."));
				Spacing();
				Text(Lang::T("Add it for gloomy weather such as fog, rain, snow or storms. If you add a clear weather by mistake, the lanterns will never switch off by day."));
				PopTextWrapPos();
				Spacing();
				if (Button(Lang::T("Add it to the list"))) {
					switch (Schedule::AddCurrentWeather()) {
					case Schedule::AddResult::Added:
						addNote = Lang::F("{} added: the lanterns now stay lit all day in this weather.", now.weather);
						break;
					case Schedule::AddResult::AlreadyListed:
						addNote = Lang::F("{} is already on the list.", now.weather);
						break;
					case Schedule::AddResult::NotSaved:
						addNote = Lang::F("{} could not be added: LoS2Control_Weathers.txt could not be written.", now.weather);
						break;
					default:
						addNote = Lang::T("No weather was found to add.");
						break;
					}
					CloseCurrentPopup();
				}
				SameLine();
				if (Button(Lang::T("Cancel"))) {
					CloseCurrentPopup();
				}
				EndPopup();
			}

			SeparatorText(Lang::T("Lanterns of Skyrim II"));
			c |= Checkbox(Lang::T("Keep its own MCM at 'Always off'"), &s.pinLoS2);
			Tip("The ISL bulbs come from Light Placer, so the original lantern lights must stay off.");

			SeparatorText(Lang::T("Now"));
			const auto st = Schedule::GetStatus();
			Text(Lang::T("Hour %s, lit from %s to %s (%s)"), Clock(st.hour).c_str(), Clock(st.onAt).c_str(), Clock(st.offAt).c_str(), st.night ? Lang::T("night") : Lang::T("day"));
			Text(Lang::T("Weather: %s%s"), st.weather.empty() ? "-" : st.weather.c_str(), st.weatherOn ? Lang::T("  (on the gloomy list: lanterns stay lit all day)") : "");
			Text(Lang::T("Lanterns: %s"), st.lit ? Lang::T("ON") : Lang::T("off"));
			if (!st.globalFound) {
				TextDisabled(Lang::T("%s was not found: it has to exist in LoS2-DLL.esp"), s.lightsGlobal);
			}
			if (s.pinLoS2 && st.pinnedFound < 2) {
				TextDisabled(Lang::T("Lanterns of Skyrim II control globals found: %d of 2"), st.pinnedFound);
			}

			Spacing();
			if (Button(Lang::T("Save now"))) {
				s.Clamp();
				s.Save();
				Schedule::SettingsChanged();
			}
			SameLine();
			if (Button(Lang::T("Reload from disk"))) {
				s.Load();
				Schedule::SettingsChanged();
			}
			SameLine();
			if (Button(Lang::T("Reset everything to default..."))) {
				OpenPopup(Lang::T("Reset all settings?"));
			}
			Tip("Puts every setting and the weather list back to the defaults that came with LoS2 Control.");
			if (ConfirmPopup("Reset all settings?", "Every setting goes back to its default: the schedule, brightness and distance, language and the rest. The weather list goes back to the original one. Light files saved earlier are not touched.", "Reset everything")) {
				s = Settings{};
				s.Clamp();
				s.Save();
				const bool listOk = Schedule::ResetWeathers();
				Schedule::SettingsChanged();
				addNote = listOk ? Lang::T("Everything is back to the defaults.") : Lang::T("The settings are back to the defaults, but LoS2Control_Weathers.txt could not be written.");
				c = false;
			}

			const auto language = Lang::Name();
			TextDisabled("%s", Lang::F("Language: {} ({} texts translated)", language, Lang::Count()).c_str());
			Tip("The menu follows the language the game runs in. To pick another, set sLanguage in LoS2Control.ini. Translations are the files in Data/SKSE/Plugins/LoS2Control/lang/ and can be edited with any text editor.");
			if (language != "english" && Lang::Count() == 0) {
				TextWrapped("%s", Lang::T("There is no translation file for this language, so the text stays English."));
			}
			if (language == "japanese" || language == "chinese" || language == "korean" || language == "russian") {
				TextWrapped("%s", Lang::T("If the menu text shows as ? marks: in SKSEMenuFramework.ini, under [Fonts], turn on the language (EnableJapanese, EnableChinese, EnableKorean or EnableCyrillic)."));
			}
			if (c) {
				s.Clamp();
				Schedule::SettingsChanged();
				Settings::MarkChanged();
			}
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("SKSE Menu Framework is not installed, the settings stay in the ini");
			return;
		}
		SKSEMenuFramework::SetSection("LoS2 Control");
		SKSEMenuFramework::AddSectionItem(Lang::T("Lanterns"), RenderSettings);
		logger::info("Registered the settings page with the SKSE Menu Framework");
	}
}
