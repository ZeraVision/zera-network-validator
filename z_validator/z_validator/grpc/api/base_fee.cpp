#include "validator_api_service.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/lexical_cast.hpp>

#include "utils.h"

grpc::Status APIImpl::RecieveRequestBaseFee(grpc::ServerContext *context, const zera_api::BaseFeeRequest *request, zera_api::BaseFeeResponse *response)
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
    
    uint256_t byte_fee = get_txn_fee(request->txn_type());

    uint256_t key_fee = get_key_fee(request->public_key());

    response->set_byte_fee(byte_fee.str());
    response->set_key_fee(key_fee.str());

    return grpc::Status::OK;
}
