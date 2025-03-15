#ifndef _CONST_H_
#define _CONST_H_

constexpr int VERSION = 100000; //version of the validator
//1000000000000000000 1 dollar
//10000000000000000   1 cent
//1 000 000 000 000 000 000 1 dollar
//2000000000000000

//MULTIPLIER VALUES
const long long RESTRICTED_KEY_FEE = 3;                                //3x multiplier on key/hash fees

//FIXED VALUES              
const unsigned long long A_KEY_FEE = 20000000000000000;                          //2 cents
const unsigned long long B_KEY_FEE = 50000000000000000;                          //5 cents
const unsigned long long a_HASH_FEE = 20000000000000000;                         //2 cents
const unsigned long long b_HASH_FEE = 50000000000000000;                         //5 cents
const unsigned long long c_HASH_FEE = 10000000000000000;                         //1 cent
const unsigned long long DELEGATED_VOTING_TXN_FEE = 50000000000000000;           //5 cents
constexpr auto VALIDATOR_HOLDING_MINIMUM = "25000000000000000000000";            //25 000 dollars
const unsigned long long VALIDATOR_MINIMUM_ZERA = 10000000000000;               //10 000 zera
const long ATTESTATION_QUORUM = 51;                                              //51% quorum


//PER BYTE VALUES 
const unsigned long long GAS_FEE = 2500000000000;                                 //.000025 cent   
const unsigned long long COIN_TXN_FEE = 150000000000000;                          //0.015 cents
const unsigned long long STORAGE_FEE = 1000000000000000;                           //0.1 cents

const unsigned long long CONTRACT_TXN_FEE = 860000000000000;                     //.086 cents
const unsigned long long EXPENSE_RATIO_TXN_FEE = 4000000000000000;               //.10 cents
const unsigned long long ITEM_MINT_TXN_FEE = 1000000000000000;                   //0.1 cents
const unsigned long long MINT_TXN_FEE = 1000000000000000;                        //0.1 cents
const unsigned long long NFT_TXN_FEE = 300000000000000;                          //0.03 cents
const unsigned long long PROPOSAL_RESULT_TXN_FEE = 10000000000000000;            //1 cent
const unsigned long long PROPOSAL_TXN_FEE = 5000000000000000;                    //0.5 cent
const unsigned long long SELF_CURRENCY_EQUIV_TXN_FEE = 500000000000000;          //0.05 cent
const unsigned long long AUTHORIZED_CURRENCY_EQUIV_TXN_FEE = 500000000000000;    //0.05 cent
const unsigned long long SMART_CONTRACT_EXECUTE_TXN_FEE = 1500000000000000;      //0.15 cents
const unsigned long long SMART_CONTRACT_DEPLOYMENT_TXN_FEE = 400000000000000;    //.04 cent
const unsigned long long SMART_CONTRACT_INSTANTIATE_TXN_FEE = 10000000000000000; //1 cent
const unsigned long long UPDATE_CONTRACT_TXN_FEE = 75000000000000000;            //7.5 cents
const unsigned long long VOTE_TXN_FEE = 100000000000000;                         //0.01 cents
const unsigned long long VALIDATOR_REGISTRATION_TXN_FEE = 10000000000000000;     //1 cent
const unsigned long long VALIDATOR_HEARTBEAT_TXN_FEE = 50000000000000;           //0.005 cent
const unsigned long long FAST_QUORUM_TXN_FEE = 40000000000000000;                //4 cents
const unsigned long long QUASH_TXN_FEE = 1000000000000000;                       //0.1 cents
const unsigned long long REVOKE_TXN_FEE = 1000000000000000;                      //0.1 cents
const unsigned long long BURN_TXN_FEE = 1000000000000000;                        //0.1 cents
const unsigned long long SAFE_SEND = 100000000000000;                            //0.01 cents
const unsigned long long COMPLIANCE_TXN_FEE = 1000000000000000;                  //0.1 cents
const unsigned long long DELEGATED_VOTE_TXN_FEE = 1000000000000000;              //0.1 cents
const unsigned long long DELEGATED_FEE = 1000000000000000;                       //0.1 cents



constexpr long long MIN_RATE = 1;     
constexpr unsigned long long ZERA_RATE = 100000000000000000; 
constexpr unsigned long long QUINTILLION = 1000000000000000000; 
constexpr unsigned long long ZRA_USD_EQUIV = 1280000000000000000;
constexpr unsigned long long ZERA_MAX_SUPPLY = 800000000;
constexpr unsigned long long ONEZERA = 1000000000;
constexpr long long ZERO = 0;
constexpr long ZERA_STAKE_PERCENTAGE = 500;       //50%
constexpr long STAKED_MATH_MULTIPLIER = 100000;     //100%

const size_t CHUNK_SIZE = 3.8 * 1024 * 1024; // 4MB

//INTS
constexpr int PROPOSER_AMOUNT = 10; //amount of randomly generated proposers
constexpr int VALIDATOR_AMOUNT = 10; //amount of validators to broadcast to
constexpr int BLOCK_TIMER = 5000; //amount of time between blocks in milliseconds
constexpr int BLOCK_SYNC = 100; //amount of blocks requested at once when syncing blockchain
constexpr int VALIDATOR_FEE_PERCENTAGE = 50; //the percentage of the fees that the validator recieves
constexpr int BURN_FEE_PERCENTAGE = 25; //the percentage of the fees that the burn recieves
constexpr int FOUNDATION_FEE_PERCENTAGE = 25;

//STRINGS
constexpr auto STAKE_MULTIPLIER = "stake_multiplier";
constexpr auto REQUIRED_VERSION = "REQUIRED_VERSION";
constexpr auto CONFIRMED_BLOCK_LATEST = "confirmed_block_latest";
constexpr auto RESTRICTED_SYMBOLS = "symbols";
constexpr auto ZERA_RATE_STRING = "100000000000000000";
constexpr auto ZERITE = "ZERITE";
constexpr auto ZERA_SYMBOL = "$ZRA+0000";
constexpr auto QUALIFIED = "QUALIFIED";
constexpr auto ANY = "ANY";
constexpr auto VALIDATOR_CONFIG = "/volume/config/validator.conf";
constexpr auto EXPLORER_CONFIG = "/volume/config/explorer_servers.conf";
constexpr auto DB_DIRECTORY = "/volume/blockchain/";
constexpr auto DB_REORGS = "/volume/reorgs/";
constexpr auto DB_VALIDATOR = "/volume/blockchain/validators";
constexpr auto DB_BLOCK = "/volume/blockchain/blocks";
constexpr auto DB_BLOCK_HEADER = "/volume/blockchain/block_headers";
constexpr auto DB_HASH_LOOKUP = "/volume/blockchain/hash_lookup";
constexpr auto DB_HASH_INDEX = "/volume/blockchain/hash_index";
constexpr auto DB_WALLET = "/volume/blockchain/wallets";
constexpr auto DB_RESTRICTED_WALLET = "/volume/blockchain/restricted_wallets";
constexpr auto DB_WALLET_TEMP = "/volume/blockchain/wallets_temp";
constexpr auto DB_CONTRACT = "/volume/blockchain/contracts";
constexpr auto DB_CONTRAT_SUPPLY = "/volume/blockchain/contract_supply";
constexpr auto DB_ITEMIZED_CONTRACT = "/volume/blockchain/contract_supply";
constexpr auto DB_TRANSACTIONS = "/volume/blockchain/transactions";
constexpr auto DB_BLOCK_TXNS = "/volume/blockchain/block_transactions";
constexpr auto DB_SMART_CONTRACT = "/volume/blockchain/smart_contracts";
constexpr auto DB_VOTING = "/volume/blockchain/govern_voting";
constexpr auto DB_PROPOSAL = "/volume/blockchain/govern_proposal";
constexpr auto EMPTY_KEY = "";
constexpr auto SEED_DIRECTORY = "/z_validator/config/seed_validators.txt";

constexpr auto BURN_WALLET = ":fire:";
constexpr auto TREASURY_WALLET = "46oLKvxWo9JASRrhJN3i4FDBhJPXTCgr1GtTe8pe8ECc";
constexpr auto PREPROCESS_PLACEHOLDER = "ThiSiSaPrePrOcesSPlaCeHolDer";

#endif