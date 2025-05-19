#pragma once

#include "txn.pb.h"
#include "rocksdb/write_batch.h"

#include "db_base.h"


void restricted_symbols()
{
    zera_validator::RestrictedSymbols restricted_symbols;
    restricted_symbols.add_symbols("ZRA");
    restricted_symbols.add_symbols("ACE");
    restricted_symbols.add_symbols("ZIP");
    restricted_symbols.add_symbols("TREASURY");
    db_foundation::store_single(RESTRICTED_SYMBOLS, restricted_symbols.SerializeAsString());
}