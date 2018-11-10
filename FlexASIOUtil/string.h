#pragma once

#include "find.h"

#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace flexasio {

	struct DefaultRender {
		template <typename T> auto operator()(T&& val) { return std::forward<T>(val); }
	};

	template <typename Items, typename Render = DefaultRender> void JoinStream(const Items& items, std::string_view delimiter, std::ostream& result, Render render = Render()) {
		auto it = std::begin(items);
		if (it == std::end(items)) return;
		for (;;) {
			result << render(*it);
			if (++it == std::end(items)) break;
			result << delimiter;
		}
	}
	template <typename Items, typename Render = DefaultRender> std::string Join(const Items& items, std::string_view delimiter, Render render = Render()) {
		std::stringstream result;
		JoinStream(items, delimiter, result, render);
		return result.str();
	}

	template <typename Enum, typename Render = DefaultRender> std::string EnumToString(Enum value, std::initializer_list<std::pair<Enum, std::string_view>> enumStrings, Render renderValue = Render()) {
		std::stringstream result;
		result << renderValue(value);
		const auto string = Find(value, enumStrings);
		if (string) result << " [" << *string << "]";
		return result.str();
	}

}