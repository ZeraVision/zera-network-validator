#include "validator_api_service.h"

#include "db_base.h"


grpc::Status APIImpl::RecieveRequestDatabase(grpc::ServerContext *context, const zera_api::DatabaseRequest *request, zera_api::DatabaseResponse *response)
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
    
    std::string data = "";
    switch(request->type())
    {
        case zera_api::DATABASE_TYPE::CONTRACTS:
            {
                db_contracts::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::HASH_INDEX:
            {
                db_hash_index::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::CONTRACT_SUPPLY:
            {
                db_contract_supply::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::SMART_CONTRACTS:
            {
                db_smart_contracts::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::VALIDATORS:
            {
                db_validators::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::BLOCKS:
            {
                db_blocks::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::HEADERS:
            {
                db_headers::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::TRANSACTIONS:
            {
                db_transactions::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::CONTRACT_ITEMS:
            {
                db_contract_items::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::VALIDATOR_UNBONDING:
            {
                db_validator_unbond::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::PROPOSAL_LEDGER:
            {
                db_proposal_ledger::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::PROPOSALS:
            {
                db_proposals::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::CURRENCY_EQUIVALENTS:
            {
                db_currency_equiv::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::EXPENSE_RATIO:
            {
                db_expense_ratio::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::ATTESTATION:
            {
                db_attestation::get_single(request->key(), data);
                break;
            }
            case zera_api::DATABASE_TYPE::CONFIRMED_BLOCKS:
            {
                db_confirmed_blocks::get_single(request->key(), data);
                break;
            }
        default:
            return grpc::Status(grpc::NOT_FOUND, "Invalid Database Type");
    }


    if(data == "")
    {
        return grpc::Status(grpc::NOT_FOUND, "No data found for this key");
    }

    response->set_value(data);

    return grpc::Status::OK;
}