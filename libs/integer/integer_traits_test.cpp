/* boost integer_traits.hpp tests
 *
 * Copyright Jens Maurer 2000
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * $Id$
 *
 * Revision history
 *  2000-02-22  Small improvements by Beman Dawes
 *  2000-06-27  Rework for better MSVC and BCC co-operation
 */

#include <iostream>
#include <boost/integer_traits.hpp>
// use int64_t instead of long long for better portability
#include <boost/cstdint.hpp>

#define BOOST_INCLUDE_MAIN
#include <boost/test/test_tools.hpp>

/*
 * General portability note:
 * MSVC mis-compiles explicit function template instantiations.
 * For example, f<A>() and f<B>() are both compiled to call f<A>().
 * BCC is unable to implicitly convert a "const char *" to a std::string
 * when using explicit function template instantiations.
 *
 * Therefore, avoid explicit function template instantiations.
 */

#if defined(BOOST_MSVC) && (BOOST_MSVC <= 1300)
template<typename T> inline T make_char_numeric_for_streaming(T x) { return x; }
namespace fix{
inline int make_char_numeric_for_streaming(char c) { return c; }
inline int make_char_numeric_for_streaming(signed char c) { return c; }
inline int make_char_numeric_for_streaming(unsigned char c) { return c; }
}
using namespace fix;
#else
template<typename T> inline T make_char_numeric_for_streaming(T x) { return x; }
inline int make_char_numeric_for_streaming(char c) { return c; }
inline int make_char_numeric_for_streaming(signed char c) { return c; }
inline int make_char_numeric_for_streaming(unsigned char c) { return c; }
#endif

template<class T>
void runtest(const char * type, T)
{
  typedef boost::integer_traits<T> traits;
  std::cout << "Checking " << type
            << "; min is " << make_char_numeric_for_streaming((traits::min)())
            << ", max is " << make_char_numeric_for_streaming((traits::max)())
            << std::endl;
  BOOST_CHECK(traits::is_specialized);
  BOOST_CHECK(traits::is_integer);
  BOOST_CHECK(traits::is_integral == true);
  BOOST_CHECK(traits::const_min == (traits::min)());
  BOOST_CHECK(traits::const_max == (traits::max)());
}

int test_main(int, char*[])
{
  runtest("bool", bool());
  runtest("char", char());
  typedef signed char signed_char;
  runtest("signed char", signed_char());
  typedef unsigned char unsigned_char;
  runtest("unsigned char", unsigned_char());
  runtest("wchar_t", wchar_t());
  runtest("short", short());
  typedef unsigned short unsigned_short;
  runtest("unsigned short", unsigned_short());
  runtest("int", int());
  typedef unsigned int unsigned_int;
  runtest("unsigned int", unsigned_int());
  runtest("long", long());
  typedef unsigned long unsigned_long;
  runtest("unsigned long", unsigned_long());
#if !defined(BOOST_NO_INT64_T) && (!defined(BOOST_MSVC) || BOOST_MSVC > 1300) && !defined(__BORLANDC__) && !defined(__BEOS__)
  //
  // MS/Borland compilers can't support 64-bit member constants
  // BeOS doesn't have specialisations for long long in SGI's <limits> header.
  runtest("int64_t (possibly long long)", boost::int64_t());
  runtest("uint64_t (possibly unsigned long long)", boost::uint64_t());
#else
  std::cout << "Skipped int64_t and uint64_t" << std::endl;
#endif
  // Some compilers don't pay attention to std:3.6.1/5 and issue a
  // warning here if "return 0;" is omitted.
  return 0;
}

