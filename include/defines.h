/*
 * =====================================================================================
 *
 *       Filename:  defines.h
 *
 *    Description:  basic types
 *
 *        Version:  1.0
 *        Created:  01/19/2016 08:26:33 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi(woodhead99@gmail.com )
 *   Organization:
 *
 *      Copyright:  2019 Ming Zhi( woodhead99@gmail.com )
 *
 *        License:  Licensed under GPL-3.0. You may not use this file except in
 *                  compliance with the License. You may find a copy of the
 *                  License at 'http://www.gnu.org/licenses/gpl-3.0.html'
 *
 * =====================================================================================
 */

#pragma once
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <atomic>
#include <algorithm>
#include <semaphore.h>
#include <chrono>
#include <sys/types.h>
#include <unistd.h>
#include <syscall.h>
#include <thread>
#include <set>
#include <string.h>

#include "clsids.h"
#include "propids.h"
#include "sys/user.h"

#include <arpa/inet.h>
#include <dbus/dbus.h>

#if defined(__LP64__) || defined(_LP64)
#define BUILD_64   1
#define LONGWORD                guint64
#else
#define BUILD_64   0
#define LONGWORD                guint32
#endif

#ifdef ARM
#ifndef PAGE_SIZE
#define PAGE_SIZE               ( getpagesize() )
#endif
#endif

#define HANDLE                  uintptr_t
#define PortToHandle( ptr )     reinterpret_cast<HANDLE>( ( IPort* )ptr )
#define HandleToPort( handle )  reinterpret_cast< IPort* >( handle )
#define INVALID_HANDLE          ( ( HANDLE )0 )

#define IPV6_ADDR_BYTES         ( sizeof( in6_addr ) )
#define IPV4_ADDR_BYTES         ( sizeof( in_addr ) )
#define MAX_IPADDR_SIZE         IPV6_ADDR_BYTES

#define likely(x)               __builtin_expect((x),1)
#define unlikely(x)             __builtin_expect((x),0)

#define ERROR_LIKELY( ret_ )   likely( ( ( ( gint32 )ret_ ) < 0 ) )
#define ERROR_UNLIKELY( ret_ ) unlikely( ( ( ( gint32 )ret_ ) < 0 ) )

#define ERROR( ret_ )          ERROR_UNLIKELY( ret_ )

#define SUCCEEDED( ret_ )      likely( ( ret_ == 0 ) )
#define FAILED( ret_ ) ERROR( ret_ )

// the status codes
// STATUS_PENDING, an irp is accepted and the
// operation will happen sometime later
#define STATUS_PENDING                      ( ( gint32 ) 0x10001 )
#define STATUS_MORE_PROCESS_NEEDED          ( ( gint32 ) 0x10002 )
#define STATUS_CHECK_RESP                   ( ( gint32 ) 0x10003 )

// STATUS_SUCCESS, the irp is completed successfully
#define STATUS_SUCCESS          0
// STATUS_FAIL, the irp is completed with unknown error
#define ERROR_FAIL              ( ( gint32 )0x80000001 )
// STATUS_TIMEOUT, the irp is timeout, no response or
// no incoming notification
#define ERROR_TIMEOUT           ( -ETIMEDOUT )

// STATUS_CANCEL, the irp is cancelled at the caller's
// request
#define ERROR_CANCEL            ( -ECANCELED )

// bad address
#define ERROR_ADDRESS           ( ( gint32 )0x80010002 ) 
#define ERROR_STATE             ( ( gint32 )0x80010003 )
#define ERROR_WRONG_THREAD      ( ( gint32 )0x80010004 )
#define ERROR_CANNOT_CANCEL     ( ( gint32 )0x80010005 )
#define ERROR_PORT_STOPPED      ( ( gint32 )0x80010006 )
#define ERROR_FALSE             ( ( gint32 )0x80010007 )

#define ERROR_REPEAT            ( ( gint32 )0x80010008 )
#define ERROR_PREMATURE         ( ( gint32 )0x80010009 )
#define ERROR_NOT_HANDLED       ( ( gint32 )0x8001000a )
#define ERROR_CANNOT_COMP       ( ( gint32 )0x8001000b )
#define ERROR_USER_CANCEL       ( ( gint32 )0x8001000c )
#define ERROR_PAUSED            ( ( gint32 )0x8001000d )
#define ERROR_CANCEL_INSTEAD    ( ( gint32 )0x8001000f )
#define ERROR_NOT_IMPL          ( ( gint32 )0x80010010 )
#define ERROR_DUPLICATED        ( ( gint32 )0x80010011 )

// for flow control
#define ERROR_QUEUE_FULL        ( ( gint32 )0x8001000e )

#define AlignDword( val )    ( ( val + 3 ) & ~( sizeof( guint32 ) - 1 ) )

#define SEM_WAKEUP_ALL( sem_ ) \
do{\
    int iCount;\
    if( 0 == sem_getvalue( &sem_, &iCount ) )\
    {\
        iCount = -iCount; \
        if( iCount <= 0 ) \
            break; \
        for( int i = 0; i < iCount; ++i )\
        {\
            Sem_Post( &sem_ );\
        }\
    }\
}while( 0 )

#define SEM_WAKEUP_SINGLE( sem_ ) \
do{\
    int iCount;\
    if( 0 == sem_getvalue( &sem_, &iCount ) )\
    {\
        iCount = -iCount; \
        if( iCount <= 0 ) \
            break; \
        Sem_Post( &sem_ );\
    }\
}while( 0 )

// Connection Management
// Passive Connection
// a controller notifies to be online
#define NOTIFY_CONTROLLER_ONLINE

// a controller notifies to be offline 
#define NOTIFY_CONTROLLER_OFFLINE

// Active Connection
// the client asks to setup the UI session
#define MSG_REQ_LOGIN

// the client is about to end the UI session
#define MSG_REQ_LOGOUT

// end a connection to a controller
// or a session
#define MSG_REQ_DISCONNECT

// make a tcp connection to a controller
// or a session
#define MSG_REQ_CONNECT

// a flag to indicate if the mod event has connection
// to all the attached object
#define MOD_ONOFFLINE_IRRELEVANT    0x02

#define REG_MAX_PATH    2048
#define REG_MAX_NAME    128 

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define DebugMsg( ret, strFmt, ... ) \
 DebugMsgEx( __FILENAME__, __LINE__, ret, strFmt, ##__VA_ARGS__ )

#ifdef DEBUG
#define DebugPrint( ret, strFmt, ... ) \
({ printf( "%s\n", \
    DebugMsg( ret, strFmt, ##__VA_ARGS__ ).c_str() );} )

#else

#define DebugPrint( ret, strFmt, ... ) \
({ printf( "%s\n", \
    DebugMsg( ret, strFmt, ##__VA_ARGS__ ).c_str() );} )

#endif

#define DebugPrintEx( _level, ret, strFmt, ... ) \
({ if( ( _level ) <= g_dwLogLevel ) \
    printf( "%s\n", \
    DebugMsg( ret, strFmt, ##__VA_ARGS__ ).c_str() );} )

#define MAX_PENDING_MSG             20
#define MAX_DBUS_REQS               ( MAX_PENDING_MSG * 1000 )

// for Sem_Timedwait interval
#define THREAD_WAKEUP_INTERVAL      10
#define RPC_SVR_DEFAULT_PORTNUM         0x1024

#define MAX_BYTES_PER_TRANSFER ( 1024 * 1024 )
#define MAX_BYTES_PER_FILE     ( 512 * 1024 * 1024 )
#define MAX_BYTES_PER_BUFFER   ( 16 * 1024 * 1024 )

// streaming parameters
// queue size for uxsock streams
#define STM_MAX_QUEUE_SIZE      128

// queue size for tcp streams
#define STM_MAX_RECV_PACKETS    ( 2 * STM_MAX_QUEUE_SIZE + 16 )

#define MAX_TIMEOUT_VALUE       ( 3600 * 24 * 30 )

#define G_SOURCE_CONTINUE true
#define G_SOURCE_REMOVE false

namespace rpcf
{

#include <poll.h>
enum GIOCondition
{
	G_IO_IN = POLLIN,   // 1,
	G_IO_PRI = POLLPRI, // 2,
	G_IO_OUT = POLLOUT, // 4,
	G_IO_ERR = POLLERR, // 8,
	G_IO_HUP = POLLHUP, // 16,
	G_IO_NVAL = POLLNVAL// 32
};

typedef enum : guint32 
{
    logEmerg = 0,
    logAlert,
    logCrit,
    logErr,
    logWarning,
    logNotice, // = 5
    logInfo,

}EnumLogLvl;

extern EnumLogLvl g_dwLogLevel;

extern std::string DebugMsgInternal(
    gint32 ret, const std::string& strMsg,
    const char* szFunc, gint32 iLineNum );

extern std::string DebugMsgEx(
    const char* szFunc, gint32 iLineNum,
    gint32 ret, const std::string& strFmt, ... );

extern gint32 ErrnoFromDbusErr( const char *error );

extern guint64 htonll( guint64 hostll );
extern guint64 ntohll( guint64 netll );
extern int Sem_Init( sem_t* psem, int pshared, unsigned int value );
extern int Sem_Timedwait( sem_t* psem, const timespec& ts );
extern int Sem_TimedwaitSec( sem_t* psem, gint32 iSec );
extern int Sem_Post( sem_t* psem );
extern int Sem_Wait( sem_t* psem );

// set the name of the current thread
extern gint32 SetThreadName(const std::string& strName );
extern std::string GetThreadName();
inline guint32 GetTid()
{ return ( guint32 )syscall( SYS_gettid ); }

inline gint32 SendBytesNoSig( int iFd,
    void* pBuf, guint32 dwSize )
{
    return send( iFd, pBuf,
        dwSize, MSG_NOSIGNAL );
}

gint64 GetRandom();

gint32 GetEnvLibPath(
    std::set< std::string >& oSet );

gint32 GetLibPathName( std::string& strResult,
    const char* szLibName = nullptr );

gint32 GetLibPath( std::string& strResult,
    const char* szLibName = nullptr );

gint32 GetModulePath( std::string& strResult );

gint32 FindInstCfg(
    const std::string& strFileName,
    std::string& strPath );

using stdstr = std::string;

// Event ids for the IEventSink
enum EnumEventId : guint32
{
    // the irp is completed, the pData contains the pointer to the irp
    // and the dwParam1 holds an extra specific event code the caller 
    // set to the irp->m_dwContext
    eventZero = 0,
    eventIrpComp = 0x100,

    // the irp is cancelled, the pData contains the pointer to the irp
    // and the dwParam1 holds an extra specific event code the caller 
    // set to the irp->m_dwContext
    eventIrpCancel,

    // a notification message arrives, pData is
    // the pointer to the notification
    eventNotification,

    // timeout event, the dwParam1 contains the
    // context previously passed to the timer
    // setup method, dwParam2 holds the reason of
    // this invoke, either timeout or cancelled 
    eventTimeout,

    // an admin event happens, dwParam1 indicate
    // the specific event
    eventAdmin,

    // the workitem is about to run, and the pData
    // contains the context
    eventWorkitem,

    // the workitem is about to cancel, and the
    // pData contains the context
    eventWorkitemCancel,

    // the fd event happens, and dwParam1 contains
    // the fd , and dwParam2 holds the errno
    eventFd,

    // deferred task to create the pdo
    eventCreatePdo,

    // cancel all the irps
    eventCancelIrp,

    // cancel a tasklet
    eventCancelTask,

    // major event id for event notification to
    // interfaces
    eventPnp,
    eventConnPoint,

    // a sub-parameter used by CTaskletRetriable
    // on timeout
    eventRetry,

    // a sub-parameter used by CIfInvokeMethodTask
    // on timeout
    eventKeepAlive,

    // an notification from server to the proxy
    eventRpcNotify,
    eventProgress,

    // iomanger start event
    eventStart,
    eventStop,

    // port event
    eventPortAttached,
    eventPortStarted,
    eventPortStartFailed,
    eventPortStopped,
    eventPortStopping,

    // the interface related events and commands
    //
    // connection point events
    eventModOnline,
    eventModOffline,
    eventDBusOnline,
    eventDBusOffline,
    eventRmtModOnline,
    eventRmtModOffline,
    eventRmtDBusOnline,
    eventRmtDBusOffline,
    eventRmtSvrOnline,
    eventRmtSvrOffline,

    // interface commands
    cmdOpenPort,
    cmdClosePort,
    cmdEnableEvent, 
    cmdDisableEvent, 
    cmdRegSvr, 
    cmdUnregSvr, 
    cmdStartRecvMsg,
    cmdStopRecvMsg,
    cmdSendReq,
    cmdSendData,
    cmdFetchData,
    eventMsgArrived,

    // administrative commands and events
    cmdShutdown,
    cmdPause,
    cmdResume,
    eventResumed,
    eventPaused,
    cmdCleanup,
    
    eventTaskThrdCtx, 
    eventOneShotTaskThrdCtx,
    eventIrpCompThrdCtx,

    eventTaskComp,

    cmdDispatchData,

    // event from the socket fd
    eventSocket,
    eventNewConn,
    eventDisconn,
    eventConnErr,
    // time elapsed in second
    eventAgeSec,
    eventCeiling,
    eventTimeoutCancel,
    eventUserCancel,

    // an event of filter completion
    eventFilterComp,

    // an event for IO watcher
    eventIoWatch,
    eventAsyncWatch,

    eventHouseClean,

    eventMaxReserved = 0x10000,
    eventUserStart = 0x10001,
    eventInvalid = 0x100000,

    eventTryLock = 0x80000000

};

// protocols to serialize/deserialze parameters
enum EnumSeriProto : guint32
{
    seriNone = 0, // No serialization
    seriRidl = 1, // ridl serialization( to come )
    seriPython = 2, // Python's Pickle
    seriJava = 3, // Java's Object Stream
    seriInvalid = 4
};

struct SERI_HEADER_BASE
{
    guint32 dwClsid;
    guint32 dwSize;
    guint8  bVersion;
    guint8  bReserved[ 3 ];

    SERI_HEADER_BASE()
    {
        dwClsid = 0;
        dwSize = 0;
        bVersion = 1;
        memset( bReserved, 0, 3 );
    };

    SERI_HEADER_BASE( const SERI_HEADER_BASE& oSrc )
    {
        dwClsid = oSrc.dwClsid;
        dwSize = oSrc.dwSize;
        bVersion = oSrc.bVersion;
        memset( bReserved, 0, 3 );
    }
    
    SERI_HEADER_BASE& operator=(
        const SERI_HEADER_BASE& rhs )
    {
        dwClsid = rhs.dwClsid;
        dwSize = rhs.dwSize;
        bVersion = rhs.bVersion;
        return *this;
    }

    void ntoh()
    {
        dwClsid = ntohl( dwClsid );
        dwSize = ntohl( dwSize );
    }

    void hton()
    {
        dwClsid = htonl( dwClsid );
        dwSize = htonl( dwSize );
    }

};

class CBuffer;
class IConfigDb;
class CObjBase
{
    // boundary marker
    guint32                                 m_dwMagic;
    static std::atomic< gint32 >            m_atmObjCount;
    EnumClsid                               m_dwClsid;
    mutable std::atomic< gint32 >           m_atmRefCount;
    guint64                                 m_qwObjId;

    static std::atomic< guint64 >           m_atmObjId;
    public:

    CObjBase();

    // copy constructor
    CObjBase( const CObjBase& rhs );

    virtual ~CObjBase();
    gint32 SetClassId(
        EnumClsid iClsid ) ;

    gint32 AddRef();

    // used in some constructors to guard the object
    // from being deleted. Don't use this function
    // unless you know what you are doing
    gint32 DecRef();

    gint32 Release();

    gint32 GetRefCount();

    EnumClsid GetClsid() const;

    guint64 GetObjId() const;

    const char* GetClassName() const;

    virtual gint32 Serialize(
        CBuffer& oBuf ) const;

    virtual gint32 Deserialize(
        const CBuffer& obuf );

    virtual gint32 Deserialize(
        const char* pBuf, guint32 dwSize );

    virtual gint32 GetProperty(
        gint32 iProp, CBuffer& oBuf ) const;

    virtual gint32 SetProperty(
        gint32 iProp, const CBuffer& oBuf );

    virtual gint32 GetPropertyType(
        gint32 iProp, gint32& iType ) const;

    virtual gint32 RemoveProperty(
        gint32 iProp );

    virtual gint32 EnumProperties(
        std::vector< gint32 >& vecProps ) const;

    virtual gint32 Intitialize(
        const IConfigDb* pCfg );

    CObjBase& operator=( const CObjBase& rhs )
    {
        m_dwClsid = rhs.m_dwClsid;
        return *this;
    }
    virtual std::string GetVersion()
    {
        // object version number
        return "1.0";
    }
    static gint32 GetActCount()
    { return m_atmObjCount; }

    void Dump( std::string& strDump );
};

class IEventSink : public CObjBase
{
    public:

    typedef CObjBase    super;
    // EVENT category:
    // irp completion
    // notification messages on the port
    // timeout event
    // admin event
    // workitem
    // fd event
    virtual gint32 OnEvent(
        EnumEventId iEvent,
        LONGWORD    dwParam1 = 0,
        LONGWORD    dwParam2 = 0,
        LONGWORD*   pData = NULL  ) = 0;
};

class ICancellableTask : public IEventSink
{
    public:
    virtual gint32 OnCancel(
        guint32 dwContext ) = 0;
};

class IService : public IEventSink 
{
    public:
    typedef IEventSink super;

    virtual gint32 Start() = 0;
    virtual gint32 Stop() = 0;
};

class IServiceEx : public IService 
{
    public:
    //
    // this interface makes administrative command
    // asynchronous
    //
    typedef IService super;

    virtual gint32 StartEx(
        IEventSink* pCallback ) = 0;

    virtual gint32 StopEx(
        IEventSink* pCallback ) = 0;

    virtual gint32 Pause(
        IEventSink* pCallback ) = 0;

    virtual gint32 Resume(
        IEventSink* pCallback ) = 0;

    virtual gint32 Shutdown(
        IEventSink* pCallback ) = 0;

    virtual gint32 Restart(
        IEventSink* pCallback ) = 0;
};

class IThread : public IService
{
    public:
    virtual void ThreadProc( void* context ) = 0;
    virtual gint32 GetLoadCount() const = 0;
    virtual bool IsRunning() const = 0;
    virtual gint32 SetThreadName(
        const char* szName = nullptr );
    virtual void Join() = 0;
};

class IoRequestPacket;
typedef IoRequestPacket  IRP, *PIRP;

template<class Mutex_>
class CMutex
{

    protected:
    Mutex_ *m_pMutex;
    bool    m_bLocked;

    public:

    CMutex()
    {
        m_pMutex = nullptr;
        m_bLocked = false;
    }

    CMutex( Mutex_& oMutex )
    {
        m_pMutex = &oMutex;
        Lock();
    }

    CMutex( Mutex_& oMutex, bool bTryLock )
    {
        m_pMutex = &oMutex;
        if( !bTryLock )
        {
            Lock();
        }
        else
        {
            m_bLocked = m_pMutex->try_lock();
        }
    }

    ~CMutex()
    {
        if( IsLocked() )
            m_pMutex->unlock();
    }

    inline void Lock()
    {
        m_pMutex->lock();
        m_bLocked = true;
    }
    inline void Unlock()
    {
        m_bLocked = false;
        m_pMutex->unlock();
    }

    private:
    inline bool IsLocked() const
    {
        return m_bLocked;
    }
};

typedef std::mutex stdmutex;
typedef std::recursive_mutex stdrmutex;
typedef std::recursive_timed_mutex stdrtmutex;
typedef CMutex<stdmutex> CStdMutex;
typedef CMutex<stdrmutex> CStdRMutex;
class  CStdRTMutex :
    public CMutex<stdrtmutex> 
{
    public:
    CStdRTMutex( stdrtmutex& oMutex );
    CStdRTMutex( stdrtmutex& oMutex, bool bTryLock );
    CStdRTMutex( stdrtmutex& oMutex, guint32 dwTryTimeMs );
};

}
