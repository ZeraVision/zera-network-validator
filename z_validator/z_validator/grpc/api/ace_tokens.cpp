#include "validator_api_service.h"

grpc::Status APIImpl::RecieveRequestACETokens(grpc::ServerContext *context, const google::protobuf::Empty *request, zera_api::ACETokensResponse *response)
{
    if (!check_rate_limit(context))
    {
        return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Rate limit exceeded");
    }
    
    std::vector<std::string> keys;
    std::vector<std::string> values;
    db_currency_equiv::get_all_data(keys, values);

    for (auto key : keys)
    {
        zera_validator::CurrencyRate cur_rate;
        cur_rate.ParseFromString(values.at(0));
        auto *token = response->add_tokens();

        token->set_contract_id(key);
        token->set_rate(cur_rate.rate());
    }

    return grpc::Status::OK;
}