/*
 * =====================================================================================
 *
 *       Filename:  reqfwdr.cpp
 *
 *    Description:  implementation of CRpcReqForwarder interface
 *
 *        Version:  1.0
 *        Created:  06/25/2017 09:50:22 AM
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

#include "configdb.h"
#include "defines.h"
#include "proxy.h"
#include "rpcroute.h"
#include "dbusport.h"

namespace rpcf
{

using namespace std;

// ---interface related information---
//
// ----open port information----
//
// propPortClass={DBusLocalPdo/DBusProxyPdo/...}
// propPortId={bus_name/ipaddr:bus_name/...}
//
// ----dbus related information----
//
// propObjPath=`object path'
// propIfName=`interface name'
// propDestDBusName=`dest module name'
// propSrcDBusName=`src module name'

CRpcReqForwarder::CRpcReqForwarder(
    const IConfigDb* pCfg )
    : CAggInterfaceServer( pCfg ),
    super( pCfg )
{
    gint32 ret = 0;
    do{
        ObjPtr pObj;
        CCfgOpener oCfg( pCfg );

        ret = oCfg.GetObjPtr(
            propRouterPtr, pObj );

        if( ERROR( ret ) )
            break;

        m_pParent = pObj;
        if( m_pParent == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        RemoveProperty( propRouterPtr );

        CIoManager* pMgr = GetIoMgr();
        ret = pMgr->GetCmdLineOpt(
            propSepConns, m_bSepConns );
        if( ERROR( ret ) )
            m_bSepConns = false;

        ret = 0;

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error occurs in CRpcReqForwarder ctor" );
        throw runtime_error( strMsg );
    }
}

gint32 CRpcReqForwarder::InitUserFuncs()
{
    BEGIN_IFHANDLER_MAP( CRpcReqForwarder );

    ADD_SERVICE_HANDLER(
        CRpcReqForwarder::OpenRemotePort,
        SYS_METHOD_OPENRMTPORT );

    ADD_SERVICE_HANDLER(
        CRpcReqForwarder::CloseRemotePort,
        SYS_METHOD_CLOSERMTPORT );

    ADD_SERVICE_HANDLER(
        CRpcReqForwarder::EnableRemoteEvent,
        SYS_METHOD_ENABLERMTEVT );

    ADD_SERVICE_HANDLER(
        CRpcReqForwarder::DisableRemoteEvent,
        SYS_METHOD_DISABLERMTEVT );

    END_HANDLER_MAP;

    return 0;
}

gint32 CRpcReqForwarder::CheckReqToFwrd(
    IConfigDb* pTransCtx,
    DMsgPtr& pMsg,
    MatchPtr& pMatchHit )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );

    if( unlikely( pRouter == nullptr ) )
        return -EFAULT;

    return pRouter->CheckReqToFwrd(
        pTransCtx, pMsg, pMatchHit );
}

gint32 CRpcReqForwarder::CreateGrpRfc(
    guint32 dwPortId,
    const stdstr& strUniqName,
    const stdstr& strSdName )
{
    gint32 ret = 0;
    do{
        GRPRFC_KEY oKey( dwPortId, strUniqName );
        CStdRMutex oLock( GetLock() );
        std::map< GRPRFC_KEY, TaskGrpPtr >::iterator
            itr = m_mapGrpRfcs.find( oKey );
        if( itr != m_mapGrpRfcs.end() )
            break;

        TaskGrpPtr pGrp;
        CCfgOpener oParams;
        oParams.SetPointer( propIfPtr, this );
        oParams[ propSrcUniqName ] = strUniqName;
        oParams[ propPrxyPortId ] = dwPortId;

        ret = pGrp.NewObj(
            clsid( CIfParallelTaskGrpRfc ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        // a placeholder task to prevent
        // m_pGrpRfc from quitting
        oParams.RemoveProperty( propSrcUniqName );
        oParams.RemoveProperty( propPrxyPortId );

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CIfCallbackInterceptor ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        pGrp->AppendTask( pTask );
        TaskletPtr pGrpTask = pGrp;
        m_mapGrpRfcs[ oKey ] = pGrp;
        m_mapUq2SdName[ strUniqName ] = strSdName;
        oLock.Unlock();

        ret = this->AddAndRun( pGrpTask );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::RemoveGrpRfc(
    guint32 dwPortId,
    const stdstr& strUniqName )
{
    gint32 ret = 0;
    do{
        CStdRMutex oLock( GetLock() );
        std::map< GRPRFC_KEY, TaskGrpPtr >::iterator
            itr = m_mapGrpRfcs.find(
                { dwPortId, strUniqName } );
        if( itr == m_mapGrpRfcs.end() )
        {
            ret = -ENOENT;
            break;
        }

        TaskGrpPtr pGrp = itr->second;
        m_mapGrpRfcs.erase(
            { dwPortId, strUniqName } );
        m_mapUq2SdName.erase( strUniqName );
        oLock.Unlock();

        ( *pGrp )( eventCancelTask );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::RemoveGrpRfcs(
    guint32 dwPortId )
{
    gint32 ret = 0;
    do{
        std::vector< TaskGrpPtr > vecGrps;
        CStdRMutex oLock( GetLock() );
        std::map< GRPRFC_KEY, TaskGrpPtr >::iterator
            itr = m_mapGrpRfcs.begin();
        while( itr != m_mapGrpRfcs.end() )
        {
            if( itr->first.first == dwPortId )
            {
                vecGrps.push_back( itr->second );
                itr = m_mapGrpRfcs.erase( itr );
                continue;
            }
            ++itr;
        }
        oLock.Unlock();

        for( auto elem : vecGrps )
            ( *elem )( eventCancelTask );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::RemoveGrpRfcs(
    const stdstr& strUniqName )
{
    gint32 ret = 0;
    do{
        std::vector< TaskGrpPtr > vecGrps;
        CStdRMutex oLock( GetLock() );
        std::map< GRPRFC_KEY, TaskGrpPtr >::iterator
            itr = m_mapGrpRfcs.begin();
        while( itr != m_mapGrpRfcs.end() )
        {
            if( itr->first.second == strUniqName )
            {
                vecGrps.push_back( itr->second );
                itr = m_mapGrpRfcs.erase( itr );
                continue;
            }
            ++itr;
        }

        m_mapUq2SdName.erase( strUniqName );
        oLock.Unlock();

        for( auto elem : vecGrps )
            ( *elem )( eventCancelTask );

    }while( 0 );

    return ret;
}

gint32 CReqFwdrCloseRmtPortTask::RunTask()
{
    gint32 ret = 0;    
    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        CRpcTcpBridgeProxy* pProxy = nullptr;
        ret = oCfg.GetPointer( 0, pProxy );
        if( ERROR( ret ) )
            break;

        CRpcRouter* pRouter =
            pProxy->GetParent();

        pRouter->RemoveBridgeProxy( pProxy );
        ret = pProxy->Shutdown( this );

    }while( 0 );

    if( ret != STATUS_PENDING )
        OnTaskComplete( ret );

    return ret;
}


gint32 CReqFwdrCloseRmtPortTask::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = 0;    
    CParamList oTaskCfg(
        ( IConfigDb* )GetConfig() );

    do{
        CParamList oParams;
        oParams[ propReturnValue ] = iRetVal;

        CRpcReqForwarder* pIf = nullptr;

        ret = oTaskCfg.GetPointer(
            propIfPtr, pIf );

        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        ret = oTaskCfg.GetIntProp(
            propPortId, dwPortId );

        if( SUCCEEDED( ret ) )
        {
            CRpcRouterReqFwdr* pRouter =
                static_cast< CRpcRouterReqFwdr* >
                    ( pIf->GetParent() );
            pRouter->RemoveLocalMatchByPortId(
                dwPortId );
        }

        EventPtr pEvt;
        ret = GetInterceptTask( pEvt );
        if( SUCCEEDED( ret ) )
        {
            pIf->SetResponse(
                pEvt, oParams.GetCfg() );
        }


    }while( 0 );

    oTaskCfg.ClearParams();

    return iRetVal;
}

gint32 CReqFwdrOpenRmtPortTask::AdvanceState()
{
    gint32 ret = 0;
    switch( m_iState )
    {
    case stateInitialized:
        {
            m_iState = stateStartBridgeProxy;
            break;
        }
    case stateStartBridgeProxy:
        {
            m_iState = stateDone;
            break;
        }
    default:
        {
            ret = ERROR_STATE;
            break;
        }
    }
    return ret;
}

gint32 CReqFwdrOpenRmtPortTask::CreateInterface(
    InterfPtr& pIf )
{
    gint32 ret = 0;
    do{
        CCfgOpener oCfg( ( IConfigDb* )GetConfig() );
        CIoManager* pMgr = nullptr;
        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        CRpcInterfaceServer* pReqFwdr = nullptr;
        ret = oCfg.GetPointer( propIfPtr, pReqFwdr );
        if( pReqFwdr == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CRpcRouter* pRouter =
            pReqFwdr->GetParent();

        // ---interface related information---
        //
        // ----open port information----
        //
        // propPortClass={DBusLocalPdo/DBusProxyPdo/...}
        // propPortId={bus_name/ipaddr:bus_name/...}
        //
        // ----dbus related information----
        //
        // propObjPath=`object path'
        // propIfName=`interface name'
        // propDestDBusName=`dest module name'
        // propSrcDBusName=`src module name'
        //
        // ---other properties
        // propIoMgr
        // propRouterPtr

        std::string strObjDesc;
        CCfgOpenerObj oIfCfg( pReqFwdr );
        ret = oIfCfg.GetStrProp(
            propObjDescPath, strObjDesc );

        if( ERROR( ret ) )
            break;

        CParamList oParams;
        oParams.CopyProp( propConnParams, this );
        IConfigDb* pConn = nullptr;
        ret = oParams.GetPointer(
            propConnParams, pConn );
        if( ERROR( ret ) )
            break;

        bool bAuth = false;
        EnumClsid iClsid =
            clsid( CRpcTcpBridgeProxyImpl );

        std::string strObjName =
            OBJNAME_TCP_BRIDGE;

        CConnParams oConn( pConn );
        bAuth = pRouter->HasAuth();
            // oConn.HasAuth();
        if( bAuth )
        {
            strObjName =
                OBJNAME_TCP_BRIDGE_AUTH;
            iClsid = clsid(
                CRpcTcpBridgeProxyAuthImpl );
        }

        ret = oParams.CopyProp(
            propSvrInstName, pRouter );
        if( ERROR( ret ) )
            break;

        oParams.SetPointer( propIoMgr, pMgr );
        ret = CRpcServices::LoadObjDesc(
            strObjDesc,
            strObjName,
            false,
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        oParams.CopyProp( propPortId, this );
        oParams[ propIfStateClass ] =
            clsid( CTcpBdgePrxyState );

        oParams.SetPointer(
            propRouterPtr, pRouter );

        pIf.Clear();

        ret = pIf.NewObj( iClsid,
            oParams.GetCfg() );

    }while( 0 );

    return ret;
}

gint32 CReqFwdrOpenRmtPortTask::OnServiceComplete(
    gint32 iRetVal )
{
    if( iRetVal == STATUS_PENDING ||
        Retriable( iRetVal ) )
        return -EINVAL;

    gint32 ret = 0;
    CRpcReqForwarder* pIf = nullptr;
    CParamList oParams;
    EventPtr pEvt;

    do{
        ret = GetInterceptTask( pEvt );
        if( ERROR( ret ) )
            break;

        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        ret = oCfg.GetPointer( propIfPtr, pIf );
        if( ERROR( ret ) )
            break;

        oParams[ propReturnValue ] = iRetVal;

        string strRouterPath;
        ret = oCfg.GetStrProp(
            propRouterPath, strRouterPath );

        if( ERROR( ret ) )
        {
            if( SUCCEEDED( iRetVal ) )
                oParams[ propReturnValue ] = ret;
            break;
        }

        if( ERROR( iRetVal ) )
        {
            ret = iRetVal;
            oParams[ propReturnValue ] = ret;
            break;
        }

        guint32 dwPortId = 0;
        ret = oCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
        {
            oParams.SetIntProp(
                propReturnValue,  ret );
            break;
        }

        if( strRouterPath == "/" )
        {
            oParams.SetIntProp(
                propConnHandle, dwPortId );

            oParams.CopyProp(
                propConnParams, this );

            oParams.SetStrProp(
                propRouterPath, strRouterPath );

            pIf->SetResponse( 
                pEvt, oParams.GetCfg() );

            break;
        }

        // pass the output to the new task
        CCfgOpener oReqCtx;
        oReqCtx[ propConnHandle ] = dwPortId;
        oReqCtx[ propRouterPath ] =
            strRouterPath;
        ret = oReqCtx.CopyProp(
            propConnParams, this );
        if( ERROR( ret ) )
            break;

        // for rollback 
        ret = oReqCtx.CopyProp(
            propSrcDBusName, this );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.CopyProp(
            propSrcUniqName, this );
        if( ERROR( ret ) )
            break;

        // schedule a checkrouterpath task
        TaskletPtr pChkRt;
        ret = DEFER_HANDLER_NOSCHED(
            pChkRt, ObjPtr( pIf ),
            &CRpcReqForwarder::CheckRouterPath,
            ( IEventSink* )pEvt,
            oReqCtx.GetCfg() );

        if( ERROR( ret ) )
        {
            oParams[ propReturnValue ] = ret;
            break;
        }

        SetInterceptTask( nullptr );
        CIoManager* pMgr = pIf->GetIoMgr();
        // a new task to release the seq task
        // queue
        TaskletPtr ptrTask( pChkRt );
        ret = pMgr->RescheduleTask( ptrTask );
        if( ERROR( ret ) )
        {
            SetInterceptTask( pEvt );
            ( *pChkRt )( eventCancelTask );
        }

    }while( 0 );

    if( ERROR( ret ) && pIf != nullptr )
    {
        pIf->SetResponse( 
            pEvt, oParams.GetCfg() );

    }

    return ret;
}

gint32 CReqFwdrOpenRmtPortTask::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = RunTaskInternal( iRetVal );

    if( ret == STATUS_PENDING )
        return ret;

    if( Retriable( ret ) )
        return STATUS_MORE_PROCESS_NEEDED;

    OnServiceComplete( ret );
    return ret;
}

gint32 CReqFwdrOpenRmtPortTask::RunTask()
{
    // call with an unlikely retval to indicate it
    // comes from RunTask
    gint32 ret = RunTaskInternal( 0x7fffffff );

    if( ret == STATUS_PENDING )
        return ret;

    if( Retriable( ret ) )
        return STATUS_MORE_PROCESS_NEEDED;

    OnServiceComplete( ret );

    return ret;
}

gint32 CReqFwdrOpenRmtPortTask::RunTaskInternal(
    gint32 iRetVal )
{
    gint32 ret = 0;
    CCfgOpener oCfg( ( IConfigDb* )GetConfig() );

    // five parameters needed:
    // propIoMgr
    // propIfPtr: the ptr to CRpcReqForwarder
    // propEventSink: the callback
    // propSrcDBusName: the source dbus
    // propConnParams: the destination ip address
    // propRouterPtr: the pointer to the router

    do{

        CIoManager* pMgr = nullptr;
        CRpcReqForwarder* pReqFwdr = nullptr;
        ObjPtr pObj;

        ret = AdvanceState();
        if( ERROR( ret ) )
            break;

        ret = oCfg.GetPointer(
            propIfPtr, pReqFwdr );

        if( ERROR( ret ) )
            break;

        CRpcRouterReqFwdr* pRouter =
            static_cast< CRpcRouterReqFwdr* >
                ( pReqFwdr->GetParent() );
        if( pRouter == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ObjPtr pConnParams;
        ret = oCfg.GetObjPtr(
            propConnParams, pConnParams );

        if( ERROR( ret ) )
            break;

        string strUniqName;
        ret = oCfg.GetStrProp(
            propSrcUniqName, strUniqName );
        if( ERROR( ret ) )
            break;

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        if( m_iState == stateDone )
        {
            if( SUCCEEDED( iRetVal ) ||
                iRetVal == 0x7fffffff )
            {
                ret = pRouter->
                    AddBridgeProxy( m_pProxy ); 
                
                if( ERROR( ret ) )
                    break;

                string strSender;
                ret = oCfg.GetStrProp(
                    propSrcDBusName, strSender );

                if( ERROR( ret ) )
                    break;

                CCfgOpenerObj oIfCfg(
                    ( CObjBase* )m_pProxy );
                // the portid should be ready
                // when the proxy is started
                // successfully
                guint32 dwPortId = 0;
                ret = oIfCfg.GetIntProp(
                    propPortId, dwPortId );

                if( ERROR( ret ) )
                    break;

                oCfg.SetIntProp(
                    propPortId, dwPortId );

                oCfg.CopyProp( propConnParams,
                    ( CObjBase* )m_pProxy );

                pReqFwdr->AddRefCount(
                    dwPortId,
                    strUniqName,
                    strSender );

                if( pReqFwdr->IsRfcEnabled() &&
                    !pRouter->HasAuth() )
                {
                    ret = pReqFwdr->CreateGrpRfc(
                        dwPortId, strUniqName,
                        strSender );
                    if( ERROR( ret ) )
                        break;
                }
            }
            else
            {
                CGenericInterface* pIf = m_pProxy;
                if( unlikely( pIf == nullptr ) )
                    break;

                EnumEventId iEvent = cmdShutdown;
                EnumIfState iState = pIf->GetState();
                if( iState == stateStartFailed )
                    iEvent = cmdCleanup;

                ret = pIf->SetStateOnEvent(
                    iEvent );

                if( ERROR( ret ) )
                    break;

                pMgr = pReqFwdr->GetIoMgr();
                DEFER_CALL( pMgr, ObjPtr( pIf ),
                    &CInterfaceProxy::StopEx,
                    pDummyTask );

                ret = iRetVal;
            }
        }
        else 
        {
            InterfPtr pBridgeIf;
            ret = CreateInterface( pBridgeIf );
            if( ERROR( ret ) )
                break;

            m_pProxy = pBridgeIf;
            ret = pBridgeIf->StartEx( this );
            if( ret == STATUS_PENDING )
                break;

            iRetVal = ret;
            continue;
        }
        break;

    }while( 1 );

    if( ret != STATUS_PENDING )
        m_pProxy.Clear();

    return ret;
}

gint32 CReqFwdrOpenRmtPortTask::OnCancel(
    guint32 dwContext )
{
    OnServiceComplete( -ECANCELED );
    super::OnCancel( dwContext );
    return AdvanceState();
}

// active disconnecting
gint32 CRpcReqForwarder::CloseRemotePort(
    IEventSink* pCallback,
    const IConfigDb* pCfg )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pCfg == nullptr )
        return -EINVAL;

    do{
        TaskletPtr pDeferTask;
        ret = DEFER_HANDLER_NOSCHED(
            pDeferTask, ObjPtr( this ),
            &CRpcReqForwarder::CloseRemotePortInternal,
            pCallback, const_cast< IConfigDb* >( pCfg ) );

        if( ERROR( ret ) )
            break;

        // using the rpcrouter's sequential task,
        // because the eventRmtSvrOffline comes
        // from the rpcrouter.
        ret = GetParent()->AddSeqTask(
            pDeferTask, false );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::ForwardRequest(
    IConfigDb* pReqCtx,
    DBusMessage* pFwdrMsg,
    DMsgPtr& pRespMsg,
    IEventSink* pCallback )
{
    if( pFwdrMsg == nullptr ||
        pCallback == nullptr ||
        pReqCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    pRespMsg.Clear();

    do{
        DMsgPtr fwdrMsg( pFwdrMsg );

        MatchPtr pMatch;
        ret = CheckReqToFwrd(
            pReqCtx, fwdrMsg, pMatch );

        // invalid request
        if( ERROR( ret ) )
            break;

        CParamList oParams;
        oParams.SetObjPtr(
            propIoMgr, GetIoMgr() );

        oParams.SetObjPtr(
            propEventSink, ObjPtr( pCallback ) );

        oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        CParamList oResp;

        oParams.SetObjPtr(
            propRespPtr, oResp.GetCfg() );

        oParams.SetPointer(
            propRouterPtr, GetParent() );

        CCfgOpenerObj oMatch(
            ( CObjBase* )pMatch ); 

        oParams.Push( pReqCtx );
        oParams.Push( fwdrMsg );

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CReqFwdrForwardRequestTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ( *pTask )( eventZero );

        ret = pTask->GetError();
        if( ret == STATUS_PENDING )
            break;

    }while( 0 );

    // the return value indicates if the response
    // message is generated or not.
    return ret;
}

gint32 CRpcReqForwarder::StopBridgeProxy(
    IEventSink* pCallback, guint32 dwPortId,
    const std::string& strSrcUniqName,
    const std::string& strSrcDBusName )
{
    gint32 ret = 0;

    do{
        InterfPtr pIf;

        ret = DecRefCount( dwPortId,
            strSrcUniqName,
            strSrcDBusName );

        if( ERROR( ret ) )
            break;

        if( ret > 0 )
        {
            ret = 0;
            break;
        }

        CRpcRouterReqFwdr* pRouter =
            static_cast< CRpcRouterReqFwdr* >
                ( GetParent() );

        ret = pRouter->GetBridgeProxy(
            dwPortId, pIf );
        if( ERROR( ret ) )
            break;

        CParamList oParams;
        if( pCallback != nullptr )
        {
            oParams.SetPointer(
                propEventSink, pCallback );
        }

        oParams.SetPointer( propIfPtr, this );
        oParams.Push( ObjPtr( pIf ) );
        oParams[ propPortId ] = dwPortId;

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CReqFwdrCloseRmtPortTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ( *pTask )( eventZero );
        ret = pTask->GetError();

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::CloseRemotePortInternal(
    IEventSink* pCallback,
    IConfigDb* pCfg )
{
    gint32 ret = 0;
    do{
        if( pCfg == nullptr ||
            pCallback == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CCfgOpener oCfg( pCfg );
        guint32 dwPortId = 0;

        ret = oCfg.GetIntProp(
            propConnHandle, dwPortId );

        if( ERROR( ret ) )
            break;

        string strSender;
        ret = oCfg.GetStrProp(
            propSrcDBusName, strSender );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oCfgInvoke( pCallback );
        DMsgPtr pMsg;

        ret = oCfgInvoke.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        string strSrcUniqName = pMsg.GetSender();
        if( strSrcUniqName.empty() )
        {
            ret = -EINVAL;
            break;
        }

        ret = StopBridgeProxy(
            pCallback, dwPortId,
            strSrcUniqName, strSender );

        break;

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        CParamList oParams;
        oParams[ propReturnValue ] = ret;
        SetResponse( pCallback, oParams.GetCfg() );
    }

    return ret;
}

gint32 CRpcReqForwarder::OpenRemotePort(
    IEventSink* pCallback,
    const IConfigDb* pcfg )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pcfg == nullptr )
        return -EINVAL;

    do{
        TaskletPtr pDeferTask;
        IConfigDb* pCfg =
            const_cast< IConfigDb* >( pcfg );

        ret = DEFER_HANDLER_NOSCHED(
            pDeferTask, ObjPtr( this ),
            &CRpcReqForwarder::OpenRemotePortInternal,
            pCallback, pCfg );

        if( ERROR( ret ) )
            break;

        ret = GetParent()->AddSeqTask(
            pDeferTask, false );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}
gint32 CRpcReqForwarder::OpenRemotePortInternal(
    IEventSink* pCallback,
    IConfigDb* pCfg )
{
    gint32 ret = 0;
    CParamList oResp;
    do{
        if( pCfg == nullptr ||
            pCallback == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwPortId = 0;
        CCfgOpener oCfg( pCfg );

        IConfigDb* pConnParams = nullptr;
        ret = oCfg.GetPointer(
            propConnParams, pConnParams );

        if( ERROR( ret ) )
            break;

        string strSender;
        ret = oCfg.GetStrProp(
            propSrcDBusName, strSender );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oCfgInvoke( pCallback );
        DMsgPtr pMsg;

        ret = oCfgInvoke.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        string strSrcUniqName = pMsg.GetSender();
        if( strSrcUniqName.empty() )
        {
            ret = -EINVAL;
            break;
        }

        CRpcRouter* pRouter = GetParent();
        CCfgOpener oConn( pConnParams );
        if( !pRouter->HasAuth() &&
            IsSepConns() )
        {
            oConn[ propSrcUniqName ] =
                strSrcUniqName;
        }
        else
        {
            ret = 0;
        }

        string strRouterPath;
        ret = oCfg.GetStrProp(
            propRouterPath, strRouterPath );
        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        ret = pRouter->GetBridgeProxy(
            pConnParams, pIf );

        if( SUCCEEDED( ret ) )
        {
            CCfgOpenerObj oIfCfg( 
                ( CObjBase* )pIf );

            ret = oIfCfg.GetIntProp(
                propPortId, dwPortId );
            
            if( ERROR( ret ) )
                break;

            ret = AddRefCount( dwPortId,
                strSrcUniqName, strSender );

            if( ERROR( ret ) )
                break;

            if( ret == 1 && IsRfcEnabled() &&
                !pRouter->HasAuth() )
            {
                ret = CreateGrpRfc(
                    dwPortId, strSrcUniqName,
                    strSender );
                if( ERROR( ret ) )
                    break;
            }

            if( strRouterPath == "/" )
            {
                oResp[ propConnHandle ] = dwPortId;
                oResp.CopyProp( propConnParams,
                    ( CObjBase* )pIf );

                oResp.SetStrProp(
                    propRouterPath, "/" );

                DebugPrint( ret,
                    "The bridge proxy already exists...,"\
                    "portid=%d, uniqName=%s, sender=%s",
                    dwPortId,
                    strSrcUniqName.c_str(),
                    strSender.c_str() );

                ret = 0;
                break;
            }
            // schedule a checkrouter path task
            CCfgOpener oReqCtx;
            oReqCtx[ propConnHandle ] = dwPortId;
            oReqCtx.SetStrProp(
                propRouterPath, strRouterPath );
            oReqCtx.CopyProp( propConnParams,
                ( CObjBase* )pIf );

            // for rollback
            oReqCtx.SetStrProp(
                propSrcUniqName, strSrcUniqName );

            oReqCtx.SetStrProp(
                propSrcDBusName, strSender );

            // schedule a checkrouterpath task
            TaskletPtr pChkRt;
            ret = DEFER_HANDLER_NOSCHED(
                pChkRt, ObjPtr( this ),
                &CRpcReqForwarder::CheckRouterPath,
                pCallback, oReqCtx.GetCfg() );
            if( ERROR( ret ) )
                break;

            // a new task to release the seq task
            // queue
            CIoManager* pMgr = GetIoMgr();
            ret = pMgr->RescheduleTask( pChkRt );
            if( SUCCEEDED( ret ) )
                ret = STATUS_PENDING;
        }
        else if( ret == -ENOENT )
        {
            CParamList oParams;

            oParams.CopyProp(
                propConnParams, pCfg );

            oParams.SetPointer(
                propIoMgr, GetIoMgr() );

            oParams.SetObjPtr( propEventSink,
                ObjPtr( pCallback ) );

            oParams.SetObjPtr( propIfPtr,
                ObjPtr( this ) );

            oParams.SetStrProp(
                propSrcDBusName, strSender );

            oParams.SetStrProp(
                propSrcUniqName, strSrcUniqName );

            oParams.CopyProp(
                propRouterPath, pCfg );

            TaskletPtr pTask;

            ret = pTask.NewObj(
                clsid( CReqFwdrOpenRmtPortTask ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            // must be pending
            ( *pTask )( eventZero );
            ret = pTask->GetError();
        }
        break;

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        oResp[ propReturnValue ] = ret;
        if( ERROR( ret ) )
        {
            oResp.RemoveProperty( propConnHandle );
            oResp.RemoveProperty( propConnParams );
        }
        SetResponse( pCallback, oResp.GetCfg() );
    }

    return ret;
}

gint32 CRpcReqForwarder::AddRefCount(
    guint32 dwPortId,
    const std::string& strSrcUniqName,
    const std::string& strSrcDBusName )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->AddRefCount( dwPortId,
        strSrcUniqName, strSrcDBusName );
}

// NOTE: the return value on success is the
// reference count to the ipAddress, but not the
// reference count to the regobj.
gint32 CRpcReqForwarder::DecRefCount(
    guint32 dwPortId,
    const std::string& strSrcUniqName,
    const std::string& strSrcDBusName )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->DecRefCount( dwPortId,
        strSrcUniqName, strSrcDBusName );
}

// for local client offline
gint32 CRpcReqForwarder::GetRefCountBySrcDBusName(
    const std::string& strSrcName )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->GetRefCountBySrcDBusName(
        strSrcName );
}

gint32 CRpcReqForwarder::FindUniqNamesByPortId(
    guint32 dwPortId,
    std::set< stdstr >& setNames )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->FindUniqNamesByPortId(
        dwPortId, setNames );
}

// for local client offline
gint32 CRpcReqForwarder::ClearRefCountBySrcDBusName(
    const std::string& strSrcName,
    std::set< guint32 >& setPortIds )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->ClearRefCountBySrcDBusName(
        strSrcName, setPortIds );
}

// for local client offline
gint32 CRpcReqForwarder::GetRefCountByUniqName(
    const std::string& strUniqName )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->GetRefCountByUniqName(
        strUniqName );
}

// for local client offline
gint32 CRpcReqForwarder::ClearRefCountByUniqName(
    const std::string& strUniqName,
    std::set< guint32 >& setPortIds )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->ClearRefCountByUniqName(
        strUniqName, setPortIds );
}

// for local client offline
gint32 CRpcReqForwarder::GetRefCountByPortId(
    guint32 dwPortId )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->GetRefCountByPortId(
        dwPortId );
}

gint32 CRpcReqForwarder::GetGrpRfcs(
    const stdstr& strUniqName,
    std::vector< TaskGrpPtr >& vecGrps ) const
{
    gint32 ret = 0;
    do{
        std::vector< TaskGrpPtr > vecGrps;
        CStdRMutex oLock( GetLock() );
        std::map< GRPRFC_KEY, TaskGrpPtr >::const_iterator
            itr = m_mapGrpRfcs.cbegin();
        while( itr != m_mapGrpRfcs.cend() )
        {
            if( itr->first.second == strUniqName )
                vecGrps.push_back( itr->second );
            ++itr;
        }
        oLock.Unlock();

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::GetGrpRfcs(
    guint32 dwPortId,
    std::vector< TaskGrpPtr >& vecGrps ) const
{
    gint32 ret = 0;
    do{
        std::vector< TaskGrpPtr > vecGrps;
        CStdRMutex oLock( GetLock() );
        std::map< GRPRFC_KEY, TaskGrpPtr >::const_iterator
            itr = m_mapGrpRfcs.cbegin();
        while( itr != m_mapGrpRfcs.cend() )
        {
            if( itr->first.first == dwPortId )
                vecGrps.push_back( itr->second );
            ++itr;
        }
        oLock.Unlock();

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::GetGrpRfc(
    GRPRFC_KEY& oKey,
    TaskGrpPtr& pGrp ) const
{
    CStdRMutex oLock( GetLock() );
    std::map< GRPRFC_KEY, TaskGrpPtr >::const_iterator
    itr = m_mapGrpRfcs.find( oKey );

    if( itr == m_mapGrpRfcs.cend() )
        return -ENOENT;

    pGrp = ObjPtr( itr->second );
    return 0;
}

gint32 CRpcReqForwarder::GetGrpRfc(
    DMsgPtr& pMsg,        
    TaskGrpPtr& pGrp )
{
    ObjPtr pObj;
    stdstr strUniqName = pMsg.GetSender();
    gint32 ret = pMsg.GetObjArgAt( 0, pObj );
    if( ERROR( ret ) )
        return ret;

    guint32 dwPortId = 0;
    CCfgOpener oReqCtx( ( IConfigDb* )pObj );
    ret = oReqCtx.GetIntProp(
        propConnHandle, dwPortId );
    if( ERROR( ret ) )
        return ret;

    GRPRFC_KEY oKey( dwPortId, strUniqName );
    return GetGrpRfc( oKey, pGrp );
}

gint32 CRpcReqForwarder::FindFwrdReqsAllRfc(
    const stdstr& strUniqName,
    FWRDREQS& vecTasks,
    bool bTaskId )
{
    gint32 ret = 0;
    do{
        std::vector< TaskGrpPtr > vecGrps;
        ret = GetGrpRfcs( strUniqName, vecGrps );
        if( ERROR( ret ) )
            break;

        for( auto elem : vecGrps )
        {
            CIfParallelTaskGrpRfc* pGrpRfc = elem;
            if( unlikely( pGrpRfc == nullptr ) )
                break;

            std::vector< TaskletPtr > vecAll;
            ret = pGrpRfc->FindTaskByClsid(
                clsid( CIfInvokeMethodTask ),
                vecAll );

            if( ERROR( ret ) )
                break;

            for( auto elem : vecAll )
            {
                guint64 qwTaskId = 0;
                if( bTaskId )
                {
                    ret = RetrieveTaskId(
                        elem, qwTaskId );
                    if( ERROR( ret ) )
                        continue;
                }
                vecTasks.push_back(
                    { elem, qwTaskId } );
            }
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::FindFwrdReqsAll(
    guint32 dwPortId,
    const stdstr& strUniqName,
    FWRDREQS& vecTasks,
    bool bTaskId )
{
    if( strUniqName.empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        TaskGrpPtr pGrp;
        std::vector< TaskletPtr > vecInvTasks;
        if( IsRfcEnabled() )
        {
            GRPRFC_KEY oKey( dwPortId, strUniqName );
            ret = GetGrpRfc( oKey, pGrp );
            if( ERROR( ret ) )
                break;
        }
        else
        {
            ret = GetParallelGrp( pGrp );
            if( ERROR( ret ) )
                break;
        }

        ret = pGrp->FindTaskByClsid(
            clsid( CIfInvokeMethodTask ),
            vecInvTasks );

        if( ERROR( ret ) || vecInvTasks.empty() )
            break;
        
        for( auto elem : vecInvTasks )
        {
            CCfgOpenerObj oInv(
                ( CObjBase* )elem );
            DMsgPtr pMsg;
            ret = oInv.GetMsgPtr(
                propMsgPtr, pMsg );
            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }
            if( pMsg.GetMember() != 
                SYS_METHOD_FORWARDREQ )
            {
                ret = 0;
                continue;
            }
            if( pMsg.GetSender() != strUniqName )
            {
                ret = 0;
                continue;
            }

            guint64 qwTaskId = 0;
            if( bTaskId )
            {
                ret = RetrieveTaskId(
                    elem, qwTaskId );
                if( ERROR( ret ) )
                    continue;
            }
            vecTasks.push_back(
                { elem , qwTaskId } );
        }

        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    if( ERROR( ret ) )
        vecTasks.clear();

    return ret;
}

gint32 CRpcReqForwarder::FindFwrdReqsAll(
    const stdstr& strUniqName,
    FWRDREQS& vecTasks,
    bool bTaskId )
{
    if( strUniqName.empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        TaskGrpPtr pGrp;
        std::vector< TaskletPtr > vecInvTasks;
        if( IsRfcEnabled() )
        {
            ret = FindFwrdReqsAllRfc(
                strUniqName, vecTasks, bTaskId );
            break;
        }
        else
        {
            ret = GetParallelGrp( pGrp );
            if( ERROR( ret ) )
                break;
        }

        ret = pGrp->FindTaskByClsid(
            clsid( CIfInvokeMethodTask ),
            vecInvTasks );

        if( ERROR( ret ) || vecInvTasks.empty() )
            break;
        
        for( auto elem : vecInvTasks )
        {
            CCfgOpenerObj oInv(
                ( CObjBase* )elem );
            DMsgPtr pMsg;
            ret = oInv.GetMsgPtr(
                propMsgPtr, pMsg );
            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }
            if( pMsg.GetMember() != 
                SYS_METHOD_FORWARDREQ )
            {
                ret = 0;
                continue;
            }
            if( pMsg.GetSender() != strUniqName )
            {
                ret = 0;
                continue;
            }

            guint64 qwTaskId = 0;
            if( bTaskId )
            {
                ret = RetrieveTaskId(
                    elem, qwTaskId );
                if( ERROR( ret ) )
                    continue;
            }
            vecTasks.push_back(
                { elem , qwTaskId } );
        }

        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    if( ERROR( ret ) )
        vecTasks.clear();

    return ret;
}

gint32 CRpcReqForwarder::FindFwrdReqsByUniqName(
    const stdstr& strName,
    FWRDREQS& vecTasks )
{
    gint32 ret = 0;
    if( strName.empty() )
        return -EINVAL;

    return FindFwrdReqsAll( strName, vecTasks );
}

gint32 CRpcReqForwarder::ClearFwrdReqsByDestAddr(
    guint32 dwPortId,
    const stdstr& strPath,
    DMsgPtr& pMsg )
{
    gint32 ret = 0;
    do{
        stdstr strDest;
        ret = pMsg.GetStrArgAt( 0, strDest );
        if( ERROR( ret ) )
            break;

        stdstr strNewOwner;
        ret = pMsg.GetStrArgAt( 2, strNewOwner );
        if( ERROR( ret ) )
            break;
        if( strNewOwner.size() > 0 )
        {
            // an online message
            ret = -EINVAL;
            break;
        }

        std::set< stdstr > setNames;
        ret = FindUniqNamesByPortId(
            dwPortId, setNames );
        if( ret <= 0 )
        {
            ret = -ENOENT;
            break;
        }

        ObjVecPtr pvecTasks( true );
        for( auto elem : setNames )
        {
            FWRDREQS vecReqs;
            ret = FindFwrdReqsByUniqName(
                elem, vecReqs );

            for( auto elemReq : vecReqs )
            {
                CIfInvokeMethodTask* pInv =
                    elemReq.first;
                DMsgPtr pMsg;
                CCfgOpenerObj oInv( pInv );
                ret = oInv.GetMsgPtr(
                    propMsgPtr, pMsg );
                if( ERROR( ret ) )
                    continue;

                stdstr strVal;
                ret = RetrieveDest(
                    pInv, strVal );
                if( ERROR( ret ) )
                    continue;
                    
                if( strVal != strDest )
                    continue;

                ObjPtr pObj;
                ret = pMsg.GetObjArgAt( 0, pObj );
                if( ERROR( ret ) )
                    continue;

                CCfgOpener oReqCtx(
                    ( IConfigDb* )pObj );
                ret = oReqCtx.GetStrProp(
                    propRouterPath, strVal );
                if( ERROR( ret ) )
                    continue;

                if( strVal != strPath )
                    continue;

                ( *pvecTasks )().push_back(
                    ObjPtr( pInv ) );
            }

            ret = 0;
            CancelInvTasks( pvecTasks );
        }

    }while( 0 );

    return 0;
}

gint32 CRpcReqForwarder::FindFwrdReqsByPrxyPortId(
    guint32 dwPrxyPortId,
    std::vector< stdstr >& strNames,
    FWRDREQS& vecTasks )
{
    gint32 ret = 0;
    do{
        FWRDREQS vecTasksAll;
        for( auto strUniqName : strNames )
        {
            FWRDREQS vecTasksUname;
            ret = FindFwrdReqsAll(
                dwPrxyPortId, strUniqName,
                vecTasksUname, false );
            if( ERROR( ret ) )
                break;
        
            CRpcRouterBridge* pRouter =
                static_cast< CRpcRouterBridge* >
                    ( GetParent() );

            for( auto elem : vecTasksUname )
            {
                CCfgOpenerObj oInv(
                    ( CObjBase* )elem.first );

                DMsgPtr pMsg;
                ret = oInv.GetMsgPtr(
                    propMsgPtr, pMsg );
                if( ERROR( ret ) )
                    continue;

                ObjPtr pObj;
                ret = pMsg.GetObjArgAt( 0, pObj );
                if( ERROR( ret ) )
                    continue;

                CCfgOpener oReqCtx(
                    ( IConfigDb* )pObj );

                guint32 dwPortId = 0;
                ret = oReqCtx.GetIntProp(
                    propConnHandle, dwPortId );
                if( ERROR( ret ) )
                    continue;

                if( dwPortId != dwPrxyPortId )
                    continue;

                vecTasks.push_back( elem );
            }

        }
        ret = 0;
        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    if( ERROR( ret ) )
        vecTasks.clear();
    return ret;
}

gint32 CRpcReqForwarder::OnModEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    if( strModule.empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( iEvent == eventModOnline )
            break;

        // not uniq name, not our call
        if( strModule[ 0 ] != ':' )
            break;

        //
        // ret = GetRefCountByUniqName( strModule );
        // if( ret <= 0 )
        //     break;

        ObjPtr pObj;
        TaskletPtr pDeferTask;
        ret = DEFER_IFCALLEX_NOSCHED(
            pDeferTask, ObjPtr( this ),
            &CRpcReqForwarder::OnModOfflineInternal,
            ( IEventSink* )pObj, iEvent, strModule );

        if( ERROR( ret ) )
            break;

        BufPtr pBuf( true );
        *pBuf = ObjPtr( pDeferTask );
        CIfDeferCallTaskEx* pTask = pDeferTask;
        pTask->UpdateParamAt( 0, pBuf );

        // using the reqfwdr's sequential
        // taskgroup
        ret = GetParent()->AddSeqTask(
            pDeferTask, false );

        if( ERROR( ret ) )
            ( *pDeferTask )( eventCancelTask );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::GetInvTaskPrxyPortId(
    TaskletPtr& pTask,
    guint32& dwPortId )
{
    if( pTask.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        DMsgPtr pMsg;
        CIfInvokeMethodTask* pInv = pTask;
        CCfgOpenerObj oInvCfg( pInv );
        ret = oInvCfg.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 0, pObj );
        if( ERROR( ret ) )
            break;

        CCfgOpener oReqCtx(
            ( IConfigDb* )pObj );
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::OnModOfflineInternal(
    IEventSink* pCallback,
    EnumEventId iEvent,
    std::string strUniqName )
{
    // remove the registered objects
    set< guint32 > setPortIds;
    gint32 ret = 0;

    do{
        DebugPrint( 0, "%s is down",
            strUniqName.c_str() );

        ret = ClearRefCountByUniqName(
            strUniqName, setPortIds );
        if( ret == 0 )
            break;

        FWRDREQS vecReqs;
        FindFwrdReqsByUniqName(
            strUniqName, vecReqs );

        std::vector< MatchPtr > vecMatches;

        // find all the local matches to remove
        CRpcRouterReqFwdr* pRouter =
            static_cast< CRpcRouterReqFwdr* >
                ( GetParent() );
        ret = pRouter->RemoveLocalMatchByUniqName(
            strUniqName, vecMatches );

        if( vecReqs.empty() &&
            vecMatches.empty() &&
            setPortIds.empty() )
        {
            if( IsRfcEnabled() )
                RemoveGrpRfcs( strUniqName );
            break;
        }

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        TaskGrpPtr pTaskGrp;
        CCfgOpener oTaskCfg;
        oTaskCfg.SetPointer( propIfPtr, this ); 

        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oTaskCfg.GetCfg() );

        if( ERROR( ret ) )
            return ret;

        pTaskGrp->SetRelation( logicNONE );
        pTaskGrp->SetClientNotify( pCallback );


        QwVecPtr pvecTaskIds( true );
        ObjVecPtr pvecTasks( true );

        // find all the ongoing tasks
        std::vector< ObjPtr >& vecTasks =
            ( *pvecTasks )();

        FWRDREQS vecRmtTasks;

        for( auto elem : vecReqs )
        {
            vecTasks.push_back(
                ObjPtr( elem.first ) );

            CIfParallelTask* pTask =
                ObjPtr( elem.first );

            CIfParallelTask* pEndTask =
                pTask->GetEndFwrdTask();

            EnumTaskState iState = 
                pEndTask->GetTaskState();

            if( iState == stateStarted )
                vecRmtTasks.push_back( elem );
        }

        std::map< guint32, gint32 > oPortRefs;
        if( setPortIds.size() )
        {
            CStdRMutex oRouterLock(
                pRouter->GetLock() );

            for( auto elem : setPortIds )
            {
                ret = GetRefCountByPortId( elem );            
                oPortRefs[ elem ] = ret;
            }
        }

        // disable the events on the bridge side
        for( auto& dwPortId : setPortIds )
        {
            // if the port is about to close, no
            // need to send this commands
            if( oPortRefs[ dwPortId ] == 0 ||
                IsSepConns() )
                continue;

            ObjVecPtr pMatches( true );
            for( auto pMatch : vecMatches )
            {
                CCfgOpenerObj oMatch(
                    ( CObjBase* )pMatch );

                ret = oMatch.IsEqual(
                    propPrxyPortId, dwPortId );

                if( ERROR( ret ) )
                    continue;

                ( *pMatches )().push_back(
                    ObjPtr( pMatch ) );
            }

            if( ( *pMatches )().size() )
            {
                InterfPtr pProxy;
                ret = pRouter->GetBridgeProxy(
                    dwPortId, pProxy );

                if( ERROR( ret ) )
                    continue;

                CRpcTcpBridgeProxy* pBdgePrxy =
                    pProxy;

                if( pBdgePrxy == nullptr )
                    continue;

                ObjPtr pObj = pMatches;

                QwVecPtr pTaskIdsToCancel( true );
                FWRDREQS_ITER itr = vecRmtTasks.begin();
                while( itr != vecRmtTasks.end() )
                {
                    guint32 dwPrxyId = 0;
                    ret = GetInvTaskPrxyPortId(
                        itr->first, dwPrxyId );
                    if( ERROR( ret ) ||
                        dwPrxyId != dwPortId )
                    {
                        ++itr;
                        continue;
                    }

                    ( *pTaskIdsToCancel )().
                        push_back( itr->second );

                    itr = vecRmtTasks.erase( itr );
                }

                ObjPtr pEmptyCb;
                ObjPtr pObjTasks = pTaskIdsToCancel;
                TaskletPtr pDeferTask;
                ret = DEFER_IFCALLEX_NOSCHED2(
                    2, pDeferTask, ObjPtr( pBdgePrxy ),
                    &CRpcTcpBridgeProxy::ClearRemoteEvents,
                    pObj, pObjTasks, pEmptyCb );

                if( ERROR( ret ) )
                    break;

                pTaskGrp->AppendTask( pDeferTask );
            }
        }

        // to close the bridge proxy with zero refcount
        for( auto& dwPortId : setPortIds )
        {
            if( oPortRefs[ dwPortId ] > 0 &&
                !IsSepConns() )
                continue;

            InterfPtr pProxy;
            ret = pRouter->GetBridgeProxy(
                dwPortId, pProxy );

            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }

            CParamList oParams;
            oParams.SetPointer( propIfPtr, this );
            oParams.Push( ObjPtr( pProxy ) );
            oParams[ propPortId ] = dwPortId;

            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CReqFwdrCloseRmtPortTask ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }

            pTaskGrp->AppendTask( pTask );
        }

        if( vecTasks.size() > 0 )
            CancelInvTasks( pvecTasks );

        if( IsRfcEnabled() )
            RemoveGrpRfcs( strUniqName );

        if( pTaskGrp->GetTaskCount() == 0 )
            break;

        // run the task
        TaskletPtr pGrpTask( pTaskGrp );
        ret = GetIoMgr()->
            RescheduleTask( pGrpTask );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

// for CRpcTcpBridgeProxyImpl offline
gint32 CRpcReqForwarder::ClearRefCountByPortId(
    guint32 dwPortId,
    vector< string >& vecUniqNames )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->ClearRefCountByPortId(
        dwPortId, vecUniqNames );
}

gint32 CRpcReqForwarder::CheckMatch(
    IMessageMatch* pMatch )
{
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    return pRouter->CheckMatch( pMatch );
}

gint32 CRpcReqForwarder::DisableRemoteEvent(
    IEventSink* pCallback,
    IMessageMatch* pMatch )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pMatch == nullptr )
        return -EINVAL;

    TaskletPtr pDeferTask;
    do{
        bool bEnable = false;
        ret = DEFER_HANDLER_NOSCHED(
            pDeferTask, ObjPtr( this ),
            &CRpcReqForwarder::EnableDisableEvent,
            pCallback, pMatch, bEnable );

        if( ERROR( ret ) )
            break;

        // using the bridge proxy's sequential
        // taskgroup
        CCfgOpenerObj oMatch( pMatch );
        guint32 dwPortId = 0;
        ret = oMatch.GetIntProp(
            propPrxyPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        InterfPtr bridgePtr;
        ret = GetParent()->GetBridgeProxy(
            dwPortId, bridgePtr );

        if( ERROR( ret ) )
            break;

        CRpcServices* pProxy = bridgePtr;
        ret = pProxy->AddSeqTask(
            pDeferTask, false );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    if( ERROR( ret ) && !pDeferTask.IsEmpty() )
        ( *pDeferTask )( eventCancelTask );

    return ret;
}

gint32 CRpcReqForwarder::EnableRemoteEvent(
    IEventSink* pCallback,
    IMessageMatch* pMatch )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pMatch == nullptr )
        return -EINVAL;

    TaskletPtr pDeferTask;
    do{
        bool bEnable = true;
        ret = DEFER_HANDLER_NOSCHED(
            pDeferTask, ObjPtr( this ),
            &CRpcReqForwarder::EnableDisableEvent,
            pCallback, pMatch, bEnable );

        if( ERROR( ret ) )
            break;

        // using the bridge proxy's sequential
        // taskgroup, in case if the connection is in
        // problem, it will not affect traffics over
        // other connections.
        CCfgOpenerObj oMatch( pMatch );
        guint32 dwPortId = 0;
        ret = oMatch.GetIntProp(
            propPrxyPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        InterfPtr bridgePtr;
        ret = GetParent()->GetBridgeProxy(
            dwPortId, bridgePtr );

        if( ERROR( ret ) )
            break;

        CRpcServices* pProxy = bridgePtr;
        ret = pProxy->AddSeqTask(
            pDeferTask, false );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    if( ERROR( ret ) && !pDeferTask.IsEmpty() )
        ( *pDeferTask )( eventCancelTask );

    return ret;
}

gint32 CRpcReqForwarder::EnableDisableEvent(
    IEventSink* pCallback,
    IMessageMatch* pMatch,
    bool& bEnable )
{
    if( pMatch == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
#ifdef LOOP_TEST
    CParamList oParams;
    oParams[ propReturnValue ] = ret;
    SetResponse( pCallback, oParams.GetCfg() );
    return ret;
#endif
    bool bUndo = false;
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );
    do{
        CCfgOpenerObj oCfgInvoke( pCallback );
        DMsgPtr pMsg;

        ret = oCfgInvoke.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        string strSrcUniqName = pMsg.GetSender();
        if( strSrcUniqName.empty() )
        {
            ret = -EINVAL;
            break;
        }
        
        CCfgOpenerObj oCfg( pMatch );

        oCfg.SetStrProp(
            propSrcUniqName, strSrcUniqName );

        // check if the match comes from a
        // registered module
        ret = CheckMatch( pMatch );
        if( ERROR( ret ) )
            break;

        // add the match
        if( bEnable )
        {
            ret = pRouter->AddLocalMatch( pMatch );
        }
        else
        {
            ret = pRouter->RemoveLocalMatch( pMatch );
        }
        if( ret == EEXIST )
        {
            ret = 0;
            if( !bEnable )
                break;

            bool bOnline = false;

            ret = pRouter->IsMatchOnline(
                pMatch, bOnline );

            if( ERROR( ret ) )
                break;

            if( !bOnline )
                ret = ENOTCONN;

            break;
        }
        else if( ERROR( ret ) )
        {
            break;
        }

        bUndo = true;
        CParamList oParams;
        oParams.SetPointer( propIoMgr, GetIoMgr() );

        oParams.SetObjPtr(
            propEventSink, ObjPtr( pCallback ) );

        oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        oParams.CopyProp( propPortId,
            propPrxyPortId,
            ( CObjBase* )pMatch );

        oParams.Push( ObjPtr( pMatch ) );
        oParams.Push( bEnable );

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CReqFwdrEnableRmtEventTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ( *pTask )( eventZero );
        ret = pTask->GetError();

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        if( bUndo && ret == -EAGAIN )
        {
            // only undo if the operation is
            // recoverable
            if( bEnable )
            {
                pRouter->RemoveLocalMatch( pMatch );
            }
            else
            {
                pRouter->AddLocalMatch( pMatch );
            }
        }
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        SetResponse( pCallback, oResp.GetCfg() );
    }

    return ret;
}

gint32 CReqFwdrEnableRmtEventTask::RunTask()
{
    gint32 ret = 0;
    do{

        CParamList oParams( GetConfig() );
        CRpcReqForwarder* pIf;
        ObjPtr pObj;
        ret = oParams.GetObjPtr( propIfPtr, pObj );
        if( ERROR( ret ) )
            break;

        pIf = pObj;
        if( pIf == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        CRpcRouterReqFwdr* pRouter =
            static_cast< CRpcRouterReqFwdr* >
                ( pIf->GetParent() );
        if( pRouter == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        guint32 dwPortId = 0;
        ret = oParams.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        InterfPtr bridgePtr;
        ret = pRouter->GetBridgeProxy(
            dwPortId, bridgePtr );

        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxy* pProxy = bridgePtr;
        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        bool bEnable = false;

        ret = oParams.GetBoolProp( 1, bEnable );
        if( ERROR( ret ) )
            break;

        IMessageMatch* pMatch = nullptr;
        ret = oParams.GetPointer( 0, pMatch );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oMatch( pMatch );
        // for local match only
        ret = oMatch.CopyProp(
            propDestTcpPort, pProxy );
        if( ERROR( ret ) )
            break;

        if( bEnable )
        {
            ret = pProxy->EnableRemoteEvent(
                this, pMatch );
        }
        else
        {
            ret = pProxy->DisableRemoteEvent(
                this, pMatch );
        }

    }while( 0 );

    if( ret != STATUS_PENDING && !Retriable( ret ) )
    {
        OnTaskComplete( ret );
    }

    return ret;
}

gint32 CReqFwdrEnableRmtEventTask::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = 0;

    do{
        EventPtr pEvent;
        ret = GetInterceptTask( pEvent );
        if( ERROR( ret ) )
            break;

        IMessageMatch* pMatch = nullptr;
        bool bEnable = true;

        ObjPtr pObj;
        CCfgOpener oCfg( ( IConfigDb* )GetConfig() );
        ret = oCfg.GetBoolProp( 1, bEnable );
        if( ERROR( ret ) )
        {
            // fatal error, cannot recover
            break;
        }
        ret = oCfg.GetObjPtr( 0, pObj );
        if( ERROR( ret ) )
            break;

        pMatch = pObj;
        if( pMatch == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = oCfg.GetObjPtr( propIfPtr, pObj );
        if( ERROR( ret ) )
            break;

        CRpcReqForwarder* pReqFwdr = pObj;
        if( pReqFwdr == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CRpcRouterReqFwdr* pRouter =
            static_cast< CRpcRouterReqFwdr* >
                ( pReqFwdr->GetParent() );

        if( SUCCEEDED( iRetVal ) )
        {
            // check to see the response from the
            // remote server
            do{
                vector< LONGWORD > vecParams;
                ret = GetParamList( vecParams );
                if( ERROR( ret ) )
                    break;
                CObjBase* pObj = reinterpret_cast
                    < CObjBase* >( vecParams[ 3 ] );

                if( pObj == nullptr )
                {
                    ret = -EFAULT;
                    break;
                }

                CCfgOpenerObj oTaskCfg( pObj );
                CConfigDb* pResp = nullptr;
                ret = oTaskCfg.GetPointer(
                    propRespPtr, pResp );

                if( ERROR( ret ) )
                    break;

                CCfgOpener oResp( pResp );
                ret = oResp.GetIntProp(
                    propReturnValue,
                    ( guint32& )iRetVal );

                if( ERROR( ret ) )
                    break;

                ret = iRetVal;
                bool bOnline = true;
                if( ret == ENOTCONN )
                    bOnline = false;

                pRouter->SetMatchOnline(
                    pMatch, bOnline );

                break;

            }while( 0 );
        }
        else
        {
            ret = iRetVal;
        }

        if( ret == -EAGAIN )
        {
            // undo Add/RemoveLocalMatch only if
            // the operation is recoverable.
            if( bEnable )
            {
                pRouter->RemoveLocalMatch( pMatch );
            }
            else
            {
                pRouter->AddLocalMatch( pMatch );
            }
        }

        if( IsPending() )
        {
            CParamList oParams;
            oParams.SetIntProp(
                propReturnValue, ret );
            
            // the response will finally be sent in
            // this method
            ret = pReqFwdr->SetResponse(
                pEvent, oParams.GetCfg() ); 
        }

    }while( 0 );

    if( ret != STATUS_PENDING && !Retriable( ret ) )
    {
        // clear the objects
        CParamList oParams( GetConfig() );
        oParams.ClearParams();
    }

    return ret;
}

gint32 CRegisteredObject::IsMyMatch(
    IMessageMatch* pMatch )
{
    gint32 ret = 0;
    if( pMatch == nullptr )
        return -EINVAL;

    do{
        CCfgOpener oCfg( this );
        const IConfigDb* pCfg =
            pMatch->GetCfg();

        ret = oCfg.IsEqualProp(
            propPrxyPortId, pCfg );
        if( ERROR( ret ) )
            break;

        ret = oCfg.IsEqualProp(
            propSrcUniqName, pCfg );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

bool CRegisteredObject::operator<(
    CRegisteredObject& rhs ) const
{
    gint32 ret = 0;
    do{
        CCfgOpener oCfg( this );
        CCfgOpener oCfg2( &rhs );

        guint32 dwVal1, dwVal2;

        ret = oCfg.GetIntProp(
            propPrxyPortId, dwVal1 );
        if( ERROR( ret ) )
            break;

        ret = oCfg2.GetIntProp(
            propPrxyPortId, dwVal2 );
        if( ERROR( ret ) )
            break;

        if( dwVal1 < dwVal2 )
            break;

        if( dwVal2 < dwVal1 )
        {
            ret = ERROR_FALSE;
            break;
        }

        std::string strVal, strVal2;

        ret = oCfg.GetStrProp(
            propSrcUniqName, strVal );
        if( ERROR( ret ) )
            break;

        ret = oCfg2.GetStrProp(
            propSrcUniqName, strVal2 );
        if( ERROR( ret ) )
            break;

        if( strVal < strVal2 )
            break;

        if( strVal2 < strVal )
        {
            ret = ERROR_FALSE;
            break;
        }

        ret = ERROR_FALSE;

    }while( 0 );

    if( SUCCEEDED( ret ) )
        return true;

    return false;
}

gint32 CReqFwdrForwardRequestTask::RunTask()
{
    gint32 ret = 0;

    ObjPtr pObj;
    CParamList oParams( GetConfig() );
    DMsgPtr pRespMsg;

    CRpcRouterReqFwdr* pRouter = nullptr;
    do{
        CRpcServices* pIf;
        ret = oParams.GetObjPtr( propIfPtr, pObj );
        if( ERROR( ret ) )
            break;

        pIf = pObj;
        if( pIf == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = oParams.GetPointer(
            propRouterPtr, pRouter );
        if( ERROR( ret ) )
            break;

        IConfigDb* pReqCtx = nullptr;
        ret = oParams.GetPointer( 0, pReqCtx );
        if( ERROR( ret ) )
            break;

        DMsgPtr pMsg;
        ret = oParams.GetMsgPtr( 1, pMsg );
        if( ERROR( ret ) )
            break;

        CRpcReqForwarder* pFwdr = ObjPtr( pIf );
        if( pFwdr == nullptr ) 
        {
            ret = -EINVAL;
            break;
        }

        InterfPtr bridgePtr;
        CCfgOpener oReqCtx( pReqCtx );
        guint32 dwPortId = 0;
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        pRouter->GetBridgeProxy(
            dwPortId, bridgePtr );

        CRpcTcpBridgeProxy* pProxy = bridgePtr;
        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        oParams.SetIntProp(
            propPrxyPortId, dwPortId );

        ret = pProxy->ForwardRequest(
            pReqCtx, pMsg, pRespMsg, this );

    }while( 0 );

    if( ret == STATUS_PENDING )
        return ret;

    // let QueueFullCallback to handle this
    // without sending the response
    if( ret == ERROR_QUEUE_FULL &&
        pRouter->IsRfcEnabled() )
        return ret;

    if( Retriable( ret ) )
        return STATUS_MORE_PROCESS_NEEDED;

    oParams.GetObjPtr(
        propRespPtr, pObj );

    CParamList oRespCfg(
        ( IConfigDb* )pObj );

    oRespCfg.SetIntProp(
        propReturnValue, ret );

    if( !pRespMsg.IsEmpty() )
        oRespCfg.Push( pRespMsg );

    ret = OnTaskComplete( ret );

    return ret;
}

gint32 CReqFwdrForwardRequestTask::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = 0;

    TaskletPtr pIoTask;
    GetCallerTask( pIoTask );

    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        if( iRetVal == ERROR_QUEUE_FULL )
        {
            // error from remote server
            ret = OnTaskCompleteRfc(
                iRetVal, pIoTask );
            if( ret == STATUS_PENDING )
                break;
        }

        ObjPtr pObj;

        IConfigDb* pReqCtx = nullptr;
        ret = oCfg.GetPointer( 0, pReqCtx );
        if( ERROR( ret ) )
            break;

        // test if the request has reponse to send
        bool bNoReply = false;
        CCfgOpener oReqCtx( pReqCtx );
        ret = oReqCtx.GetBoolProp(
            propNoReply, bNoReply );
        if( SUCCEEDED( ret ) && bNoReply )
            break;

        ret = oCfg.GetObjPtr( propIfPtr, pObj );
        if( ERROR( ret ) )
            break;

        CRpcInterfaceServer* pIfSvr = pObj;
        if( pIfSvr == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        if( !pIoTask.IsEmpty() )
        {
            CCfgOpener oRespCfg( ( IConfigDb* )
                pIoTask->GetConfig() );

            ret = oRespCfg.GetObjPtr(
                propRespPtr, pObj );
            if( ERROR( ret ) )
                break;
        }
        else
        {
            ret = oCfg.GetObjPtr(
                propRespPtr, pObj );
            if( ERROR( ret ) )
                break;
        }

        EventPtr pEvent;
        ret = GetInterceptTask( pEvent );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp(
            ( IConfigDb* )pObj );

        oResp[ propReturnValue ] = iRetVal;
        if( SUCCEEDED( iRetVal ) && !oResp.exist( 0 ) )
        {
            DebugPrint( iRetVal,
                "No response message" );
            oResp[ propReturnValue ] = -EFAULT;
            ret = -EFAULT;
        }

        if( IsPending() )
        {
            // the response will finally be sent
            // in this method
            pIfSvr->OnServiceComplete(
                pObj, pEvent ); 
        }
        else
        {
            pIfSvr->SetResponse(
                pEvent, oResp.GetCfg() );
        }

    }while( 0 );

    if( ret != STATUS_PENDING && !Retriable( ret ) )
    {
        // clear the objects
        CParamList oParams( GetConfig() );
        oParams.ClearParams();
    }

    return iRetVal;
}

gint32 CReqFwdrForwardRequestTask::CloneIoTask(
    TaskletPtr& pIoTask )
{
    gint32 ret = 0;
    vector< LONGWORD > vecParams;
    do{
        ret = GetParamList( vecParams );
        if( ERROR( ret ) )
            break;

        CObjBase* pObj = reinterpret_cast
            < CObjBase* >( vecParams[ 3 ] );

        if( unlikely( pObj == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        CIfIoReqTask* pIoReq = ObjPtr( pObj );
        if( unlikely( pIoReq == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        // we are within this task's lock, free to
        // use its member.
        IConfigDb* pIoCfg = pIoReq->GetConfig();
        CParamList oNewReq;
        oNewReq.CopyProp( propIfPtr, pIoCfg );
        oNewReq.CopyProp( propReqPtr, pIoCfg );
        oNewReq.CopyProp( propRespPtr, pIoCfg );

        ret = pIoTask.NewObj(
            clsid( CIfIoReqTask ),
            oNewReq.GetCfg() );

        if( ERROR( ret ) )
            break;

        CIfIoReqTask* pTask = pIoTask;
        pTask->SetClientNotify( this );

    }while( 0 );

    return ret;
}

gint32 CReqFwdrForwardRequestTask::OnTaskCompleteRfc(
    gint32 iRetVal,
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    if( pTask.IsEmpty() )
        return -EINVAL;

    do{
        CCfgOpener oParams(
            ( IConfigDb* )GetConfig() );

        CRpcRouter* pRouter = nullptr;
        oParams.GetPointer(
            propRouterPtr, pRouter );

        IConfigDb* pReqCtx = nullptr;
        guint32 dwPortId = 0;
        oParams.GetIntProp(
            propPrxyPortId, dwPortId );

        InterfPtr pIf;
        ret = pRouter->GetBridgeProxy(
            dwPortId, pIf );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxy* pProxy = pIf;
        if( !pProxy->IsRfcEnabled() )
        {
            ret = -ENOTSUP;
            break;
        }

        ret = CloneIoTask( pTask );
        if( ERROR( ret ) )
            break;

        // insert this task back to the pending
        // queue
        pTask->MarkPending();
        ret = pProxy->RequeueTask( pTask ); 
        if( ERROR( ret ) )
        {
            ( *pTask )( eventCancelTask );
            break;
        }

        ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::BuildBufForIrpRmtSvrEvent(
    BufPtr& pBuf, IConfigDb* pReqCall )
{
    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    DMsgPtr pOfflineMsg;

    do{
        CReqOpener oReq( pReqCall );
        bool bOnline = false;

        IConfigDb* pEvtCtx = nullptr;
        ret = oReq.GetPointer( 0, pEvtCtx );
        if( ERROR( ret ) )
            break;

        ret = oReq.GetBoolProp( 1, bOnline );
        if( ERROR( ret ) )
            break;

        ret = pOfflineMsg.NewObj( ( EnumClsid )
            DBUS_MESSAGE_TYPE_SIGNAL );

        if( ERROR( ret ) )
            break;

        pOfflineMsg.SetInterface(
            DBUS_IF_NAME( IFNAME_REQFORWARDER ) );

        string strRtName;
        CCfgOpenerObj oIfCfg( this );
        ret = oIfCfg.GetStrProp(
            propSvrInstName, strRtName );
        if( ERROR( ret ) )
            break;

        CRpcRouter* pRouter = GetParent();
        if( pRouter->HasAuth() )
        {
            pOfflineMsg.SetPath(
                DBUS_OBJ_PATH( strRtName,
                OBJNAME_REQFWDR_AUTH) );
        }
        else
        {
            pOfflineMsg.SetPath(
                DBUS_OBJ_PATH( strRtName,
                OBJNAME_REQFWDR ) );
        }

        pOfflineMsg.SetMember(
            SYS_EVENT_RMTSVREVENT );

        string strVal;
        ret = oReq.GetStrProp(
            propDestDBusName, strVal );
        if( ERROR( ret ) )
            break;

        pOfflineMsg.SetDestination( strVal );

        BufPtr pCtxBuf( true );
        ret = pEvtCtx->Serialize( *pCtxBuf );
        if( ERROR( ret ) )
            break;

        const char* pCtx = pCtxBuf->ptr();
        guint32 dwOnline = bOnline;
        if( !dbus_message_append_args( pOfflineMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_BOOLEAN, &dwOnline,
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

        *pBuf = pOfflineMsg;

    }while( 0 );

    if( ERROR( ret ) && !pOfflineMsg.IsEmpty() )
        pOfflineMsg.Clear();

    return ret;
}

gint32 CRpcReqForwarder::BuildBufForIrpFwrdEvt(
    BufPtr& pBuf, IConfigDb* pReqCall )
{
    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    DMsgPtr pFwrdMsg;

    do{
        CReqOpener oReq( pReqCall );
        IConfigDb* pReqCtx;

        ret = oReq.GetPointer( 0, pReqCtx );
        if( ERROR( ret ) )
            break;

        BufPtr pEvtBuf( true );
        ret = oReq.GetProperty( 1, *pEvtBuf );
        if( ERROR( ret ) )
            break;
        
        ret = pFwrdMsg.NewObj( ( EnumClsid )
            DBUS_MESSAGE_TYPE_SIGNAL );

        if( ERROR( ret ) )
            break;

        string strVal;

        ret = oReq.GetDestination( strVal );
        if( ERROR( ret ) )
            break;
        pFwrdMsg.SetDestination( strVal );

        ret = oReq.GetIfName( strVal );
        if( ERROR( ret ) )
            break;
        pFwrdMsg.SetInterface( strVal );

        ret = oReq.GetObjPath( strVal );
        if( ERROR( ret ) )
            break;
        pFwrdMsg.SetPath( strVal );

        ret = oReq.GetMethodName( strVal );
        if( ERROR( ret ) )
            break;
        pFwrdMsg.SetMember( strVal );

        BufPtr pCtxBuf( true );
        ret = pReqCtx->Serialize( *pCtxBuf );
        if( ERROR( ret ) )
            break;

        const char* pCtx = pCtxBuf->ptr();
        const char* pData = pEvtBuf->ptr();
        if( !dbus_message_append_args( pFwrdMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pEvtBuf->size(),
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

        *pBuf = pFwrdMsg;

    }while( 0 );

    if( ERROR( ret ) && !pFwrdMsg.IsEmpty() )
        pFwrdMsg.Clear();

    return ret;
}

gint32 CRpcReqForwarder::BuildBufForIrp(
    BufPtr& pBuf, IConfigDb* pReqCall )
{
    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        if( pBuf.IsEmpty() )
        {
            ret = pBuf.NewObj();
            if( ERROR( ret ) )
                break;
        }

        CReqOpener oReq( pReqCall );
        string strMethod;
        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        if( strMethod == SYS_EVENT_RMTSVREVENT )
        {
            ret = BuildBufForIrpRmtSvrEvent(
                pBuf, pReqCall );
        }
        else
        {
            ret = super::BuildBufForIrp(
                pBuf, pReqCall );
        }

    }while( 0 );

    return ret;
}
    
gint32 CRpcReqForwarder::SetupReqIrpFwrdEvt(
    IRP* pIrp,
    IConfigDb* pReqCall,
    IEventSink* pCallback )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        ret = super::super::SetupReqIrp(
            pIrp, pReqCall, pCallback );

        if( ERROR( ret ) )
            break;

        // make correction to the control code and
        // the io direction
        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
        pIrpCtx->SetCtrlCode( CTRLCODE_SEND_EVENT );

        // FIXME: we don't know if the request
        // needs a reply, and the flag is set
        // to CF_WITH_REPLY mandatorily
        pIrpCtx->SetIoDirection( IRP_DIR_OUT ); 

        // we don't need a timer
        if( pCallback != nullptr )
        {
            pIrp->SetCallback( pCallback, 0 );
            // already done in the base class's
            // SetupReqIrp
            // pIrp->SetIrpThread( GetIoMgr() );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::ForwardEvent(
    IConfigDb* pEvtCtx,
    DBusMessage* pEventMsg,
    IEventSink* pCallback )
{
    if( pEventMsg == nullptr ||
        pEvtCtx == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CRpcRouterReqFwdr* pRouter =
            static_cast< CRpcRouterReqFwdr* >
                ( GetParent() );

        vector< MatchPtr > vecMatches;
        DMsgPtr pEvtMsg( pEventMsg );

        ret = pRouter->CheckEvtToRelay(
            pEvtCtx, pEvtMsg, vecMatches );

        if( ERROR( ret ) )
            break;

        if( vecMatches.empty() )
            break;

        CCfgOpener oReqCtx( pEvtCtx );
        guint32 dwPortId = 0;
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;
        stdstr strPath;
        ret = oReqCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        if( IS_SVRMODOFFLINE_EVENT( pEvtMsg ) )
        {
            ClearFwrdReqsByDestAddr(
                dwPortId, strPath, pEvtMsg );
        }

        BufPtr pBuf( true );
        ret = pEvtMsg.Serialize( *pBuf );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oRtCfg( pRouter );

        string strRtName;
        ret = oRtCfg.GetStrProp(
            propSvrInstName, strRtName );
        if( ERROR( ret ) )
            break;

        for( auto&& pMatch : vecMatches )
        {
            // dispatch the event to all the
            // subscribers
            CReqBuilder oBuilder( this );

            oBuilder.Push( pEvtCtx );
            oBuilder.Push( *pBuf );

            ObjPtr pObj;
            pObj = pMatch;

            oBuilder.SetObjPtr(
                propMatchPtr, pObj );

            oBuilder.SetMethodName(
                SYS_EVENT_FORWARDEVT );

            oBuilder.SetCallFlags( 
               DBUS_MESSAGE_TYPE_SIGNAL 
               | CF_ASYNC_CALL );

            oBuilder.SetIfName( DBUS_IF_NAME(
                IFNAME_REQFORWARDER ) );

            if( pRouter->HasAuth() )
            {
                oBuilder.SetObjPath(
                    DBUS_OBJ_PATH( strRtName,
                    OBJNAME_REQFWDR_AUTH ) );
            }
            else
            {
                oBuilder.SetObjPath(
                    DBUS_OBJ_PATH( strRtName,
                    OBJNAME_REQFWDR) );
            }

            CCfgOpenerObj oMatch(
                ( CObjBase* )pObj );

            std::string strDest;
            ret = oMatch.GetStrProp(
                propSrcUniqName, strDest );

            if( ERROR( ret ) )
                break;

            // set the signal's destination 
            oBuilder.SetDestination( strDest );

            // a tcp default round trip time
            oBuilder.SetTimeoutSec(
                IFSTATE_DEFAULT_IOREQ_TIMEOUT ); 

            oBuilder.SetKeepAliveSec(
                IFSTATE_DEFAULT_IOREQ_TIMEOUT / 2 ); 

            CParamList oRespCfg;
            CfgPtr pRespCfg = oRespCfg.GetCfg();
            if( pRespCfg.IsEmpty() )
            {
                ret = -EFAULT;
                break;
            }

            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CIfDummyTask ) );

            if( ERROR( ret ) )
                break;

            RunIoTask( oBuilder.GetCfg(),
                pRespCfg, pTask );
        }

        ret = 0;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::BuildKeepAliveMessage(
    IConfigDb* pCfg, DMsgPtr& pkaMsg )
{
    if( pCfg == nullptr )
        return -EINVAL;

    CReqOpener oReq( pCfg );
    gint32 ret = 0;
    do{
        ret = pkaMsg.NewObj( ( EnumClsid )
            DBUS_MESSAGE_TYPE_SIGNAL );

        if( ERROR( ret ) )
            break;

        std::string strVal;
        // ifname
        ret = oReq.GetStrProp( 2, strVal );
        if( ERROR( ret ) )
            break;
        pkaMsg.SetInterface( strVal );

        ret = oReq.GetStrProp( 3, strVal );
        if( ERROR( ret ) )
            break;
        pkaMsg.SetPath( strVal );

        pkaMsg.SetMember( SYS_EVENT_KEEPALIVE );

        ret = oReq.GetSender( strVal );
        if( ERROR( ret ) )
            break;
        pkaMsg.SetSender( strVal );

        ret = oReq.GetDestination( strVal );
        if( ERROR( ret ) )
            break;
        pkaMsg.SetDestination( strVal );

        guint64 qwTaskId = 0;
        ret = oReq.GetQwordProp( 0, qwTaskId );
        if( ERROR( ret ) )
            break;

        guint32 dwKAFlag = 0;
        ret = oReq.GetIntProp( 1, dwKAFlag );
        if( ERROR( ret ) )
            break;

        CParamList oParams;
        oParams.Push( qwTaskId );
        oParams.Push( dwKAFlag );

        BufPtr pBuf( true );
        ret = oParams.GetCfg()->Serialize( *pBuf );
        if( ERROR( ret ) )
            break;

        pkaMsg.SetSerial( clsid( MinClsid ) );

        const char* pData = pBuf->ptr();
        if( !dbus_message_append_args( pkaMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pBuf->size(),
            DBUS_TYPE_INVALID ) )
            ret = -ENOMEM;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::OnKeepAliveRelay(
    IEventSink* pTask )
{
    if( pTask == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpenerObj oCfg( pTask );
        DMsgPtr pMsg;

        ret = oCfg.GetMsgPtr( propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        if( pMsg.GetType() !=
            DBUS_MESSAGE_TYPE_METHOD_CALL )
        {
            ret = -ENOTSUP;
            break;
        }
        // note that we only do keep-alive for
        // FORWARD_REQUEST
        if( pMsg.GetMember() !=
            SYS_METHOD_FORWARDREQ )
        {
            ret = -EINVAL;
            break;
        }

        
        std::string strIfName, strObjPath;
        std::string strFwrdIfName;
        std::string strSender = pMsg.GetSender();

        if( !IsConnected( strSender.c_str() ) )
        {
            ret = ERROR_STATE;
            break;
        }

        strFwrdIfName = pMsg.GetInterface();
        if( IsPaused( strFwrdIfName ) )
        {
            ret = ERROR_PAUSED;
            break;
        }

        CReqBuilder okaReq( this );
        okaReq.SetIfName( strFwrdIfName );

        strObjPath = pMsg.GetPath();
        okaReq.SetObjPath( strObjPath );

        ObjPtr pReqCtx;
        ret = pMsg.GetObjArgAt( 0, pReqCtx );
        if( ERROR( ret ) )
            break;

        if( true )
        {
            // retrieve the task id
            DMsgPtr pOrigMsg;
            ret = pMsg.GetMsgArgAt( 1, pOrigMsg );
            if( ERROR( ret ) )
                break;

            if( pOrigMsg.IsEmpty() )
            {
                ret = -EFAULT;
                break;
            }

            ObjPtr pObj;
            ret = pOrigMsg.GetObjArgAt( 0, pObj );
            if( ERROR( ret ) )
                break;

            IConfigDb* pOrigReq = pObj;
            if( pOrigReq == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            guint64 iTaskId = 0;
            CReqOpener oOrigReq( pOrigReq );
            ret = oOrigReq.GetTaskId( iTaskId );
            if( ERROR( ret ) )
                break;

            okaReq.Push( iTaskId );
            okaReq.Push( ( guint32 )KATerm );

            strIfName = pOrigMsg.GetInterface();
            if( strIfName.empty() )
            {
                ret = -ENOENT;
                break;
            }
            okaReq.Push( strIfName );

            strObjPath = pOrigMsg.GetPath();
            if( strObjPath.empty() )
            {
                ret = -ENOENT;
                break;
            }
            okaReq.Push( strObjPath );
        }

        okaReq.SetMethodName(
            SYS_EVENT_FORWARDEVT );

        okaReq.SetCallFlags( 
           DBUS_MESSAGE_TYPE_SIGNAL
           | CF_ASYNC_CALL );

        string strVal = pMsg.GetDestination( );
        if( strVal.empty() )
        {
            ret = -EINVAL;
            break;
        }
        okaReq.SetSender( strVal );

        strVal = pMsg.GetSender( );
        okaReq.SetDestination( strVal );

        DMsgPtr pkaMsg;
        ret = BuildKeepAliveMessage(
            okaReq.GetCfg(), pkaMsg );

        if( ERROR( ret ) )
            break;

        okaReq.ClearParams();

        CCfgOpener oKaCtx;
        ret = oKaCtx.CopyProp(
            propConnHandle, pReqCtx );

        if( ERROR( ret ) ) 
            break;

        ret = oKaCtx.CopyProp(
            propRouterPath, pReqCtx );

        if( ERROR( ret ) ) 
            break;

        IConfigDb* pKaCtx = oKaCtx.GetCfg();
        okaReq.Push( pKaCtx );

        BufPtr pMarshaledMsg( true );
        ret = pkaMsg.Serialize( pMarshaledMsg );
        if( ERROR( ret ) )
            break;

        okaReq.Push( pMarshaledMsg );

        TaskletPtr pDummyTask;

        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        ret = BroadcastEvent(
            okaReq.GetCfg(), pDummyTask );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::OnRmtSvrOffline(
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    gint32 ret = 0;
    if( pEvtCtx == nullptr )
        return -EINVAL;

    do{
        vector< string > vecUniqNames;
        std::string strPath;
        CCfgOpener oEvtCtx( pEvtCtx );

        ret = oEvtCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId;
        ret = oEvtCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        ret = ClearRefCountByPortId(
            dwPortId, vecUniqNames );

        // no client referring this addr
        if( ret == 0 )
            break;

        FWRDREQS vecTasksCancel;
        ret = FindFwrdReqsByPrxyPortId(
            dwPortId, vecUniqNames,
            vecTasksCancel );

        if( SUCCEEDED( ret ) )
        {
            ObjVecPtr pvecTasks( true );
            for( auto elem : vecTasksCancel )
            {
                ( *pvecTasks )().push_back(
                    elem.first );
            }
            if( ( *pvecTasks )().size() > 0 )
                CancelInvTasks( pvecTasks );
        }

        RemoveGrpRfcs( dwPortId );
        // send to the proxies a server offline
        // event
        for( auto& strDest : vecUniqNames )
        {
            CReqBuilder oParams( this );

            oParams.Push( pEvtCtx );
            oParams.Push( false );

            // correct the ifname 
            oParams.SetIfName( DBUS_IF_NAME(
                IFNAME_REQFORWARDER ) );

            oParams.SetMethodName(
                SYS_EVENT_RMTSVREVENT );

            oParams.SetDestination( strDest );

            oParams.SetCallFlags( 
               DBUS_MESSAGE_TYPE_SIGNAL
               | CF_ASYNC_CALL );

            TaskletPtr pDummyTask;
            ret = pDummyTask.NewObj(
                clsid( CIfDummyTask ) );

            if( ERROR( ret ) )
                break;

            ret = BroadcastEvent(
                oParams.GetCfg(), pDummyTask );
        }
    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::OnRmtSvrEvent(
    EnumEventId iEvent,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( iEvent != eventRmtSvrOffline )
        return super::OnRmtSvrEvent(
            iEvent, pEvtCtx, hPort );

    return OnRmtSvrOffline( pEvtCtx, hPort );
}

gint32 CRpcReqForwarder::SendFetch_Server(
    IConfigDb* pDataDesc,           // [in, out]
    gint32& fd,                     // [out]
    guint32& dwOffset,              // [in,out]
    guint32& dwSize,                // [in,out]
    IEventSink* pCallback )
{
    gint32 ret = 0;

    if( pDataDesc == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    do{
        CCfgOpener oDataDesc( pDataDesc );

        EnumClsid iid = clsid( Invalid );
        guint32* piid = ( guint32* )&iid;
        ret = oDataDesc.GetIntProp(
            propIid, *piid );

        if( ERROR( ret ) )
            break;

        if( iid != iid( IStream ) )
        {
            ret = -ENOTSUP;
            break;
        }

        string strMethod;
        ret = oDataDesc.GetStrProp(
            propMethodName, strMethod ); 
        if( ERROR( ret ) )
            break;

        if( strMethod != SYS_METHOD_FETCHDATA )
        {
            ret = -EINVAL;
            break;
        }

        CParamList oParams;
        oParams.SetObjPtr(
            propIoMgr, GetIoMgr() );

        // key property for intercept task
        oParams.SetObjPtr(
            propEventSink, ObjPtr( pCallback ) );

        oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        CParamList oResp;

        oParams.SetObjPtr(
            propRespPtr, oResp.GetCfg() );

        oParams.SetObjPtr( propRouterPtr,
            ObjPtr( GetParent() ) );

        oParams.Push( ObjPtr( pDataDesc ) );
        oParams.Push( ( guint32 )fd );
        oParams.Push( dwOffset );
        oParams.Push( dwSize );

        TaskletPtr pTask;

        ret = pTask.NewObj(
            clsid( CReqFwdrFetchDataTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = GetIoMgr()->RescheduleTask( pTask );
        if( ERROR( ret ) )
            break;

        ret = STATUS_PENDING;

    }while( 0 );

    // the return value indicates if the response
    // message is generated or not.
    return ret;
}

gint32 CRpcReqForwarder::RequeueInvTask(
    IEventSink* pCallback )
{
    gint32 ret = 0; 
    do{
        if( !IsRfcEnabled() )
        {
            ret = ERROR_STATE;
            break;
        }
        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        TaskletPtr pTask;
        pTask = static_cast< CIfInvokeMethodTask* >
            ( pCallback );
        CCfgOpener oTaskCfg(
            ( IConfigDb* )pTask->GetConfig() );

        DMsgPtr pMsg;
        oTaskCfg.GetMsgPtr( propMsgPtr, pMsg );
        TaskGrpPtr pGrp;
        ret = GetGrpRfc( pMsg, pGrp );
        if( ERROR( ret ) )
            break;

        pTask->MarkPending();
        CIfParallelTaskGrpRfc* pGrpRfc = pGrp;
        CStdRTMutex oLock( pGrpRfc->GetLock() );
        pGrp->InsertTask( pTask );
        if( pGrpRfc->GetRunningCount() <
            pGrpRfc->GetMaxRunning() &&
            !pGrpRfc->IsNoSched() )
        {
            TaskletPtr pTask = ObjPtr( pGrpRfc );
            GetIoMgr()->RescheduleTask( pTask );
        }

    }while( 0 );

    if( ERROR( ret ) )
    {
        TaskletPtr pTask = ObjPtr( pCallback );
        if( !pTask.IsEmpty() )
            ( *pTask )( eventCancelTask );
    }

    return ret;
}

CRpcReqForwarderProxy::CRpcReqForwarderProxy(
    const IConfigDb* pCfg )
    : CAggInterfaceProxy( pCfg ),
    super( pCfg )
{
    gint32 ret = 0;

    do{
        ObjPtr pObj;
        CCfgOpener oCfg( pCfg );

        ret = oCfg.GetObjPtr(
            propRouterPtr, pObj );

        if( ERROR( ret ) )
            break;

        m_pParent = pObj;
        if( m_pParent == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        RemoveProperty( propRouterPtr );

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error occurs in CRpcReqForwarderProxy ctor" );
        throw runtime_error( strMsg );
    }
}

CRpcReqForwarderProxy::~CRpcReqForwarderProxy()
{
}

gint32 CRpcReqForwarderProxy::SetupReqIrpFwrdReq(
    IRP* pIrp,
    IConfigDb* pReqCall,
    IEventSink* pCallback )
{

    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pReqCall == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    // DebugPrint( 0, "probe: reqfwdr prxy fwrdreq" );
    do{
        CReqOpener oReq( pReqCall );

        // the `msg' object
        DMsgPtr pReqMsg;
        ret = oReq.GetMsgPtr( 0, pReqMsg );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
        pIrpCtx->SetCtrlCode( CTRLCODE_SEND_REQ );

        guint32 dwIoDir = IRP_DIR_OUT;
        if( oReq.HasReply() )
            dwIoDir = IRP_DIR_INOUT;

        pIrpCtx->SetIoDirection( dwIoDir ); 

        BufPtr pBuf( true );
        *pBuf = pReqMsg;

        guint32 dwFlags = 0;
        bool bKeepAlive = false;
        ret = oReq.GetCallFlags( dwFlags );
        if( SUCCEEDED( ret ) && 
            ( dwFlags & CF_KEEP_ALIVE ) )
            bKeepAlive = true;

        if( bKeepAlive )
        {
            guint32 dwSerial = 0;
            pReqMsg.GetSerial( dwSerial );
            if( dwSerial != 0 )
            {
                // set the serial for keep-alive usage
                oReq.SetIntProp(
                    propSeqNo, dwSerial );
            }
        }

        pIrpCtx->SetReqData( pBuf );
        pIrp->SetCallback( pCallback, 0 );
        pIrp->SetIrpThread( GetIoMgr() );

        guint32 dwTimeoutSec = 0;
        oReq.GetTimeoutSec( dwTimeoutSec );
        guint64 qwTs = 0;
        oReq.GetQwordProp( propTimestamp, qwTs );
        guint64 qwAge =
            CTimestampSvr::GetAgeSec( qwTs );
        qwAge = abs( ( gint64 )qwAge );
        if( qwAge >= ( guint64 )dwTimeoutSec )
        {
            ret = -ETIMEDOUT;
            break;
        }
        dwTimeoutSec -= qwAge;

        pIrp->SetTimer( dwTimeoutSec, GetIoMgr() );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::SetupReqIrpEnableEvt(
    IRP* pIrp,
    IConfigDb* pReqCall,
    IEventSink* pCallback )
{

    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pReqCall == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        guint32 dwCallFlags = 0;
        CReqOpener oReq( pReqCall );
        ret = oReq.GetCallFlags( dwCallFlags );
        if( ERROR( ret ) )
            break;

        // the `match' object
        ObjPtr pObj;
        ret = oReq.GetObjPtr(
            propMatchPtr, pObj );
        if( ERROR( ret ) )
            break;

        bool bEnable;
        string strMethod;
        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        if( strMethod == IF_METHOD_ENABLEEVT )
            bEnable = true;
        else if( strMethod == IF_METHOD_DISABLEEVT )
            bEnable = false;
        else
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );

        if( bEnable ) 
        {
            pIrpCtx->SetCtrlCode(
                CTRLCODE_REG_MATCH );
        }
        else
        {
            pIrpCtx->SetCtrlCode(
                CTRLCODE_UNREG_MATCH );
        }

        guint32 dwIoDir = IRP_DIR_OUT;
        if( oReq.HasReply() )
            dwIoDir = IRP_DIR_INOUT;

        pIrpCtx->SetIoDirection( dwIoDir ); 

        BufPtr pBuf( true );
        *pBuf = pObj;

        pIrpCtx->SetReqData( pBuf );
        pIrp->SetCallback( pCallback, 0 );
        pIrp->SetIrpThread( GetIoMgr() );

        guint32 dwTimeoutSec = 0;
        ret = oReq.GetTimeoutSec( dwTimeoutSec );
        if( ERROR( ret ) )
        {
            dwTimeoutSec =
                IFSTATE_ENABLE_EVENT_TIMEOUT;
            ret = 0;
        }
        pIrp->SetTimer( dwTimeoutSec, GetIoMgr() );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::SetupReqIrp(
    IRP* pIrp,
    IConfigDb* pReqCall,
    IEventSink* pCallback )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pReqCall == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CReqOpener oReq( pReqCall );
        string strMethod;

        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        if( strMethod == IF_METHOD_ENABLEEVT ||
            strMethod == IF_METHOD_DISABLEEVT )
        {
            ret = SetupReqIrpEnableEvt(
                pIrp, pReqCall, pCallback );
            break;
        }
        else if( strMethod == SYS_METHOD_FORWARDREQ )
        {
            ret = SetupReqIrpFwrdReq(
                pIrp, pReqCall, pCallback );
            break;

        }
        else
        {
            ret = super::SetupReqIrp(
                pIrp, pReqCall, pCallback );
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::FillRespData(
    IRP* pIrp, CfgPtr& pResp )
{
    gint32 ret = 0;
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pResp.IsEmpty() )
        return -EINVAL;

    // retrieve the data from the irp
    IrpCtxPtr& pCtx = pIrp->GetTopStack();
    if( pCtx->GetMajorCmd() != IRP_MJ_FUNC ||
        pCtx->GetMinorCmd() != IRP_MN_IOCTL )
        return -ENOTSUP;

    CParamList oParams( (IConfigDb*) pResp );
    guint32 dwCtrlCode = pCtx->GetCtrlCode();


    switch( dwCtrlCode )
    {
    case CTRLCODE_REG_MATCH:
    case CTRLCODE_UNREG_MATCH:
        {
            break;
        }
    case CTRLCODE_SEND_REQ:
        {
            // FIXME: ugly method to know if the resp is for
            // ForwardRequest
            TaskletPtr pTask = pIrp->m_pCallback;
            CCfgOpenerObj oTaskCfg( ( CObjBase* )pTask );
            CTasklet* pParentTask = nullptr;
            ret = oTaskCfg.GetPointer(
                propEventSink, pParentTask );
            if( SUCCEEDED( ret ) )
            {
                gint32 iClsid =
                    pParentTask->GetClsid();

                if( clsid( CBridgeForwardRequestTask )
                    == iClsid )
                {
                    // DebugPrint( 0, "probe: BdgeFwrdReq FillRespData" );
                    DMsgPtr pMsg;
                    pMsg = ( DMsgPtr& )*pCtx->m_pRespData;
                    string strOrigSender;
                    // recover the original dest name
                    ret = oParams.GetStrProp(
                        propSrcDBusName, strOrigSender );
                    if( SUCCEEDED( ret ) )
                        pMsg.SetDestination( strOrigSender );
                    oParams.Push( pMsg );
                    break;
                }
            }
            // fall through
        }
    default:
        {
            ret = super::FillRespData(
                pIrp, pResp );
            break;
        }
    }
    return ret;
}

gint32 CRpcReqForwarderProxy::BuildNewMsgToFwrd(
    IConfigDb* pReqCtx,
    DMsgPtr& pFwrdMsg,
    DMsgPtr& pNewMsg )
{
    gint32 ret = 0;
    do{
        // append a session hash for access
        // control
        CCfgOpener oReqCtx;

        oReqCtx.CopyProp( propRouterPath,
            propPath2, pReqCtx );
        oReqCtx.CopyProp( propSessHash, pReqCtx );
        oReqCtx.CopyProp( propTimestamp, pReqCtx );

        pNewMsg.CopyHeader( pFwrdMsg );
        BufPtr pArg( true );
        gint32 iType = 0;
        ret = pFwrdMsg.GetArgAt( 0, pArg, iType );

        BufPtr pCtxBuf( true );
        ret = oReqCtx.Serialize( pCtxBuf );
        if( ERROR( ret ) )
            break;

        const char* pData = pArg->ptr();
        const char* pCtx = pCtxBuf->ptr();
        if( !dbus_message_append_args( pNewMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pArg->size(),
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::ForwardRequest(
    IConfigDb* pReqCtx,
    DBusMessage* pMsg,
    DMsgPtr& pRespMsg,
    IEventSink* pCallback )
{
    if( pMsg == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        DMsgPtr pReqMsg( pMsg );
        CReqBuilder oBuilder( this );

        string strOrigSender;
        string strSender;
        ret = oBuilder.GetStrProp(
            propSrcDBusName, strSender );

        if( ERROR( ret ) )
            break;

        strOrigSender = pReqMsg.GetSender();
        // replace the sender with the local
        // sender.
        //
        // NOTE that the sender could be changed
        // if the underlying port is a loopback
        // port. However the destination for this
        // inner message does not matter that much
        // because the envelop ForwardRequest
        // message will provide the correct
        // destination when this message arrives
        // the reqfwdr side. But at lease, the
        // proxy side has the true dest info.
        pReqMsg.SetSender( strSender );

        ObjPtr pObj;
        CCfgOpener oReqCtx( pReqCtx );
        ret = oReqCtx.GetObjPtr(
            propReqPtr, pObj );

        if( ERROR( ret ) )
            break;

        CReqOpener oReq( ( IConfigDb* )pObj );
        guint32 dwFlags = CF_WITH_REPLY |
            DBUS_MESSAGE_TYPE_METHOD_CALL |
            CF_ASYNC_CALL;
        oReq.GetCallFlags( dwFlags );
        oBuilder.SetCallFlags( dwFlags );
        oBuilder.CopyProp(
            propTimestamp, pReqCtx );

        bool bResp = true;
        if( !( dwFlags & CF_WITH_REPLY ) )
            bResp = false;

        DMsgPtr pNewMsg;
        ret = BuildNewMsgToFwrd( pReqCtx,
            pReqMsg, pNewMsg );
        if( ERROR( ret ) )
            break;

        oBuilder.Push( pNewMsg );
        oBuilder.SetMethodName(
            SYS_METHOD_FORWARDREQ );

        // a tcp default round trip time
        guint32 dwTimeoutSec = 0;
        oReq.GetTimeoutSec( dwTimeoutSec );
        oBuilder.SetTimeoutSec(
            dwTimeoutSec != 0 ? dwTimeoutSec :
            IFSTATE_DEFAULT_IOREQ_TIMEOUT ); 

        dwTimeoutSec = 0;
        oReq.GetKeepAliveSec( dwTimeoutSec );
        oBuilder.SetKeepAliveSec(
            dwTimeoutSec != 0 ? dwTimeoutSec :
            IFSTATE_DEFAULT_IOREQ_TIMEOUT / 2 ); 

        // copy the port id from where this
        // request comes, which is for canceling
        // purpose when the bridge is down
        oBuilder.CopyProp( propPortId,
            propConnHandle, pReqCtx );

        CParamList oRespCfg;
        CfgPtr pRespCfg = oRespCfg.GetCfg();

        // keep a copy of the orignal sender
        oRespCfg.SetStrProp(
            propSrcDBusName, strOrigSender );

        ret = RunIoTask( oBuilder.GetCfg(),
            pRespCfg, pCallback );

        if( ret == STATUS_PENDING )
            break;

        if( SUCCEEDED( ret ) && bResp )
        {
            CCfgOpener oCfg(
                ( IConfigDb* )pRespCfg );

            gint32 iRet = 0;
            ret = oCfg.GetIntProp(
                propReturnValue,
                ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            ret = oCfg.GetMsgPtr(
                0, pRespMsg );

            if( ERROR( ret ) )
                break;

            ret = iRet;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::DoInvoke(
    DBusMessage* pEvtMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;

    do{
        if( pEvtMsg == nullptr
            || pCallback == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        DMsgPtr pMsg( pEvtMsg );

        if( pMsg.GetType()
            != DBUS_MESSAGE_TYPE_SIGNAL )
        {
            ret = -EINVAL;
            break;
        }

        string strMethod = pMsg.GetMember();
        if( strMethod.empty() )
        {
            ret = -EBADMSG;
            break;
        }

        if( strMethod == SYS_EVENT_KEEPALIVE )
        {
            ret = OnKeepAlive( pCallback, KAOrigin ); 
            break;
        }
        else
        {
            // ignore ip address 
            CCfgOpener oEvtCtx;
            oEvtCtx[ propRouterPath ] = "/";
            ret = ForwardEvent( oEvtCtx.GetCfg(),
                pEvtMsg, pCallback );
        }

    }while( 0 );

    if( ret == -ENOTSUP )
    {
        ret = super::DoInvoke(
            pEvtMsg, pCallback );
    }

    return ret;
}

gint32 CRpcRfpForwardEventTask::RunTask()
{
    gint32 ret = 0;
    do{
        CParamList oParams( GetConfig() );
        CRpcRouterBridge* pRouter = nullptr;
        ret = oParams.GetPointer(
            propRouterPtr, pRouter );

        if( ERROR( ret ) )
            break;

        DMsgPtr pEvtMsg;
        ret = oParams.GetMsgPtr(
            propMsgPtr, pEvtMsg );

        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        ret = oParams.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        ret = pRouter->GetBridge( dwPortId, pIf );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridge* pBridge = pIf;
        if( pBridge == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IConfigDb* pEvtCtx = nullptr;
        ret = oParams.GetPointer( 0, pEvtCtx );
        if( ERROR( ret ) )
            break;

        ret = pBridge->ForwardEvent(
            pEvtCtx, pEvtMsg, this );

    }while( 0 );

    return ret;

}

gint32 CRpcReqForwarderProxy::ForwardEvent(
    IConfigDb* pEvtCtx,
    DBusMessage* pEventMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;
    if( pEventMsg == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    do{
        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( GetParent() );

        CParamList oParams;
        oParams.SetObjPtr(
            propRouterPtr, ObjPtr( pRouter ) );

        oParams.SetMsgPtr(
            propMsgPtr, DMsgPtr( pEventMsg ) );

        oParams.SetPointer(
            propIfPtr, this );

        CCfgOpenerObj oCfg( pCallback );
        CRouterRemoteMatch* pMatch = nullptr;
        gint32 ret = oCfg.GetPointer(
            propMatchPtr, pMatch );

        if( ERROR( ret ) )
            break;

        ret = oParams.CopyProp(
            propPortId, pMatch );
        if( ERROR( ret ) )
            break;

        oParams.Push( pEvtCtx );
        ret = GetIoMgr()->ScheduleTask(
            clsid( CRpcRfpForwardEventTask ),
            oParams.GetCfg() );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::CustomizeRequest(
    IConfigDb* pReqCfg,
    IEventSink* pCallback )
{
    if( pReqCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CParamList oParams( pReqCfg );
        string strMethod;

        ret = oParams.GetStrProp(
            propMethodName, strMethod );

        if( ERROR( ret ) )
            break;

        if( strMethod == SYS_METHOD_SENDDATA ||
            strMethod == SYS_METHOD_FETCHDATA )
        {
            ObjPtr pObj = oParams[ 0 ];
            IConfigDb* pDesc = pObj;
            if( pDesc == nullptr )
            {
                ret = EFAULT;
                break;
            }
            // set the destination information with the
            // ones in the data description
            oParams.CopyProp(
                propIfName, pDesc );

            oParams.CopyProp(
                propObjPath, pDesc );

            oParams.CopyProp(
                propDestDBusName, pDesc );
        }
        else
        {
            ret = super::CustomizeRequest(
                pReqCfg, pCallback );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::InitUserFuncs()
{
    BEGIN_IFPROXY_MAP( IInterfaceServer, false );

    ADD_PROXY_METHOD_EX( 1, 
        CRpcReqForwarderProxy::ForceCancelRequests,
        SYS_METHOD_FORCECANCELREQS );

    END_IFPROXY_MAP;

    BEGIN_IFHANDLER_MAP( CRpcReqForwarder );
    END_HANDLER_MAP;

    return 0;
}

gint32 CRpcReqForwarderProxy::RebuildMatches()
{
    do{
        // add interface id to all the matches
        CCfgOpenerObj oIfCfg( this );
        guint32 dwQueSize = MAX_DBUS_REQS;

        // empty all the matches, the interfaces will
        // be added to the vector on the remote req.
        m_vecMatches.clear();

        // set the interface id for this match
        CCfgOpenerObj oMatchCfg(
            ( CObjBase* )m_pIfMatch );

        oMatchCfg.SetBoolProp(
            propDummyMatch, true );

        oMatchCfg.SetIntProp(
            propIid, GetClsid() );

        dwQueSize = MAX_DBUS_REQS;

        oMatchCfg.SetIntProp(
            propQueSize, dwQueSize );

        // also append the match for master interface
        m_vecMatches.push_back( m_pIfMatch );

    }while( 0 );

    return 0;
}

gint32 CRpcReqForwarderProxy::AddInterface(
    IMessageMatch* pMatch )
{
    if( pMatch == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        MatchPtr ptrMatch( pMatch );
        CStdRMutex oIfLock( GetLock() );
        std::map< MatchPtr, IFREF >::iterator
            itr = m_mapMatchRefs.find( ptrMatch );

        if( itr == m_mapMatchRefs.end() )
        {
            m_mapMatchRefs[ ptrMatch ] =
                IFREF(m_vecMatches.size(), 1 );

            m_vecMatches.push_back( ptrMatch );
        }
        else
        {
            ++itr->second.second;
            ret = EEXIST;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::RemoveInterface(
    IMessageMatch* pMatch )
{
    if( pMatch == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        MatchPtr ptrMatch( pMatch );
        CStdRMutex oIfLock( GetLock() );
        std::map< MatchPtr, IFREF >::iterator
            itr = m_mapMatchRefs.find( ptrMatch );

        if( itr != m_mapMatchRefs.end() )
        {
            --itr->second.second;
            if( itr->second.second <= 0 )
            {
                m_vecMatches.erase(
                    m_vecMatches.begin() +
                    itr->second.first );

                m_mapMatchRefs.erase( ptrMatch );
            }
            else
            {
                ret = EEXIST;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::OnRmtSvrEvent(
    EnumEventId iEvent,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( hPort == INVALID_HANDLE ||
        pEvtCtx == nullptr )
        return -EINVAL;

    if( iEvent != eventRmtSvrOffline )
        return super::OnRmtSvrEvent(
            iEvent, pEvtCtx, hPort );

    return OnRmtSvrOffline( pEvtCtx, hPort );
}

gint32 CRpcReqForwarderProxy::OnRmtSvrOffline(
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( hPort == INVALID_HANDLE )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oEvtCtx( pEvtCtx );
        guint32 dwPortId = 0;
        ret = oEvtCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;
        
        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        TaskGrpPtr pParaGrp;

        ret = GetParallelGrp( pParaGrp );
        if( ERROR( ret ) )
            break;

        CIfParallelTaskGrp* pGroup = pParaGrp;
        if( unlikely( pGroup == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        std::vector< TaskletPtr > vecIoTasks;
        ret = pGroup->FindTaskByClsid(
            clsid( CIfIoReqTask ),
            vecIoTasks );

        if( ERROR( ret ) )
            break;

        // search for the tasks with the equal
        // portid to dwPortId, and cancel them
        for( auto elem : vecIoTasks )
        {
            CIfIoReqTask* pIoTask = elem;
            if( unlikely( pIoTask == nullptr ) )
                continue;

            CCfgOpenerObj oTaskCfg( pIoTask );

            guint32 dwTaskPortId = 0;
            ret = oTaskCfg.GetIntProp(
                propPortId, dwTaskPortId );

            if( ERROR( ret ) )
                continue;

            if( dwPortId != dwTaskPortId )
                continue;

            ( *pIoTask )( eventCancelTask );
        }
        
    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxy::OnKeepAliveOrig(
    IEventSink* pTask )
{
    gint32 ret = 0;

    if( unlikely( pTask == nullptr ) )
        return -EINVAL;

    // servicing the incoming request/event
    // message
    do{
        DMsgPtr pMsg;
        CCfgOpenerObj oCfg( pTask );

        ret = oCfg.GetMsgPtr( propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        if( pMsg.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 0, pObj );
        if( ERROR( ret ) )
            break;

        
        IConfigDb* pCfg = pObj;
        if( pCfg == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CReqOpener oEvent( pCfg );
        guint32 iSeqNo = 0;

        ret = oEvent.GetIntProp(
            propSeqNo, iSeqNo );

        if( ERROR( ret ) )
            break;

        TaskletPtr pTaskToKa;

        TaskGrpPtr pTaskGrp;
        GetParallelGrp( pTaskGrp );
        if( pTaskGrp.IsEmpty() )
        {
            ret = -ENOENT;    
            break;
        }

        std::vector< TaskletPtr > vecIoReqs;
        ret = pTaskGrp->FindTaskByClsid(
            clsid( CIfIoReqTask ), vecIoReqs );

        if( ERROR( ret ) )
            break;

        for( auto elem : vecIoReqs )
        {
            CCfgOpenerObj oTaskCfg(
                ( CObjBase* )elem );

            IConfigDb* pReq = nullptr;
            ret = oTaskCfg.GetPointer(
                propReqPtr, pReq );

            if( ERROR( ret ) )
                continue;

            CCfgOpener oReq( pReq );
            ret = oReq.IsEqual(
                propSeqNo, iSeqNo );

            if( ERROR( ret ) )
                continue;

            pTaskToKa = elem;
            break;
        }

        if( pTaskToKa.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        CIfIoReqTask* pReqTask = pTaskToKa;
        if( pReqTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        // send a keepalive event
        EnumEventId iEvent = ( EnumEventId )
            ( eventTryLock | eventKeepAlive );

        ret = pTaskToKa->OnEvent( iEvent,
            KARelay, 0, nullptr );

        if( ret == STATUS_PENDING )
            ret = 0;

    }while( 0 );

    return ret;
}

gint32 CRfpModEventRespTask::OnTaskComplete(
    gint32 iRet )
{
    gint32 ret = 0;
    do{
        CCfgOpener oTaskCfg(
            ( IConfigDb* )GetConfig() );

        guint32 dwEvent = 0;

        ret = oTaskCfg.GetIntProp(
            0, dwEvent );
        if( ERROR( ret ) )
            break;

        std::string strModule;
        ret = oTaskCfg.GetStrProp(
            1, strModule );

        if( ERROR( ret ) )
            break;

        CRpcReqForwarderProxy* pIf = nullptr;
        ret = oTaskCfg.GetPointer(
            propIfPtr, pIf );

        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pIf->GetParent() );

        pRouter->OnEvent(
            ( EnumEventId )dwEvent, 0, 0,
            ( LONGWORD* )strModule.c_str() );

    }while( 0 );

    return ret;
}
gint32 CRpcReqForwarderProxy::OnModEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    gint32 ret = 0;

    do{
        TaskletPtr pTask;

        if( !IsMyDest( strModule ) )
            break;

        if( TestSetState( iEvent )
            == ERROR_STATE )
            break;

        CParamList oParams;
        ret = oParams.Push( iEvent );

        if( ERROR( ret ) )
            break;

        oParams.Push( strModule );

        if( ERROR( ret ) )
            break;

        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret  ) )
            break;

        TaskletPtr pRespTask;
        ret = pRespTask.NewObj(
            clsid( CRfpModEventRespTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        // activate the task
        ( *pRespTask )( eventZero );

        ret = pTask.NewObj(
            clsid( CIfCpEventTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        CIfRetryTask* pRetryTask = pTask;
        pRetryTask->SetClientNotify( pRespTask );

        ret = AppendAndRun( pTask );
        if( ERROR( ret ) )
            break;

        if( ret == STATUS_PENDING )
            break;

        ret = pTask->GetError();
        if( SUCCEEDED( ret ) )
        {
            pRetryTask->ClearClientNotify();
            pRetryTask = pRespTask;
            pRetryTask->OnEvent( eventTaskComp,
                ret, 0, nullptr );
        }

    }while( 0 );

    return ret;
}

// a user-initialized cancel request
gint32 CRpcReqForwarderProxy::ForceCancelRequests(
	ObjPtr& pvecTasks,
	guint64& qwThisTaskId,
    IEventSink* pCallback )
{
	if( pvecTasks.IsEmpty() )
		return -EINVAL;

    const string& strIfName = CoGetIfNameFromIid(
        iid( IInterfaceServer ), "p" );

    if( strIfName.empty() )
        return ERROR_FAIL;

    CParamList oOptions;

    oOptions[ propIfName ] =
        DBUS_IF_NAME( strIfName );

    oOptions[ propSysMethod ] = true;

    CfgPtr pResp;
    // make the call
    std::string strMethod( __func__ );
    gint32 ret = AsyncCall(
        pCallback,
        oOptions.GetCfg(),
        pResp, strMethod,
        pvecTasks );

    if( ERROR( ret ) )
        return ret;

    if( ret == STATUS_PENDING )
        return ret;

    // fill the return code
    gint32 iRet = 0;
    ret = FillArgs( pResp, iRet );

    if( SUCCEEDED( ret ) )
        ret = iRet;

    if( SUCCEEDED( ret ) )
    {
        DebugPrint( 0,
            "Req Tasks Canceled" );
    }
    return ret;
}

gint32 CReqFwdrFetchDataTask::RunTask()
{
    gint32 ret = 0;

    ObjPtr pObj;
    CParamList oParams( GetConfig() );
    DMsgPtr pRespMsg;

    do{
        CRpcReqForwarder* pIf;
        ret = oParams.GetObjPtr( propIfPtr, pObj );
        if( ERROR( ret ) )
            break;

        pIf = pObj;
        if( pIf == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CRpcRouterReqFwdr* pRouter = nullptr;
        ret = oParams.GetPointer(
            propRouterPtr, pRouter );
        if( ERROR( ret ) )
            break;

        ObjPtr pDataDesc;
        ret = oParams.GetObjPtr( 0, pDataDesc );
        if( ERROR( ret ) )
            break;

        guint32 dwFd = 0;
        ret = oParams.GetIntProp( 1, dwFd );
        if( ERROR( ret ) )
            break;

        guint32 dwOffset = 0;
        ret = oParams.GetIntProp( 2, dwOffset );
        if( ERROR( ret ) )
            break;

        guint32 dwSize = 0;
        ret = oParams.GetIntProp( 3, dwSize );
        if( ERROR( ret ) )
            break;

        IConfigDb* pDesc = pDataDesc;
        if( unlikely( pDesc == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        IConfigDb* pTransCtx = nullptr;

        CParamList oDesc( pDesc );
        string strDest;
        ret = oDesc.GetStrProp(
            propDestDBusName, strDest );
        if( ERROR( ret ) )
            break;

        ret = oDesc.GetPointer(
            propTransCtx, pTransCtx );

        if( ERROR( ret ) )
            break;

        CCfgOpener oTransCtx( pTransCtx );
        guint32 dwPortId = 0;
        ret = oTransCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        string strRouterPath;
        ret = oTransCtx.GetStrProp(
            propRouterPath, strRouterPath );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        if( strRouterPath != "/" )
        {
            // requesting the IStreamMH
            // interface to handle this request
            oDesc.SetIntProp( propIid,
                iid( IStreamMH ) );
        }

        InterfPtr proxyPtr;
        ret = pRouter->GetBridgeProxy(
            dwPortId, proxyPtr );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxy* pProxy = proxyPtr;
        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        ret = pProxy->FetchData(
            pDataDesc, ( gint32& )dwFd,
            dwOffset, dwSize, this );

    }while( 0 );

    if( ret != STATUS_PENDING && !Retriable( ret ) )
    {
        oParams.GetObjPtr(
            propRespPtr, pObj );

        CParamList oRespCfg(
            ( IConfigDb* )pObj );

        oRespCfg.SetIntProp(
            propReturnValue, ret );

        if( !pRespMsg.IsEmpty() )
            oRespCfg.Push( pRespMsg );

        OnTaskComplete( ret );
    }

    return ret;
}

gint32 CReqFwdrFetchDataTask::OnTaskComplete(
    gint32 iRetVal )
{
    if( iRetVal == STATUS_PENDING )
        return iRetVal;

    gint32 ret = 0;
    if( Retriable( iRetVal ) )
    {
        ret = STATUS_MORE_PROCESS_NEEDED;
    }
    else if( ERROR( iRetVal ) )
    {
        ret = iRetVal;
    }

    if( IsPending() )
        OnServiceComplete( ret );

    return ret;
}

gint32 CReqFwdrFetchDataTask::OnServiceComplete(
    gint32 iRetVal )
{
    if( iRetVal == STATUS_PENDING ||
        Retriable( iRetVal ) )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oCfg( ( IConfigDb* )GetConfig() );
        IEventSink* pTask = nullptr;
        ret = oCfg.GetPointer(
            propEventSink, pTask );

        if( ERROR( ret ) )
            break;

        CInterfaceServer* pReqFwdr = nullptr;
        ret = oCfg.GetPointer( propIfPtr, pReqFwdr );
        if( ERROR( ret ) )
            break;

        IConfigDb* pResp = nullptr;
        ret = oCfg.GetPointer( propRespPtr, pResp );
        if( ERROR( ret ) )
        {
            if( SUCCEEDED( iRetVal ) )
                iRetVal = ret;
            
            // if error, generate a response
            CCfgOpener oResp;
            oResp.SetIntProp(
                propReturnValue, iRetVal );

            ret = pReqFwdr->OnServiceComplete( 
                oResp.GetCfg(), pTask );
        }
        else
        {
            CCfgOpener oResp( pResp );
            IConfigDb* pDesc = nullptr;
            ret = oResp.GetPointer( 0, pDesc );
            if( SUCCEEDED( ret ) )
            {
                CCfgOpener oDesc( pDesc );
                // make sure the iid is istream
                if( oDesc.IsEqual( propIid,
                    iid( IStreamMH ) )  )
                {
                    oDesc[ propIid ] =
                        iid( IStream );
                }
            }
            ret = pReqFwdr->OnServiceComplete( 
                pResp, pTask );
        }

        oCfg.RemoveProperty( propRespPtr );
        ClearClientNotify();

    }while( 0 );

    return ret;
}

}
