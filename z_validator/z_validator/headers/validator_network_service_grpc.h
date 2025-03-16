#pragma once

// Standard Library
#include <random>
#include <thread>

// Third-party Libraries
#include <google/protobuf/empty.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <grpcpp/grpcpp.h>

// Project Headers
#include "db_base.h"
// #include "db_validators.h"
#include "txn.pb.h"
#include "validator.pb.h"
#include "validator.grpc.pb.h"
#include "verify_process_txn.h"
#include "zera_status.h"
#include "validator_network_client.h"
#include "../block_process/block_process.h"
#include "../temp_data/temp_data.h"
#include "threadpool.h"
#include "../logging/logging.h"

using google::protobuf::Empty;
using google::protobuf::Timestamp;
using zera_txn::InstrumentContract;
using zera_txn::MintTXN;
using zera_txn::ValidatorRegistration;
using zera_validator::Block;
using zera_validator::BlockBatch;
using zera_validator::BlockSync;
using zera_validator::ValidatorSync;
using zera_validator::ValidatorSyncRequest;

class ValidatorServiceImpl final : public zera_validator::ValidatorService::Service
{
public:
    // streams
    grpc::Status StreamBroadcast(grpc::ServerContext *context, grpc::ServerReader<zera_validator::DataChunk> *reader, google::protobuf::Empty *response) override;
    grpc::Status SyncBlockchain(grpc::ServerContext *context, const BlockSync *request, grpc::ServerWriter<zera_validator::DataChunk> *writer) override;
    grpc::Status StreamBlockAttestation(grpc::ServerContext *context, grpc::ServerReaderWriter<zera_validator::DataChunk, zera_validator::DataChunk> *stream) override;

    grpc::Status Broadcast(grpc::ServerContext *context, const Block *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorRegistration(grpc::ServerContext *context, const zera_txn::ValidatorRegistration *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorMint(grpc::ServerContext *context, const MintTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorNFT(grpc::ServerContext *context, const zera_txn::NFTTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorItemMint(grpc::ServerContext *context, const zera_txn::ItemizedMintTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorContract(grpc::ServerContext *context, const InstrumentContract *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorGovernProposal(grpc::ServerContext *context, const zera_txn::GovernanceProposal *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorGovernVote(grpc::ServerContext *context, const zera_txn::GovernanceVote *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorSmartContract(grpc::ServerContext *context, const zera_txn::SmartContractTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorSmartContractExecute(grpc::ServerContext *context, const zera_txn::SmartContractExecuteTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorCurrencyEquiv(grpc::ServerContext *context, const zera_txn::SelfCurrencyEquiv *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorAuthCurrencyEquiv(grpc::ServerContext *context, const zera_txn::AuthorizedCurrencyEquiv *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorExpenseRatio(grpc::ServerContext *context, const zera_txn::ExpenseRatioTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorContractUpdate(grpc::ServerContext *context, const zera_txn::ContractUpdateTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorHeartbeat(grpc::ServerContext *context, const zera_txn::ValidatorHeartbeat *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorFoundation(grpc::ServerContext *context, const zera_txn::FoundationTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorDelegatedVoting(grpc::ServerContext *context, const zera_txn::DelegatedTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorQuash(grpc::ServerContext *context, const zera_txn::QuashTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorRevoke(grpc::ServerContext *context, const zera_txn::RevokeTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorFastQuorum(grpc::ServerContext *context, const zera_txn::FastQuorumTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorCompliance(grpc::ServerContext *context, const zera_txn::ComplianceTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorBurnSBT(grpc::ServerContext *context, const zera_txn::BurnSBTTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorCoin(grpc::ServerContext *context, const zera_txn::CoinTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ValidatorSmartContractInstantiate(grpc::ServerContext *context, const zera_txn::SmartContractInstantiateTXN *request, google::protobuf::Empty *response) override;

    grpc::Status IndexerVoting(grpc::ServerContext *context, const zera_validator::IndexerVotingRequest *request, zera_validator::IndexerVotingResponse *response) override;
    grpc::Status Nonce(grpc::ServerContext *context, const zera_validator::NonceRequest *request, zera_validator::NonceResponse *response) override;
    grpc::Status Balance(grpc::ServerContext *context, const zera_validator::BalanceRequest *request, zera_validator::BalanceResponse *response) override;

    void StartService()
    {
        grpc::ServerBuilder builder;
        builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        server->Wait();
    }

    static void chunkData(const std::string &data, std::vector<zera_validator::DataChunk> *responses)
    {
        size_t dataSize = data.size();
        int x = 0;
        for (size_t i = 0; i < dataSize; i += CHUNK_SIZE)
        {
            zera_validator::DataChunk chunk;
            chunk.set_chunk_data(data.substr(i, std::min(CHUNK_SIZE, dataSize - i)));
            chunk.set_chunk_number(x);
            responses->push_back(chunk);
            x++;
        }
        if (!responses->empty())
        {
            responses->at(0).set_total_chunks(static_cast<int>(responses->size()));
        }
    }

private:
    static void ProcessBroadcastAsync(const Block *request);
    static void ProcessValidatorRegistrationAsync(const zera_txn::ValidatorRegistration *request);
    static void ProcessBlockAttestationAsync(const zera_validator::BlockAttestation *request, const zera_validator::BlockAttestationResponse *response);

    template <typename TXType>
    grpc::Status RecieveRequest(grpc::ServerContext *context, const TXType *request, google::protobuf::Empty *response)
    {
        // Start asynchronous processing of the request
        TXType *txn = new TXType();
        txn->CopyFrom(*request);
        if (recieved_txn_tracker::check_txn(txn->base().hash()))
        {
            logging::print("TXN already recieved");
            delete txn;
            return grpc::Status::CANCELLED;
        }


        recieved_txn_tracker::add_txn(txn->base().hash());

        ThreadPool &pool = ThreadPool::getInstance();

        try
        {
            // Enqueue the task into the thread pool
            pool.enqueueTask([txn](){ 
                ProcessRequest<TXType>(txn);
                delete txn;
            });
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to enqueue task: " << e.what() << std::endl;
        }

        return grpc::Status::OK;
    }

    template <typename TXType>
    static void ProcessRequest(const TXType *request)
    {

        ZeraStatus status = verify_txns::verify_txn(request);
        std::string memo = "";

        if (request->base().has_memo())
        {
            memo = request->base().memo();
        }

        if (!status.ok())
        {
            status.prepend_message("validator_network_service_grpc.h: ProcessRequestAsync: " + memo);

            if (status.code() != ZeraStatus::Code::DUPLICATE_TXN_ERROR)
            {
                logging::print(status.read_status());
            }

            return;
        }

        zera_txn::TRANSACTION_TYPE txn_type;
        status = verify_txns::store_txn(request, txn_type);

        // if txn was stored start gossip
        if (!status.ok())
        {
            status.prepend_message("validator_network_service_grpc.h: ProcessRequestAsync: " + memo);
            logging::print(status.read_status());
            return;
        }

        TXType *txn = new TXType();
        txn->CopyFrom(*request);

        ThreadPool &pool = ThreadPool::getInstance();

        // Enqueue the task into the thread pool
        pool.enqueueTask([txn, txn_type]()
                         { 
            pre_process::process_txn(txn, txn_type); 
            delete txn; });
    }
};
