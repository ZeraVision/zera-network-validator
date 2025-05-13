#include "temp_data.h"
#include "../logging/logging.h"

std::vector<std::string> txn_hash_tracker::txn_hash_vec;
std::vector<std::string> txn_hash_tracker::allowance_txn_hash_vec;
std::mutex txn_hash_tracker::mtx;

void txn_hash_tracker::add_hash(const std::string &txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    txn_hash_vec.push_back(txn_hash);
}

void txn_hash_tracker::add_allowance_hash(const std::string &txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    allowance_txn_hash_vec.push_back(txn_hash);
}

void txn_hash_tracker::get_hash(std::vector<std::string> &txn_hash, std::vector<std::string> &allowance_txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    txn_hash.assign(txn_hash_vec.begin(), txn_hash_vec.end());
    txn_hash_vec.clear();

    allowance_txn_hash_vec.assign(allowance_txn_hash_vec.begin(), allowance_txn_hash_vec.end());
    allowance_txn_hash_vec.clear();
}