#include "validator_api_service.h"

grpc::Status APIImpl::RecieveRequestBalance(grpc::ServerContext *context, const zera_api::BalanceRequest *request, zera_api::BalanceResponse *response)
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

    if(request->encoded())
    {
        auto wallet_vec = base58_decode(request->wallet_address());
        wallet.assign(wallet_vec.begin(), wallet_vec.end());
    }
    else
    {
        wallet.assign(request->wallet_address().begin(), request->wallet_address().end());
    }

    std::string contract_data;
    if(db_contracts::get_single(request->contract_id(), contract_data))
    {
        zera_txn::InstrumentContract contract;
        contract.ParseFromString(contract_data);
        response->set_denomination(contract.coin_denomination().amount());

        std::string cur_data;

        if(db_currency_equiv::get_single(request->contract_id(), cur_data))
        {
            zera_validator::CurrencyRate cur_rate;
            cur_rate.ParseFromString(cur_data);
            response->set_rate(cur_rate.rate());
        }
        else
        {
            return grpc::Status(grpc::NOT_FOUND, "Currency Equivalence not found (this should not happen)");
        }   
    }
    else
    {
        return grpc::Status(grpc::NOT_FOUND, "Invalid Contract ID");
    }

    std::string balance;

    if(db_wallets::get_single(wallet + request->contract_id(), balance))
    {
        response->set_balance(balance);

        return grpc::Status::OK;
    }
    else
    {
        return grpc::Status(grpc::NOT_FOUND, "Invalid Wallet");
    }

    return grpc::Status::OK;
}