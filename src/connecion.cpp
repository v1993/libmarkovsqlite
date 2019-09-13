#include <string_view>
#include <regex>
#include <sstream>

#include "markov/connection.hpp"
#include "sqlite_modern_cpp.h"

using namespace std::literals;

// Regex related stuff
#ifdef USE_STD_REGEX
#include <regex>
namespace {
	using sviterator = std::regex_token_iterator<std::string_view::const_iterator>;
	using regex_type = std::regex;
	using regex_error = std::regex_error;
	constexpr auto regex_grammar = std::regex::ECMAScript;
	constexpr auto regex_optimize = std::regex::optimize;

	std::string_view matchToSv(std::csub_match m) {
		if (!m.matched) return {};

		return { m.first, static_cast<size_t>(m.second - m.first) };
		}
	}
#else
#include <boost/regex.hpp>
namespace {
	using sviterator = boost::regex_token_iterator<std::string_view::const_iterator>;
	using regex_type = boost::regex;
	using regex_error = boost::regex_error;
	constexpr auto regex_grammar = boost::regex::perl;
	constexpr auto regex_optimize = boost::regex::optimize;

	std::string_view matchToSv(boost::csub_match m) {
		if (!m.matched) return {};

		return { m.first, static_cast<size_t>(m.second - m.first) };
		}
	}
#endif

namespace {
	const std::string metainfotable = "markov_metainfo"s;

	std::string joinStr(std::vector<std::string>& v, std::string delim) {
		std::ostringstream ss;

		for (size_t i = 0; i < v.size(); ++i) {
				if (i != 0) ss << delim;

				ss << v[i];
				}

		return ss.str();
		}

	std::string repeatDelim(std::string str, std::string delim, unsigned int cnt) { // Correct for cnt > 0
		std::string join = str + delim; // Cache
		std::string res;
		res.reserve(str.length()*cnt + delim.length() * (cnt - 1));

		for (unsigned int i = 1; i < cnt; ++i) {
				res += join;
				}

		res += str;
		return res;
		}

	class SqlTransaction { // RAII class to rollback on error
		public:
			SqlTransaction(std::shared_ptr<sqlite::database> db_): db(db_) { *db << "begin;"; };
			~SqlTransaction() { if (!finished) rollback(); };
			void commit() { if (!finished) { *db << "commit;"; finished = true; } };
			void rollback() { if (!finished) { *db << "rollback;"; finished = true; } };

			// Disable both copying and moving
			SqlTransaction(const SqlTransaction&) = delete;
			SqlTransaction& operator=(const SqlTransaction&) = delete;
			SqlTransaction(SqlTransaction&&) = delete;

		private:
			std::shared_ptr<sqlite::database> db;
			bool finished = false;
		};

	template<typename T>
	void shiftDeque(std::deque<T>& dq, T& elem) {
		dq.pop_front();
		dq.push_back(elem);
		};

	std::string indexName(const std::string& tab) {
		return tab + "_index"s;
		};
	}

namespace Markov {
	struct TrainData {
		regex_type iter;
		std::optional<regex_type> sep;
		Markov::ChainSettings conf;
		sviterator badIter;
		std::function<void(std::string_view, bool)> func;
		};

	bool ChainSettings::isValid() const {
		return
		N != 0 && // At least one previous element is stored
		(separator ? !(*separator).empty() : true) && // Eiter no separator or non-empty one
		!sqlite_table.empty() && // TODO: add better validation
		!sqlite_table_dict.empty();
		};

	Connection::Connection(const std::string& connstr, bool ro):
		readonly(ro) {
		sqlite::sqlite_config conf;
		conf.flags = (ro ? (sqlite::OpenFlags::READONLY) : (sqlite::OpenFlags::READWRITE | sqlite::OpenFlags::CREATE)) | sqlite::OpenFlags::NOMUTEX | sqlite::OpenFlags::URI;
		conf.encoding = sqlite::Encoding::UTF8; // UTF8 FTW!
		db = std::make_shared<sqlite::database>(connstr, conf);
		*db << "PRAGMA synchronous = 0;"; // Our db isn't critical and need huge speed
		*db << "PRAGMA journal_mode = MEMORY;";

		initDatabase();
		};

	void Connection::initDatabase() {
		// This table maps ChainSettings 1:1 (+name)
		// TODO: check does table exist if in readonly mode
		// Kinda optional, it's up to user to shoot their legs
		if (!readonly) {
				// TODO: not using raw literal because it breaks kdevelop's formatter
				(*db) << "CREATE TABLE IF NOT EXISTS "s + metainfotable +
					  "("
					  "name TEXT NOT NULL UNIQUE,"
					  "iter TEXT NOT NULL,"
					  "prefixmiddle TEXT NOT NULL,"
					  "N UNSIGNED INTEGER NOT NULL,"
					  "splitstr TINYINT NOT NULL,"
					  "separator TEXT,"
					  "maxgen UNSIGNED BIGINT NOT NULL,"
					  "rndstart TINYINT NOT NULL,"
					  "sqlite_table TEXT NOT NULL UNIQUE," // To make shooting your leg harder
					  "sqlite_table_dict TEXT NOT NULL"
					  ");";
				}
		};

	void Connection::checkWritable() {
		if (readonly) throw Exception::ReadOnly();
		}

	void Connection::AddConfig(const std::string& name, const ChainSettings& conf) {
		if (!conf.isValid()) throw Exception::BadConfig();

		// TODO: move into isValid()
		try {
				regex_type r(conf.iter, regex_grammar);

				if (conf.separator) regex_type r1(*conf.separator, regex_grammar);
				}
		catch (regex_error) {
				throw Exception::BadConfig();
				};

			// This function is a popular place to crash
			// Give it some love
			SqlTransaction trans(db);
			{

			try {
					(*db) << "INSERT INTO "s + metainfotable + " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
						  << name
						  << conf.iter
						  << conf.prefixmiddle
						  << conf.N
						  << conf.splitstr
						  << conf.separator
						  << conf.maxgen
						  << conf.rndstart
						  << conf.sqlite_table
						  << conf.sqlite_table_dict
						  ;
					}
			catch (sqlite::errors::constraint) {
					throw Exception::ConfigExist();
					}

			// Create dict (if not exist)
			*db << "CREATE TABLE IF NOT EXISTS "s + conf.sqlite_table_dict + " (id INTEGER PRIMARY KEY ASC, str TEXT NOT NULL UNIQUE ON CONFLICT IGNORE);"s;
			*db << "INSERT OR IGNORE INTO " + conf.sqlite_table_dict + "(rowid, str) VALUES(?, ?);" << 0 << ""; // Special value
			// Create actual chain table
				{
				std::vector<std::string> vec = {"rstr INTEGER NOT NULL"};

				for (unsigned int i = 1; i <= conf.N; ++i) {
						vec.push_back("str" + std::to_string(i) + " INTEGER NOT NULL"); // All "base" strings
						}

				*db << "CREATE TABLE " + conf.sqlite_table + " (" + joinStr(vec, ", ") + ");";
				}
			}
			trans.commit();
		};

	std::optional<ChainSettings> Connection::GetConfig(const std::string& name) {
		// Never mind warning, it is intended to run only once
		for (auto&& row : (*db) << "SELECT * FROM "s + metainfotable + " WHERE name=?;" << name) {
				ChainSettings conf;
				std::string n;
				row
						>> n
						>> conf.iter
						>> conf.prefixmiddle
						>> conf.N
						>> conf.splitstr
						>> conf.separator
						>> conf.maxgen
						>> conf.rndstart
						>> conf.sqlite_table
						>> conf.sqlite_table_dict
						;
				return conf;
				}

		return std::nullopt;
		};

	bool Connection::DeleteConfig(const std::string& name) {
		auto conf = GetConfig(name);

		if (!conf) return false;

			{(*db) << "DELETE FROM "s + metainfotable + " WHERE name=?;"s << name;}
		// Clean up stuff
		(*db) << "DROP TABLE "s + (*conf).sqlite_table + ";"s;
		// Few configurations may share dict
		bool drop = true;
		(*db) << "SELECT count(*) FROM "s + metainfotable + " WHERE sqlite_table_dict=?;"s << (*conf).sqlite_table_dict
		>> [&drop](rowid_t cnt) {
			drop = (cnt == 0);
			};

		if (drop)(*db) << "DROP TABLE "s + (*conf).sqlite_table_dict + ";"s;

		return true;
		};

	void Connection::Train(const std::string& name, const std::vector<std::string_view>& src) {
		checkWritable();
		auto conf_ = GetConfig(name);

		if (!conf_) throw Exception::ConfigNotFound();

		TrainData td;
		td.conf = std::move(*conf_);
		td.iter = regex_type(td.conf.iter, regex_grammar | regex_optimize);

		if (td.conf.separator) td.sep = regex_type(*td.conf.separator, regex_grammar | regex_optimize);

			{
			// Build dict
			SqlTransaction trans(db);

			// Drop index first
			*db << "DROP INDEX IF EXISTS "s + indexName(td.conf.sqlite_table_dict) + ";"s;

			auto dictPstm = *db << "INSERT INTO " + td.conf.sqlite_table_dict + " (str) VALUES(?);";
			td.func = [&dictPstm](std::string_view str, bool) {
				dictPstm << str;
				dictPstm++;
				};

			for (auto src1 : src) {
					TrainFull(src1, td);
					}

			// Rebuild index

			*db << "CREATE INDEX " + indexName(td.conf.sqlite_table_dict) + " ON " + td.conf.sqlite_table_dict + " (str);";

			trans.commit();
			}

			{
			// Build actual chain
			SqlTransaction trans(db);

			// Drop index first
			*db << "DROP INDEX IF EXISTS "s + indexName(td.conf.sqlite_table) + ";"s;

			auto pstm = *db << "INSERT INTO " + td.conf.sqlite_table + " VALUES(" + repeatDelim("?", ", ", td.conf.N + 1) + ");";
			auto dictSelectPstm = *db << "SELECT rowid FROM " + td.conf.sqlite_table_dict + " WHERE str=? LIMIT 1;";
			MarkovDeque dq(td.conf.N, 0);

			td.func = [&pstm, &dictSelectPstm, &dq, &td](std::string_view str, bool reset) {
				rowid_t id = 0;
				dictSelectPstm << str >> id;
				pstm << id;

				for (int i = 0; i < td.conf.N; ++i) {
						pstm << dq.at(i);
						}

				pstm++;
				shiftDeque(dq, id);

				if (reset) {
						dq = MarkovDeque(td.conf.N, 0);
						}
				};

			for (auto src1 : src) {
					TrainFull(src1, td);
					// No need to explictly reset dq as TrainPart does so
					}

				{
				std::vector<std::string> vec;

				for (unsigned i = 1; i <= td.conf.N; ++i) {
						vec.push_back("str" + std::to_string(i));
						}

				*db << "CREATE INDEX " + indexName(td.conf.sqlite_table) + " ON " + td.conf.sqlite_table + " (" + joinStr(vec, ", ") + ");";
				}

			trans.commit();

			}

		};

	void Connection::TrainFull(std::string_view src, const TrainData& td) {
		if (td.sep) {
				for (auto iter = sviterator(src.begin(), src.end(), *td.sep, -1); iter != td.badIter; iter++) {
						TrainPart(matchToSv(*iter), td);
						}
				}
		else {
				TrainPart(src, td);
				}
		};

	void Connection::TrainPart(std::string_view src, const TrainData& td) {
		if (td.conf.splitstr) {
				regex_type nline("\\n+");

				for (auto iter = sviterator(src.begin(), src.end(), nline, -1); iter != td.badIter; iter++) {
						TrainFinal(matchToSv(*iter), td);
						td.func("\n", false);
						}
				}
		else {
				TrainFinal(src, td);
				}

		td.func("", true);
		};

	void Connection::TrainFinal(std::string_view src, const TrainData& td) {
		for (auto iter = sviterator(src.begin(), src.end(), td.iter); iter != td.badIter; iter++) {
				td.func(matchToSv(*iter), false);
				}
		}

	void Connection::Output(const std::string& name, std::ostream& ostr, std::optional<unsigned long long int> maxgenLocal) {
		auto conf = GetConfig(name);

		if (!conf) throw Exception::ConfigNotFound();
		unsigned long long int maxgen = (maxgenLocal ? (*maxgenLocal) : conf->maxgen);

		std::string pstmString;
			{
			std::vector<std::string> vec;

			for (int i = 1; i <= conf->N; ++i) {
					vec.push_back("str" + std::to_string(i) + "=?"); // All "base" strings
					}

			pstmString = "SELECT rstr FROM " + conf->sqlite_table + " WHERE " + joinStr(vec, " AND ") + " ORDER BY RANDOM() LIMIT 1;";
			}

		auto pstm = *db << pstmString;
		auto dictStrPastm = *db << "SELECT str FROM " + conf->sqlite_table_dict + " WHERE rowid=? LIMIT 1;";

		MarkovDeque dq(conf->N, 0);

		if (conf->rndstart) {
				auto row = *db << "SELECT * FROM " + conf->sqlite_table + " ORDER BY RANDOM() LIMIT 1;";
				rowid_t id;
				row >> id;

				for (unsigned int i = 0; i < conf->N; ++i) row >> dq[i];
				}

		auto outGet = [&conf, &pstm, &dq]() {
			for (size_t i = 0; i < conf->N; ++i) {
					pstm << dq.at(i);
					}

			rowid_t res = 0;
			pstm >> res;
			return res;
			};

		unsigned long long int n = 0;
		std::string stringRes;

		for (rowid_t res = outGet(); res != 0; res = outGet()) {
				dictStrPastm << res >> stringRes;
				ostr << stringRes;

				if (stringRes != "\n") ostr << conf->prefixmiddle;

				if (maxgen > 0 && ++n == maxgen) {
						return;
						}

				shiftDeque(dq, res);
				}
		}
	}

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; ;
