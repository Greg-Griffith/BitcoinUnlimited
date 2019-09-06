// Copyright (c) 2019 Greg Griffith
// Copyright (c) 2019 The Bitcoin Unlimited developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SLP_VALIDATION_H
#define BITCOIN_SLP_VALIDATION_H

#include "primitives/transaction.h"

#include "slpdb.h"
#include "token.h"

#include <vector>
#include <utility>

std::vector<std::pair<size_t, CSLPToken> > ValidateForSLP(CCoinsViewCache &view, const CTransaction &tx, CSLPTokenCache *slptokenview, int nHeight);

#endif
