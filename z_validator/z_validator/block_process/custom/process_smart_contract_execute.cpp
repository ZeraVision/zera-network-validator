#include "../block_process.h"
#include "../../temp_data/temp_data.h"
#include "const.h"
#include "wallets.h"
#include "smart_contract_service.h"
#include <typeinfo>
#include <any>
#include "../logging/logging.h"
#include "validators.h"

namespace
{
    void storage_fees(const zera_txn::SmartContractExecuteTXN *txn, const uint256_t &fees, zera_txn::TXNStatusFees &status_fees, const std::string &fee_address)
    {
        uint256_t usd_equiv;
        std::string contract_id = "$ZRA+0000";
        zera_txn::InstrumentContract contract;

        auto wallet_adr = wallets::generate_wallet(txn->base().public_key());

        block_process::process_fees(contract, fees, wallet_adr, contract_id, true, status_fees, txn->base().hash(), fee_address, true);
    }

    void gas_fees(const zera_txn::SmartContractExecuteTXN *txn, const uint64_t &used_gas, zera_txn::TXNStatusFees &status_fees, const std::string &fee_address)
    {
        uint256_t usd_equiv;
        std::string contract_id = txn->base().fee_id();
        zera_txn::InstrumentContract contract;

        block_process::get_cur_equiv(contract_id, usd_equiv);
        block_process::get_contract(contract_id, contract);
        uint256_t denomination(contract.coin_denomination().amount());
        uint256_t gas_used_fee = used_gas * GAS_FEE;

        uint256_t gas_used_fee_value = (gas_used_fee * denomination) / usd_equiv;
        auto wallet_adr = wallets::generate_wallet(txn->base().public_key());

        block_process::process_fees(contract, gas_used_fee_value, wallet_adr, contract_id, true, status_fees, txn->base().hash(), fee_address);
    }
    ZeraStatus gas_limit_calc(const uint256_t &fee_taken, const zera_txn::SmartContractExecuteTXN *txn, uint64_t &gas_approved, uint256_t &fee_left)
    {
        uint256_t usd_equiv;
        std::string contract_id = txn->base().fee_id();
        zera_txn::InstrumentContract contract;

        block_process::get_cur_equiv(contract_id, usd_equiv);
        block_process::get_contract(contract_id, contract);
        uint256_t denomination(contract.coin_denomination().amount());

        uint256_t fee_left_value = (fee_left * usd_equiv) / denomination;

        uint256_t gas = fee_left_value / GAS_FEE;
        logging::print("fee_taken:", fee_taken.str());
        logging::print("gas_approved:", gas.str());

        gas_approved = static_cast<uint64_t>(gas);

        auto wallet_adr = wallets::generate_wallet(txn->base().public_key());

        return balance_tracker::subtract_txn_balance(wallet_adr, contract_id, fee_left, txn->base().hash());
    }
    ZeraStatus check_execute(const zera_txn::SmartContractExecuteTXN *txn, zera_txn::TXNStatusFees &status_fees, const std::string &fee_address, const uint64_t &gas_approved, uint64_t &used_gas, std::vector<std::string> &txn_hashes)
    {
        std::vector<std::any> params_vector;

        for (auto param : txn->parameters())
        {

            const char *value = param.value().c_str();
            std::string type = param.type();

            if (type == "string")
            {
                logging::print("string: ", value, true);
                params_vector.push_back(param.value());
            }
            else if (type == "int")
            {
                int val;
                std::memcpy(&val, value, sizeof(int));
                params_vector.push_back(val);
            }
            else if (type == "bytes")
            {
                size_t length = param.value().size();
                std::vector<uint8_t> byte_array(value, value + length);
                params_vector.push_back(byte_array);
            }
        }

        if (txn->function() == "init")
        {
            logging::print("[ProcessSmartContractExecute] DONE with ERROR: call 'init' function not allowed");
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "call 'init' function not allowed", zera_txn::TXN_STATUS::INVALID_PARAMETERS);
        }

        int instance_number = txn->instance();
        const std::string instance_string = std::to_string(instance_number);

        zera_txn::SmartContractTXN db_contract;

        // read instance contract
        std::string instance_name = txn->smart_contract_name() + "_" + instance_string;

        std::string raw_data;
        db_smart_contracts::get_single(instance_name, raw_data);

        if (raw_data.empty())
        {
            logging::print("[ProcessSmartContractExecute] DONE with ERROR: no smart contract found:", instance_name);
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "no smart contract found", zera_txn::TXN_STATUS::INVALID_PARAMETERS);
        }

        db_contract.ParseFromString(raw_data);

        std::string sender_wallet_adr = wallets::generate_wallet(txn->base().public_key());
        // get dependencies contracts
        std::vector<std::string> dependencies_vector;
        // for (auto dep : db_contract.dependencies())
        // {
        //     dependencies_vector.push_back(dep);
        // }

        const std::string sender_pub_key = wallets::get_public_key_string(txn->base().public_key());

        uint64_t timestamp = txn->base().timestamp().seconds();
        std::string block_txns_key = "BLOCK_TXNS_" + txn->base().hash();
        zera_txn::PublicKey smart_contract_pub_key;
        smart_contract_pub_key.set_smart_contract_auth("sc_" + instance_name);
        std::string smart_contract_wallet = wallets::generate_wallet(smart_contract_pub_key);

        try
        {
            std::vector<std::any> results = smart_contract_service::eval(sender_pub_key, sender_wallet_adr, 
                instance_name, db_contract.binary_code(),
                 db_contract.language(), txn->function(), 
                 params_vector, dependencies_vector, 
                 txn->base().hash(), timestamp, 
                 block_txns_key, fee_address, 
                 smart_contract_wallet, gas_approved, 
                 used_gas, txn_hashes);

            // store result
            for (int i = results.size() - 1; i >= 0; --i)
            {
                std::string val = std::any_cast<std::string>(results[i]);
                logging::print("result", std::to_string(i), ":", val);
                status_fees.add_smart_contract_result(val);
            }
            txn_hash_tracker::add_sc_to_hash();
            nonce_tracker::add_sc_to_used_nonce();
            db_sc_temp::remove_all();
            return ZeraStatus();
        }
        catch (const std::exception &e)
        {
            logging::print("[ProcessSmartContractExecute] Exception caught:", e.what(), true);

            uint32_t version = ValidatorConfig::get_required_version(); 

            if (version >= 101001)
            {
                logging::print("gas fees:", std::to_string(used_gas));
                nonce_tracker::clear_sc_nonce();
                txn_hash_tracker::clear_sc_txn_hash();
                for (auto hash : txn_hashes)
                {
                    balance_tracker::remove_txn_balance(hash);
                }

                std::vector<std::string> keys;
                std::vector<std::string> values;

                db_sc_temp::get_all_data(keys, values);

                int x = 0;

                for (auto key : keys)
                {
                    if (values[x].empty())
                    {
                        db_smart_contracts::remove_single(key);
                    }
                    else
                    {
                        db_smart_contracts::store_single(key, values[x]);
                    }
                    x++;
                }

            }

            txn_hash_tracker::add_sc_to_hash();
            nonce_tracker::add_sc_to_used_nonce();
            db_sc_temp::remove_all();
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "Failed to execute txn", zera_txn::TXN_STATUS::INVALID_TXN_DATA);
        }
    }
}
template <>
ZeraStatus block_process::process_txn<zera_txn::SmartContractExecuteTXN>(const zera_txn::SmartContractExecuteTXN *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, bool timed, const std::string &fee_address, bool sc_txn)
{
    logging::print("[ProcessSmartContractExecute] executing smart contract...", txn->smart_contract_name());
    logging::print("instance:", txn->smart_contract_name());
    logging::print("function:", txn->function());
    logging::print("parameters_size:", std::to_string(txn->parameters_size()));

    uint64_t nonce = txn->base().nonce();
    ZeraStatus status;

    // timed txns do need to check nonce, they have already been checked on the original txn
    if (!timed)
    {
        // check nonce, if its bad return failed txn
        status = block_process::check_nonce(txn->base().public_key(), nonce, txn->base().hash(), sc_txn);

        if (!status.ok())
        {
            return status;
        }
    }

    // only check restricted keys if not timed, original txn has already been checked if it is timed
    if (!timed)
    {
        // this checks to see if the key is valid to send this type of txn, also checks to see if key is from a validator, which is not allowed
        std::string pub_key = wallets::get_public_key_string(txn->base().public_key());
        status = block_process::check_validator(pub_key, txn_type);

        if (!status.ok())
        {
            return ZeraStatus(ZeraStatus::Code::BLOCK_FAULTY_TXN, status.message(), zera_txn::TXN_STATUS::INVALID_TXN_DATA);
        }
    }

    uint256_t fee_taken = 0;
    // process base fees. If wallet cannot pay fees or anything else is wrong with the fees return failed txn
    status = block_process::process_simple_fees_gas(txn, status_fees, zera_txn::TRANSACTION_TYPE::SMART_CONTRACT_EXECUTE_TYPE, fee_taken, fee_address);

    if (!status.ok())
    {
        return status;
    }

    uint64_t gas_approved;
    uint256_t fee_approved(txn->base().fee_amount());
    uint256_t fee_left = fee_approved - fee_taken;
    uint64_t used_gas = 0;
    std::string wallet_adr = wallets::generate_wallet(txn->base().public_key());

    status = gas_limit_calc(fee_taken, txn, gas_approved, fee_left);

    std::vector<std::string> txn_hashes;
    if (status.ok())
    {
        // all of Igors code
        status = check_execute(txn, status_fees, fee_address, gas_approved, used_gas, txn_hashes);
        balance_tracker::add_txn_balance(wallet_adr, txn->base().fee_id(), fee_left, txn->base().hash());
        status_fees.set_gas(used_gas);

        std::string storage_key = "STORAGE_FEE_" + txn->smart_contract_name() + "_" + std::to_string(txn->instance());
        std::string storage_data;

        if (db_smart_contracts::get_single(storage_key, storage_data))
        {
            uint256_t total_fee = 0;
            if (is_valid_uint256(storage_data))
            {
                total_fee = boost::lexical_cast<uint256_t>(storage_data);
            }

            storage_fees(txn, total_fee, status_fees, fee_address);
            db_smart_contracts::remove_single(storage_key);
        }

        if (used_gas > 0)
        {
            gas_fees(txn, used_gas, status_fees, fee_address);
        }
    }

    // nothing went wrong, status is good!
    // add nonce to nonce tracker, if block passed nonce will be stored for wallet

    nonce_tracker::add_nonce(wallet_adr, nonce, txn->base().hash());
    status_fees.set_status(status.txn_status());

    if (!status.ok())
    {

        logging::print(status.read_status());
    }

    logging::print("[ProcessSmartContractExecute] DONE");

    return ZeraStatus();
}