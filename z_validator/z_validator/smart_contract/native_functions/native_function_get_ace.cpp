#include "native_function_get_ace.h"
#include "smart_contract_service.h"
#include "db_base.h"
#include "smart_contract_sender_data.h"

WasmEdge_Result GetACEData(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt, const WasmEdge_Value *In, WasmEdge_Value *Out)
{
    /*
     * Params: {i32, i32, i32}
     * Returns: {i32}
     */
    zera_txn::CurrencyEquiv cur;
    uint32_t ContractPointer = WasmEdge_ValueGetI32(In[0]);
    uint32_t ContractSize = WasmEdge_ValueGetI32(In[1]);
    uint32_t TargetPointer = WasmEdge_ValueGetI32(In[2]);

    std::vector<unsigned char> ContractKey(ContractSize);

    WasmEdge_MemoryInstanceContext *MemCxt = WasmEdge_CallingFrameGetMemoryInstance(CallFrameCxt, 0);
    // read data
    WasmEdge_Result Res = WasmEdge_MemoryInstanceGetData(MemCxt, ContractKey.data(), ContractPointer, ContractSize);
    if (WasmEdge_ResultOK(Res))
    {
        // retrieve Value by ContractID
        //
        std::string contract_id(reinterpret_cast<char *>(ContractKey.data()), ContractSize);

        std::string raw_data;
        db_currency_equiv::get_single(contract_id, raw_data);
        zera_validator::CurrencyRate rate;
        rate.ParseFromString(raw_data);

        std::string qualified = rate.qualified() ? "true" : "false";

        std::string rate_str = rate.rate();

        std::string return_data = qualified + "," + rate_str;

        const char *val = return_data.c_str();
        const size_t len = return_data.length();
        WasmEdge_MemoryInstanceSetData(MemCxt, (unsigned char *)val, TargetPointer, len);
        Out[0] = WasmEdge_ValueGenI32(len);

        return WasmEdge_Result_Success;
    }
    else
    {
        return Res;
    }
}