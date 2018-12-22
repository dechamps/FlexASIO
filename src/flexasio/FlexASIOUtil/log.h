#pragma once

#include <optional>
#include <sstream>

namespace flexasio {

	class Log final
	{
	public:
		Log();
		~Log();

		template <typename T> friend Log&& operator<<(Log&& lhs, T&& rhs) {
			if (!lhs.stream.has_value()) return std::move(lhs);
			*lhs.stream << std::forward<T>(rhs);
			return std::move(lhs);
		}

	private:
		class Output;

		Log(Output*);

		Output* const output;
		std::optional<std::stringstream> stream;
	};

}
