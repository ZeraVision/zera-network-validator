#include "block_process.h"
#include "validator_network_client.h"
#include "../temp_data/temp_data.h"
#include "threadpool.h"
#include "utils.h"
#include "../logging/logging.h"
namespace
{
    template <typename TXType>
    void get_nonce(TXType *txn, uint64_t &nonce)
    {
        nonce = txn->base().nonce();
    }

    template <>
    void get_nonce<zera_txn::CoinTXN>(zera_txn::CoinTXN *txn, uint64_t &nonce)
    {
        for (auto input_nonce : txn->auth().nonce())
        {
            nonce += input_nonce;
        }
    }

}
template <typename TXType>
void pre_process::process_txn(TXType *txn, const zera_txn::TRANSACTION_TYPE &txn_type)
{
    ZeraStatus status;
    zera_txn::TXNStatusFees status_fee;
    std::string txn_hash = txn->base().hash();
    zera_txn::TXNWrapper wrapper;
    uint64_t txn_nonce = 0;

    get_nonce(txn, txn_nonce);

    std::string txn_key = get_txn_key(txn_nonce, txn_hash);

    if (!db_block_txns::exist(txn_hash) && !db_transactions::exist(txn_key))
    {
        status = block_process::process_txn(txn, status_fee, txn_type, false, PREPROCESS_PLACEHOLDER);
        if (status.ok())
        {
            if (status_fee.status() != zera_txn::TXN_STATUS::OK)
            {

                logging::print(txn->base().memo(), "prepocessing txn failed!");
                logging::print(zera_txn::TXN_STATUS_Name(status_fee.status()));
            }
            else
            {
                logging::print(txn->base().memo(), "prepocessing txn passed!");
            }

            // add status fees to preprocessed status fees temp data
            status_fee.set_txn_hash(txn_hash);
            status_fee_tracker::add_fee(status_fee);

            // add txn to preprocessed txns
            verify_txns::store_wrapper(txn, wrapper);
            db_processed_txns::store_single(txn_key, wrapper.SerializeAsString());

            // add txn to pending block txns
            db_block_txns::store_single(txn_hash, "1");
            logging::print("txn prepocessed!");

            TXType *txn_copy = new TXType();
            txn_copy->CopyFrom(*txn);

            ValidatorThreadPool &pool = ValidatorThreadPool::getInstance();
            pool.enqueueTask([txn_copy](){ 
                ValidatorNetworkClient::StartGossip(txn_copy);
                delete txn_copy; 
            });

                
        }
        else
        {
           logging::print("Faulty TXN:", status.message());

            if (status.code() == ZeraStatus::Code::NONCE_ERROR)
            {
                verify_txns::store_wrapper(txn, wrapper);
                db_transactions::store_single(txn_key, wrapper.SerializeAsString());
                TXType *txn_copy = new TXType();
                txn_copy->CopyFrom(*txn);

                ValidatorThreadPool &pool = ValidatorThreadPool::getInstance();
                pool.enqueueTask([txn_copy](){ 
                    ValidatorNetworkClient::StartGossip(txn_copy);
                    delete txn_copy; 
                });
            }
        }
    }
    recieved_txn_tracker::remove_txn(txn_hash);
}
template void pre_process::process_txn<zera_txn::GovernanceProposal>(zera_txn::GovernanceProposal *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::GovernanceVote>(zera_txn::GovernanceVote *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::NFTTXN>(zera_txn::NFTTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::MintTXN>(zera_txn::MintTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::ItemizedMintTXN>(zera_txn::ItemizedMintTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::SmartContractTXN>(zera_txn::SmartContractTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::SelfCurrencyEquiv>(zera_txn::SelfCurrencyEquiv *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::AuthorizedCurrencyEquiv>(zera_txn::AuthorizedCurrencyEquiv *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::ContractUpdateTXN>(zera_txn::ContractUpdateTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::FoundationTXN>(zera_txn::FoundationTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::DelegatedTXN>(zera_txn::DelegatedTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::QuashTXN>(zera_txn::QuashTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::FastQuorumTXN>(zera_txn::FastQuorumTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::RevokeTXN>(zera_txn::RevokeTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::ComplianceTXN>(zera_txn::ComplianceTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::BurnSBTTXN>(zera_txn::BurnSBTTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::CoinTXN>(zera_txn::CoinTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::ValidatorHeartbeat>(zera_txn::ValidatorHeartbeat *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::ValidatorRegistration>(zera_txn::ValidatorRegistration *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::SmartContractInstantiateTXN>(zera_txn::SmartContractInstantiateTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type);
template void pre_process::process_txn<zera_txn::InstrumentContract>(zera_txn::InstrumentContract *txn, const zera_txn::TRANSACTION_TYPE &txn_type);

template <>
void pre_process::process_txn<zera_txn::ExpenseRatioTXN>(zera_txn::ExpenseRatioTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type)
{
    std::string value;
    std::string txn_hash = txn->base().hash();
    zera_txn::TXNWrapper wrapper;
    std::string txn_key = get_txn_key(txn->base().nonce(), txn_hash);

    if (!db_block_txns::get_single(txn_hash, value))
    {

        // add txn to preprocessed txns
        verify_txns::store_wrapper(txn, wrapper);
        db_transactions::store_single(txn_key, wrapper.SerializeAsString());

        // add txn to pending block txns

        zera_txn::ExpenseRatioTXN *txn_copy = new zera_txn::ExpenseRatioTXN();
        txn_copy->CopyFrom(*txn);

        ValidatorThreadPool &pool = ValidatorThreadPool::getInstance();
        pool.enqueueTask([txn_copy](){ 
            ValidatorNetworkClient::StartGossip(txn_copy);
            delete txn_copy; 
        });
    }
}

template <>
void pre_process::process_txn<zera_txn::SmartContractExecuteTXN>(zera_txn::SmartContractExecuteTXN *txn, const zera_txn::TRANSACTION_TYPE &txn_type)
{
    std::string value;
    std::string txn_hash = txn->base().hash();
    zera_txn::TXNWrapper wrapper;
    std::string txn_key = get_txn_key(txn->base().nonce(), txn_hash);

    if (!db_block_txns::get_single(txn_hash, value))
    {

        // add txn to preprocessed txns
        verify_txns::store_wrapper(txn, wrapper);
        db_transactions::store_single(txn_key, wrapper.SerializeAsString());

        zera_txn::SmartContractExecuteTXN *txn_copy = new zera_txn::SmartContractExecuteTXN();
        txn_copy->CopyFrom(*txn);

        ValidatorThreadPool &pool = ValidatorThreadPool::getInstance();
        pool.enqueueTask([txn_copy](){ 
            ValidatorNetworkClient::StartGossip(txn_copy);
            delete txn_copy; 
        });
    }
}
