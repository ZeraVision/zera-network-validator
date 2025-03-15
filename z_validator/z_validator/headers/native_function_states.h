#pragma once
#include <wasmedge/wasmedge.h>

WasmEdge_Result StoreState(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt,
                           const WasmEdge_Value *In, WasmEdge_Value *Out);

WasmEdge_Result RetrieveState(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt,
                              const WasmEdge_Value *In, WasmEdge_Value *Out);

WasmEdge_Result ClearState(void *Data, const WasmEdge_CallingFrameContext *CallFrameCxt,
                              const WasmEdge_Value *In, WasmEdge_Value *Out);