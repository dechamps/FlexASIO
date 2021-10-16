#include "control_panel.h"

#include "../FlexASIOUtil/windows_com.h"
#include "../FlexASIOUtil/windows_error.h"
#include "../FlexASIOUtil/windows_registry.h"
#include "../FlexASIOUtil/windows_string.h"

#include <dechamps_cpputil/exception.h>
#include <dechamps_CMakeUtils/version.h>

#include "log.h"

#include <Windows.h>

namespace flexasio {

	namespace {

		void Execute(HWND windowHandle, const std::wstring& file) {
			Log() << "Initializing COM for shell execution";
			std::optional<COMInitializer> comInitializer;
			try {
				// As suggested in https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew#remarks
				comInitializer.emplace(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			}
			catch (const std::exception& exception) {
				Log() << "Unable to initialize COM: " << ::dechamps_cpputil::GetNestedExceptionMessage(exception);
			}
			catch (...) {
				Log() << "Unable to initialize COM due to unknown exception";
			}

			Log() << "Executing: " << ConvertToUTF8(file);
			if (!::ShellExecuteW(windowHandle, NULL, file.c_str(), NULL, NULL, SW_SHOWNORMAL))
				throw std::runtime_error("Execution failed: " + GetWindowsErrorString(::GetLastError()));
		}

		std::wstring GetStringRegistryValue(HKEY registryKey, LPCWSTR valueName) {
			std::vector<unsigned char> value;
			for (;;) {
				DWORD valueType = REG_NONE;
				DWORD valueSize = static_cast<DWORD>(value.size());
				Log() << "Querying registry value with buffer size " << valueSize;
				const auto regQueryValueError = ::RegQueryValueExW(registryKey, valueName, NULL, &valueType, reinterpret_cast<BYTE*>(value.data()), &valueSize);
				if ((regQueryValueError == ERROR_SUCCESS && value.size() == 0) || regQueryValueError == ERROR_MORE_DATA) {
					if (valueSize <= value.size()) throw std::runtime_error("Invalid value size returned from RegQueryValueEx(" + std::to_string(value.size()) + "): " + std::to_string(valueSize));
					value.resize(valueSize);
					continue;
				}
				if (regQueryValueError != ERROR_SUCCESS) throw std::runtime_error("Unable to query string registry value: " + GetWindowsErrorString(regQueryValueError));
				Log() << "Registry value size: " << valueSize;
				if (valueType != REG_SZ) throw std::runtime_error("Expected string registry value type, got " + std::to_string(valueType));
				value.resize(valueSize);
				break;
			}

			const auto char_size = sizeof(std::wstring::value_type);
			if (value.size() % char_size != 0) throw std::runtime_error("Invalid value size returned from RegQueryValueEx(): " + std::to_string(value.size()));
			std::wstring result(value.size() / char_size, 0);
			memcpy(result.data(), value.data(), value.size());
			while (!result.empty() && result.back() == 0) result.pop_back();
			return result;
		}

		UniqueHKEY OpenFlexAsioGuiInstallRegistryKey() {
			HKEY registryKey;
			const auto regOpenKeyError = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Fabrikat\\FlexASIOGUI\\Install", {}, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &registryKey);
			if (regOpenKeyError != ERROR_SUCCESS) throw std::runtime_error("Unable to open FlexASIOGUI registry key: " + GetWindowsErrorString(regOpenKeyError));
			return UniqueHKEY(registryKey);
		}

		std::wstring GetFlexAsioGuiInstallDirectory() {
			Log() << "Attempting to open FlexASIOGUI install registry key";
			const auto installRegistryKey = OpenFlexAsioGuiInstallRegistryKey();

			Log() << "Attempting to query FlexASIOGUI install path registry value";
			return GetStringRegistryValue(installRegistryKey.get(), L"InstallPath");
		}

		void OpenFlexAsioGui(HWND windowHandle) {
			const auto installDirectory = GetFlexAsioGuiInstallDirectory();
			Log() << "FlexASIOGUI install directory: " << ConvertToUTF8(installDirectory);

			Execute(windowHandle, installDirectory + L"\\FlexASIOGUI.exe");
		}
		
		void OpenConfigurationDocs(HWND windowHandle) {
			Execute(windowHandle, std::wstring(L"https://github.com/dechamps/FlexASIO/blob/") + ConvertFromUTF8(::dechamps_CMakeUtils_gitDescription) + L"/CONFIGURATION.md");
		}

	}

	void OpenControlPanel(HWND windowHandle) {
		Log() << "Attempting to open FlexASIO GUI";
		try {
			OpenFlexAsioGui(windowHandle);
			return;
		}
		catch (const std::exception& exception) {
			Log() << "Unable to open FlexASIO GUI: " << ::dechamps_cpputil::GetNestedExceptionMessage(exception);
		}
		catch (...) {
			Log() << "Unable to open FlexASIO GUI due to unknown exception";
		}

		Log() << "Attempting to open configuration docs";
		OpenConfigurationDocs(windowHandle);
	}

}
