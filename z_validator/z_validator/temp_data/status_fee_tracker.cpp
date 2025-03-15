#include "temp_data.h"

std::map<std::string, zera_txn::TXNStatusFees> status_fee_tracker::status_fees;
std::mutex status_fee_tracker::mtx;

void status_fee_tracker::add_fee(const zera_txn::TXNStatusFees &status_fee)
{
    std::lock_guard<std::mutex> lock(mtx);
    status_fees[status_fee.txn_hash()] = status_fee;
}

void status_fee_tracker::get_status(zera_txn::TXNStatusFees &status_fee, const std::string& key)
{
    std::lock_guard<std::mutex> lock(mtx);
    
    status_fee = status_fees[key];
    status_fees.erase(key);
}
