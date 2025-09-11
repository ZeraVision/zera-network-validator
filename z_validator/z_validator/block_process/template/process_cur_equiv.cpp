#include <regex>

#include "../block_process.h"
#include "../../temp_data/temp_data.h"
#include "utils.h"
#include "../restricted/restricted_keys.h"
#include "../logging/logging.h"
#include "fees.h"

namespace
{
    ZeraStatus check_contract(const std::string &contract_id)
    {
        zera_txn::InstrumentContract contract;
        ZeraStatus status = block_process::get_contract(contract_id, contract);
        if (!status.ok())
        {
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_contract: Contract does not exist. - " + contract_id, zera_txn::TXN_STATUS::INVALID_CONTRACT);
        }

        if (contract.type() != zera_txn::CONTRACT_TYPE::TOKEN)
        {
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_contract: Contract is not a token.", zera_txn::TXN_STATUS::INVALID_CONTRACT);
        }

        return ZeraStatus();
    }

    ZeraStatus check_auth_parameters(const zera_txn::AuthorizedCurrencyEquiv *txn)
    {
        if (txn->cur_equiv_size() <= 0)
        {
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: No currency equivs.", zera_txn::TXN_STATUS::INVALID_CONTRACT_PARAMETERS);
        }

        for (auto cur_equiv : txn->cur_equiv())
        {
            ZeraStatus status = check_contract(cur_equiv.contract_id());
            if (!status.ok())
            {
                logging::print("status:", status.read_status());
                return status;
            }

            if (cur_equiv.contract_id() != ZERA_SYMBOL)
            {
                if (!cur_equiv.has_max_stake())
                {
                    return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: No max stake: Auth", zera_txn::TXN_STATUS::INVALID_CONTRACT_PARAMETERS);
                }

                if (!cur_equiv.has_authorized())
                {
                    return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: authorized parameter not is false.", zera_txn::TXN_STATUS::INVALID_CONTRACT_PARAMETERS);
                }

                if (!is_valid_uint256(cur_equiv.max_stake()))
                {

                    return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: Invalid max stake.", zera_txn::TXN_STATUS::INVALID_UINT256);
                }
            }

            if (!is_valid_uint256(cur_equiv.rate()))
            {
                return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: Invalid rate.", zera_txn::TXN_STATUS::INVALID_UINT256);
            }

            uint256_t rate(cur_equiv.rate());

            if (rate == 0)
            {
                return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: Invalid rate of 0.", zera_txn::TXN_STATUS::INVALID_UINT256);
            }
        }
        return ZeraStatus();
    }

    ZeraStatus check_restricted(const std::string &contract_id, std::string public_key)
    {
        zera_txn::InstrumentContract contract;
        ZeraStatus status = block_process::get_contract(contract_id, contract);
        if (!status.ok())
        {
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_restricted: Contract does not exist. - " + contract_id, zera_txn::TXN_STATUS::INVALID_CONTRACT);
        }

        std::regex pattern("^\\:.*");

        for (auto r_keys : contract.restricted_keys())
        {
            std::string r_public_key = *(r_keys.mutable_public_key()->mutable_single());
            if (std::regex_match(r_public_key, pattern))
            {
                std::string inherited = r_keys.mutable_public_key()->mutable_single()->substr(1);
                zera_txn::InstrumentContract inherited_contract;
                ZeraStatus status = block_process::get_contract(inherited, inherited_contract);

                if (!status.ok())
                {
                    break;
                }

                for (auto inherited_key : inherited_contract.restricted_keys())
                {
                    std::string pub_key = wallets::get_public_key_string(inherited_key.public_key());
                    if (pub_key == public_key && inherited_key.cur_equiv())
                    {
                        return ZeraStatus();
                    }
                }
            }

            std::string pub_key = wallets::get_public_key_string(r_keys.public_key());
            if (pub_key == public_key && r_keys.cur_equiv())
            {
                return ZeraStatus();
            }
        }

        return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_restricted: Public key is not authorized to use this contract.", zera_txn::TXN_STATUS::INVALID_AUTH_KEY);
    }
}

template <>
ZeraStatus block_process::process_txn<zera_txn::AuthorizedCurrencyEquiv>(const zera_txn::AuthorizedCurrencyEquiv *txn, zera_txn::TXNStatusFees &status_fees, const zera_txn::TRANSACTION_TYPE &txn_type, bool timed, const std::string &fee_address, bool sc_txn)
{
    uint64_t nonce = txn->base().nonce();
    ZeraStatus status;

    if (!timed)
    {
        status = block_process::check_nonce(txn->base().public_key(), nonce, txn->base().hash(), sc_txn);

        if (!status.ok())
        {
            return status;
        }
    }

    zera_txn::InstrumentContract fee_contract;
    status = block_process::get_contract(txn->base().fee_id(), fee_contract);
    if (!status.ok())
    {
        return status;
    }

    status = zera_fees::process_simple_fees(txn, status_fees, txn_type, fee_address);

    if (!status.ok())
    {
        return status;
    }

    zera_txn::InstrumentContract contract;
    block_process::get_contract("$ACE+0000", contract);
    status = restricted_keys_check::check_restricted_keys(txn, contract, txn_type, timed);

    if (status.ok())
    {
        status = check_auth_parameters(txn);
    }

    balance_tracker::store_temp_database();
    status_fees.set_status(status.txn_status());

    if (!status.ok())
    {
        logging::print(status.read_status());
    }
    std::string wallet_adr = wallets::generate_wallet(txn->base().public_key());
    status_fees.set_status(status.txn_status());
    nonce_tracker::add_nonce(wallet_adr, nonce, txn->base().hash());

    return ZeraStatus();
}
template <>
ZeraStatus block_process::check_parameters<zera_txn::SelfCurrencyEquiv>(const zera_txn::SelfCurrencyEquiv *txn, zera_txn::TXNStatusFees &status_fees, const std::string &fee_address)
{
    if (txn->cur_equiv_size() <= 0)
    {
        return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: No currency equivs.", zera_txn::TXN_STATUS::INVALID_CONTRACT_PARAMETERS);
    }

    for (auto cur_equiv : txn->cur_equiv())
    {
        ZeraStatus status = check_contract(cur_equiv.contract_id());
        if (!status.ok())
        {
            return status;
        }

        if (cur_equiv.contract_id() != ZERA_SYMBOL)
        {
            if (cur_equiv.has_max_stake())
            {
                return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: No max stake: Self", zera_txn::TXN_STATUS::INVALID_CONTRACT_PARAMETERS);
            }
            if (cur_equiv.authorized())
            {
                return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: No authorized.", zera_txn::TXN_STATUS::INVALID_CONTRACT_PARAMETERS);
            }
        }

        if (!is_valid_uint256(cur_equiv.rate()))
        {
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: Invalid rate.", zera_txn::TXN_STATUS::INVALID_UINT256);
        }

        uint256_t rate(cur_equiv.rate());

        if(rate <= 0)
        {
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_cur_equiv.cpp: check_auth_parameters: Invalid rate of 0.", zera_txn::TXN_STATUS::INVALID_UINT256);
        }

        return ZeraStatus();
    }
}