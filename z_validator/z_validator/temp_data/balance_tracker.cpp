#include "temp_data.h"
#include "db_base.h"
#include "base58.h"
#include "../logging/logging.h"

namespace
{
    bool get_proposal_wallet(const std::string &wallet_address, zera_validator::VoteWallet &vote_wallet)
    {
        std::string proposal_wallet_data;
        if (!db_proposal_wallets::get_single(wallet_address, proposal_wallet_data) || !vote_wallet.ParseFromString(proposal_wallet_data))
        {
            return false;
        }
        return true;
    }

    void adjust_proposal_votes(const std::string &wallet_address, const uint256_t &add_amount, const uint256_t &remove_amount, const std::string contract_id)
    {
        zera_validator::VoteWallet vote_wallet;
        if (get_proposal_wallet(wallet_address, vote_wallet))
        {
            for (auto proposal_ledger : vote_wallet.proposal_votes())
            {
                std::vector<uint8_t> vec = base58_decode(proposal_ledger.first);
                std::string proposal_id(vec.begin(), vec.end());
                std::string proposal_data;
                zera_validator::Proposal proposal;
                if ((!db_proposals_temp::get_single(proposal_id, proposal_data) || !proposal.ParseFromString(proposal_data)) &&
                    (!db_proposals::get_single(proposal_id, proposal_data) || !proposal.ParseFromString(proposal_data)))
                {
                    continue;
                }

                if (proposal_ledger.second.has_support())
                {
                    if (proposal_ledger.second.support())
                    {
                        auto map = proposal.mutable_yes();
                        if (map->count(contract_id) > 0)
                        {
                            // The key exists in the map
                            std::string value_str = (*map)[contract_id];
                            uint256_t value = boost::lexical_cast<uint256_t>(value_str);
                            value += add_amount;
                            value -= remove_amount;
                            (*map)[contract_id] = boost::lexical_cast<std::string>(value);
                        }
                        else
                        {
                            continue;
                        }
                    }
                    else
                    {
                        auto map = proposal.mutable_no();
                        if (map->count(contract_id) > 0)
                        {
                            // The key exists in the map
                            std::string value_str = (*map)[contract_id];
                            uint256_t value = boost::lexical_cast<uint256_t>(value_str);
                            value += add_amount;
                            value -= remove_amount;
                            (*map)[contract_id] = boost::lexical_cast<std::string>(value);
                        }
                        else
                        {
                            continue;
                        }
                    }
                }
                else if (proposal_ledger.second.has_option())
                {
                    auto outer_map = proposal.mutable_options();
                    auto inner_map = (*outer_map)[proposal_ledger.second.option()].mutable_vote();

                    if (inner_map->count(contract_id) > 0)
                    {
                        // The key exists in the map
                        std::string value_str = (*inner_map)[contract_id];
                        uint256_t value = boost::lexical_cast<uint256_t>(value_str);
                        value += add_amount;
                        value -= remove_amount;
                        (*inner_map)[contract_id] = boost::lexical_cast<std::string>(value);
                    }
                    else
                    {
                        // The key does not exist in the map
                        continue;
                    }
                }
                db_proposals_temp::store_single(proposal_id, proposal.SerializeAsString());
            }
        }
    }
}

std::map<std::string, uint256_t> balance_tracker::block_balances;
std::map<std::string, std::map<std::string, uint256_t>> balance_tracker::add_txn_balances;
std::map<std::string, std::map<std::string, uint256_t>> balance_tracker::subtract_txn_balances;
std::mutex balance_tracker::mtx;

void balance_tracker::add_txn_balance(const std::string &wallet_key, const uint256_t &amount, const std::string &txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::string balance_key = "ADD_BALANCE_"+ txn_hash;
    std::string balance_value;
    zera_validator::BalanceTracker balance_tracker;

    if(db_processed_wallets::get_single(balance_key, balance_value))
    {
        balance_tracker.ParseFromString(balance_value);
    }

    int x = 0;
    bool found = false;
    for(auto address : balance_tracker.wallet_addresses())
    {
        if(address == wallet_key)
        {
            uint256_t balance(balance_tracker.balances(x));
            balance += amount;
            balance_tracker.set_balances(x, boost::lexical_cast<std::string>(balance));
            db_processed_wallets::store_single(balance_key, balance_tracker.SerializeAsString());
            found = true;
            break;
        }

        x++;
    }

    if(!found)
    {
        balance_tracker.add_wallet_addresses(wallet_key);
        balance_tracker.add_balances(boost::lexical_cast<std::string>(amount));
        db_processed_wallets::store_single(balance_key, balance_tracker.SerializeAsString());
    }

    std::string balance_str;
    if (!db_processed_wallets::get_single(wallet_key, balance_str))
    {
        db_wallets::get_single(wallet_key, balance_str);
    }

    uint256_t balance(balance_str);
    balance += amount;
    db_processed_wallets::store_single(wallet_key, boost::lexical_cast<std::string>(balance));
}

ZeraStatus balance_tracker::subtract_txn_balance(const std::string& wallet_key, const uint256_t &amount, const std::string &txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::string balance_key = "SUB_BALANCE_"+ txn_hash;
    std::string balance_value;
    zera_validator::BalanceTracker balance_tracker;

    std::string balance_str;
    if (db_processed_wallets::get_single(wallet_key, balance_str) || db_wallets::get_single(wallet_key, balance_str))
    {
        uint256_t balance(balance_str);


        if(balance < amount)
        {
            std::string message = "balance_tracker.cpp: subtract_txn_balances: Insufficient wallet balance.";
            logging::print("amount:", amount.str());
            logging::print("balance:", balance.str());
            return ZeraStatus(ZeraStatus::BLOCK_FAULTY_TXN, message, zera_txn::TXN_STATUS::INSUFFICIENT_AMOUNT);
        }

        balance -= amount;
        db_processed_wallets::store_single(wallet_key, boost::lexical_cast<std::string>(balance));
    }
    else
    {
        std::string message = "balance_tracker.cpp: subtract_txn_balances: Invalid wallet address. : " + amount.str();

        return ZeraStatus(ZeraStatus::BLOCK_FAULTY_TXN, message, zera_txn::TXN_STATUS::INVALID_WALLET_ADDRESS);
    }

    if(!db_processed_wallets::get_single(balance_key, balance_value))
    {
        subtract_txn_balances[txn_hash] = std::map<std::string, uint256_t>();
    }
    else
    {
        balance_tracker.ParseFromString(balance_value);
    }

    int x = 0;
    bool found = false;
    for(auto address : balance_tracker.wallet_addresses())
    {
        if(address == wallet_key)
        {
            uint256_t balance(balance_tracker.balances(x));
            balance += amount;
            balance_tracker.set_balances(x, boost::lexical_cast<std::string>(balance));
            found = true;
            break;
        }

        x++;
    }

    if(!found)
    {
        balance_tracker.add_wallet_addresses(wallet_key);
        balance_tracker.add_balances(boost::lexical_cast<std::string>(amount));
    }

    db_processed_wallets::store_single(balance_key, balance_tracker.SerializeAsString());


    // if (subtract_txn_balances.find(txn_hash) == subtract_txn_balances.end())
    // {
    //     subtract_txn_balances[txn_hash] = std::map<std::string, uint256_t>();
    // }

    // if (subtract_txn_balances[txn_hash].find(wallet_key) == subtract_txn_balances[txn_hash].end())
    // {
    //     subtract_txn_balances[txn_hash][wallet_key] = 0;
    // }

    // subtract_txn_balances[txn_hash][wallet_key] += amount;

    return ZeraStatus();
}
void balance_tracker::get_txn_balance(const std::string &txn_hash, std::map<std::string, uint256_t> &add_txn_balance, std::map<std::string, uint256_t> &subtract_txn_balance)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::string balance_key = "ADD_BALANCE_"+ txn_hash;
    std::string sub_balance_key = "SUB_BALANCE_"+ txn_hash;
    std::string balance_value;
    std::string sub_balance_value;
    zera_validator::BalanceTracker sub_balance_tracker;
    zera_validator::BalanceTracker balance_tracker;

    if(!db_processed_wallets::get_single(balance_key, balance_value))
    {
        add_txn_balance = std::map<std::string, uint256_t>();
    }
    else
    {
        balance_tracker.ParseFromString(balance_value);
        for(int x = 0; x < balance_tracker.wallet_addresses_size(); x++)
        {
            add_txn_balance[balance_tracker.wallet_addresses(x)] = boost::lexical_cast<uint256_t>(balance_tracker.balances(x));
        }
    }

    if(db_processed_wallets::get_single(sub_balance_key, sub_balance_value))
    {
        sub_balance_tracker.ParseFromString(sub_balance_value);
        for(int x = 0; x < sub_balance_tracker.wallet_addresses_size(); x++)
        {
            subtract_txn_balance[sub_balance_tracker.wallet_addresses(x)] = boost::lexical_cast<uint256_t>(sub_balance_tracker.balances(x));
        }
    }
    else
    {
        subtract_txn_balance = std::map<std::string, uint256_t>();
    }

    // if (subtract_txn_balances.find(txn_hash) == subtract_txn_balances.end())
    // {
    //     subtract_txn_balance = std::map<std::string, uint256_t>();
    // }
    // else
    // {
    //     subtract_txn_balance = subtract_txn_balances[txn_hash];
    // }
}

void balance_tracker::remove_txn_balance(const std::string &txn_hash)
{
    std::lock_guard<std::mutex> lock(mtx);
    std::string balance_key = "ADD_BALANCE_"+ txn_hash;
    std::string sub_balance_key = "SUB_BALANCE_"+ txn_hash;
    db_processed_wallets::remove_single(balance_key);
    db_processed_wallets::remove_single(sub_balance_key);

    // add_txn_balances.erase(txn_hash);
    // subtract_txn_balances.erase(txn_hash);
}

void balance_tracker::add_balance(const std::string wallet_address, const std::string contract_id, const uint256_t &amount)
{
    std::string sender_key = wallet_address + contract_id;
    std::lock_guard<std::mutex> lock(mtx);

    if (block_balances.find(sender_key) == block_balances.end())
    {
        std::string balance_str;
        if (!db_wallets_temp::get_single(sender_key, balance_str) && !db_wallets::get_single(sender_key, balance_str))
        {
            balance_str = "0";
        }

        const uint256_t balance(balance_str);
        block_balances[sender_key] = balance;
    }
    uint256_t zero(0);

    adjust_proposal_votes(wallet_address, amount, zero, contract_id);

    // add new balance to recipient balances
    block_balances[sender_key] += amount;
}

ZeraStatus balance_tracker::remove_balance(const std::string wallet_address, const std::string contract_id, const uint256_t &amount)
{

    std::string sender_key = wallet_address + contract_id;
    std::lock_guard<std::mutex> lock(mtx);

    if (block_balances.find(sender_key) == block_balances.end())
    {
        std::string balance_str;
        if (!db_wallets_temp::get_single(sender_key, balance_str) && !db_wallets::get_single(sender_key, balance_str))
        {
            return ZeraStatus(ZeraStatus::TXN_FAILED, "balance_tracker.cpp: remove_balance: Invalid wallet address.", zera_txn::TXN_STATUS::INVALID_WALLET_ADDRESS);
        }

        const uint256_t balance(balance_str);
        block_balances[sender_key] = balance;
    }

    if (block_balances[sender_key] < amount)
    {
        return ZeraStatus(ZeraStatus::TXN_FAILED, "balance_tracker.cpp: remove_balance: Insufficient wallet balance.", zera_txn::TXN_STATUS::INSUFFICIENT_AMOUNT);
    }
    uint256_t zero(0);

    adjust_proposal_votes(wallet_address, zero, amount, contract_id);
    // add new balance to recipient balances
    block_balances[sender_key] -= amount;

    return ZeraStatus();
}

void balance_tracker::store_temp_database()
{
    // if transaction passed add new balance to temp wallet database, for further processing.
    leveldb::WriteBatch batch;
    for (const auto &sender_pair : block_balances)
    {
        batch.Put(sender_pair.first, boost::lexical_cast<std::string>(sender_pair.second));
    }

    db_wallets_temp::store_batch(batch);
    clear_balances();
}
void balance_tracker::clear_balances()
{
    std::lock_guard<std::mutex> lock(mtx);
    block_balances.clear();
}
