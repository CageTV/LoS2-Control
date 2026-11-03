#pragma once

// Light editor: pick the light you look at, show and edit its Community Shaders ISL values live, and list the Light Placer json files that define it.
// Light Placer lights are NiPointLight nodes whose fields Community Shaders reads directly:
//   ambient.red = flag bits (low 24) + cull bits (high 8, owned by Light Placer: never touched here), ambient.green = cutoff, ambient.blue = LIGH FormID,
//   diffuse = colour, radius.x/y = radius, radius.z = size, fade = intensity.
// So the editor works on any light that has such a node, whether it comes from Light Placer, ISL Helper or a vanilla placed light.
// Edits are session-only (nothing is written to disk here): the in-game patcher (phase 3) writes them out.
namespace Editor
{
	struct LightSnap
	{
		std::string   node;
		float         radius{ 0.0f };
		float         size{ 0.0f };
		float         cutoff{ 0.0f };
		float         fade{ 0.0f };
		float         color[3]{ 1.0f, 1.0f, 1.0f };
		std::uint32_t flags{ 0 };   // low 24 bits only
		std::uint8_t  cull{ 0 };    // Light Placer's cull bits, read-only
		RE::FormID    ligh{ 0 };
		std::string   lighEdid;
		bool          lighIsl{ false };  // the LIGH record itself carries the inverse square flag (ISL Helper converted it, or the author did)
		int           attachKind{ 0 };   // 0 unknown, 1 node of the model (attach = its name), 2 point (attach = "x,y,z")
		std::string   attach;
	};

	// bits of LightSnap::flags (Community Shaders / Light Placer layout)
	namespace Flag
	{
		constexpr std::uint32_t PortalStrict = 1u << 0;
		constexpr std::uint32_t Shadow = 1u << 1;
		constexpr std::uint32_t Simple = 1u << 2;
		constexpr std::uint32_t Initialised = 1u << 8;
		constexpr std::uint32_t Disabled = 1u << 9;
		constexpr std::uint32_t InverseSquare = 1u << 10;
		constexpr std::uint32_t Linear = 1u << 11;
		constexpr std::uint32_t IgnoreScale = 1u << 21;
		constexpr std::uint32_t NoExternalEmittance = 1u << 23;
	}

	struct TargetSnap
	{
		bool                   valid{ false };
		std::uint32_t          serial{ 0 };  // changes when another object is picked
		RE::FormID             refId{ 0 };
		RE::FormID             baseId{ 0 };
		std::string            name;
		std::string            baseEdid;
		std::string            model;
		float                  distance{ 0.0f };
		std::vector<LightSnap> lights;
		std::vector<std::string> nodes;  // names of the nodes of the model, to attach new lights to
	};

	// One entry of a Light Placer json that matches the picked object
	struct Match
	{
		std::string              file;       // path under Data/LightPlacer
		int                      entry{ 0 };
		std::string              by;         // "model" or "formID"
		std::vector<std::string> nodes;      // nodes named by the entry's lights
		std::vector<std::string> lights;     // LIGH EditorIDs named by the entry
		bool                     conditions{ false };
	};
	struct Matches
	{
		std::vector<Match>       list;
		std::vector<std::string> conflicts;  // text lines: the same node is lit by more than one entry
		int                      filesScanned{ 0 };
		int                      entriesScanned{ 0 };
	};

	// Main thread, called every update tick: works on a pending pick request
	void Tick();
	// UI thread
	void RequestPick();
	TargetSnap GetTarget();
	// applyAll: also the other loaded copies of the same base object within range. revert: put the values read at pick time back
	void QueueEdit(const LightSnap& a_edit, bool a_applyAll);
	void QueueRevert(const std::string& a_node, bool a_applyAll);
	// Reads Data/LightPlacer/**/*.json (cached; rescan after changing files)
	void       RescanJson();
	Matches    FindMatches();
	std::string StatusLine();

	// ---- Phase 3: write Light Placer json ----
	// Everything below takes effect after the next game start (Light Placer reads its files once).
	constexpr auto kPatchFile = "Data/LightPlacer/LoS2Control/LoS2Control_Patches.json";
	struct SaveOptions
	{
		int schedule{ 0 };  // 0 always lit, 1 the LoS2 Control schedule outdoors only, 2 the schedule everywhere (LoS2C_LightsOn)
	};
	// Saves the lights of the picked object as one entry (keyed by its model) in LoS2Control_Patches.json: the only file this part creates or replaces
	void SavePatch(const SaveOptions& a_opt);
	// Removes our entry for the picked object's model
	void RemovePatch();
	// Conflict resolution: switches a whole file off by renaming it to <file>.hidden (Light Placer only reads .json); EnableFile undoes it. Both take effect at the next game load.
	void DisableFile(const std::string& a_file);
	void EnableFile(const std::string& a_hidden);
	std::vector<std::string> HiddenFiles();
	// Files an earlier version of this plugin changed (other mods' json, copy kept), so they can be put back
	std::vector<std::string> ChangedFiles();
	void                     RestoreFile(const std::string& a_file);
	std::string              PatchStatus();
}
