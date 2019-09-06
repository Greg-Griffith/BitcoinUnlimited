// Copyright (c) 2019 Greg Griffith
// Copyright (c) 2019 The Bitcoin Unlimited developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "slpvalidation.h"

#include "coins.h"
#include "util.h"

bool ValidateMint(CCoinsViewCache &view, const CTransaction &tx)
{
    bool batonFound = false;
    for (auto &input : tx.vin)
    {
        Coin coin;
        if (!view.GetCoin(input.prevout, coin))
        {
            return false;
        }
        CSLPToken inputToken;
        if (inputToken.ParseBytes(coin.out.scriptPubKey) == 0)
        {
            if (inputToken.GetType() == SLP_TX_TYPE::SLP_MINT)
            {
                if (inputToken.GetBatonOut() == input.prevout.n)
                {
                    batonFound = true;
                    break;
                }
            }
        }
    }
    return batonFound;
}

bool ValidateSend(CCoinsViewCache &view, const CTransaction &tx, CSLPToken &newToken)
{
    uint64_t total_out = newToken.GetOutputAmount();
    uint64_t total_in = 0;
    for (auto &input : tx.vin)
    {
        Coin coin;
        if (!view.GetCoin(input.prevout, coin))
        {
            return false;
        }
        CSLPToken inputToken;
        if (inputToken.ParseBytes(coin.out.scriptPubKey) == 0)
        {
            total_in = total_in + inputToken.GetOutputAmountAt(input.prevout.n);
        }
    }
    return (total_in == total_out);
}

std::vector<std::pair<size_t, CSLPToken> > ValidateForSLP(CCoinsViewCache &view, const CTransaction &tx, CSLPTokenCache *slptokenview, int nHeight)
{
    std::vector<std::pair<size_t, CSLPToken> >valid_slp_txs;
    std::vector<std::pair<size_t, CSLPToken> >slp_txs;
    for (size_t i = 0; i < tx.vout.size(); ++i)
    {
        CSLPToken newToken(nHeight);
        if (newToken.ParseBytes(tx.vout[i].scriptPubKey) != 0)
        {
            // LOG SLP PARSE ERROR
        }
        slp_txs.push_back(std::make_pair(i, newToken));
    }
    for (auto &slp_tx : slp_txs)
    {
        if (slp_tx.second.GetType() == SLP_TX_TYPE::SLP_GENESIS)
        {
            // intentionally left blank, if it was a valid parse,
            // genesis doesnt rely on any past slp tx's so we are done
        }
        else if (slp_tx.second.GetType() == SLP_TX_TYPE::SLP_MINT)
        {
            if (!ValidateMint(view, tx))
            {
                continue;
            }
        }
        else if (slp_tx.second.GetType() == SLP_TX_TYPE::SLP_SEND)
        {
            if (!ValidateSend(view, tx, slp_tx.second))
            {
                continue;
            }
        }
        else if (slp_tx.second.GetType() == SLP_TX_TYPE::SLP_COMMIT)
        {
            // not in the spec yet so just continue
            continue;
        }
        valid_slp_txs.push_back(slp_tx);
    }
    return valid_slp_txs;
}
