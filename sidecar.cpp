// Modified from https://github.com/danielgrigg/ContextMenuDemo
// SPDX-License-Identifier: WTFPL
// SPDX-FileCopyrightText: 2014 Daniel C Grigg

#include "common.h"

using namespace ATL;

class CPluscontrolShellExtModule : public CAtlDllModuleT<CPluscontrolShellExtModule>
{
} _AtlModule;

// Registration helper
static HRESULT RegisterUnregisterShellExt(BOOL bRegister)
{
	// Read the rgs script from the rgs/ini hybrid file.
	try
	{
		CRegObject ro;
		ATLENSURE_SUCCEEDED(ro.FinalConstruct());
		WCHAR module_path[MAX_PATH];
		GetModuleFileName(_AtlBaseModule.GetModuleInstance(), module_path, _countof(module_path));
		ATLENSURE_SUCCEEDED(ro.AddReplacement(OLESTR("Module"), module_path));
		PathRenameExtension(module_path, L".ini");
		std::wifstream stream(module_path);
		std::wstring data;
		if (std::getline(stream, data, L'['))
		{
			auto method = bRegister ? &CRegObject::StringRegister : &CRegObject::StringUnregister;
			ATLENSURE_SUCCEEDED((ro.*method)(data.c_str()));
		}
		return S_OK;
	}
	catch (...)
	{
		return UncatchAs<HRESULT>();
	}
}

CLSID CLSID_ShellExt = CLSID_NULL;

// DLL Entry Point
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		// Read the CLSID from the rgs/ini hybrid file.
		try
		{
			WCHAR module_path[MAX_PATH];
			GetModuleFileName(_AtlBaseModule.GetModuleInstance(), module_path, _countof(module_path));
			PathRenameExtension(module_path, L".ini");
			std::wifstream stream(module_path);
			std::wstring data;
			if (std::getline(stream, data, L'}'))
			{
				data.erase(0, data.rfind(L'{'));
				data.push_back(L'}');
				ATLENSURE_SUCCEEDED(CLSIDFromString(data.c_str(), &CLSID_ShellExt));
			}
		}
		catch (...)
		{
			return FALSE;
		}
		DisableThreadLibraryCalls(hInstance);
	}
	return _AtlModule.DllMain(dwReason, lpReserved);
}

// Used to determine whether the DLL can be unloaded by OLE.
STDAPI DllCanUnloadNow()
{
	return _AtlModule.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type.
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry.
STDAPI DllRegisterServer()
{
	return RegisterUnregisterShellExt(TRUE);
}

// DllUnregisterServer - Removes entries from the system registry.
STDAPI DllUnregisterServer()
{
	return RegisterUnregisterShellExt(FALSE);
}

// DllInstall - Adds/Removes entries to the system registry per user per machine.
STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine)
{
	HRESULT hr = E_FAIL;

	if (pszCmdLine != NULL)
	{
#ifdef _DEBUG
		static const WCHAR szTestSwitch[] = L"test";
		if (_wcsnicmp(pszCmdLine, szTestSwitch, _countof(szTestSwitch)) == 0)
		{
			LPVOID pv = NULL;
			if (FAILED(hr = DllGetClassObject(CLSID_ShellExt, IID_IClassFactory, &pv)))
				return hr;
			if (FAILED(hr = static_cast<IClassFactory*>(pv)->CreateInstance(NULL, IID_IContextMenu, &pv)))
				return hr;
			return S_OK;
		}
#endif
		static const WCHAR szUserSwitch[] = L"user";
		if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0)
		{
			ATL::AtlSetPerUserRegistration(true);
		}
	}

	if (bInstall)
	{	
		hr = DllRegisterServer();
		if (FAILED(hr))
		{
			DllUnregisterServer();
		}
	}
	else
	{
		hr = DllUnregisterServer();
	}

	return hr;
}
