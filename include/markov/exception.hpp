#pragma once
#include <stdexcept>
#include <string_view>
#include <markov/utils.hpp>

namespace Markov {
	class MARKOV_SHARED_EXPORT MarkovException: public std::logic_error {
		public:
			MarkovException(const std::string& what_arg = "generic markov exception"s): std::logic_error(what_arg) {};
		};
	template<const char* const err> class MarkovExceptionTemplate: public MarkovException {
		public:
			MarkovExceptionTemplate(): MarkovException(err) {};
		};
	};

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 
