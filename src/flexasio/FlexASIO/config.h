#pragma once

#include <windows.h>

#include <array>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <semaphore>
#include <span>
#include <thread>
#include <variant>
#include <vector>

namespace flexasio {

	struct Config {
		struct DefaultDevice final {
			bool operator==(const DefaultDevice&) const { return true; }
		};
		struct NoDevice final {
			bool operator==(const NoDevice&) const { return true; }
		};
		struct DeviceRegex final {
			DeviceRegex(std::string string) : string(std::move(string)), regex(this->string) {}

			const std::string& getString() const { return string; }
			const std::regex& getRegex() const { return regex; }

			bool operator==(const DeviceRegex& other) const { return string == other.string; }

		private:
			std::string string;
			std::regex regex;
		};
		using Device = std::variant<DefaultDevice, NoDevice, std::string, DeviceRegex>;

		std::optional<std::string> backend;
		std::optional<int64_t> bufferSizeSamples;

		struct Stream {			
			Device device;
			std::optional<int> channels;
			std::optional<std::string> sampleType;
			std::optional<double> suggestedLatencySeconds;
			bool wasapiExclusiveMode = false;
			bool wasapiAutoConvert = true;
			bool wasapiExplicitSampleFormat = true;

			bool operator==(const Stream& other) const {
				return
					device == other.device &&
					channels == other.channels &&
					sampleType == other.sampleType &&
					suggestedLatencySeconds == other.suggestedLatencySeconds &&
					wasapiExclusiveMode == other.wasapiExclusiveMode &&
					wasapiAutoConvert == other.wasapiAutoConvert &&
					wasapiExplicitSampleFormat == other.wasapiExplicitSampleFormat;
			}
		};
		Stream input;
		Stream output;

		bool operator==(const Config& other) const {
			return
				backend == other.backend &&
				bufferSizeSamples == other.bufferSizeSamples &&
				input == other.input &&
				output == other.output;
		}
	};

	class ConfigLoader {
	public:
		ConfigLoader();

		const Config& Initial() const { return initialConfig; }

		class Watcher {
		public:
			Watcher(const ConfigLoader& configLoader, std::function<void()> onConfigChange);
			~Watcher() noexcept(false);

		private:
			class ConfigDirectoryWatchOperation final {
			public:
				ConfigDirectoryWatchOperation(HANDLE directory, OVERLAPPED* overlapped, std::span<std::byte> fileNotifyInformationBuffer);
				~ConfigDirectoryWatchOperation() noexcept(false);

				ConfigDirectoryWatchOperation(const ConfigDirectoryWatchOperation&) = delete;
				ConfigDirectoryWatchOperation& operator=(const ConfigDirectoryWatchOperation&) = delete;

				void Cancel();

				struct Aborted final {};
				struct Overflow final {};
				using Outcome = std::variant<Aborted, Overflow, std::span<const std::byte>>;
				Outcome Await();

			private:
				const HANDLE directory;
				OVERLAPPED* overlapped;
				std::span<std::byte> fileNotifyInformationBuffer;
			};

			// We abuse exception handling to handle stop requests. This is a bit unorthodox, but
			// it does make the code significantly simpler.
			struct StopRequested final {};
			void CheckStopRequested(std::chrono::milliseconds timeout);

			void RunThread();
			void TriggerConfigFileEventThenWait(OVERLAPPED*, std::span<std::byte> fileNotifyInformationBuffer);
			bool FileNotifyInformationContainsConfigFileEvents(std::span<const std::byte> fileNotifyInformationBuffer);
			void OnConfigFileEvent();

			const ConfigLoader& configLoader;
			const std::function<void()> onConfigChange;
			
			std::binary_semaphore stopSemaphore{0};
			std::mutex directoryMutex;
			HANDLE directory = INVALID_HANDLE_VALUE;

			std::thread thread;
		};

	private:
		const std::filesystem::path configDirectory;
		const Config initialConfig;
	};

}