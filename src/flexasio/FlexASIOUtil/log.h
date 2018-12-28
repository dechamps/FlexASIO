#pragma once

#include <functional>
#include <optional>
#include <sstream>
#include <string_view>

namespace flexasio {

	class Logger final
	{
	public:
		using Write = std::function<void(std::string_view)>;

		explicit Logger(Write write);
		~Logger();

		template <typename T> friend Logger&& operator<<(Logger&& lhs, T&& rhs) {
			if (lhs.enabledState.has_value()) lhs.enabledState->stream << std::forward<T>(rhs);
			return std::move(lhs);
		}

	private:
		struct EnabledState {
			Write write;
			std::stringstream stream;
		};

		std::optional<EnabledState> enabledState;
	};

	Logger Log();

}
