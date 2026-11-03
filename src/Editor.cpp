#include "Editor.h"

#include <nlohmann/json.hpp>

namespace Editor
{
	namespace
	{
		constexpr float kPickRange = 2500.0f;     // units around the player that are searched
		constexpr float kPickMinCos = 0.94f;      // about 20 degrees around the view direction
		constexpr float kApplyAllRange = 6000.0f;

		std::mutex                                   lock;
		TargetSnap                                   target;
		RE::ObjectRefHandle                          targetHandle;
		std::unordered_map<std::string, LightSnap>   originals;  // by node name, values read at pick time
		std::atomic<bool>                            pickRequested{ false };
		std::string                                  status = "Look at a lantern and press the button.";
		std::uint32_t                                serial = 0;

		std::string Lower(std::string a_s)
		{
			std::ranges::transform(a_s, a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		// "Meshes\Lanterns\x.nif" -> "lanterns/x.nif"
		std::string NormModel(std::string a_s)
		{
			a_s = Lower(std::move(a_s));
			std::ranges::replace(a_s, '\\', '/');
			if (a_s.starts_with("meshes/")) {
				a_s.erase(0, 7);
			}
			return a_s;
		}

		// the reference callbacks take a pointer in the SE/AE library and a reference in the VR one
		RE::TESObjectREFR* Ptr(RE::TESObjectREFR* a_ref) { return a_ref; }
		RE::TESObjectREFR* Ptr(RE::TESObjectREFR& a_ref) { return &a_ref; }

		// the VR library has no NiPointLight::SetLightAttenuation: call the game function (a Skyrim SE id; VR resolves it through the VR address library)
		template <class T>
		void SetAttenuation(T* a_light, float a_radius)
		{
			if constexpr (requires { a_light->SetLightAttenuation(a_radius); }) {
				a_light->SetLightAttenuation(a_radius);
			} else {
				using func_t = void(T*, float);
				static REL::Relocation<func_t> func{ RELOCATION_ID(17224, 17626) };
				func(a_light, a_radius);
			}
		}

		void CollectLights(RE::NiAVObject* a_obj, std::vector<RE::NiPointLight*>& a_out, int a_depth = 0)
		{
			if (!a_obj || a_depth > 40) {
				return;
			}
			if (const auto light = netimmerse_cast<RE::NiPointLight*>(a_obj)) {
				a_out.push_back(light);
			}
			if (const auto node = a_obj->AsNode()) {
				for (auto& child : node->GetChildren()) {
					CollectLights(child.get(), a_out, a_depth + 1);
				}
			}
		}

		// Where the light hangs, as Light Placer's json names it. Light Placer puts a light straight on the named node when it has no offset or rotation,
		// otherwise on a child "LP_Node[path|NAME(x,y,z)]#i" (node) or "LP_Node[path|x,y,z]#i" (point).
		void ReadAttach(RE::NiPointLight* a_light, RE::NiAVObject* a_root, LightSnap& a_s)
		{
			const auto parent = a_light->parent;
			if (!parent) {
				return;
			}
			if (parent == a_root) {
				a_s.attachKind = 2;
				a_s.attach = "0,0,0";
				return;
			}
			const std::string pn = parent->name.c_str() ? parent->name.c_str() : "";
			if (pn.empty()) {
				return;
			}
			if (pn.starts_with("LP_Node[")) {
				const auto bar = pn.find('|');
				const auto end = bar == std::string::npos ? bar : pn.find_first_of("(]", bar);
				if (end == std::string::npos) {
					return;
				}
				a_s.attach = pn.substr(bar + 1, end - bar - 1);
				a_s.attachKind = a_s.attach.find(',') != std::string::npos ? 2 : 1;
			} else {
				a_s.attach = pn;
				a_s.attachKind = 1;
			}
		}

		void CollectNodeNames(RE::NiAVObject* a_obj, std::vector<std::string>& a_out, int a_depth = 0)
		{
			if (!a_obj || a_depth > 30 || a_out.size() >= 300) {
				return;
			}
			if (const auto node = a_obj->AsNode()) {
				const std::string n = node->name.c_str() ? node->name.c_str() : "";
				if (a_depth > 0 && !n.empty() && !n.starts_with("LP_") && std::ranges::find(a_out, n) == a_out.end()) {
					a_out.push_back(n);
				}
				for (auto& child : node->GetChildren()) {
					CollectNodeNames(child.get(), a_out, a_depth + 1);
				}
			}
		}

		LightSnap Read(RE::NiPointLight* a_light, RE::NiAVObject* a_root = nullptr)
		{
			LightSnap s;
			s.node = a_light->name.c_str() ? a_light->name.c_str() : "";
			ReadAttach(a_light, a_root, s);
			const auto& d = a_light->GetLightRuntimeData();
			s.radius = d.radius.x;
			s.size = d.radius.z;
			s.cutoff = d.ambient.green;
			s.fade = d.fade;
			s.color[0] = d.diffuse.red;
			s.color[1] = d.diffuse.green;
			s.color[2] = d.diffuse.blue;
			const auto bits = std::bit_cast<std::uint32_t>(d.ambient.red);
			s.flags = bits & 0x00FFFFFFu;
			s.cull = static_cast<std::uint8_t>(bits >> 24);
			s.ligh = std::bit_cast<std::uint32_t>(d.ambient.blue);
			if (const auto form = s.ligh ? RE::TESForm::LookupByID(s.ligh) : nullptr) {
				if (const char* e = form->GetFormEditorID(); e && *e) {
					s.lighEdid = e;
				}
				if (const auto ligh = form->As<RE::TESObjectLIGH>()) {
					s.lighIsl = ligh->data.flags.any(static_cast<RE::TES_LIGHT_FLAGS>(1u << 14));
				}
			}
			return s;
		}

		void Write(RE::NiPointLight* a_light, const LightSnap& a_s)
		{
			auto& d = a_light->GetLightRuntimeData();
			d.diffuse = RE::NiColor(a_s.color[0], a_s.color[1], a_s.color[2]);
			d.radius.x = a_s.radius;
			d.radius.y = a_s.radius;
			d.radius.z = a_s.size;
			SetAttenuation(a_light, a_s.radius);
			d.fade = a_s.fade;
			d.ambient.green = a_s.cutoff;
			// the high byte is Light Placer's (condition, game and script culling): keep it exactly as it is
			const auto old = std::bit_cast<std::uint32_t>(d.ambient.red);
			d.ambient.red = std::bit_cast<float>((old & 0xFF000000u) | (a_s.flags & 0x00FFFFFFu));
		}

		std::string ModelOf(RE::TESBoundObject* a_base)
		{
			if (!a_base) {
				return {};
			}
			if (const auto model = skyrim_cast<RE::TESModel*>(a_base)) {
				if (const char* m = model->GetModel(); m && *m) {
					return m;
				}
			}
			return {};
		}

		// the light nearest the view direction, within range
		RE::TESObjectREFR* PickRef(float& a_dist, bool a_any)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto cam = RE::PlayerCamera::GetSingleton();
			if (!player || !cam || !cam->cameraRoot) {
				return nullptr;
			}
			const RE::NiPoint3 origin = cam->cameraRoot->world.translate;
			const RE::NiPoint3 fwd = cam->cameraRoot->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f);

			RE::TESObjectREFR* best = nullptr;
			float              bestScore = -1.0f;
			RE::TES::GetSingleton()->ForEachReferenceInRange(player, kPickRange, [&](auto&& a_arg) {
				RE::TESObjectREFR* a_ref = Ptr(a_arg);
				if (!a_ref || a_ref->IsDisabled() || !a_ref->Get3D()) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				std::vector<RE::NiAVObject*> points;  // what is aimed at: the lights, or with a_any the origin of the object itself
				if (a_any) {
					if (skyrim_cast<RE::Actor*>(a_ref) || a_ref == player) {
						return RE::BSContainer::ForEachResult::kContinue;
					}
					points.push_back(a_ref->Get3D());
				} else {
					std::vector<RE::NiPointLight*> found;
					CollectLights(a_ref->Get3D(), found);
					points.assign(found.begin(), found.end());
				}
				for (const auto light : points) {
					const RE::NiPoint3 to = light->world.translate - origin;
					const float        len = to.Length();
					if (len < 1.0f) {
						continue;
					}
					const float c = (to.x * fwd.x + to.y * fwd.y + to.z * fwd.z) / len;
					if (c < kPickMinCos) {
						continue;
					}
					// mostly the angle, a little the distance, so that a near lantern beats a far one on the same line
					const float score = c - len / 100000.0f;
					if (score > bestScore) {
						bestScore = score;
						best = a_ref;
						a_dist = len;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			return best;
		}

		TargetSnap Snapshot(RE::TESObjectREFR* a_ref, float a_dist)
		{
			TargetSnap t;
			if (!a_ref) {
				return t;
			}
			t.valid = true;
			t.refId = a_ref->GetFormID();
			t.distance = a_dist;
			const auto base = a_ref->GetBaseObject();
			t.baseId = base ? base->GetFormID() : 0;
			if (const char* n = a_ref->GetDisplayFullName(); n && *n) {
				t.name = n;
			}
			if (base) {
				if (const char* e = base->GetFormEditorID(); e && *e) {
					t.baseEdid = e;
				}
				t.model = ModelOf(base);
			}
			std::vector<RE::NiPointLight*> lights;
			CollectLights(a_ref->Get3D(), lights);
			for (const auto light : lights) {
				t.lights.push_back(Read(light, a_ref->Get3D()));
			}
			CollectNodeNames(a_ref->Get3D(), t.nodes);
			return t;
		}

		void ApplyTo(RE::TESObjectREFR* a_ref, const LightSnap& a_s)
		{
			if (!a_ref || !a_ref->Get3D()) {
				return;
			}
			std::vector<RE::NiPointLight*> lights;
			CollectLights(a_ref->Get3D(), lights);
			for (const auto light : lights) {
				if (light->name.c_str() && a_s.node == light->name.c_str()) {
					Write(light, a_s);
				}
			}
		}

		void ApplyEdit(const LightSnap& a_s, bool a_all)
		{
			const auto ref = targetHandle.get();
			if (!ref) {
				return;
			}
			ApplyTo(ref.get(), a_s);
			if (!a_all) {
				return;
			}
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto base = ref->GetBaseObject();
			if (!player || !base) {
				return;
			}
			const auto baseId = base->GetFormID();
			RE::TES::GetSingleton()->ForEachReferenceInRange(player, kApplyAllRange, [&](auto&& a_arg) {
				RE::TESObjectREFR* a_other = Ptr(a_arg);
				if (a_other && a_other != ref.get() && !a_other->IsDisabled() && a_other->GetBaseObject() && a_other->GetBaseObject()->GetFormID() == baseId) {
					ApplyTo(a_other, a_s);
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}

		// ---- Light Placer json index -------------------------------------------------------------------------------------------------------------------

		struct IndexEntry
		{
			std::string              file;
			int                      entry{ 0 };
			std::vector<std::string> models;   // normalised
			std::vector<std::string> formIDs;  // lower case: editor ids or 0xID~Plugin
			std::vector<std::string> nodes;
			std::vector<std::string> lights;
			bool                     conditions{ false };
		};
		std::mutex              indexLock;
		std::vector<IndexEntry> index;
		int                     indexFiles{ 0 };
		bool                    indexed{ false };
		std::vector<std::string> hiddenFiles;  // Data/LightPlacer/**/*.json.hidden: files this plugin switched off

		void BuildIndex()
		{
			namespace fs = std::filesystem;
			std::vector<IndexEntry> list;
			int                     files = 0;
			std::vector<std::string> hidden;
			std::error_code         ec;
			const fs::path          root{ "Data/LightPlacer" };
			if (fs::is_directory(root, ec)) {
				for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
					if (ec) {
						break;
					}
					if (it->is_regular_file(ec) && it->path().extension() == ".hidden" && it->path().stem().extension() == ".json") {
						hidden.push_back(it->path().generic_string());
						continue;
					}
					if (!it->is_regular_file(ec) || it->path().extension() != ".json") {
						continue;
					}
					std::ifstream in(it->path());
					if (!in) {
						continue;
					}
					++files;
					const auto doc = nlohmann::json::parse(in, nullptr, false, true);  // no exceptions, comments allowed
					if (doc.is_discarded() || !doc.is_array()) {
						continue;
					}
					int i = 0;
					for (const auto& e : doc) {
						IndexEntry x;
						x.file = it->path().generic_string();
						x.entry = i++;
						if (!e.is_object()) {
							continue;
						}
						if (e.contains("models") && e["models"].is_array()) {
							for (const auto& m : e["models"]) {
								if (m.is_string()) {
									x.models.push_back(NormModel(m.get<std::string>()));
								}
							}
						}
						for (const char* key : { "formIDs", "visualEffects" }) {
							if (e.contains(key) && e[key].is_array()) {
								for (const auto& m : e[key]) {
									if (m.is_string()) {
										x.formIDs.push_back(Lower(m.get<std::string>()));
									}
								}
							}
						}
						if (e.contains("lights") && e["lights"].is_array()) {
							for (const auto& l : e["lights"]) {
								if (!l.is_object()) {
									continue;
								}
								for (const char* key : { "nodes", "points" }) {
									if (l.contains(key) && l[key].is_array()) {
										for (const auto& n : l[key]) {
											if (n.is_string()) {
												x.nodes.push_back(n.get<std::string>());
											}
										}
									}
								}
								if (l.contains("data") && l["data"].is_object()) {
									const auto& d = l["data"];
									if (d.contains("light") && d["light"].is_string()) {
										x.lights.push_back(d["light"].get<std::string>());
									}
									if (d.contains("conditions")) {
										x.conditions = true;
									}
								}
							}
						}
						list.push_back(std::move(x));
					}
				}
			}
			std::scoped_lock l(indexLock);
			index = std::move(list);
			hiddenFiles = std::move(hidden);
			indexFiles = files;
			indexed = true;
			logger::info("Light Placer index: {} files, {} entries", files, index.size());
		}
	}

	// ---- Phase 3: Light Placer json writer and conflict resolution ----------------------------------------------------------------------------------------

	namespace
	{
		using json = nlohmann::json;
		namespace fs = std::filesystem;

		constexpr auto kBackupDir = "Data/SKSE/Plugins/LoS2Control/backups";
		constexpr auto kManifest = "Data/SKSE/Plugins/LoS2Control/overrides.json";
		constexpr auto kDisabled = "Data/SKSE/Plugins/LoS2Control/disabled.json";  // memory of the files switched off: [ "Data/LightPlacer/.../x.json", ... ]

		std::vector<std::string> LoadDisabled();
		void                     SaveDisabled(const std::vector<std::string>& a_list);

		std::mutex               patchLock;
		std::string              patchText;
		std::vector<std::string> changedCache;
		bool                     changedValid{ false };

		void SetPatchStatus(std::string a_text)
		{
			logger::info("{}", a_text);
			std::scoped_lock l(patchLock);
			patchText = std::move(a_text);
			changedValid = false;
		}

		double Round(float a_v, int a_decimals)
		{
			const double k = std::pow(10.0, a_decimals);
			return std::round(static_cast<double>(a_v) * k) / k;
		}

		std::string FlagNames(std::uint32_t a_flags)
		{
			struct Name
			{
				std::uint32_t bit;
				const char*   text;
			};
			static constexpr Name names[] = {
				{ 1u << 0, "PortalStrict" }, { 1u << 1, "Shadow" }, { 1u << 2, "Simple" }, { 1u << 10, "InverseSquare" }, { 1u << 11, "Linear" },
				{ 1u << 18, "UpdateOnWaiting" }, { 1u << 19, "UpdateOnCellTransition" }, { 1u << 20, "SyncAddonNodes" }, { 1u << 21, "IgnoreScale" },
				{ 1u << 22, "RandomAnimStart" }, { 1u << 23, "NoExternalEmittance" }
			};
			std::string out;
			for (const auto& n : names) {
				if (a_flags & n.bit) {
					out += (out.empty() ? "" : "|");
					out += n.text;
				}
			}
			return out;
		}

		// A missing file is an empty list. A file that exists but is not a json array is an error: it is never overwritten.
		bool LoadArray(const fs::path& a_path, json& a_out, std::string& a_err)
		{
			std::error_code ec;
			if (!fs::exists(a_path, ec)) {
				a_out = json::array();
				return true;
			}
			std::ifstream in(a_path);
			if (!in) {
				a_err = "Cannot open " + a_path.generic_string();
				return false;
			}
			a_out = json::parse(in, nullptr, false, true);
			if (a_out.is_discarded() || !a_out.is_array()) {
				a_err = a_path.generic_string() + " is not a valid Light Placer json (a list of entries)";
				return false;
			}
			return true;
		}

		bool SaveJson(const fs::path& a_path, const json& a_json, std::string& a_err)
		{
			std::error_code ec;
			fs::create_directories(a_path.parent_path(), ec);
			std::ofstream out(a_path, std::ios::trunc);
			if (!out) {
				a_err = "Cannot write " + a_path.generic_string();
				return false;
			}
			out << a_json.dump(2, ' ', false, json::error_handler_t::replace) << '\n';
			out.flush();
			if (!out.good()) {
				a_err = "Writing " + a_path.generic_string() + " failed";
				return false;
			}
			return true;
		}

		// "Meshes\Lanterns\x.nif" -> "Lanterns\x.nif": Light Placer's model names are relative to the meshes folder
		std::string ModelForJson(std::string a_model)
		{
			if (a_model.size() > 7 && (a_model[6] == '\\' || a_model[6] == '/') && Lower(a_model.substr(0, 6)) == "meshes") {
				a_model.erase(0, 7);
			}
			return a_model;
		}

		json Conditions(const SaveOptions& a_opt)
		{
			if (a_opt.schedule == 1) {
				return json::array({ "Self IsInInterior NONE NONE == 0 AND", "Self GetGlobalValue LoS2C_LightsOn NONE == 1" });
			}
			if (a_opt.schedule == 2) {
				return json::array({ "Self GetGlobalValue LoS2C_LightsOn NONE == 1" });
			}
			return json::array();
		}

		std::vector<std::string> LoadDisabled()
		{
			std::vector<std::string> list;
			json                     m;
			std::string              err;
			if (LoadArray(kDisabled, m, err)) {
				for (const auto& e : m) {
					if (e.is_string()) {
						list.push_back(e.get<std::string>());
					}
				}
			}
			return list;
		}

		void SaveDisabled(const std::vector<std::string>& a_list)
		{
			json        m = json::array();
			std::string err;
			for (const auto& e : a_list) {
				m.push_back(e);
			}
			SaveJson(kDisabled, m, err);
		}

		json LoadManifest()
		{
			json        m;
			std::string err;
			if (!LoadArray(kManifest, m, err)) {
				return json::array();
			}
			return m;
		}

		std::string FlatName(std::string a_path)
		{
			std::ranges::replace(a_path, '/', '_');
			std::ranges::replace(a_path, '\\', '_');
			return a_path + ".orig";
		}
	}

	void SavePatch(const SaveOptions& a_opt)
	{
		const auto t = GetTarget();
		if (!t.valid || t.model.empty()) {
			SetPatchStatus("Nothing to save: pick a light first.");
			return;
		}
		json lights = json::array();
		int  skipped = 0;
		std::set<std::string> seen;
		for (const auto& l : t.lights) {
			if (l.lighEdid.empty() || l.attachKind == 0) {
				++skipped;
				continue;
			}
			if (!seen.insert(l.attach + "|" + l.lighEdid).second) {
				continue;  // two entries lit the same node: one is enough in the new file
			}
			json data;
			data["light"] = l.lighEdid;
			data["color"] = json::array({ Round(l.color[0], 3), Round(l.color[1], 3), Round(l.color[2], 3) });
			data["radius"] = Round(l.radius, 1);
			data["size"] = Round(l.size, 3);
			data["cutoff"] = Round(l.cutoff, 4);
			data["fade"] = Round(l.fade, 3);
			if (const auto flags = FlagNames(l.flags); !flags.empty()) {
				data["flags"] = flags;
			}
			if (const auto cond = Conditions(a_opt); !cond.empty()) {
				data["conditions"] = cond;
			}
			json entry;
			if (l.attachKind == 1) {
				entry["nodes"] = json::array({ l.attach });
			} else {
				float x = 0, y = 0, z = 0;
				if (std::sscanf(l.attach.c_str(), "%f,%f,%f", &x, &y, &z) != 3) {
					++skipped;
					continue;
				}
				entry["points"] = json::array({ json::array({ x, y, z }) });
			}
			entry["data"] = std::move(data);
			lights.push_back(std::move(entry));
		}
		if (lights.empty()) {
			SetPatchStatus(std::format("Nothing saved: none of the {} light node(s) could be described (no LIGH record or unknown attach point).", t.lights.size()));
			return;
		}

		const auto  model = ModelForJson(t.model);
		const auto  modelKey = NormModel(t.model);
		json        arr;
		std::string err;
		if (!LoadArray(kPatchFile, arr, err)) {
			SetPatchStatus(err);
			return;
		}
		json entry = { { "models", json::array({ model }) }, { "lights", lights } };
		bool replaced = false;
		for (auto& e : arr) {
			if (e.is_object() && e.contains("models") && e["models"].is_array() && e["models"].size() == 1 && e["models"][0].is_string() &&
				NormModel(e["models"][0].get<std::string>()) == modelKey) {
				e = entry;
				replaced = true;
				break;
			}
		}
		if (!replaced) {
			arr.push_back(entry);
		}
		if (!SaveJson(kPatchFile, arr, err)) {
			SetPatchStatus(err);
			return;
		}
		RescanJson();
		SetPatchStatus(std::format("{} {} light(s) for {} in {}{}. Restart the game for Light Placer to read it. If another file also lights this model, turn that entry off below.",
			replaced ? "Updated" : "Saved", lights.size(), model, kPatchFile, skipped ? std::format(" ({} skipped)", skipped) : std::string{}));
	}

	void RemovePatch()
	{
		const auto t = GetTarget();
		if (!t.valid) {
			SetPatchStatus("Pick a light first.");
			return;
		}
		json        arr;
		std::string err;
		if (!LoadArray(kPatchFile, arr, err)) {
			SetPatchStatus(err);
			return;
		}
		const auto modelKey = NormModel(t.model);
		int        removed = 0;
		for (auto it = arr.begin(); it != arr.end();) {
			if (it->is_object() && it->contains("models") && (*it)["models"].is_array() && (*it)["models"].size() == 1 && (*it)["models"][0].is_string() &&
				NormModel((*it)["models"][0].get<std::string>()) == modelKey) {
				it = arr.erase(it);
				++removed;
			} else {
				++it;
			}
		}
		if (!removed) {
			SetPatchStatus("LoS2Control_Patches.json has no entry for this model.");
			return;
		}
		if (!SaveJson(kPatchFile, arr, err)) {
			SetPatchStatus(err);
			return;
		}
		RescanJson();
		SetPatchStatus(std::format("Removed {} entry from {}. Restart the game for it to take effect.", removed, kPatchFile));
	}

	std::vector<std::string> ChangedFiles()
	{
		// drawn every frame: read the manifest once, again after it changed
		std::scoped_lock l(patchLock);
		if (!changedValid) {
			changedCache.clear();
			for (const auto& m : LoadManifest()) {
				if (m.is_object() && m.contains("file") && m["file"].is_string()) {
					changedCache.push_back(m["file"].get<std::string>());
				}
			}
			changedValid = true;
		}
		return changedCache;
	}

	void RestoreFile(const std::string& a_file)
	{
		auto        manifest = LoadManifest();
		std::string err;
		for (auto it = manifest.begin(); it != manifest.end(); ++it) {
			if (!it->is_object() || it->value("file", "") != a_file) {
				continue;
			}
			std::error_code ec;
			const fs::path  backup{ it->value("backup", "") };
			fs::copy_file(backup, fs::path(a_file), fs::copy_options::overwrite_existing, ec);
			if (ec) {
				SetPatchStatus("Could not put " + a_file + " back (" + ec.message() + ").");
				return;
			}
			fs::remove(backup, ec);
			manifest.erase(it);
			SaveJson(kManifest, manifest, err);
			RescanJson();
			SetPatchStatus("Put back " + a_file + ". Restart the game for it to take effect.");
			return;
		}
	}

	void DisableFile(const std::string& a_file)
	{
		namespace fs = std::filesystem;
		if (a_file == kPatchFile || a_file.size() < 6 || a_file.substr(a_file.size() - 5) != ".json") {
			SetPatchStatus("Only another mod's .json file under Data/LightPlacer can be disabled.");
			return;
		}
		const fs::path  from{ a_file };
		const fs::path  to{ a_file + ".hidden" };
		std::error_code ec;
		if (fs::exists(to, ec)) {
			SetPatchStatus(to.generic_string() + " already exists: re-enable or delete it first.");
			return;
		}
		fs::rename(from, to, ec);
		if (ec) {
			SetPatchStatus("Could not disable " + a_file + " (" + ec.message() + ").");
			return;
		}
		// the mod manager's virtual file system decides where the rename lands: check that the original is really gone
		if (fs::exists(from, ec) || !fs::exists(to, ec)) {
			fs::rename(to, from, ec);
			SetPatchStatus("The mod manager did not let " + a_file + " be renamed, nothing was changed. Disable that file in your mod manager instead.");
			return;
		}
		{
			auto list = LoadDisabled();
			if (std::ranges::find(list, a_file) == list.end()) {
				list.push_back(a_file);
				SaveDisabled(list);
			}
		}
		RescanJson();
		SetPatchStatus("Disabled " + a_file + " by renaming it to " + a_file + ".hidden. This switches off EVERY light that file defines, not only the picked object. Takes effect at the next game load; use 'Re-enable' to undo.");
	}

	void EnableFile(const std::string& a_hidden)
	{
		namespace fs = std::filesystem;
		const std::string suffix = ".hidden";
		if (a_hidden.size() <= suffix.size() || a_hidden.substr(a_hidden.size() - suffix.size()) != suffix) {
			return;
		}
		const std::string orig = a_hidden.substr(0, a_hidden.size() - suffix.size());
		const fs::path    to{ orig };
		std::error_code   ec;
		const auto        forget = [&]() {
            auto list = LoadDisabled();
            std::erase(list, orig);
            SaveDisabled(list);
		};
		if (fs::exists(to, ec)) {
			if (!fs::exists(fs::path(a_hidden), ec)) {
				forget();  // it is back already (put back by hand or by the mod manager)
				SetPatchStatus(orig + " is already in place. Removed it from the list.");
			} else {
				SetPatchStatus(orig + " already exists, so " + a_hidden + " cannot be switched back on.");
			}
			return;
		}
		if (!fs::exists(fs::path(a_hidden), ec)) {
			forget();
			SetPatchStatus(a_hidden + " no longer exists. Removed it from the list.");
			return;
		}
		fs::rename(fs::path(a_hidden), to, ec);
		if (ec) {
			SetPatchStatus("Could not re-enable " + a_hidden + " (" + ec.message() + ").");
			return;
		}
		forget();
		RescanJson();
		SetPatchStatus("Re-enabled " + to.generic_string() + ". Takes effect at the next game load.");
	}

	std::vector<std::string> HiddenFiles()
	{
		// drawn every frame: read the memory file at most once a second
		static std::chrono::steady_clock::time_point lastRead{};
		static std::vector<std::string>              cache;
		const auto now = std::chrono::steady_clock::now();
		if (now - lastRead < std::chrono::seconds(1)) {
			return cache;
		}
		lastRead = now;

		// the remembered list first (the scan can miss a file the mod manager put in another folder), then anything the scan found
		std::vector<std::string> out;
		for (const auto& f : LoadDisabled()) {
			out.push_back(f + ".hidden");
		}
		std::scoped_lock l(indexLock);
		for (const auto& f : hiddenFiles) {
			if (std::ranges::find(out, f) == out.end()) {
				out.push_back(f);
			}
		}
		cache = out;
		return out;
	}

	std::string PatchStatus()
	{
		std::scoped_lock l(patchLock);
		return patchText;
	}

	void Tick()
	{
		if (!pickRequested.exchange(false)) {
			// keep the snapshot of the picked object fresh
			const auto ref = targetHandle.get();
			if (ref) {
				auto snap = Snapshot(ref.get(), 0.0f);
				std::scoped_lock l(lock);
				if (target.valid) {
					snap.serial = target.serial;
					snap.distance = target.distance;
					target = std::move(snap);
				}
			}
			return;
		}
		float      dist = 0.0f;
		const auto ref = PickRef(dist, false);
		std::scoped_lock l(lock);
		originals.clear();
		if (!ref) {
			target = TargetSnap{};
			targetHandle = {};
			status = "No light found in front of you (within 2500 units, about 20 degrees around the view). Look at a lantern and try again.";
			return;
		}
		targetHandle = ref->GetHandle();
		target = Snapshot(ref, dist);
		target.serial = ++serial;
		for (const auto& s : target.lights) {
			originals[s.node] = s;
		}
		status = std::format("Picked {} ({:08X}), {} light(s), {:.0f} units away.", target.baseEdid.empty() ? target.name : target.baseEdid, target.refId, target.lights.size(), dist);
		logger::info("{}", status);
	}

	void RequestPick()
	{
		pickRequested = true;
	}

	TargetSnap GetTarget()
	{
		std::scoped_lock l(lock);
		return target;
	}

	std::string StatusLine()
	{
		std::scoped_lock l(lock);
		return status;
	}

	void QueueEdit(const LightSnap& a_edit, bool a_applyAll)
	{
		if (const auto task = SKSE::GetTaskInterface()) {
			task->AddTask([edit = a_edit, a_applyAll]() { ApplyEdit(edit, a_applyAll); });
		}
	}

	void QueueRevert(const std::string& a_node, bool a_applyAll)
	{
		LightSnap original;
		{
			std::scoped_lock l(lock);
			const auto       it = originals.find(a_node);
			if (it == originals.end()) {
				return;
			}
			original = it->second;
		}
		QueueEdit(original, a_applyAll);
	}

	void RescanJson()
	{
		// reading files does not need the main thread, but must not block the render thread that calls this
		std::thread([]() { BuildIndex(); }).detach();
	}

	Matches FindMatches()
	{
		Matches out;
		if (!indexed) {
			return out;
		}
		TargetSnap t = GetTarget();
		if (!t.valid) {
			return out;
		}
		const auto model = NormModel(t.model);
		const auto edid = Lower(t.baseEdid);
		std::scoped_lock l(indexLock);
		out.filesScanned = indexFiles;
		out.entriesScanned = static_cast<int>(index.size());
		for (const auto& e : index) {
			std::string by;
			if (!model.empty() && std::ranges::find(e.models, model) != e.models.end()) {
				by = "model";
			} else if (!edid.empty() && std::ranges::find(e.formIDs, edid) != e.formIDs.end()) {
				by = "formID";
			}
			if (by.empty()) {
				continue;
			}
			out.list.push_back({ e.file, e.entry, by, e.nodes, e.lights, e.conditions });
		}
		// the same node lit by more than one entry: each of them adds its own bulb
		std::map<std::string, std::vector<std::string>> byNode;
		for (const auto& m : out.list) {
			for (const auto& n : m.nodes) {
				byNode[Lower(n)].push_back(std::format("{} #{}", std::filesystem::path(m.file).filename().string(), m.entry));
			}
		}
		for (const auto& [node, where] : byNode) {
			if (where.size() > 1) {
				std::string line = std::format("Node '{}' is lit by {} entries:", node, where.size());
				for (const auto& w : where) {
					line += " " + w + ";";
				}
				out.conflicts.push_back(std::move(line));
			}
		}
		return out;
	}
}
