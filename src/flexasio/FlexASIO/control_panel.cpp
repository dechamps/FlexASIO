#include "control_panel.h"

#include "../FlexASIOUtil/windows_error.h"

#include <dechamps_CMakeUtils/version.h>

#include "log.h"

namespace flexasio {

	void OpenControlPanel(HWND windowHandle) {
		const auto url = std::string("https://github.com/dechamps/FlexASIO/blob/") + ::dechamps_CMakeUtils_gitDescription + "/CONFIGURATION.md";
		Log() << "Opening URL: " << url;
		const auto result = ShellExecuteA(windowHandle, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
		Log() << "ShellExecuteA() result: " << result;
	}

}
