#pragma once
#define __USE_C_
#ifdef __USE_C_

#ifdef  _MSC_VER
#define ACCEPT_VLD_INCLUDE 1
#else
#define ACCEPT_VLD_INCLUDE 0
#endif

#define CCSTD_LANGUAGE_C
#define CCSTD_NAMESPACE_BEGIN
#define CCSTD_NAMESPACE_END
#else
#define CCSTD_LANGUAGE_CPP
#define CCSTD_NAMESPACE_BEGIN	namespace CCSTD{
#define CCSTD_NAMESPACE_END		}
#endif

#if defined(USE_WINDOWS) || defined(_WIN32)
	#ifndef _WIN32
	#error "unable Mark the Exsiting Windows! detecting by checking the _WIN32!"
	#endif
#define CCSTD_USE_WINDOWS
#elif defined(USE_LINUX) || defined(__linux__)
// Use linux
	#ifndef (__linux__)
	#error "unable Mark the Exsiting Linux, detecting by checking the _WIN32!"
	#endif
#define CCSTD_USE_LINUX
#else
#error "unable to support other operating system!"
#endif


