// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2015-2017 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Server/client environment: argument handling, config file parsing,
 * logging, thread wrappers
 */
#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "allowed_args.h"
#include "compat.h"
#include "fs.h"
#include "tinyformat.h"
#include "utiltime.h"

#include <exception>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/signals2/signal.hpp>
#include <boost/thread/exceptions.hpp>

#ifdef DEBUG_ASSERTION
/// If DEBUG_ASSERTION is enabled this asserts when the predicate is false.
//  If DEBUG_ASSERTION is disabled and the predicate is false, it executes the execInRelease statements.
//  Typically, the programmer will error out -- return false, raise an exception, etc in the execInRelease code.
//  DO NOT USE break or continue inside the DbgAssert!
#define DbgAssert(pred, execInRelease) assert(pred)
#else
#define DbgStringify(x) #x
#define DbgStringifyIntLiteral(x) DbgStringify(x)
#define DbgAssert(pred, execInRelease)                                                                        \
    do                                                                                                        \
    {                                                                                                         \
        if (!(pred))                                                                                          \
        {                                                                                                     \
            LogPrintStr(std::string(                                                                          \
                __FILE__ "(" DbgStringifyIntLiteral(__LINE__) "): Debug Assertion failed: \"" #pred "\"\n")); \
            execInRelease;                                                                                    \
        }                                                                                                     \
    } while (0)
#endif

#define UNIQUE2(pfx, LINE) pfx##LINE
#define UNIQUE1(pfx, LINE) UNIQUE2(pfx, LINE)
/// UNIQUIFY is a macro that appends the current file's line number to the passed prefix, creating a symbol
// that is unique in this file.
#define UNIQUIFY(pfx) UNIQUE1(pfx, __LINE__)

static const bool DEFAULT_LOGTIMEMICROS = false;
static const bool DEFAULT_LOGIPS = true;
static const bool DEFAULT_LOGTIMESTAMPS = true;

// For bitcoin-cli
extern const char DEFAULT_RPCCONNECT[];
static const int DEFAULT_HTTP_CLIENT_TIMEOUT = 900;

/** Signals for translation. */
class CTranslationInterface
{
public:
    /** Translate a message to the native language of the user. */
    boost::signals2::signal<std::string(const char *psz)> Translate;
};

extern std::map<std::string, std::string> mapArgs;
extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern bool fDebug;
extern bool fPrintToConsole;
extern bool fPrintToDebugLog;
extern bool fServer;
extern std::string strMiscWarning;
extern bool fLogTimestamps;
extern bool fLogTimeMicros;
extern bool fLogIPs;
extern volatile bool fReopenDebugLog;
extern CTranslationInterface translationInterface;

extern const char *const BITCOIN_CONF_FILENAME;
extern const char *const BITCOIN_PID_FILENAME;

/** Send a string to the log output */
int LogPrintStr(const std::string &str);

// Logging API:
// Use the two macros
// LOG(ctgr,...)
// LOGA(...)
// located further down.
// (Do not use the Logging functions directly)
namespace Logging
{
extern uint64_t categoriesEnabled;

/*
To add a new log category:
1) Create a unique 1 bit category mask. (Easiest is to 2* the last enum entry.)
   Put it at the end of enum below.
2) Add an category/string pair to LOGLABELMAP macro below.
*/

// Log Categories:
// 64 Bits: (Define unique bits, not 'normal' numbers)
enum
{
    NONE = 0x0, // No logging
    ALL = 0xFFFFFFFFFFFFFFFFUL, // Log everything

    // LOG Categories:
    THN = 0x1,
    MEP = 0x2,
    CDB = 0x4,
    TOR = 0x8,

    NET = 0x10,
    ADR = 0x20,
    LIB = 0x40,
    HTP = 0x80,

    RPC = 0x100,
    PRT = 0x200,
    BNC = 0x400,
    PRN = 0x800,

    RDX = 0x1000,
    MPR = 0x2000,
    BLK = 0x4000,
    EVC = 0x8000,

    PRL = 0x10000,
    RND = 0x20000,
    REQ = 0x40000,
    BLM = 0x80000,

    EST = 0x100000,
    LCK = 0x200000,
    PRX = 0x400000,
    DBS = 0x800000,
    SLC = 0x1000000,

};

// Add corresponding upper case string for the category:
#define LOGLABELMAP                                                                                           \
    {                                                                                                         \
        {NONE, "NONE"}, {ALL, "ALL"}, {THN, "THN"}, {MEP, "MEP"}, {CDB, "CDB"}, {TOR, "TOR"}, {NET, "NET"},   \
            {ADR, "ADR"}, {LIB, "LIB"}, {HTP, "HTP"}, {RPC, "RPC"}, {PRT, "PRT"}, {BNC, "BNC"}, {PRN, "PRN"}, \
            {RDX, "RDX"}, {MPR, "MPR"}, {BLK, "BLK"}, {EVC, "EVC"}, {PRL, "PRL"}, {RND, "RND"}, {REQ, "REQ"}, \
            {BLM, "BLM"}, {LCK, "LCK"}, {PRX, "PRX"}, {DBS, "DBS"}, {SLC, "SLC"},                             \
        {                                                                                                     \
            EST, "EST"                                                                                        \
        }                                                                                                     \
    }

/**
 * Check if a category should be logged
 * @param[in] category
 * returns true if should be logged
 */
inline bool LogAcceptCategory(uint64_t category) { return (categoriesEnabled & category); }
/**
 * Turn on/off logging for a category
 * @param[in] category
 * @param[in] on  True turn on, False turn off.
 */
inline void LogToggleCategory(uint64_t category, bool on)
{
    if (on)
        categoriesEnabled |= category;
    else
        categoriesEnabled &= ~category; // off
}

/**
* Get a category associated with a string.
* @param[in] label string
* returns category
*/
uint64_t LogFindCategory(const std::string label);

/**
 * Get the label / associated string for a category.
 * @param[in] category
 * returns label
 */
std::string LogGetLabel(uint64_t category);

/**
 * Get all categories and their state.
 * Formatted for display.
 * returns all categories and states
 */
std::string LogGetAllString();

/**
 * Initialize
 */
void LogInit();

/**
 * Write log string to console:
 *
 * @param[in] All parameters are "printf like".
 */
template <typename T1, typename... Args>
inline void LogStdout(const char *fmt, const T1 &v1, const Args &... args)
{
    try
    {
        std::string str = tfm::format(fmt, v1, args...);
        ::fwrite(str.data(), 1, str.size(), stdout);
    }
    catch (...)
    {
        // Number of format specifiers (%) do not match argument count, etc
    };
}

/**
 * Write log string to console:
 * @param[in] str String to log.
 */
inline void LogStdout(const std::string &str)
{
    ::fwrite(str.data(), 1, str.size(), stdout); // No formatting for a simple string
}

/**
 * Log a string
 * @param[in] All parameters are "printf like args".
 */
template <typename T1, typename... Args>
inline void LogWrite(const char *fmt, const T1 &v1, const Args &... args)
{
    try
    {
        LogPrintStr(tfm::format(fmt, v1, args...));
    }
    catch (...)
    {
        // Number of format specifiers (%) do not match argument count, etc
    };
}

/**
 * Log a string
 * @param[in] str String to log.
 */
inline void LogWrite(const std::string &str)
{
    LogPrintStr(str); // No formatting for a simple string
}
}

// Logging API:
//
/**
 * LOG macro: Log a string if a category is enabled.
 * Note that categories can be ORed, such as: (NET|TOR)
 *
 * @param[in] category -Which category to log
 * @param[in] ... "printf like args".
 */
#define LOG(ctgr, ...)               \
    {                                \
        using namespace Logging;     \
        if (LogAcceptCategory(ctgr)) \
            LogWrite(__VA_ARGS__);   \
    }                                \
    void(0)

/**
 * LOGA macro: Log a string to the console.
 *
 * @param[in] ... "printf like args".
 */
#define LOGA(...) Logging::LogStdout(__VA_ARGS__)
//

// Log tests:
UniValue setlog(const UniValue &params, bool fHelp);
// END logging.


/**
 * Translate a boolean string to a bool.
 * Throws an exception if not one of the strings.
 * Is case insensitive.
 * @param[in] one of "enable|disable|1|0|true|false|on|off"
 * returns true if enabled, false if not.
 */
bool IsStringTrue(const std::string &str);

/**
 * Translation function: Call Translate signal on UI interface, which returns a boost::optional result.
 * If no translation slot is registered, nothing is returned, and simply return the input.
 */
inline std::string _(const char *psz)
{
    boost::optional<std::string> rv = translationInterface.Translate(psz);
    return rv ? (*rv) : psz;
}

void SetupEnvironment();
bool SetupNetworking();

/** Return true if log accepts specified category */
bool LogAcceptCategory(const char *category);

#define LogPrintf(...) LogPrint(NULL, __VA_ARGS__)

template <typename T1, typename... Args>
static inline int LogPrint(const char *category, const char *fmt, const T1 &v1, const Args &... args)
{
    if (!LogAcceptCategory(category))
        return 0;
    return LogPrintStr(tfm::format(fmt, v1, args...));
}

template <typename T1, typename... Args>
bool error(const char *fmt, const T1 &v1, const Args &... args)
{
    LogPrintStr("ERROR: " + tfm::format(fmt, v1, args...) + "\n");
    return false;
}

/**
 * Zero-arg versions of logging and error, these are not covered by
 * the variadic templates above (and don't take format arguments but
 * bare strings).
 */
static inline int LogPrint(const char *category, const char *s)
{
    if (!LogAcceptCategory(category))
        return 0;
    return LogPrintStr(s);
}
static inline bool error(const char *s)
{
    LogPrintStr(std::string("ERROR: ") + s + "\n");
    return false;
}

/**
 Format an amount of bytes with a unit symbol attached, such as MB, KB, GB.
 Uses Kilobytes x1000, not Kibibytes x1024.

 Output value has two digits after the dot. No space between unit symbol and
 amount.

 Also works for negative amounts. The maximum unit supported is 1 Exabyte (EB).
 This formatting is used by the thinblock statistics functions, and this
 is a factored-out utility function.

 @param [value] The value to format
 @return String with unit
 */
extern std::string formatInfoUnit(double value);

void PrintExceptionContinue(const std::exception *pex, const char *pszThread);
void ParseParameters(int argc, const char *const argv[], const AllowedArgs::AllowedArgs &allowedArgs);
void FileCommit(FILE *fileout);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);
bool RenameOver(fs::path src, fs::path dest);
bool TryCreateDirectories(const fs::path &p);
fs::path GetDefaultDataDir();
const fs::path &GetDataDir(bool fNetSpecific = true);
void ClearDatadirCache();
fs::path GetConfigFile(const std::string &confPath);
#ifndef WIN32
fs::path GetPidFile();
void CreatePidFile(const fs::path &path, pid_t pid);
#endif
void ReadConfigFile(std::map<std::string, std::string> &mapSettingsRet,
    std::map<std::string, std::vector<std::string> > &mapMultiSettingsRet,
    const AllowedArgs::AllowedArgs &allowedArgs);
#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void OpenDebugLog();
void ShrinkDebugFile();
void runCommand(const std::string &strCommand);

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string &strArg, const std::string &strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string &strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string &strArg, bool fDefault);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string &strArg, const std::string &strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string &strArg, bool fValue);

/**
 * Return the number of physical cores available on the current system.
 * @note This does not count virtual cores, such as those provided by HyperThreading
 * when boost is newer than 1.56.
 */
int GetNumCores();

void SetThreadPriority(int nPriority);
void RenameThread(const char *name);

/**
 * .. and a wrapper that just calls func once
 */
template <typename Callable>
void TraceThread(const char *name, Callable func)
{
    std::string s = strprintf("bitcoin-%s", name);
    RenameThread(s.c_str());
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    }
    catch (const boost::thread_interrupted &)
    {
        LogPrintf("%s thread interrupt\n", name);
        throw;
    }
    catch (const std::exception &e)
    {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...)
    {
        PrintExceptionContinue(NULL, name);
        throw;
    }
}

std::string CopyrightHolders(const std::string &strPrefix);

#endif // BITCOIN_UTIL_H
