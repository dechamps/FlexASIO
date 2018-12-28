#pragma once

#include <functional>
#include <optional>
#include <sstream>
#include <string_view>

namespace flexasio {

	class LogSink {
	public:
		virtual void Write(std::string_view) = 0;
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

	Logger Log();

}
