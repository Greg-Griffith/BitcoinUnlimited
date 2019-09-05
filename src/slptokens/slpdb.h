// Copyright (c) 2019 Greg Griffith
// Copyright (c) 2019 The Bitcoin Unlimited developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SLP_DB_H
#define BITCOIN_SLP_DB_H

#include "chain.h"
#include "coins.h"
#include "dbwrapper.h"
#include "serialize.h"
#include "sync.h"
#include "token.h"

#include <unordered_map>

static const bool DEFAULT_SLPINDEX = false;
extern bool fSLPIndex;
extern CSharedCriticalSection cs_slp_utxo;

struct CSLPTokenCacheEntry
{
    CSLPToken token; // The actual cached data.
    unsigned char flags;

    enum Flags
    {
        DIRTY = (1 << 0), // This cache entry is potentially different from the version in the parent view.
        FRESH = (1 << 1), // The parent view does not have this entry (or it is pruned).
    };

    CSLPTokenCacheEntry() : flags(0) {}
    explicit CSLPTokenCacheEntry(CSLPToken &&token_) : token(std::move(token_)), flags(0) {}
};

typedef std::unordered_map<COutPoint, CSLPTokenCacheEntry, SaltedOutpointHasher> CSLPTokenMap;

class CSLPTokenDB
{
protected:
    CDBWrapper db;

public:
    CSLPTokenDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetToken(const COutPoint &outpoint, CSLPToken &token) const;
    bool HaveToken(const COutPoint &outpoint) const;
    bool BatchWrite(CSLPTokenMap &mapTokens,
        const uint256 &hashBlock,
        const int64_t nBestCoinHeight,
        size_t &nChildCachedCoinsUsage);
    CCoinsViewCursor *Cursor() const;

    size_t EstimateSize() const;

    //! Return the current memory allocated for the write buffers
    size_t TotalWriteBufferSize() const;

    uint256 GetBestBlock() const;
    uint256 _GetBestBlock() const;
    void WriteBestBlock(const uint256 &nBestBlockHash);
    void _WriteBestBlock(const uint256 &nBestBlockHash);
};
extern CSLPTokenDB *pslptokendbview;

class CSLPTokenCache
{
protected:
    /**
     * Make mutable so that we can "fill the cache" even from Get-methods
     * declared as "const".
     */
    CSLPTokenDB *base;
    mutable uint256 hashBlock;
    mutable int64_t nBestTokenHeight;
    mutable CSLPTokenMap cacheTokens;
    mutable CSharedCriticalSection csCacheInsert;
    /* Cached dynamic memory usage for the inner Coin objects. */
    mutable size_t cachedTokensUsage;


public:
    CSLPTokenCache(CSLPTokenDB *baseIn);
    CSLPTokenCache(CSLPTokenCache *viewIn);

    // Standard CCoinsView methods
    bool GetSLPToken(const COutPoint &outpoint, CSLPToken &coin) const;
    bool HaveSLPToken(const COutPoint &outpoint) const;
    uint256 GetBestBlock() const;
    uint256 _GetBestBlock() const;
    void SetBestBlock(const uint256 &hashBlock);
    bool BatchWrite(CCoinsMap &mapCoins,
        const uint256 &hashBlock,
        const int64_t nBestTokenHeight,
        size_t &nChildCachedCoinsUsage);

    /**
     * Check if we have the given utxo already loaded in this cache.
     * The semantics are the same as HaveCoin(), but no calls to
     * the backing CCoinsView are made.
     */
    bool HaveCoinInCache(const COutPoint &outpoint) const;

    /**
     * Return a reference to Coin in the cache, or a pruned one if not found. This is
     * more efficient than GetCoin. Modifications to other cache entries are
     * allowed while accessing the returned pointer.
     */
    const Coin &_AccessCoin(const COutPoint &output) const;

    /**
     * Add a coin. Set potential_overwrite to true if a non-pruned version may
     * already exist.
     */
    void AddSLPToken(const COutPoint &outpoint, CSLPToken &&coin);

    /**
     * Spend a coin. Pass moveto in order to get the deleted data.
     * If no unspent output exists for the passed outpoint, this call
     * has no effect.
     */
    void SpendSLPToken(const COutPoint &outpoint);

    /**
     * Push the modifications applied to this cache to its base.
     * Failure to call this method before destruction will cause the changes to be forgotten.
     * If false is returned, the state of this cache (and its backing view) will be undefined.
     */
    bool Flush();

    /**
     * Empty the coins cache. Used primarily when we're shutting down and want to release memory
     */
    void Clear()
    {
        WRITELOCK(cs_slp_utxo);
        cacheTokens.clear();
    }

    /**
     * Remove excess entries from this cache.
     * Entries are trimmed starting from the beginning of the map.  In this way if those entries
     * are needed later they will all be collocated near the the beginning of the leveldb database
     * and will be faster to retrieve.
     */
    void Trim(size_t nTrimSize) const;

    /**
     * Removes the UTXO with the given outpoint from the cache, if it is
     * not modified.
     */
    void Uncache(const COutPoint &outpoint);

    /**
     * Removes all the UTXO outpoints for a given transaction, if they are
     * not modified.
     */
    void UncacheTx(const CTransaction &tx);

    //! Calculate the size of the cache (in number of transaction outputs)
    unsigned int GetCacheSize() const;

    //! Calculate the size of the cache (in bytes)
    size_t DynamicMemoryUsage() const;
    size_t _DynamicMemoryUsage() const;

    //! Recalculate and Reset the size of cachedCoinsUsage
    size_t ResetCachedCoinUsage() const;

    /**
     * Amount of bitcoins coming in to a transaction
     * Note that lightweight clients may not know anything besides the hash of previous transactions,
     * so may not be able to calculate this.
     *
     * @param[in] tx        transaction for which we are checking input total
     * @return        Sum of value of all inputs (scriptSigs)
     */
    CAmount GetValueIn(const CTransaction &tx) const;

    //! Check whether all prevouts of the transaction are present in the UTXO set represented by this view
    bool HaveInputs(const CTransaction &tx) const;

protected:
    // returns an iterator pointing to the coin and lock is taken (caller must unlock when finished with iterator)
    // If lock is nullptr, the writelock must already be taken.
    CSLPTokenMap::iterator FetchSLPToken(const COutPoint &outpoint, CDeferredSharedLocker *lock) const;

    /**
     * By making the copy constructor private, we prevent accidentally using it when one intends to create a cache on
     * top of a base cache.
     */
    CSLPTokenCache(const CSLPTokenCache &);
};


/**
 * if you are tracking the SLP UTXO, you should do it just like UTXO for BCH (just check presence)
 * rather than validating the entire history which would become prohibitive.  It would be
 * like validating the entire history of every input back to the mining point for every possible parental path.
 * But you need to properly rewind the SLP UTXO just like we need to rewind the BCH UTXO.
 */
void AddSLPTokens(CSLPTokenCache &cache, const CTransaction &tx, int nHeight);
void SpendSLPTokens(const CTransaction &tx, CSLPTokenCache &inputs);

#endif
