// Standard library headers
#include <string>
#include <iostream>

// Third-party library headers
#include "validator.pb.h"
#include "validator_network_service_grpc.h"

// Project-specific headers (from the current directory)
#include "db_base.h"

grpc::Status ValidatorServiceImpl::Nonce(grpc::ServerContext *context, const zera_validator::NonceRequest *request, zera_validator::NonceResponse *response)
{
    std::string nonce_data;

    if(!db_wallet_nonce::get_single(request->wallet_address(), nonce_data)){
        return grpc::Status(grpc::StatusCode::CANCELLED, "Wallet address does not exist.");
    }

    uint64_t nonce;

    try
    {
        nonce = std::stoull(nonce_data);
    } 
    catch (std::exception& e)
    {
        return grpc::Status(grpc::StatusCode::CANCELLED, "Failed to parse nonce. (this should never happen)");
    }

    response->set_nonce(nonce);

    return grpc::Status::OK;
}