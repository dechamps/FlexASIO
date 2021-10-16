#pragma once

#include <string>
#include <string_view>

namespace flexasio {

	std::string ConvertToUTF8(std::wstring_view);
	std::wstring ConvertFromUTF8(std::string_view);

}
