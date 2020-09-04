#pragma once

#include <optional>
#include <string>

namespace flexasio {

	struct Config {
		std::optional<std::string> backend;
		std::optional<int64_t> bufferSizeSamples;

		struct Stream {
			std::optional<std::string> device;
			std::optional<int> channels;
			std::optional<std::string> sampleType;
			std::optional<double> suggestedLatencySeconds;
			bool wasapiExclusiveMode = false;
			bool wasapiAutoConvert = true;
		};
		Stream input;
		Stream output;
	};

	Config LoadConfig();

}