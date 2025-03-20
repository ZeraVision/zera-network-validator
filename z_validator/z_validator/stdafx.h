#ifndef STDAFX_H
#define STDAFX_H

// Standard Library Headers
#include <iostream>
#include <string>
#include <vector>
#include <random>

// Third-Party Library Headers
#include <boost/multiprecision/cpp_int.hpp>

#include <grpcpp/grpcpp.h>
#include <google/protobuf/empty.pb.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/err.h>

// Your Project's Common Headers
#include "crypto/hashing.h"
#include "crypto/signatures.h"
#include "crypto/wallets.h"
#include "util/base58.h"
#include "util/const.h"
#include "proto/txn.grpc.pb.h"
#include "proto/txn.pb.h"
#include "proto/validator.grpc.pb.h"
#include "proto/validator.pb.h"
#include "proto/wallet.grpc.pb.h"
#include "proto/wallet.pb.h"

#endif // STDAFX_H