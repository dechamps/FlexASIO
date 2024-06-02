#define _CRT_SECURE_NO_WARNINGS  // Avoid issues with toml.h

#include "config.h"

#include <dechamps_cpputil/exception.h>
#include <toml/toml.h>

#include "log.h"
#include "../FlexASIOUtil/shell.h"
#include "../FlexASIOUtil/variant.h"

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
			ProcessTypedOption<typename RemoveOptional<T>::Value>(table, key, [&](const RemoveOptional<T>::Value& value) {
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
			if (table.find("device") != table.end() && table.find("deviceRegex") != table.end())
				throw std::runtime_error("the device and deviceRegex options cannot be specified at the same time");
			ProcessTypedOption<std::string>(table, "device", [&](const std::string& deviceString) {
				if (deviceString == "") stream.device = Config::NoDevice();
				else stream.device = deviceString;
			});
			ProcessTypedOption<std::string>(table, "deviceRegex", [&](const std::string& deviceRegexString) {
				if (deviceRegexString == "") throw std::runtime_error("the deviceRegex option cannot be empty");
				try {
					stream.device = Config::DeviceRegex(deviceRegexString);
				}
				catch (...) {
					std::throw_with_nested(std::runtime_error("Invalid regex in deviceRegex option"));
				}
			});

			SetOption(table, "channels", stream.channels, ValidateChannelCount);
			SetOption(table, "sampleType", stream.sampleType);
			SetOption(table, "suggestedLatencySeconds", stream.suggestedLatencySeconds, ValidateSuggestedLatency);
			SetOption(table, "wasapiExclusiveMode", stream.wasapiExclusiveMode);
			SetOption(table, "wasapiAutoConvert", stream.wasapiAutoConvert);
			SetOption(table, "wasapiExplicitSampleFormat", stream.wasapiExplicitSampleFormat);
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

		struct HandleCloser {
			void operator()(HANDLE handle) {
				if (::CloseHandle(handle) == 0)
					throw std::system_error(::GetLastError(), std::system_category(), "unable to close handle");
			}
		};
		using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

		struct OverlappedWithEvent final {
			OverlappedWithEvent() {
				overlapped.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
				if (overlapped.hEvent == NULL)
					throw std::system_error(::GetLastError(), std::system_category(), "Unable to create watch event");
			}
			~OverlappedWithEvent() {
				UniqueHandle(overlapped.hEvent);
			}

			OverlappedWithEvent(const OverlappedWithEvent&) = delete;
			OverlappedWithEvent& operator=(const OverlappedWithEvent&) = delete;

			OVERLAPPED overlapped = { 0 };
		};

	}

	ConfigLoader::Watcher::Watcher(const ConfigLoader& configLoader, std::function<void()> onConfigChange) :
		configLoader(configLoader),
		onConfigChange(std::move(onConfigChange)) {
		// Trigger an initial event so that if the config has already changed we fire the callback immediately inline.
		OnConfigFileEvent();

		Log() << "Starting config watcher thread";
		thread = std::thread([this] { RunThread(); });
	}

	ConfigLoader::Watcher::~Watcher() noexcept(false) {
		Log() << "Stopping config watcher";
		{
			std::scoped_lock lock(directoryMutex);
			if (directory != INVALID_HANDLE_VALUE) {
				Log() << "Cancelling any pending config directory operations";
				if (::CancelIoEx(directory, NULL) == 0)
					throw std::system_error(::GetLastError(), std::system_category(), "Unable to cancel directory watch operation");
			}
		}
		stopSemaphore.release();

		Log() << "Waiting for config watcher thread to finish";
		thread.join();

		Log() << "Joined config watcher thread";
	}

	void ConfigLoader::Watcher::CheckStopRequested(std::chrono::milliseconds waitFor = {}) {
		if (stopSemaphore.try_acquire_for(waitFor)) throw StopRequested();
	}

	void ConfigLoader::Watcher::RunThread() {
		Log() << "Config watcher thread running";

		try {
			OverlappedWithEvent overlapped;
			std::vector<std::byte> fileNotifyInformationBuffer(64 * 1024);
			for (;;) {
				TriggerConfigFileEventThenWait(&overlapped.overlapped, fileNotifyInformationBuffer);

				// It's best to debounce events that arrive in quick succession, otherwise we might attempt to read the file while it's being changed,
				// resulting in spurious resets.
				// (e.g. the Visual Studio Code editor will empty the file first before writing the new contents)
				// Another reason to debounce is that it might make it less likely we'll run into file locking issues.
				// We do this by sleeping for a while, and getting rid of all events that occurred in the mean time.
				Log() << "Sleeping for debounce";
				CheckStopRequested(/*waitFor=*/std::chrono::milliseconds(250));
			}
		}
		catch (StopRequested) {}
		catch (const std::exception& exception) {
			Log() << "Config watcher thread encountered error: " << ::dechamps_cpputil::GetNestedExceptionMessage(exception);
		}
		catch (...) {
			Log() << "Config watcher thread encountered unknown exception";
		}

		Log() << "Config watcher thread stopping";
	}

	void ConfigLoader::Watcher::TriggerConfigFileEventThenWait(OVERLAPPED* overlapped, std::span<std::byte> fileNotifyInformationBuffer) {
		// We don't keep the directory open between config file events, because otherwise
		// we would get events that accumulated during the debounce period.
		Log() << "Opening config directory for watching: " << configLoader.configDirectory;
		const auto ownedDirectory = [&] {
			const auto handle = ::CreateFileW(
				configLoader.configDirectory.wstring().c_str(),
				FILE_LIST_DIRECTORY,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				/*lpSecurityAttributes=*/NULL,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
				/*hTemplateFile=*/NULL);
			if (handle == INVALID_HANDLE_VALUE)
				throw std::system_error(::GetLastError(), std::system_category(), "Unable to open config directory for watching");

			{
				std::scoped_lock lock(directoryMutex);
				assert(directory == INVALID_HANDLE_VALUE);
				directory = handle;
			}
			const auto directoryDeleter = [&](HANDLE handle) {
				{
					std::scoped_lock lock(directoryMutex);
					assert(directory == handle);
					directory = INVALID_HANDLE_VALUE;
				}
				UniqueHandle{handle};
			};
			return std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(directoryDeleter)>(handle, directoryDeleter);
		}();

		Log() << "Watching config directory";
		for (bool first = true;; first = false) {
			// Note: we need to be careful about logging here - since the logfile is in the same directory as the config file,
			// we could end up with directory change events entering an infinite feedback loop.

			ConfigDirectoryWatchOperation operation(ownedDirectory.get(), overlapped, fileNotifyInformationBuffer);

			// Check for stop requests *after* we start watching, so that we correctly exit
			// even if the destructor ran just before there was an I/O to cancel.
			CheckStopRequested();

			if (first) {
				Log() << "Triggering initial config file event";
				OnConfigFileEvent();
			}

			if (OnVariant(operation.Await(),
				[&](ConfigDirectoryWatchOperation::Aborted) -> bool {
					CheckStopRequested();
					throw std::runtime_error("Config directory watch operation was aborted, but we were not requested to stop");
				},
				[&](ConfigDirectoryWatchOperation::Overflow) {
					Log() << "Config watcher file notify information buffer overflowed";
					// We don't know if something happened to the logfile, so assume it did.
					// If for some reason there is enough churn in the directory and we overflow all the time,
					// this will de facto fall back to polling at intervals given by the debounce period.
					return true;
				},
				[&](std::span<const std::byte> fileNotifyInformation) {
					return FileNotifyInformationContainsConfigFileEvents(fileNotifyInformation);
				}
			)) break;
		}
	}

	bool ConfigLoader::Watcher::FileNotifyInformationContainsConfigFileEvents(std::span<const std::byte> fileNotifyInformationBuffer) {
		for (;;) {
			constexpr auto fileNotifyInformationHeaderSize = offsetof(FILE_NOTIFY_INFORMATION, FileName);
			FILE_NOTIFY_INFORMATION fileNotifyInformationHeader;
			const auto fileNotifyInformationHeaderBuffer = fileNotifyInformationBuffer.first(fileNotifyInformationHeaderSize);
			memcpy(&fileNotifyInformationHeader, fileNotifyInformationHeaderBuffer.data(), fileNotifyInformationHeaderBuffer.size());

			const auto fileNameBuffer = fileNotifyInformationBuffer.subspan(fileNotifyInformationHeaderSize, fileNotifyInformationHeader.FileNameLength);
			std::wstring fileName(fileNameBuffer.size() / sizeof(wchar_t), 0);
			memcpy(fileName.data(), fileNameBuffer.data(), fileNameBuffer.size());
			if (fileName == configFileName) {
				// Here we can safely log.
				Log() << "Config directory change received with matching file name: "
					<< " NextEntryOffset = " << fileNotifyInformationHeader.NextEntryOffset
					<< " Action = " << fileNotifyInformationHeader.Action
					<< " FileNameLength = " << fileNotifyInformationHeader.FileNameLength;

				if (fileNotifyInformationHeader.Action == FILE_ACTION_ADDED ||
					fileNotifyInformationHeader.Action == FILE_ACTION_REMOVED ||
					fileNotifyInformationHeader.Action == FILE_ACTION_MODIFIED ||
					fileNotifyInformationHeader.Action == FILE_ACTION_RENAMED_NEW_NAME) {
					Log() << "Detected configuration file change event";
					return true;
				}
			}

			if (fileNotifyInformationHeader.NextEntryOffset == 0) break;
			fileNotifyInformationBuffer = fileNotifyInformationBuffer.subspan(fileNotifyInformationHeader.NextEntryOffset);
		}
		return false;
	}

	ConfigLoader::Watcher::ConfigDirectoryWatchOperation::ConfigDirectoryWatchOperation(HANDLE directory, OVERLAPPED* overlapped, std::span<std::byte> fileNotifyInformationBuffer) :
		directory(directory), overlapped(overlapped), fileNotifyInformationBuffer(fileNotifyInformationBuffer) {
		assert(overlapped != nullptr);
		assert(reinterpret_cast<std::uintptr_t>(fileNotifyInformationBuffer.data()) % sizeof(DWORD) == 0);
		if (::ReadDirectoryChangesW(
			directory,
			fileNotifyInformationBuffer.data(), DWORD(fileNotifyInformationBuffer.size()),
			/*bWatchSubtree=*/FALSE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
			/*lpBytesReturned=*/NULL,
			overlapped,
			/*lpCompletionRoutine=*/NULL) == 0)
			throw std::system_error(::GetLastError(), std::system_category(), "Unable to watch for directory changes");

	}
	ConfigLoader::Watcher::ConfigDirectoryWatchOperation::~ConfigDirectoryWatchOperation() noexcept (false) {
		if (overlapped == nullptr) return;

		Log() << "Cancelling pending directory watch operation";
		if (::CancelIoEx(directory, overlapped) == 0)
			throw std::system_error(::GetLastError(), std::system_category(), "Unable to cancel directory watch operation");

		Log() << "Awaiting cancelled operation";
		Await();
	}

	ConfigLoader::Watcher::ConfigDirectoryWatchOperation::Outcome ConfigLoader::Watcher::ConfigDirectoryWatchOperation::Await() {
		assert(overlapped != nullptr);

		DWORD size;
		const auto result = ::GetOverlappedResult(directory, overlapped, &size, /*bWait=*/TRUE);
		overlapped = nullptr;
		if (result == 0) {
			const auto error = ::GetLastError();
			if (error == ERROR_OPERATION_ABORTED) {
				Log() << "Directory watch operation was aborted";
				return Aborted();
			}
			throw std::system_error(::GetLastError(), std::system_category(), "GetOverlappedResult() failed");
		}
		if (size < 0 || size > fileNotifyInformationBuffer.size())
			throw std::system_error(::GetLastError(), std::system_category(), "ReadDirectoryChangesW() produced invalid size: " + std::to_string(size));
		return size > 0 ? Outcome(std::span(fileNotifyInformationBuffer).first(size)) : Outcome(Overflow());
	}

	ConfigLoader::ConfigLoader() :
		configDirectory(GetUserDirectory()),
		initialConfig(LoadConfig(configDirectory / configFileName)) {}

	void ConfigLoader::Watcher::OnConfigFileEvent() {
		Log() << "Handling config file event";

		Config newConfig;
		try {
			newConfig = LoadConfig(configLoader.configDirectory / configFileName);
		}
		catch (const std::exception& exception) {
			Log() << "Unable to load config, ignoring event: " << ::dechamps_cpputil::GetNestedExceptionMessage(exception);
			return;
		}
		if (newConfig == configLoader.Initial()) {
			Log() << "New config is identical to initial config, not taking any action";
			return;
		}

		onConfigChange();
	}

}