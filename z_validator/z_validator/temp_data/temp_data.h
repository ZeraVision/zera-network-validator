#pragma once
#include <vector>
#include <mutex>

#include <boost/multiprecision/cpp_int.hpp>

#include "wallet.pb.h"
#include "txn.pb.h"
#include "validator.pb.h"
#include "zera_status.h"

using uint256_t = boost::multiprecision::uint256_t;

class proposer_tracker{
    public:
    static void add_proposer(const zera_txn::Validator& proposer_data);
    static void get_current_proposers(std::vector<zera_txn::Validator>& proposer_data);
    static void clear_proposers();

    private:
    static std::vector<zera_txn::Validator> proposers;
    static std::mutex mtx;
};

class txn_hash_tracker{
    public:
    static void add_hash(const std::string& txn_hash);
    static void get_hash(std::vector<std::string> &txn_hash);

    private:
    static std::vector<std::string> txn_hash_vec;
    static std::mutex mtx;
};

class status_fee_tracker{
    public:
    static void add_fee(const zera_txn::TXNStatusFees& status_fee);
    static void get_status(zera_txn::TXNStatusFees &status_fee, const std::string& key);

    private:
    static std::map<std::string, zera_txn::TXNStatusFees> status_fees;
    static std::mutex mtx;
};
class nonce_txn_tracker{
    public:
    static void add_txn_nonce(const std::string &wallet_adress, const uint64_t &nonce, const std::string& txn_hash);
    static bool get_txn_hash(const std::string &wallet_adress, const uint64_t &nonce, std::string& txn_hash);
    static void remove_txn_nonce(const std::string &wallet_adress, const uint64_t &nonce);

    private:
    static std::map<std::string, std::string> wallet_nonce; // key = wallet_address + nonce, value = txn_hash
    static std::mutex mtx;
};

class nonce_tracker
{

    public:
    static bool remove_pre_nonce(const std::string &wallet_address, uint64_t &nonce);
    static void add_nonce(const std::string &wallet_address, const uint64_t &nonce, const std::string &txn_hash);
    static bool get_nonce(const std::string& wallet_address, uint64_t &nonce);
    static void add_used_nonce(const std::string &wallet_address, const uint64_t &nonce);
    static void store_used_nonce();
    static void store_all();
    private:
    static std::map<std::string, uint64_t> wallet_nonce;
    static std::map<std::string, uint64_t> used_nonce;
    static std::mutex mtx;
    static std::mutex mtx_used;
};
class supply_tracker
{
public:
    static ZeraStatus store_supply(const zera_txn::InstrumentContract &contract, const uint256_t &amount);
    static ZeraStatus supply_to_database();

private:
    static std::map<std::string, zera_wallets::MaxSupply> max_supply;
    static std::mutex mtx;
};

class balance_tracker{
    public:
    static void add_txn_balance(const std::string& wallet_key, const uint256_t &amount, const std::string &txn_hash);
    static ZeraStatus subtract_txn_balance(const std::string& wallet_key, const uint256_t &amount, const std::string &txn_hash);
    static void get_txn_balance(const std::string &txn_hash, std::map<std::string, uint256_t> &add_txn_balance, std::map<std::string, uint256_t> &subtract_txn_balance);
    static void add_balance(const std::string wallet_address, const std::string contract_id, const uint256_t &amount);
    static ZeraStatus remove_balance(const std::string wallet_address, const std::string contract_id, const uint256_t &amount);
    static void change_txn_address(const std::string old_wallet_address, const std::string new_wallet_address);
    static void clear_balances();
    static void store_temp_database();
    static void remove_txn_balance(const std::string& txn_hash);

    private:
    static std::map<std::string, uint256_t> block_balances;
    static std::map<std::string, std::map<std::string, uint256_t>> add_txn_balances;
    static std::map<std::string, std::map<std::string, uint256_t>> subtract_txn_balances;
    static std::mutex mtx;
};
class quash_tracker{
    public:
    static void add_quash(const std::string& proposal_id);
    static void add_quash_keys(const std::string& txn_hash, const zera_txn::PublicKey &public_key);
    static void quash_result(zera_txn::TXNS* txns);

    private:
    static std::map<std::string, std::vector<zera_txn::PublicKey>> quash_keys;
    static std::vector<std::string> quash_list;
    static std::mutex mtx;
};

class fast_quorum_tracker{
    public:
    static void add_proposal(const std::string& proposal_id);
    static void clear_proposals();

    private:
    static std::vector<std::string> proposal_list;
    static std::mutex mtx;
};

class sbt_burn_tracker{
    public:
    static void add_burn(const std::string& sbt_id);
    static void clear_burns();

    private:
    static std::vector<std::string> sbt_list;
    static std::mutex mtx;

};

class item_tracker{
    public:
    static ZeraStatus add_item(const std::string& item_id);
    static void clear_items();

    private:
    static std::vector<std::string> item_list;
    static std::mutex mtx;
};

class txn_tracker{
    public:
    static void add_txn(const std::string& txn_id, google::protobuf::Timestamp& timestamp);
    static void update_txn_ledger();
};

class contract_price_tracker{
    public:
    static void update_price(const std::string& contract_id);
    static void clear_prices();
    static void store_prices();
    static void get_price(const std::string& contract_id, uint256_t& price);

    private:
    static std::map<std::string, zera_validator::ContractPrice> contract_prices;
    static std::mutex mtx;
};

class recieved_txn_tracker
{
    public:
    static void add_txn(const std::string& txn_hash);
    static bool check_txn(const std::string& txn_hash);
    static void remove_txn(const std::string& txn_hash);

    private:
    static std::vector<std::string> recieved_txns;
    static std::mutex mtx;
};