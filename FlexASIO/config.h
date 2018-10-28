#pragma once

#include <optional>
#include <string>

namespace flexasio {

	struct Config {
		std::optional<std::string> backend;
		bool wasapiExclusiveMode = false;
	};

	std::optional<Config> LoadConfig();

}