/*
 * =====================================================================================
 *
 *       Filename:  proxy.h
 *
 *    Description:  the proxy interface for dbus communication
 *
 *        Version:  1.0
 *        Created:  02/27/2016 11:06:12 PM
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

#include <sys/time.h>
#include <tuple>
#include <deque>
#include "defines.h"
#include "utils.h"
#include "configdb.h"
#include "msgmatch.h"
#include "ifstat.h"
#include "reqopen.h"

namespace rpcf
{
#define ICT_MAX_IRPS 100
#define ICT_MAX_IFS 10

#define IFSTATE_OPENPORT_RETRIES        3
#define IFSTATE_OPENPORT_INTERVAL       120
#define IFSTATE_ENABLE_EVENT_TIMEOUT    IFSTATE_OPENPORT_INTERVAL
#define IFSTATE_DEFAULT_IOREQ_TIMEOUT   IFSTATE_OPENPORT_INTERVAL

class IGenericInterface :
    public IServiceEx
{
    public:

    typedef IServiceEx super;

    virtual EnumIfState GetState() const = 0;
    virtual bool IsServer() const = 0;

};

typedef enum {
    stratIgnore,
    stratStop
}EnumStrategy;

struct IMessageFilter
{
/**
* @name FilterMsgIncoming: to filter the incoming
* messages before being invoked
* @{ */
/**
 * Parameter:
 *
 *      pInvTask: current invoke task.
 *
 *      pMsg: the message to filter. There are two
 *      formats of incoming messages, DBusMessage and
 *      IConfigDb.
 *
 * Return Value:
 *      STATUS_SUCCESS: the message can move on
 *
 *      ERROR_PREMATURE: the message cannot move
 *          forward. error will be returned.
 *
 *      STATUS_PENDING: The filter cannot finish the
 *      task immediately. The invoke task will resume
 *      after the filter has finished its work
 *      asynchronously. on complete, the invoke task
 *      will be resumed by the event `eventFilter' with
 *      the return value.
 *
 *      Other Errors: depends on the strategy to know
 *      if the message can be invoked or not
 *      .
 * @} */

    virtual gint32 FilterMsgIncoming(
        IEventSink* pInvTask, DBusMessage* pMsg ) = 0;

    virtual gint32 FilterMsgIncoming(
        IEventSink* pInvTask, IConfigDb* pMsg ) = 0;

/**
* @name GetFilterStrategy: to query the strategy of the
* filter when error happens
* @{ */
/**
 * Parameter:
 *
 *      pCallback: current task to handle the message,
 *      could be CIfIoRequest or CIfInvokeMethodTask
 *
 *      pMsg: the message to filter. There are two
 *      formats of outgoing messages, DBusMessage and
 *      IConfigDb.
 *
 * Return Value:
 *
 *      stratStop: don't move forward to next filter,
 *      neither invoke the message.
 *
 *      stratIgnore: continue to the next filter, or
 *      inoke the message immediately.
 *
 * @} */
    virtual gint32 GetFilterStrategy(
        IEventSink* pCallback,
        DBusMessage* pMsg,
        EnumStrategy& iStrategy ) = 0;

    virtual gint32 GetFilterStrategy(
        IEventSink* pCallback,
        IConfigDb* pMsg,
        EnumStrategy& iStrategy ) = 0;

/**
* @name FilterMsgOutgoing: to filter the messages
* before being sent out
* @{ */
/**
 * Parameter:
 *
 *      pReqTask: current CIfIoRequest task
 *
 *      pMsg: the message to filter. There is just one
 *      format of outgoing messages, DBusMessage and
 *      IConfigDb.
 *
 * Return Value:
 *      STATUS_SUCCESS: the message can move on
 *
 *      ERROR_PREMATURE: the message cannot move
 *          forward. error will be returned.
 *
 *      STATUS_PENDING: The filter cannot finish the
 *      task immediately. The invoke task will resume
 *      after the filter has finished its work
 *      asynchronously. on complete, the invoke task
 *      will be resumed by the event `eventFilter' with
 *      the return value.
 *
 *      Other Errors: depends on the strategy to know
 *      if the message can be invoked or not
 *      .
 * @} */
    virtual gint32 FilterMsgOutgoing(
        IEventSink* pReqTask, IConfigDb* pMsg ) = 0;

    virtual gint32 FilterMsgOutgoing(
        IEventSink* pReqTask, DBusMessage* pMsg ) = 0;
};

typedef CAutoPtr< clsid( Invalid ), IGenericInterface > InterfPtr;

class IInterfaceCommands
{
    public:

    // pCallback is the callback if STATUS_PENDING
    // is returned
    //
    // Start and Stop will come from IServiceEx
    virtual gint32 Pause(
        IEventSink* pCallback = nullptr ) = 0;

    virtual gint32 Resume(
        IEventSink* pCallback = nullptr ) = 0;

    virtual gint32 Shutdown(
        IEventSink* pCallback = nullptr ) = 0;

    virtual gint32 Restart(
        IEventSink* pCallback = nullptr ) = 0;
};

class CGenericInterface :
    public IGenericInterface
{
    mutable stdrmutex       m_oLock;

    protected:
    IfStatPtr               m_pIfStat;

    public:

    typedef IGenericInterface   super;

    inline stdrmutex& GetLock() const
    { return m_oLock; }

    inline CIoManager* GetIoMgr() const
    {
        return ( ( CInterfaceState* )m_pIfStat )->GetIoMgr();
    }

    CGenericInterface( const IConfigDb* pCfg );

    virtual gint32 GetProperty(
        gint32 iProp, CBuffer& pVal ) const
    {
        return m_pIfStat->GetProperty( iProp, pVal );
    }

    virtual gint32 SetProperty(
        gint32 iProp, const CBuffer& pVal )
    {
        return m_pIfStat->SetProperty( iProp, pVal );
    }

    gint32 RemoveProperty( gint32 iProp )
    {
        return m_pIfStat->RemoveProperty( iProp );
    }

    gint32 EnumProperties(
        std::vector< gint32 >& vecProps ) const
    {
        return m_pIfStat->EnumProperties( vecProps );
    }

    gint32 CopyProp( gint32 iProp, CObjBase* pObj )
    {
        return m_pIfStat->CopyProp( iProp, pObj );
    }

    gint32 SetStateOnEvent( EnumEventId iEvent )
    {
        return m_pIfStat->SetStateOnEvent( iEvent );
    }

    gint32 TestSetState( EnumEventId iEvent )
    {
        return m_pIfStat->TestSetState( iEvent );
    }

    virtual EnumIfState GetState() const
    {
        return m_pIfStat->GetState();
    }

    inline HANDLE GetPortHandle() const
    {
        return m_pIfStat->GetHandle();
    }

    PortPtr GetPort() const;

    // Set the default request timeout for an
    // interface . The outgoing requests with
    // reply afterwards will all follow this
    // timeout value.
    gint32 SetTimeout( gint32 iTimeoutSec )
    {
        CCfgOpenerObj oCfg( ( CObjBase* )m_pIfStat );
        return oCfg.SetIntProp(
            propTimeoutSec, iTimeoutSec );
    }

    gint32 GetTimeout( gint32& iTimeoutSec )
    {
        CCfgOpenerObj oCfg( ( CObjBase* )m_pIfStat );
        return oCfg.GetIntProp( propTimeoutSec,
            ( guint32& )iTimeoutSec );
    }

    gint32 SetKeepAlive( gint32 iTimeoutSec  )
    {
        CCfgOpenerObj oCfg( ( CObjBase* )m_pIfStat );
        return oCfg.SetIntProp(
            propKeepAliveSec, iTimeoutSec );
    }

    gint32 GetKeepAlive( gint32& iTimeoutSec )
    {
        CCfgOpenerObj oCfg( ( CObjBase* )m_pIfStat );
        gint32 ret = oCfg.GetIntProp(
            propKeepAliveSec,
            ( guint32& )iTimeoutSec );

        if( SUCCEEDED( ret ) )
            return 0;

        return ERROR_FALSE;
    }

};

class CRpcBaseOperations :
    public CGenericInterface
{
    protected:

    // the default message match to register with
    // the port stack
    MatchPtr        m_pIfMatch;

    // the listening irp
    IrpPtr          m_pIrp;

    gint32 EnableEventInternal(
        IMessageMatch* pMatch,
        bool bEnable,
        IEventSink* pCompletion );

    gint32 PauseResumeInternal(
        EnumEventId iEvent,
        IEventSink* pCallback );

    gint32 AddPortProps();

    public:

    typedef CGenericInterface super;
    CRpcBaseOperations( const IConfigDb* pCfg )
        : super( pCfg )
    {;}

    virtual gint32 OpenPort(
        IEventSink* pCallback );

    virtual gint32 ClosePort(
        IEventSink* pCallback = nullptr );

    // start listening the event from the
    // interface server
    virtual gint32 EnableEvent(
        IMessageMatch* pMatch,
        IEventSink* pCompletion );

    // stop listening the event from the interface
    // server
    virtual gint32 DisableEvent(
        IMessageMatch* pMatch,
        IEventSink* pCompletion );

    virtual gint32 StartRecvMsg(
        IEventSink* pCompletion,
        IMessageMatch* pMatch );

    gint32 CancelRecvMsg();

    virtual gint32 DoPause(
        IEventSink* pCallback = nullptr );

    virtual gint32 DoResume(
        IEventSink* pCallback = nullptr );

    virtual gint32 DoModEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    virtual gint32 DoRmtModEvent(
        EnumEventId iEvent,
        const std::string& strModule,
        IConfigDb* pEvtCtx );

    virtual gint32 SetupReqIrp( IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback ) = 0;

    virtual gint32 BuildBufForIrp(
        BufPtr& pBuf,
        IConfigDb* pReqCall ) = 0;

    virtual bool IsConnected(
        const char* szDestAddr = nullptr )
    {
        return GetState() == stateConnected;
    }

    virtual gint32 GetPortToSubmit(
        CObjBase* pCfg,
        PortPtr& pPort,
        bool& bPdo );

    gint32 GetPortToSubmit(
        CObjBase* pCfg,
        PortPtr& pPort );
};

}

#include "iftasks.h"

namespace rpcf
{
/**
* @name CRpcInterfaceBase class will provide the
* the interface state mangement and task
* serializations related to state changing.
* it will interface with the events from the
* connection point and the request from client
* requests.
* 
* @{ */
/**  @} */

class CRpcInterfaceBase :
    public CRpcBaseOperations,
    public IInterfaceEvents,
    public IInterfaceCommands
{


    gint32 PauseResume( bool bResume,
        IEventSink* pCallback );

    // multiple interface support
    protected:
    std::vector< MatchPtr > m_vecMatches;
    TaskGrpPtr              m_pRootTaskGroup;

    gint32 SetReqQueSize(
        IMessageMatch* pMatch, guint32 dwSize );

    gint32 GetParallelGrp(
        TaskGrpPtr& pParaGrp );

    public:

    typedef CRpcBaseOperations super;

    CRpcInterfaceBase( const IConfigDb* pCfg )
        :super( pCfg )
    {;}

    gint32 ClearActiveTasks();
    gint32 ClearPausedTasks();

    TaskGrpPtr& GetTaskGroup()
    { return m_pRootTaskGroup; }

    bool IsMyPort( HANDLE hPort )
    {
        return static_cast< CInterfaceState* >
            ( m_pIfStat )->IsMyPort( hPort );
    }

    bool IsMyDest( const std::string& strMod )
    {
        return static_cast< CInterfaceState* >
            ( m_pIfStat )->IsMyDest( strMod );
    }

    // events from the connection points
    virtual gint32 OnPortEvent(
        EnumEventId iEvent,
        HANDLE hPort );

    virtual gint32 OnModEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    virtual gint32 OnDBusEvent(
        EnumEventId iEvent );

    virtual gint32 OnRmtModEvent(
        EnumEventId iEvent,
        const std::string& strModule,
        IConfigDb* pEvtCtx );

    virtual gint32 OnRmtSvrEvent(
        EnumEventId iEvent,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    virtual gint32 OnAdminEvent(
        EnumEventId iEvent,
        const std::string& strParams );

    // IServiceEx methods
    // start the proxy, called by the interface
    // owner
    virtual gint32 Start();
    virtual gint32 StartEx( IEventSink* pCallback );

    gint32 StartRecvTasks(
        std::vector< MatchPtr >& vecMatches );

    // stop the proxy, called by the interface
    // owner
    // synchronous version of stop
    virtual gint32 Stop();

    // asynchronous stop
    virtual gint32 StopEx( IEventSink* pCallback );
    // two stage stop: PreStop and DoStop
    virtual gint32 OnPreStop( IEventSink* pCallback );
    // fixed part of stop
    gint32 DoStop( IEventSink* pCallback );


    virtual gint32 Pause(
        IEventSink* pCallback = nullptr );

    virtual gint32 Resume(
        IEventSink* pCallback = nullptr );

    virtual gint32 Shutdown(
        IEventSink* pCallback = nullptr );

    virtual gint32 Restart(
        IEventSink* pCallback = nullptr )
    { return -ENOTSUP; }

    gint32 AppendAndRun( TaskletPtr& pTask );

    virtual gint32 AddAndRun(
        TaskletPtr& pParallelTask,
        bool bImmediate = false );

    virtual gint32 OnEvent( EnumEventId iEvent,
        LONGWORD dwParam1 = 0,
        LONGWORD dwParam2 = 0,
        LONGWORD* pData = NULL  );

    virtual gint32 AddStartTasks(
        IEventSink* pTaskGrp ) = 0;

    virtual gint32 AddStopTasks(
        IEventSink* pTaskGrp ) = 0;
};

struct IRpcNonDBusIf
{
    virtual gint32 DoInvoke(
        IConfigDb* pReqMsg,
        IEventSink* pCallback = nullptr ) = 0;

    virtual gint32 OnCancel(
        IConfigDb* pReqCall,
        IEventSink* pCallback = nullptr )
    { return -ENOTSUP; }

    virtual gint32 SendResponse(
        IEventSink* pInvTask,
        IConfigDb* pReqMsg,
        CfgPtr& pRespData ) = 0;

};

typedef std::map< std::string, TaskletPtr > FUNC_MAP;
typedef std::map< std::string, ObjPtr > PROXY_MAP;


std::string CoGetIfNameFromIid(
    EnumClsid Iid, const std::string& strSuffix );

EnumClsid CoGetIidFromIfName(
    const std::string& strName,
    const std::string& strSuffix
    );

gint32 CoAddIidName(
    const std::string& strName,
    EnumClsid Iid,
    const std::string& strSuffix
    );

// server side timestamp
struct CTimestampSvr
{
    guint64 m_qwLocalBaseSec;

    CTimestampSvr()
    {}
    CTimestampSvr( guint64 qwLocalTs )
    { m_qwLocalBaseSec = qwLocalTs; }

    void SetBase( guint64 qwTs )
    { m_qwLocalBaseSec = qwTs; }

    static guint64 GetAgeSec(
        guint64 qwTimestampSec )
    {
        timespec tv;
        clock_gettime( CLOCK_MONOTONIC, &tv );
        return ( ( guint64 )tv.tv_sec -
            qwTimestampSec );
    }
};

// proxy side timestamp
struct CTimestampProxy :
    public CTimestampSvr
{
    guint64 m_qwPeerBaseSec;
    CTimestampProxy() :
        CTimestampSvr() 
    {}

    CTimestampProxy(
        guint64 qwLocalTs, guint64 qwPeerTs ) :
        CTimestampSvr( qwLocalTs ),
        m_qwPeerBaseSec( qwPeerTs )
    {}

    void SetPeer( guint64 qwTs )
    { m_qwPeerBaseSec = qwTs; }

    // for relay
    guint64 GetPeerTimestamp(
        guint64 qwTimestampSec ) const
    {
        return m_qwPeerBaseSec +
            ( qwTimestampSec - m_qwLocalBaseSec );
    }

    // for start point in reqfwdr
    guint64 GetPeerTimestamp() const
    {
        timespec tv;
        clock_gettime( CLOCK_MONOTONIC, &tv );
        return GetPeerTimestamp( tv.tv_sec );
    }
};

/**
* @name CRpcServices as an interface to provide
* the transport layer interface for an rpc
* connection.
* @{ */
/**  @} */
class CRpcServices : 
    public CRpcInterfaceBase,
    public IRpcNonDBusIf
{
    std::deque< CfgPtr > m_queEvents;

    gint32 PackEvent( EnumEventId iEvent,
        LONGWORD dwParam1,
        LONGWORD dwParam2,
        LONGWORD* pData,
        CfgPtr& pDestCfg  );

    gint32 UnpackEvent( CfgPtr& pSrcCfg,
        EnumEventId& iEvent,
        LONGWORD& dwParam1,
        LONGWORD& dwParam2,
        LONGWORD*& pData );


    protected:

    // message filters
    std::deque< TaskletPtr > m_queFilters;

    std::map< gint32, PROXY_MAP > m_mapProxyFuncs;
    std::map< gint32, FUNC_MAP > m_mapFuncs;

    // message filter for file transfer 
    MatchPtr    m_pFtsMatch;
    // message filter for stream
    MatchPtr    m_pStmMatch;

    // the queue of pending invoke tasks, for queued
    // task processing
    TaskGrpPtr             m_pSeqTasks;

    virtual gint32 SetupReqIrp( IRP* pIrp,
        IConfigDb* pReqCall, IEventSink* pCallback );

    gint32 BuildBufForIrpIfMethod(
        BufPtr& pBuf,
        IConfigDb* pReqCall );

    virtual gint32 BuildBufForIrp(
        BufPtr& pBuf,
        IConfigDb* pReqCall );

    gint32 FillRespData(
        DMsgPtr& pMsg, CfgPtr& pResp );

    gint32 SetFuncMap( FUNC_MAP& oMap,
        EnumClsid iIfId = clsid( Invalid ) );

    FUNC_MAP* GetFuncMap(
        EnumClsid iIfId = clsid( Invalid ) );

    virtual gint32 RestartListening(
        EnumIfState iState ) = 0;

    virtual gint32 RebuildMatches();

    public:

    virtual gint32 StartEx2(
        IEventSink* pCallback );

    PROXY_MAP* GetProxyMap(
        EnumClsid iIfId = clsid( Invalid ) );

    gint32 SetProxyMap( PROXY_MAP& oMap,
        EnumClsid iIfId = clsid( Invalid ) );

    gint32 GetDelegate(
        const std::string& strFunc,
        TaskletPtr& pFunc,
        EnumClsid iIfId = clsid( Invalid ) );

    gint32 GetProxy(
        const std::string& strFunc,
        ObjPtr& pProxy,
        EnumClsid iIfId = clsid( Invalid ) );

    typedef CRpcInterfaceBase super;

    // bring overloaded methods from base class to
    // current scope
    using IRpcNonDBusIf::DoInvoke;
    using IRpcNonDBusIf::OnCancel;
    using IRpcNonDBusIf::SendResponse;
/**
* @name InitUserFuncs

        // InitUserFuncs function should be called
        // from the topmost level of your
        // interface's constructor
        //
* @{ */
/**  
        // the content for the server interface
        // should be like the following:
        //
        // call the base class's InitUserFuncs
        super::InitUserFuncs();
        // declare your server side methods you
        // want to expose for remote users to call
    
        BEGIN_HANDLER_MAP;
        
        ADD_SERVICE_HANDLER(
            CInterfaceServer::UserCancelRequest,
            SYS_METHOD_USERCANCELREQ );
    
        END_HANDLER_MAP;
    
        // for proxy interface, should be like the
        // following:
    
        super::InitUserFuncs();
        // declare your proxy side event handlers
        // which will handle the incoming events
        // from the server
        BEGIN_HANDLER_MAP;

        ADD_EVENT_HANDLER(
            CRpcTcpBridgeProxy::OnRmtSvrOffline,
            BRIDGE_EVENT_SVROFF );

        ADD_EVENT_HANDLER(
            CRpcTcpBridgeProxy::OnRmtModOffline,
            BRIDGE_EVENT_MODOFF );

        ADD_EVENT_HANDLER(
            CRpcTcpBridgeProxy::OnInvalidStreamId,
            BRIDGE_EVENT_INVALIDSTM );

        END_HANDLER_MAP;

        // declare your proxy side method calls,
        // which will be forward to the server
        // side
        BEGIN_PROXY_MAP( false );

        ADD_PROXY_METHOD( SYS_METHOD_USERCANCELREQ, guint64 );

        END_PROXY_MAP
 @} */

    virtual gint32 InitUserFuncs() 
    { return 0; }

    virtual gint32 StartEx(
        IEventSink* pCallback );

    CRpcServices( const IConfigDb* pCfg );

    virtual gint32 SendData(
        IConfigDb* pDataDesc,           // [in]
        gint32 fd,                      // [in]
        guint32 dwOffset,               // [in]
        guint32 dwSize,                 // [in]
        IEventSink* pCallback = nullptr ) = 0;

    // caller provide the following info in the
    //
    // pDataDesc: at least file name. and
    // operation commands if necessary
    //
    // pDataDesc, before going into the port
    // stack, includes information for
    // DBusMessage:
    //
    // propIfName
    // propObjPath
    // propDestDBusName
    // propSrcDBusName
    //
    // They are provided by the underlying
    // interface support methods.

    virtual gint32 FetchData(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [in,out]
        guint32& dwSize,                // [in,out]
        IEventSink* pCallback = nullptr ) = 0;

    // The method will allocate the irp and send
    // it down to the port stack from user thread.
    // If it is a synchronous call, it will wait
    // for the response till it is completed. For
    // an asyn call, the callback will be called
    // by from within a worker thread.
    // note that: both proxy and server stub needs
    // this methods
    virtual gint32 SendMethodCall(
        IConfigDb* pReqCall,
        CfgPtr& pRespData,
        IEventSink* pCallback = nullptr );

    // invoke an event henadler on the subclass if
    // event is enabled. called by OnEvent on the
    // async thread
    //
    // Interface properties for InvokeMethod:
    //
    // propQueuedReq: true if the incoming
    // requests will be serviced one after the
    // other.  Otherwise, the requests are
    // serviced concurrently, which means the
    // client code should handle the concurrency
    // and reentrancy on its own
    //
    // we have two version of InvokeMethod for
    // dbus and Non-dbus invoke of the method
    // non-dbus is currently existing only between
    // CRpcTcpBridgeProxy and CRpcTcpBridge
    //
    template< typename T1, 
        typename T = typename std::enable_if<
            std::is_same<T1, DBusMessage>::value ||
            std::is_same<T1, IConfigDb>::value, T1 >::type >
    gint32 InvokeMethod( T* pReqMsg,
        IEventSink* pCallback = nullptr );

    virtual gint32 DoInvoke(
        DBusMessage* pReqMsg,
        IEventSink* pCallback = nullptr ) = 0;

    virtual gint32 InvokeUserMethod(
        IConfigDb* pParams,
        IEventSink* pCallback = nullptr );

    // a method for user to override on method
    // canceling
    virtual gint32 OnCancel(
        DBusMessage* pReqMsg,
        IEventSink* pCallback = nullptr )
    { return 0; }

    gint32 GetIidFromIfName(
        const std::string& strIfName,
        EnumClsid& iid );

    // put the request to the task queue and run
    // it immediately if possible
    // main entry from user code into a proxy/stub
    // context
    gint32 RunIoTask(
        IConfigDb* pReqCall,
        CfgPtr& pRespData,
        IEventSink* pCallback,
        guint64* pTaskId = nullptr );

    virtual gint32 FillRespData(
        IRP* pIrp, CfgPtr& pResp );

    virtual gint32 SendResponse(
        IEventSink* pInvTask,
        DBusMessage* pReqMsg,
        CfgPtr& pRespData ) = 0;

    enum EnumKAPhase{
        KAOrigin, // the KA relay start point
        KARelay,
        KATerm, // the last KA relay node
    };

    virtual gint32 OnKeepAlive(
        IEventSink* pInvokeTask,
        EnumKAPhase bOrigin ) = 0;

    static gint32 LoadObjDesc(
        const std::string& strFile,
        const std::string& strObjName,
        bool bServer,
        CfgPtr& pCfg );

    virtual gint32 OnEvent( EnumEventId iEvent,
        LONGWORD dwParam1 = 0,
        LONGWORD dwParam2 = 0,
        LONGWORD* pData = NULL  );

    virtual gint32 DoModEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    virtual gint32 DoRmtModEvent(
        EnumEventId iEvent,
        const std::string& strModule,
        IConfigDb* pEvtCtx );

    virtual gint32 OnPostStop(
        IEventSink* pCallback );

    virtual gint32 OnPreStart(
        IEventSink* pCallback );

    virtual gint32 OnPostStart(
        IEventSink* pCallback );

    virtual gint32 CustomizeRequest(
        IConfigDb* pReqCfg,
        IEventSink* pCallback ) = 0;

    // this method is a point to insert tasks that
    // need to run before the interface to run in
    // full swing. They are run sequencially. the
    // `pTaskGrp' is a pointer to CIfTaskGroup.
    virtual gint32 AddStartTasks(
        IEventSink* pTaskGrp )
    { return 0; }

    virtual gint32 AddStopTasks(
        IEventSink* pTaskGrp )
    { return 0; }

    // a helper for deferred task to run in the
    // interface's taskgroup
    gint32 RunManagedTask(
        IEventSink* pTask,
        const bool& bRoot = false );

    gint32 AddSeqTaskInternal(
        TaskGrpPtr& pTaskGrp,
        TaskletPtr& pTask,
        bool bLong );

    gint32 AddSeqTask(
        TaskletPtr& pTask,
        bool bLong = false );

    gint32 RegisterFilter(
        IEventSink* pFilterTask )
    {
        CStdRMutex oIfLock( GetLock() );

        CTasklet* pTask = static_cast
            < CTasklet* >( pFilterTask );

        m_queFilters.push_back( TaskletPtr( pTask ) );
        return 0;
    }

    gint32 UnregisterFilter(
        IEventSink* pFilterTask )
    {
        gint32 ret = -ENOENT;
        CStdRMutex oIfLock( GetLock() );

        CTasklet* pTask = static_cast
            < CTasklet* >( pFilterTask );

        std::deque< TaskletPtr >::iterator itr =
            m_queFilters.begin();

        TaskletPtr ptrTask( pTask );
        for( ; itr < m_queFilters.end(); ++itr )
        {
            if( *itr == ptrTask )
            {
                m_queFilters.erase( itr );
                ret = 0;
                break;
            }
        }
        return ret;
    }

    gint32 GetFilters(
        std::deque< TaskletPtr >& oFilters )
    {
        CStdRMutex oIfLock( GetLock() );
        oFilters = m_queFilters;
        return 0;
    }

    virtual gint32 FilterMessage(
        IEventSink* pTask,
        DBusMessage* pMsg,
        bool bIncoming,
        IEventSink* pFilter = nullptr )
    {
        return FilterMessageInternal( pTask,
            pMsg, bIncoming, pFilter );
    }

    virtual gint32 FilterMessage(
        IEventSink* pTask,
        IConfigDb* pMsg,
        bool bIncoming,
        IEventSink* pFilter = nullptr )
    {
        return FilterMessageInternal( pTask,
            pMsg, bIncoming, pFilter );
    }

    gint32 SetPortProp(
        IPort* pPort,
        guint32 dwPropId,
        BufPtr& pVal );

    gint32 GetPortProp(
        IPort* pPort,
        guint32 dwPropId,
        BufPtr& pVal );

    private:
    template< class T >
    gint32 FilterMessageInternal(
        IEventSink* pTask,
        T* pMsg,
        bool bIncoming,
        IEventSink* pFilter = nullptr )
    {
        gint32 ret = 0;

        std::deque< TaskletPtr > oFilters;
        GetFilters( oFilters );
        std::deque< TaskletPtr >::iterator
            itr = oFilters.begin();

        if( pFilter != nullptr )
        {
            CTasklet* pFiltTask = ( CTasklet* )pFilter;
            TaskletPtr pLastFilt( pFiltTask );
            bool bFound = false;
            for( ;itr!= oFilters.end(); ++itr )
            {
                if( *itr == pLastFilt )
                {
                    bFound = true;
                    break;
                }
            }

            // already the last one
            if( !bFound )
                return STATUS_SUCCESS;
            // skip this one
            ++itr;
        }

        for( ; itr != oFilters.end(); ++itr )
        {
            IMessageFilter* pFilter =
                dynamic_cast< IMessageFilter* >(
                    ( CTasklet* ) *itr );

            if( pFilter == nullptr )
                continue;

            if( bIncoming )
            {
                ret = pFilter->FilterMsgIncoming(
                    pTask, pMsg );
            }
            else
            {
                ret = pFilter->FilterMsgOutgoing(
                    pTask, pMsg );
            }

            if( SUCCEEDED( ret ) )
                continue;

            if( ret == ERROR_PREMATURE )
                break;

            if( ERROR( ret ) )
            {
                EnumStrategy iStrategy;

                ret = pFilter->GetFilterStrategy(
                    pTask, pMsg, iStrategy );

                // stop processing
                if( ERROR( ret ) )
                {
                    ret = ERROR_PREMATURE;
                    break;
                }

                if( iStrategy == stratStop )
                {
                    ret = ERROR_PREMATURE;
                    break;
                }
                else
                {
                    ret = 0;
                    continue;
                }
            }
        }

        return ret;
    }
};

template< typename ...Args>
class CMethodProxy;

class CSyncCallback;

#define DecType( _T ) typename std::decay< _T >::type

template< int iNumInput >
struct InputCount
{
    CfgPtr m_pOptions;
    InputCount( IConfigDb* pCfg )
    {
        if( pCfg != nullptr )
            m_pOptions = pCfg;
    }
};
#define _N( _iCount ) ( ( InputCount< _iCount >* )0 )

class CInterfaceProxy :
    public CRpcServices
{
    protected:

    // invoke an event henadler on the subclass if
    // event is enabled. called by OnEvent on the
    // async thread
    virtual gint32 DoInvoke(
        DBusMessage* pEvtMsg,
        IEventSink* pCallback = nullptr );

    virtual gint32 DoInvoke(
        IConfigDb* pEvtMsg,
        IEventSink* pCallback );

    virtual gint32 OnCancel(
        IConfigDb* pReqCall,
        IEventSink* pCallback = nullptr )
    {
        return OnCancel_Proxy(
            pReqCall, pCallback );
    }

    gint32 OnKeepAliveTerm(
        IEventSink* pInvokeTask );

    virtual gint32 SendFetch_Proxy(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [out]
        guint32& dwSize,                // [out]
        IEventSink* pCallback );

    gint32 PauseResume_Proxy(
        const std::string& strMethod );

    gint32 RestartListening(
        EnumIfState iStateOld  );

    public:

    typedef CRpcServices super;

    CInterfaceProxy( const IConfigDb* pCfg );
    ~CInterfaceProxy();

    bool IsServer() const
    { return false; }

    virtual gint32 SendData(
        IConfigDb* pDataDesc,           // [in]
        gint32 fd,                      // [in]
        guint32 dwOffset,               // [in]
        guint32 dwSize,                 // [in]
        IEventSink* pCallback = nullptr )
    {
        if( pDataDesc == nullptr )
            return -EINVAL;

        return SendData_Proxy( pDataDesc,
            fd, dwOffset, dwSize, pCallback );
    }

    virtual gint32 FetchData(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [in,out]
        guint32& dwSize,                // [in,out]
        IEventSink* pCallback = nullptr )
    {
        if( pDataDesc == nullptr )
            return -EINVAL;

        return FetchData_Proxy( pDataDesc,
            fd, dwOffset, dwSize, pCallback );
    }

    virtual gint32 OnCancel_Proxy(
        IConfigDb* pReqCall,
        IEventSink* pCallback = nullptr );


    virtual gint32 SendData_Proxy(
        IConfigDb* pDataDesc,           // [in]
        gint32 fd,                      // [in]
        guint32 dwOffset,               // [in]
        guint32 dwSize,                 // [in]
        IEventSink* pCallback = nullptr );

    virtual gint32 FetchData_Proxy(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [out]
        guint32& dwSize,                // [out]
        IEventSink* pCallback = nullptr );

    virtual gint32 SendResponse(
        IEventSink* pInvTask,
        DBusMessage* pReqMsg,
        CfgPtr& pRespData );

    virtual gint32 SendResponse(
        IEventSink* pInvTask,
        IConfigDb* pReqMsg,
        CfgPtr& pRespData );

    virtual gint32 OnKeepAlive(
        IEventSink* pInvokeTask,
        EnumKAPhase bTarget );

    virtual gint32 SendProxyReq(
        IEventSink* pCallback,
        bool bNonDBus,
        const std::string& strMethod,
        std::vector< BufPtr >& vecParams,
        guint64& qwIoTaskId );

    virtual gint32 CustomizeRequest(
        IConfigDb* pReqCfg,
        IEventSink* pCallback );

    virtual gint32 CustomizeSendFetch(
        IConfigDb* pReqCfg,
        IEventSink* pCallback );

    template< typename ...Args >
    gint32 CallProxyFunc(
        IEventSink* pCallback,
        guint64& qwIoTaskId,
        const std::string& strMethod,
        Args&&... args )
    {
        ObjPtr pObj;

        CCfgOpenerObj oCfg( pCallback );
        std::string strIfName;

        gint32 ret = oCfg.GetStrProp(
            propIfName, strIfName );

        if( ERROR( ret ) ) 
            return ret;

        strIfName = IF_NAME_FROM_DBUS( strIfName );
        EnumClsid iid =
            CoGetIidFromIfName( strIfName, "p" );

        ret = GetProxy( strMethod, pObj, iid );
        if( ERROR( ret ) )
            return ret;

        CMethodProxy< DecType( Args )...>* pProxy = pObj;
        if( pProxy == nullptr )
            return -EFAULT;

        return ( *pProxy )( this, pCallback,
            qwIoTaskId, args... );
    }

    virtual gint32 InitUserFuncs();

    gint32 KeepAliveRequest(
        guint64 qwTaskToCancel );

    // a user-initialized cancel request
    gint32 UserCancelRequest(
        guint64& qwThisTaksId,
        guint64 qwTaskToCancel );

    // a helper to use UserCancelRequest
    // it is a synchronous call
    gint32 CancelRequest(
        guint64 qwTaskId );

    gint32 Pause_Proxy();
    gint32 Resume_Proxy();

    template< typename ...Args >
    gint32 AsyncCall(
        IEventSink* pCallback,
        CfgPtr& pOptions,
        CfgPtr& pResp,
        const std::string& strMethod,
        Args&&... args );

    template< typename ...Args >
    gint32 SyncCallEx(
        CfgPtr& pOptions,
        CfgPtr& pResp,
        const std::string& strMethod,
        Args&&... args );

    template< typename ...Args >
    gint32 SyncCall(
        CfgPtr& pResp,
        const std::string& strMethod,
        Args&&... args )
    {
        CParamList oOptions;
        const std::string& strIfName =
            CoGetIfNameFromIid( GetClsid(), "p" );

        oOptions[ propIfName ] =
            DBUS_IF_NAME( strIfName );

        return SyncCallEx( oOptions.GetCfg(),
            pResp, strMethod, args... );
    }
    // FillArgs assumes that the response package
    // contains at least a propReturnValue property. if
    // the return value is zero, the property 0..n,
    // contains the other returned information.
    template< typename ...Args >
    gint32 FillArgs( CfgPtr& pResp,
        gint32& iRet, Args&&... args );

    template< int iNumInput, typename...Args >
    gint32 ProxyCall(
        InputCount< iNumInput >* p,
        const std::string& strMethod,
        Args&&... args );

    gint32 CopyUserOptions(
        CObjBase* pDest,
        CObjBase* pSrc );
};

class CInterfaceServer;
class CIfSvrConnMgr : public CObjBase
{
    std::map< HANDLE, std::string >
        m_mapTaskToAddr;

    std::map< HANDLE, bool > m_mapTaskStates;

    std::multimap< std::string, HANDLE >
        m_mapAddrToTask;

    CInterfaceServer* m_pIfSvr;

    typedef std::pair<std::multimap<std::string, HANDLE>::iterator, std::multimap<std::string, HANDLE>::iterator>
        AddrRange;

    public:
    CIfSvrConnMgr( const IConfigDb* pCfg );
    ~CIfSvrConnMgr();

    // a new invoke task comes
    gint32 OnInvokeMethod( HANDLE hTask,
        const std::string& strSrc );

    // the invoke task is done, and can be removed
    gint32 OnInvokeComplete( HANDLE hTask );

    // received a disconnection event
    gint32 OnDisconn( const std::string& strDest );

    // test if the task can send a response to the
    // client
    gint32 CanResponse( HANDLE hTask );
    gint32 CanResponse( const std::string& strSrc );
};

typedef CAutoPtr< clsid( CIfSvrConnMgr ), CIfSvrConnMgr > SvrConnPtr;

class CInterfaceServer :
    public CRpcServices
{

    protected:

    SvrConnPtr m_pConnMgr;

    virtual gint32 SendData_Server(
        IConfigDb* pDataDesc,           // [in]
        gint32 fd,                      // [in]
        guint32 dwOffset,               // [in]
        guint32 dwSize,                 // [in]
        IEventSink* pCallback = nullptr );

    virtual gint32 FetchData_Server(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [out]
        guint32& dwSize,                // [out]
        IEventSink* pCallback = nullptr );

    // invoke a method on subclass via the method
    // name
    virtual gint32 DoInvoke_SendData(
        DBusMessage* pReqMsg,
        IEventSink* pCallback,
        CParamList& oResp );

    virtual gint32 ValidateRequest_SendData(
        DBusMessage* pReqMsg,
        IConfigDb* pDataDesc );

    virtual gint32 DoInvoke(
        DBusMessage* pReqMsg,
        IEventSink* pCallback = nullptr );

    virtual gint32 DoInvoke(
        IConfigDb* pEvtMsg,
        IEventSink* pCallback );

    virtual gint32 CheckReqCtx(
        IEventSink* pCallback,
        DMsgPtr& pReqMsg );

    // OnCancel is called when the request will be
    // canceled due to some reasons. This call
    // should only be effective when it is an
    // asynchronous service of message
    virtual gint32 OnCancel(
        DBusMessage* pReqMsg,
        IEventSink* pCallback = nullptr )
    {
        return OnCancel_Server(
            pReqMsg, pCallback );
    }

    gint32 OnKeepAliveOrig(
        IEventSink* pTask );

    gint32 OnKeepAliveRelay(
        IEventSink* pTask );

    virtual gint32 DoPause(
        IEventSink* pCallback = nullptr );

    virtual gint32 DoResume(
        IEventSink* pCallback = nullptr );

    gint32 PauseResume_Server(
        IEventSink* pCallback, bool bPause );

    virtual gint32 RestartListening(
        EnumIfState iState );

    public:

    typedef CRpcServices super;

    CInterfaceServer() = delete;
    CInterfaceServer( const IConfigDb* pCfg );
    ~CInterfaceServer();

    bool IsServer() const
    { return true; }

    virtual bool IsConnected(
        const char* szDestAddr = nullptr );

    bool IsPaused( const std::string& strIfName );
    bool IsPaused( IMessageMatch* pMatch );

    virtual gint32 CustomizeRequest(
        IConfigDb* pReqCfg,
        IEventSink* pCallback )
    { return 0; }

    // called to set the response to send back
    virtual gint32 SetResponse(
        IEventSink* pTask,
        CfgPtr& pRespData );

    virtual gint32 SendResponse(
        IEventSink* pInvTask,
        DBusMessage* pReqMsg,
        CfgPtr& pRespData );

    virtual gint32 SendResponse(
        IEventSink* pInvTask,
        IConfigDb* pReqMsg,
        CfgPtr& pRespData );

    // send a dbus signal, called from the user
    // code
    gint32 BroadcastEvent(
        IConfigDb* pEvent,
        IEventSink* pCallback = nullptr );

    // server side SendData
    virtual gint32 SendData(
        IConfigDb* pDataDesc,           // [in]
        gint32 fd,                      // [in]
        guint32 dwOffset,               // [in]
        guint32 dwSize,                 // [in]
        IEventSink* pCallback = nullptr )
    {
        return SendData_Server( pDataDesc,
            fd, dwOffset, dwSize, pCallback );
    }

    // server side FetchData
    virtual gint32 FetchData(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [in,out]
        guint32& dwSize,                // [in,out]
        IEventSink* pCallback = nullptr )
    {
        return FetchData_Server( pDataDesc,
            fd, dwOffset, dwSize, pCallback );
    }

    virtual gint32 OnCancel_Server(
        DBusMessage* pReqMsg,
        IEventSink* pCallback )
    {
        return super::OnCancel( pReqMsg, pCallback );
    }

    // if the SendData_Server and FetchData_Server
    // return STATUS_PENDING, you should call
    // OnServiceComplete later sometime to
    // complete the task.  This callback is called
    // from user code the pResp point to the
    // response information
    gint32 OnServiceComplete(
        IConfigDb* pResp,
        IEventSink* pCallback );
 
    virtual gint32 OnKeepAlive(
        IEventSink* pInvokeTask,
        EnumKAPhase  bOrigin );


    virtual gint32 InitUserFuncs();

    // a user-initialized cancel request
    gint32 UserCancelRequest(
        IEventSink* pInvokeTask,
        guint64 qwIoTaskId );

    // a user-initialized cancel request
    gint32 ForceCancelRequests(
        IEventSink* pInvokeTask,
        ObjPtr& pvecTasks );

    // a user-initialized keep-alive request
    gint32 KeepAliveRequest(
        IEventSink* pCallback,
        guint64 qwIoTaskId );

    template< int N, typename ...Args >
    struct FillResp
    {
        FillResp( std::tuple<Args...>& oTuple,
            CParamList& oParams )
        {
            FillResp<N-1, Args...> oAv( oTuple, oParams );
            oParams.Push( std::get<N-1>( oTuple ) );
        }
    };

    template< typename ...Args >
    struct FillResp< 1, Args... >
    {
        FillResp( std::tuple<Args...>& oTuple,
            CParamList& oParams )
        {
            oParams.Push( std::get<0>( oTuple ) );
        }
    };

    template< typename ...Args >
    struct FillResp< 0, Args... >
    {
        FillResp( std::tuple<Args...>& oTuple,
            CParamList& oParams )
        {}
    };

    template< typename ...Args >
    gint32 FillAndSetResponse(
        IEventSink* pCallback,
        gint32 iRet,
        Args&&... args )
    {
        gint32 ret = 0;

        do{
            CParamList oResp;

            oResp[ propReturnValue ] = iRet;
            if( SUCCEEDED( iRet ) )
            {
                auto oTuple = std::tuple<Args...>(args...);
                FillResp<sizeof...(Args), Args...> oFr(
                    oTuple, oResp );
            }

            CfgPtr pCfg = oResp.GetCfg();
            ret = SetResponse( pCallback, pCfg );

        }while( 0 );

        return ret;
    }

    template< typename ...Args >
    gint32 SendEvent(
        IEventSink* pCallback,
        EnumClsid iid,
        const std::string& strMethod,
        const std::string& strDest, // optional
        Args&&... args );

    template< typename ...Args >
    gint32 SendEventEx(
        IEventSink* pCallback,
        IConfigDb* pOptions,
        EnumClsid iid,
        const std::string& strMethod,
        const std::string& strDest, // optional
        Args&&... args );

    virtual gint32 OnModEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    virtual gint32 OnPostStop(
        IEventSink* pCallback );

    virtual gint32 OnPreStart(
        IEventSink* pCallback );

    gint32 StartRecvMsg(
        IEventSink* pCompletion,
        IMessageMatch* pMatch );

    inline SvrConnPtr GetConnMgr()
    { return m_pConnMgr; }

    // IInterfaceServer handlers
    gint32 Pause_Server(
        IEventSink* pCallback );

    gint32 Resume_Server(
        IEventSink* pCallback );

};


#define SET_RMT_TASKID( pReq, oTaskCfg ) \
do{ \
    CCfgOpener oReq( pReq ); \
    guint64 qwTaskId; \
    gint32 ret1 = oReq.GetQwordProp( \
        propTaskId, qwTaskId ); \
    if( SUCCEEDED( ret1 ) ) \
    { \
        oTaskCfg.SetQwordProp( \
            propRmtTaskId, qwTaskId ); \
    } \
}while( 0 )

/**
* @name AsyncCall: to make an asynchronous call to the
* server with the arguments in args
* @{ */
/**
 * pTask: the callback context
 * pOptions: the extra options, and currently only
 *      propIfName is inside
 *
 * pResp: the response from the server if this is an
 * immediate successful request. It will contain a
 * propTaskId of the CIfIoRequest this call is carried
 * on if the this method returns STATUS_PENDING
 * instead. A handle as you can use to cancel the
 * request in the future. the content of the response
 * from the server is as follows:
 *
 *      propReturnValue: the return code
 *      0..n : the parameters the server returned if
 *          propReturnValue contains 0. otherwise,
 *          these parameters do not exist.
 *
 * you can use CInterfaceProxy::FillArgs to assign the
 * parameters returned to the individual variable
 * respectively.
 * 
 *
 * strMethod: the name of the method to call on server
 * side. you can determine the method by the InitUserFuncs,
 * the propIfName in pOptions as mentioned above.
 *
 * args... : the arguments that will be sent to the
 * server side.
 *
 * @} */
template< typename ...Args >
gint32 CInterfaceProxy::AsyncCall(
    IEventSink* pTask,
    CfgPtr& pOptions,
    CfgPtr& pResp,
    const std::string& strcMethod,
    Args&&... args )
{
    gint32 ret = 0;

    do{ 
        if( pTask == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        if( pResp.IsEmpty() )
            pResp.NewObj();

        guint64 qwIoTaskId = 0; 

        CopyUserOptions(
            pTask, ( CObjBase* )pOptions );

        CParamList oResp( ( IConfigDb* )pResp );

        std::string strMethod( strcMethod );
        if( !pOptions.IsEmpty() )
        {
            bool bSysMethod = false;
            CCfgOpener oOptions(
                ( IConfigDb* )pOptions );

            ret = oOptions.GetBoolProp(
                propSysMethod, bSysMethod );

            if( SUCCEEDED( ret ) && bSysMethod )
                strMethod = SYS_METHOD( strMethod );
            else
                strMethod = USER_METHOD( strMethod );
        }

        ret = CallProxyFunc( pTask, 
            qwIoTaskId, strMethod, args... ); 

        if( ret == STATUS_PENDING ) 
        {
            // for canceling purpose
            oResp[ propTaskId ] = qwIoTaskId;
            break;
        }

        if( ERROR( ret ) ) 
            break; 

        if( !pOptions.IsEmpty() )
        {
            CCfgOpener oOptions(
                ( IConfigDb* )pOptions );
            bool bNoReply = false;
            gint32 iRet = oOptions.GetBoolProp(
                propNoReply, bNoReply );
            if( SUCCEEDED( iRet ) && bNoReply )
                break;
        }

        ObjPtr pObj; 
        CCfgOpenerObj oTask( ( CObjBase* )pTask ); 
        ret = oTask.GetObjPtr( propRespPtr, pObj ); 
        if( ERROR( ret ) ) 
            break; 

        pResp = pObj; 
        CCfgOpener oNewResp( ( IConfigDb*)pResp );
        if( !oNewResp.exist( propReturnValue ) )
        {
            ret = ERROR_FAIL;
            break;
        }

    }while( 0 );

    return ret;
}

template<typename T, typename T2= typename std::enable_if<
    !std::is_same<char*, T>::value &&
    !std::is_same<const char*, T>::value &&
    !std::is_base_of<IAutoPtr, T>::value, T>::type >
inline void AssignVal( T& rVal, CBuffer& rBuf )
{
    rVal = rBuf;
}

template<typename T, typename T2= typename std::enable_if<
    std::is_same<char*, T>::value ||
    std::is_same<const char*, T>::value, T>::type,
    typename T3=T2 >
inline void AssignVal( T& rVal, CBuffer& rBuf )
{
    rVal = ( char* )rBuf;
}

template<typename T, typename T2= typename std::enable_if<
    std::is_base_of<IAutoPtr, T>::value &&
    !std::is_same<BufPtr, T>::value &&
    !std::is_same<DMsgPtr, T>::value, T>::type,
    typename T3=T2,
    typename T4=T3 >
inline void AssignVal( T& rVal, CBuffer& rBuf )
{
    ObjPtr& pObj = rBuf;
    rVal = pObj;
}

template<typename T, typename T2= typename std::enable_if<
    std::is_same<BufPtr, T>::value, T>::type,
    typename T3=T2,
    typename T4=T3,
    typename T5=T4 >
inline void AssignVal( BufPtr& rVal, CBuffer& rBuf )
{
    rVal = rBuf;
}

template<typename T, typename T2= typename std::enable_if<
    std::is_same<DMsgPtr, T>::value, T>::type,
    typename T3=T2,
    typename T4=T3,
    typename T5=T4,
    typename T6=T5 >
inline void AssignVal( DMsgPtr& rVal, CBuffer& rBuf )
{
    rVal = ( DMsgPtr& )rBuf;
}

#define AC \
    std::tuple_size<TupleType>::value


#define AI( _i ) \
    ( AC - _i )

template< int N, typename TupleType >
void AssignValues( TupleType& oTuple,
    std::deque< BufPtr >& queResp )
{}

template< int N, typename TupleType, typename First >
void AssignValues( TupleType& oTuple,
    std::deque< BufPtr >& queResp )
{

    CBuffer& oBuf = *queResp[ AI( N ) ];
    // this assignment serve as a rvalue
    // storage to avoid the rvalue to go away
    // unexpectedly
    DecType( First ) oVal;
    AssignVal< DecType( First ) >( oVal, oBuf );
    std::get< AI( N ) >( oTuple ) = oVal;
}

template< int N, typename TupleType, typename First, typename Second, typename ...Args >
void AssignValues( TupleType& oTuple,
        std::deque< BufPtr >& queResp )
{
    CBuffer& oBuf = *queResp[ AI( N ) ];
    DecType( First ) oVal;

    AssignVal< DecType( First ) >( oVal, oBuf );

    std::get< AI( N ) >( oTuple ) = oVal;
    AssignValues<N-1, TupleType, Second, Args...>( oTuple, queResp );
}

inline gint32 GetSeriProto(
    IConfigDb* pCfg,
    EnumSeriProto& dwSeriProto )
{
    if( pCfg == nullptr )
        return -EINVAL;
    CCfgOpener oCfg( pCfg );
    return oCfg.GetIntProp( 
        propSeriProto, ( guint32& )dwSeriProto );
}

template< typename ...Args >
gint32 CInterfaceProxy::FillArgs( CfgPtr& pResp,
    gint32& iRet, Args&&... args )
{
    gint32 ret = 0;

    if( pResp.IsEmpty() )
        return -EINVAL;

    auto oTuple = std::tuple<Args...>(args...);

    do{
        EnumSeriProto dwSeriProto = seriNone;
        ret = GetSeriProto( pResp, dwSeriProto );
        if( dwSeriProto != seriNone )
        {
            ret = -EBADMSG;
            break;
        }
        ret = 0;
        CParamList oResp( ( IConfigDb* )pResp );
        if( !oResp.exist( propReturnValue ) )
        {
            ret = -EINVAL;
            break;
        }

        iRet = oResp[ propReturnValue ];

        if( ERROR( iRet ) )
            break;

        if( sizeof...(args) > ( size_t )oResp.GetCount() )
        {
            ret = -EINVAL;
            break;
        }
        std::deque<BufPtr> queResp;
        for( size_t i = 0; i < sizeof...( Args ); i++ )
        {
            BufPtr pBuf( true );
            ret = oResp.GetProperty( i, pBuf );
            if( ERROR( ret ) )
                return ret;

            queResp.push_back( pBuf );
        }

        AssignValues<sizeof...(Args), decltype( oTuple ), Args...>(
             oTuple, queResp );

    }while( 0 );

    return ret;
}

gint32 IsMidwayPath(
    const std::string& strTest,
    const std::string& strDest );

}
