#include "block_process.h"

#include <google/protobuf/util/time_util.h>
#include <thread>

#include "proposer.h"
#include "validator_network_client.h"
#include "../governance/gov_process.h"
#include "../attestation/attestation_process.h"
#include "threadpool.h"
#include "../logging/logging.h"
#include "zera_manager.h"

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
        pre_process::process_txn(heartbeat, zera_txn::TRANSACTION_TYPE::VALIDATOR_HEARTBEAT_TYPE, "");
        logging::print("sending heartbeat nonce:", std::to_string(nonce));
    }

    // check if its been 5 seconds since last block, if not wait
    void check_time_dif(BlockManager &block_manager)
    {
        google::protobuf::Timestamp last_ts = block_manager.last_header.timestamp();
        google::protobuf::Timestamp now_ts = google::protobuf::util::TimeUtil::GetCurrentTime();
        int64_t time_dif = now_ts.seconds() - last_ts.seconds();

        logging::print("time dif:", std::to_string(time_dif), true);

        if (time_dif < 5)
        {
            int64_t sleep_time = 5 - time_dif;
            block_manager.proposal_timer++;
            logging::print("sleeping for:", std::to_string(sleep_time), true);
            std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
        }
    }

    void get_proposer_or_new_block(BlockManager &block_manager)
    {
        while (!block_manager.my_block)
        {
            block_manager.new_header.Clear();
            block_manager.new_key = "";

            block_manager.proposer_pub = wallets::get_public_key_string(block_manager.proposers.at(block_manager.proposer_index).public_key());

            if (block_manager.proposer_pub == ValidatorConfig::get_public_key())
            {
                block_manager.my_block = true;
                break;
            }

            if (db_headers_tag::get_last_data(block_manager.new_header, block_manager.new_key) && block_manager.new_header.block_height() > block_manager.last_header.block_height())
            {
                block_manager.last_header.CopyFrom(block_manager.new_header);
                block_manager.last_key = block_manager.new_key;
                break;
            }
            else
            {
                block_manager.block_sync_attempts++;
                ValidatorNetworkClient::StartSyncBlockchain();

                if (db_headers_tag::get_last_data(block_manager.new_header, block_manager.new_key) && block_manager.new_header.block_height() > block_manager.last_header.block_height())
                {
                    block_manager.last_header.CopyFrom(block_manager.new_header);
                    block_manager.last_key = block_manager.new_key;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (block_manager.block_sync_attempts >= 5)
            {
                block_manager.block_sync_attempts = 0;

                if (block_manager.proposer_index < block_manager.proposers.size() - 1)
                {
                    block_manager.proposer_index++;
                    logging::print("proposer_index:", std::to_string(block_manager.proposer_index));
                    proposer_tracker::add_proposer(block_manager.proposers.at(block_manager.proposer_index));
                }
            }
        }

        block_manager.same_block = false;
    }

    void process_block(BlockManager &block_manager)
    {
        logging::print("Has transactions -", std::to_string(block_manager.txns.keys.size()));
        logging::print("Has pre_processed transactions -", std::to_string(block_manager.txns.processed_keys.size()));
        logging::print("Has timed transactions -", std::to_string(block_manager.txns.timed_keys.size()));
        logging::print("Has sc transactions -", std::to_string(block_manager.txns.sc_keys.size()));
        logging::print("Has gov transactions -", std::to_string(block_manager.txns.gov_keys.size()));

        block_manager.proposer_pub = wallets::get_public_key_string(block_manager.proposers.at(block_manager.proposer_index).public_key());

        if (block_manager.proposer_pub != ValidatorConfig::get_public_key())
        {
            logging::print("not my block to propose attempt", std::to_string(block_manager.proposer_index));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            block_manager.my_block = false;
            return;
        }

        block_manager.last_heartbeat = 0;
        // create block
        block_manager.has_transactions = true;

        zera_validator::Block *block = new zera_validator::Block();

        Stopwatch stopwatch;
        stopwatch.start();
        google::protobuf::Timestamp *tsp = block->mutable_block_header()->mutable_timestamp();
        tsp->CopyFrom(google::protobuf::util::TimeUtil::GetCurrentTime());

        if (!proposing::make_block(block, block_manager.txns, stopwatch).ok())
        {
            logging::print("Error creating block");
            return;
        }

        logging::print("Block created", std::to_string(block->block_header().block_height()), false);

        block_process::store_txns(block);

        zera_validator::Block *block_copy = new zera_validator::Block();
        block_copy->CopyFrom(*block);

        // Enqueue both tasks into the same thread pool
        ValidatorThreadPool::enqueueTask([block]()
                                         { 
                    ValidatorNetworkClient::StartGossip(block);
                    delete block; });

        ValidatorThreadPool::enqueueTask([block_copy]()
                                         { 
                    AttestationProcess::CreateAttestation(block_copy);
                    delete block_copy; });

    }

}

void block_process::start_block_process_v2()
{
    BlockManager block_manager;
    block_manager.has_transactions = false;
    block_manager.my_block = true;
    block_manager.proposal_timer = 0;
    block_manager.last_heartbeat = 0;
    block_manager.same_block = true;
    block_manager.wallet_adr = wallets::generate_wallet_single(ValidatorConfig::get_public_key());

    while (true)
    {
        if (block_manager.last_heartbeat >= 450)
        {
            send_heartbeat(block_manager.wallet_adr);
            block_manager.last_heartbeat = 0;
        }

        block_manager.reset();
        db_headers_tag::get_last_data(block_manager.last_header, block_manager.last_key);
        block_manager.proposers = SelectValidatorsByWeight(block_manager.last_header.hash(), block_manager.last_header.block_height()); // select validators for the lottery
        logging::print("by wieght proposers size:", std::to_string(block_manager.proposers.size()));
        proposer_tracker::clear_proposers();

        if(block_manager.proposers.size() > 0)
        {
            proposer_tracker::add_proposer(block_manager.proposers.at(0));
        }

        check_time_dif(block_manager);

        logging::print("Waiting for txn to create block.");

        while (block_manager.same_block)
        {
            if (block_manager.proposal_timer >= 5)
            {
                block_manager.proposal_timer = 0;
                txn_tracker::update_txn_ledger();
            }

            if (proposing::get_transactions(block_manager.txns))
            {
                get_proposer_or_new_block(block_manager);

                if (block_manager.my_block)
                {
                    process_block(block_manager);
                }
            }
            else if (db_headers_tag::get_last_data(block_manager.new_header, block_manager.new_key) && block_manager.new_header.block_height() > block_manager.last_header.block_height())
            {
                block_manager.last_header.CopyFrom(block_manager.new_header);
                block_manager.last_key = block_manager.new_key;
                block_manager.same_block = false;
                break;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            block_manager.proposal_timer++;
        }

        block_manager.last_heartbeat++;
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

        //check_time_dif(last_header, proposal_timer);

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
                    break;
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

                Stopwatch stopwatch;
                stopwatch.start();
                google::protobuf::Timestamp *tsp = block->mutable_block_header()->mutable_timestamp();
                tsp->CopyFrom(google::protobuf::util::TimeUtil::GetCurrentTime());

                if (!proposing::make_block(block, txns, stopwatch).ok())
                {
                    logging::print("Error creating block");
                    break;
                }

                logging::print("Block created", std::to_string(block->block_header().block_height()), false);

                block_process::store_txns(block);

                zera_validator::Block *block_copy = new zera_validator::Block();
                block_copy->CopyFrom(*block);

                // Enqueue both tasks into the same thread pool
                ValidatorThreadPool::enqueueTask([block]()
                                                 { 
                    ValidatorNetworkClient::StartGossip(block);
                    delete block; });

                ValidatorThreadPool::enqueueTask([block_copy]()
                                                 { 
                    AttestationProcess::CreateAttestation(block_copy);
                    delete block_copy; });
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            proposal_timer++;
        }
        last_heartbeat++;
    }
}