#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#include <optional>
#include <sstream>
#include <string_view>

namespace flexasio {

	class LogSink {
	public:
		virtual void Write(std::string_view) = 0;
	};

	class PreambleLogSink final : public LogSink {
	public:
		PreambleLogSink(LogSink& backend);

		void Write(const std::string_view str) override { backend.Write(str); }

	private:
		LogSink& backend;
	};

	class StreamLogSink final : public LogSink {
	public:
		StreamLogSink(std::ostream& stream) : stream(stream) {}

		void Write(const std::string_view str) override { stream << str << std::endl;  }

	private:
		std::ostream& stream;
	};

	class FileLogSink final : public LogSink {
	public:
		FileLogSink(const std::filesystem::path& path);
		~FileLogSink();

		void Write(const std::string_view str) override { stream_sink.Write(str); }

	private:
		std::ofstream stream;
		StreamLogSink stream_sink{ stream };
	};

	class ThreadSafeLogSink final : public LogSink {
	public:
		ThreadSafeLogSink(LogSink& backend) : backend(backend) {}

		void Write(std::string_view) override;

	private:
		std::mutex mutex;
		LogSink& backend;
	};

	class AsyncLogSink final : public LogSink {
	public:
		AsyncLogSink(LogSink& backend) : backend(backend), thread([&] { RunThread(); }) {}
		~AsyncLogSink();

		void Write(std::string_view) override;

	private:
		void RunThread();

		LogSink& backend;
		std::mutex mutex;
		std::condition_variable stateChanged;
		std::vector<std::string> queue;
		bool shutdown = false;

		std::thread thread;
	};

	class Logger final
	{
	public:
		explicit Logger(LogSink* sink);
		~Logger();

		template <typename T> friend Logger&& operator<<(Logger&& lhs, T&& rhs) {
			if (lhs.enabledState.has_value()) lhs.enabledState->stream << std::forward<T>(rhs);
			return std::move(lhs);
		}

	private:
		struct EnabledState {
			explicit EnabledState(LogSink& sink) : sink(sink) {}

			LogSink& sink;
			std::stringstream stream;
		};

		std::optional<EnabledState> enabledState;
	};

}
