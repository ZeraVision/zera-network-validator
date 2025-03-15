#include "compliance.h"
#include "../block_process/block_process.h"
#include "wallets.h"
#include "db_base.h"
#include "base58.h"
#include "validator.pb.h"
#include "txn.pb.h"
namespace
{
    bool check_wallet(const std::string &wallet_address, const zera_txn::InstrumentContract &contract)
    {

        std::string compliance_data;
        zera_validator::WalletLookup wallet_lookup;
        if (!db_wallet_lookup::get_single(wallet_address, compliance_data) || !wallet_lookup.ParseFromString(compliance_data))
        {
            return false;
        }

        zera_validator::BlockHeader header;
        std::string key;
        db_headers_tag::get_last_data(header, key);

        bool compliant = true;
        std::vector<std::string> delete_contract_id;
        std::vector<int> delete_inner_index;

        for (auto token_compliance : contract.token_compliance())
        {
            compliant = true;
            for (auto compliance : token_compliance.compliance())
            {
                auto compliance_map = wallet_lookup.mutable_compliance();
                auto it = compliance_map->find(compliance.contract_id());

                if (it == compliance_map->end())
                {
                    compliant = false;
                    break;
                }

                auto compliance_levels = it->second;
                bool level_compliant = false;

                int x = 0;
                for (auto level : compliance_levels.levels())
                {
                    if (level.level() == compliance.compliance_level())
                    {
                        if (level.expiry().seconds() != 0)
                        {
                            if (level.expiry().seconds() < header.timestamp().seconds())
                            {
                                delete_contract_id.push_back(compliance.contract_id());
                                delete_inner_index.push_back(x);
                                break;
                            }
                        }

                        level_compliant = true;
                        break;
                    }
                    x++;
                }

                if (!level_compliant)
                {
                    compliant = false;
                    break;
                }
            }

            if (compliant)
            {
                break;
            }
        }

        auto compliance_map = wallet_lookup.mutable_compliance();
        int size = delete_contract_id.size() - 1;
        for (int i = size; i >= 0; i--)
        {
            auto compliance_levels = (*compliance_map)[delete_contract_id[i]];
            auto levels = compliance_levels.mutable_levels();
            levels->DeleteSubrange(delete_inner_index[i], 1);
        }
        if (delete_contract_id.size() > 0)
        {
            db_wallet_lookup::store_single(wallet_address, wallet_lookup.SerializeAsString());
        }
        return compliant;
    }
}

bool compliance::check_compliance(const std::string &wallet_address, const zera_txn::InstrumentContract &contract)
{
    if (!contract.kyc_status())
    {
        return true;
    }

    bool compliant = check_wallet(wallet_address, contract);
    return compliant;
}