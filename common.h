#define STRICT
#define STRSAFE_NO_DEPRECATE
#define _HAS_CXX17 1
#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

// Follow https://alax.info/blog/1351 to have ATLTRACE in RelWithDebInfo builds.
// This doesn't resuse Roman's code because the links in the blog post are dead.

#include <stdlib.h>
#include <strsafe.h>
#include <windows.h>

#ifndef _DEBUG

template <size_t>
class CAtlTrace
{
public:
	CAtlTrace(LPCSTR file, int line) {}
	void operator()(int category, UINT level, LPCSTR fmt, ...) {}
	void operator()(int category, UINT level, LPCWSTR fmt, ...) {}
	void operator()(LPCSTR fmt, ...) {}
	void operator()(LPCWSTR fmt, ...) {}
	int ReportAssertionFailed(LPCSTR expr) { return 0; }
};

template <>
class CAtlTrace <sizeof "RelWithDebInfo">
{
public:
	CAtlTrace(LPCSTR file, int line)
		: m_file(file)
		, m_line(line)
		, m_ppszDestEndA(m_bufferA)
		, m_ppszDestEndW(m_bufferW)
		, m_cchRemainingA(_countof(m_bufferA))
		, m_cchRemainingW(_countof(m_bufferW))
	{
	}
	void operator()(int category, UINT level, LPCSTR fmt, ...)
	{
		va_list ptr;
		va_start(ptr, fmt);
		StringCchPrintfExA(m_ppszDestEndA, _countof(m_bufferA), &m_ppszDestEndA, &m_cchRemainingA, 0, "%hs(%d): ", m_file, m_line);
		StringCchVPrintfExA(m_ppszDestEndA, _countof(m_bufferA), &m_ppszDestEndA, &m_cchRemainingA, 0, fmt, ptr);
		OutputDebugStringA(m_bufferA);
		va_end(ptr);
	}
	void operator()(int category, UINT level, LPCWSTR fmt, ...)
	{
		va_list ptr;
		va_start(ptr, fmt);
		StringCchPrintfExW(m_ppszDestEndW, _countof(m_bufferW), &m_ppszDestEndW, &m_cchRemainingW, 0, L"%hs(%d): ", m_file, m_line);
		StringCchVPrintfExW(m_ppszDestEndW, _countof(m_bufferW), &m_ppszDestEndW, &m_cchRemainingW, 0, fmt, ptr);
		OutputDebugStringW(m_bufferW);
		va_end(ptr);
	}
	void operator()(LPCSTR fmt, ...)
	{
		va_list ptr;
		va_start(ptr, fmt);
		operator()(0, 0, fmt, ptr);
		va_end(ptr);
	}
	void operator()(LPCWSTR fmt, ...)
	{
		va_list ptr;
		va_start(ptr, fmt);
		operator()(0, 0, fmt, ptr);
		va_end(ptr);
	}
	int ReportAssertionFailed(LPCSTR expr)
	{
		operator()(0, 0, L"Assertion failed: %hs", expr);
		switch (MessageBox(NULL, m_bufferW, NULL, MB_ABORTRETRYIGNORE | MB_ICONSTOP | MB_TOPMOST))
		{
		case IDABORT:
			TerminateProcess(GetCurrentProcess(), 1);
			break;
		case IDRETRY:
			__debugbreak();
			break;
		default:
			break;
		}
		return 0;
	}

private:
	const LPCSTR m_file;
	const int m_line;
	union
	{
		CHAR m_bufferA[1024];
		WCHAR m_bufferW[1024];
	};
	LPSTR m_ppszDestEndA;
	LPWSTR m_ppszDestEndW;
	size_t m_cchRemainingA;
	size_t m_cchRemainingW;
};

#define ATLTRACE CAtlTrace<sizeof CMAKE_INTDIR>(__FILE__, __LINE__)
#define ATLTRACE2 ATLTRACE
#define ATLASSERT(expr) (void)((expr) || CAtlTrace<sizeof CMAKE_INTDIR>(__FILE__, __LINE__).ReportAssertionFailed(#expr))

#endif

#include <atlbase.h>
#include <atlcom.h>
#include <ShlObj.h>
#include <algorithm>
#include <exception>
#include <fstream>
#include <new>
#include <string>
#include <vector>

using namespace ATL;

template <typename T>
T UncatchAs();

template <>
inline HRESULT UncatchAs<HRESULT>()
{
	try
	{
		throw;
	}
	catch (const CAtlException &e)
	{
		ATLTRACE(L"Caught ATL::CAtlException: 0x%08X", static_cast<HRESULT>(e));
		return e;
	}
	catch (const std::bad_alloc &)
	{
		ATLTRACE(L"Caught std::bad_alloc");
		return E_OUTOFMEMORY;
	}
	catch (const std::exception &e)
	{
		ATLTRACE(L"Caught std::exception: %hs", e.what());
		return E_FAIL;
	}
	catch (...)
	{
		ATLTRACE(L"Caught unknown exception");
		return E_FAIL;
	}
}

extern class CModule
	: public CAtlDllModuleT<CModule>
{
public:
	CModule();
	~CModule();
} _AtlModule;
