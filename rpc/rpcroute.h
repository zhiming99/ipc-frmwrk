/*
 * =====================================================================================
 *
 *       Filename:  rpcroute.h
 *
 *    Description:  implementation RPC related stuffs
 *
 *        Version:  1.0
 *        Created:  04/06/2016 09:59:50 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi( woodhead99@gmail.com )
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
#include "defines.h"
#include "port.h"
#include "ifhelper.h"
#include "iftasks.h"
#include "counters.h"
#include "tractgrp.h"

#define ROUTER_OBJ_DESC             "./router.json"
#define ROUTER_OBJ_DESC_AUTH        "./rtauth.json"

#define DEFAULT_WNDSIZE     20
#define MAX_NUM_CHECK   8640000

namespace rpcf
{

gint32 AppendConnParams(
    IConfigDb* pConnParams,
    BufPtr& pBuf );

gint32 gen_sess_hash(
    BufPtr& pBuf,
    std::string& strSess );

struct IRpcReqProxyAsync
{

    // all the methods here are asynchronous
    //
    // open the listening port if not existing and
    // add an irp to listen to the strObjPath
    // usually called on start of the pdo config
    // db including `ip addr', `dst dbus name'
    // `obj name'
    virtual gint32 OpenRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg ) = 0;

    // config db including `ip addr', `dst dbus
    // name' called on interface demand
    virtual gint32 CloseRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg ) = 0;

    // modified by the pdo on call to interface's
    // enable event and redirect to the router
    // config db including `ip addr', `event dbus
    // name' `obj name' `if name'
    virtual gint32 EnableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch ) = 0;

    // modified by the pdo and redirect to the
    // router
    //
    // config db including `ip addr', `event dbus
    // name' `obj name' `if name'
    virtual gint32 DisableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch ) = 0;

    // called by the pdo to redirect a method call
    // to the router
    //
    // Response message layout:
    //
    // the propReturnValue contains the return
    // value
    //
    // 0 is the first [out] param
    //
    // 1 is the second [out] param ...
    // and so on
    //
    // the pRespMsg is returned only if the method
    // succeeded, if the method returns error,
    // the pRespMsg is empty.
    //
    // if STATUS_PENDING is returned,
    // the pRespMsg is not valid. And the msg will
    // be returned as the first arg in the
    // pCallback's propRespPtr.

    virtual gint32 ForwardRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pReqMsg,           // [ in ]
        DMsgPtr& pRespMsg,              // [ out ]
        IEventSink* pCallback ) = 0;

};

class IRpcReqServerAsync
    : public IRpcReqProxyAsync
{
    typedef IRpcReqProxyAsync super;
    // IMPORTANT NOTE: that all the methods in
    // this interface ( including those methods
    // inherited from super's members ) cannot be
    // accessed randomly even if you have the
    // pointer to the interface, because they are
    // wrapped heavily with synchronization and
    // interception mechanism, and calling them
    // from outside the mechanism will cause
    // unexpected reasult
};

class IRpcEventRelay
{
    public:
    // the event message which comes from remote
    // server at strSrcIrp. The host Msg sent out
    // by this method has a layout as the
    // following
    //
    // 1. the first argument must be the string of
    // source ip addr of the event, [required by
    // the CProxyMsgMatch]. So if you don't have
    // an ip address related message, just put
    // '0.0.0.0' as a placeholder.
    //
    // 2. if there is dbus message across the
    // network, the dbus message from remote
    // server is serialized in byte array
    //
    // 3. we don't support array of variable
    // lenght element for now. All you have to do
    // is to serialize your data to a byte array
    // before adding it to the dbus message
    //
    // 4. IMPORTANT: It is a proxy-style method,
    // and can be accessed from all threads within
    // the process

    virtual gint32 ForwardEvent(
        IConfigDb* pEvtCtx,
        DBusMessage* pEventMsg,
        IEventSink* pCallback ) = 0;

};

class CRpcRouter;

class CRegisteredObject
    : public CConfigDb
{
    public:
    typedef CConfigDb super;

    CRegisteredObject( const IConfigDb* pCfg )
        : super( nullptr )
    { SetClassId( clsid( CRegisteredObject ) ); }

    gint32 SetProperties(
        const std::string& strSrcDBusName,
        const std::string& strSrcUniqName,
        guint32 dwPortId )
    {
        CCfgOpener oCfg( this );

        oCfg.SetStrProp(
            propSrcUniqName, strSrcUniqName );

        oCfg.SetStrProp(
            propSrcDBusName, strSrcDBusName );

        oCfg.SetIntProp(
            propPrxyPortId, dwPortId );

        return 0;
    }

    guint32 GetPortId()
    {
        CCfgOpener oCfg( this );
        guint32 dwPortId =
            oCfg[ propPrxyPortId ];
        return dwPortId;
    }

    std::string GetUniqName()
    {
        std::string strName;
        CCfgOpener oCfg( this );
        strName = oCfg[ propSrcUniqName ];
        return strName;
    }

    std::string GetSrcDBusName()
    {
        std::string strName;
        CCfgOpener oCfg( this );
        strName = oCfg[ propSrcDBusName ];
        return strName;
    }

    // test if the match belongs to this
    // registered interface
    gint32 IsMyMatch( IMessageMatch* pMatch );
    bool operator<( CRegisteredObject& rhs ) const;
};

typedef CAutoPtr< clsid( CRegisteredObject ), CRegisteredObject > RegObjPtr;

}

namespace std
{
    using namespace rpcf;
    template<>
    struct less<RegObjPtr>
    {
        bool operator()(const RegObjPtr& k1, const RegObjPtr& k2) const
        {
             return ( *k1 < *k2 );
        }
    };
}

namespace rpcf
{

class CRpcInterfaceProxy :
    virtual public CAggInterfaceProxy,
    public IRpcReqProxyAsync,
    public IRpcEventRelay
{
    protected:
    CRpcRouter  *m_pParent;

    gint32 OnKeepAliveRelay(
        IEventSink* pTask );

    //gint32 OnKeepAliveTerm(
     //   IEventSink* pTask );

    virtual gint32 OnKeepAliveOrig(
        IEventSink* pTask );

    public:
    typedef CAggInterfaceProxy super;
    CRpcInterfaceProxy( const IConfigDb* pCfg ) :
        super( pCfg )
    {;}

    virtual gint32 OnKeepAlive(
        IEventSink* pInvokeTask,
        EnumKAPhase bOrigin );

    CRpcRouter* GetParent() const
    { return m_pParent; }

    virtual bool IsConnected(
        const char* szAddr = nullptr );
};

class CRpcInterfaceServer :
    virtual public CAggInterfaceServer,
    public IRpcReqServerAsync,
    public IRpcEventRelay
{

    // common methods in both the CRpcReqForwarder
    // and CRpcTcpBridge 
    protected:
    CRpcRouter  *m_pParent;

    virtual gint32 BuildBufForIrpFwrdEvt(
        BufPtr& pBuf,
        IConfigDb* pReqCall ) = 0;

    virtual gint32 SetupReqIrpFwrdEvt(
        IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback ) = 0;

    virtual gint32 OnKeepAliveRelay(
        IEventSink* pInvokeTask ) = 0;

    gint32 ValidateRequest_SendData(
        DBusMessage* pReqMsg,
        IConfigDb* pDataDesc );

    gint32 OnFwrdReqQueueFull(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 CloneInvTask(
        IEventSink* pCallback,
        TaskletPtr& pTask ) const;

    gint32 RetrieveTaskId(
        IEventSink* pCallback,
        guint64& qwTaskId ) const;

    gint32 RetrieveDest(
        IEventSink* pCallback,
        stdstr& strDest ) const;

    virtual gint32 GetGrpRfc(
        DMsgPtr& pMsg,
        TaskGrpPtr& pGrp ) = 0;

    public:

    typedef CAggInterfaceServer super;
    CRpcInterfaceServer( const IConfigDb* pCfg ) :
        super( pCfg )
    {;}

    virtual bool IsConnected(
        const char* szAddr = nullptr );

    virtual gint32 CheckReqCtx(
        IEventSink* pCallback,
        DMsgPtr& pMsg ) override
    { return STATUS_SUCCESS; }

    virtual gint32 DoInvoke(
        DBusMessage* pReqMsg,
        IEventSink* pCallback );

    CRpcRouter* GetParent() const
    { return m_pParent; }

    virtual gint32 SetupReqIrp(
        IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    virtual gint32 BuildBufForIrp(
        BufPtr& pBuf,
        IConfigDb* pReqCall );

    virtual gint32 OnKeepAlive(
        IEventSink* pInvokeTask,
        EnumKAPhase bOrigin );

    gint32 CancelInvTasks( ObjVecPtr& pTasks );

    virtual gint32 RequeueInvTask(
        IEventSink* pCallback ) = 0;

    bool IsRfcEnabled() const;

    gint32 InstallQFCallback(
        TaskletPtr& pInvTask ) const;

    virtual gint32 AddAndRun(
        TaskletPtr& pTask,
        bool bImmediate = false ) override;
};

using FWRDREQ_ELEM = 
    std::pair< TaskletPtr, guint64 >;

using FWRDREQS = 
    std::vector< FWRDREQ_ELEM >;

using FWRDREQS_ITER =
    FWRDREQS::iterator;

using IF_TASK_MAP = CStlMap< InterfPtr, QwVecPtr >;

class CRpcReqForwarder :
     public CRpcInterfaceServer
{
    protected:

    // the cfgptr contains the following
    // properties
    //
    // propPortId
    // propSrcUniqName
    // propSrcDBusName
    // check if the match is valid before it is
    // registered
    gint32 CheckMatch( IMessageMatch* pMatch );

    gint32 SchedToStopBridgeProxy( 
        IConfigDb* pReqCtx );

    virtual gint32 BuildBufForIrpFwrdEvt(
        BufPtr& pBuf,
        IConfigDb* pReqCall );

    virtual gint32 OnKeepAliveRelay(
        IEventSink* pInvokeTask );

    gint32 EnableDisableEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch,
        bool& bEnable );

    gint32 OnRmtSvrOnline(
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 OnRmtSvrOffline(
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 BuildBufForIrpRmtSvrEvent(
        BufPtr& pBuf, IConfigDb* pReqCall );

    virtual gint32 OnRmtSvrEvent(
        EnumEventId iEvent,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    virtual gint32 BuildBufForIrp(
        BufPtr& pBuf, IConfigDb* pReqCall );

    gint32 OpenRemotePortInternal(
        IEventSink* pCallback,
        IConfigDb* pCfg );

    gint32 CloseRemotePortInternal(
        IEventSink* pCallback,
        IConfigDb* pCfg );

    virtual gint32 SetupReqIrpFwrdEvt(
        IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    gint32 StopBridgeProxy(
        IEventSink* pCallback,
        guint32 dwPortId,
        const std::string& strSrcUniqName,
        const std::string& strSrcDBusName );

    gint32 OnModOfflineInternal(
        IEventSink* pCallback,
        EnumEventId iEvent,
        std::string strUniqName );

    gint32 BuildKeepAliveMessage(
        IConfigDb* pCfg, DMsgPtr& pkaMsg );

    // CRpcInterfaceServer
    gint32 SendFetch_Server(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [in,out]
        guint32& dwSize,                // [in,out]
        IEventSink* pCallback );

    bool SupportIid(
        EnumClsid iIfId ) const override
    {
        if( iIfId == iid( IStream ) )
            return true;
        if( iIfId == iid( IStreamMH ) )
            return true;
        return false;
    }

    gint32 OnCheckRouterPathComplete(
        IEventSink* pCallback, 
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    bool m_bRfc = false;
    using GRPRFC_KEY=std::pair< gint32, stdstr >;
    std::map< GRPRFC_KEY, TaskGrpPtr > m_mapGrpRfcs;
    std::map< stdstr, stdstr > m_mapUq2SdName;
    bool m_bSepConns = false;

    inline gint32 GetSrcDBusName(
        const stdstr& strUqName,
        stdstr& strName ) const
    {
        CStdRMutex oLock( GetLock() );
        std::map< stdstr, stdstr >::const_iterator
            itr = m_mapUq2SdName.find( strUqName );

        if( itr == m_mapUq2SdName.cend() )
            return -ENOENT;

        strName = itr->second;
        return 0;
    }

    gint32 GetGrpRfcs(
        guint32 dwPortId,
        std::vector< TaskGrpPtr >& vecGrps ) const;

    gint32 GetGrpRfcs(
        const stdstr& strUniqName,
        std::vector< TaskGrpPtr >& vecGrps ) const;

    gint32 GetGrpRfc( GRPRFC_KEY& oKey,
        TaskGrpPtr& pGrp ) const;

    gint32 GetGrpRfc(
        DMsgPtr& pMsg,        
        TaskGrpPtr& pGrp ) override;

    gint32 RemoveGrpRfcs(
        const stdstr& strUniqName );

    gint32 RemoveGrpRfcs(
        guint32 dwPortId );

    gint32 RemoveGrpRfc(
        guint32 dwPortId,
        const stdstr& strUniqName );

    gint32 FindFwrdReqsAll(
        guint32 dwPortId,
        const stdstr& strUniqName,
        FWRDREQS& vecTasks,
        bool bTaskId );

    gint32 FindFwrdReqsAllRfc(
        guint32 dwPortId,
        const stdstr& strUniqName,
        FWRDREQS& vecTasks,
        bool bTaskId );

    gint32 FindFwrdReqsAllRfc(
        const stdstr& strUniqName,
        FWRDREQS& vecTasks,
        bool bTaskId );

    gint32 FindFwrdReqsAll(
        const stdstr& strUniqName,
        FWRDREQS& vecTasks,
        bool bTaskId = true );

    gint32 GetInvTaskProxyMH(
        TaskletPtr& pTask,
        InterfPtr& pProxy );

    gint32 GetInvTaskPrxyPortId(
        TaskletPtr& pTask,
        guint32& dwPortId );

    gint32 ClearFwrdReqsByDestAddr(
        guint32 dwPortId,
        const stdstr& strPath,
        DMsgPtr& pMsg );

    gint32 FindUniqNamesByPortId(
        guint32 dwPortId,
        std::set< stdstr >& setNames );

    gint32 FindFwrdReqsByPrxyPortId(
        guint32 dwPrxyPortId,
        std::vector< stdstr >& strNames,
        FWRDREQS& vecTasks );

    public:

    typedef CRpcInterfaceServer super;

    CRpcReqForwarder( const IConfigDb* pCfg );

    gint32 InitUserFuncs();

    gint32 AddRefCount(
        guint32 dwPortId,
        const std::string& strSrcUniqName,
        const std::string& strSrcDBusName );

    gint32 DecRefCount(
        guint32 dwPortId,
        const std::string& strSrcUniqName,
        const std::string& strSrcDBusName );

    virtual gint32 ClearRefCountByPortId(
        guint32 dwPortId,
        std::vector< std::string >& vecUniqNames );

    gint32 ClearRefCountByUniqName(
        const std::string& strUniqName,
        std::set< guint32 >& setPortIds );

    gint32 ClearRefCountBySrcDBusName(
        const std::string& strName,
        std::set< guint32 >& setPortIds );

    gint32 GetRefCountByPortId(
        guint32 dwPortId );

    gint32 GetRefCountByUniqName(
        const std::string& strUniqName );

    gint32 GetRefCountBySrcDBusName(
        const std::string& strName );

    const EnumClsid GetIid() const
    { return iid( CRpcReqForwarder ); }
    //
    // called by the bus port to create the remote
    // connection before the proxy pdo is created
    //
    // we have to call it from the proxy pdo to
    // utilize the support for synchronous call
    // across the process border
    //
    // make a connection to the remote target
    //
    // propConnParams: the remote address the
    // connection will be made
    //
    // propSrcDBusName: the local source who send
    // out the request
    //
    // propObjPath
    // propIfName
    // 
    virtual gint32 OpenRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg );

    // active disconnecting
    virtual gint32 CloseRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg );

    // mandatory properties within the pMatch
    //
    // propRouterPath: address of the remote server
    // propIfName: interface name
    // propObjPath: object path
    // propDestDBusName: destination dbus name
    //
    virtual gint32 EnableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    virtual gint32 DisableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    virtual gint32 ForwardEvent(
        IConfigDb* pEvtCtx,
        DBusMessage* pEventMsg,
        IEventSink* pCallback );

    gint32 CheckReqToFwrd(
        IConfigDb* pReqCtx,
        DMsgPtr& pMsg,
        MatchPtr& pMatchHit );

    // methods of CRpcBaseOperations
    virtual gint32 OnModEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    gint32 ForwardRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pFwdrMsg,
        DMsgPtr& pRespMsg,
        IEventSink* pCallback );

    virtual gint32 FetchData_Server(
        IConfigDb* pDataDesc,           // [in]
        gint32 fd,                      // [in]
        guint32 dwOffset,               // [in]
        guint32 dwSize,                 // [in]
        IEventSink* pCallback )
    {
        return SendFetch_Server( pDataDesc,
            fd, dwOffset, dwSize, pCallback );
    }

    gint32 CheckRouterPath(
        IEventSink* pCallback,
        IConfigDb* pReqCtx );

    gint32 FindFwrdReqsByUniqName(
        const stdstr& strName,
        FWRDREQS& vecTasks );
 
    gint32 RequeueInvTask(
        IEventSink* pCallback ) override;

    gint32 CreateGrpRfc(
        guint32 dwPortId,
        const stdstr& strUniqName,
        const stdstr& strSdName );

    inline bool IsSepConns() const
    { return m_bSepConns; }
}; // CRpcReqForwarder

class CRpcRfpForwardEventTask
    : public CIfParallelTask
{
    public:
    typedef CIfParallelTask super;
    CRpcRfpForwardEventTask(
        const IConfigDb* pCfg = nullptr )
        : CIfParallelTask( pCfg )
    {
        SetClassId( clsid( CRpcRfpForwardEventTask ) );
    }
    virtual gint32 RunTask();
};

class CRfpModEventRespTask :
    public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;

    CRfpModEventRespTask ( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CRfpModEventRespTask  ) ); }

    virtual gint32 RunTask()
    { return STATUS_PENDING; }

    virtual gint32 OnTaskComplete( gint32 iRetVal );
};

class CRpcReqForwarderProxy :
    public CRpcInterfaceProxy
{
    protected:
    // tcp port handle and the related
    // informations
    gint32 SetupReqIrpEnableEvt( IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    gint32 SetupReqIrpFwrdReq( IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    virtual gint32 RebuildMatches();

    virtual gint32 BuildNewMsgToFwrd(
        IConfigDb* pReqCtx,
        DMsgPtr& pFwrdMsg,
        DMsgPtr& pNewMsg );

    using IFREF = std::pair< gint32, gint32 >;
    std::map< MatchPtr, std::pair< gint32, gint32 > > m_mapMatchRefs;

    public:

    typedef CRpcInterfaceProxy super;
    CRpcReqForwarderProxy( const IConfigDb* pCfg );
    ~CRpcReqForwarderProxy();

    gint32 InitUserFuncs();

    const EnumClsid GetIid() const
    { return iid( CRpcReqForwarder ); }

    virtual gint32 SetupReqIrp( IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    virtual gint32 OpenRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg )
    { return -ENOTSUP; }

    // active disconnecting
    virtual gint32 CloseRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg )
    { return -ENOTSUP; }

    virtual gint32 EnableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch )
    { return -ENOTSUP; }

    virtual gint32 DisableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch )
    { return -ENOTSUP; }

    virtual gint32 ForwardRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pReqMsg,           // [ in ]
        DMsgPtr& pRespMsg,              // [ out ]
        IEventSink* pCallback );

    virtual gint32 ForwardEvent(
        IConfigDb* pEvtCtx,
        DBusMessage* pEventMsg,
        IEventSink* pCallback );

    virtual gint32 CustomizeRequest(
        IConfigDb* pReqCfg,
        IEventSink* pCallback );

    gint32 FillRespData(
        IRP* pIrp, CfgPtr& pResp );

    gint32 DoInvoke(
        DBusMessage* pEvtMsg,
        IEventSink* pCallback );

    gint32 NormalizeMatch(
        IMessageMatch* pMatchExternal,
        MatchPtr& pMatchOut );

    gint32 AddInterface(
        IMessageMatch* pMatch );

    gint32 RemoveInterface(
        IMessageMatch* pMatch );

    gint32 GetActiveIfCount()
    {
        gint32 iCount = 0;

        CStdRMutex oIfLock( GetLock() );
        for( auto elem : m_vecMatches )
        {
            IMessageMatch* pMatch = elem; 
            CCfgOpenerObj oMatch( pMatch );
            bool bDummy = false;

            gint32 ret = oMatch.GetBoolProp(
                propDummyMatch, bDummy );

            if( SUCCEEDED( ret ) && bDummy )
                continue;

            ++iCount;
        }
        return iCount;
    }

    gint32 OnRmtSvrEvent(
        EnumEventId iEvent,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 ForceCancelRequests(
        ObjPtr& pvecTasks,
        guint64& qwThisTaskId,
        IEventSink* pCallback );

    protected:

    gint32 OnModEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    gint32 OnRmtSvrOffline(
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 OnKeepAliveOrig(
        IEventSink* pTask );

}; // CRpcReqForwarderProxy 

#define GET_TARGET_PORT( pPort ) \
do{ \
        CCfgOpener oReq; \
        bool _bPdo = false; \
        oReq.SetBoolProp( propSubmitPdo, true ); \
        ret = this->GetPortToSubmit( \
            oReq.GetCfg(), pPort, _bPdo ); \
}while( 0 ) 

struct CRpcTcpBridgeShared
{
    CRpcTcpBridgeShared( CRpcServices* pIf );

    gint32 OpenLocalStream( IPort* pPort,
        gint32 iStreamId,
        guint32 dwProtoId,
        gint32& iStmIdOpened );

    gint32 CloseLocalStream(
        IPort* pPort, gint32 iStreamId );

    InterfPtr GetIfPtr() const
    {
        return InterfPtr( const_cast
        < CRpcServices* >( m_pParentIf ) );
    }

    // local commands
    gint32 ReadStream( gint32 iStreamId,
        BufPtr& pDestBuf,
        guint32 dwSize,
        IEventSink* pCallback );

    gint32 WriteStream( gint32 iStreamId,
        CBuffer* pSrcBuf,
        guint32 dwSize,
        IEventSink* pCallback );

    gint32 IoControlStream( gint32 iStreamId,
        CBuffer* pReq,
        BufPtr& pResp,
        IEventSink* pCallback );

    gint32 RegMatchCtrlStream(
        gint32 iStreamId,
        MatchPtr& pMatch,
        bool bReg );

    gint32 GetPeerStmId(
        gint32 iStmId,
        gint32& iPeerStmid );

    gint32 StartEx2(
        IEventSink* pCallback );

    gint32 StartExOrig(
        IEventSink* pCallback );

    gint32 StartHandshake(
        IEventSink* pCallback );

    protected:
    CRpcServices* m_pParentIf;
    TaskGrpPtr    m_pGrpRfc;

    gint32 ReadWriteStream(
        gint32 iStreamId,
        BufPtr& pSrcBuf,
        guint32 dwSize,
        IEventSink* pCallback,
        bool bRead );

    gint32 SetupReqIrpListeningShared(
        IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    gint32 OnPostStartShared(
        IEventSink* pContext,
        MatchPtr& pMatch );

    gint32 GetPortToSubmitShared(
        CObjBase* pCfg,
        PortPtr& pPort,
        bool& bPdo );

    gint32 InitRfc( CCfgOpener& oParams );
};

class CRpcTcpBridge :
    public CRpcInterfaceServer,
    public CRpcTcpBridgeShared
{
    friend class CRpcTcpBridgeShared;

    gint32 OnEnableRemoteEventCompleteMH(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pCtx );

    gint32 EnableRemoteEventInternal(
        IEventSink* pCallback,
        IMessageMatch* pMatch,
        bool bEnable );

    gint32 EnableRemoteEventInternalMH(
        IEventSink* pCallback,
        IMessageMatch* pMatch,
        bool bEnable );

    virtual gint32 BuildBufForIrpFwrdEvt(
        BufPtr& pBuf,
        IConfigDb* pReqCall );

    virtual gint32 OnKeepAliveRelay(
        IEventSink* pTask );

    gint32 SetupReqIrpListening( IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback )
    {
        return SetupReqIrpListeningShared(
            pIrp, pReqCall, pCallback );
    }

    gint32 OpenStream_Server(
        IEventSink* pCallback,
        gint32 iPeerStm,
        guint32 wProtocol );

    gint32 CloseStream_Server(
        IEventSink* pCallback,
        gint32 iStreamId );

    gint32 OnCheckRouterPathComplete(
        IEventSink* pCallback, 
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 ClearRemoteEventsLocal(
        IEventSink* pCallback,
        ObjPtr& pvecMatches );

    gint32 OnClearRemoteEventsComplete(
        IEventSink* pCallback,
        IEventSink*  pIoReq,
        IConfigDb* pReqCtx );

    gint32 FindFwrdReqsByTaskId(
        std::vector< guint64 >& vecTaskIds,
        FWRDREQS& vecTasks );

    gint32 FindFwrdReqsByDestAddr(
        const stdstr& strPath,
        const stdstr& strDest,
        FWRDREQS& vecTasks );

    gint32 GetFwrdReqs(
        FWRDREQS& vecReqs,
        ObjMapPtr& pmapTaskIdsLoc,
        ObjMapPtr& pmapTaskIdsMH );

    gint32 ClearFwrdReqsByDestAddr(
        const stdstr strPath,
        DMsgPtr& pMsg );

    gint32 IsMHTask(
        TaskletPtr& pTask );

    gint32 GetInvTaskProxyMH(
        TaskletPtr& pTask,
        InterfPtr& pIf,
        bool bQueued );

    protected:
    virtual gint32 SetupReqIrpFwrdEvt(
        IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    gint32 CheckRouterPathAgain(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    bool IsAccesable( IConfigDb* pReqCtx );
    gint32 IsCyclicPath( IConfigDb* pReqCtx );
    gint32 AddCheckStamp( IConfigDb* pReqCtx );

    gint32 ForwardRequestInternal(
        IConfigDb* pReqCtx,
        DBusMessage* pFwdrMsg,
        DMsgPtr& pRespMsg,
        IEventSink* pCallback,
        bool bSeqTask );

    virtual gint32 GetGrpRfc(
        DMsgPtr& pMsg,
        TaskGrpPtr& pGrp )
    {
        pGrp = m_pGrpRfc;
        if( pGrp.IsEmpty() )
            return -EFAULT;
        return 0;
    }

    protected:
    TaskletPtr m_pHsTicker;
    bool m_bHandshaked = false;
    bool m_bHsFailed = false;
    stdstr m_strSess;

    CTimestampSvr m_oTs;

    // for load balance
    gint32 OnCheckRouterPathCompleteLB(
        IEventSink* pCallback, 
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 OnCheckRouterPathCompleteLB2(
        IEventSink* pCallback, 
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 CheckRouterPathAgainLB(
        IEventSink* pCallback,
        IEventSink* pInvTask,
        IConfigDb* pReqCtx );

    gint32 OnOpenPortCompleteLB(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 OpenPortLB(
        IEventSink* pCallback,
        IEventSink* pInvTask,
        IEventSink* pOpTask,
        bool bLast );

    gint32 CheckRouterPathLB(
        IEventSink* pCallback,
        IConfigDb* pReqCtx,
        const std::string& strOldPath,
        const std::string& strNext,
        const std::string& strLBNode,
        const std::vector< std::string >& vecNodes );

    gint32 GenSessHash(
        std::string& strSess ) const;

    public:

    virtual gint32 OnPostStart(
        IEventSink* pContext ) override;

    typedef CRpcInterfaceServer super;
    CRpcTcpBridge( const IConfigDb* pCfg );
    ~CRpcTcpBridge();

    const EnumClsid GetIid() const
    { return iid( CRpcTcpBridge ); }

    virtual gint32 OpenRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg )
    { return -ENOTSUP; }

    virtual gint32 SetupReqIrp( IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    // active disconnecting
    virtual gint32 CloseRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg )
    { return -ENOTSUP; }

    virtual gint32 EnableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    virtual gint32 DisableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    virtual gint32 ForwardEvent(
        IConfigDb* pEvtCtx,
        DBusMessage* pEventMsg,
        IEventSink* pCallback );

    gint32 CheckReqToRelay(
        IConfigDb* pReqCtx,
        DMsgPtr& pMsg,
        MatchPtr& pMatchHit );

    virtual gint32 InitUserFuncs();

    // bridge and stream specific methods
    gint32 OpenStream(
        IEventSink* pCallback,  
        gint32 iPeerStm,
        guint32 wProtocol )
    {
        return OpenStream_Server(
            pCallback,
            iPeerStm,
            wProtocol );
    }

    gint32 CloseStream(
        IEventSink* pCallback,
        gint32 iStreamId )
    {
        return CloseStream_Server(
            pCallback, iStreamId );
    }

    gint32 StartEx2(
        IEventSink* pCallback ) override;

    gint32 CheckHsTimeout(
        IEventSink* pTask,
        IEventSink* pStartCb );

    gint32 DoStartHandshake(
        IEventSink* pCallback );

    gint32 Handshake(
        IEventSink* pCallback,
        IConfigDb* pInfo );

    virtual gint32 SendResponse(
        IEventSink* pInvTask,
        IConfigDb* pReqMsg,
        CfgPtr& pRespData );

    // called to set the response to send back
    virtual gint32 SetResponse(
        IEventSink* pTask,
        CfgPtr& pRespData );

    virtual gint32 OnPreStart(
        IEventSink* pCallback )
    {
        gint32 ret =
            super::OnPreStart( pCallback );

        if( ERROR( ret ) )
            return ret;

        // we don't need connect manager
        m_pConnMgr.Clear();
        return 0;
    }

    gint32 Pause_Server(
        IEventSink* pCallback )
    {
        // uncomment me if necessary.
        if( pCallback == nullptr )
            return -EINVAL;

        FillAndSetResponse(
            pCallback, -ENOTSUP );         
        return -ENOTSUP;
    }

    gint32 Resume_Server(
        IEventSink* pCallback )
    {
        if( pCallback == nullptr )
            return -EINVAL;

        FillAndSetResponse(
            pCallback, -ENOTSUP );         
        return -ENOTSUP;
    }

    gint32 ClearRemoteEvents(
        IEventSink* pCallback,
        ObjPtr& pVecMatches , // [ in ]
        ObjPtr& pVecTaskIds );

    gint32 CheckRouterPath(
        IEventSink* pCallback,
        IConfigDb* pReqCtx );

    using CRpcBaseOperations::GetPortToSubmit;

    virtual gint32 GetPortToSubmit(
        CObjBase* pCfg,
        PortPtr& pPort,
        bool& bPdo )
    {
        return GetPortToSubmitShared(
            pCfg, pPort, bPdo );
    }

    virtual gint32 ForwardRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pFwdrMsg,
        DMsgPtr& pRespMsg,
        IEventSink* pCallback );

    inline guint64 GetAgeSec( guint64 qwTs )
    { return m_oTs.GetAgeSec( qwTs ); }

    gint32 PostDisconnEvent();

    virtual gint32 OnPostStop(
        IEventSink* pCallback ) override;

    gint32 RefreshReqLimit(
        guint32 dwMaxReqs,
        guint32 dwMaxPendigns );

    const stdstr& GetSessHash() const
    { return m_strSess; }
 
    gint32 FindFwrdReqsAll(
        FWRDREQS& vecTasks,
        bool bTaskId = true );

    gint32 GetFwrdReqsToCancel(
        ObjMapPtr& pmapTaskIdsLoc,
        ObjMapPtr& pmapTaskIdsMH );

    gint32 RequeueInvTask(
        IEventSink* pCallback ) override ;

    gint32 GetAllFwrdReqs(
        FWRDREQS& vecReqs,
        ObjMapPtr& pmapTaskIdsLoc,
        ObjMapPtr& pmapTaskIdsMH );

    gint32 FindFwrdReqsByPrxyPortId(
        guint32 dwPrxyPortId,
        FWRDREQS& vecTasks );

    gint32 FindFwrdReqsByPath(
        const stdstr& strPath,
        FWRDREQS& vecTasks );

}; // CRpcTcpBridge

class CRpcTcpBridgeProxy :
    public CRpcInterfaceProxy,
    public CRpcTcpBridgeShared
{
    friend class CRpcTcpBridgeShared;

    gint32 EnableRemoteEventInternal(
        IEventSink* pCallback,
        IMessageMatch* pMatch,
        bool bEnable );

    gint32 FillRespDataFwrdReq(
        IRP* pIrp, CfgPtr& pResp );

    gint32 BuildBufForIrpFwrdReq(
        BufPtr& pBuf,
        IConfigDb* pReqCall );

    gint32 SetupReqIrpListening(
        IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback )
    {
        return SetupReqIrpListeningShared(
            pIrp, pReqCall, pCallback );
    }

    gint32 OpenStream_Proxy(
        guint32 wProtocol,
        gint32& iStreamId,
        IEventSink* pCallback );

    gint32 CloseStream_Proxy( gint32 iStreamId,
        IEventSink* pCallback );

    gint32 SendFetch_TcpProxy(
        IConfigDb* pDataDesc,           // [in, out]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [out]
        guint32& dwSize,                // [out]
        IEventSink* pCallback );

    gint32 CustomizeRequest(
        IConfigDb* pReqCfg,
        IEventSink* pCallback );

    gint32 OnRmtSvrEvent(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        guint32 dwEventId );

    CTimestampProxy m_oTs;

    inline void SetRfcEnabled( bool bRfc )
    { m_bRfc = bRfc; }

    bool          m_bRfc = false;

    public:

    typedef CRpcInterfaceProxy super;

    CRpcTcpBridgeProxy(
        const IConfigDb* pCfg );

    virtual gint32 SetupReqIrp( IRP* pIrp,
        IConfigDb* pReqCall,
        IEventSink* pCallback );

    virtual gint32 OpenRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg )
    { return -ENOTSUP; }

    const EnumClsid GetIid() const
    { return iid( CRpcTcpBridge ); }

    // RPC methods
    // active disconnecting
    virtual gint32 CloseRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pCfg )
    { return -ENOTSUP; }

    // the method to do cleanup when the client is
    // down
    gint32 ClearRemoteEvents(
        ObjPtr& pVecMatches , // [ in ]
        ObjPtr& pVecTaskIds,
        IEventSink* pCallback );

    gint32 CheckRouterPath(
        IConfigDb* pReqCtx,
        IEventSink* pCallback );

    virtual gint32 EnableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    virtual gint32 DisableRemoteEvent(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    virtual gint32 ForwardRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pReqMsg,           // [ in ]
        DMsgPtr& pRespMsg,              // [ out ]
        IEventSink* pCallback );

    virtual gint32 ForwardEvent(
        IConfigDb* pEvtCtx,
        DBusMessage* pEventMsg,
        IEventSink* pCallback );

    virtual gint32 FillRespData(
        IRP* pIrp, CfgPtr& pResp );

    virtual gint32 BuildBufForIrp(
        BufPtr& pBuf,
        IConfigDb* pReqCall );

    virtual gint32 DoInvoke(
        DBusMessage* pEvtMsg,
        IEventSink* pCallback );

    virtual gint32 AddAndRun(
        TaskletPtr& pParallelTask,
        bool bImmediate = false ) override;

    // bridge and stream specific methods
    gint32 OpenStream(
        guint32 wProtocol,
        gint32& iStreamId,
        IEventSink* pCallback )
    {
        return OpenStream_Proxy(
            wProtocol, iStreamId,
            pCallback );
    }

    gint32 CloseStream( gint32 iStreamId,
        IEventSink* pCallback )
    {
        return CloseStream_Proxy( 
            iStreamId, pCallback );
    }

    gint32 StartEx2(
        IEventSink* pCallback ) override
    {
        return CRpcTcpBridgeShared::StartEx2(
            pCallback );
    }

    gint32 OnHandshakeComplete(
        IEventSink* pCallback,
        IEventSink*  pIoReq,
        IConfigDb* pReqCtx );

    gint32 DoStartHandshake(
        IEventSink* pCallback );

    gint32 Handshake(
        IConfigDb* pInfo,
        IEventSink* pCallback );

    virtual gint32 InitUserFuncs();

    gint32 OnInvalidStreamId(
        IEventSink* pCallback,
        gint32 iStreamId,
        gint32 iPeerStreamId,
        guint32 dwSeqNo,
        guint32 dwProtocol );

    virtual gint32 OnPostStart(
        IEventSink* pContext );

    using CRpcBaseOperations::GetPortToSubmit;
    virtual gint32 GetPortToSubmit(
        CObjBase* pCfg,
        PortPtr& pPort,
        bool& bPdo )
    {
        return GetPortToSubmitShared(
            pCfg, pPort, bPdo );
    }

    virtual gint32 OnPostStop(
        IEventSink* pCallback ) override;

    gint32 RefreshReqLimit(
        IEventSink* pCallback,
        guint32 dwMaxReqs,
        guint32 dwMaxPendigns );

    gint32 RequeueTask(
        TaskletPtr& pFcTask );

    inline bool IsRfcEnabled() const
    { return m_bRfc; }

}; // CRpcTcpBridgeProxy

class CRouterStopBridgeProxyTask
    : public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;
    CRouterStopBridgeProxyTask( const IConfigDb* pCfg )
        : CIfInterceptTaskProxy( pCfg )
    {
        SetClassId( clsid( CRouterStopBridgeProxyTask ) );
    }
    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
    gint32 OnCancel( guint32 dwContext )
    {  return OnTaskComplete( -ECANCELED ); }
};

class CRouterStopBridgeProxyTask2
    : public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;
    CRouterStopBridgeProxyTask2( const IConfigDb* pCfg )
        : CIfInterceptTaskProxy( pCfg )
    {
        SetClassId( clsid( CRouterStopBridgeProxyTask2 ) );
    }
    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
    gint32 OnCancel( guint32 dwContext )
    {  return OnTaskComplete( -ECANCELED ); }
};

class CRouterStopBridgeTask
    : public CIfInterceptTaskProxy
{
    gint32 DoSyncedTask(
        IEventSink* pCallback,
        ObjVecPtr& pvecMatchsLoc );

    gint32 SyncedTasks(
        IEventSink* pCallback,
        ObjVecPtr& pvecMatchsLoc );

    public:
    typedef CIfInterceptTaskProxy super;
    CRouterStopBridgeTask( const IConfigDb* pCfg )
        : CIfInterceptTaskProxy( pCfg )
    {
        SetClassId( clsid( CRouterStopBridgeTask ) );
    }
    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
    CIoManager* GetIoMgr();

    gint32 OnCancel( guint32 dwContext )
    {  return OnTaskComplete( -ECANCELED ); }

};

class CIfRouterState : public CIfServerState
{
    public:
    typedef CIfServerState super;
    CIfRouterState( const IConfigDb* pCfg );
    virtual gint32 SubscribeEvents();
};

class CRpcRouterManager;
class CRpcRouter :
    public virtual CAggInterfaceServer
{
    // the key is the peer ip-addr and the value is the
    // pair of the tcp bridge and tcp bridge proxy

    mutable stdrmutex   m_oLock;
    std::vector< EnumPropId >   m_vecTopicList;
    CRpcRouter*         m_pParent;

    protected:
    std::map< guint32, InterfPtr > m_mapPid2BdgeProxies;

    public:

    typedef CAggInterfaceServer super;

    CRpcRouter( const IConfigDb* pCfg );
    ~CRpcRouter();

    CRpcRouter* GetParent() const
    { return m_pParent; }

    CRpcRouterManager* GetRouterMgr() const;

    virtual bool HasReqForwarder() const
    { return false; }

    virtual bool HasBridge() const
    { return false; }

    virtual bool HasAuth() const
    { return false; }

    gint32 AddRemoveMatch(
        IMessageMatch* pMatch, bool bAdd,
        std::map< MatchPtr, gint32 >* plm );

    gint32 GetMatchToAdd(
        IMessageMatch* pMatch,
        bool bRemote,
        MatchPtr& pMatchAdd ) const;

    inline gint32 GetMatchToRemove(
        IMessageMatch* pMatch,
        bool bRemote,
        MatchPtr& pMatchRemove )
    {
        return GetMatchToAdd(
            pMatch, bRemote, pMatchRemove );
    }

    virtual gint32 GetBridgeProxy(
        const IConfigDb* pConnParams,
        InterfPtr& pIf );

    gint32 GetBridgeProxy(
        guint32 dwPortId,
        InterfPtr& pIf );

    gint32 AddBridgeProxy( 
        IGenericInterface* pIf );
        
    gint32 RemoveBridgeProxy( 
        IGenericInterface* pIf );

    gint32 SubscribeEvents();
    gint32 UnsubscribeEvents();

    protected:
    virtual gint32 OnRmtSvrOffline(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort )
    { return -ENOTSUP; }

    virtual gint32 OnRmtSvrEvent(
        EnumEventId iEvent,
        IConfigDb* pEvtCtx,
        HANDLE hPort )
    { return -ENOTSUP; }

    virtual gint32 OnRmtSvrOnline(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort )
    { return -ENOTSUP; }

    public:
    virtual gint32 OnPreStopLongWait(
        IEventSink* pCallback )
    { return -ENOTSUP; }

    gint32 OnPostStart( IEventSink* pContext );
    gint32 OnPreStop( IEventSink* pCallback );

    static gint32 GetNodeName(
        const std::string& strPath,
        std::string& strNode );

    gint32 GetPortId( HANDLE hPort,
        guint32 dwPortId ) const;

    virtual gint32 IsEqualConn(
        const IConfigDb* pConn1,
        const IConfigDb* pConn2 );

    bool IsRfcEnabled() const;
};

class CReqFwdrCloseRmtPortTask
    : public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;
    CReqFwdrCloseRmtPortTask( const IConfigDb* pCfg )
        : CIfInterceptTaskProxy( pCfg )
    {
        SetClassId( clsid( CReqFwdrCloseRmtPortTask ) );
    }
    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
    gint32 OnCancel( guint32 dwContext )
    {  return 0; }
};

class CReqFwdrOpenRmtPortTask 
    : public CIfInterceptTaskProxy
{

    protected:
    typedef enum{
        stateInitialized,
        stateStartBridgeProxy,
        stateStartBridgeServer,
        stateDone
    } TaskState;

    TaskState   m_iState;
    InterfPtr   m_pProxy;
    InterfPtr   m_pServer;

    // actively create a CRpcTcpPdo
    virtual gint32 CreateInterface( InterfPtr& pIf );
    virtual gint32 RunTaskInternal( gint32 iRetVal );

    // will call the CInterfaceServer's
    // OnServiceComplete to finish the async call
    virtual gint32 OnServiceComplete( gint32 iRetVal );

    public:
    typedef CIfInterceptTaskProxy super;

    CReqFwdrOpenRmtPortTask ( const IConfigDb* pCfg )
        : CIfInterceptTaskProxy( pCfg )
    {
        SetClassId( clsid( CReqFwdrOpenRmtPortTask ) );
        m_iState = stateInitialized;
    }

    virtual gint32 AdvanceState();
    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
    gint32 OnCancel( guint32 dwContext );
};
/**
* @name CRouterOpenBdgePortTask
* @{ the task tries to create a
* CRpcTcpBridgeImpl or CRpcTcpBridgeProxyImpl
* object, and on success, register the object with
* the router  */
/** 
 * Parameters:
 *      0: bServer[ bool ], whether to create a
 *      proxy or a server.
 *      propRouterPtr: pointer to the router
 *      object to create this object.
 *
 *      propPortId: the port id for the
 *      CRpcTcpBridgeImpl to create, or the
 *      CRpcTcpBridgeImpl object, who requests to
 *      create the CRpcTcpBridgeProxyImpl if
 *      bServer is false.
 *
 *      propNodeName: the node name for the
 *      CRpcTcpBridgeProxyImpl to create when
 *      bServer is false.
 *
 *      propConnParams: the connection parameters
 *      to create the CRpcTcpBridgeProxyImpl.
 *
 *
 * @} */

class CRouterOpenBdgePortTask :
    public CReqFwdrOpenRmtPortTask
{
    gint32 AdvanceState();
    public:
    // passively open a CRpcTcpPdo, when the port has
    // already created.
    typedef CReqFwdrOpenRmtPortTask super;
    CRouterOpenBdgePortTask( const IConfigDb* pCfg )
        : super( pCfg )
    {
        SetClassId( clsid( CRouterOpenBdgePortTask ) ); 
    }

    virtual gint32 CreateInterface( InterfPtr& pIf );
    virtual gint32 RunTaskInternal( gint32 iRetVal );
    virtual gint32 OnServiceComplete( gint32 iRetVal )
    { return 0; }
};

class CIfRollbackableTask :
    public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;
    CIfRollbackableTask( const IConfigDb* pCfg )
        : super( pCfg )
    {}

    gint32 GetTractGrp( CIfTransactGroup*& pTractGrp )
    {
        CCfgOpener oTaskCfg(
            ( IConfigDb* ) GetConfig() );

        guint32* intptr = nullptr;
        gint32 ret = oTaskCfg.GetIntPtr(
            propTransGrpPtr, intptr );

        if( ERROR( ret ) )
            return ret;

        pTractGrp = reinterpret_cast
            < CIfTransactGroup* >( intptr );

        if( pTractGrp == nullptr )
            return -EFAULT;

        return 0;
    }

    gint32 AddRollbackTask(
        TaskletPtr& pTask, bool bBack = false )
    {
        CIfTransactGroup* pTractGrp = nullptr;
        gint32 ret = GetTractGrp( pTractGrp );
        if( ERROR( ret ) )
            return ret;

        if( pTractGrp == nullptr )
            return -EFAULT;

        return pTractGrp->AddRollback(
            pTask, bBack );
    }

    gint32 ChangeRelation( EnumLogicOp iOp )
    {
        CIfTransactGroup* pTractGrp = nullptr;
        gint32 ret = GetTractGrp( pTractGrp );
        if( ERROR( ret ) )
            return ret;

        if( pTractGrp == nullptr )
            return -EFAULT;

        return pTractGrp->SetTaskRelation( iOp );
    }
    
    gint32 OnCancel( guint32 dwContext )
    { return OnTaskComplete( -ECANCELED ); }
};

class CRouterEnableEventRelayTask :
    public CIfRollbackableTask
{
    public:
    typedef CIfRollbackableTask super;

    // we will utilize the
    // CIfRetryTask::OnComplete's notification
    // support for the client notification
    //
    // parameters:
    // 0: bEnable
    // 1: pointer to a IMessageMatch object
    // propIfPtr
    // propEventSink
    // propIoMgr
    CRouterEnableEventRelayTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CRouterEnableEventRelayTask  ) ); }

    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
};

class CRouterStartRecvTask :
    public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;
    CRouterStartRecvTask ( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CRouterStartRecvTask ) ); }

    virtual gint32 RunTask();
};

class CRouterStartReqFwdrProxyTask :
    public CIfRollbackableTask
{
    gint32 OnTaskCompleteStart( gint32 iRetVal );
    public:
    typedef CIfRollbackableTask super;

    // we will utilize the
    // CIfRetryTask::OnComplete's notification
    // support for the client notification
    //
    // parameters:
    // 0: bEnable
    // 1: pointer to a IMessageMatch object
    // propIfPtr
    // propEventSink
    // propIoMgr
    CRouterStartReqFwdrProxyTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CRouterStartReqFwdrProxyTask  ) ); }

    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
};

class CReqFwdrEnableRmtEventTask :
    public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;

    CReqFwdrEnableRmtEventTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CReqFwdrEnableRmtEventTask ) ); }

    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
};

class CRouterAddRemoteMatchTask :
    public CIfRollbackableTask
{
    public:
    typedef CIfRollbackableTask super;

    // parameters:
    // 0: bAdd
    // 1: pointer to a IMessageMatch object
    // propIfPtr
    // propEventSink
    // propIoMgr
    CRouterAddRemoteMatchTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CRouterAddRemoteMatchTask ) ); }

    virtual gint32 RunTask();
    gint32 AddRemoteMatchInternal(
        CRpcRouter* pRouter,
        IMessageMatch* pMatch,
        bool bEnable );
};

class CRouterEventRelayRespTask :
    public CIfInterceptTaskProxy
{
    bool m_bEnable = true;
    public:
    typedef CIfInterceptTaskProxy super;

    CRouterEventRelayRespTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CRouterEventRelayRespTask ) ); }

    virtual gint32 RunTask()
    { return STATUS_PENDING; }

    virtual gint32 OnTaskComplete( gint32 iRetVal );
    void SetEnable( bool bEnable )
    { m_bEnable = bEnable; }

};

class CReqFwdrForwardRequestTask :
    public CIfInterceptTaskProxy
{
    virtual gint32 OnTaskCompleteRfc(
        gint32 iRetVal, TaskletPtr& pTask );

    gint32 CloneIoTask( TaskletPtr& pIoTask );

    public:
    typedef CIfInterceptTaskProxy super;

    CReqFwdrForwardRequestTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CReqFwdrForwardRequestTask ) ); }

    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
};

class CBridgeForwardRequestTask :
    public CReqFwdrForwardRequestTask
{
    public:
    typedef CReqFwdrForwardRequestTask super;

    CBridgeForwardRequestTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CBridgeForwardRequestTask ) ); }

    gint32 RunTask();
    gint32 ForwardRequestMH(
        IConfigDb* pReqCtx,
        DBusMessage* pReqMsg,           // [ in ]
        DMsgPtr& pRespMsg );            // [ out ]

    gint32 OnTaskCompleteRfc(
        gint32 iRetVal, TaskletPtr& pTask ) override;
};

class CReqFwdrFetchDataTask :
    public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;

    CReqFwdrFetchDataTask( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CReqFwdrFetchDataTask ) ); }

    virtual gint32 RunTask();
    virtual gint32 OnTaskComplete( gint32 iRetVal );
    gint32 OnServiceComplete( gint32 iRetVal );
};

class CBdgeProxyReadWriteComplete:
    public CIfInterceptTaskProxy
{
    public:
    typedef CIfInterceptTaskProxy super;

    // parameters:
    // propIfPtr
    // propEventSink
    // propIoMgr
    CBdgeProxyReadWriteComplete ( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CBdgeProxyReadWriteComplete ) ); }
    virtual gint32 OnIrpComplete( PIRP pIrp );
    virtual gint32 RunTask()
    { return STATUS_PENDING; }
};

class CRpcRouterReqFwdr : public CRpcRouter
{
    // local registered matches
    // to check the validity of the outgoing requests
    // the key is the matchptr 
    // the value is reference count
    std::map< MatchPtr, gint32 > m_mapLocMatches;

    // this service listening on dbus to receive
    // the req from the client and send the
    // messages via tcp socket
    InterfPtr           m_pReqFwdr;
    protected:

    std::map< RegObjPtr, guint32 > m_mapRefCount;
    MatchPtr            m_pDBusSysMatch;

    gint32 OnPreStopLongWait(
        IEventSink* pCallback );

    gint32 OnRmtSvrOnline(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort )
    { return 0; }

    gint32 OnRmtSvrOffline(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 OnRmtSvrOfflineMH(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    bool IsProxyInUse( guint32 dwPortId );

    public:
    typedef CRpcRouter super;
    CRpcRouterReqFwdr( const IConfigDb* pCfg );

    const EnumClsid GetIid() const
    { return iid( CRpcRouterReqFwdr ); }

    inline gint32 GetReqFwdr(
        InterfPtr& pIf )
    {
        pIf = m_pReqFwdr;
        return 0;
    }

    gint32 Start();

    bool HasReqForwarder() const override
    { return true; }

    // check if the match is valid before it is
    // registered
    gint32 CheckMatch( IMessageMatch* pMatch );

    gint32 StartReqFwdr(
        InterfPtr& pReqFwder,
        IEventSink* pCallback );

    gint32 AddRefCount(
        guint32 dwPortId,
        const std::string& strSrcUniqName,
        const std::string& strSrcDBusName );

    virtual gint32 DecRefCount(
        guint32 dwPortId,
        const std::string& strSrcUniqName,
        const std::string& strSrcDBusName );

    gint32 ClearRefCountByPortId(
        guint32 dwPortId,
        std::vector< std::string >& vecUniqNames );

    gint32 ClearRefCountByUniqName(
        const std::string& strUniqName,
        std::set< guint32 >& setPortIds );

    gint32 ClearRefCountBySrcDBusName(
        const std::string& strName,
        std::set< guint32 >& setPortIds );

    gint32 GetRefCountByPortId(
        guint32 dwPortId );

    gint32 GetRefCountByUniqName(
        const std::string& strUniqName );

    gint32 GetRefCountBySrcDBusName(
        const std::string& strName );

    gint32 AddLocalMatch(
        IMessageMatch* pMatch );

    gint32 RemoveLocalMatch(
        IMessageMatch* pMatch );

    gint32 RemoveLocalMatchByPortId(
        guint32 dwPortId );

    gint32 RemoveLocalMatchByUniqName(
        const std::string& strUniqName,
        std::vector< MatchPtr >& vecMatches );

    gint32 RemoveLocalMatchByPath(
        const std::string& strPath,
        guint32 dwProxyPortId,
        std::vector< MatchPtr >& vecMatches );

    // local match operation
    gint32 SetMatchOnline(
        IMessageMatch* pMatch,
        bool bOnline );

    gint32 IsMatchOnline(
        IMessageMatch* pMatch,
        bool& bOnline ) const;

    // filter the request with the local side
    // match table. The req cannot make to the
    // destination if it failes the tests against
    // the match table
    virtual gint32 CheckReqToFwrd(
        IConfigDb* pReqCtx,
        DMsgPtr& pMsg,
        MatchPtr& pMatch );

    // filter the event with the local side
    // match table. The event cannot make to the
    // destination if it failes the tests against
    // the match tables
    virtual gint32 CheckEvtToRelay(
        IConfigDb* pEvtCtx,
        DMsgPtr& pMsg,
        std::vector< MatchPtr >& vecMatches );

    gint32 OnRmtSvrEvent(
        EnumEventId iEvent,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 FindUniqNamesByPortId(
        guint32 dwPortId,
        std::set< stdstr >& setNames );

};

class CRegObjectBridge
{
    guint32 m_dwPortId = 0;
    guint32 m_dwProxyPortId = 0;
    std::string m_strNodeName;

    public:
    CRegObjectBridge()
    {}

    CRegObjectBridge( const CRegObjectBridge& rhs )
    {
        m_dwProxyPortId = rhs.m_dwProxyPortId;
        m_dwPortId = rhs.m_dwPortId;
        m_strNodeName = rhs.m_strNodeName;
    }

    CRegObjectBridge( 
        const std::string& strNode,
        guint32 dwPortId,
        guint32 dwProxyId = 0 )
    {
        m_dwProxyPortId = dwProxyId;
        m_dwPortId = dwPortId;
        m_strNodeName = strNode;
    }

    gint32 SetProperties(
        const std::string& strNodeName,
        guint32 dwPortId,
        guint32 dwProxyId = 0 )
    {
        if( dwPortId == 0 ||
            strNodeName.empty() )
            return -EINVAL;

        m_dwPortId = dwPortId;
        m_dwProxyPortId = dwProxyId;
        return 0;
    }

    guint32 GetPortId() const
    { return m_dwPortId; }

    guint32 GetProxyPortId() const
    { return m_dwProxyPortId; }

    const std::string& GetNodeName() const
    { return m_strNodeName; }

    // test if the match belongs to this
    // registered interface
    gint32 IsMyMatch( IMessageMatch* pMatch )
    {
        gint32 ret = 0;
        CCfgOpenerObj oMatch( pMatch );
        do{
            ret = oMatch.IsEqual(
                propPortId, m_dwPortId );
            if( ERROR( ret ) )
                break;

            std::string strPath;
            ret = oMatch.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
                break;

            std::string strNode;
            ret = CRpcRouter::GetNodeName(
                strPath, strNode );
            if( ERROR( ret ) )
                break;

            if( strNode!= m_strNodeName )
            {
                ret = ERROR_FALSE;
                break;
            }

        }while( 0 );

        return ret;
    }

    bool operator<( const CRegObjectBridge& rhs ) const
    {
        if( m_dwPortId < rhs.m_dwPortId )
            return true;

        if( m_strNodeName < rhs.m_strNodeName )
            return true;

        return false;
    }
};

struct ILoadBalancer : public IEventSink
{
    virtual gint32 GetNodesAvail(
        const std::string& strGrpName,
        std::vector< std::string >& vecNodes ) = 0;   

    gint32 OnEvent(
        EnumEventId iEvent,
        LONGWORD    dwParam1 = 0,
        LONGWORD    dwParam2 = 0,
        LONGWORD*   pData = NULL  ) 
    { return 0; }
};

class CRedudantNodes : public ILoadBalancer
{

    typedef std::map< std::string,
        std::deque< std::string > > LBGRP_MAP;
    typedef LBGRP_MAP::iterator ITER_LBGRPMAP;

    LBGRP_MAP  m_mapLBGrp;
    IEventSink* m_pParent;

    public:
    typedef ILoadBalancer super;
    CRedudantNodes( const IConfigDb* pCfg );
    gint32 GetNodesAvail(
        const std::string& strGrpName,
        std::vector< std::string >& vecNodes );
    gint32 LoadLBInfo( Json::Value& oLBInfo );
};

class CRpcRouterBridge : public CRpcRouter
{
    // the key is the peer ip-addr plus peer port-number
    std::map< guint32, InterfPtr > m_mapPortId2Bdge;

    // remote registered matches
    // to route the outgoing events to the subscriber
    // the key is the matchptr 
    // the value is the reference count
    std::map< MatchPtr, gint32 > m_mapRmtMatches;

    // this interface listening on the dbus of
    // server side to relay the request from
    // remote client to the remote server object
    std::map< std::string, InterfPtr > m_mapReqProxies;

    ObjPtr m_pLBGrp;

    gint32 OnRmtSvrOnline(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 OnRmtSvrOfflineMH(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 OnClearRemoteEventsComplete(
        IEventSink* pCallback,
        IEventSink*  pIoReq,
        IConfigDb* pReqCtx );

    gint32 BuildNodeMap();

    #define RFC_HISTORY_LEN     1440
    struct REQ_LIMIT
    {
        guint32 dwMaxReqs;
        guint32 dwMaxPendigns;
        bool operator==( REQ_LIMIT& rhs )
        {
            return ( dwMaxReqs == rhs.dwMaxReqs &&
                dwMaxPendigns == rhs.dwMaxPendigns );
        }
    };
    using HISTORY_ELEM = std::pair< guint64, REQ_LIMIT >;
    std::deque< HISTORY_ELEM > m_queHistory;

    protected:

    std::map< std::string, guint32 > m_mapNode2Pid;
    std::map< std::string, CfgPtr > m_mapNode2ConnParams;

    std::map< CRegObjectBridge, gint32 > m_mapRefCount;

    gint32 ForwardModOnOfflineEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    gint32 OnPreStopLongWait(
        IEventSink* pCallback );

    // methods of CRpcBaseOperations
    virtual gint32 OnModEvent(
        EnumEventId iEvent,
        const std::string& strModule );

    virtual gint32 OnRmtSvrOffline(
        IEventSink* pCallback,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    virtual gint32 OnPostStart(
        IEventSink* pContext );

    gint32 BuildRefreshReqLimit(
        CfgPtr& pRequest,
        guint32 dwMaxReqs,
        guint32 dwMaxPendings );

    private:

    inline gint32 AddProxy(
        const std::string& strDest,
        InterfPtr& pIf )
    {
        if( !IsConnected() )
            return ERROR_STATE;

        CStdRMutex oRouterLock( GetLock() );
        std::map< std::string, InterfPtr >::iterator
            itr = m_mapReqProxies.find( strDest );
        if( itr == m_mapReqProxies.end() )
        {
            m_mapReqProxies[ strDest ] = pIf;
            return 0;
        }
        else
        {
            if( itr->second == pIf )
                return EEXIST;
        }
        return -EEXIST;
    }

    inline gint32 RemoveProxy(
        const std::string& strDest,
        const InterfPtr& pIf )
    {
        gint32 ret = -ENOENT;
        CStdRMutex oRouterLock( GetLock() );
        std::map< std::string, InterfPtr >::iterator
            itr = m_mapReqProxies.find( strDest );
        if( itr != m_mapReqProxies.end() )
        {
            if( pIf == itr->second )
            {
                m_mapReqProxies.erase( itr );
                ret = 0;
            }
        }
        return ret;
    }

    gint32 RemoveRemoteMatchByPath(
        const std::string& strUpstm,
        std::vector< MatchPtr >& vecMatches );

    public:

    typedef CRpcRouter super;
    CRpcRouterBridge( const IConfigDb* pCfg );

    using IFMAP_CITR =
        std::map< std::string, InterfPtr >::const_iterator;

    bool HasBridge() const override
    { return true; }

    inline gint32 GetReqFwdrProxy(
        const std::string& strDest,
        InterfPtr& pIf ) const
    {
        CStdRMutex oRouterLock( GetLock() );
        IFMAP_CITR citr =
            m_mapReqProxies.find( strDest );

        if( citr == m_mapReqProxies.cend() )
            return -ENOENT;

        pIf = citr->second;
        return 0;
    }

    const EnumClsid GetIid() const
    { return iid( CRpcRouterBridge ); }

    gint32 AddRemoteMatch(
        IMessageMatch* pMatch );

    gint32 RemoveRemoteMatch(
        IMessageMatch* pMatch );

    gint32 FindRemoteMatchByPortId(
        guint32 dwPortId,
        std::vector< MatchPtr >& vecMatches,
        bool bRemove = false );

    gint32 RemoveRemoteMatchByPortId(
        guint32 dwPortId,
        std::vector< MatchPtr >& vecMatches );

    gint32 RemoveRemoteMatchByNodeName(
        const std::string& strNode,
        std::vector< MatchPtr >& vecMatches );

    inline gint32 GetObjAddr(
        IMessageMatch* pMatch,
        std::string& strDest )
    {
        if( pMatch == nullptr )
            return -EINVAL;

        CCfgOpenerObj oMatch( pMatch );
        gint32 ret = oMatch.GetStrProp(
            propDestDBusName, strDest );

        return ret;
    }

    inline gint32 GetReqFwdrProxy(
        IMessageMatch* pMatch,
        InterfPtr& pIf )
    {
        if( pMatch == nullptr )
            return -EINVAL;

        std::string strDest;
        gint32 ret = GetObjAddr( pMatch, strDest );
        if( ERROR( ret ) )
            return ret;

        return GetReqFwdrProxy( strDest, pIf );
    }

    inline gint32 AddConnParamsByNodeName(
        const std::string& strNode,
        const CfgPtr& pConnParams )
    {
        if( strNode.empty() )
            return -EINVAL;
        
        CStdRMutex oRouterLock( GetLock() );
        std::map< std::string, CfgPtr >::const_iterator 
            itr = m_mapNode2ConnParams.find(
                strNode );
        if( itr != m_mapNode2ConnParams.end() )
            return -EEXIST;

        m_mapNode2ConnParams[ strNode ] = pConnParams;
        return 0;
    }

    inline gint32 RemoveConnParamsByNodeName(
        const std::string& strNode )
    {
        if( strNode.empty() )
            return -EINVAL;
        
        CStdRMutex oRouterLock( GetLock() );
        size_t ret =
            m_mapNode2ConnParams.erase( strNode );

        if( ret == 0 )
            ret = -ENOENT;

        return 0;
    }

    inline gint32 GetConnParamsByNodeName(
        const std::string& strNode,
        CfgPtr& pConnParams ) const
    {
        if( strNode.empty() )
            return -EINVAL;
        
        CStdRMutex oRouterLock( GetLock() );
        std::map< std::string, CfgPtr >::const_iterator 
            itr = m_mapNode2ConnParams.find(
                strNode );
        if( itr == m_mapNode2ConnParams.end() )
            return -ENOENT;

        pConnParams = itr->second;
        return 0;
    }

    inline gint32 AddProxy(
        IMessageMatch* pMatch,
        InterfPtr& pIf )
    {
        if( pMatch == nullptr )
            return -EINVAL;

        std::string strDest;
        gint32 ret =
            GetObjAddr( pMatch, strDest );

        if( ERROR( ret ) )
            return ret;

        return AddProxy( strDest, pIf );
    }

    inline gint32 RemoveProxy(
        IMessageMatch* pMatch,
        const InterfPtr& pIf )
    {
        if( pMatch == nullptr )
            return -EINVAL;

        std::string strDest;
        gint32 ret =
            GetObjAddr( pMatch, strDest );

        if( ERROR( ret ) )
            return ret;

        return RemoveProxy( strDest, pIf );
    }

    gint32 AddToNodePidMap(
        const std::string& strNode,
        guint32 dwPortId );

    gint32 RemoveFromNodePidMap(
        const std::string& strNode );

    gint32 BuildStartRecvTask(
        IMessageMatch* pMatch,
        TaskletPtr& pTask );

    gint32 BuildStartStopReqFwdrProxy( 
        IMessageMatch* pMatch,
        bool bStart,
        TaskletPtr& pTask );

    gint32 BuildEventRelayTask(
        IMessageMatch* pMatch,
        bool bAdd,
        TaskletPtr& pTask );

    gint32 BuildAddMatchTask(
        IMessageMatch* pMatch,
        bool bEnable,
        TaskletPtr& pTask );

    gint32 RunEnableEventTask(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    gint32 BuildDisEvtTaskGrp(
        IEventSink* pCallback,
        IMessageMatch* pMatch,
        TaskletPtr& pTask );

    gint32 RunDisableEventTask(
        IEventSink* pCallback,
        IMessageMatch* pMatch );

    gint32 GetBridge(
        guint32 dwPortId,
        InterfPtr& pIf );

    gint32 AddBridge( 
        IGenericInterface* pIf );

    gint32 RemoveBridge( 
        IGenericInterface* pIf );

    // filter the request with the remote side
    // match table. The req cannot make to the
    // destination if it failes the tests against
    // the match table
    virtual gint32 CheckReqToRelay(
        IConfigDb* pReqCtx,
        DMsgPtr& pMsg,
        MatchPtr& pMatchHit );

    // filter the event with the remote side
    // match table. The event cannot make to the
    // destination if it failes the tests against
    // the match table
    virtual gint32 CheckEvtToFwrd(
        IConfigDb* pReqCtx,
        DMsgPtr& pMsg,
        std::set< guint32 >& setPortIds );

    gint32 AddRefCount(
        const std::string& strNodeName,
        guint32 dwPortId,
        guint32 dwProxyPortId );

    gint32 DecRefCount(
        const std::string& strNodeName,
        guint32 dwPortId );

    gint32 ClearRefCountByPortId(
        guint32 dwPortId,
        std::vector< std::string >& vecNodes );

    gint32 ClearRefCountByNodeName(
        const std::string& strUniqName,
        std::set< guint32 >& setPortIds );

    gint32 FindRefCount(
        const std::string& strNode,
        guint32 dwPortId,
        guint32 dwProxyPortId = 0 );

    // get bridgeproxy's refcount by the bridge's
    // portid
    gint32 GetRefCountByPortId(
        guint32 dwPortId );

    gint32 GetRefCountByNodeName(
        const std::string& strNodeName );

    using CRpcRouter::GetBridgeProxy;

    gint32 GetBridgeProxy(
        const std::string& strNode,
        InterfPtr& pIf );

    gint32 GetBridgeProxyByPath(
        const std::string& strPath,
        InterfPtr& pIf );

    // get bridgeproxy's portid by node name
    gint32 GetProxyIdByNodeName(
        const std::string& strNode,
        guint32& dwPortId ) const;

    gint32 OnRmtSvrEvent(
        EnumEventId iEvent,
        IConfigDb* pEvtCtx,
        HANDLE hPort );

    gint32 ClearRemoteEventsMH(
        IEventSink* pCallback,
        ObjPtr& pVecMatches,
        ObjPtr& pMapTaskIds,
        bool bForceClear );

    gint32 BuildRmtSvrEventMH(
        EnumEventId iEvent,
        IConfigDb* pCtx,
        CfgPtr& pEvtReq );

    gint32 GetLBNodes(
        const std::string& strGrpName,
        std::vector< std::string >& vecNodes );

    inline gint32 GetBridgeCount() const
    {
        CStdRMutex oRouterLock( GetLock() );
        return m_mapPortId2Bdge.size();
    }

    gint32 GetCurConnLimit(
        guint32& dwMaxReqs,
        guint32& dwMaxPendings,
        bool bNew = false ) const;

    gint32 AddToHistory(
        guint32 dwMaxReqs,
        guint32 dwMaxPendings );

    gint32 RefreshReqLimit();

};

class CIfRouterMgrState : public CIfServerState
{
    public:
    typedef CIfServerState super;
    CIfRouterMgrState ( const IConfigDb* pCfg )
        : super( pCfg )
    { SetClassId( clsid( CIfRouterMgrState ) ); }
    gint32 SubscribeEvents();
};

class CRpcRouterManager : public CRpcRouter
{
    std::vector< InterfPtr > m_vecRoutersBdge;
    InterfPtr m_pRouterReqFwdr;
    guint32   m_dwRole = 1;
    bool      m_bRfc = false;
    TaskletPtr m_pRfcChecker;

    protected:
    gint32 RebuildMatches();

    public:
    typedef CRpcRouter super;

    CRpcRouterManager( const IConfigDb* pCfg );
    ~CRpcRouterManager();

    const EnumClsid GetIid() const
    { return iid( CRpcRouterManager ); }

    gint32 GetRouters(
        std::vector< InterfPtr >& vecRouters ) const;

    gint32 Start();
    gint32 OnPreStop(
        IEventSink* pCallback ); 

    inline bool IsRfcEnabled() const
    { return m_bRfc; }

    gint32 RefreshReqLimit();
};

DECLARE_AGGREGATED_SERVER(
     CRpcRouterManagerImpl,
     CRpcRouterManager );

DECLARE_AGGREGATED_SERVER(
     CRpcRouterReqFwdrImpl,
     CRpcRouterReqFwdr,
     CStatCountersServer ); 

DECLARE_AGGREGATED_SERVER(
     CRpcRouterBridgeImpl,
     CRpcRouterBridge,
     CStatCountersServer ); 

DECLARE_AGGREGATED_PROXY(
    CRpcReqForwarderProxyImpl,
    CRpcReqForwarderProxy,
    CStatCountersProxy );

}

#include "stmrelay.h"
#include "streammh.h"

namespace rpcf
{

DECLARE_AGGREGATED_SERVER(
    CRpcReqForwarderImpl,
    CRpcReqForwarder,
    CStatCountersServer ); 

DECLARE_AGGREGATED_SERVER(
    CRpcTcpBridgeImpl,
    CRpcTcpBridge,
    CStatCountersServer,
    CStreamServerRelay,
    CStreamServerRelayMH
    ); 

DECLARE_AGGREGATED_PROXY(
    CRpcTcpBridgeProxyImpl,
    CRpcTcpBridgeProxy,
    CStatCountersProxy,
    CStreamProxyRelay );

#define AUTH_DEST( _pObj ) \
( { \
    std::string strRtName; \
    CCfgOpenerObj oCfg( _pObj ); \
    gint32 ret = oCfg.GetStrProp( \
        propSvrInstName, strRtName ); \
    SUCCEEDED( ret ) ? DBUS_DESTINATION2( strRtName, \
        OBJNAME_ROUTER_BRIDGE_AUTH ) : strRtName; \
} )

class CIfDeferCallTaskEx2 :
    public CIfDeferCallTaskBase< CIfRollbackableTask >
{
    public:
    typedef CIfDeferCallTaskBase< CIfRollbackableTask > super;
    CIfDeferCallTaskEx2 ( const IConfigDb* pCfg )
        :super( pCfg )
    { SetClassId( clsid( CIfDeferCallTaskEx2 ) );}
};

template < typename C, typename ... Types, typename ...Args>
inline gint32 NewIfDeferredCall2( EnumClsid iTaskClsid,
    TaskletPtr& pCallback, ObjPtr pIf, gint32(C::*f)(Types ...),
    Args&&... args )
{
    TaskletPtr pWrapper;
    gint32 ret = NewDeferredCall( pWrapper, pIf, f, args... );
    if( ERROR( ret ) )
        return ret;

    TaskletPtr pIfTask;
    CParamList oParams;
    oParams[ propIfPtr ] = pIf;
    ret = pIfTask.NewObj(
        iTaskClsid, oParams.GetCfg() );
    if( ERROR( ret ) )
        return ret;

    if( iTaskClsid == clsid( CIfDeferCallTaskEx2 ) )
    {
        CIfDeferCallTaskEx2* pDeferTask = pIfTask;
        pDeferTask->SetDeferCall( pWrapper );
        pCallback = pDeferTask;
    }
    else
    {
        return -ENOTSUP;
    }
    return 0;
}

#define DEFER_IFCALLEX2_NOSCHED( _pTask, pObj, func, ... ) \
    NewIfDeferredCall2( clsid( CIfDeferCallTaskEx2 ), \
        _pTask, pObj, func , ##__VA_ARGS__ )

// use this macro when the _pTask will be added
// to a task group.
#define DEFER_IFCALLEX2_NOSCHED2( _pos, _pTask, pObj, func, ... ) \
({ \
    gint32 _ret = NewIfDeferredCall2( clsid( CIfDeferCallTaskEx2 ), \
        _pTask, pObj, func , ##__VA_ARGS__ ); \
    if( SUCCEEDED( _ret ) ) \
    { \
        CIfDeferCallTaskEx2* pDefer = _pTask; \
        BufPtr pBuf( true ); \
        *pBuf = ObjPtr( pDefer ); \
        pDefer->UpdateParamAt( _pos, pBuf );  \
    } \
    _ret; \
})

class CIfParallelTaskGrpRfc :
    public CIfParallelTaskGrp
{
    guint32 m_dwMaxRunning = RFC_MAX_REQS;
    guint32 m_dwMaxPending = RFC_MAX_PENDINGS;
    guint32 m_dwTaskAdded = 0;
    guint32 m_dwTaskRejected = 0;

    public:
    typedef CIfParallelTaskGrp super;
    CIfParallelTaskGrpRfc( const IConfigDb* pCfg );

    gint32 SetLimit(
        guint32 dwMaxRunning,
        guint32 dwMaxPending );

    inline guint32 GetMaxRunning() const
    { return m_dwMaxRunning; }

    inline guint32 GetMaxPending() const
    { return m_dwMaxPending; }

    inline guint32 GetRunningCount() const
    { return m_setTasks.size() - 1; }

    virtual gint32 RunTaskInternal(
        guint32 dwContext ) override;

    virtual gint32 OnChildComplete(
        gint32 ret, CTasklet* pChild ) override;

    gint32 AddAndRun( TaskletPtr& pTask );

    virtual gint32 AppendTask(
        TaskletPtr& pTask ) override ;

    virtual gint32 InsertTask(
        TaskletPtr& pTask ) override ;
};

#define IS_SVRMODOFFLINE_EVENT( __pEvtMsg ) \
( __pEvtMsg.GetMember() == "NameOwnerChanged" && \
    __pEvtMsg.GetInterface() == DBUS_SYS_INTERFACE )
}
