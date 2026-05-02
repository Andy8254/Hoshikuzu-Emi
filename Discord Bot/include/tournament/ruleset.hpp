#pragma once
#include "tournament/tournament/MatchRules.hpp"
#include <optional>
#include <string>

namespace tournament_ruleset {
	enum class RulesetScope {
		PRIMARY,
		SECONDARY
	};

	enum class SecondaryTrigger {
		NONE,
		TOP_8,
		GRAND_FINALS
	};

	struct RulesetConfig {
		int tournament_id = 0;
		RulesetScope scope = RulesetScope::PRIMARY;
		SecondaryTrigger trigger = SecondaryTrigger::NONE;
		MatchRules rules = fallback_rules();
	};

	bool init();

	bool set_ruleset(const RulesetConfig& config);
	bool clear_secondary_ruleset(int tournament_id);

	std::optional<RulesetConfig> get_ruleset(int tournament_id, RulesetScope scope);
	RulesetConfig get_effective_primary_ruleset(int tournament_id);

	std::string to_string(RulesetScope scope);
	std::string to_string(SecondaryTrigger trigger);
	std::string to_string(DeuceMode mode);

	std::optional<SecondaryTrigger> parse_secondary_trigger(const std::string& value);
	std::optional<DeuceMode> parse_deuce_mode(const std::string& value);
	std::string describe_ruleset(const RulesetConfig& config);
}
