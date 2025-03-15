#include "txn_batch.h"
#include "db_base.h"
#include "base58.h"
#include "validators.h"

void txn_batch::batch_required_version(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed)
{
    if (txns.has_required_version_txn())
    {
        auto required_version = txns.required_version_txn();

        if (txn_passed.at(required_version.base().hash()))
        {
            db_system::store_single(REQUIRED_VERSION, required_version.SerializeAsString());

            if(required_version.version(0) == 101000)
            {
                ValidatorConfig::set_version(101000);
                ValidatorConfig::set_treasury_wallet("4Yg2ZeYrzMjVBXvU2YWtuZ7CzWR9atnQCD35TQj1kKcH");
            }
        }
    }
}
void txn_batch::batch_currency_equiv(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed)
{
    for (auto equiv : txns.self_cur_equivs())
    {
        if (txn_passed.at(equiv.base().hash()))
        {
            for (auto currency_equiv : equiv.cur_equiv())
            {
                zera_validator::CurrencyRate cur_rate;
                cur_rate.set_qualified(false);
                cur_rate.set_rate(currency_equiv.rate());
                cur_rate.set_max_stake(currency_equiv.max_stake());
                db_currency_equiv::store_single(currency_equiv.contract_id(), cur_rate.SerializeAsString());
            }
        }
    }
}
void txn_batch::batch_authorized_equiv(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed)
{
    for (auto equiv : txns.auth_cur_equivs())
    {
        if (txn_passed.at(equiv.base().hash()))
        {
            for (auto currency_equiv : equiv.cur_equiv())
            {
                zera_validator::CurrencyRate cur_rate;
                cur_rate.set_qualified(currency_equiv.authorized());
                cur_rate.set_rate(currency_equiv.rate());
                db_currency_equiv::store_single(currency_equiv.contract_id(), cur_rate.SerializeAsString());
            }
        }
    }
}

void txn_batch::batch_foundation(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed)
{
    for (auto foundation : txns.foundation_txns())
    {
        if (txn_passed.at(foundation.base().hash()))
        {
            if (foundation.restricted_symbols_size() > 0)
            {
                zera_validator::RestrictedSymbols symbols;
                for (auto symbol : foundation.restricted_symbols())
                {
                    symbols.add_symbols(symbol);
                }
                db_foundation::store_single(RESTRICTED_SYMBOLS, symbols.SerializeAsString());
            }

            if (foundation.byte_multiplier_size() > 0)
            {

                for (auto multiplier : foundation.byte_multiplier())
                {
                    std::string txn_type_str = zera_txn::TRANSACTION_TYPE_Name(multiplier.txn_type());
                    db_foundation::store_single(txn_type_str, multiplier.multiplier());
                }
            }
        }
    }
}

void txn_batch::batch_revoke(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed)
{

    leveldb::WriteBatch revoke_batch;

    for (auto revoke : txns.revoke_txns())
    {
        if (txn_passed.at(revoke.base().hash()))
        {
            revoke_batch.Delete(revoke.item_id() + revoke.contract_id());
        }
    }

    db_contract_items::store_batch(revoke_batch);
}
void txn_batch::batch_compliance(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed)
{
    leveldb::WriteBatch compliance_batch;
    for (auto compliance : txns.compliance_txns())
    {
        if (txn_passed.at(compliance.base().hash()))
        {
            for (auto compliance_assign : compliance.compliance())
            {
                std::string compliance_data;
                zera_validator::WalletLookup wallet_lookup;

                if (db_wallet_lookup::get_single(compliance_assign.recipient_address(), compliance_data))
                {
                    wallet_lookup.ParseFromString(compliance_data);
                }

                auto compliance_map = wallet_lookup.mutable_compliance();
                bool continue_while = true;
                while (continue_while)
                {
                    auto it = compliance_map->find(compliance.contract_id());
                    if (it != compliance_map->end())
                    {
                        // Key exists, remove the item from the repeated field
                        auto &compliance_levels = it->second;
                        auto levels = compliance_levels.mutable_levels();
                        bool found = false;
                        for (int i = 0; i < levels->size(); ++i)
                        {
                            if (levels->Get(i).level() == compliance_assign.compliance_level())
                            {
                                levels->SwapElements(i, levels->size() - 1);
                                levels->RemoveLast();
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                        {
                            continue_while = false;
                        }
                    }
                    else
                    {
                        continue_while = false;
                    }
                }
                if (compliance_assign.assign_revoke())
                {
                    zera_validator::ComplianceData data;

                    data.set_level(compliance_assign.compliance_level());
                    data.mutable_expiry()->CopyFrom(compliance_assign.expiry());

                    (*wallet_lookup.mutable_compliance())[compliance.contract_id()].add_levels()->CopyFrom(data);
                    for (auto levels : (*wallet_lookup.mutable_compliance())[compliance.contract_id()].levels())
                    {
                    }
                }

                compliance_batch.Put(compliance_assign.recipient_address(), wallet_lookup.SerializeAsString());

                db_wallet_lookup::store_batch(compliance_batch);
            }
        }
    }
}