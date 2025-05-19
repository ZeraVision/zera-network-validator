#include "validator_api_service.h"

grpc::Status APIImpl::RecieveRequestNonce(grpc::ServerContext *context, const zera_api::NonceRequest *request, zera_api::NonceResponse *response)
{

    //Get the client's IP address
    std::string peer_info = context->peer();
    std::string client_ip;

    // Extract the IP address from the peer info
    size_t pos = peer_info.find(":");
    if (pos != std::string::npos)
    {
        client_ip = peer_info.substr(0, pos); // Extract everything before the first colon
    }
    else
    {
        client_ip = peer_info; // Fallback if no colon is found
    }

    if (!rate_limiter.canProceed(client_ip))
    {
        std::cerr << "Rate limit exceeded for IP: " << client_ip << std::endl;
        return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Rate limit exceeded");
    }

    rate_limiter.processUpdate(client_ip, false);
    
    std::string wallet;

    if (request->encoded())
    {
        auto wallet_vec = base58_decode(request->wallet_address());
        wallet.assign(wallet_vec.begin(), wallet_vec.end());

    }
    else
    {
        wallet.assign(request->wallet_address().begin(), request->wallet_address().end());
    }

    std::string nonce_data;

    uint64_t nonce;

    if (db_wallet_nonce::get_single(wallet, nonce_data))
    {
        try
        {
            nonce = std::stoull(nonce_data);
        }
        catch (std::exception &e)
        {
            return grpc::Status(grpc::StatusCode::CANCELLED, "Failed to parse nonce. (this should never happen)");
        }
    }
    else
    {
        nonce = 0;
    }

    response->set_nonce(nonce);

    return grpc::Status::OK;
}