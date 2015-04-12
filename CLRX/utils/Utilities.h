/*
 *  CLRadeonExtender - Unofficial OpenCL Radeon Extensions Library
 *  Copyright (C) 2014-2015 Mateusz Szpakowski
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*! \file Utilities.h
 * \brief utilities for other libraries and programs
 */

#ifndef __CLRX_UTILITIES_H__
#define __CLRX_UTILITIES_H__

#include <CLRX/Config.h>
#include <exception>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <CLRX/utils/Containers.h>

/// main namespace
namespace CLRX
{

struct NonCopyableAndNonMovable
{
    NonCopyableAndNonMovable() { }
    NonCopyableAndNonMovable(const NonCopyableAndNonMovable&) = delete;
    NonCopyableAndNonMovable(NonCopyableAndNonMovable&&) = delete;
    NonCopyableAndNonMovable& operator=(const NonCopyableAndNonMovable&) = delete;
    NonCopyableAndNonMovable& operator=(NonCopyableAndNonMovable&&) = delete;
};

/// exception class
class Exception: public std::exception
{
protected:
    std::string message;
public:
    Exception() = default;
    explicit Exception(const std::string& message);
    virtual ~Exception() throw() = default;
    
    const char* what() const throw();
};

/// parse exception class
class ParseException: public Exception
{
public:
    ParseException() = default;
    explicit ParseException(const std::string& message);
    ParseException(size_t lineNo, const std::string& message);
    ParseException(size_t lineNo, size_t charNo, const std::string& message);
    virtual ~ParseException() throw() = default;
};


enum {
    DYNLIB_LOCAL = 0,   ///< treat symbols locally
    DYNLIB_LAZY = 1,    ///< resolve symbols when is needed
    DYNLIB_NOW = 2,     ///< resolve symbols now
    DYNLIB_MODE1_MASK = 7,  ///
    DYNLIB_GLOBAL = 8   ///< treats symbols globally
};

/// dynamic library class
class DynLibrary: public NonCopyableAndNonMovable
{
private:
    void* handle;
    static std::mutex mutex;
public:
    DynLibrary();
    /** constructor - loads library
     * \param filename library filename
     * \param flags flags specifies way to load library and a resolving symbols
     */
    DynLibrary(const char* filename, cxuint flags = 0);
    ~DynLibrary();
    
    /** loads library
     * \param filename library filename
     * \param flags flags specifies way to load library and a resolving symbols
     */
    void load(const char* filename, cxuint flags = 0);
    /// unload library
    void unload();
    
    /// get symbol
    void* getSymbol(const char* symbolName);
};

/* parse utilities */

/// skip spaces from cString
inline const char* skipSpaces(const char* s);

inline const char* skipSpaces(const char* s)
{
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' ||
        *s == '\f' || *s == '\v') s++;
    return s;
}

/// skip spaces from cString
inline const char* skipSpacesAtEnd(const char* s, size_t length);

inline const char* skipSpacesAtEnd(const char* s, size_t length)
{
    const char* t = s+length;
    if (t == s) return s;
    for (t--; t != s-1 && (*t == ' ' || *t == '\n' || *t == '\r' || *t == '\t' ||
        *t == '\f' || *t == '\v'); t--);
    return t+1;
}

/// parses integer or float point formatted looks like C-style
/** parses integer or float point from str string. inend can points
 * to end of string or can be null. Function throws ParseException when number in string
 * is out of range, when string does not have number or inend points to string.
 * Function accepts decimal format, octal form (with prefix '0'), hexadecimal form
 * (prefix '0x' or '0X'), and binary form (prefix '0b' or '0B').
 * For floating points function accepts decimal format and binary format.
 * Result is rounded to nearest even
 * (if two values are equally close will be choosen a even value).
 * Currently only IEEE-754 format is supported.
 * \param str input string pointer
 * \param inend pointer points to end of string or null if not end specified
 * \param outend returns end of number in string
 * \return parsed integer value
 */
template<typename T>
extern T cstrtovCStyle(const char* str, const char* inend, const char*& outend);

/// parse environment variable
/**parse environment variable
 * \param envVar - name of environment variable
 * \param defaultValue - value that will be returned if environment variable is not present
 */
template<typename T>
T parseEnvVariable(const char* envVar, const T& defaultValue = T())
{
    const char* var = getenv(envVar);
    if (var == nullptr)
        return defaultValue;
    var = skipSpaces(var);
    if (*var == 0)
        return defaultValue;
    const char* outend;
    try
    { return cstrtovCStyle<T>(var, nullptr, outend); }
    catch(const ParseException& ex)
    { return defaultValue; }
}

extern template
cxuchar parseEnvVariable<cxuchar>(const char* envVar, const cxuchar& defaultValue);

extern template
cxchar parseEnvVariable<cxchar>(const char* envVar, const cxchar& defaultValue);

extern template
cxuint parseEnvVariable<cxuint>(const char* envVar, const cxuint& defaultValue);

extern template
cxint parseEnvVariable<cxint>(const char* envVar, const cxint& defaultValue);

extern template
cxushort parseEnvVariable<cxushort>(const char* envVar, const cxushort& defaultValue);

extern template
cxshort parseEnvVariable<cxshort>(const char* envVar, const cxshort& defaultValue);

extern template
cxulong parseEnvVariable<cxulong>(const char* envVar, const cxulong& defaultValue);

extern template
cxlong parseEnvVariable<cxlong>(const char* envVar, const cxlong& defaultValue);

extern template
cxullong parseEnvVariable<cxullong>(const char* envVar, const cxullong& defaultValue);

extern template
cxllong parseEnvVariable<cxllong>(const char* envVar, const cxllong& defaultValue);

extern template
float parseEnvVariable<float>(const char* envVar, const float& defaultValue);

extern template
double parseEnvVariable<double>(const char* envVar, const double& defaultValue);

#ifndef __UTILITIES_MODULE__

extern template
std::string parseEnvVariable<std::string>(const char* envVar,
              const std::string& defaultValue);

extern template
bool parseEnvVariable<bool>(const char* envVar, const bool& defaultValue);

#endif

/// function class that returns true if first C string is less than second
struct CStringLess
{
    inline bool operator()(const char* c1, const char* c2) const
    { return ::strcmp(c1, c2)<0; }
};

/// function class that returns true if C strings are equal
struct CStringEqual
{
    inline bool operator()(const char* c1, const char* c2) const
    { return ::strcmp(c1, c2)==0; }
};

/// generate hash function for C string
struct CStringHash
{
    size_t operator()(const char* c) const;
};

/// counts leading zeroes for 32-bit unsigned integer. For zero behavior is undefined
inline cxuint CLZ32(uint32_t v);
/// counts leading zeroes for 64-bit unsigned integer. For zero behavior is undefined
inline cxuint CLZ64(uint64_t v);

inline cxuint CLZ32(uint32_t v)
{
#ifdef __GNUC__
    return __builtin_clz(v);
#else
    cxuint count = 0;
    for (uint32_t t = 1U<<31; t > v; t>>=1, count++);
    return count;
#endif
}

inline cxuint CLZ64(uint64_t v)
{
#ifdef __GNUC__
    return __builtin_clzll(v);
#else
    cxuint count = 0;
    for (uint64_t t = 1ULL<<63; t > v; t>>=1, count++);
    return count;
#endif
}

/// safely compares sum of two unsigned integers with other unsigned integer
template<typename T, typename T2>
inline bool usumGt(T a, T b, T2 c)
{ return ((a+b)>c) || ((a+b)<a); }

/// safely compares sum of two unsigned integers with other unsigned integer
template<typename T, typename T2>
inline bool usumGe(T a, T b, T2 c)
{ return ((a+b)>=c) || ((a+b)<a); }

/// escape string into C-style string
extern std::string escapeStringCStyle(const std::string& str);

/// escapes string into C-style string
extern std::string escapeStringCStyle(size_t strSize, const char* str);

/// escapes string into C-style string
/**
 * \param strSize string size
 * \param str string
 * \param outMaxSize output max size (including null-character)
 * \param outStr output string
 * \return number of processed input characters
 */
extern size_t escapeStringCStyle(size_t strSize, const char* str,
                 size_t outMaxSize, char* outStr, size_t& outSize);

/// parses unsigned integer regardless locales
/** parses unsigned integer in decimal form from str string. inend can points
 * to end of string or can be null. Function throws ParseException when number in string
 * is out of range, when string does not have number or inend points to string.
 * \param str input string pointer
 * \param inend pointer points to end of string or null if not end specified
 * \param outend returns end of number in string
 * \return parsed integer value
 */
extern cxuint cstrtoui(const char* str, const char* inend, const char*& outend);

extern int64_t cstrtoiXCStyle(const char* str, const char* inend,
             const char*& outend, cxuint bits);

extern uint64_t cstrtouXCStyle(const char* str, const char* inend,
             const char*& outend, cxuint bits);

extern uint64_t cstrtofXCStyle(const char* str, const char* inend,
             const char*& outend, cxuint expBits, cxuint mantisaBits);

/* cstrtovcstyle impls */

template<> inline
cxuchar cstrtovCStyle<cxuchar>(const char* str, const char* inend, const char*& outend)
{ return cstrtouXCStyle(str, inend, outend, sizeof(cxuchar)<<3); }

template<> inline
cxchar cstrtovCStyle<cxchar>(const char* str, const char* inend, const char*& outend)
{ return cstrtoiXCStyle(str, inend, outend, sizeof(cxchar)<<3); }

template<> inline
cxuint cstrtovCStyle<cxuint>(const char* str, const char* inend, const char*& outend)
{ return cstrtouXCStyle(str, inend, outend, sizeof(cxuint)<<3); }

template<> inline
cxint cstrtovCStyle<cxint>(const char* str, const char* inend, const char*& outend)
{ return cstrtoiXCStyle(str, inend, outend, sizeof(cxint)<<3); }

template<> inline
cxushort cstrtovCStyle<cxushort>(const char* str, const char* inend, const char*& outend)
{ return cstrtouXCStyle(str, inend, outend, sizeof(cxushort)<<3); }

template<> inline
cxshort cstrtovCStyle<cxshort>(const char* str, const char* inend, const char*& outend)
{ return cstrtoiXCStyle(str, inend, outend, sizeof(cxshort)<<3); }

template<> inline
cxulong cstrtovCStyle<cxulong>(const char* str, const char* inend, const char*& outend)
{ return cstrtouXCStyle(str, inend, outend, sizeof(cxulong)<<3); }

template<> inline
cxlong cstrtovCStyle<cxlong>(const char* str, const char* inend, const char*& outend)
{ return cstrtoiXCStyle(str, inend, outend, sizeof(cxlong)<<3); }

template<> inline
cxullong cstrtovCStyle<cxullong>(const char* str, const char* inend, const char*& outend)
{ return cstrtouXCStyle(str, inend, outend, sizeof(cxullong)<<3); }

template<> inline
cxllong cstrtovCStyle<cxllong>(const char* str, const char* inend, const char*& outend)
{ return cstrtoiXCStyle(str, inend, outend, sizeof(cxllong)<<3); }

template<> inline
float cstrtovCStyle<float>(const char* str, const char* inend, const char*& outend)
{
    union {
        float f;
        uint32_t u;
    } v;
    v.u = cstrtofXCStyle(str, inend, outend, 8, 23);
    return v.f;
}

template<> inline
double cstrtovCStyle<double>(const char* str, const char* inend, const char*& outend)
{
    union {
        double d;
        uint64_t u;
    } v;
    v.u = cstrtofXCStyle(str, inend, outend, 11, 52);
    return v.d;
}

/// parses half float formatted looks like C-style
/** parses half floating point from str string. inend can points
 * to end of string or can be null. Function throws ParseException when number in string
 * is out of range, when string does not have number or inend points to string.
 * Function accepts decimal format and binary format. Result is rounded to nearest even
 * (if two values are equally close will be choosen a even value).
 * Currently only IEEE-754 format is supported.
 * \param str input string pointer
 * \param inend pointer points to end of string or null if not end specified
 * \param outend returns end of number in string
 * \return parsed floating point value
 */
cxushort cstrtohCStyle(const char* str, const char* inend, const char*& outend);

inline cxushort cstrtohCStyle(const char* str, const char* inend, const char*& outend)
{ return cstrtofXCStyle(str, inend, outend, 5, 10); }


extern size_t uXtocstrCStyle(uint64_t value, char* str, size_t maxSize, cxuint radix,
            cxuint width, bool prefix);

extern size_t iXtocstrCStyle(int64_t value, char* str, size_t maxSize, cxuint radix,
            cxuint width, bool prefix);

/// formats an integer
/** formats an integer in C-style formatting.
 * \param value integer value
 * \param str output string
 * \param maxSize max size of string (including null-character)
 * \param radix radix of digits (2, 8, 10, 16)
 * \param width max number of digits in number
 * \param prefix adds required prefix if true
 * \return length of output string (excluding null-character)
 */
template<typename T>
extern size_t itocstrCStyle(T value, char* str, size_t maxSize, cxuint radix = 10,
       cxuint width = 0, bool prefix = true);

template<> inline
size_t itocstrCStyle<cxuchar>(cxuchar value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return uXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxchar>(cxchar value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return iXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxushort>(cxushort value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return uXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxshort>(cxshort value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return iXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxuint>(cxuint value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return uXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxint>(cxint value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return iXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxulong>(cxulong value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return uXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxlong>(cxlong value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return iXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxullong>(cxullong value, char* str, size_t maxSize, cxuint radix,
       cxuint width , bool prefix)
{ return uXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

template<> inline
size_t itocstrCStyle<cxllong>(cxllong value, char* str, size_t maxSize, cxuint radix,
       cxuint width, bool prefix)
{ return iXtocstrCStyle(value, str, maxSize, radix, width, prefix); }

extern size_t fXtocstrCStyle(uint64_t value, char* str, size_t maxSize,
        bool scientific, cxuint expBits, cxuint mantisaBits);

/// formats half float in C-style
/** formats to string the half float in C-style formatting. This function handles 2 modes
 * of printing value: human readable and scientific. Scientific mode forces form with
 * decimal exponent.
 * Currently only IEEE-754 format is supported.
 * \param value float value
 * \param str output string
 * \param maxSize max size of string (including null-character)
 * \param scientific enable scientific mode
 * \return length of output string (excluding null-character)
 */
size_t htocstrCStyle(cxushort value, char* str, size_t maxSize,
                            bool scientific = false);

inline size_t htocstrCStyle(cxushort value, char* str, size_t maxSize, bool scientific)
{ return fXtocstrCStyle(value, str, maxSize, scientific, 5, 10); }

/// formats single float in C-style
/** formats to string the single float in C-style formatting. This function handles 2 modes
 * of printing value: human readable and scientific. Scientific mode forces form with
 * decimal exponent.
 * Currently only IEEE-754 format is supported.
 * \param value float value
 * \param str output string
 * \param maxSize max size of string (including null-character)
 * \param scientific enable scientific mode
 * \return length of output string (excluding null-character)
 */
size_t ftocstrCStyle(float value, char* str, size_t maxSize,
                            bool scientific = false);

inline size_t ftocstrCStyle(float value, char* str, size_t maxSize, bool scientific)
{
    union {
        float f;
        uint32_t u;
    } v;
    v.f = value;
    return fXtocstrCStyle(v.u, str, maxSize, scientific, 8, 23);
}

/// formats double float in C-style
/** formats to string the double float in C-style formatting. This function handles 2 modes
 * of printing value: human readable and scientific. Scientific mode forces form with
 * decimal exponent.
 * Currently only IEEE-754 format is supported.
 * \param value float value
 * \param str output string
 * \param maxSize max size of string (including null-character)
 * \param scientific enable scientific mode
 * \return length of output string (excluding null-character)
 */
size_t dtocstrCStyle(double value, char* str, size_t maxSize,
                            bool scientific = false);

inline size_t dtocstrCStyle(double value, char* str, size_t maxSize, bool scientific)
{
    union {
        double d;
        uint64_t u;
    } v;
    v.d = value;
    return fXtocstrCStyle(v.u, str, maxSize, scientific, 11, 52);
}

/* file system utilities */

/// returns true if path refers to directory
extern bool isDirectory(const char* path);

/// load data from file (any regular or pipe or device)
/**
 * \param filename filename
 * \return array of data (can be greater than size of data from file)
 */
extern Array<cxbyte> loadDataFromFile(const char* filename);

/*
 * Reference support
 */

/// reference countable object
class RefCountable
{
private:
    std::atomic<size_t> refCount;
public:
    /// constructor
    RefCountable() : refCount(1)
    { }
    virtual ~RefCountable();
    
    /// reference object
    void reference()
    {
        refCount.fetch_add(1);
    }
    
    /// unreference object (and delete object when no references)
    void unreference()
    {
        if (refCount.fetch_sub(1) == 1)
            delete this; /* delete this object */
    }
};

/// reference pointer based on Glibmm refptr
template<typename T>
class RefPtr
{
private:
    T* ptr;
public:
    RefPtr(): ptr(nullptr)
    { }
    
    explicit RefPtr(T* inputPtr) : ptr(inputPtr)
    { }
    
    RefPtr(const RefPtr<T>& refPtr)
        : ptr(refPtr.ptr)
    {
        if (ptr != nullptr)
            ptr->reference();
    }
    
    RefPtr(RefPtr<T>&& refPtr)
        : ptr(refPtr.ptr)
    { refPtr.ptr = nullptr; }
    
    ~RefPtr()
    {
        if (ptr != nullptr)
            ptr->unreference();
    }
    
    RefPtr<T>& operator=(const RefPtr<T>& refPtr)
    {
        if (ptr != nullptr)
            ptr->unreference();
        if (refPtr.ptr != nullptr)
            refPtr.ptr->reference();
        ptr = refPtr.ptr;
        return *this;
    }
    RefPtr<T>& operator=(RefPtr<T>&& refPtr)
    {
        if (ptr != nullptr)
            ptr->unreference();
        ptr = refPtr.ptr;
        refPtr.ptr = nullptr;
        return *this;
    }
    
    bool operator==(const RefPtr<T>& refPtr) const
    { return ptr == refPtr.ptr; }
    bool operator!=(const RefPtr<T>& refPtr) const
    { return ptr != refPtr.ptr; }
    
    operator bool() const
    { return ptr!=nullptr; }
    
    T* operator->() const
    { return ptr; }
    
    void reset()
    {
        if (ptr != nullptr)
            ptr->unreference();
        ptr = nullptr;
    }
    
    void swap(RefPtr<T>& refPtr)
    {
        T* tmp = ptr;
        ptr = refPtr.ptr;
        refPtr.ptr = tmp;
    }
};

}

#endif
