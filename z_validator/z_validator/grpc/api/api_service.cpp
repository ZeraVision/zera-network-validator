#include "validator_api_service.h"

RateLimiter APIImpl::rate_limiter;

grpc::Status APIImpl::Balance(grpc::ServerContext *context, const zera_api::BalanceRequest *request, zera_api::BalanceResponse *response)
{
    return RecieveRequestBalance(context, request, response);
}
grpc::Status APIImpl::Nonce(grpc::ServerContext *context, const zera_api::NonceRequest *request, zera_api::NonceResponse *response)
{
    return RecieveRequestNonce(context, request, response);
}
grpc::Status APIImpl::ContractFee(grpc::ServerContext *context, const zera_api::ContractFeeRequest *request, zera_api::ContractFeeResponse *response)
{
    return RecieveRequestContractFee(context, request, response);
}
grpc::Status APIImpl::BaseFee(grpc::ServerContext *context, const zera_api::BaseFeeRequest *request, zera_api::BaseFeeResponse *response)
{
    return RecieveRequestBaseFee(context, request, response);
}
grpc::Status APIImpl::ACETokens(grpc::ServerContext *context, const google::protobuf::Empty *request, zera_api::ACETokensResponse *response)
{
    return RecieveRequestACETokens(context, request, response);
}
grpc::Status APIImpl::Items(grpc::ServerContext *context, const zera_api::ItemRequest *request, zera_api::ItemResponse *response)
{
    return RecieveRequestItems(context, request, response);
}
grpc::Status APIImpl::Denomination(grpc::ServerContext *context, const zera_api::DenominationRequest *request, zera_api::DenominationResponse *response)
{
    return RecieveRequestDenomination(context, request, response);
}
grpc::Status APIImpl::Database(grpc::ServerContext *context, const zera_api::DatabaseRequest *request, zera_api::DatabaseResponse *response) 
{
    return RecieveRequestDatabase(context, request, response);
}

