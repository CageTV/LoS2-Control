#include "Menu.h"
#include "Schedule.h"
#include "Settings.h"

namespace
{
	void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			return;
		}
		*path /= "LoS2Control.log"sv;
		auto log = std::make_shared<spdlog::logger>("global log"s, std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v"s);
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
			Schedule::Init();
			Menu::Register();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	InitializeLog();

	Settings::Get().Load();
	if (Settings::Get().debugLog) {
		spdlog::set_level(spdlog::level::debug);
	}
	// writes the file when there is none yet, so there is something to edit
	Settings::Get().Save();

	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

	logger::info("LoS2 Control loaded");
	return true;
}
