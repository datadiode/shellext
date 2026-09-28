// Modified from https://github.com/danielgrigg/ContextMenuDemo
// SPDX-License-Identifier: WTFPL
// SPDX-FileCopyrightText: 2014 Daniel C Grigg

#include "common.h"

using namespace ATL;

class CPluscontrolShellExtModule : public CAtlDllModuleT<CPluscontrolShellExtModule>
{
} _AtlModule;

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
			GetModuleFileName(hInstance, module_path, _countof(module_path));
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
	return _AtlModule.DllRegisterServer(FALSE);
}

// DllUnregisterServer - Removes entries from the system registry.
STDAPI DllUnregisterServer()
{
	return _AtlModule.DllUnregisterServer(FALSE);
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
			if (HMENU hMenu = CreatePopupMenu())
			{
				static_cast<IContextMenu*>(pv)->QueryContextMenu(hMenu, 0, 1, 5, 0);
				DestroyMenu(hMenu);
				CMINVOKECOMMANDINFO ici = { sizeof(ici) };
				static_cast<IContextMenu*>(pv)->InvokeCommand(&ici);
			}
			static_cast<IUnknown*>(pv)->Release();
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
