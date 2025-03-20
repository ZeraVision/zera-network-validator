#ifndef DB_H
#define DB_H
#include <leveldb/db.h>
#include <leveldb/write_batch.h> 
#include <vector>
#include <boost/multiprecision/cpp_int.hpp>
#include "const.h"
#include "zera_status.h"

using namespace boost::multiprecision;

class database {
public:
	static int open_db(leveldb::DB*& db, leveldb::Options& options, leveldb::Status& status, const std::string& db_type);
	static void close_db(leveldb::DB*& db);
	static int store_single(leveldb::DB* db, leveldb::Status& status, const std::string& key, const std::string& data);
	static int get_data(leveldb::DB* db, leveldb::Status& status, const std::string& key, std::string& data);
	static int store_batch(leveldb::DB* db, leveldb::Status& status, leveldb::WriteBatch& batch);
	static int get_all_data(leveldb::DB* db, std::vector<std::string>& keys, std::vector<std::string>& values);
	static int get_multi_data(leveldb::DB* db, std::string& start_key, int amount, std::vector<std::string>& keys, std::vector<std::string>& values);
	static int get_last_data(leveldb::DB* db, std::string& last_key, std::string& last_value);
	static int get_last_amount(leveldb::DB* db, std::vector<std::string>& keys, std::vector<std::string>& values, int amount);
	static int remove_single(leveldb::DB* db, leveldb::Status& status, const std::string& key);
	static void commit(leveldb::DB* db, leveldb::Status& status);
	static int get_all_keys(leveldb::DB* db, std::vector<std::string>& keys);
	static int compact_all(leveldb::DB* db);
};
#endif