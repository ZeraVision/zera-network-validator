// Standard library headers

// Third-party library headers
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/lexical_cast.hpp>

// Project-specific headers
#include "const.h"
#include "block_process.h"
#include "wallets.h"
#include "validators.h"
#include "db_base.h"
#include "hashing.h"
#include "proposer.h"
#include "signatures.h"
#include "../temp_data/temp_data.h"
#include "../compliance/compliance.h"
#include "utils.h"
#include "../logging/logging.h"

template <typename TXType>
ZeraStatus block_process::process_simple_fees(const TXType *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address)
{
    uint256_t fee_type = get_txn_fee(txn_type);

    zera_txn::InstrumentContract contract;
    ZeraStatus status = block_process::get_contract(txn->base().fee_id(), contract);
    if (!status.ok())
    {
        return status;
    }

    // check to see if token is qualified and get usd_equiv if it is, or send back zra usd equiv if it is not qualified
    uint256_t usd_equiv;

    if (!block_process::check_qualified(contract.contract_id()))
    {
        return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, "process_utils.cpp: process_simple_fees: invalid token for fees: " + contract.contract_id());
    }

    block_process::get_cur_equiv(contract.contract_id(), usd_equiv);
    // calculate the fees that need to be paid, and verify they have authorized enough coin to pay it
    uint256_t txn_fee_amount;
    status = block_process::calculate_fees(usd_equiv, fee_type, txn->ByteSize(), txn->base().fee_amount(), txn_fee_amount, contract.coin_denomination().amount(), txn->base().public_key());

    if (!status.ok())
    {
        return status;
    }
    std::string wallet_key = wallets::generate_wallet(txn->base().public_key());

    status = block_process::process_fees(contract, txn_fee_amount, wallet_key, contract.contract_id(), true, status_fees, txn->base().hash(), fee_address);
    return status;
}
template ZeraStatus block_process::process_simple_fees<zera_txn::GovernanceVote>(const zera_txn::GovernanceVote *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::GovernanceProposal>(const zera_txn::GovernanceProposal *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::NFTTXN>(const zera_txn::NFTTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::ContractUpdateTXN>(const zera_txn::ContractUpdateTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::AuthorizedCurrencyEquiv>(const zera_txn::AuthorizedCurrencyEquiv *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::SelfCurrencyEquiv>(const zera_txn::SelfCurrencyEquiv *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::ExpenseRatioTXN>(const zera_txn::ExpenseRatioTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::FoundationTXN>(const zera_txn::FoundationTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::DelegatedTXN>(const zera_txn::DelegatedTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::QuashTXN>(const zera_txn::QuashTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::FastQuorumTXN>(const zera_txn::FastQuorumTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::RevokeTXN>(const zera_txn::RevokeTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::ComplianceTXN>(const zera_txn::ComplianceTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::BurnSBTTXN>(const zera_txn::BurnSBTTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::ValidatorHeartbeat>(const zera_txn::ValidatorHeartbeat *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::ValidatorRegistration>(const zera_txn::ValidatorRegistration *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::SmartContractTXN>(const zera_txn::SmartContractTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::SmartContractInstantiateTXN>(const zera_txn::SmartContractInstantiateTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::MintTXN>(const zera_txn::MintTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees<zera_txn::AllowanceTXN>(const zera_txn::AllowanceTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address);

template <>
ZeraStatus block_process::process_simple_fees<zera_txn::InstrumentContract>(const zera_txn::InstrumentContract *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, const std::string &fee_address)
{
    uint256_t fee_type = get_txn_fee_contract(txn_type, txn);

    zera_txn::InstrumentContract contract;
    ZeraStatus status = block_process::get_contract(txn->base().fee_id(), contract);
    if (!status.ok())
    {
        return status;
    }

    // check to see if token is qualified and get usd_equiv if it is, or send back zra usd equiv if it is not qualified
    uint256_t usd_equiv;

    if (!block_process::check_qualified(contract.contract_id()))
    {
        return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, "process_utils.cpp: process_simple_fees: invalid token for fees: " + contract.contract_id());
    }

    block_process::get_cur_equiv(contract.contract_id(), usd_equiv);
    // calculate the fees that need to be paid, and verify they have authorized enough coin to pay it
    uint256_t txn_fee_amount;
    status = block_process::calculate_fees(usd_equiv, fee_type, txn->ByteSize(), txn->base().fee_amount(), txn_fee_amount, contract.coin_denomination().amount(), txn->base().public_key());

    if (!status.ok())
    {
        return status;
    }
    std::string wallet_key = wallets::generate_wallet(txn->base().public_key());

    status = block_process::process_fees(contract, txn_fee_amount, wallet_key, contract.contract_id(), true, status_fees, txn->base().hash(), fee_address);
    return status;
}

template <typename TXType>
ZeraStatus block_process::process_simple_fees_gas(const TXType *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, uint256_t &fee_amount, const std::string &fee_address)
{
    uint256_t fee_type = get_txn_fee(txn_type);

    zera_txn::InstrumentContract contract;
    ZeraStatus status = block_process::get_contract(txn->base().fee_id(), contract);
    if (!status.ok())
    {
        return status;
    }

    // check to see if token is qualified and get usd_equiv if it is, or send back zra usd equiv if it is not qualified
    uint256_t usd_equiv;

    if (!block_process::check_qualified(contract.contract_id()))
    {
        return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, "process_utils.cpp: process_simple_fees: invalid token for fees: " + contract.contract_id());
    }

    block_process::get_cur_equiv(contract.contract_id(), usd_equiv);
    // calculate the fees that need to be paid, and verify they have authorized enough coin to pay it
    uint256_t txn_fee_amount;
    status = block_process::calculate_fees(usd_equiv, fee_type, txn->ByteSize(), txn->base().fee_amount(), txn_fee_amount, contract.coin_denomination().amount(), txn->base().public_key());
    fee_amount = txn_fee_amount;
    
    if (!status.ok())
    {
        return status;
    }
    std::string wallet_key = wallets::generate_wallet(txn->base().public_key());

    status = block_process::process_fees(contract, txn_fee_amount, wallet_key, contract.contract_id(), true, status_fees, txn->base().hash(), fee_address);
    return status;
}
template ZeraStatus block_process::process_simple_fees_gas<zera_txn::SmartContractExecuteTXN>(const zera_txn::SmartContractExecuteTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, uint256_t &fee_amount, const std::string &fee_address);
template ZeraStatus block_process::process_simple_fees_gas<zera_txn::SmartContractInstantiateTXN>(const zera_txn::SmartContractInstantiateTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, uint256_t &fee_amount, const std::string &fee_address);

ZeraStatus block_process::get_sender_wallet(const std::string &sender_key, uint256_t &sender_balance)
{
    std::string wallet_balance;
    // see if sender has already made a transaction in this block if not get wallet from main database
    // if wallet doesnt exist send back error status
    if (!db_wallets_temp::get_single(sender_key, wallet_balance))
    {
        if (!db_wallets::get_single(sender_key, wallet_balance))
        {
            return ZeraStatus(ZeraStatus::Code::WALLET_ERROR, "process_utils.cpp: get_sender_wallet: Sender wallet does not exist: ", zera_txn::TXN_STATUS::INSUFFICIENT_AMOUNT);
        }
    }

    sender_balance = boost::lexical_cast<uint256_t>(wallet_balance);
    return ZeraStatus(ZeraStatus::Code::OK);
}

ZeraStatus block_process::calculate_fees(const uint256_t &TOKEN_USD_EQIV, const uint256_t &FEE_PER_BYTE, const int &bytes,
                                         const std::string &authorized_fees, uint256_t &txn_fee_amount, std::string denomination_str, const zera_txn::PublicKey &public_key, const bool safe_send)
{

    uint256_t fee_per_byte(FEE_PER_BYTE);
    uint256_t fee = fee_per_byte * bytes;
    uint256_t denomination(denomination_str);
    uint256_t item_fee = denomination * fee;
    txn_fee_amount = item_fee / TOKEN_USD_EQIV;

    uint256_t authorized_fees_uint(authorized_fees);

    uint256_t key_fee = get_key_fee(public_key);
    uint256_t key_fee_amount = key_fee * denomination;
    uint256_t safe_send_amount = SAFE_SEND * denomination;

    txn_fee_amount += key_fee_amount / TOKEN_USD_EQIV;
    txn_fee_amount += safe_send_amount / TOKEN_USD_EQIV;

    if (txn_fee_amount > authorized_fees_uint)
    {
        return ZeraStatus(ZeraStatus::Code::COIN_TXN_ERROR, "process_coin.cpp: calculate_fees: The sender did not authorize enough fees.", zera_txn::TXN_STATUS::INSUFFICIENT_AMOUNT);
    }

    return ZeraStatus();
}

ZeraStatus block_process::calculate_fees(const uint256_t &TOKEN_USD_EQIV, const uint256_t &FEE_PER_BYTE, const int &bytes,
                                         const std::string &authorized_fees, uint256_t &txn_fee_amount, std::string denomination_str, const bool safe_send)
{

    uint256_t fee_per_byte(FEE_PER_BYTE);
    uint256_t fee = fee_per_byte * bytes;
    uint256_t denomination(denomination_str);
    uint256_t item_fee = denomination * fee;
    txn_fee_amount = item_fee / TOKEN_USD_EQIV;

    uint256_t authorized_fees_uint(authorized_fees);

    uint256_t safe_send_amount = SAFE_SEND * denomination;

    txn_fee_amount += safe_send_amount / TOKEN_USD_EQIV;

    if (txn_fee_amount > authorized_fees_uint)
    {
        return ZeraStatus(ZeraStatus::Code::COIN_TXN_ERROR, "process_coin.cpp: calculate_fees: The sender did not authorize enough fees.", zera_txn::TXN_STATUS::INSUFFICIENT_AMOUNT);
    }

    return ZeraStatus();
}

ZeraStatus block_process::calculate_fees_heartbeat(const uint256_t &TOKEN_USD_EQIV, const uint256_t &FEE_PER_BYTE, const int &bytes,
                                                   const std::string &authorized_fees, uint256_t &txn_fee_amount, std::string denomination_str, const zera_txn::PublicKey &public_key)
{
    uint256_t fee_per_byte(FEE_PER_BYTE);
    uint256_t fee = fee_per_byte * bytes;
    uint256_t denomination(denomination_str);
    uint256_t item_fee = denomination * fee;
    txn_fee_amount = item_fee / TOKEN_USD_EQIV;

    uint256_t authorized_fees_uint(authorized_fees);

    if (txn_fee_amount > authorized_fees_uint)
    {
        return ZeraStatus(ZeraStatus::Code::COIN_TXN_ERROR, "process_coin.cpp: calculate_fees: The sender did not authorize enough fees.", zera_txn::TXN_STATUS::INSUFFICIENT_AMOUNT);
    }

    return ZeraStatus();
}

// function to check if token is qualified, and get the usd equiv of token
bool block_process::check_qualified(const std::string &contract_id)
{
    zera_validator::CurrencyRate rate;
    std::string rate_str;
    db_currency_equiv::get_single(contract_id, rate_str);
    rate.ParseFromString(rate_str);

    return rate.qualified();
}

// function to get the cur equivalent of both the token and the fee token
// 1.00$ = 1 000 000 000 000 000 000
void block_process::get_cur_equiv(const std::string &contract_id, uint256_t &cur_equiv)
{
    zera_validator::CurrencyRate rate;
    std::string rate_str;
    if (db_currency_equiv::get_single(contract_id, rate_str) && rate.ParseFromString(rate_str))
    {
        cur_equiv = boost::lexical_cast<uint256_t>(rate.rate());
    }

    if (cur_equiv == 0)
    {
        cur_equiv = 1;
    }
}

ZeraStatus block_process::check_allowed_contract_fee(const google::protobuf::RepeatedPtrField<std::string> &allowed_fees, const std::string contract_id, block_process::ALLOWED_CONTRACT_FEE &allowed_fee)
{
    allowed_fee = block_process::ALLOWED_CONTRACT_FEE::NOT_ALLOWED;
    for (const auto fee_id : allowed_fees)
    {
        logging::print("fee_id: ", fee_id);
        // if fee_id is qualified do you not break!
        // this is due to the possibility of other unqualified tokens being allowed
        if (fee_id == "QUALIFIED")
        {
            allowed_fee = block_process::ALLOWED_CONTRACT_FEE::QUALIFIED;
            break;
        }
        else if (fee_id == "ANY")
        {
            allowed_fee = block_process::ALLOWED_CONTRACT_FEE::ANY;
            break;
        }
        if (contract_id == fee_id)
        {
            allowed_fee = block_process::ALLOWED_CONTRACT_FEE::ALLOWED;
            break;
        }
    }

    if (allowed_fee == block_process::ALLOWED_CONTRACT_FEE::NOT_ALLOWED)
    {
        return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_utils.cpp: check_contract_fee_id: Fee contract id is not allowed: " + contract_id, zera_txn::TXN_STATUS::INVALID_CONTRACT_FEE_ID);
    }

    if (!db_contracts::exist(contract_id))
    {
        return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_utils.cpp: check_contract_fee_id: Fee contract id does not exist", zera_txn::TXN_STATUS::INVALID_CONTRACT_FEE_ID);
    }

    return ZeraStatus();
}

ZeraStatus block_process::get_contract(const std::string contract_id, zera_txn::InstrumentContract &contract)
{
    std::string contract_data;
    contract.Clear();
    if (!db_contracts::get_single(contract_id, contract_data) || !contract.ParseFromString(contract_data))
    {
        return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, "process_utils.cpp: get_contract: Invalid Contract: " + contract_id, zera_txn::TXN_STATUS::INVALID_CONTRACT);
    }

    return ZeraStatus();
}

void block_process::store_wallets()
{
    std::vector<std::string> addresses;
    std::vector<std::string> wallets_data;
    rocksdb::WriteBatch wallet_batch;
    rocksdb::WriteBatch exist_batch;
    rocksdb::WriteBatch lookup_batch;

    db_wallets_temp::get_all_data(addresses, wallets_data);
    logging::print("starting store");

    for (std::size_t i = 0; i < addresses.size(); ++i)
    {
        auto tres_wallet_vec = base58_decode(ValidatorConfig::get_treasury_wallet());

        const std::string &address = addresses[i];
        std::string burn_wallet = std::string(BURN_WALLET) + ZERA_SYMBOL;
        std::string foundation_wallet(tres_wallet_vec.begin(), tres_wallet_vec.end());
        foundation_wallet += ZERA_SYMBOL;
        std::string postfix;
        std::string address_no_symbol;

        size_t pos = address.find_last_of('$');
        if (pos != std::string::npos)
        {
            postfix.assign(address.begin() + pos, address.end());
            address_no_symbol.assign(address.begin(), address.begin() + pos);
        }

        std::string wallet_balance1;
        // ADD LOGS AGAIN!
        if (db_wallets::get_single(address, wallet_balance1))
        {
            if (address == burn_wallet)
            {
                logging::print("current wallet -", address, " amount: ", wallet_balance1);
            }
            else
            {
                logging::print("current wallet -", base58_encode(address_no_symbol) + postfix, "amount:", wallet_balance1);
            }
        }
        else
        {
            logging::print("current wallet -", base58_encode(address_no_symbol) + postfix , "amount:", wallet_balance1);
        }

        const std::string &wallet_balance = wallets_data[i];
        if (address == burn_wallet)
        {
            logging::print("temp_wallet -   ", address, "amount: ", wallet_balance + "\n");
        }
        else
        {
            logging::print("temp_wallet -   ", base58_encode(address_no_symbol) + postfix, "amount:", wallet_balance + "\n");
        }

        wallet_batch.Put(address, wallet_balance);

        if (!db_wallet_lookup::exist(address_no_symbol))
        {
            zera_validator::WalletLookup wallet_lookup;
            exist_batch.Put(address_no_symbol, wallet_lookup.SerializeAsString());
        }

        std::string token_lookup_key = "TOKEN_LOOKUP_" + address_no_symbol;
        std::string data;
        db_wallet_lookup::get_single(token_lookup_key, data);
        zera_validator::TokenLookup token_lookup;
        token_lookup.ParseFromString(data);

        bool found = false;
        bool remove = false;
        int x = 0;
        for (auto token : token_lookup.tokens())
        {
            if (postfix == token)
            {
                if (wallet_balance == "0")
                {
                    remove = true;
                }
                found = true;
                break;
            }
            x++;
        }
        if (remove)
        {
            token_lookup.mutable_tokens()->SwapElements(x, token_lookup.tokens_size() - 1);
            token_lookup.mutable_tokens()->RemoveLast();
            lookup_batch.Put(token_lookup_key, token_lookup.SerializeAsString());
        }
        if (!found)
        {
            token_lookup.add_tokens(postfix);
            lookup_batch.Put(token_lookup_key, token_lookup.SerializeAsString());
        }
    }

    
    db_wallets::store_batch(wallet_batch);
    db_wallets_temp::remove_all();
    db_wallet_lookup::store_batch(exist_batch);
    db_wallet_lookup::store_batch(lookup_batch);
}

ZeraStatus block_process::process_fees(const zera_txn::InstrumentContract &contract, uint256_t fee_amount,
                                       const std::string &wallet_adr, const std::string &fee_symbol,
                                       bool base, zera_txn::TXNStatusFees &status_fees, const std::string &txn_hash, const std::string &current_validator_address, const bool storage_fees)
{

    const uint256_t zero_256 = 0;
    uint256_t sender_balance;
    ZeraStatus status = ZeraStatus();
    if (!storage_fees)
    {
        status = balance_tracker::subtract_txn_balance(wallet_adr, fee_symbol, fee_amount, txn_hash);
    }

    if (!status.ok())
    {
        logging::print("process_utils.cpp: process_fees: ", status.read_status());

        if (base)
        {
            logging::print("ContractID: ", contract.contract_id());
            logging::print("WalletAdr: ", base58_encode(wallet_adr));
            return ZeraStatus(ZeraStatus::Code::WALLET_INSUFFICIENT_FUNDS, "process_utils.cpp: process_fess: " + status.read_status());
        }

        return status;
    }
    std::string validator_fee_address = ValidatorConfig::get_fee_address_string();

    if (current_validator_address != "")
    {
        validator_fee_address = current_validator_address;
    }

    auto tres_wallet_vec = base58_decode(ValidatorConfig::get_treasury_wallet());
    std::string foundation_wallet(tres_wallet_vec.begin(), tres_wallet_vec.end());

    uint256_t burn_percent = BURN_FEE_PERCENTAGE;
    uint256_t validator_percent = VALIDATOR_FEE_PERCENTAGE;
    uint256_t foundation_percent = 0;
    uint256_t contract_percent = 0;

    uint256_t validator_fee = 0;
    uint256_t contract_fee = 0;
    uint256_t foundation_fee = 0;
    uint256_t burn_fee = 0;

    if (!base)
    {
        burn_percent = boost::lexical_cast<uint256_t>(contract.contract_fees().burn());
        validator_percent = boost::lexical_cast<uint256_t>(contract.contract_fees().validator());
        contract_percent = 100 - burn_percent - validator_percent;
    }
    else
    {
        foundation_percent = FOUNDATION_FEE_PERCENTAGE;
    }
    if (validator_percent > zero_256)
    {
        validator_fee = (validator_percent * fee_amount) / 100;
        balance_tracker::add_txn_balance(validator_fee_address, fee_symbol, validator_fee, txn_hash);
        proposing::set_txn_token_fees(txn_hash, fee_symbol, validator_fee_address, validator_fee);
    }
    if (foundation_percent > zero_256)
    {
        foundation_fee = (foundation_percent * fee_amount) / 100;
        balance_tracker::add_txn_balance(foundation_wallet, fee_symbol, foundation_fee, txn_hash);
        proposing::set_txn_token_fees(txn_hash, fee_symbol, foundation_wallet, foundation_fee);
    }
    if (burn_percent > zero_256)
    {
        if (base)
        {
            burn_fee = fee_amount - validator_fee - contract_fee - foundation_fee;
        }
        else
        {
            burn_fee = (burn_percent * fee_amount) / 100;
        }

        balance_tracker::add_txn_balance(BURN_WALLET, fee_symbol, burn_fee, txn_hash);
        proposing::set_txn_token_fees(txn_hash, fee_symbol, BURN_WALLET, burn_fee);
    }
    if (contract_percent > zero_256)
    {
        contract_fee = fee_amount - validator_fee - burn_fee;
        balance_tracker::add_txn_balance(contract.contract_fees().fee_address(), fee_symbol, contract_fee, txn_hash);
        proposing::set_txn_token_fees(txn_hash, fee_symbol, contract.contract_fees().fee_address(), contract_fee);
    }

    if (!storage_fees)
    {
        if (is_valid_uint256(status_fees.base_fees()))
        {
            uint256_t total_fee(status_fees.base_fees());
            total_fee += fee_amount;
            status_fees.set_base_fees(boost::lexical_cast<std::string>(total_fee));
        }
        else
        {
            status_fees.set_base_contract_id(fee_symbol);
            status_fees.set_base_fees(boost::lexical_cast<std::string>(fee_amount));
        }
    }
    else
    {
        status_fees.set_native_function_fees(boost::lexical_cast<std::string>(fee_amount));
    }

    return ZeraStatus(ZeraStatus::Code::OK);
}

ZeraStatus block_process::check_validator(const std::string &public_key, const zera_txn::TRANSACTION_TYPE &txn_type)
{

    if (txn_type == zera_txn::TRANSACTION_TYPE::VOTE_TYPE || txn_type == zera_txn::TRANSACTION_TYPE::VALIDATOR_REGISTRATION_TYPE || txn_type == zera_txn::TRANSACTION_TYPE::VALIDATOR_HEARTBEAT_TYPE)
    {
        return ZeraStatus();
    }
    // if sender is a validator remove txn. validators can only recieve coins
    if (db_validator_lookup::exist(public_key))
    {
        return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_coin.cpp: coin_process: The sender of this transaction is a validator.", zera_txn::TXN_STATUS::VALIDATOR_ADDRESS);
    }
    if (db_validator_unbond::exist(public_key))
    {
        return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_coin.cpp: coin_process: The sender of this transaction is in an unbonding period.", zera_txn::TXN_STATUS::VALIDATOR_ADDRESS);
    }

    return ZeraStatus();
}

bool check_safe_send(const zera_txn::BaseTXN base, const std::string &wallet_address)
{
    if(base.safe_send() && !db_wallet_lookup::exist(wallet_address))
    {
        return false;
    }

    return true;
}
