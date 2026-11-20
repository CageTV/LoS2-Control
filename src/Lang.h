#pragma once

// Interface language. It follows the game's language (sLanguage in Skyrim.ini), or Settings::language when that is set.
// Every text is looked up by its English wording in Data/SKSE/Plugins/LoS2Control/lang/<language>.json (UTF-8, one object: { "English text": "translation" }).
// A missing file or a missing key leaves the English text, so a partial translation is fine.
// Placeholders ({} and %s) must stay in the translation, in a form the same code can fill.
namespace Lang
{
	// The translation of a_english, or a_english itself. The pointer stays valid for the rest of the session.
	const char* T(const char* a_english);

	// "english", "japanese", ... the language in use
	std::string Name();
	// How many texts the language file gave
	int Count();

	// T() followed by std::format. A translation with a broken placeholder falls back to the English text instead of failing.
	template <class... Args>
	std::string F(const char* a_english, const Args&... a_args)
	{
		try {
			return std::vformat(T(a_english), std::make_format_args(a_args...));
		} catch (...) {
			return std::vformat(a_english, std::make_format_args(a_args...));
		}
	}

	// "a|b|c" -> "A\0B\0C\0" with every item translated: the item list of a combo box. The result must outlive the call that uses it.
	std::string Items(const char* a_english_pipe_separated);
}
