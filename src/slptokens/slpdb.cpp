// Copyright (c) 2019 Greg Griffith
// Copyright (c) 2019 The Bitcoin Unlimited developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "slpdb.h"
#include "txdb.h"
#include "unlimited.h"

bool fSLPIndex = DEFAULT_SLPINDEX;
static const char DB_BEST_SLP_BLOCK = 'B';
static const char DB_SLP_TOKEN = 'T';
extern bool fImporting;
extern bool fReindex;
CSharedCriticalSection cs_slp_utxo;
CSLPTokenDB *pslptokendbview = nullptr;

struct TokenEntry
{
    COutPoint *outpoint;
    char key;
    TokenEntry(const COutPoint *ptr) : outpoint(const_cast<COutPoint *>(ptr)), key(DB_SLP_TOKEN) {}
    template <typename Stream>
    void Serialize(Stream &s) const
    {
        s << key;
        s << outpoint->hash;
        s << VARINT(outpoint->n);
    }

    template <typename Stream>
    void Unserialize(Stream &s)
    {
        s >> key;
        s >> outpoint->hash;
        s >> VARINT(outpoint->n);
    }
};

CSLPTokenDB::CSLPTokenDB(size_t nCacheSize, bool fMemory, bool fWipe)
    : db(GetDataDir() / "slpdb", nCacheSize, fMemory, fWipe, true)
{
}

bool CSLPTokenDB::GetToken(const COutPoint &outpoint, CSLPToken &token) const
{
    READLOCK(cs_slp_utxo);
    return db.Read(TokenEntry(&outpoint), token);
}

bool CSLPTokenDB::HaveToken(const COutPoint &outpoint) const
{
    READLOCK(cs_slp_utxo);
    return db.Exists(TokenEntry(&outpoint));
}

bool CSLPTokenDB::BatchWrite(CSLPTokenMap &mapTokens,
    const uint256 &hashBlock,
    const int64_t nBestTokenHeight,
    size_t &nChildcachedTokensUsage)
{
    WRITELOCK(cs_slp_utxo);
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    size_t nBatchWrites = 0;
    size_t batch_size = nMaxDBBatchSize;

    for (CSLPTokenMap::iterator it = mapTokens.begin(); it != mapTokens.end();)
    {
        if (it->second.flags & CSLPTokenCacheEntry::DIRTY)
        {
            TokenEntry entry(&it->first);
            size_t nUsage = it->second.token.DynamicMemoryUsage();
            if (it->second.token.IsSpent())
            {
                batch.Erase(entry);
                // Update the usage of the child cache before deleting the entry in the child cache
                nChildcachedTokensUsage -= nUsage;
                it = mapTokens.erase(it);
            }
            else
            {
                batch.Write(entry, it->second.token);

                // Only delete valid coins from the cache when we're nearly syncd.  During IBD
                // these coins will be used, whereas, once the chain is
                // syncd we only need the coins that have come from accepting txns into the memory pool.
                if (IsChainNearlySyncd() && !fImporting && !fReindex)
                {
                    // Update the usage of the child cache before deleting the entry in the child cache
                    nChildcachedTokensUsage -= nUsage;
                    it = mapTokens.erase(it);
                }
                else
                {
                    it->second.flags = 0;
                    it++;
                }
            }
            changed++;

            // In order to prevent the spikes in memory usage that used to happen when we prepared large as
            // was possible, we instead break up the batches such that the performance gains for writing to
            // leveldb are still realized but the memory spikes are not seen.
            if (batch.SizeEstimate() > batch_size)
            {
                db.WriteBatch(batch);
                batch.Clear();
                nBatchWrites++;
            }
        }
        else
            it++;
        count++;
    }
    if (!hashBlock.IsNull())
        _WriteBestBlock(hashBlock);

    bool ret = db.WriteBatch(batch);
    // LOG(COINDB, "Committing %u changed transactions (out of %u) to coin database with %u batch writes...\n",
    //    (unsigned int)changed, (unsigned int)count, (unsigned int)nBatchWrites);
    return ret;
}

size_t CSLPTokenDB::EstimateSize() const
{
    READLOCK(cs_slp_utxo);
    return db.EstimateSize(DB_SLP_TOKEN, (char)(DB_SLP_TOKEN + 1));
}

size_t CSLPTokenDB::TotalWriteBufferSize() const
{
    READLOCK(cs_slp_utxo);
    return db.TotalWriteBufferSize();
}

uint256 CSLPTokenDB::GetBestBlock() const
{
    READLOCK(cs_slp_utxo);
    return _GetBestBlock();
}

uint256 CSLPTokenDB::_GetBestBlock() const
{
    AssertLockHeld(cs_slp_utxo);
    uint256 nBestHeight;
    if (!db.Read(DB_BEST_SLP_BLOCK, nBestHeight))
    {
        return uint256(0);
    }
    return nBestHeight;
}

void CSLPTokenDB::WriteBestBlock(const uint256 &nBestBlockHash)
{
    WRITELOCK(cs_slp_utxo);
    _WriteBestBlock(nBestBlockHash);
}

void CSLPTokenDB::_WriteBestBlock(const uint256 &nBestBlockHash)
{
    AssertWriteLockHeld(cs_slp_utxo);
    db.Write(DB_BEST_SLP_BLOCK, nBestBlockHash);
}

CSLPTokenCache::CSLPTokenCache(CSLPTokenDB *baseIn) : base(baseIn), nBestTokenHeight(0), cachedTokensUsage(0) {}
CSLPTokenCache::CSLPTokenCache(CSLPTokenCache *viewIn) : base(viewIn->base), nBestTokenHeight(0), cachedTokensUsage(0)
{
}

CSLPTokenMap::iterator CSLPTokenCache::FetchSLPToken(const COutPoint &outpoint, CDeferredSharedLocker *lock) const
{
    // When fetching a coin, we only need the shared lock if the coin exists in the cache.
    // So we have the Locker object take the shared lock and return with the read lock held if the coin was in cache.
    {
        if (lock)
            lock->lock_shared();
        CSLPTokenMap::iterator it = cacheTokens.find(outpoint);
        if (it != cacheTokens.end())
            return it;
        if (lock)
            lock->unlock();
    }
    CSLPToken tmp;
    if (!base->GetToken(outpoint, tmp))
        return cacheTokens.end();

    // But if the coin is NOT in the cache, we need to grab the exclusive lock in order to modify the cache
    if (lock)
        lock->lock();
    CSLPTokenMap::iterator ret =
        cacheTokens
            .emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::forward_as_tuple(std::move(tmp)))
            .first;
    ret->second.flags = CCoinsCacheEntry::FRESH;
    cachedTokensUsage += ret->second.token.DynamicMemoryUsage();

    if (nBestTokenHeight < ret->second.token.nHeight)
        nBestTokenHeight = ret->second.token.nHeight;

    return ret;
}

bool CSLPTokenCache::GetSLPToken(const COutPoint &outpoint, CSLPToken &token) const
{
    CDeferredSharedLocker lock(cs_slp_utxo);
    CSLPTokenMap::const_iterator it = FetchSLPToken(outpoint, &lock);
    if (it != cacheTokens.end())
    {
        token = it->second.token;
        return true;
    }
    return false;
}

void CSLPTokenCache::AddSLPToken(const COutPoint &outpoint, CSLPToken &&token)
{
    WRITELOCK(cs_slp_utxo);
    CSLPTokenMap::iterator it;
    bool inserted;
    std::tie(it, inserted) =
        cacheTokens.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::tuple<>());
    bool fresh = false;
    if (!inserted)
    {
        cachedTokensUsage -= it->second.token.DynamicMemoryUsage();
    }
    fresh = !(it->second.flags & CCoinsCacheEntry::DIRTY);
    it->second.token = std::move(token);
    it->second.flags |= CCoinsCacheEntry::DIRTY | (fresh ? CCoinsCacheEntry::FRESH : 0);
    cachedTokensUsage += it->second.token.DynamicMemoryUsage();
    if (nBestTokenHeight < it->second.token.nHeight)
        nBestTokenHeight = it->second.token.nHeight;
}

void CSLPTokenCache::SpendSLPToken(const COutPoint &outpoint)
{
    WRITELOCK(cs_slp_utxo);
    CSLPTokenMap::iterator it = FetchSLPToken(outpoint, nullptr);
    if (it == cacheTokens.end())
        return;
    cachedTokensUsage -= it->second.token.DynamicMemoryUsage();
    if (it->second.flags & CCoinsCacheEntry::FRESH)
    {
        cacheTokens.erase(it);
    }
    else
    {
        it->second.flags |= CCoinsCacheEntry::DIRTY;
        it->second.token.Spend();
    }
}


/**
 * if you are tracking the SLP UTXO, you should do it just like UTXO for BCH (just check presence)
 * rather than validating the entire history which would become prohibitive.  It would be
 * like validating the entire history of every input back to the mining point for every possible parental path.
 * But you need to properly rewind the SLP UTXO just like we need to rewind the BCH UTXO.
 */

 void AddSLPToken(CSLPTokenCache &cache, const uint256 &txid, size_t i, CSLPToken &newToken)
 {
     cache.AddSLPToken(COutPoint(txid, i), std::move(newToken));
 }

void AddSLPTokens(CSLPTokenCache &cache, const CTransaction &tx, int nHeight)
{
    const uint256 &txid = tx.GetHash();
    for (size_t i = 0; i < tx.vout.size(); ++i)
    {
        CSLPToken newToken(nHeight);
        if (newToken.ParseBytes(tx.vout[i].scriptPubKey) != 0)
        {
            continue;
        }
        cache.AddSLPToken(COutPoint(txid, i), std::move(newToken));
    }
}

void SpendSLPTokens(const CTransaction &tx, CSLPTokenCache &inputs)
{
    for (const CTxIn &txin : tx.vin)
    {
        inputs.SpendSLPToken(txin.prevout);
    }
}
