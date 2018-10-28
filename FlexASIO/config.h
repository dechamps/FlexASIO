#pragma once

#include <optional>
#include <string>

namespace flexasio {

	struct Config {
		std::optional<std::string> backend;
	};

	std::optional<Config> LoadConfig();

}