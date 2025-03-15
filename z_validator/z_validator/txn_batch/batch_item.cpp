#include "txn_batch.h"

#include "db_base.h"
#include "../temp_data/temp_data.h"
#include "wallet.pb.h"
#include "wallets.h"
#include "../governance/time_calc.h"

void txn_batch::batch_item_mint(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed, leveldb::WriteBatch &item_batch)
{
    for (auto item : txns.item_mint_txns())
    {
        if (txn_passed.at(item.base().hash()))
        {
            zera_validator::NFT nft;
            nft.set_holder_address(item.recipient_address());
            nft.set_item_id(item.item_id());
            nft.set_contract_id(item.contract_id());
            if (item.has_contract_fees())
            {
                nft.mutable_contract_fees()->CopyFrom(item.contract_fees());
            }
            if (item.has_voting_weight())
            {
                nft.set_voting_weight(item.voting_weight());
            }
            
            item_batch.Put(item.item_id() + item.contract_id(), nft.SerializeAsString());
        }
    }
}
void txn_batch::batch_nft_transfer(const zera_txn::TXNS &txns, const std::map<std::string, bool> &txn_passed, leveldb::WriteBatch &item_batch)
{
    for (auto item : txns.nft_txns())
    {
        if (txn_passed.at(item.base().hash()))
        {
            std::string nft_key = item.item_id() + item.contract_id();
            std::string nft_data;
            zera_validator::NFT nft;
            if (!db_contract_items::get_single(nft_key, nft_data) || nft.ParseFromString(nft_data))
            {
                break;
            }
            nft.set_holder_address(item.recipient_address());
            item_batch.Put(nft_key, nft.SerializeAsString());
        }
    }
}
