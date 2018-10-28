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

		template <typename T> struct TomlValueTraits;
		template <> struct TomlValueTraits<bool> {
			constexpr static toml::Value::Type type = toml::Value::BOOL_TYPE;
		};
		template <> struct TomlValueTraits<int64_t> {
			constexpr static toml::Value::Type type = toml::Value::INT_TYPE;
		};
		template <> struct TomlValueTraits<int> {
			constexpr static toml::Value::Type type = toml::Value::INT_TYPE;
		};
		template <> struct TomlValueTraits<double> {
			constexpr static toml::Value::Type type = toml::Value::DOUBLE_TYPE;
		};
		template <> struct TomlValueTraits<std::string> {
			constexpr static toml::Value::Type type = toml::Value::STRING_TYPE;
		};
		template <> struct TomlValueTraits<toml::Time> {
			constexpr static toml::Value::Type type = toml::Value::TIME_TYPE;
		};
		template <> struct TomlValueTraits<toml::Array> {
			constexpr static toml::Value::Type type = toml::Value::ARRAY_TYPE;
		};
		template <> struct TomlValueTraits<toml::Table> {
			constexpr static toml::Value::Type type = toml::Value::TABLE_TYPE;
		};

		constexpr const std::pair<toml::Value::Type, std::string_view> tomlTypes[] = {
			{toml::Value::NULL_TYPE, "NULL"},
			{toml::Value::BOOL_TYPE, "BOOL"},
			{toml::Value::INT_TYPE, "INT"},
			{toml::Value::DOUBLE_TYPE, "DOUBLE"},
			{toml::Value::STRING_TYPE, "STRING"},
			{toml::Value::TIME_TYPE, "TIME"},
			{toml::Value::ARRAY_TYPE, "ARRAY"},
			{toml::Value::TABLE_TYPE, "TABLE"},
		};

		template <typename T> auto GetValue(const toml::Value& table, const std::string& key) -> std::optional<T> {
			const auto option = table.find(key);
			if (option == nullptr) return std::nullopt;
			if (!option->is<T>()) {
				std::stringstream error;
				error << "configuration option '" << key << "' is of type " << Find(option->type(), tomlTypes).value_or("(unknown)") << ", expected " << Find(TomlValueTraits<T>::type, tomlTypes).value_or("(unknown)");
				throw std::runtime_error(error.str());
			}
			return option->as<T>();
		}

		template <typename T> struct RemoveOptional { using Value = T; };
		template <typename T> struct RemoveOptional<std::optional<T>> { using Value = T; };

		template <typename T> void SetOption(const toml::Value& table, const std::string& key, T& option) {
			const auto value = GetValue<RemoveOptional<T>::Value>(table, key);
			if (!value.has_value()) return;
			option = *value;
		}

	}

	std::optional<Config> LoadConfig() {
		const auto tomlValue = LoadConfigToml();
		if (!tomlValue.has_value()) return std::nullopt;

		Config config;

		try {
			SetOption(*tomlValue, "backend", config.backend);
			SetOption(*tomlValue, "wasapiExclusiveMode", config.wasapiExclusiveMode);
		}
		catch (const std::exception& exception) {
			Log() << "Invalid configuration: " << exception.what();
			return std::nullopt;
		}

		return config;
	}

}