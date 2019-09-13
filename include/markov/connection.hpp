#pragma once
#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <deque>
#include <markov/utils.hpp>
#include <markov/exception.hpp>

namespace sqlite { class database; };

namespace Markov {
	using rowid_t = long long;
	using MarkovDeque = std::deque<rowid_t>;

	namespace Exception {
		inline char const BadConfigM[] = "Invalid configuration";
		using BadConfig = MarkovExceptionTemplate<BadConfigM>;
		inline char const ConfigExistM[] = "Configuration already exist";
		using ConfigExist = MarkovExceptionTemplate<ConfigExistM>;
		inline char const ReadOnlyM[] = "Database is in readonly mode";
		using ReadOnly = MarkovExceptionTemplate<ReadOnlyM>;
		inline char const ConfigNotFoundM[] = "Requested config not found";
		using ConfigNotFound = MarkovExceptionTemplate<ConfigNotFoundM>;
		};
	class MARKOV_SHARED_EXPORT ChainSettings {
		public:
			// TODO: make it possible to change regex type
			std::string iter; // Add matching stuff into chain…
			std::string prefixmiddle = ""; // … and insert these inbetween when outputing
			unsigned int N = 3; // Number of previous states considered when generating next one
			bool splitstr = false; // Force add linebreaks even if pattern doesn't match them
			std::optional<std::string> separator; // Break chains on it (indicates end)
			unsigned long long int maxgen = 0; // Stop after printing maxgen points of data (0 means never force stop)
			bool rndstart = false; // Start from random entery and not NULL/NULL/… one (usually bad idea)
			std::string sqlite_table; // Name of relationship table (actual chains with numbers)
			std::string sqlite_table_dict; // Name of dictionary table (word <-> [row]id)

			bool isValid() const; // Check is current config acceptable
		};

	struct TrainData;

	class MARKOV_SHARED_EXPORT Connection {
		public:
			Connection(const std::string& = "file:memdb1?mode=memory"s, bool = false);

			void AddConfig(const std::string&, const ChainSettings&); // Add new chain
			std::optional<ChainSettings> GetConfig(const std::string&); // Get config for chain (if exists)
			bool DeleteConfig(const std::string&); // Delete config (no errors if one didn't existed)

			void Train(const std::string&, const std::vector<std::string_view>&);
			void Output(const std::string&, std::ostream&, std::optional<unsigned long long int> = std::nullopt);
		private:
			void TrainFull(std::string_view, const TrainData&);
			void TrainPart(std::string_view, const TrainData&);
			void TrainFinal(std::string_view, const TrainData&);
			std::shared_ptr<sqlite::database> db; // Use it to avoid exporting SQLite stuff
			bool readonly;

			void checkWritable();
			void initDatabase();
		};
	};
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 
