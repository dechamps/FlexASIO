#define _CRT_SECURE_NO_WARNINGS  // Avoid issues with toml.h

#include "config.h"

#include <toml/toml.h>

#include "log.h"
#include "../FlexASIOUtil/shell.h"

namespace flexasio {

	namespace {

		constexpr auto configFileName = L"FlexASIO.toml";

		toml::Value LoadConfigToml(const std::filesystem::path& path) {
			Log() << "Attempting to load configuration file: " << path;

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

			const auto parseResult = [&] {
				try {
					const auto parseResult = toml::parse(stream);
					if (!parseResult.valid()) throw std::runtime_error(parseResult.errorReason);
					return parseResult;
				}
				catch (...) {
					std::throw_with_nested(std::runtime_error("TOML parse error"));
				}
			}();

			Log() << "Configuration file successfully parsed as valid TOML: " << parseResult.value;

			return parseResult.value;
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

		template <typename T, typename Validator> void SetOption(const toml::Table& table, const std::string& key, T& option, Validator validator) {
			ProcessTypedOption<RemoveOptional<T>::Value>(table, key, [&](const RemoveOptional<T>::Value& value) {
				validator(value);
				option = value;
			});
		}
		template <typename T> void SetOption(const toml::Table& table, const std::string& key, T& option) {
			return SetOption(table, key, option, [](const T&) {});
		}

		void ValidateChannelCount(const int& channelCount) {
			if (channelCount <= 0) throw std::runtime_error("channel count must be strictly positive - to disable a stream direction, set the 'device' option to the empty string \"\" instead");
		}

		void ValidateSuggestedLatency(const double& suggestedLatencySeconds) {
			if (!(suggestedLatencySeconds >= 0 && suggestedLatencySeconds <= 3600)) throw std::runtime_error("suggested latency must be between 0 and 3600 seconds");
		}

		void ValidateBufferSize(const int64_t& bufferSizeSamples) {
			if (bufferSizeSamples <= 0) throw std::runtime_error("buffer size must be strictly positive");
			if (bufferSizeSamples >= (std::numeric_limits<long>::max)()) throw std::runtime_error("buffer size is too large");
		}

		void SetStream(const toml::Table& table, Config::Stream& stream) {
			SetOption(table, "device", stream.device);
			SetOption(table, "channels", stream.channels, ValidateChannelCount);
			SetOption(table, "sampleType", stream.sampleType);
			SetOption(table, "suggestedLatencySeconds", stream.suggestedLatencySeconds, ValidateSuggestedLatency);
			SetOption(table, "wasapiExclusiveMode", stream.wasapiExclusiveMode);
			SetOption(table, "wasapiAutoConvert", stream.wasapiAutoConvert);
		}

		void SetConfig(const toml::Table& table, Config& config) {
			SetOption(table, "backend", config.backend);
			SetOption(table, "bufferSizeSamples", config.bufferSizeSamples, ValidateBufferSize);
			ProcessTypedOption<toml::Table>(table, "input", [&](const toml::Table& table) { SetStream(table, config.input); });
			ProcessTypedOption<toml::Table>(table, "output", [&](const toml::Table& table) { SetStream(table, config.output); });
		}


		Config LoadConfig(const std::filesystem::path& path) {
			toml::Value tomlValue;
			try {
				tomlValue = LoadConfigToml(path);
			}
			catch (...) {
				std::throw_with_nested(std::runtime_error("Unable to load configuration file"));
			}

			try {
				Config config;
				SetConfig(tomlValue.as<toml::Table>(), config);
				return config;
			}
			catch (...) {
				std::throw_with_nested(std::runtime_error("Invalid configuration"));
			}
		}

	}

	void ConfigLoader::HandleCloser::operator()(HANDLE handle) {
		if (::CloseHandle(handle) == 0)
			throw std::system_error(::GetLastError(), std::system_category(), "unable to close handle");
	}

	ConfigLoader::Watcher::Watcher(std::function<void()> onConfigFileEvent, const std::filesystem::path& configDirectory) : onConfigFileEvent(std::move(onConfigFileEvent)), stopEvent([&] {
		const auto handle = CreateEventA(NULL, TRUE, FALSE, NULL);
		if (handle == NULL)
			throw std::system_error(::GetLastError(), std::system_category(), "Unable to create stop event");
		return UniqueHandle(handle);
		}()), directory([&] {
		Log() << "Opening config directory for watching";
		const auto handle = ::CreateFileW(
			configDirectory.wstring().c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			/*lpSecurityAttributes=*/NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			/*hTemplateFile=*/NULL);
		if (handle == INVALID_HANDLE_VALUE)
			throw std::system_error(::GetLastError(), std::system_category(), "Unable to open config directory for watching");
		return UniqueHandle(handle);
		}()) {
		Log() << "Starting configuration file watcher";
		StartWatching();
		thread = std::thread([this] { RunThread(); });
	}

	ConfigLoader::Watcher::~Watcher() noexcept(false) {
		if (!SetEvent(stopEvent.get()))
			throw std::system_error(::GetLastError(), std::system_category(), "Unable to set stop event");
		thread.join();
	}

	void ConfigLoader::Watcher::RunThread() {
		// TODO: handle exceptions

		Log() << "Config watcher thread running";

		for (;;) {
			std::array handles = { stopEvent.get(), overlapped.overlapped.hEvent };
			const auto waitResult = ::WaitForMultipleObjects(DWORD(handles.size()), handles.data(), /*bWaitAll=*/FALSE, INFINITE);
			if (waitResult == WAIT_OBJECT_0) break;
			else if (waitResult == WAIT_OBJECT_0 + 1) OnEvent();
			else throw std::system_error(::GetLastError(), std::system_category(), "Unable to wait for events");
		}

		Log() << "Config watcher thread stopping";
	}

	void ConfigLoader::Watcher::OnEvent() {
		// Note: we need to be careful about logging here - since the logfile is in the same directory as the config file,
		// we could end up with directory change events entering an infinite feedback loop.

		DWORD size;
		if (!GetOverlappedResult(directory.get(), &overlapped.overlapped, &size, /*bWait=*/FALSE))
			throw std::system_error(::GetLastError(), std::system_category(), "GetOverlappedResult() failed");
		if (size <= 0) {
			Log() << "Config directory event buffer overflow";
			// We don't know if something happened to the logfile, so assume it did.
			onConfigFileEvent();
		}
		else {
			const char* fileNotifyInformationPtr = fileNotifyInformationBuffer;
			for (;;) {
				constexpr auto fileNotifyInformationHeaderSize = offsetof(FILE_NOTIFY_INFORMATION, FileName);
				FILE_NOTIFY_INFORMATION fileNotifyInformationHeader;
				memcpy(&fileNotifyInformationHeader, fileNotifyInformationPtr, fileNotifyInformationHeaderSize);

				std::wstring fileName(fileNotifyInformationHeader.FileNameLength / sizeof(wchar_t), 0);
				memcpy(fileName.data(), fileNotifyInformationPtr + fileNotifyInformationHeaderSize, fileNotifyInformationHeader.FileNameLength);
				if (fileName == configFileName) {
					// Here we can safely log.
					Log() << "Configuration file directory change received: NextEntryOffset = " << fileNotifyInformationHeader.NextEntryOffset
						<< " Action = " << fileNotifyInformationHeader.Action
						<< " FileNameLength = " << fileNotifyInformationHeader.FileNameLength;

					if (fileNotifyInformationHeader.Action == FILE_ACTION_ADDED ||
						fileNotifyInformationHeader.Action == FILE_ACTION_REMOVED ||
						fileNotifyInformationHeader.Action == FILE_ACTION_MODIFIED ||
						fileNotifyInformationHeader.Action == FILE_ACTION_RENAMED_NEW_NAME) {
						onConfigFileEvent();
					}
				}

				if (fileNotifyInformationHeader.NextEntryOffset == 0) break;
				fileNotifyInformationPtr += fileNotifyInformationHeader.NextEntryOffset;
			}
		}

		StartWatching();
	}

	ConfigLoader::Watcher::OverlappedWithEvent::OverlappedWithEvent() {
		overlapped.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
		if (overlapped.hEvent == NULL)
			throw std::system_error(::GetLastError(), std::system_category(), "Unable to create watch event");
	}
	ConfigLoader::Watcher::OverlappedWithEvent::~OverlappedWithEvent() {
		UniqueHandle(overlapped.hEvent);
	}

	void ConfigLoader::Watcher::StartWatching() {
		if (::ReadDirectoryChangesW(
			directory.get(),
			fileNotifyInformationBuffer, sizeof(fileNotifyInformationBuffer),
			/*bWatchSubtree=*/FALSE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
			/*lpBytesReturned=*/NULL,
			&overlapped.overlapped,
			/*lpCompletionRoutine=*/NULL) == 0)
			throw std::system_error(::GetLastError(), std::system_category(), "Unable to watch for directory changes");
	}

	ConfigLoader::ConfigLoader(std::function<void()> onConfigChange) :
		onConfigChange(std::move(onConfigChange)),
		configDirectory(GetUserDirectory()),
		initialConfig(LoadConfig(configDirectory / configFileName)) {}

	void ConfigLoader::OnConfigFileEvent() {
		Log() << "Handling config file event";

		// TODO: handle exceptions
		// TODO: some editors empty the file first, resulting in a spurious reset. We should probably debounce events.
		const auto newConfig = LoadConfig(configDirectory / configFileName);
		if (newConfig == initialConfig) {
			Log() << "New config is identical to initial config, not taking any action";
			return;
		}

		onConfigChange();
	}

}