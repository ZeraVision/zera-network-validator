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

#include <rocksdb/write_batch.h>
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
#include "validator_api_service.h"
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
#include <random> // For random number generation

void configure_rate_limiters()
{
    APIImpl::RateLimitConfig(ValidatorConfig::get_whitelist());
    ValidatorServiceImpl::RateLimitConfig();
    ClientNetworkServiceImpl::RateLimitConfig();
}

void deregister_validator()
{
    std::string server_address = ValidatorConfig::get_seed_validators().at(0);
    zera_validator::NonceResponse response = ValidatorNetworkClient::GetNonce(server_address);
    uint64_t nonce = response.nonce() + 1;

    KeyPair kp = ValidatorConfig::get_key_pair();

    zera_txn::ValidatorRegistration reggy;
    reggy.set_register_(false);
    zera_txn::Validator *val = reggy.mutable_validator();
    val->mutable_public_key()->set_single(ValidatorConfig::get_public_key());

    zera_txn::BaseTXN *base = reggy.mutable_base();
    base->mutable_public_key()->set_single(ValidatorConfig::get_public_key());
    base->set_fee_id("$ZRA+0000");
    base->set_fee_amount("10000000000");
    base->set_nonce(nonce);

    google::protobuf::Timestamp now_ts = google::protobuf::util::TimeUtil::GetCurrentTime();

    google::protobuf::Timestamp *ts = base->mutable_timestamp();
    ts->set_seconds(now_ts.seconds());

    signatures::sign_txns(&reggy, kp);
    std::vector<uint8_t> hash = Hashing::sha256_hash(reggy.SerializeAsString());
    std::string hash_str(hash.begin(), hash.end());
    base->set_hash(hash_str);

    ValidatorNetworkClient::StartRegisterSeeds(&reggy);
}

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
    if (ValidatorConfig::get_host() == "" || ValidatorConfig::get_client_port() == "" || ValidatorConfig::get_validator_port() == "" || ValidatorConfig::get_seed_validators().size() == 0 || ValidatorConfig::get_private_key() == "" || ValidatorConfig::get_public_key() == "" || ValidatorConfig::get_fee_address_string() == "" || ValidatorConfig::get_register() == "N/A")
    {
        return false;
    }

    return true;
}

void RunAPI()
{
    APIImpl api_service;
    api_service.StartService(ValidatorConfig::get_api_port());
}
void RunValidator()
{
    ValidatorThreadPool::setNumThreads();
    ValidatorServiceImpl validator_service;
    validator_service.StartService(ValidatorConfig::get_validator_port());
}

void RunClient()
{
    ThreadPool::setNumThreads();
    ClientNetworkServiceImpl client_service;
    client_service.StartService(ValidatorConfig::get_client_port());
}

void configure_self(zera_txn::ValidatorRegistration &registration_message)
{
    std::string validator_config = ValidatorConfig::get_block_height();

    if (validator_config != "NONE" && validator_config != "")
    {
        Reorg::restore_database(validator_config);
        ValidatorConfig::set_config();
    }

    ValidatorConfig::generate_keys();
    set_explorer_config();
    zera_txn::Validator validator;

    store_self(&validator);
    // gets registration txn and removes private key from memory
    // now using generated key for signing blocks
    get_validator_registration(&validator, &registration_message);
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
    // 1.
    //  Initialize libsodium
    if (sodium_init() == -1)
    {
        std::cerr << "Error initializing libsodium\n";
        return -1;
    }

    // open all databases
    open_dbs();
    ValidatorConfig::set_config();
    logging::print("ZERA Validator v1.1.6", false);

    if (!check_config())
    {
        logging::print("Configuration is not set correctly. Please check your configuration file.");
        return -1;
    }

    // deregister if requested
    logging::print("Register: ", ValidatorConfig::get_register(), false);

    if (ValidatorConfig::get_register() == "false")
    {
        deregister_validator();
        logging::print("You have been deregistered from the Zera Network. Your wallet will be free in 7 days.");
        return -1;
    }

    // 3. create a validator registration message to apply to the blockchain
    zera_txn::ValidatorRegistration registration_message;
    configure_self(registration_message);
    configure_rate_limiters();

    // 5.
    debug::startup_logs();

    // 8. Send registration message to 10 validators if have seed validator, else create a block with registration message
    bool heartbeat = false;
    bool sync = true;

    if (ValidatorConfig::get_seed_validators().size() > 0)
    {
        logging::print("Validator Registration Message");
        ValidatorNetworkClient::StartRegisterSeeds(&registration_message);
        logging::print("Registering to Zera Network... 10 seconds", false);
        std::this_thread::sleep_for(std::chrono::seconds(10));
        sync = ValidatorNetworkClient::StartSyncBlockchain(true);
        logging::print("Successfull Network sync.", false);

        heartbeat = true;
    }

    if (!sync)
    {
        return -1;
    }

    restricted_symbols();

    zera_validator::BlockHeader last_header;
    std::string last_key;
    db_headers_tag::get_last_data(last_header, last_key);
    std::string last_height = std::to_string(last_header.block_height());

    std::thread thread1(RunValidator);
    std::thread thread2(RunClient);
    std::thread thread3;
    std::thread thread4(ValidatorNetworkClient::GossipThread);

    if (ValidatorConfig::get_api_port() != "0")
    {
        logging::print("Starting API Service on port: " + ValidatorConfig::get_api_port(), false);
        thread3 = std::thread(RunAPI);
    }

    uint64_t nonce = registration_message.base().nonce() + 1;
    send_heartbeat(nonce);
    validator_utils::archive_balances(last_height);
    logging::print("Sending Heartbeat to Zera Network... 10 seconds", false);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    ValidatorNetworkClient::StartSyncBlockchain(true);
    logging::print("Heartbeat Successful! Joining Zera Network.", false);

    block_process::start_block_process();

    return 0;
}