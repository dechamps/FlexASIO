#pragma once

#include <ostream>
#include <string>
#include <string_view>

namespace flexasio {

	struct DefaultJoinRender {
		template <typename T> auto operator()(T&& val) { return std::forward<T>(val); }
	};

	template <typename Items, typename Render = DefaultJoinRender> void JoinStream(const Items& items, std::string_view delimiter, std::ostream& result, Render render = Render()) {
		auto it = std::begin(items);
		if (it == std::end(items)) return;
		for (;;) {
			result << render(*it);
			if (++it == std::end(items)) break;
			result << delimiter;
		}
	}
	template <typename Items, typename Render = DefaultJoinRender> std::string Join(const Items& items, std::string_view delimiter, Render render = Render()) {
		std::stringstream result;
		JoinStream(items, delimiter, result, render);
		return result.str();
	}

}