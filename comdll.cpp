/*

	Copyright (C) 2014 Etienne Dechamps (e-t172) <etienne@edechamps.fr>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <atlbase.h>
#include <atlcom.h>

// This implements DLL entry points so that the automagic ATL module can do its thing. Nothing to see here, move along.
class COMDLL : public CAtlDllModuleT<COMDLL> { };
static COMDLL comdll;
extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD dwReason, LPVOID lpReserved) { return comdll.DllMain(dwReason, lpReserved); }
STDAPI DllCanUnloadNow(void) { return comdll.DllCanUnloadNow(); }
STDAPI DllGetClassObject(__in REFCLSID rclsid, __in REFIID riid, __deref_out LPVOID* ppv) { return comdll.DllGetClassObject(rclsid, riid, ppv); }
STDAPI DllRegisterServer(void) { return comdll.DllRegisterServer(); }
STDAPI DllUnregisterServer(void) { return comdll.DllUnregisterServer(); }
