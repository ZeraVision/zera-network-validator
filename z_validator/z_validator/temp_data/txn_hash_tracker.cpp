#include "temp_data.h"

std::vector<std::string> txn_hash_tracker::txn_hash_vec;
std::mutex txn_hash_tracker::mtx;

void txn_hash_tracker::add_hash(const std::string &txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    txn_hash_vec.push_back(txn_hash);
}

void txn_hash_tracker::get_hash(std::vector<std::string> &txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    txn_hash.assign(txn_hash_vec.begin(), txn_hash_vec.end());
    txn_hash_vec.clear();
}