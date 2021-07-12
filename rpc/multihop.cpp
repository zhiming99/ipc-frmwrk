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

namespace rpcf
{

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
    std::string strPath, strNode;
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

        CCfgOpenerObj oIfCfg( this );
        guint32 dwPortId = 0;
        ret = oIfCfg.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        CCfgOpener oReqCtx( pReqCtx );
        ret = oReqCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );

        ret = CRpcRouter::GetNodeName(
            strPath, strNode );
        if( ERROR( ret ) )
            break;

        guint32 dwProxyId;
        ret = pRouter->GetProxyIdByNodeName(
            strNode, dwProxyId );
        if( ERROR( ret ) )
            break;

        ret = pRouter->AddRefCount(
            strNode, dwPortId, dwProxyId );

        if( ERROR( ret ) )
            break;

        ret = 0;

    }while( 0 );

    CParamList oResp;
    oResp[ propReturnValue ] = ret;

    if( SUCCEEDED( ret ) )
        oResp[ propRouterPath ] = strPath;

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
    IEventSink* pCallback,
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    // we are here after the proxy port is
    // created.
    if( pReqCtx == nullptr ||
        pCallback == nullptr ||
        pIoReq == nullptr )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;
    CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );
    CIoManager* pMgr = GetIoMgr();
    std::string strPath;

    do{
        CCfgOpenerObj oIoReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oIoReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp(
            propReturnValue, ( guint32& )iRet );
        if( ERROR( ret ) )
            break;

        // the proxy fails to create
        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        CCfgOpener oReqCtx( pReqCtx );
        oReqCtx.GetStrProp(
            propRouterPath, strPath );

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
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        guint32 dwProxyId = 0;
        ret = pRouter->GetProxyIdByNodeName(
            strNode, dwProxyId );
        if( ERROR( ret ) )
        {
            // open port failed
            ret = ERROR_FAIL;
            break;
        }

        if( strNext == "/" )
        {
            ret = pRouter->FindRefCount(
                strNode, dwPortId );

            if( SUCCEEDED( ret ) )
                break;

            ret = pRouter->AddRefCount(
                strNode, dwPortId, dwProxyId );

            if( ERROR( ret ) )
                break;

            ret = 0;
            break;
        }

        InterfPtr pIf;
        ret = pRouter->GetBridgeProxy(
            dwProxyId, pIf );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxy* pProxy = pIf;
        if( unlikely( pProxy == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        oReqCtx[ propRouterPath ] = strPath;

        CCfgOpener oReqCtx2;
        oReqCtx2.SetStrProp(
            propRouterPath, strNext );

        oReqCtx2.CopyProp(
            propObjList, pReqCtx );

        AddCheckStamp( oReqCtx2.GetCfg() );

        TaskletPtr pMajorCall;
        ret = DEFER_CALL_NOSCHED(
            pMajorCall, ObjPtr( pProxy ),
            &CRpcTcpBridgeProxy::CheckRouterPath,
            oReqCtx2.GetCfg(),
            ( IEventSink* )nullptr );
        if( ERROR( ret ) )
            break;

        TaskletPtr pIoCall;
        ret = NEW_PROXY_IOTASK( 
            1, pIoCall, pMajorCall, ObjPtr( this ),
            &CRpcTcpBridge::OnCheckRouterPathComplete,
            pCallback, pReqCtx );
        if( ERROR( ret ) )
            break;

        // reschedule a task to let the seq
        // queue move forward
        ret = pMgr->RescheduleTask( pIoCall );
        if( ERROR( ret ) )
        {
            (*pIoCall)( eventCancelTask );
        }
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        if( SUCCEEDED( ret ) )
            oResp[ propRouterPath ] = strPath;
        OnServiceComplete(
            oResp.GetCfg(), pCallback );
    }

    return ret;
}

bool CRpcTcpBridge::IsAccesable(
    IConfigDb* pReqCtx )
{ return true; }

gint32 CRpcTcpBridge::IsCyclicPath(
    IConfigDb* pReqCtx )
{
    gint32 ret = 0;
    if( pReqCtx == nullptr )
        return false;

    CCfgOpener oReqCtx( pReqCtx );
    ObjPtr pObj;
    ret = oReqCtx.GetObjPtr(
        propObjList, pObj );
    if( ERROR( ret ) )
        return false;

    QwVecPtr pBridgeIds = pObj;
    if( pBridgeIds.IsEmpty() )
        return false;

    guint64 qwObjId = GetObjId();
    for( auto elem : ( *pBridgeIds )() )
    {
        if( qwObjId == elem )
            return true;
    }
    return false;
}

gint32 CRpcTcpBridge::AddCheckStamp(
    IConfigDb* pReqCtx )
{
    gint32 ret = 0;
    if( pReqCtx == nullptr )
        return -EINVAL;

    do{
        ObjPtr pObj;
        QwVecPtr pBridgeIds;

        CCfgOpener oReqCtx( pReqCtx );
        ret = oReqCtx.GetObjPtr(
            propObjList, pObj );
        if( ERROR( ret ) )
        {
            ret = pBridgeIds.NewObj();
            if( ERROR( ret ) )
                break;
            pObj = pBridgeIds;
            oReqCtx.SetObjPtr(
                propObjList, pObj );
        }
        else
        {
            pBridgeIds = pObj;
        }

        ( *pBridgeIds )().push_back(
            GetObjId() );

    }while( 0 );

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

    std::string strPath;
    do{
        if( IsCyclicPath( pReqCtx ) )
        {
            ret = -EINVAL;
            break;
        }

        // check if this router is accessable for
        // the session
        if( !IsAccesable( pReqCtx ) )
        {
            ret = -EACCES;
            break;
        }

        CCfgOpener oReqCtx;
        ret = oReqCtx.CopyProp(
            propRouterPath, pReqCtx );
        if( ERROR( ret ) )
            break;

        oReqCtx.CopyProp(
            propObjList, pReqCtx );

        oReqCtx.GetStrProp(
            propRouterPath, strPath );

        if( strPath == "/" )
        {
            // we cannot be here.
            ret = -EINVAL;
            break;
        }

        // move on to next node
        ret = oReqCtx.CopyProp(
            propConnHandle, propPortId, this );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        oReqCtx.GetIntProp(
            propConnHandle, dwPortId );

        InterfPtr pIf;
        std::string strNode;
        ret = pRouter->GetNodeName(
            strPath, strNode );
        if( ERROR( ret ) )
        {
            ret = -ENOTDIR;
            break;
        }

        std::string strNext =
            strPath.substr( 1 + strNode.size() );

        if( strNext.empty() )
        {
            strNext = "/";
        }
        else if( strNext[ 0 ] != '/' &&
            strNext.size() < 2 )
        {
            ret = -ENOTDIR;
            break;
        }

        std::vector< std::string > vecNodes;

        ret = pRouter->GetLBNodes(
            strNode, vecNodes );

        if( SUCCEEDED( ret ) &&
            !vecNodes.empty() )
        {
            return CheckRouterPathLB(
                pCallback,
                ( IConfigDb* )oReqCtx.GetCfg(),
                strPath, strNext, strNode,
                vecNodes );
        }

        guint32 dwProxyId = 0;
        ret = pRouter->GetProxyIdByNodeName(
            strNode, dwProxyId );

        if( SUCCEEDED( ret ) )
        {
            // no need to check further if strNext
            // is the root dir, the presence of
            // bridge proxy is ok to make further
            // rpc calls.
            if( strNext == "/" )
            {
                ret = pRouter->FindRefCount(
                    strNode, dwPortId );

                if( SUCCEEDED( ret ) )
                    break;

                ret = pRouter->AddRefCount(
                    strNode, dwPortId, dwProxyId );

                if( ERROR( ret ) )
                    break;

                ret = 0;
                break;
            }

            InterfPtr pIf;    
            ret = pRouter->GetBridgeProxy(
                dwProxyId, pIf );
            if( ERROR( ret ) )
                break;

            CRpcTcpBridgeProxy* pProxy = pIf;
            if( unlikely( pProxy == nullptr ) )
            {
                ret = -EFAULT;
                break;
            }

            oReqCtx[ propRouterPath ] = strPath;

            // oReqCtx2 as the reqctx to next node
            CCfgOpener oReqCtx2;
            oReqCtx2[ propRouterPath ] = strNext;

            ObjPtr pObj;
            // for cyclic path avoidance
            oReqCtx2.CopyProp(
                propObjList, pReqCtx );
            ret = AddCheckStamp(
                oReqCtx2.GetCfg() );
            if( ERROR( ret ) )
                break;

            TaskletPtr pRespCb;
            ret = NEW_PROXY_RESP_HANDLER2(
                pRespCb, ObjPtr( this ),
                &CRpcTcpBridge::OnCheckRouterPathComplete,
                pCallback, oReqCtx.GetCfg() );

            if( ERROR( ret ) )
                break;

            ret = pProxy->CheckRouterPath(
                oReqCtx2.GetCfg(), pRespCb );

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
            {
                ret = -ENOTDIR;
                break;
            }

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

            oParams.SetStrProp(
                propNodeName, strNode );

            oParams.CopyProp(
                propPortId, this );

            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CRouterOpenBdgePortTask  ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            // pChkRt will be inserted between
            // pOpenPortTask and the pCallback
            TaskletPtr pChkRt;
            ret = NEW_PROXY_RESP_HANDLER2(
                pChkRt, ObjPtr( this ),
                &CRpcTcpBridge::CheckRouterPathAgain,
                pCallback, oReqCtx.GetCfg() );
            if( ERROR( ret ) )
            {
                ( *pTask )( eventCancelTask );
                break;
            }

            CIfInterceptTaskProxy*
                pOpenPortTask = pTask;

            pOpenPortTask->SetClientNotify(
                pChkRt );

            ret = pRouter->AddSeqTask(
                pTask, false );
            if( ERROR( ret ) )
            {
                ( *pTask )( eventCancelTask );
                ( *pChkRt )( eventCancelTask );
            }
            else if( SUCCEEDED( ret ) )
            {
                ret = STATUS_PENDING;
            }
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        if( SUCCEEDED( ret ) )
            oResp[ propRouterPath ] = strPath;
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

        CRpcRouter* pRouter = GetParent();

        if( strMethod ==
            SYS_METHOD_CHECK_ROUTERPATH )
        {
            if( !pRouter->HasBridge() )
                break;

            IConfigDb* pCtx = nullptr;
            ret = oParams.GetPointer(
                propContext, pCtx );

            if( ERROR( ret ) )
            {
                ret = -EINVAL;
                break;
            }

            // the port id for canceling purpose
            // if the local bridge object is down
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

gint32 CRpcReqForwarder::SchedToStopBridgeProxy( 
    IConfigDb* pReqCtx )
{
    gint32 ret = 0;
    do{
        guint32 dwPortId = 0;
        // stop the bridge proxy if necessary
        std::string strSrcUniqName, strSender;
        CCfgOpener oReqCtx( pReqCtx );
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.GetStrProp(
            propSrcUniqName, strSrcUniqName );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.GetStrProp(
            propSrcDBusName, strSender );
        if( ERROR( ret ) )
            break;

        TaskletPtr pTask;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pTask, ObjPtr( this ),
            &CRpcReqForwarder::StopBridgeProxy,
            ( IEventSink* )nullptr, dwPortId,
            strSrcUniqName, strSender );

        if( ERROR( ret ) )
            break;

        CRpcRouter* pRouter = GetParent();
        ret = pRouter->AddSeqTask(
            pTask, false );

        if( ERROR( ret ) )
            ( *pTask )( eventCancelTask );

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
    IConfigDb* pResp = nullptr;
    do{
        CCfgOpenerObj oReq( pIoReq );
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

        oParams.CopyProp(
            propRouterPath, pResp );

    }
    else
    {
        ret = SchedToStopBridgeProxy( pReqCtx );
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

    CCfgOpener oReqCtx( pReqCtx );
    do{
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
            pEvt, pReqCtx );

        if( ERROR( ret ) )
            break;

        CCfgOpener oReqCtx2;
        oReqCtx2.CopyProp(
            propRouterPath, pReqCtx );

        ret = pProxy->CheckRouterPath(
            oReqCtx2.GetCfg(), pRespCb );

        if( ERROR( ret ) )
            ( *pRespCb )( eventCancelTask );

        if( ret == STATUS_PENDING )
            pIntercept->
                SetInterceptTask( nullptr );

    }while( 0 );

    if( ret != STATUS_PENDING && !pEvt.IsEmpty() )
    {
        if( ERROR( ret ) )
        {
            SchedToStopBridgeProxy( pReqCtx );
            oReqCtx.Clear();
        }
        oReqCtx[ propReturnValue ] = ret;
        SetResponse( pEvt, oReqCtx.GetCfg() );
    }

    // the callback transferred to other task
    // current task can retire.
    if( ret == STATUS_PENDING )
        ret = 0;

    return ret;
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

        std::string strRtName;
        CCfgOpenerObj oRtCfg( this );
        ret = oRtCfg.GetStrProp(
            propSvrInstName, strRtName );
        if( ERROR( ret ) )
            break;

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

        oParams.SetIntProp(
            propTimeoutSec, 3 );

        pEvtReq = oParams.GetCfg();

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

            ret = IsMidwayPath(
                strUpstm, strPath );

            if( SUCCEEDED( ret ) )
            {
                vecMatches.push_back(
                    itr->first );
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

        // forward to the upstream subscribers
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
            pEvtCtx, pReqCall );

        CReqBuilder oAuthEvt;
        ret = oAuthEvt.Append( pReqCall );
        if( ERROR( ret ) )
            break;

        std::string strRtName;
        CCfgOpenerObj oRtCfg( this );
        ret = oRtCfg.GetStrProp(
            propSvrInstName, strRtName );
        if( ERROR( ret ) )
            break;

        oAuthEvt.SetObjPath(
            DBUS_OBJ_PATH( strRtName,
            OBJNAME_TCP_BRIDGE_AUTH ) );

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
            
            bool bAuth = false;
            if( pIf->GetClsid() ==
                clsid( CRpcTcpBridgeAuthImpl ) )
                bAuth = true;

            CfgPtr pReq = bAuth ?
                oAuthEvt.GetCfg() : pReqCall;

            TaskletPtr pTask;
            ret = DEFER_IFCALLEX_NOSCHED2(
                1, pTask, ObjPtr( pBridge ),
                &CRpcTcpBridge::BroadcastEvent,
                pReq, ( IEventSink* )nullptr );

            if( ERROR( ret ) )
                continue;

            pGrp->AppendTask( pTask );

            FWRDREQS vecReqs;
            ret = pBridge->FindFwrdReqsByPath(
                strPath, vecReqs );
            if( ERROR( ret ) )
                continue;

            ObjVecPtr pvecTasks( true );
            for( auto pTask : vecReqs )
            {
                ( *pvecTasks )().push_back(
                    pTask.first );
            }
            pBridge->CancelInvTasks( pvecTasks );
        }

        if( pGrp->GetTaskCount() > 0 )
        {
            // NOTE: it would be fatal if pending
            // bit is not set for pGrp when the
            // BroadcastEvent could complete
            // immediately and pGrp would ignore
            // the pCallback, and the Router
            // bridge's seq task queue would be
            // jammed for ever.
            pGrp->MarkPending();
            TaskletPtr pGrpTask( pGrp );
            ret = DEFER_CALL( pMgr, this,
                &CRpcServices::RunManagedTask,
                pGrpTask, false );

            if( ERROR( ret ) )
            {
                pGrp->MarkPending( false );
                ( *pGrp )( eventCancelTask );
            }
            else
            {
                ret = STATUS_PENDING;
            }
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
    OnServiceComplete( pReqCtx, pCallback );
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
    ObjPtr& pMapTaskIds,
    bool bForceClear )
{
    if( unlikely( pVecMatches.IsEmpty() ||
        pCallback == nullptr ) )
        return -EINVAL;

    ObjVecPtr pMatches( pVecMatches );
    ObjMapPtr pTaskIdsIn( pMapTaskIds );
    if( unlikely( pMatches.IsEmpty() &&
        pTaskIdsIn.IsEmpty() ) )
        return -EINVAL;

    if( unlikely( ( *pMatches )().size() == 0 &&
        ( *pTaskIdsIn )().size() == 0 ) )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;

    do{
        std::vector< ObjPtr >& vecMatches =
            ( *pMatches )();

        ObjMapPtr pTaskIds( pMapTaskIds );
        std::map< ObjPtr, QwVecPtr >& mapTaskIds =
            ( *pTaskIds )();

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

        pTaskGrp->SetClientNotify( pCallback );

        std::map< guint32, std::vector< ObjPtr > >
            mapIfMatches;

        for( auto pObj : vecMatches )
        {
            MatchPtr pMatch( pObj );
            MatchPtr pRmtMatch;

            CCfgOpener oMatch(
                ( IConfigDb* )pMatch->GetCfg() );

            ret = GetMatchToAdd(
                pMatch, true, pRmtMatch );

            if( ERROR( ret ) )
                continue;

            ret = RemoveRemoteMatch( pRmtMatch );

            if( !bForceClear )
            {
                if( ret == EEXIST ||
                    ERROR( ret ) )
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

            guint32 dwPortId = 0;
            ret = oMatch.GetIntProp(
                propPrxyPortId, dwPortId );
            if( ERROR( ret ) )
                continue;
            
            std::vector< ObjPtr >& vecIfMatches =
                mapIfMatches[ dwPortId ];

            std::string strNode;
            ret = GetNodeName( strPath, strNode );
            if( ERROR( ret ) )
                continue;

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

            std::map< ObjPtr, QwVecPtr >::iterator
                itr = mapTaskIds.find( ObjPtr( pIf ) );

            QwVecPtr pvecTaskIds( true );
            if( itr != mapTaskIds.end() )
            {
                pvecTaskIds = itr->second;
                mapTaskIds.erase( itr );
            }
                
            
            CRpcTcpBridgeProxy* pProxy = pIf;

            TaskletPtr pTask;
            ObjVecPtr pvecIfMat( true );
            ( *pvecIfMat )() = elem.second;

            ret = DEFER_IFCALLEX_NOSCHED2(
                1, pTask, ObjPtr( pProxy ),
                &CRpcTcpBridgeProxy::ClearRemoteEvents,
                ObjPtr( pvecIfMat ),
                ObjPtr( pvecTaskIds ),
                ( IEventSink* )nullptr );

            if( SUCCEEDED( ret ) )
                pTaskGrp->AppendTask( pTask ); \
        }

        ObjVecPtr pvecIfMat( true );
        for( auto elem : mapTaskIds )
        {
            TaskletPtr pTask;
            ret = DEFER_IFCALLEX_NOSCHED2(
                1, pTask, elem.first,
                &CRpcTcpBridgeProxy::ClearRemoteEvents,
                ObjPtr( pvecIfMat ),
                ObjPtr( elem.second ),
                ( IEventSink* )nullptr );

            if( SUCCEEDED( ret ) )
                pTaskGrp->AppendTask( pTask ); \
        }

        ret = 0;
        if( pTaskGrp->GetTaskCount() == 0 )
        {
            pTaskGrp->ClearClientNotify();
            pTaskGrp.Clear();
            break;
        }

        TaskletPtr pTask = ObjPtr( pTaskGrp );
        ret = GetIoMgr()->RescheduleTask( pTask );

        if( ERROR( ret ) )
        {
            ( *pTask )( eventCancelTask );
        }
        else
        {
            ret = STATUS_PENDING;
        }

    }while( 0 );

    if( ERROR( ret ) && pCallback != nullptr )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        SetResponse(
            pCallback, oResp.GetCfg() );
    }

    return ret;
}

gint32 CRpcTcpBridge::OnEnableRemoteEventCompleteMH(
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

        // iRet may have other code than
        // STATUS_SUCCESS
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

        CParamList oReqCtx;
        oReqCtx.SetPointer( propMatchPtr,
            ( CObjBase* )pRmtMatch );
        oReqCtx.Push( ( bool& )bEnable );

        TaskletPtr pTask;
        ret = NEW_PROXY_RESP_HANDLER2( 
            pTask, ObjPtr( this ),
            &CRpcTcpBridge::OnEnableRemoteEventCompleteMH,
            pCallback,
            ( IConfigDb* )oReqCtx.GetCfg() );

        if( ERROR( ret ) )
            break;

        if( bEnable )
        {
            ret = pProxy->EnableRemoteEvent(
                pTask, pRmtMatch );
        }
        else
        {
            ret = pProxy->DisableRemoteEvent(
                pTask, pRmtMatch );
        }

        if( ERROR( ret ) )
            ( *pTask )( eventCancelTask );

    }while( 0 );

    return ret;
}

gint32 CRpcRouterReqFwdr::RemoveLocalMatchByPath(
    const std::string& strPath,
    guint32 dwProxyPortId,
    std::vector< MatchPtr >& vecMatches )
{
    gint32 ret = 0;
    if( strPath.empty() || dwProxyPortId == 0 )
        return -EINVAL;

    do{
        std::map< MatchPtr, gint32 >* plm =
            &m_mapLocMatches;

        CStdRMutex oRouterLock( GetLock() );
        std::map< MatchPtr, gint32 >::iterator
            itr = plm->begin();

        while( itr != plm->end() )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )itr->first );

            guint32 dwVal = 0;
            ret = oMatch.GetIntProp(
                propPrxyPortId, dwVal );
            if( ERROR( ret ) )
            {
                ++itr;
                continue;
            }
            if( dwVal != dwProxyPortId )
            {
                ++itr;
                continue;
            }
            std::string strVal;
            ret = oMatch.GetStrProp(
                propRouterPath, strVal );

            if( ERROR( ret ) )
            {
                ++itr;
                continue;
            }

            ret = IsMidwayPath( strPath, strVal );
            if( SUCCEEDED( ret ) )
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

bool CRpcRouterReqFwdr::IsProxyInUse(
    guint32 dwPortId )
{
    bool bFound = false;
    do{
        CStdRMutex oRouterLock( GetLock() );
        for( auto elem : m_mapLocMatches )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )elem.first );

            guint32 dwVal = 0;
            gint32 ret = oMatch.GetIntProp(
                propPrxyPortId, dwVal );
            if( ERROR( ret ) )
                continue;

            if( dwPortId != dwVal )
                continue;

            bFound = true;
            break;
        }

    }while( 0 );

    return bFound;
}
/**
* @name OnRmtSvrOfflineMH
* @brief Pass on the eventRmtSvrOffline to the
* local client if the router path is equal or
* substring to the one in the match.
* The method returns STATUS_MORE_PROCESS_NEEDED if
* the related bridge proxy needs to be stopped due
* to no usage at the moment. Otherwise, it will
* sends out the eventRmtSvrOffline messages to the
* affected local clients, without shutting down
* the bridge proxy.
* @{ */
/**  @} */

gint32 CRpcRouterReqFwdr::OnRmtSvrOfflineMH(
    IEventSink* pCallback,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    gint32 ret = 0;
    if( pEvtCtx == nullptr )
        return -EINVAL;

    do{
        std::string strPath;
        CCfgOpener oEvtCtx( pEvtCtx );

        guint32 dwPortId = 0;
        ret = oEvtCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        ret = oEvtCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        std::vector< MatchPtr > vecMatches;
        ret = RemoveLocalMatchByPath(
            strPath, dwPortId, vecMatches );

        // no client referring this addr
        if( ERROR( ret ) )
            break;

        if( ret == 0 )
        {
            ret = STATUS_SUCCESS;
            break;
        }

        // test if the proxies are ok to stop
        if( !IsProxyInUse( dwPortId ) )
        {
            ret = STATUS_MORE_PROCESS_NEEDED;
            break;
        }

        // collect the local clients affected
        std::set< std::string > setUniqNames;
        for( auto& elem : vecMatches )
        {
            std::string strVal;
            CCfgOpenerObj oMatch(
                ( CObjBase* )elem );

            ret = oMatch.GetStrProp(
                propSrcUniqName, strVal );
            if( ERROR( ret ) )
                continue;

            setUniqNames.insert( strVal );
        }

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        GetReqFwdr( pIf );
        CRpcReqForwarder* pReqFwdr = pIf;
        if( pReqFwdr == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        // send out the eventRmtSvrOffline
        for( auto& strDest : setUniqNames )
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

            pReqFwdr->BroadcastEvent(
                oParams.GetCfg(), pDummyTask );
        }

    }while( 0 );

    return ret;
}

}
