#include "native_function_txns.h"
#include "smart_contract_service.h"
#include "db_base.h"
#include "hashing.h"
#include "../../temp_data/temp_data.h"
#include "wallets.h"
#include "proposer.h"
#include "zera_status.h"
#include "../../block_process/block_process.h"
#include "utils.h"
#include "smart_contract_sender_data.h"
#include "fees.h"

namespace
{
    std::vector<std::string> getWords(std::string s, std::string delim)
    {
        std::vector<std::string> tokens;
        size_t start = 0;
        size_t end = s.find(delim);
        while (end != std::string::npos)
        {
            tokens.push_back(s.substr(start, end - start));
            start = end + delim.length();
            end = s.find(delim, start);
        }
        tokens.push_back(s.substr(start));
        return tokens;
    }

    void calc_fee(zera_txn::AuthorizedCurrencyEquiv *txn)
    {
        uint256_t txn_fee_amount;

        uint256_t equiv;
        zera_fees::get_cur_equiv("$ZRA+0000", equiv);
        zera_txn::InstrumentContract fee_contract;
        block_process::get_contract("$ZRA+0000", fee_contract);

        uint256_t fee_per_byte = get_txn_fee(zera_txn::TRANSACTION_TYPE::AUTHORIZED_CURRENCY_EQUIV_TYPE);
        int byte_size = txn->ByteSize() + 64;
        std::string denomination_str = fee_contract.coin_denomination().amount();

        uint256_t fee = fee_per_byte * byte_size;
        uint256_t denomination(denomination_str);
        txn_fee_amount = (fee * denomination) / equiv;

        uint256_t key_fee = get_key_fee(txn->base().public_key());
        txn_fee_amount += (key_fee * denomination) / equiv;

        txn->mutable_base()->set_fee_amount(txn_fee_amount.str());
    }

    void set_base(zera_txn::BaseTXN *base, SenderDataType &sender)
    {
        std::string sc_auth = "sc_" + sender.smart_contract_instance;
        base->mutable_public_key()->set_smart_contract_auth(sc_auth);
        std::string wallet = sender.smart_contract_wallet;
        uint64_t nonce = 0;
        nonce_tracker::get_nonce(wallet, nonce);
        nonce = nonce + 1;

        base->set_fee_amount("1000000000000");
        base->set_nonce(nonce);
        base->set_fee_id("$ZRA+0000");
        base->set_safe_send(false);
    }
    std::string process_txn(SenderDataType &sender, const zera_txn::AuthorizedCurrencyEquiv &txn)
    {
        std::string value;
        db_smart_contracts::get_single(sender.block_txns_key, value);
        zera_txn::TXNS block_txns;
        block_txns.ParseFromString(value);

        std::string fee_address = sender.fee_address;
        ZeraStatus status = proposing::unpack_process_wrapper(&txn, &block_txns, zera_txn::TRANSACTION_TYPE::AUTHORIZED_CURRENCY_EQUIV_TYPE, false, fee_address, true);
        if (status.ok())
        {
            sender.txn_hashes.push_back(txn.base().hash());
            block_txns.add_auth_cur_equivs()->CopyFrom(txn);
            txn_hash_tracker::add_sc_hash(txn.base().hash());
            uint64_t nonce = txn.base().nonce();
            nonce_tracker::store_sc_nonce(sender.smart_contract_wallet, nonce);
        }
        db_smart_contracts::store_single(sender.block_txns_key, block_txns.SerializeAsString());

        return zera_txn::TXN_STATUS_Name(status.txn_status());
    }

    std::string create_authorized_currency_equiv(SenderDataType &sender, const std::vector<std::string> &contract_ids, const std::vector<std::string> &rates, const std::vector<std::string> &authorized, const std::vector<std::string> &max_stakes)
    {
        if (contract_ids.size() != rates.size() || contract_ids.size() != authorized.size() || contract_ids.size() != max_stakes.size())
        {
            return "FAILED: Delegate wallet not found";
        }

        zera_txn::AuthorizedCurrencyEquiv txn;

        zera_txn::BaseTXN *base = txn.mutable_base();

        set_base(base, sender);

        for (int i = 0; i < contract_ids.size(); i++)
        {
            zera_txn::CurrencyEquiv *cur = txn.add_cur_equiv();
            cur->set_contract_id(contract_ids[i]);
            cur->set_rate(rates[i]);
            bool authorize = authorized[i] == "true" ? true : false;
            cur->set_authorized(authorize);
            cur->set_max_stake(max_stakes[i]);
        }

        calc_fee(&txn);

        auto hash_vec = Hashing::sha256_hash(txn.SerializeAsString());
        std::string hash(hash_vec.begin(), hash_vec.end());
        base->set_hash(hash);

        return process_txn(sender, txn);
    }

}
WasmEdge_Result AuthorizedCurrencyEquiv(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt,
                                        const WasmEdge_Value *In, WasmEdge_Value *Out)
{
    SenderDataType sender = *(SenderDataType *)Data;

    logging::print("[AuthorizedCurrencyEquiv] START");
    uint32_t ContractPointer = WasmEdge_ValueGetI32(In[0]);
    uint32_t ContractSize = WasmEdge_ValueGetI32(In[1]);

    uint32_t RatePointer = WasmEdge_ValueGetI32(In[2]);
    uint32_t RateSize = WasmEdge_ValueGetI32(In[3]);

    uint32_t AuthorizedPointer = WasmEdge_ValueGetI32(In[4]);
    uint32_t AuthorizedSize = WasmEdge_ValueGetI32(In[5]);

    uint32_t MaxStakePointer = WasmEdge_ValueGetI32(In[6]);
    uint32_t MaxStakeSize = WasmEdge_ValueGetI32(In[7]);

    uint32_t TargetPointer = WasmEdge_ValueGetI32(In[8]);

    std::vector<unsigned char> ContractKey(ContractSize);
    std::vector<unsigned char> RateKey(RateSize);
    std::vector<unsigned char> AuthorizedKey(AuthorizedSize);
    std::vector<unsigned char> MaxStakeKey(MaxStakeSize);

    WasmEdge_MemoryInstanceContext *MemCxt = WasmEdge_CallingFrameGetMemoryInstance(CallFrameCxt, 0);

    logging::print("[AuthorizedCurrencyEquiv] Res");
    WasmEdge_Result Res = WasmEdge_MemoryInstanceGetData(MemCxt, ContractKey.data(), ContractPointer, ContractSize);

    std::vector<std::string> contract_ids;
    std::vector<std::string> rates;
    std::vector<std::string> authorized;
    std::vector<std::string> max_stakes;

    if (WasmEdge_ResultOK(Res))
    {
        std::string contract_ids_temp(reinterpret_cast<char *>(ContractKey.data()), ContractSize);
        contract_ids = getWords(contract_ids_temp, "**");
    }
    else
    {
        return Res;
    }

    logging::print("[AuthorizedCurrencyEquiv] Res2");
    WasmEdge_Result Res2 = WasmEdge_MemoryInstanceGetData(MemCxt, RateKey.data(), RatePointer, RateSize);
    if (WasmEdge_ResultOK(Res2))
    {
        std::string rates_temp(reinterpret_cast<char *>(RateKey.data()), RateSize);
        rates = getWords(rates_temp, "**");
    }
    else
    {
        return Res2;
    }

    logging::print("[AuthorizedCurrencyEquiv] Res3");
    WasmEdge_Result Res3 = WasmEdge_MemoryInstanceGetData(MemCxt, AuthorizedKey.data(), AuthorizedPointer, AuthorizedSize);
    if (WasmEdge_ResultOK(Res3))
    {
        std::string authorized_temp(reinterpret_cast<char *>(AuthorizedKey.data()), AuthorizedSize);
        authorized = getWords(authorized_temp, "**");
    }
    else
    {
        return Res3;
    }

    logging::print("[AuthorizedCurrencyEquiv] Res4");
    WasmEdge_Result Res4 = WasmEdge_MemoryInstanceGetData(MemCxt, MaxStakeKey.data(), MaxStakePointer, MaxStakeSize);
    if (WasmEdge_ResultOK(Res4))
    {
        std::string max_temp(reinterpret_cast<char *>(MaxStakeKey.data()), MaxStakeSize);
        max_stakes = getWords(max_temp, "**");
    }
    else
    {
        return Res4;
    }

    std::string status = create_authorized_currency_equiv(sender, contract_ids, rates, authorized, max_stakes);

    const char *val = status.c_str();
    const size_t len = status.length();

    auto fee_address = sender.fee_address;

    WasmEdge_MemoryInstanceSetData(MemCxt, (unsigned char *)val, TargetPointer, len);
    Out[0] = WasmEdge_ValueGenI32(len);
    return WasmEdge_Result_Success;
}