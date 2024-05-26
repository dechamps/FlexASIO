#pragma once

#include <variant>

namespace flexasio {

	namespace variant_internal {
		template<class... Functors>
		struct overloaded final : Functors... { using Functors::operator()...; };
	}

	template <typename... Alternatives, class... Functors>
	decltype(auto) OnVariant(std::variant<Alternatives...>& variant, Functors... functors) { return std::visit(variant_internal::overloaded<Functors...>{functors...}, variant); }
	template <typename... Alternatives, class... Functors>
	decltype(auto) OnVariant(const std::variant<Alternatives...>& variant, Functors... functors) { return std::visit(variant_internal::overloaded<Functors...>{functors...}, variant); }
	template <typename... Alternatives, class... Functors>
	decltype(auto) OnVariant(std::variant<Alternatives...>&& variant, Functors... functors) { return std::visit(variant_internal::overloaded<Functors...>{functors...}, std::move(variant)); }
}
