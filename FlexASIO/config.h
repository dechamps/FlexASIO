#pragma once

#include <optional>
#include <string>

namespace flexasio {

	struct Config {
		std::optional<std::string> backend;

		struct Stream {
			bool wasapiExclusiveMode = false;
		};
		Stream input;
		Stream output;
	};

	std::optional<Config> LoadConfig();

}