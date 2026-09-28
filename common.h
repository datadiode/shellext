#ifndef STRICT
#define STRICT
#endif

#undef NOGDI

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include <atlbase.h>
#include <atlcom.h>

#include <ShlObj.h>
#include <strsafe.h>
#include <exception>
#include <fstream>
#include <new>
#include <string>
#include <vector>

template <typename T>
T UncatchAs();

template <>
inline HRESULT UncatchAs<HRESULT>()
{
	try
	{
		throw;
	}
	catch (const ATL::CAtlException &e)
	{
		return e;
	}
	catch (const std::bad_alloc &)
	{
		return E_OUTOFMEMORY;
	}
	catch (...)
	{
		return E_FAIL;
	}
}
