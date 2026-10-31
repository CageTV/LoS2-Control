#include "Menu.h"

#include "Editor.h"
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
				SetTooltip("%s", a_text);
			}
		}

		std::uint32_t                  pickedSerial = 0;
		std::vector<Editor::LightSnap> edits;
		bool                           applyAll = false;
		std::uint32_t                  addedSerial = 0;

		struct Pending
		{
			Editor::NewLight l;
			char             buf[128]{};
		};

		bool FlagBox(const char* a_label, Editor::LightSnap& a_s, std::uint32_t a_bit, const char* a_tip)
		{
			bool v = (a_s.flags & a_bit) != 0;
			const bool changed = Checkbox(a_label, &v);
			Tip(a_tip);
			if (changed) {
				a_s.flags = v ? (a_s.flags | a_bit) : (a_s.flags & ~a_bit);
			}
			return changed;
		}

		void __stdcall RenderEditor()
		{
			Logo();
			SeparatorText("Pick a light");
			TextWrapped("%s", Editor::StatusLine().c_str());
			if (Button("Pick the light I am looking at")) {
				Editor::RequestPick();
			}
			SameLine();
			if (Button("Pick the object I am looking at (even without a light)")) {
				Editor::RequestPickAny();
			}
			Tip("For adding a light to an object that has none. Picks the object whose origin is closest to where the camera points.");
			Tip("Searches within 2500 units for the light closest to where the camera points. Works on any object whose model carries a light: Light Placer lights, ISL Helper converted lights and placed vanilla lights.");
			SameLine();
			if (Button("Scan Light Placer files")) {
				Editor::RescanJson();
			}
			Tip("Reads Data/LightPlacer/**/*.json so the list below can show which files define the picked object. Press it again after changing files.");
			Checkbox("Edits also change all loaded copies of this object", &applyAll);
			Tip("Within 6000 units of you. Useful to see a change on every lantern of a street at once.");

			const auto t = Editor::GetTarget();
			if (!t.valid) {
				return;
			}
			if (t.serial != pickedSerial) {
				pickedSerial = t.serial;
				edits = t.lights;
			}

			SeparatorText("Object");
			Text("%s", t.baseEdid.empty() ? "(no editor id)" : t.baseEdid.c_str());
			if (!t.name.empty()) {
				SameLine();
				TextDisabled("\"%s\"", t.name.c_str());
			}
			TextDisabled("Reference %08X, base %08X", t.refId, t.baseId);
			TextWrapped("Model: %s", t.model.empty() ? "-" : t.model.c_str());

			SeparatorText("Lights on it");
			if (t.lights.empty()) {
				TextDisabled("This object has no light nodes in its loaded model.");
			}
			for (std::size_t i = 0; i < t.lights.size() && i < edits.size(); ++i) {
				auto& e = edits[i];
				PushID(std::to_string(i).c_str());
				const std::string head = std::format("{}  ({})", e.node.empty() ? "(unnamed node)" : e.node, e.lighEdid.empty() ? "no LIGH" : e.lighEdid);
				if (CollapsingHeader(head.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
					bool c = false;
					c |= SliderFloat("Radius", &e.radius, 0.0f, 3000.0f, "%.0f");
					Tip("Reach of the light in game units.");
					c |= SliderFloat("Size", &e.size, 0.01f, 50.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
					Tip("Inverse square size: how large the emitting source is. Small = tight bright hot spot, large = softer.");
					c |= SliderFloat("Cutoff", &e.cutoff, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
					Tip("The light level at which the reach ends. Lower = flatter curve that carries further.");
					c |= SliderFloat("Fade (intensity)", &e.fade, 0.0f, 20.0f, "%.2f");
					c |= ColorEdit3("Colour", e.color);
					c |= FlagBox("Inverse square", e, Editor::Flag::InverseSquare, "Community Shaders inverse square falloff.");
					SameLine();
					c |= FlagBox("Linear", e, Editor::Flag::Linear, "Linear falloff (Light Limit Fix).");
					c |= FlagBox("Shadow", e, Editor::Flag::Shadow, "Casts shadows. Community Shaders limits these to 4 per cell.");
					SameLine();
					c |= FlagBox("Portal strict", e, Editor::Flag::PortalStrict, "Does not light through portals (interior rooms).");
					c |= FlagBox("Ignore scale", e, Editor::Flag::IgnoreScale, "Ignores the object's scale.");
					SameLine();
					c |= FlagBox("No external emittance", e, Editor::Flag::NoExternalEmittance, "Does not follow the time of day colour.");
					if (e.cull != 0) {
						TextDisabled("Hidden by Light Placer (cull bits 0x%02X): conditions, the game or a script. Edits are kept for when it shows again.", e.cull);
					}
					TextDisabled("LIGH record %08X: %s", e.ligh, e.lighIsl ? "carries the inverse square flag (converted by ISL Helper or the author)" : "vanilla falloff");
					if (c) {
						Editor::QueueEdit(e, applyAll);
					}
					if (Button("Revert to the values at pick time")) {
						edits[i] = t.lights[i];
						Editor::QueueRevert(e.node, applyAll);
					}
				}
				PopID();
			}

			SeparatorText("Write it as Light Placer json");
			static int schedule = 0;
			Combo("When is it lit", &schedule, "Always on\0On the lantern schedule, outdoors only\0On the lantern schedule, indoors too\0");
			Tip("Always on: no condition, a permanent light. The schedule options add the condition 'LoS2C_LightsOn = 1', so the settings on the first page (hours, sun, weather) drive the light. Outdoors only also adds 'not interior'. Applies to the Save buttons in this section and in 'Add a light'.");
			if (Button("Save to LoS2Control_Patches.json")) {
				Editor::SaveOptions o;
				o.schedule = schedule;
				Editor::SavePatch(o);
			}
			Tip("Writes all the lights shown above, with the values in the sliders, as one entry for this object's model in Data/LightPlacer/LoS2Control/LoS2Control_Patches.json. Light Placer reads it at the next game start. ISL Helper leaves lights alone that a Light Placer file names.");
			SameLine();
			if (Button("Remove my entry for this model")) {
				Editor::RemovePatch();
			}

			SeparatorText("Add a light to this object");
			static std::vector<Pending> added;
			if (t.serial != addedSerial) {
				addedSerial = t.serial;
				added.clear();
			}
			if (Button("Add a light")) {
				Pending p;
				std::snprintf(p.buf, sizeof(p.buf), "%s", "LOS_Candle");
				p.l.ligh = p.buf;
				Editor::LightDefaults(p.l.ligh, p.l);
				added.push_back(p);
			}
			Tip("A light that the object does not have yet. It is created by Light Placer at the next game start (no live preview).");
			for (std::size_t i = 0; i < added.size(); ++i) {
				auto& p = added[i];
				PushID(static_cast<int>(1000 + i));
				Separator();
				if (InputText("Light record (EditorID)", p.buf, sizeof(p.buf))) {
					p.l.ligh = p.buf;
				}
				Tip("The LIGH record the light is based on. It gives the default look; the values below replace it. Use one that exists in your load order, e.g. LOS_Candle.");
				if (Button("Take radius, colour and fade from the record")) {
					if (!Editor::LightDefaults(p.l.ligh, p.l)) {
						p.l.ligh += "  (not found)";
						std::snprintf(p.buf, sizeof(p.buf), "%s", p.l.ligh.c_str());
					}
				}
				bool atNode = p.l.attachKind == 1;
				if (Checkbox("Attach to a node of the model", &atNode)) {
					p.l.attachKind = atNode ? 1 : 2;
				}
				if (atNode) {
					if (BeginCombo("Node", p.l.node.empty() ? "(choose)" : p.l.node.c_str())) {
						for (const auto& n : t.nodes) {
							if (Selectable(n.c_str(), n == p.l.node)) {
								p.l.node = n;
							}
						}
						EndCombo();
					}
				} else {
					DragFloat3("Position (x y z)", p.l.point, 0.5f, -1000.0f, 1000.0f, "%.1f");
					Tip("Offset from the origin of the model, in game units: x right, y forward, z up.");
				}
				SliderFloat("Radius", &p.l.radius, 0.0f, 3000.0f, "%.0f");
				SliderFloat("Size", &p.l.size, 0.01f, 50.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
				SliderFloat("Cutoff", &p.l.cutoff, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
				SliderFloat("Fade (intensity)", &p.l.fade, 0.0f, 20.0f, "%.2f");
				ColorEdit3("Colour", p.l.color);
				const auto bit = [&](const char* a_label, std::uint32_t a_bit) {
					bool v = (p.l.flags & a_bit) != 0;
					if (Checkbox(a_label, &v)) {
						p.l.flags = v ? (p.l.flags | a_bit) : (p.l.flags & ~a_bit);
					}
				};
				bit("Inverse square", Editor::Flag::InverseSquare);
				SameLine();
				bit("Linear", Editor::Flag::Linear);
				bit("Shadow", Editor::Flag::Shadow);
				SameLine();
				bit("Portal strict", Editor::Flag::PortalStrict);
				bit("Ignore scale", Editor::Flag::IgnoreScale);
				SameLine();
				bit("No external emittance", Editor::Flag::NoExternalEmittance);
				if (Button("Remove this light")) {
					added.erase(added.begin() + static_cast<std::ptrdiff_t>(i));
					PopID();
					break;
				}
				PopID();
			}
			if (!added.empty() && Button("Save the new lights")) {
				std::vector<Editor::NewLight> list;
				for (const auto& p : added) {
					list.push_back(p.l);
				}
				Editor::SaveOptions o;
				o.schedule = schedule;
				Editor::SaveNewLights(list, o);
			}

			if (const auto ps = Editor::PatchStatus(); !ps.empty()) {
				TextWrapped("%s", ps.c_str());
			}

			SeparatorText("Light Placer files that define it");
			const auto m = Editor::FindMatches();
			if (m.entriesScanned == 0) {
				TextDisabled("Press 'Scan Light Placer files' first.");
			} else {
				TextDisabled("%d entries in %d files scanned", m.entriesScanned, m.filesScanned);
				if (m.list.empty()) {
					TextWrapped("No Light Placer entry names this model or object. Its lights come from somewhere else (ISL Helper, a placed light or the model itself).");
				}
				for (const auto& x : m.list) {
					std::string nodes;
					for (const auto& n : x.nodes) {
						nodes += (nodes.empty() ? "" : ", ") + n;
					}
					std::string lights;
					for (const auto& n : x.lights) {
						lights += (lights.empty() ? "" : ", ") + n;
					}
					BulletText("%s  #%d  (by %s)%s", x.file.c_str(), x.entry, x.by.c_str(), x.conditions ? "  [conditions]" : "");
					if (x.file != Editor::kPatchFile) {
						PushID((x.file + "#" + std::to_string(x.entry)).c_str());
						SameLine();
						if (SmallButton("Turn off for this object")) {
							Editor::DisableMatch(x.file, x.entry, x.by);
						}
						Tip("Takes this object out of that entry in a copy of the file (the original is kept and can be put back). Use it when a second file also lights the object.");
						PopID();
					}
					TextDisabled("    nodes: %s | lights: %s", nodes.empty() ? "-" : nodes.c_str(), lights.empty() ? "-" : lights.c_str());
				}
				for (const auto& c : m.conflicts) {
					TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", c.c_str());
				}
				if (!m.conflicts.empty()) {
					TextDisabled("Each entry adds its own bulb: turn the others off for this object once ours is saved.");
				}
			}

			const auto changed = Editor::ChangedFiles();
			if (!changed.empty()) {
				SeparatorText("Files I changed");
				for (const auto& f : changed) {
					PushID(f.c_str());
					TextWrapped("%s", f.c_str());
					SameLine();
					if (SmallButton("Put back")) {
						Editor::RestoreFile(f);
					}
					PopID();
				}
			}
		}

		void __stdcall RenderSettings()
		{
			auto& s = Settings::Get();
			bool  c = false;

			Logo();

			SeparatorText("When the lanterns are lit");
			c |= Combo("Mode", &s.mode, "Fixed hours\0Follow the sun\0");
			Tip("Fixed hours uses the two times below. Follow the sun uses the current climate's sunset and sunrise.");
			if (s.mode == Settings::kFixedHours) {
				c |= SliderFloat("Lit from", &s.onHour, 12.0f, 24.0f, "%.2f h");
				c |= SliderFloat("Off at", &s.offHour, 0.0f, 12.0f, "%.2f h");
			} else {
				c |= SliderFloat("After sunset begins", &s.sunsetOffsetMinutes, -180.0f, 180.0f, "%+.0f min");
				c |= SliderFloat("After sunrise ends", &s.sunriseOffsetMinutes, -180.0f, 180.0f, "%+.0f min");
			}
			c |= Checkbox("Gloomy weather keeps them lit by day", &s.weatherKeepsOn);
			Tip("Fog, rain, storm, snow and ash, from the list in LoS2Control_Weathers.txt.");
			c |= Checkbox("Always on", &s.alwaysOn);
			c |= Checkbox("Always off", &s.alwaysOff);

			SeparatorText("Weather list");
			if (Button("Add the current weather")) {
				Schedule::AddCurrentWeather();
			}
			Tip("For a weather mod that adds new weathers: stand in the weather, press this.");
			SameLine();
			if (Button("Reload the list")) {
				Schedule::ReloadWeathers();
			}

			SeparatorText("Lanterns of Skyrim II");
			c |= Checkbox("Keep its own MCM at 'Always off'", &s.pinLoS2);
			Tip("The ISL bulbs come from Light Placer, so the original lantern lights must stay off.");

			SeparatorText("Now");
			const auto st = Schedule::GetStatus();
			Text("Hour %s, lit from %s to %s (%s)", Clock(st.hour).c_str(), Clock(st.onAt).c_str(), Clock(st.offAt).c_str(), st.night ? "night" : "day");
			Text("Weather: %s%s", st.weather.empty() ? "-" : st.weather.c_str(), st.weatherOn ? "  (keeps the lanterns lit)" : "");
			Text("Lanterns: %s", st.lit ? "ON" : "off");
			if (!st.globalFound) {
				TextDisabled("%s was not found: it has to exist in LoS2-DLL.esp", s.lightsGlobal);
			}
			if (s.pinLoS2 && st.pinnedFound < 2) {
				TextDisabled("Lanterns of Skyrim II control globals found: %d of 2", st.pinnedFound);
			}

			Spacing();
			if (Button("Save now")) {
				s.Clamp();
				s.Save();
				Schedule::SettingsChanged();
			}
			SameLine();
			if (Button("Reload from disk")) {
				s.Load();
				Schedule::SettingsChanged();
			}
			if (c) {
				s.Clamp();
				Schedule::SettingsChanged();
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
		SKSEMenuFramework::AddSectionItem("Lanterns", RenderSettings);
		SKSEMenuFramework::AddSectionItem("Light editor", RenderEditor);
		logger::info("Registered the settings page with the SKSE Menu Framework");
	}
}
