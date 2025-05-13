#pragma once

// Standard library headers
#include <string>

// Third-party library headers
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/lexical_cast.hpp>

// Project-specific headers
#include "txn.pb.h"
#include "wallet.pb.h"
// #include "db_wallets.h"
#include "db_base.h"
#include "zera_status.h"
#include "../restricted/restricted_keys.h"
#include "../temp_data/temp_data.h"
#include "verify_process_txn.h" 
#include "wallets.h"
#include "../util/stopwatch.h"

class block_process
{
public:
    enum ALLOWED_CONTRACT_FEE
    {
        ANY = 0,
        QUALIFIED = 1,
        ALLOWED = 2,
        NOT_ALLOWED = 3
    };
    static ZeraStatus check_nonce(const zera_txn::PublicKey &public_key, const uint64_t& txn_nonce, const std::string &txn_hash = "");
    static ZeraStatus check_nonce_adr(const std::string& wallet_adr , const uint64_t &txn_nonce, const std::string& txn_hash = "");
    static ZeraStatus process_fees(const zera_txn::InstrumentContract &contract, uint256_t fee_amount,
                                   const std::string &wallet_adr, const std::string &fee_symbol,
                                   bool base, zera_txn::TXNStatusFees &status_fees, const std::string &txn_hash, const std::string& current_validator_address = "", const bool storage_fees = false);

    static bool check_qualified(const std::string &contract_id);
    static void get_cur_equiv(const std::string &contract_id, uint256_t &cur_equiv);
    static ZeraStatus check_allowed_contract_fee(const google::protobuf::RepeatedPtrField<std::string> &allowed_fees, const std::string contract_id, block_process::ALLOWED_CONTRACT_FEE &allowed_fee);
    static ZeraStatus get_contract(const std::string contract_id, zera_txn::InstrumentContract &contract);
    static ZeraStatus check_validator(const std::string &public_key, const zera_txn::TRANSACTION_TYPE &txn_type);
    static void start_block_process();
    static void start_block_process_v2();

    template <typename TXType>
    static ZeraStatus check_parameters(const TXType *txn, zera_txn::TXNStatusFees &status_fees, const std::string &fee_address = "");

    template <typename TXType>
    static ZeraStatus process_txn(const TXType *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, bool timed = false, const std::string& fee_address = "", bool sc_txn = false);

    template <typename TXType>
    static ZeraStatus process_txn(const TXType *txn, zera_txn::TXNStatusFees &status_fees, zera_txn::ExpenseRatioResult *result, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address = "", bool sc_txn = false);

    template <typename TXType>
    static ZeraStatus restricted_check(const TXType *txn, const zera_txn::TRANSACTION_TYPE &txn_type);

    static ZeraStatus get_sender_wallet(const std::string &sender_key, uint256_t &sender_balance);
    static void store_wallets();

    template <typename TXType>
    static ZeraStatus process_simple_fees(const TXType *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address = "");

    template <typename TXType>
    static ZeraStatus process_simple_fees_gas(const TXType *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, uint256_t &fee_amount, const std::string &fee_address = "");

    static ZeraStatus store_txns(zera_validator::Block *block, bool archive = true, bool backup = true);

    static ZeraStatus calculate_fees(const uint256_t &TOKEN_USD_EQIV, const uint256_t &FEE_PER_BYTE, const int &bytes,
                                     const std::string &authorized_fees, uint256_t &txn_fee_amount, std::string denomination_str, const zera_txn::PublicKey& public_key, const bool safe_send = false);

    static ZeraStatus calculate_fees(const uint256_t &TOKEN_USD_EQIV, const uint256_t &FEE_PER_BYTE, const int &bytes,
                                     const std::string &authorized_fees, uint256_t &txn_fee_amount, std::string denomination_str, const bool safe_send = false);

    static ZeraStatus calculate_fees_heartbeat(const uint256_t &TOKEN_USD_EQIV, const uint256_t &FEE_PER_BYTE, const int &bytes,
                                                   const std::string &authorized_fees, uint256_t &txn_fee_amount, std::string denomination_str, const zera_txn::PublicKey &public_key);
};

class pre_process
{
public:
    // process txn
    // after, remove from pending txns and either add to preprocessed or discard
    template <typename TXType>
    static void process_txn(TXType *txn, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string& client_ip);

};

bool check_safe_send(const zera_txn::BaseTXN base, const std::string &wallet_address);
