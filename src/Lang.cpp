#include "Lang.h"

#include "Settings.h"

#include <nlohmann/json.hpp>

namespace Lang
{
	namespace
	{
		std::once_flag                               once;
		std::string                                  name{ "english" };
		std::unordered_map<std::string, std::string> table;

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

		void Load()
		{
			std::string lang = Trim(Settings::Get().language);
			if (lang.empty()) {
				if (const auto ini = RE::INISettingCollection::GetSingleton()) {
					const auto set = ini->GetSetting("sLanguage:General");
					if (set && set->GetType() == RE::Setting::Type::kString && set->GetString()) {
						lang = set->GetString();
					}
				}
			}
			lang = Lower(Trim(lang));
			if (lang.empty()) {
				lang = "english";
			}
			name = lang;
			if (name == "english") {
				logger::info("Language: english");
				return;
			}
			// the file name is a plain word: nothing from the ini or the game may point outside the folder
			if (name.find_first_of("/\\.:") != std::string::npos) {
				logger::warn("Language '{}' is not a plain name, using english", name);
				name = "english";
				return;
			}
			const auto path = std::format("Data/SKSE/Plugins/LoS2Control/lang/{}.json", name);
			std::ifstream in(path);
			if (!in) {
				logger::info("Language: {} (no {}, the text stays english)", name, path);
				return;
			}
			const auto doc = nlohmann::json::parse(in, nullptr, false, true);
			if (doc.is_discarded() || !doc.is_object()) {
				logger::warn("Language: {} is not a valid json object, the text stays english", path);
				return;
			}
			for (const auto& [key, value] : doc.items()) {
				if (value.is_string() && !value.get<std::string>().empty()) {
					table[key] = value.get<std::string>();
				}
			}
			logger::info("Language: {} ({} texts from {})", name, table.size(), path);
		}
	}

	const char* T(const char* a_english)
	{
		if (!a_english) {
			return "";
		}
		std::call_once(once, Load);
		if (table.empty()) {
			return a_english;
		}
		const auto it = table.find(a_english);
		return it == table.end() ? a_english : it->second.c_str();
	}

	std::string Name()
	{
		std::call_once(once, Load);
		return name;
	}

	int Count()
	{
		std::call_once(once, Load);
		return static_cast<int>(table.size());
	}

	std::string Items(const char* a_english_pipe_separated)
	{
		std::string out;
		const std::string_view all{ a_english_pipe_separated ? a_english_pipe_separated : "" };
		std::size_t            start = 0;
		while (start <= all.size()) {
			const auto bar = all.find('|', start);
			const auto end = bar == std::string_view::npos ? all.size() : bar;
			if (end > start) {
				out += T(std::string(all.substr(start, end - start)).c_str());
				out += '\0';
			}
			if (bar == std::string_view::npos) {
				break;
			}
			start = bar + 1;
		}
		return out;
	}
}
