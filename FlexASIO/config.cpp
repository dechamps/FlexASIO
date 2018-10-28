#define _CRT_SECURE_NO_WARNINGS  // Avoid issues with toml.h

#include "config.h"

#include <filesystem>

#include <toml/toml.h>

#include "log.h"
#include "../FlexASIOUtil/find.h"
#include "../FlexASIOUtil/shell.h"

namespace flexasio {

	namespace {

		std::optional<toml::Value> LoadConfigToml() {
			const auto userDirectory = GetUserDirectory();
			if (!userDirectory.has_value()) {
				Log() << "Unable to determine user directory for configuration file";
				return toml::Table();
			}

			std::filesystem::path path(*userDirectory);
			path.append("FlexASIO.toml");

			Log() << "Attempting to load configuration file: " << path;

			try {
				std::ifstream stream;
				stream.exceptions(stream.badbit | stream.failbit);
				try {
					stream.open(path);
				}
				catch (const std::exception& exception) {
					Log() << "Unable to open configuration file: " << exception.what();
					return toml::Table();
				}
				stream.exceptions(stream.badbit);

				const auto parseResult = toml::parse(stream);
				if (!parseResult.valid()) {
					Log() << "Unable to parse configuration file as TOML: " << parseResult.errorReason;
					return std::nullopt;
				}

				Log() << "Configuration file successfully parsed as valid TOML";

				return parseResult.value;
			}
			catch (const std::exception& exception) {
				Log() << "Unable to read configuration file: " << exception.what();
				return std::nullopt;
			}
		}
	}

	std::optional<Config> LoadConfig() {
		const auto tomlValue = LoadConfigToml();
		if (!tomlValue.has_value()) return std::nullopt;

		Config config;

		const auto backend = tomlValue->find("backend");
		if (backend != nullptr) {
			if (!backend->is<std::string>()) {
				Log() << "Configuration error: backend value must be a string";
				return std::nullopt;
			}
			config.backend = backend->as<std::string>();
		}

		const auto wasapiExclusiveMode = tomlValue->find("wasapiExclusiveMode");
		if (wasapiExclusiveMode != nullptr) {
			if (!wasapiExclusiveMode->is<bool>()) {
				Log() << "Configuration error: wasapiExclusiveMode value must be a bool";
				return std::nullopt;
			}
			config.wasapiExclusiveMode = wasapiExclusiveMode->as<bool>();
		}

		return config;
	}

}