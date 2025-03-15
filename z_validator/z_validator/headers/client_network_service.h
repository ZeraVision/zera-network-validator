#ifndef CLIENT_NETWORK_SERVICE_H
#define CLIENT_NETWORK_SERVICE_H

// Standard Library
#include <random>
#include <thread>

// Third-party Libraries
#include <google/protobuf/empty.pb.h>
#include <grpcpp/grpcpp.h>

// Project Headers
#include "txn.pb.h"
#include "txn.grpc.pb.h"
#include "validator_network_client.h"
#include "verify_process_txn.h"
#include "zera_status.h"
#include "../block_process/block_process.h"
#include "threadpool.h"
#include "../logging/logging.h"

class ClientNetworkServiceImpl final : public zera_txn::TXNService::Service
{
public:
    grpc::Status Coin(grpc::ServerContext *context, const zera_txn::CoinTXN *request, google::protobuf::Empty *response) override;
    grpc::Status Mint(grpc::ServerContext *context, const zera_txn::MintTXN *request, google::protobuf::Empty *response) override;
    grpc::Status NFT(grpc::ServerContext *context, const zera_txn::NFTTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ItemMint(grpc::ServerContext *context, const zera_txn::ItemizedMintTXN *request, google::protobuf::Empty *response) override;
    grpc::Status Contract(grpc::ServerContext *context, const zera_txn::InstrumentContract *request, google::protobuf::Empty *response) override;
    grpc::Status GovernProposal(grpc::ServerContext *context, const zera_txn::GovernanceProposal *request, google::protobuf::Empty *response) override;
    grpc::Status GovernVote(grpc::ServerContext *context, const zera_txn::GovernanceVote *request, google::protobuf::Empty *response) override;
    grpc::Status SmartContract(grpc::ServerContext *context, const zera_txn::SmartContractTXN *request, google::protobuf::Empty *response) override;
    grpc::Status SmartContractExecute(grpc::ServerContext *context, const zera_txn::SmartContractExecuteTXN *request, google::protobuf::Empty *response) override;
    grpc::Status CurrencyEquiv(grpc::ServerContext *context, const zera_txn::SelfCurrencyEquiv *request, google::protobuf::Empty *response) override;
    grpc::Status AuthCurrencyEquiv(grpc::ServerContext *context, const zera_txn::AuthorizedCurrencyEquiv *request, google::protobuf::Empty *response) override;
    grpc::Status ExpenseRatio(grpc::ServerContext *context, const zera_txn::ExpenseRatioTXN *request, google::protobuf::Empty *response) override;
    grpc::Status ContractUpdate(grpc::ServerContext *context, const zera_txn::ContractUpdateTXN *request, google::protobuf::Empty *response) override;
    grpc::Status Foundation(grpc::ServerContext *context, const zera_txn::FoundationTXN *request, google::protobuf::Empty *response) override;
    grpc::Status DelegatedVoting(grpc::ServerContext *context, const zera_txn::DelegatedTXN *request, google::protobuf::Empty *response) override;
    grpc::Status Quash(grpc::ServerContext *context, const zera_txn::QuashTXN *request, google::protobuf::Empty *response) override;
    grpc::Status Revoke(grpc::ServerContext *context, const zera_txn::RevokeTXN *request, google::protobuf::Empty *response) override;
    grpc::Status FastQuorum(grpc::ServerContext *context, const zera_txn::FastQuorumTXN *request, google::protobuf::Empty *response) override;
    grpc::Status Compliance(grpc::ServerContext *context, const zera_txn::ComplianceTXN *request, google::protobuf::Empty *response) override;
    grpc::Status BurnSBT(grpc::ServerContext *context, const zera_txn::BurnSBTTXN *request, google::protobuf::Empty *response) override;
    grpc::Status SmartContractInstantiate(grpc::ServerContext *context, const zera_txn::SmartContractInstantiateTXN *request, google::protobuf::Empty *response) override;

    void StartService()
    {
        grpc::ServerBuilder builder;
        builder.AddListeningPort("0.0.0.0:50052", grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        server->Wait();
    }

private:
    template <typename TXType>
    grpc::Status RecieveRequest(grpc::ServerContext *context, const TXType *request, google::protobuf::Empty *response)
    {
        TXType *txn = new TXType();
        txn->CopyFrom(*request);

        ThreadPool &pool = ThreadPool::getInstance();

        try
        {
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

        std::string memo = request->base().memo();
        if (request->base().has_memo())
        {
            memo = request->base().memo();
        }

        if (!status.ok())
        {
            status.prepend_message("client_network_service: ProcessRequest: " + memo);

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
            status.prepend_message("client_network_service: ProcessRequest: " + memo);
            logging::print(status.read_status());
            return;
        }

        TXType *txn = new TXType();
        txn->CopyFrom(*request);
        

        ThreadPool &pool = ThreadPool::getInstance();

        try
        {
            // Enqueue the task instead of creating a new thread
            pool.enqueueTask([txn, txn_type](){ 
                pre_process::process_txn(txn, txn_type); 
                delete txn;
            });
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to enqueue task: " << e.what() << std::endl;
        }


        logging::print(memo, "client transaction verified!");
    }
};

#endif
