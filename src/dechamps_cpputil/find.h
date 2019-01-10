#pragma once

#include <optional>

namespace dechamps_cpputil {

	template <typename Key, typename KeyValues> auto Find(const Key& key, const KeyValues& keyValues) -> std::optional<decltype(begin(keyValues)->second)> {
		for (const auto& keyValue : keyValues) {
			if (keyValue.first == key) return keyValue.second;
		}
		return std::nullopt;
	}

}