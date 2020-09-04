#pragma once

#include <windows.h>

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>

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

			bool operator==(const Stream& other) const {
				return
					device == other.device &&
					channels == other.channels &&
					sampleType == other.sampleType &&
					suggestedLatencySeconds == other.suggestedLatencySeconds &&
					wasapiExclusiveMode == other.wasapiExclusiveMode &&
					wasapiAutoConvert == other.wasapiAutoConvert;
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
			struct HandleCloser {
				void operator()(HANDLE handle);
			};
			using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

			struct OverlappedWithEvent {
				OverlappedWithEvent();
				~OverlappedWithEvent();

				OVERLAPPED overlapped = { 0 };
			};

			void StartWatching();
			void RunThread();
			void OnEvent();
			void Debounce();
			bool FillNotifyInformationBuffer();
			bool FindConfigFileEvents();
			void OnConfigFileEvent();

			const ConfigLoader& configLoader;
			const std::function<void()> onConfigChange;
			const UniqueHandle stopEvent;
			const UniqueHandle directory;
			OverlappedWithEvent overlapped;
			alignas(DWORD) char fileNotifyInformationBuffer[64 * 1024];
			std::thread thread;
		};

	private:
		const std::filesystem::path configDirectory;
		const Config initialConfig;
	};

}