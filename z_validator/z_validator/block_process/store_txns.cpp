#include "block_process.h"
#include "../txn_batch/txn_batch.h"
#include "db_base.h"
#include "../temp_data/temp_data.h"
#include "wallet.pb.h"
#include "wallets.h"
#include "../governance/time_calc.h"
#include "validators.h"
#include "../db/reorg.h"
#include "../governance/gov_process.h"
#include "../restricted/restricted_keys.h"
#include "../logging/logging.h"

namespace
{

    std::string get_block_key(uint64_t height, std::string hash)
    {
        std::ostringstream oss;
        oss << std::setw(20) << std::setfill('0') << height;
        std::string paddedHeight = oss.str();
        return paddedHeight + ":" + hash;
    }

    void staged(const zera_txn::InstrumentContract &contract, const zera_validator::ProposalLedger &old_proposal_ledger, zera_validator::ProposalLedger &new_proposal_ledger)
    {
        bool final_stage = false;
        int stage;

        if (new_proposal_ledger.stage() >= contract.governance().stage_length_size())
        {
            stage = 1;
            new_proposal_ledger.set_stage(1);
            final_stage = true;
        }
        else
        {
            stage = old_proposal_ledger.stage() + 1;
            new_proposal_ledger.set_stage(stage);
        }

        if (final_stage)
        {
            uint32_t days = 0;
            uint32_t months = 0;
            if (contract.governance().proposal_period() == zera_txn::PROPOSAL_PERIOD::DAYS)
            {
                days = contract.governance().voting_period();
            }
            else
            {
                months = contract.governance().voting_period();
            }
            google::protobuf::Timestamp end_ts = time_calc::get_end_date_cycle(old_proposal_ledger.cycle_end_date(), days, months);
            new_proposal_ledger.mutable_cycle_end_date()->set_seconds(end_ts.seconds());
            new_proposal_ledger.mutable_cycle_start_date()->set_seconds(old_proposal_ledger.cycle_end_date().seconds());
            new_proposal_ledger.mutable_proposal_ids()->CopyFrom(old_proposal_ledger.pending_proposal_ids());
        }
        else
        {
            new_proposal_ledger.mutable_cycle_start_date()->set_seconds(old_proposal_ledger.cycle_start_date().seconds());
            new_proposal_ledger.mutable_cycle_end_date()->set_seconds(old_proposal_ledger.cycle_end_date().seconds());
            new_proposal_ledger.mutable_proposal_ids()->CopyFrom(old_proposal_ledger.proposal_ids());
            new_proposal_ledger.mutable_pending_proposal_ids()->CopyFrom(old_proposal_ledger.pending_proposal_ids());
        }

        uint32_t days = 0;
        uint32_t months = 0;

        if (contract.governance().stage_length_size() < stage)
        {
            return;
        }

        if (contract.governance().stage_length().at(stage - 1).period() == zera_txn::PROPOSAL_PERIOD::DAYS)
        {
            days = contract.governance().stage_length().at(stage - 1).length();
        }
        else
        {
            days = contract.governance().stage_length().at(stage - 1).length();
        }

        google::protobuf::Timestamp end_ts = time_calc::get_end_date_cycle(old_proposal_ledger.stage_end_date(), days, months);
        new_proposal_ledger.mutable_stage_end_date()->set_seconds(end_ts.seconds());
        new_proposal_ledger.mutable_stage_start_date()->set_seconds(old_proposal_ledger.stage_end_date().seconds());
    }
    void cycle(const zera_txn::InstrumentContract &contract, const zera_validator::ProposalLedger &old_proposal_ledger, zera_validator::ProposalLedger &new_proposal_ledger)
    {
        uint32_t days = 0;
        uint32_t months = 0;
        if (contract.governance().proposal_period() == zera_txn::PROPOSAL_PERIOD::DAYS)
        {
            days = contract.governance().voting_period();
        }
        else
        {
            months = contract.governance().voting_period();
        }
        google::protobuf::Timestamp end_ts = time_calc::get_end_date_cycle(old_proposal_ledger.cycle_end_date(), days, months);
        new_proposal_ledger.mutable_cycle_end_date()->set_seconds(end_ts.seconds());
        new_proposal_ledger.mutable_cycle_start_date()->set_seconds(old_proposal_ledger.cycle_end_date().seconds());

        new_proposal_ledger.mutable_proposal_ids()->CopyFrom(old_proposal_ledger.pending_proposal_ids());
    }

    void update_staged_cycle(zera_validator::Block *block)
    {
        std::vector<std::string> contract_ids;
        std::vector<std::string> ledger_data;
        std::string header_data;
        zera_validator::BlockHeader header;
        db_proposal_ledger::get_all_data(contract_ids, ledger_data);
        std::string key = get_block_key((block->block_header().block_height() - 1), block->block_header().previous_block_hash());
        db_headers::get_single(key, header_data);
        header.ParseFromString(header_data);

        int x = 0;
        while (x < contract_ids.size())
        {
            bool process = false;
            std::string proposal_ledger_data;
            zera_validator::ProposalLedger old_proposal_ledger;
            zera_validator::ProposalLedger new_proposal_ledger;
            std::string contract_data;
            zera_txn::InstrumentContract contract;
            if (!old_proposal_ledger.ParseFromString(ledger_data.at(x)))
            {
                x++;
                continue;
            }
            if (!db_contracts::get_single(contract_ids.at(x), contract_data) || !contract.ParseFromString(contract_data))
            {
                x++;
                continue;
            }

            if (contract.governance().type() == zera_txn::GOVERNANCE_TYPE::STAGED)
            {
                if (header.timestamp().seconds() >= old_proposal_ledger.stage_end_date().seconds())
                {
                    staged(contract, old_proposal_ledger, new_proposal_ledger);
                    process = true;
                }
            }
            else if (contract.governance().type() == zera_txn::GOVERNANCE_TYPE::CYCLE)
            {
                if (header.timestamp().seconds() >= old_proposal_ledger.cycle_end_date().seconds())
                {
                    cycle(contract, old_proposal_ledger, new_proposal_ledger);
                    process = true;
                }
            }

            if (process)
            {
                db_proposal_ledger::store_single(contract_ids.at(x), new_proposal_ledger.SerializeAsString());
            }
            x++;
        }
    }

    void store_proposal_adjustment()
    {
        std::vector<std::string> keys;
        std::vector<std::string> values;
        rocksdb::WriteBatch proposal_batch;
        db_proposals_temp::get_all_data(keys, values);

        int x = 0;

        while (x < keys.size())
        {
            proposal_batch.Put(keys[x], values[x]);
            x++;
        }
        db_proposals::store_batch(proposal_batch);
        db_proposals_temp::remove_all();
    }

    // if you have been unbonding for 7 days, remove from unbonding list
    void remove_unbonding_validators(const zera_validator::BlockHeader &header)
    {
        std::vector<std::string> keys;
        std::vector<std::string> values;
        db_validator_unbond::get_all_data(keys, values);
        rocksdb::WriteBatch unbond_batch;

        int x = 0;
        while (x < keys.size())
        {
            google::protobuf::Timestamp timestamp;
            timestamp.ParseFromString(values[x]);
            int time_passed = header.timestamp().seconds() - timestamp.seconds();

            if (time_passed >= 604800)
            {
                unbond_batch.Delete(keys[x]);
            }

            x++;
        }

        db_validator_unbond::store_batch(unbond_batch);
    }

    void update_proposal_ledgers(zera_validator::Block *block)
    {
        db_transactions::remove_single("1");
        rocksdb::WriteBatch process_ledger_batch;
        rocksdb::WriteBatch proposal_batch;

        for (auto result : block->transactions().proposal_result_txns())
        {
            if ((result.fast_quorum() && result.passed()) || (!result.fast_quorum() && result.final_stage()))
            {
                logging::print("Delete proposal:", base58_encode(result.proposal_id()));
            }
        }
        update_staged_cycle(block);
    }

    void update_proposal_heartbeat(zera_validator::Block *block)
    {
        if(block->block_header().block_height() == 0)
        {
            return;
        }
        
        std::string pub_str = wallets::get_public_key_string(block->block_header().public_key());
        std::string validator_str;
        zera_txn::Validator validator;
        db_validators::get_single(pub_str, validator_str);

        validator.ParseFromString(validator_str);
        validator.set_last_heartbeat(block->block_header().block_height());
        validator.set_online(true);
        validator.set_version(block->block_header().version());
        db_validators::store_single(pub_str, validator.SerializeAsString());
    }
}
ZeraStatus block_process::store_txns(zera_validator::Block *block, bool archive, bool backup)
{
    logging::print("****************************\nStoring Block: " + std::to_string(block->block_header().block_height()));
    logging::print("****************************");

    block_process::store_wallets();
    allowance_tracker::update_allowance_database();

    auto txns = block->transactions();
    std::map<std::string, bool> txn_passed;
    txn_batch::find_passed(txn_passed, txns);
    txn_batch::batch_contracts(txns, txn_passed);
    txn_batch::batch_contract_updates(txns, txn_passed);
    txn_batch::batch_item_mint(txns, txn_passed);
    txn_batch::batch_nft_transfer(txns, txn_passed);
    txn_batch::batch_proposals(txns, txn_passed, block->block_header().timestamp().seconds());
    txn_batch::batch_votes(txns, txn_passed);
    txn_batch::batch_proposal_results(txns, txn_passed);
    txn_batch::batch_currency_equiv(txns, txn_passed);
    txn_batch::batch_authorized_equiv(txns, txn_passed);
    txn_batch::batch_foundation(txns, txn_passed);
    txn_batch::batch_compliance(txns, txn_passed);
    txn_batch::batch_delegated_voting(txns, txn_passed);
    txn_batch::batch_validator_registration(txns, txn_passed, block->block_header());
    txn_batch::batch_validator_heartbeat(txns, txn_passed, block->block_header().block_height());
    txn_batch::batch_smart_contract(txns, txn_passed);
    txn_batch::batch_instantiate(txns, txn_passed);
    txn_batch::batch_allowance_txns(txns, txn_passed, block->block_header().timestamp().seconds());
    std::string block_height_str = std::to_string(block->block_header().block_height());
    contract_price_tracker::store_prices();
    nonce_tracker::store_used_nonce(block_height_str);
    store_proposal_adjustment();
    item_tracker::clear_items();
    sbt_burn_tracker::clear_burns();
    remove_unbonding_validators(block->block_header());
    update_proposal_ledgers(block);

    supply_tracker::supply_to_database();
    std::string block_height = std::to_string(block->block_header().block_height());

    update_proposal_heartbeat(block);

    if (archive)
    {
        validator_utils::archive_balances(block_height);
    }

    if (backup)
    {
        Reorg::backup_blockchain(block_height);
    }

    gov_process::check_ledgers(block);
    restricted_keys_check::check_quash_ledger(block);
    txn_batch::batch_required_version(txns, txn_passed);
    logging::print("****************************\nDONE Storing Block: " + std::to_string(block->block_header().block_height()));
    return ZeraStatus();
}
