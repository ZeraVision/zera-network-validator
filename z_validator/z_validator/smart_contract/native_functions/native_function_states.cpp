#include "native_function_states.h"
#include "smart_contract_service.h"
#include "db_base.h"
#include "../../temp_data/temp_data.h"
#include "zera_status.h"
#include "const.h"
#include "../../block_process/block_process.h"
#include "smart_contract_sender_data.h"

namespace
{
  bool storage_fees(const SenderDataType &sender, const uint64_t &storage_size)
  {
    uint256_t storage_fee = STORAGE_FEE * storage_size;
    uint256_t usd_equiv;
    block_process::get_cur_equiv("$ZRA+0000", usd_equiv);
    storage_fee = (storage_fee * 1000000000) / usd_equiv;

    ZeraStatus status = balance_tracker::subtract_txn_balance(sender.fee_smart_contract_wallet, "$ZRA+0000", storage_fee, sender.txn_hash);

    if (!status.ok())
    {
      return false;
    }

    std::string storage_key = "STORAGE_FEE_" + sender.fee_smart_contract_instance;

    std::string fee_data;

    if (db_smart_contracts::get_single(storage_key, fee_data))
    {
      uint256_t fee(fee_data);
      storage_fee += fee;
    }

    db_smart_contracts::store_single(storage_key, storage_fee.str());

    return true;
  }
}
WasmEdge_Result StoreState(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt,
                           const WasmEdge_Value *In, WasmEdge_Value *Out)
{
  /*
   * Params: {i32, i32, i32, i32}
   */

  uint32_t KeyPointer = WasmEdge_ValueGetI32(In[0]);
  uint32_t KeySize = WasmEdge_ValueGetI32(In[1]);
  uint32_t ValuePointer = WasmEdge_ValueGetI32(In[2]);
  uint32_t ValueSize = WasmEdge_ValueGetI32(In[3]);

  std::vector<unsigned char> Key(KeySize);
  std::vector<unsigned char> Value(ValueSize);

  // https://wasmedge.org/docs/embed/c/host_function/#calling-frame-context
  // https://www.secondstate.io/articles/extend-webassembly/
  WasmEdge_MemoryInstanceContext *MemCxt = WasmEdge_CallingFrameGetMemoryInstance(CallFrameCxt, 0);
  // read data
  WasmEdge_Result Res = WasmEdge_MemoryInstanceGetData(MemCxt, Key.data(), KeyPointer, KeySize);
  WasmEdge_Result Res2 = WasmEdge_MemoryInstanceGetData(MemCxt, Value.data(), ValuePointer, ValueSize);
  if (WasmEdge_ResultOK(Res))
  {
    if (WasmEdge_ResultOK(Res2))
    {
      SenderDataType sender = *(SenderDataType *)Data;
      std::string keyString(reinterpret_cast<char *>(Key.data()), KeySize);

      std::string storeKey = sender.current_smart_contract_instance + "_" + keyString;

      std::string storage_data;
      db_smart_contracts::get_single(storeKey, storage_data);

      uint64_t storage_fee = KeySize + ValueSize - storage_data.length();

      if (!storage_fees(sender, storage_fee))
      {
        logging::print("Storage fees check failed for sender: " + storeKey, true);
        return WasmEdge_Result_Terminate;
      }
      // store Key and Value
      std::string valueString(reinterpret_cast<char *>(Value.data()), ValueSize);

      if (!db_sc_temp::exist(storeKey))
      {
        std::string original_data;
        db_smart_contracts::get_single(storeKey, original_data);
        db_sc_temp::store_single(storeKey, original_data);
      }

      db_smart_contracts::store_single(storeKey, valueString);

      int value = 1;
      Out[0] = WasmEdge_ValueGenI32(value);
      return WasmEdge_Result_Success;
    }
    else
    {
      return Res2;
    }
  }
  else
  {
    return Res;
  }
}

WasmEdge_Result RetrieveState(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt,
                              const WasmEdge_Value *In, WasmEdge_Value *Out)
{
  /*
   * Params: {i32, i32, i32}
   * Returns: {i32}
   */

  uint32_t KeyPointer = WasmEdge_ValueGetI32(In[0]);
  uint32_t KeySize = WasmEdge_ValueGetI32(In[1]);
  uint32_t TargetPointer = WasmEdge_ValueGetI32(In[2]);

  std::vector<unsigned char> Key(KeySize);

  WasmEdge_MemoryInstanceContext *MemCxt = WasmEdge_CallingFrameGetMemoryInstance(CallFrameCxt, 0);
  // read data
  WasmEdge_Result Res = WasmEdge_MemoryInstanceGetData(MemCxt, Key.data(), KeyPointer, KeySize);
  if (WasmEdge_ResultOK(Res))
  {
    SenderDataType sender = *(SenderDataType *)Data;

    // retrieve Value by Key
    //
    std::string keyString(reinterpret_cast<char *>(Key.data()), KeySize);

    std::string storeKey = sender.current_smart_contract_instance + "_" + keyString;
    std::string raw_data;
    db_smart_contracts::get_single(storeKey, raw_data);

    const char *val = raw_data.c_str();
    const size_t len = raw_data.length();

    WasmEdge_MemoryInstanceSetData(MemCxt, (unsigned char *)val, TargetPointer, len);
    Out[0] = WasmEdge_ValueGenI32(len);

    return WasmEdge_Result_Success;
  }
  else
  {
    return Res;
  }
}

WasmEdge_Result ClearState(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt,
                           const WasmEdge_Value *In, WasmEdge_Value *Out)
{
  /*
   * Params: {i32, i32, i32, i32}
   */

  uint32_t KeyPointer = WasmEdge_ValueGetI32(In[0]);
  uint32_t KeySize = WasmEdge_ValueGetI32(In[1]);

  std::vector<unsigned char> Key(KeySize);

  // https://wasmedge.org/docs/embed/c/host_function/#calling-frame-context
  // https://www.secondstate.io/articles/extend-webassembly/
  WasmEdge_MemoryInstanceContext *MemCxt = WasmEdge_CallingFrameGetMemoryInstance(CallFrameCxt, 0);
  // read data
  WasmEdge_Result Res = WasmEdge_MemoryInstanceGetData(MemCxt, Key.data(), KeyPointer, KeySize);
  if (WasmEdge_ResultOK(Res))
  {
    SenderDataType sender = *(SenderDataType *)Data;
    // store Key and Value
    std::string keyString(reinterpret_cast<char *>(Key.data()), KeySize);
    std::string storeKey = sender.current_smart_contract_instance + "_" + keyString;

    if (!db_sc_temp::exist(storeKey))
    {
      std::string original_data;
      db_smart_contracts::get_single(storeKey, original_data);
      db_sc_temp::store_single(storeKey, original_data);
    }

    db_smart_contracts::remove_single(storeKey);

    return WasmEdge_Result_Success;
  }
  else
  {
    return Res;
  }
}