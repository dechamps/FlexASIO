#include "log.h"

#include <shlobj.h>
#include <windows.h>

#include "../FlexASIOUtil/shell.h"
#include "../version/version.h"

namespace flexasio {

	namespace {

		class FlexASIOLogSink final : public LogSink {
			public:
				static std::unique_ptr<FlexASIOLogSink> Open() {
					const auto userDirectory = GetUserDirectory();
					if (!userDirectory.has_value()) return nullptr;

					std::filesystem::path path(*userDirectory);
					path.append("FlexASIO.log");

					if (!std::filesystem::exists(path)) return nullptr;

					return std::make_unique<FlexASIOLogSink>(path);
				}

				static FlexASIOLogSink* Get() {
					static const auto output = Open();
					return output.get();
				}

				FlexASIOLogSink(const std::filesystem::path& path) : file_sink(path) {
					Logger(this) << "FlexASIO " << BUILD_CONFIGURATION << " " << BUILD_PLATFORM << " " << version << " built on " << buildTime;
				}

				void Write(const std::string_view str) override { return preamble_sink.Write(str); }

			private:
				FileLogSink file_sink;
				ThreadSafeLogSink thread_safe_sink{ file_sink };
				PreambleLogSink preamble_sink{ thread_safe_sink };
		};

	}

	Logger Log() { return Logger(FlexASIOLogSink::Get()); }

}