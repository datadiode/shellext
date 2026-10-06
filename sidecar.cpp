// Modified from https://github.com/danielgrigg/ContextMenuDemo
// SPDX-License-Identifier: WTFPL
// SPDX-FileCopyrightText: 2014 Daniel C Grigg

#include "common.h"

CModule _AtlModule;

// DLL Entry Point
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
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
#ifdef DEBUG
		static const WCHAR szTestSwitch[] = L"test";
		if (_wcsnicmp(pszCmdLine, szTestSwitch, _countof(szTestSwitch)) == 0)
		{
			LPVOID pv = NULL;
			if (FAILED(hr = DllGetClassObject(_AtlModule.m_libid, IID_IClassFactory, &pv)))
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
			AtlSetPerUserRegistration(true);
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
