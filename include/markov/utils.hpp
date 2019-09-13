#pragma once
#include <string>

#ifdef _WIN32
#ifdef shared_EXPORTS
#define MARKOV_SHARED_EXPORT __declspec(dllexport)
#else
#define MARKOV_SHARED_EXPORT __declspec(dllimport)
#endif
#else
#define MARKOV_SHARED_EXPORT
#endif

namespace Markov {
	using namespace std::string_literals;
	};
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 
