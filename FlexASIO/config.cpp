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

		template <typename Functor> void ProcessOption(const toml::Table& table, const std::string& key, Functor functor) {
			const auto value = table.find(key);
			if (value == table.end()) return;
			try {
				return functor(value->second);
			}
			catch (const std::exception& exception) {
				throw std::runtime_error(std::string("in option '") + key + "': " + exception.what());
			}
		}

		template <typename T, typename Functor> void ProcessTypedOption(const toml::Table& table, const std::string& key, Functor functor) {
			return ProcessOption(table, key, [&](const toml::Value& value) { return functor(value.as<T>()); });
		}

		template <typename T> struct RemoveOptional { using Value = T; };
		template <typename T> struct RemoveOptional<std::optional<T>> { using Value = T; };

		template <typename T> void SetOption(const toml::Table& table, const std::string& key, T& option) {
			ProcessTypedOption<RemoveOptional<T>::Value>(table, key, [&](const RemoveOptional<T>::Value& value) {
				option = value;
			});
		}

		void SetStream(const toml::Table& table, Config::Stream& stream) {
			SetOption(table, "wasapiExclusiveMode", stream.wasapiExclusiveMode);
		}

		void SetConfig(const toml::Table& table, Config& config) {
			SetOption(table, "backend", config.backend);
			ProcessTypedOption<toml::Table>(table, "input", [&](const toml::Table& table) { SetStream(table, config.input); });
			ProcessTypedOption<toml::Table>(table, "output", [&](const toml::Table& table) { SetStream(table, config.output); });
		}

	}

	std::optional<Config> LoadConfig() {
		const auto tomlValue = LoadConfigToml();
		if (!tomlValue.has_value()) return std::nullopt;

		try {
			Config config;
			SetConfig(tomlValue->as<toml::Table>(), config);
			return config;
		}
		catch (const std::exception& exception) {
			Log() << "Invalid configuration: " << exception.what();
			return std::nullopt;
		}
	}

}