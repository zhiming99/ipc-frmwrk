/*
 * =====================================================================================
 *
 *       Filename:  multihop.cpp
 *
 *    Description:  multihop related classes and tasks 
 *
 *        Version:  1.0
 *        Created:  02/26/2020 08:05:04 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi(woodhead99@gmail.com )
 *   Organization:
 *
 *      Copyright:  2020 Ming Zhi( woodhead99@gmail.com )
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
#include "connhelp.h"

gint32 CRpcTcpBridge::OnCheckRouterPathComplete(
    IEventSink* pCallback, 
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pCallback == nullptr ||
        pIoReq == nullptr ||
        pReqCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp( propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
            ret = iRet;

    }while( 0 );

    CParamList oResp;
    oResp[ propReturnValue ] = ret;
    OnServiceComplete(
        oResp.GetCfg(), pCallback );

    if( pReqCtx )
    {
        CParamList oContext( pReqCtx );
        oContext.ClearParams();
    }

    return ret;
}

gint32 CRpcTcpBridge::CheckRouterPathAgain(
    IEventSink* pCallback, IConfigDb* pReqCtx )
{
    // we are here after the proxy port is
    // created.
    if( pReqCtx == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;

    CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );
    CIoManager* pMgr = GetIoMgr();

    do{
        CCfgOpener oReqCtx( pReqCtx );
        std::string strPath;
        oReqCtx.GetStrProp(
            propRouterPath, strPath );

        InterfPtr pIf;
        std::string strNode;
        ret = pRouter->GetNodeName(
            strPath, strNode );
        if( ERROR( ret ) )
            break;

        std::string strNext =
            strPath.substr( 1 + strNode.size() );

        if( strNext.empty() )
        {
            strNext = "/";
        }
        else if( strNext[ 0 ] != '/' &&
            strNext.size() < 2 )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwPortId = 0;
        ret = pRouter->GetProxyIdByNodeName(
            strNode, dwPortId );
        if( ERROR( ret ) )
        {
            // open port failed
            ret = ERROR_FAIL;
            break;
        }
        else
        {
            if( strNext == "/" )
                break;

            InterfPtr pIf;    
            ret = pRouter->GetBridgeProxy(
                dwPortId, pIf );
            if( ERROR( ret ) )
                break;

            CRpcTcpBridgeProxy* pProxy = pIf;
            if( unlikely( pProxy == nullptr ) )
            {
                ret = -EFAULT;
                break;
            }

            oReqCtx[ propRouterPath ] = strNext;

            TaskletPtr pRespCb;
            ret = NEW_PROXY_RESP_HANDLER2(
                pRespCb, ObjPtr( this ),
                &CRpcTcpBridge::OnCheckRouterPathComplete,
                pCallback, oReqCtx.GetCfg() );

            if( ERROR( ret ) )
                break;

            TaskletPtr pChkRt;
            ret = DEFER_IFCALLEX_NOSCHED2(
                1, pChkRt, ObjPtr( pProxy ),
                &CRpcTcpBridgeProxy::CheckRouterPath,
                oReqCtx.GetCfg(),
                ( IEventSink* )nullptr );

            if( ERROR( ret ) )
            {
                ( *pRespCb )( eventCancelTask );
                break;
            }

            ( ( CIfRetryTask* )pChkRt )->
                SetClientNotify( pRespCb );
                
            
            // schedule a new task to release the
            // router's seq task queue 
            ret = pMgr->RescheduleTask( pChkRt );
            if( SUCCEEDED( ret ) )
                ret = pChkRt->GetError();

            if( ERROR( ret ) )
            {
               ( *pChkRt )(eventCancelTask );
               ( *pRespCb )(eventCancelTask );
            }

        }

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        OnServiceComplete(
            oResp.GetCfg(), pCallback );
    }

    return ret;
}

gint32 CRpcTcpBridge::CheckRouterPath(
    IEventSink* pCallback,
    IConfigDb* pReqCtx )
{
    if( pReqCtx == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;

    CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );
    do{
        CCfgOpener oReqCtx;
        std::string strPath;
        ret = oReqCtx.CopyProp(
            propRouterPath, pReqCtx );

        if( ERROR( ret ) )
            break;

        oReqCtx.GetStrProp(
            propRouterPath, strPath );

        // bound for this node
        if( strPath == "/" )
            break;

        ret = oReqCtx.CopyProp(
            propConnHandle, propPortId, this );
        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        std::string strNode;
        ret = pRouter->GetNodeName(
            strPath, strNode );
        if( ERROR( ret ) )
            break;

        std::string strNext =
            strPath.substr( 1 + strNode.size() );

        if( strNext.empty() )
        {
            strNext = "/";
        }
        else if( strNext[ 0 ] != '/' &&
            strNext.size() < 2 )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwPortId = 0;
        ret = pRouter->GetProxyIdByNodeName(
            strNode, dwPortId );

        if( SUCCEEDED( ret ) )
        {
            // no need to check further, the
            // presence of bridge proxy is OK to
            // make rpc calls.
            if( strNext == "/" )
                break;

            InterfPtr pIf;    
            ret = pRouter->GetBridgeProxy(
                dwPortId, pIf );
            if( ERROR( ret ) )
                break;

            CRpcTcpBridgeProxy* pProxy = pIf;
            if( unlikely( pProxy == nullptr ) )
            {
                ret = -EFAULT;
                break;
            }

            oReqCtx[ propRouterPath ] = strNext;

            TaskletPtr pRespCb;
            ret = NEW_PROXY_RESP_HANDLER2(
                pRespCb, ObjPtr( this ),
                &CRpcTcpBridge::OnCheckRouterPathComplete,
                pCallback, oReqCtx.GetCfg() );

            if( ERROR( ret ) )
                break;

            ret = pProxy->CheckRouterPath(
                oReqCtx.GetCfg(), pRespCb );

            if( ERROR( ret ) )
                ( *pRespCb )( eventCancelTask );
        }
        else
        {
            // the bridge proxy is yet to create.
            CParamList oParams;

            CfgPtr pConnParams;
            ret = pRouter->GetConnParamsByNodeName(
                strNode, pConnParams );
            if( ERROR( ret ) )
                break;

            bool bServer = false;
            oParams.Push( bServer );

            oParams.SetPointer(
                propIoMgr, GetIoMgr() );

            oParams.SetPointer(
                propIfPtr, this );

            oParams.SetPointer( propConnParams,
                ( IConfigDb* )pConnParams );

            oParams.SetObjPtr(
                propRouterPtr, ObjPtr( pRouter ) );

            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CRouterOpenBdgePortTask  ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            CIfInterceptTaskProxy*
                pOpenPortTask = pTask;

            TaskletPtr pChkRt;
            ret = NEW_COMPLETE_CALLBACK(
                pChkRt, nullptr, ObjPtr( this ),
                &CRpcTcpBridge::CheckRouterPathAgain,
                pCallback, oReqCtx.GetCfg() );
            if( ERROR( ret ) )
            {
                ( *pOpenPortTask )( eventCancelTask );
                break;
            }
            pOpenPortTask->SetInterceptTask( 
                ( IEventSink* ) pChkRt );

            ret = pRouter->AddSeqTask(
                pTask, false );
            if( ERROR( ret ) )
            {
                ( *pOpenPortTask )( eventCancelTask );
                ( *pChkRt )( eventCancelTask );
            }
        }

    }while( 0 );

    if( SUCCEEDED( ret ) )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        SetResponse(
            pCallback, oResp.GetCfg() );
    }

    return ret;
}

gint32 CRpcTcpBridgeProxy::CustomizeRequest(
    IConfigDb* pReqCfg,
    IEventSink* pCallback )
{
    if( pReqCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CParamList oParams( pReqCfg );

        std::string strMethod;
        ret = oParams.GetStrProp(
            propMethodName, strMethod );

        if( ERROR( ret ) )
            break;

        if( strMethod ==
            SYS_METHOD_CHECK_ROUTERPATH )
        {
            IConfigDb* pCtx = nullptr;
            ret = oParams.GetPointer(
                propContext, pCtx );

            if( ERROR( ret ) )
            {
                ret = -EINVAL;
                break;
            }

            // the port id for canceling purpose
            // if the bridge is down
            ret = oParams.CopyProp( propPortId,
                propConnHandle, pCtx );

            if( ERROR( ret ) )
                break;

            CCfgOpener oReqCtx( pCtx );
            oReqCtx.RemoveProperty(
                propConnHandle );
        }
        else
        {
            ret = super::CustomizeRequest(
                pReqCfg, pCallback );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::CheckRouterPath(
    IConfigDb* pEvtCtx,
    IEventSink* pCallback )
{
    // NOTE: pCallback is not an output parameter
    // from the server.
    //
    // this is an async call
    gint32 ret = 0;

    if( pEvtCtx == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    do{
        CParamList oOptions;
        CParamList oResp;
        EnumClsid iid = iid( CRpcTcpBridge );
        const std::string& strIfName =
            CoGetIfNameFromIid( iid, "p" );

        if( strIfName.empty() )
        {
            ret = -ENOTSUP;
            break;
        }

        oOptions[ propIfName ] = 
            DBUS_IF_NAME( strIfName );

        oOptions[ propSysMethod ] = ( bool )true;

        CCfgOpenerObj oTask(
            ( CObjBase* )pCallback ); 

        oTask.SetPointer(
            propContext, pEvtCtx );
            
        ret = AsyncCall( pCallback,
            oOptions.GetCfg(), oResp.GetCfg(),
            __func__, pEvtCtx );

        if( ret == STATUS_PENDING )
            break;
        
        if( ERROR( ret ) )
            break;

        ret = oTask.SetObjPtr(
            propRespPtr, oResp.GetCfg() );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarder::OnCheckRouterPathComplete(
    IEventSink* pCallback, 
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pCallback == nullptr ||
        pIoReq == nullptr ||
        pReqCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CParamList oParams;
    do{
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp( propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

    }while( 0 );

    oParams[ propReturnValue ] = ret;
    if( SUCCEEDED( ret ) )
    {
        oParams.CopyProp(
            propConnHandle, pReqCtx );

        oParams.CopyProp(
            propConnParams, pReqCtx );
    }

    OnServiceComplete(
        oParams.GetCfg(), pCallback );

    return ret;
}

gint32 CRpcReqForwarder::CheckRouterPath(
    IEventSink* pCallback,
    IConfigDb* pReqCtx )
{
    if( pReqCtx == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;

    EventPtr pEvt;
    CRpcRouterReqFwdr* pRouter =
        static_cast< CRpcRouterReqFwdr* >
            ( GetParent() );

    CCfgOpener oReqCtx;
    do{
        std::string strPath;

        ret = oReqCtx.CopyProp(
            propRouterPath, pReqCtx );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        InterfPtr pIf;    
        ret = pRouter->GetBridgeProxy(
            dwPortId, pIf );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxy* pProxy = pIf;
        if( unlikely( pProxy == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        CIfInterceptTaskProxy* pIntercept =
            ObjPtr( pCallback );

        ret = pIntercept->
            GetInterceptTask( pEvt );
        if( ERROR( ret ) )
            break;

        // move the intercepted task 
        // to OnCheckRouterPathComplete

        TaskletPtr pRespCb;
        ret = NEW_PROXY_RESP_HANDLER2(
            pRespCb, ObjPtr( this ),
            &CRpcReqForwarder::OnCheckRouterPathComplete,
            pEvt, oReqCtx.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = pProxy->CheckRouterPath(
            oReqCtx.GetCfg(), pRespCb );

        if( ERROR( ret ) )
            ( *pRespCb )( eventCancelTask );

        if( ret == STATUS_PENDING )
            pIntercept->
                SetInterceptTask( nullptr );

    }while( 0 );

    if( ret != STATUS_PENDING && !pEvt.IsEmpty() )
    {
        oReqCtx[ propReturnValue ] = ret;
        SetResponse( pEvt, oReqCtx.GetCfg() );
    }

    return 0;
}

gint32 CRpcRouterBridge::BuildRmtSvrEventMH(
    EnumEventId iEvent,
    IConfigDb* pCtx,
    CfgPtr& pEvtReq )
{
    gint32 ret = 0;

    do{
        CReqBuilder oParams;

        oParams.SetIfName(
            DBUS_IF_NAME( IFNAME_TCP_BRIDGE ) );

        CIoManager* pMgr = GetIoMgr();

        std::string strRtName;
        pMgr->GetRouterName( strRtName );

        oParams.SetObjPath( DBUS_OBJ_PATH(
            strRtName, OBJNAME_TCP_BRIDGE ) );

        oParams.SetSender(
            DBUS_DESTINATION( strRtName ) );

        oParams.SetMethodName(
            SYS_EVENT_RMTSVREVENT );

        oParams.SetIntProp( propStreamId,
            TCP_CONN_DEFAULT_STM );

        oParams.Push( ObjPtr( pCtx ) );
        oParams.Push( iEvent );

        oParams.SetIntProp(
            propCmdId, CTRLCODE_SEND_EVENT );

        oParams.SetCallFlags( CF_ASYNC_CALL |
            DBUS_MESSAGE_TYPE_SIGNAL );

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::RemoveRemoteMatchByPath(
    const std::string& strUpstm,
    std::vector< MatchPtr >& vecMatches )
{
    if( strUpstm.empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        std::map< MatchPtr, gint32 >* plm =
            &m_mapRmtMatches;

        CStdRMutex oRouterLock( GetLock() );
        std::map< MatchPtr, gint32 >::iterator
            itr = plm->begin();
        while( itr != plm->end() )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )itr->first );

            std::string strPath;
            ret = oMatch.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
            {
                ++itr;
                continue;
            }

            if( strPath.size() < strUpstm.size() )
            {
                ++itr;
                continue;
            }
            if( strPath.substr( 0, strUpstm.size() )
                != strUpstm )
            {
                ++itr;
                continue;
            }
            if( strPath.size() == strUpstm.size() ||
                strPath[ strUpstm.size() ] == '/' )
            {
                vecMatches.push_back( itr->first );
                itr = plm->erase( itr );
            }
            else
            {
                ++itr;
            }
        }

    }while( 0 );

    return vecMatches.size();
}

gint32 CRpcRouterBridge::OnRmtSvrOfflineMH(
    IEventSink* pCallback,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( hPort == INVALID_HANDLE ||
        pEvtCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CIoManager* pMgr = GetIoMgr();
    do{
        InterfPtr pIf;
        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        CCfgOpener oEvtCtx( pEvtCtx );
        std::string strPath, strNode;
        ret = oEvtCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        ret = oEvtCtx.GetStrProp(
            propNodeName, strNode );

        if( ERROR( ret ) )
            break;

        strPath = std::string( "/" )
            + strNode + strPath;
        oEvtCtx.SetStrProp( 
            propRouterPath, strPath );

        // remove all the related matches
        std::vector< MatchPtr > vecMatches;
        ret = RemoveRemoteMatchByPath(
            strPath, vecMatches );
        if( vecMatches.empty() )
            break;

        // forward to the downstream subscribers
        std::set< gint32 > setBridges;
        for( auto elem : vecMatches )
        {
            CCfgOpenerObj oMatchCfg(
                ( CObjBase* ) elem );

            guint32 dwPortId = 0;
            ret = oMatchCfg.GetIntProp(
                propPortId, dwPortId );
            if( ERROR( ret ) )
                continue;

            setBridges.insert( dwPortId );
        }

        if( setBridges.empty() )
            break;

        CfgPtr pReqCall;
        ret = BuildRmtSvrEventMH(
            eventRmtSvrOffline,
            oEvtCtx.GetCfg(),
            pReqCall );

        TaskGrpPtr pGrp;
        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );
        ret = pGrp.NewObj(
            clsid( CIfParallelTaskGrp ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        pGrp->SetClientNotify( pCallback );
        for( auto elem : setBridges )
        {
            InterfPtr pIf;
            ret = GetBridge( elem, pIf );
            if( ERROR( ret ) )
                continue;

            CRpcTcpBridge* pBridge = pIf;
            if( unlikely( pBridge == nullptr ) )
                continue;

            TaskletPtr pTask;
            ret = DEFER_IFCALLEX_NOSCHED2(
                1, pTask, ObjPtr( pBridge ),
                &CRpcTcpBridge::BroadcastEvent,
                pReqCall,
                ( IEventSink* )nullptr );

            if( ERROR( ret ) )
                continue;

            pGrp->AppendTask( pTask );
        }

        if( pGrp->GetTaskCount() > 0 )
        {
            TaskletPtr pGrpTask( pGrp );
            ret = DEFER_CALL( pMgr, this,
                &CRpcServices::RunManagedTask,
                pGrpTask, false );

            if( ERROR( ret ) )
                ( *pGrp )( eventCancelTask );
            else
                ret = pGrp->GetError();
        }

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::OnClearRemoteEventsComplete(
    IEventSink* pCallback,
    IEventSink*  pIoReq,
    IConfigDb* pReqCtx )
{
    if( pCallback == nullptr )
        return -EINVAL;

    // BUGBUG: simply set success and complete the
    // request, because at this moment, all the
    // tasks are gone.
    CfgPtr pCfg( pReqCtx );
    SetResponse( pCallback, pCfg );
    return 0;
}
/**
* @name ClearRemoteEventsMH
* @{ * */
/**
 * Disable an array of matches at the request from
 * a remote bridge proxy. Or clear all the matches
 * assocatited with the bridge to shutdown. 
 *
 * @Parameters
 *      pvecMatches: matches to clear or disable.
 *      bForceClear: whether to clear the matches
 *      on the downstream nodes, ignoring the
 *      matches refcount.
 * @} */

gint32 CRpcRouterBridge::ClearRemoteEventsMH(
    IEventSink* pCallback,
    ObjPtr& pVecMatches,
    bool bForceClear )
{
    if( unlikely( pVecMatches.IsEmpty() ||
        pCallback == nullptr ) )
        return -EINVAL;

    ObjVecPtr pMatches( pVecMatches );
    if( unlikely( pMatches.IsEmpty() ) )
        return -EINVAL;

    if( unlikely( ( *pMatches )().size() == 0 ) )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;

    do{
        std::vector< ObjPtr >& vecMatches =
            ( *pMatches )();

        CParamList oParams;
        oParams[ propEventSink ] =
            ObjPtr( pCallback );

        oParams[ propIfPtr ] = ObjPtr( this );

        TaskGrpPtr pTaskGrp;
        ret = pTaskGrp.NewObj(
            clsid( CIfParallelTaskGrp ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        if( !bForceClear )
        {
            CCfgOpener oReqCtx;
            oReqCtx[ propReturnValue ] = 0;
            TaskletPtr pRespCb;

            ret = NEW_PROXY_RESP_HANDLER2(
                pRespCb, ObjPtr( this ),
                &CRpcRouterBridge::OnClearRemoteEventsComplete,
                pCallback, oReqCtx.GetCfg() );

            if( ERROR( ret ) )
                break;

            pTaskGrp->SetClientNotify( pRespCb );
        }
        else
        {
            pTaskGrp->SetClientNotify( pCallback );
        }

        std::map< std::string, std::vector< ObjPtr > >
            mapIfMatches;

        for( auto pObj : vecMatches )
        {
            MatchPtr pMatch( pObj );
            MatchPtr pRmtMatch;

            CCfgOpenerObj oMatch(
                ( CObjBase* )pObj );

            oMatch.CopyProp( propPortId, this );

            ret = GetMatchToAdd(
                pMatch, true, pRmtMatch );

            if( ERROR( ret ) )
                continue;

            ret = RemoveRemoteMatch( pRmtMatch );

            if( !bForceClear )
            {
                if( ret == EEXIST || ERROR( ret ) )
                {
                    ret = 0;
                    continue;
                }
            }

            std::string strPath;
            ret = oMatch.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
                continue;

            if( strPath.size() < 2 )
                continue;

            std::string strNode;
            ret = GetNodeName(
                strPath, strNode );
            if( ERROR( ret ) )
                continue;
            
            std::vector< ObjPtr >& vecIfMatches =
                mapIfMatches[ strNode ];

            strPath = strPath.substr(
                1 + strNode.size() );

            if( strPath.empty() )
                strPath = "/";

            CCfgOpenerObj oRmtMatch(
                ( CObjBase* )pRmtMatch );

            oRmtMatch.SetStrProp(
                propRouterPath, strPath );

            oRmtMatch.RemoveProperty(
                propPortId );

            vecIfMatches.push_back(
                ObjPtr( pRmtMatch ) );
        }

        for( auto elem : mapIfMatches )
        {
            InterfPtr pIf;

            ret = GetBridgeProxy(
                elem.first, pIf );

            if( ERROR( ret ) )
                continue;

            CRpcTcpBridgeProxy* pProxy = pIf;

            TaskletPtr pTask;
            if( elem.second.size() < 2 )
            {
                ret = DEFER_IFCALLEX_NOSCHED2(
                    0, pTask, ObjPtr( pProxy ),
                    &CRpcTcpBridgeProxy::DisableRemoteEvent,
                    ( IEventSink* )nullptr,
                    ( IMessageMatch* )elem.second[ 0 ] );
            }
            else
            {
                ObjVecPtr pvecIfMat( true );
                ( *pvecIfMat )() = elem.second;

                ret = DEFER_IFCALLEX_NOSCHED2(
                    1, pTask, ObjPtr( pProxy ),
                    &CRpcTcpBridgeProxy::ClearRemoteEvents,
                    ObjPtr( pvecIfMat ),
                    ( IEventSink* )nullptr );
            }
            if( SUCCEEDED( ret ) )
                pTaskGrp->AppendTask( pTask ); \
        }

        ret = 0;
        if( pTaskGrp->GetTaskCount() == 0 )
            break;

        TaskletPtr pTask = ObjPtr( pTaskGrp );
        ret = DEFER_CALL( GetIoMgr(), this,
            &CRpcServices::RunManagedTask,
            pTask, false );

        if( ERROR( ret ) )
            ( *pTask )( eventCancelTask );
        else
            ret = pTask->GetError();

    }while( 0 );

    if( ERROR( ret ) )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        SetResponse(
            pCallback, oResp.GetCfg() );
    }

    return ret;
}

gint32 CRpcTcpBridge::EnableRemoteEventInternalMH(
    IEventSink* pCallback,
    IMessageMatch* pMatch,
    bool bEnable )
{
    if( unlikely( pMatch == nullptr ||
        pCallback == nullptr ) )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;
    CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );
    do{
        MatchPtr pRmtMatch;
        CCfgOpenerObj oMatch( pMatch );
        ret = pRouter->GetMatchToAdd(
            pMatch, true, pRmtMatch );

        if( bEnable )
            ret = pRouter->AddRemoteMatch(
                pRmtMatch );
        else
            ret = pRouter->RemoveRemoteMatch(
                pRmtMatch );

        if( ret == EEXIST || ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        std::string strPath;
        ret = oMatch.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        if( strPath.size() < 2 )
        {
            ret = -EINVAL;
            break;
        }

        std::string strNode;
        ret = pRouter->GetNodeName(
            strPath, strNode );
        if( ERROR( ret ) )
            break;
        
        strPath = strPath.substr(
            1 + strNode.size() );

        if( strPath.empty() )
            strPath = "/";

        CCfgOpenerObj oRmtMatch(
            ( CObjBase* )pRmtMatch );

        oRmtMatch.SetStrProp(
            propRouterPath, strPath );

        oRmtMatch.RemoveProperty(
            propPortId );

        InterfPtr pIf;
        ret = pRouter->GetBridgeProxy(
            strNode, pIf );
        if( ERROR( ret ) )
            continue;

        CRpcTcpBridgeProxy* pProxy = pIf;

        TaskletPtr pTask;
        if( bEnable )
        {
            ret = DEFER_HANDLER_NOSCHED(
                pTask, ObjPtr( pProxy ),
                &CRpcTcpBridgeProxy::EnableRemoteEvent,
                pCallback,
                ( IMessageMatch* )pRmtMatch );
        }
        else
        {
            ret = DEFER_HANDLER_NOSCHED(
                pTask, ObjPtr( pProxy ),
                &CRpcTcpBridgeProxy::DisableRemoteEvent,
                pCallback,
                ( IMessageMatch* )pRmtMatch );
        }

        ret = DEFER_CALL( GetIoMgr(),
            ObjPtr( pProxy ),
            &CRpcServices::RunManagedTask,
            pTask, false );

        if( ERROR( ret ) )
            ( *pTask )( eventCancelTask );
        else
            ret = pTask->GetError();

    }while( 0 );

    return ret;
}