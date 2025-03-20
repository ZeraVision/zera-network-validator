#include "database.h"
#include "reorg.h"
#include <locale>
#include <iostream>
#include <string>
#include <leveldb/iterator.h>
#include "base58.h"
#include <filesystem>
#include "../logging/logging.h"


int database::open_db(leveldb::DB*& db, leveldb::Options& options, leveldb::Status& status, const std::string& db_type)
{
    options.create_if_missing = true;
    status = leveldb::DB::Open(options, db_type, &db);

    if (!status.ok()) {
        return 0;
    }

    return 1;
}

void database::close_db(leveldb::DB*& db)
{
    leveldb::Status status;
    commit(db, status);
    delete db;
    db = nullptr;
}
int database::store_single(leveldb::DB* db, leveldb::Status& status, const std::string& key, const std::string& data)
{
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }
    status = db->Put(leveldb::WriteOptions(), key, data);
    if (!status.ok()) {
        return 0;
    }
    commit(db, status);
    return 1;
}

int database::get_data(leveldb::DB* db, leveldb::Status& status, const std::string&  key, std::string& data)
{
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }

    status = db->Get(leveldb::ReadOptions(), key, &data);
    if (!status.ok() || status.IsNotFound()) {
        return 0;
    }
    return 1;
}

int database::store_batch(leveldb::DB* db, leveldb::Status& status, leveldb::WriteBatch& batch)
{
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }

    status = db->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        return 0;
    }
    commit(db, status);
    return 1;
}

int database::get_all_data(leveldb::DB* db, std::vector<std::string>& keys, std::vector<std::string>& values)
{
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }

    leveldb::Iterator* iterator = db->NewIterator(leveldb::ReadOptions());
    int x = 0;
    for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
        std::string key = iterator->key().ToString();
        std::string value = iterator->value().ToString();

        if (value != "commit_marker_value" && key != "commit_marker_key")
        {
            keys.push_back(key);
            values.push_back(value);
        }

        std::vector<uint8_t> base64_key(key.begin(), key.end());

        x++;
    }

    if (!iterator->status().ok()) {
        delete iterator;
        return 0;
    }

    delete iterator;

    if(keys.size() <= 0 || values.size() <= 0){
        return 0;
    }

    return 1;
}

int database::get_multi_data(leveldb::DB* db, std::string& start_key, int amount, std::vector<std::string>& keys, std::vector<std::string>& values)
{
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }

    leveldb::Iterator* iterator = db->NewIterator(leveldb::ReadOptions());
    int x = 0;

    // Seek to the key after the starting key
    iterator->Seek(start_key);
    if (iterator->Valid()) {
        if (start_key == EMPTY_KEY)
        {
            std::string key = iterator->key().ToString();
            std::string value = iterator->value().ToString();
            if (value != "commit_marker_value" && key != "commit_marker_key")
            {
                keys.push_back(key);
                values.push_back(value);
            }
            x++;
        }
        iterator->Next();  // Move to the next key
    }

    for (; iterator->Valid() && x < amount; iterator->Next()) {
        std::string key = iterator->key().ToString();
        std::string value = iterator->value().ToString();

        if (value != "commit_marker_value" && key != "commit_marker_key")
        {
            keys.push_back(key);
            values.push_back(value);
        }

        x++;
    }

    if (!iterator->status().ok()) {
        delete iterator;
        return 0;
    }

    delete iterator;

    return 1;
}
int database::get_last_amount(leveldb::DB* db, std::vector<std::string>& keys, std::vector<std::string>& values, int amount){
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print( "Reorg in progress. Database operation delayed.");
        return 0;
    }

    leveldb::Iterator* iterator = db->NewIterator(leveldb::ReadOptions());
    int x = 0;

    // Seek to the key after the starting key
    iterator->SeekToLast();
    if (iterator->Valid()) {

        std::string key = iterator->key().ToString();
        std::string value = iterator->value().ToString();
        if (value != "commit_marker_value" && key != "commit_marker_key")
        {
            keys.push_back(key);
            values.push_back(value);
            x++;
        }
        iterator->Prev();  // Move to the next key
    }

    for (; iterator->Valid() && x < amount; iterator->Prev()) {
        std::string key = iterator->key().ToString();
        std::string value = iterator->value().ToString();

        if (value != "commit_marker_value" && key != "commit_marker_key")
        {
            keys.push_back(key);
            values.push_back(value);
        }

        x++;
    }

    if (!iterator->status().ok()) {
        delete iterator;
        return 0;
    }

    delete iterator;

    return 1;
}
int database::get_last_data(leveldb::DB* db, std::string& last_key, std::string& last_value)
{
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }

    // Create an iterator for the database
    leveldb::ReadOptions readOptions;
    readOptions.fill_cache = false;
    leveldb::Iterator* it = db->NewIterator(readOptions);

    // Move the iterator to the last key
    it->SeekToLast();

    // Check if the iterator is valid
    if (it->Valid()) {
        // Retrieve the last key and value
        last_key = it->key().ToString();
        last_value = it->value().ToString();
        if (last_value == "commit_marker_value" && last_key == "commit_marker_key") {
            it->Prev();
            if (it->Valid())
            {
                // Retrieve the last key and value
                last_key = it->key().ToString();
                last_value = it->value().ToString();
            }
            else {
                last_key = "";
                last_value = "";
            }
        }
        delete it;
        return 1;
    }
    else {
        delete it;
        return 0;
    }
}
int database::remove_single(leveldb::DB* db, leveldb::Status& status, const std::string& key) {
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }
    // Delete the key-value pair from the database
    status = db->Delete(leveldb::WriteOptions(), key);

    if (status.ok()) {
        // Commit the changes
        commit(db, status);

        if (status.ok()) {
            return 1;  // Success
        }
        else {
            return 0;  // Error during commit
        }
    }
    else {
        return 0;  // Error during deletion
    }
}

void database::commit(leveldb::DB* db, leveldb::Status& status)
{
    // Commit the changes by ensuring synchronous writes
    leveldb::WriteOptions writeOptions;
    writeOptions.sync = true;  // Make the write operation synchronous
    status = db->Put(writeOptions, "commit_marker_key", "commit_marker_value");
}

int database::get_all_keys(leveldb::DB* db, std::vector<std::string>& keys) {
    if (Reorg::is_in_progress.load()) {
        // Handle the fact that a reorg is in progress, e.g., by postponing or skipping the function
        logging::print("Reorg in progress. Database operation delayed.");
        return 0;
    }
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        keys.push_back(it->key().ToString());
        delete it;
        return 1;
    }
    if (!it->status().ok()) {
        logging::print("Error during key iteration:", it->status().ToString());
        delete it;
        return 0;
    }
}

int database::compact_all(leveldb::DB* db) {
    // Compact the database to reduce the size of the database
    db->CompactRange(nullptr, nullptr);

    return 1;  // Success
}
