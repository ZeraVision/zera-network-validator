#include "validator_api_service.h"

grpc::Status APIImpl::RecieveRequestACETokens(grpc::ServerContext *context, const google::protobuf::Empty *request, zera_api::ACETokensResponse *response)
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
    
    std::vector<std::string> keys;
    std::vector<std::string> values;
    db_currency_equiv::get_all_data(keys, values);

    for(auto key : keys)
    {
        zera_validator::CurrencyRate cur_rate;
        cur_rate.ParseFromString(values.at(0));
        auto *token = response->add_tokens();

        token->set_contract_id(key);
        token->set_rate(cur_rate.rate());
    }        
    
    return grpc::Status::OK;
}