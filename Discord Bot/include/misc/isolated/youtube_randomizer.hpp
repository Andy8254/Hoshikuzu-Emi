#ifndef MISC_ISOLATED_YOUTUBE_RANDOMIZER_HPP
#define MISC_ISOLATED_YOUTUBE_RANDOMIZER_HPP

#include <optional>
#include <string>
#include <vector>

namespace misc_isolated_youtube_randomizer {

	struct MusicLink {
		int id = 0;
		std::string guild_id;
		std::string title;
		std::string youtube_url;
		std::string added_by;
		int added_at = 0;
		bool enabled = true;
	};

	bool enabled();
	bool init();
	bool add_link(
		const std::string& guild_id,
		const std::string& title,
		const std::string& youtube_url,
		const std::string& added_by,
		int added_at
	);
	std::vector<MusicLink> list_links(const std::string& guild_id, bool include_disabled = false);
	std::optional<MusicLink> random_link(const std::string& guild_id);
	bool set_link_enabled(const std::string& guild_id, int id, bool link_enabled);

}

#endif
