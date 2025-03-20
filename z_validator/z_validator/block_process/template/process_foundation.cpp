#include "../block_process.h"
#include <regex>

#include "../../temp_data/temp_data.h"
#include "utils.h"

template <>
ZeraStatus block_process::check_parameters<zera_txn::FoundationTXN>(const zera_txn::FoundationTXN *txn, zera_txn::TXNStatusFees &status_fees, const std::string &fee_address)
{

    // check the the public key to see if its authorized to mint
    // get the contract aswell
    // ZeraStatus status = check_restricted("$ZFT+0000", txn->base().public_key());

    for (auto multi : txn->byte_multiplier())
    {
        if (!is_valid_uint256(multi.multiplier()))
        {
            return ZeraStatus(ZeraStatus::Code::TXN_FAILED, "process_mint.cpp: process_txn: Invalid uint256", zera_txn::TXN_STATUS::INVALID_UINT256);
        }
    }

    return ZeraStatus();
}
