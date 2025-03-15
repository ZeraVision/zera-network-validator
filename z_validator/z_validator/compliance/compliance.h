#include "zera_status.h"

class compliance{
    public:
    static bool check_compliance(const std::string &wallet_address, const zera_txn::InstrumentContract &contract);
};