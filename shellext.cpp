// Modified from https://github.com/danielgrigg/ContextMenuDemo
// SPDX-License-Identifier: WTFPL
// SPDX-FileCopyrightText: 2014 Daniel C Grigg

#include "common.h"
#include <gdiplus.h>

using namespace ATL;

extern CLSID CLSID_ShellExt;

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
	public CComCoClass<CShellExt, &CLSID_ShellExt>,
	public IShellExtInit,
	public IContextMenu
{
public:
	CShellExt()
		: _bitmap(NULL)
	{
		WCHAR module_path[MAX_PATH];
		GetModuleFileName(_AtlBaseModule.GetModuleInstance(), module_path, _countof(module_path));

		PathRenameExtension(module_path, L".png");
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
		if (Gdiplus::Bitmap *const bitmap = Gdiplus::Bitmap::FromFile(module_path))
		{
			if (Gdiplus::Bitmap *const scaled = new Gdiplus::Bitmap(
				GetSystemMetrics(SM_CXMENUCHECK), GetSystemMetrics(SM_CYMENUCHECK)))
			{
				if (Gdiplus::Graphics *const graphics = Gdiplus::Graphics::FromImage(scaled))
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
				scaled->GetHBITMAP(Gdiplus::Color::Transparent, &_bitmap);
				delete scaled;
			}
			delete bitmap;
		}
		Gdiplus::GdiplusShutdown(gdiplusToken);

		PathRenameExtension(module_path, L".ini");
		WCHAR buffer[1024];
		if (GetPrivateProfileSectionNames(buffer, _countof(buffer), module_path))
		{
			for (WCHAR* p = buffer; *p; p += wcslen(p) + 1)
			{
				_sectionNames.push_back(p);
			}
		}
	}

	~CShellExt()
	{
		::DeleteObject(_bitmap);
	}

	DECLARE_REGISTRY_RESOURCEID(1)

	DECLARE_NOT_AGGREGATABLE(CShellExt)

	BEGIN_COM_MAP(CShellExt)
		COM_INTERFACE_ENTRY(IShellExtInit)
		COM_INTERFACE_ENTRY(IContextMenu)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

public:
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
	void ProcessFiles(UINT idCmd);

	std::vector<std::wstring> _selectedFiles;
	std::vector<std::wstring> _sectionNames;
	HBITMAP _bitmap;
};

OBJECT_ENTRY_AUTO(CLSID_ShellExt, CShellExt)

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
		_fileCount = ::DragQueryFile(_hDrop, 0xFFFFFFFF, NULL, 0);
	}

	~FileEnumFromDataObject()
	{
		if (_hDrop)
		{
			::GlobalUnlock(_stgm.hGlobal);
			::ReleaseStgMedium(&_stgm);
		}
	}

	UINT FileCount() const { return _fileCount; }

	std::wstring FileAt(UINT index) const 
	{
		WCHAR filenameBuffer[MAX_PATH];
		UINT copied = ::DragQueryFile(_hDrop, index, filenameBuffer, _countof(filenameBuffer));
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
		if (UINT idCmd = LOWORD(pici->lpVerb); idCmd < _sectionNames.size())
		{
			ProcessFiles(idCmd);
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
		UINT count = 0;
		for (const auto &sectionName : _sectionNames)
		{
			if (idCmdFirst == idCmdLast)
				break;
			::InsertMenu(hMenu, indexMenu, MF_STRING | MF_BYPOSITION, idCmdFirst++, sectionName.c_str());
			::SetMenuItemBitmaps(hMenu, indexMenu, MF_BYPOSITION, _bitmap, NULL);
			++indexMenu;
			++count;
		}
		return count;
	}
	catch (...)
	{
		return UncatchAs<HRESULT>();
	}
}

void CShellExt::ProcessFiles(UINT idCmd)
{
	CComBSTR workers;
	ATLENSURE(workers.LoadString(1));

	CComPtr<IProgressDialog> progress;
	ATLENSURE_SUCCEEDED(progress.CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER));
	progress->SetTitle(_sectionNames[idCmd].c_str());
	progress->StartProgressDialog(NULL, NULL, PROGDLG_AUTOTIME, NULL);

	const DWORD total = static_cast<DWORD>(_selectedFiles.size());
	DWORD finished = 0;

	WCHAR module_path[MAX_PATH];
	GetModuleFileName(_AtlBaseModule.GetModuleInstance(), module_path, _countof(module_path));
	PathRenameExtension(module_path, L".ini");

	WCHAR command[1024];
	GetPrivateProfileString(_sectionNames[idCmd].c_str(), L"command", L"", command, _countof(command), module_path);
	const LPWSTR args = PathGetArgs(command);
	PathRemoveArgs(command);

	WCHAR filter[MAX_PATH];
	GetPrivateProfileString(_sectionNames[idCmd].c_str(), L"filter", L"*.*", filter, _countof(filter), module_path);

	const int show = GetPrivateProfileInt(_sectionNames[idCmd].c_str(), L"show", SW_HIDE, module_path);

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
		if (limit < 1)
			limit = 1;
	}

	std::vector<HANDLE> satellites;
	satellites.reserve(limit);

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
			// Show the most recently added conversion candidate.
			progress->SetLine(2, PathFindFileName(filename.c_str()), FALSE, NULL);

			if (!PathMatchSpec(filename.c_str(), filter))
				continue;

			std::wstring command;
			command.append(L"\"").append(path).append(L"\" ").append(args);
			replace_all(command, L"<filename>", filename);
			replace_all(command, L"<basename>", filename.substr(0, filename.find_last_of(L".")));

			PROCESS_INFORMATION pi;
			STARTUPINFOW si = { sizeof(si) };
			si.wShowWindow = static_cast<WORD>(show);
			if (CreateProcess(NULL, command.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
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
