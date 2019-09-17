// Copyright (c) 2019 Greg Griffith
// Copyright (c) 2019 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_THREAD_DEADLOCK_H
#define BITCOIN_THREAD_DEADLOCK_H

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <memory>
#include <mutex>

#include "locklocation.h"
#include "utilstrencodings.h"

#ifdef DEBUG_LOCKORDER // this ifdef covers the rest of the file
#ifdef __linux__
#include <sys/syscall.h>
inline uint64_t getTid(void)
{
    // "native" thread id used so the number correlates with what is shown in gdb
    pid_t tid = (pid_t)syscall(SYS_gettid);
    return tid;
}
#else
#include <functional>
inline uint64_t getTid(void)
{
    // Note: there is no guaranteed way to turn the thread-id into an int
    // since it's an opaque type. Just about the only operation it supports
    // is std::hash (so that thread id's may be placed in maps).
    // So we just do this.
    static std::hash<std::thread::id> hasher;
    return uint64_t(hasher(std::this_thread::get_id()));
}
#endif

struct LockData
{
    // Very ugly hack: as the global constructs and destructors run single
    // threaded, we use this boolean to know whether LockData still exists,
    // as DeleteLock can get called by global CCriticalSection destructors
    // after LockData disappears.
    bool available;
    LockData() : available(true) {}
    ~LockData() { available = false; }
    ReadLocksWaiting readlockswaiting;
    WriteLocksWaiting writelockswaiting;

    ReadLocksHeld readlocksheld;
    WriteLocksHeld writelocksheld;
    LocksHeldByThread locksheldbythread;
    SeenLockOrders seenlockorders;
    std::mutex dd_mutex;
};
extern LockData lockdata;

/**
 * Adds a new lock to LockData tracking
 *
 * Should only be called by EnterCritical
 */
void push_lock(void *c, const CLockLocation &locklocation, LockType locktype, OwnershipType ownership, bool fTry);

/**
 * Removes a critical section and all locks related to it from LockData
 *
 * Should only be called by a critical section destructor
 */
void DeleteCritical(void *cs);

/**
 * Removes the most recent instance of locks from LockData
 *
 * Should only be called by LeaveCritical
 */
void remove_lock_critical_exit(void *cs);

/**
 * Prints all of the locks held by the calling thread
 */
std::string LocksHeld();

/**
 * Moves a lock that is currently in one of the waiting maps to the corresponding held map
 */
void SetWaitingToHeld(void *c, OwnershipType ownership);

#else // NOT DEBUG_LOCKORDER

static inline void SetWaitingToHeld(void *c, OwnershipType ownership) {}

#endif // END DEBUG_LOCKORDER

#endif