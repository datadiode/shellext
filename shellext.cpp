// Modified from https://github.com/danielgrigg/ContextMenuDemo
// SPDX-License-Identifier: WTFPL
// SPDX-FileCopyrightText: 2014 Daniel C Grigg

#include "common.h"
#include <gdiplus.h>

CModule::CModule()
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
			ATLENSURE_SUCCEEDED(CLSIDFromString(data.c_str(), &_AtlModule.m_libid));
		}
	}
	catch (...)
	{
		UncatchAs<HRESULT>();
	}
}

CModule::~CModule()
{
}

void replace_all(std::wstring &str, const std::wstring &placeholder, const std::wstring &text)
{
	size_t pos = 0;
	while ((pos = str.find(placeholder, pos)) != std::wstring::npos)
	{
		str.replace(pos, placeholder.size(), text);
		pos += text.size();
	}
}

class ATL_NO_VTABLE CShellExt :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CShellExt, &_AtlModule.m_libid>,
	public IShellExtInit,
	public IContextMenu
{
public:
	DECLARE_NOT_AGGREGATABLE(CShellExt)

	BEGIN_COM_MAP(CShellExt)
		COM_INTERFACE_ENTRY(IShellExtInit)
		COM_INTERFACE_ENTRY(IContextMenu)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	static HRESULT WINAPI UpdateRegistry(BOOL);
	HRESULT FinalConstruct();
	void FinalRelease();

	STDMETHODIMP Initialize(
		PCIDLIST_ABSOLUTE pidlFolder,
		IDataObject* pdtobj,
		HKEY hkeyProgID);

	STDMETHODIMP GetCommandString(
		UINT_PTR idCmd,
		UINT uFlags,
		UINT* pwReserved,
		LPSTR pszName,
		UINT cchMax);

	STDMETHODIMP InvokeCommand(
		LPCMINVOKECOMMANDINFO pici);

	STDMETHODIMP QueryContextMenu(
		HMENU hMenu,
		UINT indexMenu,
		UINT idCmdFirst,
		UINT idCmdLast,
		UINT uFlags);

private:
	void ProcessFiles(HWND hWnd, UINT idCmd);

	std::vector<std::wstring> _selectedFiles;
	static std::vector<std::string> _sectionNames;
	static std::unique_ptr<HBITMAP__, decltype(&DeleteObject)> _bitmap;
};

decltype(CShellExt::_sectionNames) CShellExt::_sectionNames;
decltype(CShellExt::_bitmap) CShellExt::_bitmap(NULL, &DeleteObject);

OBJECT_ENTRY_AUTO(_AtlModule.m_libid, CShellExt)

class FileEnumFromDataObject
{
public:
	explicit FileEnumFromDataObject(IDataObject* pdtobj): 
		_hDrop(NULL),
		_fileCount(0)
	{
		ATLENSURE_THROW(pdtobj != NULL, E_INVALIDARG);

		// format to request hdrop data from the data object
		FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

		memset(&_stgm, 0, sizeof(_stgm));
		_stgm.tymed = TYMED_HGLOBAL;

		// retrieve hdrop data from source data object
		ATLENSURE_SUCCEEDED(pdtobj->GetData(&fmte, &_stgm));

		// get point to the file names data
		_hDrop = reinterpret_cast<HDROP>(::GlobalLock(_stgm.hGlobal));
		ATLENSURE_THROW(_hDrop != NULL, E_INVALIDARG);
		_fileCount = DragQueryFile(_hDrop, 0xFFFFFFFF, NULL, 0);
	}

	~FileEnumFromDataObject()
	{
		if (_hDrop)
		{
			GlobalUnlock(_stgm.hGlobal);
			ReleaseStgMedium(&_stgm);
		}
	}

	UINT FileCount() const { return _fileCount; }

	std::wstring FileAt(UINT index) const 
	{
		WCHAR filenameBuffer[MAX_PATH];
		UINT copied = DragQueryFile(_hDrop, index, filenameBuffer, _countof(filenameBuffer));
		return std::wstring(filenameBuffer, copied > 0 && copied < _countof(filenameBuffer) ? copied : 0);
	}

private:
	FileEnumFromDataObject(const FileEnumFromDataObject &);
	FileEnumFromDataObject& operator=(const FileEnumFromDataObject &);
	HDROP _hDrop;			// hdrop from IDataObject
	STGMEDIUM _stgm;		// storage medium (global memory handle)
	UINT _fileCount;
};

STDMETHODIMP CShellExt::Initialize(
	PCIDLIST_ABSOLUTE pidlFolder,
	IDataObject* pdtobj,
	HKEY hkeyProgId)
{
	try
	{
		_selectedFiles.clear();
		FileEnumFromDataObject files(pdtobj);
		for (UINT i = 0; i < files.FileCount(); ++i)
		{
			auto filename = files.FileAt(i);
			if (!filename.empty()) 
			{
				_selectedFiles.push_back(filename);
			}
		}
		return _selectedFiles.size() != 0 ? S_OK : E_INVALIDARG;
	}
	catch (...)
	{
		return UncatchAs<HRESULT>();
	}
}

HRESULT CShellExt::UpdateRegistry(BOOL bRegister)
{
	// Read the rgs script from the rgs/ini hybrid file.
	try
	{
		CComPtr<IRegistrar> registrar;
		ATLENSURE_SUCCEEDED(registrar.CoCreateInstance(CLSID_Registrar));
		WCHAR module_path[MAX_PATH];
		GetModuleFileName(_AtlBaseModule.GetModuleInstance(), module_path, _countof(module_path));
		CRegObject ro;
		ATLENSURE_SUCCEEDED(ro.FinalConstruct());
		ATLENSURE_SUCCEEDED(ro.AddReplacement(L"Module", module_path));
		PathRenameExtension(module_path, L".ini");
		std::wifstream stream(module_path);
		std::wstring data;
		HRESULT hr = S_OK;
		if (std::getline(stream, data, L'['))
		{
			LPTSTR szReg = NULL;
			ATLENSURE_SUCCEEDED(CRegParser(&ro).PreProcessBuffer(const_cast<LPWSTR>(data.c_str()), &szReg));
			auto method = bRegister ? &IRegistrar::StringRegister : &IRegistrar::StringUnregister;
			hr = (registrar->*method)(szReg);
			CoTaskMemFree(szReg);
		}
		return hr;
	}
	catch (...)
	{
		return UncatchAs<HRESULT>();
	}
}

HRESULT CShellExt::FinalConstruct()
{
	// When coming here for the first time, initialize some static data.
	static LONG visited = FALSE;
	if (InterlockedExchange(&visited, TRUE))
		return S_OK;

	try
	{
		WCHAR module_path[MAX_PATH];
		GetModuleFileName(_AtlBaseModule.GetModuleInstance(), module_path, _countof(module_path));

		PathRenameExtension(module_path, L".png");
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
		if (Gdiplus::Bitmap* const bitmap = Gdiplus::Bitmap::FromFile(module_path))
		{
			if (Gdiplus::Bitmap* const scaled = new Gdiplus::Bitmap(
				GetSystemMetrics(SM_CXMENUCHECK), GetSystemMetrics(SM_CYMENUCHECK)))
			{
				if (Gdiplus::Graphics* const graphics = Gdiplus::Graphics::FromImage(scaled))
				{
					graphics->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
					Gdiplus::RectF rect(0, 0, Gdiplus::REAL(scaled->GetWidth()), Gdiplus::REAL(scaled->GetHeight()));
					COLORREF rgb = GetSysColor(COLOR_3DFACE);
					Gdiplus::SolidBrush brush(Gdiplus::Color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)));
					graphics->FillRectangle(&brush, rect);
					graphics->DrawImage(bitmap, rect,
						0, 0, Gdiplus::REAL(bitmap->GetWidth()), Gdiplus::REAL(bitmap->GetHeight()),
						Gdiplus::UnitPixel);
					delete graphics;
				}
				HBITMAP handle = NULL;
				scaled->GetHBITMAP(Gdiplus::Color::Transparent, &handle);
				_bitmap.reset(handle);
				delete scaled;
			}
			delete bitmap;
		}
		Gdiplus::GdiplusShutdown(gdiplusToken);

		PathRenameExtension(module_path, L".ini");
		struct _stat32 stat32;
		if (_wstat32(module_path, &stat32) == 0)
		{
			std::vector<WCHAR> buffer(stat32.st_size);
			LPWSTR p = buffer.data();
			if (GetPrivateProfileSectionNames(p, stat32.st_size, module_path))
			{
				while (const size_t n = wcslen(p))
				{
#pragma warning(disable: 4244)
					_sectionNames.emplace_back(p, p + n); // narrowing is intentional
#pragma warning(default: 4244)
					p += n + 1;
				}
			}
		}
		return S_OK;
	}
	catch (...)
	{
		return UncatchAs<HRESULT>();
	}
}

void CShellExt::FinalRelease()
{
}

STDMETHODIMP CShellExt::GetCommandString(
	UINT_PTR idCmd,
	UINT uFlags,
	UINT *pwReserved,
	LPSTR pszName,
	UINT cchMax) 
{
	return E_INVALIDARG;
}

STDMETHODIMP CShellExt::InvokeCommand(
	LPCMINVOKECOMMANDINFO pici)
{
	try
	{
		// not using verbs here
		if (!IS_INTRESOURCE(pici->lpVerb))
			return E_INVALIDARG;
		// extract command index from the low word
		const UINT idCmd = LOWORD(pici->lpVerb);
		if (idCmd < _sectionNames.size())
		{
			ProcessFiles(pici->hwnd, idCmd);
			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	}
	catch (...)
	{
		return UncatchAs<HRESULT>();
	}
}

STDMETHODIMP CShellExt::QueryContextMenu(
	HMENU hMenu,
	UINT indexMenu,
	UINT idCmdFirst,
	UINT idCmdLast,
	UINT uFlags)
{
	try
	{
		if (uFlags & CMF_DEFAULTONLY) return 0;

		const LCID lcid = GetUserDefaultUILanguage();
		WCHAR lang[4];
		WCHAR ctry[4];
		GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, lang, _countof(lang));
		GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, ctry, _countof(ctry));
		WCHAR precise[12];
		WCHAR fallback[8];
		StringCchPrintf(precise, _countof(precise), L";%s-%s=", lang, ctry);
		StringCchPrintf(fallback, _countof(fallback), L";%s=", lang);

		UINT count = 0;
		for (const auto &sectionName : _sectionNames)
		{
			if (idCmdFirst == idCmdLast)
				break;

			WCHAR utf16[1024];
			if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sectionName.c_str(), -1, utf16, _countof(utf16)))
				break;

			UINT uFlags = MF_STRING | MF_BYPOSITION;

			LPCWSTR q = utf16;
			if (*q == '~')
				++q;

			if (const LPWSTR filter = std::wcsstr(utf16, L";*"))
			{
				if (std::any_of(_selectedFiles.begin(), _selectedFiles.end(),
								[filter](const std::wstring& filename)
								{
									return !PathMatchSpec(filename.c_str(), filter + 1);
								}))
				{
					uFlags |= MF_GRAYED;
					q = utf16;
				}
			}

			if (*q != '~')
			{
				LPWSTR r = std::wcsstr(utf16, precise);
				if (r != NULL || (r = std::wcsstr(utf16, fallback)) != NULL)
					if ((r = std::wcschr(r, L'=')) != NULL)
						q = ++r;
				if ((r = std::wcschr(std::max<LPWSTR>(utf16, r), L';')) != NULL)
					*r = L'\0';

				InsertMenu(hMenu, indexMenu, uFlags, idCmdFirst, q);
				SetMenuItemBitmaps(hMenu, indexMenu, MF_BYPOSITION, _bitmap.get(), NULL);
				++indexMenu;
			}

			++idCmdFirst;
			++count;
		}
		return count;
	}
	catch (...)
	{
		return UncatchAs<HRESULT>();
	}
}

void CShellExt::ProcessFiles(HWND hWnd, UINT idCmd)
{
	CComBSTR workers;
	ATLENSURE(workers.LoadString(1));

	WCHAR module_path[MAX_PATH];
	GetModuleFileName(_AtlBaseModule.GetModuleInstance(), module_path, _countof(module_path));
	PathRenameExtension(module_path, L".ini");

	const std::wstring key(PBYTE(&_sectionNames[idCmd].front()), PBYTE(&_sectionNames[idCmd].back() + 1));
	const size_t keyfilter(key.find(L";*") + 1); // if != 0, offset of the filter in the key string

	WCHAR command[1024];
	GetPrivateProfileString(key.c_str(), L"command", L"", command, _countof(command), module_path);
	const LPWSTR args = PathGetArgs(command);
	PathRemoveArgs(command);

	WCHAR destfolder[MAX_PATH];
	GetPrivateProfileString(key.c_str(), L"destfolder", L".", destfolder, _countof(destfolder), module_path);

	const int show = GetPrivateProfileInt(key.c_str(), L"show", SW_HIDE, module_path);

	PathRemoveFileSpec(module_path);

	// Assume an executable given without quotation marks lives in the same directory as the shell extension DLL.
	WCHAR path[MAX_PATH];
	PathCombine(path, PathUnquoteSpaces(command) ? NULL : module_path, command);

	int limit = PathParseIconLocation(args);
	if (limit <= 0)
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		limit += si.dwNumberOfProcessors;
	}
	limit = std::clamp(limit, 1, MAXIMUM_WAIT_OBJECTS);

	if (std::wcsstr(args, L"<destfolder>"))
	{
		CComBSTR title;
		ATLENSURE(title.LoadString(2));
		BROWSEINFO bi = { hWnd };
		bi.ulFlags = BIF_USENEWUI;
		bi.lpszTitle = title;
		CreateDirectory(destfolder, NULL);
		SHILCreateFromPath(destfolder, const_cast<LPITEMIDLIST*>(&bi.pidlRoot), NULL);
		BOOL ok = FALSE;
		if (PIDLIST_ABSOLUTE pidl =	SHBrowseForFolder(&bi))
		{
			ok = SHGetPathFromIDList(pidl, destfolder);
			CoTaskMemFree(pidl);
		}
		CoTaskMemFree(const_cast<LPITEMIDLIST>(bi.pidlRoot));
		if (!ok)
			return;
	}

	CComPtr<IProgressDialog> progress;
	ATLENSURE_SUCCEEDED(progress.CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER));

	// Query the localized menu item text to use as the progress dialog title.
	if (const HMENU hMenu = CreatePopupMenu())
	{
		WCHAR title[256];
		QueryContextMenu(hMenu, 0, 0, static_cast<UINT>(_sectionNames.size()), 0);
		GetMenuString(hMenu, idCmd, title, _countof(title), MF_BYCOMMAND);
		progress->SetTitle(title);
		DestroyMenu(hMenu);
	}

	progress->StartProgressDialog(hWnd, NULL, PROGDLG_AUTOTIME, NULL);

	std::vector<HANDLE> satellites;
	satellites.reserve(limit);

	const DWORD total = static_cast<DWORD>(_selectedFiles.size());
	DWORD finished = 0;

	auto it = _selectedFiles.begin();
	while ((satellites.size() || it != _selectedFiles.end()) && !progress->HasUserCancelled())
	{
		if (const DWORD count = static_cast<DWORD>(satellites.size()))
		{
			WCHAR msg[256];
			StringCchPrintf(msg, _countof(msg), workers, count);
			progress->SetLine(1, msg, FALSE, NULL);
			const DWORD wait = WaitForMultipleObjects(count, satellites.data(), FALSE, 1000);
			if (wait >= WAIT_OBJECT_0 && wait < WAIT_OBJECT_0 + count)
			{
				satellites.erase(satellites.begin() + (wait - WAIT_OBJECT_0));
				progress->SetProgress(++finished, total);
			}
		}
		while (satellites.size() < static_cast<DWORD>(limit) && it != _selectedFiles.end())
		{
			const auto &filename = *it++;

			// Skip files that don't match the filter, if any.
			if (keyfilter && !PathMatchSpec(filename.c_str(), &key[keyfilter]))
				continue;

			// Show the most recently added conversion candidate.
			progress->SetLine(2, PathFindFileName(filename.c_str()), FALSE, NULL);

			std::wstring command;
			command.append(L"\"").append(path).append(L"\" ").append(args);
			replace_all(command, L"<filename>", filename);
			replace_all(command, L"<basename>", filename.substr(0, filename.find_last_of(L".")));
			replace_all(command, L"<destfolder>", destfolder);

			PROCESS_INFORMATION pi;
			STARTUPINFOW si = { sizeof(si) };
			si.wShowWindow = static_cast<WORD>(show);
			DWORD flags = 0;
			if (show == SW_HIDE)
				flags |= CREATE_NO_WINDOW;
			if (CreateProcess(NULL, const_cast<LPWSTR>(command.data()), NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi))
			{
				CloseHandle(pi.hThread);
				satellites.push_back(pi.hProcess);
			}
		}
	}

	progress->StopProgressDialog();

	for (const HANDLE hProcess : satellites)
	{
		TerminateProcess(hProcess, ERROR_TIMEOUT);
		CloseHandle(hProcess);
	}
}
