#include "proposer.h"

#include <iostream>
#include <regex>

#include "../governance/gov_process.h"
#include "../temp_data/temp_data.h"
#include "../crypto/merkle.h"
#include "wallets.h"
#include "block.h"
#include "validators.h"

std::map<std::string, std::map<std::string, std::map<std::string, boost::multiprecision::uint256_t>>> proposing::txn_token_fees;
std::mutex proposing::mtx;

void proposing::set_txn_token_fees(std::string txn_hash, std::string contract_id, std::string address, boost::multiprecision::uint256_t amount)
{
    std::lock_guard<std::mutex> lock(mtx);

    if (txn_token_fees.find(txn_hash) != txn_token_fees.end())
    {
        if (txn_token_fees[txn_hash].find(address) != txn_token_fees[txn_hash].end())
        {
            if (txn_token_fees[txn_hash][address].find(contract_id) != txn_token_fees[txn_hash][address].end())
            {
                txn_token_fees[txn_hash][address][contract_id] += amount;
            }
            else
            {
                txn_token_fees[txn_hash][address][contract_id] = amount;
            }
        }
        else
        {
            txn_token_fees[txn_hash][address][contract_id] = amount;
        }
    }
    else
    {
        txn_token_fees[txn_hash][address][contract_id] = amount;
    }
}
void proposing::add_temp_wallet_balance(const std::vector<std::string> &txn_hash_vec, const std::string &fee_address)
{

    std::regex pattern("\\$[^$]*$");

    for (auto txn_hash : txn_hash_vec)
    {
        std::map<std::string, uint256_t> txn_balance;
        std::map<std::string, uint256_t> subtract_txn_balance;
        balance_tracker::get_txn_balance(txn_hash, txn_balance, subtract_txn_balance);
        for (auto balance : txn_balance)
        {
            std::smatch match;

            // Find the last $ and everything after it
            std::regex_search(balance.first, match, pattern);
            std::string contract_id = match.str();
            // Remove the last $ and everything after it
            std::string wallet_address = std::regex_replace(balance.first, pattern, "");

            std::string fee_wallet = wallet_address;
            if (wallet_address == PREPROCESS_PLACEHOLDER)
            {
                fee_wallet = fee_address;
            }

            balance_tracker::add_balance(fee_wallet, contract_id, balance.second);
        }

        for (auto subtract : subtract_txn_balance)
        {
            std::smatch match;

            // Find the last $ and everything after it
            std::regex_search(subtract.first, match, pattern);
            std::string contract_id = match.str();
            // Remove the last $ and everything after it
            std::string wallet_address = std::regex_replace(subtract.first, pattern, "");
            balance_tracker::remove_balance(wallet_address, contract_id, subtract.second);
        }
        balance_tracker::remove_txn_balance(txn_hash);
    }

    balance_tracker::store_temp_database();
}
void proposing::set_all_token_fees(zera_validator::Block *block, const std::vector<std::string> &txn_hash_vec, const std::string &fee_address)
{

    std::map<std::string, std::map<std::string, boost::multiprecision::uint256_t>> token_fees;

    std::lock_guard<std::mutex> lock(mtx);

    for (auto txn_hash : txn_hash_vec)
    {
        auto txn_hash_map = txn_token_fees[txn_hash];
        for (auto address : txn_hash_map)
        {
            for (auto contract : address.second)
            {
                set_token_fees(contract.first, address.first, contract.second, token_fees);
            }
        }
        txn_token_fees.erase(txn_hash);
    }

    // Define a nested map to accumulate token amounts
    std::map<std::string, std::map<std::string, uint256_t>> accumulated_fees;

    // Accumulate token amounts
    for (const auto &token_fee : token_fees)
    {
        std::string address = token_fee.first;
        if (address == PREPROCESS_PLACEHOLDER)
        {
            address = fee_address;
        }

        for (const auto &token : token_fee.second)
        {

            accumulated_fees[address][token.first] += token.second;
        }
    }

    // Add accumulated amounts to the block
    for (const auto &token_fee : accumulated_fees)
    {
        zera_txn::TokenFees *temp_fees = block->mutable_transactions()->add_token_fees();
        for (const auto &token : token_fee.second)
        {
            zera_txn::Token *temp_token = temp_fees->add_tokens();
            temp_token->set_amount(boost::lexical_cast<std::string>(token.second));
            temp_token->set_contract_id(token.first);
        }
        temp_fees->set_address(token_fee.first);
    }
}

void proposing::set_token_fees(std::string contract_id, std::string address, boost::multiprecision::uint256_t amount, std::map<std::string, std::map<std::string, boost::multiprecision::uint256_t>> &token_fees)
{
    if (token_fees.find(address) != token_fees.end())
    {
        if (token_fees[address].find(contract_id) != token_fees[address].end())
        {
            token_fees[address][contract_id] += amount;
        }
        else
        {
            token_fees[address][contract_id] = amount;
        }
    }
    else
    {
        token_fees[address][contract_id] = amount;
    }
}

ZeraStatus proposing::make_block(zera_validator::Block *block, const transactions &txns)
{

    // add prepocessed txns to block
    // this function will remove the txns from the processed database
    bool added_txn = add_processed(txns.processed_keys, txns.processed_values, block);

    if (txns.timed_keys.size() > 0)
    {
        ZeraStatus status = proposing::process_txns(txns.timed_values, txns.timed_keys, block, true);
        if (status.ok())
        {
            added_txn = true;
        }
    }

    // process txns
    if (txns.keys.size() > 0)
    {
        // process and add any txns that have not been processed
        // this function will remove the txns from the pending database
        ZeraStatus status = proposing::process_txns(txns.values, txns.keys, block);

        if (!status.ok())
        {
            if (!added_txn)
            {
                return status;
            }
        }
        added_txn = true;
    }

    if (txns.gov_keys.size() > 0)
    {
        ZeraStatus status = proposing::process_txns(txns.gov_values, txns.gov_keys, block, false);
        if (status.ok())
        {
            added_txn = true;
        }
    }

    int x = 0;
    std::vector<std::string> sc_keys;
    std::vector<std::string> sc_values;
    while (db_sc_transactions::get_all_data(sc_keys, sc_values) && x < 10)
    {
        ZeraStatus status = proposing::process_txns(sc_values, sc_keys, block, false);
        if (status.ok())
        {
            added_txn = true;
        }

        for (auto key : sc_keys)
        {
            db_sc_transactions::remove_single(key);
        }
        sc_keys.clear();
        sc_values.clear();
        x++;
    }

    for (auto key : txns.gov_keys)
    {
        db_gov_txn::remove_single(key);
    }

    if (!added_txn)
    {
        return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, "proposer.h: make_block: No txns added to block.");
    }

    auto block_txns = block->mutable_transactions();
    // process fast quorum proposals if any
    gov_process::process_fast_quorum(block_txns);

    quash_tracker::quash_result(block_txns);

    std::vector<std::string> txn_hash_vec;
    txn_hash_tracker::get_hash(txn_hash_vec);

    set_all_token_fees(block, txn_hash_vec, ValidatorConfig::get_fee_address_string());
    add_temp_wallet_balance(txn_hash_vec, ValidatorConfig::get_fee_address_string());

    merkle_tree::build_merkle_tree(block);

    block_utils::set_block(block);
    std::string block_write;
    std::string header_write;
    std::string key1 = block_utils::block_to_write(block, block_write, header_write);
    std::string block_data;

    if (db_blocks::get_single(key1, block_data))
    {
        return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, "proposer.h: make_block: Block already exists.");
    }

    db_blocks::store_single(key1, block_write);
    db_headers::store_single(key1, header_write);
    db_hash_index::store_single(block->block_header().hash(), key1);
    signatures::sign_block_broadcast(block, ValidatorConfig::get_gen_key_pair());

    return ZeraStatus(ZeraStatus::OK);
}

ZeraStatus proposing::make_block_sync(zera_validator::Block *block, const transactions &txns, const std::string &fee_address)
{

    // add prepocessed txns to block
    // this function will remove the txns from the processed database
    bool added_txn = add_processed(txns.processed_keys, txns.processed_values, block);

    if (txns.timed_keys.size() > 0)
    {
        ZeraStatus status = proposing::process_txns(txns.timed_values, txns.timed_keys, block, true);
        if (status.ok())
        {
            added_txn = true;
        }
    }

    // process txns
    if (txns.keys.size() > 0)
    {
        // process and add any txns that have not been processed
        // this function will remove the txns from the pending database
        ZeraStatus status = proposing::process_txns(txns.values, txns.keys, block, false, fee_address);

        if (!status.ok())
        {
            if (!added_txn)
            {
                return status;
            }
        }
        added_txn = true;
    }

    if (txns.gov_keys.size() > 0)
    {
        ZeraStatus status = proposing::process_txns(txns.gov_values, txns.gov_keys, block, false, fee_address);
        if (status.ok())
        {
            added_txn = true;
        }
    }

    int x = 0;
    std::vector<std::string> sc_keys;
    std::vector<std::string> sc_values;
    while (db_sc_transactions::get_all_data(sc_keys, sc_values) && x < 10)
    {
        ZeraStatus status = proposing::process_txns(sc_values, sc_keys, block, false);
        if (status.ok())
        {
            added_txn = true;
        }

        for (auto key : sc_keys)
        {
            db_sc_transactions::remove_single(key);
        }
        sc_keys.clear();
        sc_values.clear();
        x++;
    }

    for (auto key : txns.gov_keys)
    {
        db_gov_txn::remove_single(key);
    }

    if (!added_txn)
    {
        return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, "proposer.h: make_block: No txns added to block.");
    }

    return ZeraStatus(ZeraStatus::OK);
}