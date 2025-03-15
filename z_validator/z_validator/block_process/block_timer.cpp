#include "block_process.h"

#include <google/protobuf/util/time_util.h>
#include <thread>

#include "proposer.h"
#include "validator_network_client.h"
#include "../governance/gov_process.h"
#include "../attestation/attestation_process.h"
#include "threadpool.h"
#include "../logging/logging.h"

namespace
{
    void create_heartbeat(zera_txn::ValidatorHeartbeat &heartbeat, const uint64_t &nonce)
    {
        heartbeat.set_online(true);
        heartbeat.set_version(ValidatorConfig::get_version());
        zera_txn::BaseTXN *base = heartbeat.mutable_base();
        base->mutable_public_key()->set_single(ValidatorConfig::get_gen_public_key());
        base->set_fee_id("$ZRA+0000");
        base->set_fee_amount("10000000000");
        base->set_memo("Validator Heartbeat");
        base->set_nonce(nonce);

        google::protobuf::Timestamp *ts = base->mutable_timestamp();
        google::protobuf::Timestamp now_ts = google::protobuf::util::TimeUtil::GetCurrentTime();
        ts->set_seconds(now_ts.seconds());
        ts->set_nanos(now_ts.nanos());

        signatures::sign_txns(&heartbeat, ValidatorConfig::get_gen_key_pair());

        auto hash_vec = Hashing::sha256_hash(heartbeat.SerializeAsString());
        std::string hash(hash_vec.begin(), hash_vec.end());
        base->set_hash(hash);
    }

    void send_heartbeat(const std::string &wallet_adr)
    {
        zera_txn::ValidatorHeartbeat *heartbeat = new zera_txn::ValidatorHeartbeat();
        std::string nonce_str;
        db_wallet_nonce::get_single(wallet_adr, nonce_str);
        uint64_t nonce = std::stoull(nonce_str) + 1;

        create_heartbeat(*heartbeat, nonce);
        pre_process::process_txn(heartbeat, zera_txn::TRANSACTION_TYPE::VALIDATOR_HEARTBEAT_TYPE);
        logging::print("sending heartbeat nonce:", std::to_string(nonce));
    }

    // check if its been 5 seconds since last block, if not wait
    void check_time_dif(const zera_validator::BlockHeader &last_header, int &proposal_timer)
    {
        google::protobuf::Timestamp last_ts = last_header.timestamp();
        google::protobuf::Timestamp now_ts = google::protobuf::util::TimeUtil::GetCurrentTime();
        int64_t time_dif = now_ts.seconds() - last_ts.seconds();

        if (time_dif < 5)
        {
            proposal_timer++;
            std::this_thread::sleep_for(std::chrono::seconds(5 - time_dif));
        }
    }

}
void block_process::start_block_process()
{
    bool has_transactions = false;
    bool my_block = true;
    int proposal_timer = 0;
    int last_heartbeat = 0;
    std::string wallet_adr = wallets::generate_wallet_single(ValidatorConfig::get_public_key());

    while (true)
    {
        if (last_heartbeat >= 450)
        {
            send_heartbeat(wallet_adr);
            last_heartbeat = 0;
        }

        uint32_t proposer_index = 0;             // which validator to use in the lottery proposer list (after each failed attempt to create a block, the next validator in the list is used)
        uint32_t block_sync_attempts = 0;        // after attempting to get a block from the network 5 times, the next validator in the list is used
        zera_validator::BlockHeader last_header; // get last block header
        std::string last_key;
        db_headers_tag::get_last_data(last_header, last_key);
        std::vector<zera_txn::Validator> proposers = SelectValidatorsByWeight(last_header.hash(), last_header.block_height()); // select validators for the lottery
        logging::print("by wieght proposers size:", std::to_string(proposers.size()));
        proposer_tracker::clear_proposers();
        has_transactions = false;

        check_time_dif(last_header, proposal_timer);

        logging::print("Waiting for txn to create block.");

        while (!has_transactions)
        {
            while (!my_block)
            {
                zera_validator::BlockHeader new_header;
                std::string new_key;

                if (db_headers_tag::get_last_data(new_header, new_key) && new_header.block_height() > last_header.block_height())
                {
                    last_header.CopyFrom(new_header);
                    last_key = new_key;
                    my_block = true;
                }
                else
                {
                    block_sync_attempts++;
                    ValidatorNetworkClient::StartSyncBlockchain();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (block_sync_attempts >= 5)
                {
                    block_sync_attempts = 0;

                    if (proposer_index < proposers.size() - 1)
                    {
                        proposer_index++;
                        logging::print("proposer_index:", std::to_string(proposer_index));
                        proposer_tracker::add_proposer(proposers.at(proposer_index));
                    }
                    break;
                }

                std::string proposer_pub = wallets::get_public_key_string(proposers.at(proposer_index).public_key());

                if (proposer_pub == ValidatorConfig::get_public_key())
                {
                    my_block = true;
                }
            }

            if (proposal_timer >= 5)
            {
                proposal_timer = 0;

                txn_tracker::update_txn_ledger();
            }

            transactions txns;

            if (proposing::get_transactions(txns))
            {
                logging::print("Has transactions -", std::to_string(txns.keys.size()));
                logging::print("Has pre_processed transactions -", std::to_string(txns.processed_keys.size()));
                logging::print("Has timed transactions -", std::to_string(txns.timed_keys.size()));
                logging::print("Has sc transactions -", std::to_string(txns.sc_keys.size()));
                logging::print("Has gov transactions -", std::to_string(txns.gov_keys.size()));

                std::string proposer_pub = wallets::get_public_key_string(proposers.at(proposer_index).public_key());

                if (proposer_pub != ValidatorConfig::get_public_key())
                {
                    logging::print("not my block to propose attempt", std::to_string(proposer_index));
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    my_block = false;
                    break;
                }

                // create block
                has_transactions = true;

                zera_validator::Block *block = new zera_validator::Block();

                if (!proposing::make_block(block, txns).ok())
                {
                    logging::print("Error creating block");
                    break;
                }

                logging::print("Block created", std::to_string(block->block_header().block_height()), false);

                block_process::store_txns(block);

                zera_validator::Block* block_copy = new zera_validator::Block();
                block_copy->CopyFrom(*block);

                ValidatorThreadPool &pool = ValidatorThreadPool::getInstance();
                ValidatorThreadPool &pool1 = ValidatorThreadPool::getInstance();

                // Enqueue both tasks into the same thread pool
                pool.enqueueTask([block](){ 
                    ValidatorNetworkClient::StartGossip(block);
                    delete block;
                    });

                pool1.enqueueTask([block_copy](){ 
                    AttestationProcess::CreateAttestation(block_copy);
                    delete block_copy;
                    });
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            proposal_timer++;
        }
        last_heartbeat++;
    }
}