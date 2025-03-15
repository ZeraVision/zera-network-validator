#include <cstdio>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <time.h>
#include <locale>
#include <thread>
#include <string_view>
#include <chrono>
#include <regex>

#include <leveldb/write_batch.h>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/time_util.h>

#include "const.h"

#include "db_base.h"
#include "debug.h"
#include "validators.h"
#include "test.h"
#include "../block_process/block_process.h"
#include "validator.pb.h"
#include "validator_network_service_grpc.h"
#include "client_network_service.h"
#include "validator_network_client.h"
#include "base58.h"
#include "block.h"
#include "proposer.h"
#include "verify_process_txn.h"
#include "hex_conversion.h"
#include "utils.h"
#include "governance/gov_process.h"
#include "../temp_data/temp_data.h"
#include "hashing.h"
#include "util/default_values.h"
#include "db/reorg.h"
#include "hex_conversion.h"
#include "crypto/merkle.h"
#include "logging/logging.h"

void initial_archive()
{
    zera_validator::BlockHeader last_header;
    std::string last_key;
    db_headers_tag::get_last_data(last_header, last_key);
    std::string last_height = std::to_string(last_header.block_height());
    validator_utils::archive_balances(last_height);
}
bool check_config()
{
    if (ValidatorConfig::get_host() == "" || ValidatorConfig::get_client_port() == "" || ValidatorConfig::get_validator_port() == "" || ValidatorConfig::get_seed_validators().size() == 0 || ValidatorConfig::get_private_key() == "" || ValidatorConfig::get_public_key() == "" || ValidatorConfig::get_fee_address_string() == "")
    {
        return false;
    }

    return true;
}
void RunValidator()
{
    ValidatorServiceImpl validator_service;
    validator_service.StartService();
}

void RunClient()
{
    ClientNetworkServiceImpl client_service;
    client_service.StartService();
}

void configure_self(zera_txn::ValidatorRegistration &registration_message)
{
    ValidatorConfig::set_config();
    std::string validator_config = ValidatorConfig::get_block_height();

    if (validator_config != "NONE" && validator_config != "")
    {
        Reorg::restore_database(validator_config);
    }

    ValidatorConfig::generate_keys();

    set_explorer_config();
    zera_txn::Validator validator;

    store_self(&validator);
    // gets registration txn and removes private key from memory
    // now using generated key for signing blocks
    get_validator_registration(&validator, &registration_message);

    restricted_symbols();
}
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

void send_heartbeat(const uint64_t &nonce)
{
    zera_txn::ValidatorHeartbeat *heartbeat = new zera_txn::ValidatorHeartbeat();

    create_heartbeat(*heartbeat, nonce);

    ValidatorNetworkClient::StartGossip(heartbeat);
    delete heartbeat;
}

int main()
{
    //  Initialize libsodium
    if (sodium_init() == -1)
    {
        std::cerr << "Error initializing libsodium\n";
        return -1;
    }

    // open all databases
    open_dbs();

    // create a validator registration message to apply to the blockchain
    zera_txn::ValidatorRegistration registration_message;
    configure_self(registration_message);

    if (!check_config())
    {
        logging::print("Configuration Error: Please check your validator configuration file.");
        return -1;
    }

    //Prints the startup logs
    debug::startup_logs();

    // Send registration message and sync blockchain
    ValidatorNetworkClient::StartRegisterSeeds(&registration_message);
    logging::print("Registering to Zera Network... 10 seconds", false);
    std::this_thread::sleep_for(std::chrono::seconds(10));

    if (!ValidatorNetworkClient::StartSyncBlockchain(true))
    {
        return -1;
    }

    logging::print("Successfull Network sync.", false);

    // start validator and client threads
    std::thread thread1(RunValidator);
    std::thread thread2(RunClient);

    // Send Heartbeat and finish startup process
    uint64_t nonce = registration_message.base().nonce() + 1;
    send_heartbeat(nonce);
    initial_archive();
    logging::print("Sending Heartbeat to Zera Network... 10 seconds", false);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    ValidatorNetworkClient::StartSyncBlockchain(true);
    logging::print("Heartbeat Successful! Joining Zera Network.", false);

    block_process::start_block_process();

    return 0;
}
