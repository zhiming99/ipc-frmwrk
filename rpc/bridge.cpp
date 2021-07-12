/*
 * =====================================================================================
 *
 *       Filename:  bridge.cpp
 *
 *    Description:  implementation of CRpcTcpBridge and CRpcTcpBridgeProxy
 *
 *        Version:  1.0
 *        Created:  07/29/2017 09:57:27 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi( woodhead99@gmail.com )
 *   Organization:  
 *
 *      Copyright:  2020 Ming Zhi( woodhead99@gmail.com )
 *
 *        License:  This program is free software; you can redistribute it
 *                  and/or modify it under the terms of the GNU General Public
 *                  License version 3.0 as published by the Free Software
 *                  Foundation at 'http://www.gnu.org/licenses/gpl-3.0.html'
 *
 * =====================================================================================
 */


#include "configdb.h"
#include "defines.h"
#include "rpcroute.h"
#include "dbusport.h"
#include "tcpport.h"
#include "emaphelp.h"
#include "ifhelper.h"
#include "connhelp.h"

namespace rpcf
{

#define BRIDGE_GREETINGS "rpcf-bridge"
#define BRIDGE_PROXY_GREETINGS "rpcf-bridge-proxy"

using namespace std;

CRpcTcpBridgeProxy::CRpcTcpBridgeProxy(
    const IConfigDb* pCfg )
    : CAggInterfaceProxy( pCfg ),
    super( pCfg ),
    CRpcTcpBridgeShared( this )
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
            "Error occurs in CRpcTcpBridgeProxy ctor" );
        throw runtime_error( strMsg );
    }
}

gint32 CRpcTcpBridgeProxy::InitUserFuncs()
{
    BEGIN_IFHANDLER_MAP( CRpcTcpBridge );

    ADD_EVENT_HANDLER(
        CRpcTcpBridgeProxy::OnInvalidStreamId,
        BRIDGE_EVENT_INVALIDSTM );

    ADD_EVENT_HANDLER(
        CRpcTcpBridgeProxy::OnRmtSvrEvent,
        "RmtSvrEvent" );

    ADD_EVENT_HANDLER(
        CRpcTcpBridgeProxy::RefreshReqLimit,
        "RefreshReqLimit" );

    END_IFHANDLER_MAP;

    BEGIN_IFPROXY_MAP( CRpcTcpBridge, false );

    ADD_PROXY_METHOD_EX( 2,
        CRpcTcpBridgeProxy::ClearRemoteEvents,
        SYS_METHOD_CLEARRMTEVTS );

    ADD_PROXY_METHOD_EX( 1,
        CRpcTcpBridgeProxy::CheckRouterPath,
        SYS_METHOD_CHECK_ROUTERPATH );

    ADD_PROXY_METHOD_EX( 1,
        CRpcTcpBridgeProxy::Handshake,
        SYS_METHOD_HANDSAKE );

    END_IFPROXY_MAP;

    return 0;
}

gint32 CRpcTcpBridgeProxy::OnHandshakeComplete(
    IEventSink* pCallback,
    IEventSink*  pIoReq,
    IConfigDb* pReqCtx )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pIoReq == nullptr ||
        pReqCtx == nullptr )
        return -EINVAL;

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

        IConfigDb* pInfo = nullptr;
        ret = oResp.GetPointer( 0, pInfo );
        if( ERROR( ret ) )
            break;

        CCfgOpener oInfo( pInfo );
        std::string strHs;
        ret = oInfo.GetStrProp( 0, strHs );
        if( ERROR( ret ) ||
            strHs != BRIDGE_GREETINGS )
        {
            ret = ERROR_FAIL;
            break;
        }

        DebugPrint( ret,
            "Handshake completed successfully" );

        guint64 qwTimestamp = 0;
        ret = oInfo.GetQwordProp(
            propTimestamp, qwTimestamp );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oIfCfg( this );
        guint32 dwmr = 0, dwmp = 0;
        ret = oInfo.GetIntProp(
            propMaxReqs, dwmr );
        if( SUCCEEDED( ret ) )
        {
            ret = oInfo.GetIntProp(
                propMaxPendings, dwmp );

            if( ERROR( ret ) )
                break;

            guint32 dwMaxReqs = std::min(
                RFC_MAX_REQS, dwmr + dwmp );

            oIfCfg.SetIntProp(
                propMaxReqs, dwMaxReqs );

            oIfCfg.SetIntProp( propMaxPendings,
                RFC_MAX_PENDINGS );

            SetRfcEnabled( true );
            CCfgOpener oCfg;
            oCfg.SetIntProp(
                propMaxReqs, dwMaxReqs );

            oCfg.SetIntProp(
                propMaxPendings, RFC_MAX_PENDINGS );

            ret = InitRfc( oCfg );
            if( ERROR( ret ) )
                break;
        }

        CCfgOpener oReqCtx( pReqCtx );
        guint64 qwOriginTs = 0;
        ret = oReqCtx.GetQwordProp(
            propTimestamp, qwOriginTs );
        if( ERROR( ret ) )
            break;

        timespec ts;
        clock_gettime( CLOCK_MONOTONIC, &ts );
        m_oTs.SetBase(
            ( qwOriginTs + ts.tv_sec ) >> 1 );
        m_oTs.SetPeer( qwTimestamp );

    }while( 0 );

    if( ERROR( ret ) )
        DebugPrintEx( logErr, ret,
            "TcpBridgeProxy Handshake failed" );

    pCallback->OnEvent(
        eventTaskComp, ret, 0, nullptr );

    return ret;
}

gint32 CRpcTcpBridgeProxy::Handshake(
    IConfigDb* pInfo,
    IEventSink* pCallback )
{
    if( pInfo == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CParamList oOptions;
        CParamList oResp;
        EnumClsid iid = iid( CRpcTcpBridge );
        const string& strIfName =
            CoGetIfNameFromIid( iid, "p" );

        if( strIfName.empty() )
        {
            ret = -ENOTSUP;
            break;
        }

        oOptions[ propIfName ] = 
            DBUS_IF_NAME( strIfName );

        oOptions[ propSysMethod ] = ( bool )true;

        ret = AsyncCall( pCallback,
            oOptions.GetCfg(), oResp.GetCfg(),
            __func__, pInfo );

        if( ret == STATUS_PENDING )
            break;
        
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oTask(
            ( CObjBase* )pCallback ); 

        ret = oTask.CopyProp(
            propRespPtr, oResp.GetCfg() );

    }while( 0 );

    if( ERROR( ret ) )
    {
        DebugPrintEx( logErr, ret,
            "TcpBridgeProxy Handshake failed" );
    }
    else if( SUCCEEDED( ret ) )
    {
        DebugPrint( ret,
            "Handshake completed successfully" );
    }
    return ret;
}

gint32 CRpcTcpBridgeProxy::DoStartHandshake(
    IEventSink* pCallback )
{
    CParamList oParams;
    gint32 ret = 0;
    do{
        timespec tv;
        ret = clock_gettime( CLOCK_MONOTONIC, &tv );
        if( ret == -1 )
        {
            ret = -errno;
            break;
        }

        oParams.SetQwordProp(
            propTimestamp, tv.tv_sec );
        oParams.Push( BRIDGE_PROXY_GREETINGS );

        TaskletPtr pRespCb;
        NEW_PROXY_RESP_HANDLER2(
            pRespCb, ObjPtr( this ),
            &CRpcTcpBridgeProxy::OnHandshakeComplete,
            pCallback, oParams.GetCfg() );

        ret = Handshake(
            oParams.GetCfg(), pRespCb );
        if( ERROR( ret ) )
            break;

        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oCfg( pCallback );
        IConfigDb* pResp = nullptr;
        ret = oCfg.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp ); 
        gint32 iRet = 0;
        ret = oResp.GetIntProp(
            propReturnValue,
            *( guint32* )&iRet );
        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        IConfigDb* pInfo = nullptr;
        ret = oResp.GetPointer( 0, pInfo );
        if( ERROR( ret ) )
            break;

        guint64 qwTimestamp = 0;
        CCfgOpener oInfo( pInfo );
        ret = oResp.GetQwordProp(
            propTimestamp, qwTimestamp );
        if( ERROR( ret ) )
            break;

        timespec ts;
        clock_gettime( CLOCK_MONOTONIC, &ts );
        m_oTs.SetBase(
            ( tv.tv_sec + ts.tv_sec ) >> 1 );
        m_oTs.SetPeer( qwTimestamp );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::ClearRemoteEvents(
    ObjPtr& pVecMatches,
    ObjPtr& pVecTaskIds,
    IEventSink* pCallback )
{
    // NOTE: pCallback is not an output parameter
    // from the server.
    //
    // this is an async call
    gint32 ret = 0;

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

    do{
        CParamList oOptions;
        CParamList oResp;
        EnumClsid iid = iid( CRpcTcpBridge );
        const string& strIfName =
            CoGetIfNameFromIid( iid, "p" );

        if( strIfName.empty() )
        {
            ret = -ENOTSUP;
            break;
        }

        oOptions[ propIfName ] = 
            DBUS_IF_NAME( strIfName );

        oOptions[ propSysMethod ] = ( bool )true;

        ret = AsyncCall( pCallback,
            oOptions.GetCfg(), oResp.GetCfg(),
            __func__, pVecMatches, pVecTaskIds );

        if( ret == STATUS_PENDING )
            break;
        
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oTask(
            ( CObjBase* )pCallback ); 

        ret = oTask.SetObjPtr(
            propRespPtr, oResp.GetCfg() );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::EnableRemoteEvent(
    IEventSink* pCallback,
    IMessageMatch* pMatch )
{
    if( pMatch == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    return EnableRemoteEventInternal(
        pCallback, pMatch, true );
}

gint32 CRpcTcpBridgeProxy::DisableRemoteEvent(
    IEventSink* pCallback,
    IMessageMatch* pMatch )
{
    if( pMatch == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    return EnableRemoteEventInternal(
        pCallback, pMatch, false );
}

gint32 CRpcTcpBridgeProxy::EnableRemoteEventInternal(
    IEventSink* pCallback,
    IMessageMatch* pMatch,
    bool bEnable )
{
    gint32 ret = 0;
    do{
        if( pMatch == nullptr ||
            pCallback == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CReqBuilder oBuilder( this );
        oBuilder.Push( ObjPtr( pMatch ) );
        if( bEnable )
        {
            oBuilder.SetMethodName(
                SYS_METHOD_ENABLERMTEVT );
        }
        else
        {
            oBuilder.SetMethodName(
                SYS_METHOD_DISABLERMTEVT );
        }

        EnumClsid iid = iid( CRpcTcpBridge );
        const string& strIfName =
            CoGetIfNameFromIid( iid, "p" );
        if( strIfName.empty() )
        {
            ret = -ENOTSUP;
            break;
        }

        oBuilder[ propIfName ] = 
            DBUS_IF_NAME( strIfName );

        oBuilder[ propStreamId ] =
            TCP_CONN_DEFAULT_STM;

        oBuilder[ propProtoId ] = protoDBusRelay;

        oBuilder.SetCallFlags( CF_WITH_REPLY
           | DBUS_MESSAGE_TYPE_METHOD_CALL 
           | CF_ASYNC_CALL );

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

        ret = RunIoTask( oBuilder.GetCfg(),
            pRespCfg, pCallback );

        if( ret == STATUS_PENDING )
            break;

        if( SUCCEEDED( ret ) )
        {
            CCfgOpener oCfg(
                ( IConfigDb* )pRespCfg );

            gint32 iRet = 0;
            ret = oCfg.GetIntProp(
                propReturnValue,
                ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            ret = iRet;
        }
    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::BuildBufForIrp(
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

        if( strMethod == SYS_METHOD_FORWARDREQ )
        {
            ret = BuildBufForIrpFwrdReq(
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

gint32 CRpcTcpBridgeProxy::BuildBufForIrpFwrdReq(
    BufPtr& pBuf, IConfigDb* pReqCall )
{
    if( pReqCall == nullptr ||
        pBuf.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        DMsgPtr pMsg, pMsgToFwrd;
        CReqOpener oReq( pReqCall );

        IConfigDb* pReqCtx = nullptr;
        ret = oReq.GetPointer( 0, pReqCtx );
        if( ERROR( ret ) )
            break;

        ret = oReq.GetMsgPtr( 1, pMsgToFwrd );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oIfCfg( this );
        string strDest;
        ret = oIfCfg.GetStrProp(
            propDestDBusName, strDest );
        if( ERROR( ret ) )
            break;

        string strObjPath;
        ret = oIfCfg.GetStrProp(
            propObjPath, strObjPath );
        if( ERROR( ret ) )
            break;

        string strRtName;
        ret = oIfCfg.GetStrProp(
            propSvrInstName, strRtName );
        if( ERROR( ret ) )
            break;

        string strSender =
            DBUS_DESTINATION( strRtName );

        string strIfName = DBUS_IF_NAME(
            IFNAME_TCP_BRIDGE );


        ret = pMsg.NewObj();
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetPath( strObjPath );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetInterface( strIfName );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetDestination( strDest );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetSender( strSender );
        if( ERROR( ret ) ) 
            break;

        ret = pMsg.SetMember(
            SYS_METHOD_FORWARDREQ );

        if( ERROR( ret ) )
            break;

        pMsg.SetSerial( 0 );

        BufPtr pReqMsgBuf( true );
        ret = pMsgToFwrd.Serialize( *pReqMsgBuf );
        if( ERROR( ret ) )
            break;

        guint64 qwTaskId;
        ret = oReq.GetTaskId( qwTaskId );
        if( ERROR( ret ) )
            break;

        // NOTE: the task id is for keep-alive
        // purpose
        CCfgOpener oReqCtx( pReqCtx );
        guint32 dwPortId = 0;
        bool bExist = false;
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( SUCCEEDED( ret ) )
            bExist = true;

        oReqCtx.RemoveProperty( propConnHandle );

        BufPtr pCtxBuf( true );
        ret = pReqCtx->Serialize( *pCtxBuf );
        if( ERROR( ret ) )
            break;

        if( bExist )
        {
            oReqCtx.SetIntProp(
                propConnHandle, dwPortId );
        }

        const char* pCtx = pCtxBuf->ptr();
        const char* pData = pReqMsgBuf->ptr();
        if( !dbus_message_append_args( pMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pReqMsgBuf->size(),
            DBUS_TYPE_UINT64, &qwTaskId,
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }
        *pBuf = pMsg;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::ForwardRequest(
    IConfigDb* pReqCtx,
    DBusMessage* pReqMsg,           // [ in ]
    DMsgPtr& pRespMsg,              // [ out ]
    IEventSink* pCallback )
{
    if( pReqMsg == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    if( pReqCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CReqBuilder oBuilder( this );
        CCfgOpener oOrigCtx( pReqCtx );
        CCfgOpener oReqCtx;
        std::string strPath;
        ret = oOrigCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        oReqCtx.SetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        bool bRelay = false;

        CRpcRouter* pRouter = GetParent();
        if( unlikely( pRouter == nullptr ) )
            continue;

        if( pRouter->HasBridge() )
            bRelay = true;

        oReqCtx.CopyProp(
            propTaskId, pReqCtx );

        guint32 dwAge = 0;
        if( !bRelay )
        {
            oReqCtx.SetQwordProp(
                propTimestamp,
                m_oTs.GetPeerTimestamp() );
            oReqCtx.SetStrProp(
                propPath2, strPath );
        }
        else
        {
            oReqCtx.CopyProp(
                propSessHash, pReqCtx );

            guint64 qwLocalTs = 0;
            ret = oOrigCtx.GetQwordProp(
                propTimestamp, qwLocalTs );
            if( ERROR( ret ) )
                break;

            guint64 qwPeerTs =
                m_oTs.GetPeerTimestamp( qwLocalTs );

            oReqCtx.SetQwordProp(
                propTimestamp, qwPeerTs );

            guint64 qwVal =
                m_oTs.GetAgeSec( qwLocalTs );
            qwVal = ( abs( ( gint64 )qwVal ) );
            dwAge = qwVal; 

            oReqCtx.CopyProp(
                propPath2, pReqCtx );
        }


        // just to conform to the rule
        oBuilder.Push( ( IConfigDb* )
            oReqCtx.GetCfg() );

        oBuilder.Push( DMsgPtr( pReqMsg ) );
        oBuilder.CopyProp( propPortId,
            propConnHandle, pReqCtx );

        oBuilder.SetMethodName(
            SYS_METHOD_FORWARDREQ );

        guint32 dwFlags = 0;
        bool bResp = true;

        guint32 dwtos =
            IFSTATE_DEFAULT_IOREQ_TIMEOUT;

        IConfigDb* pReqPtr = nullptr;
        ret = oOrigCtx.GetPointer(
            propReqPtr, pReqPtr );
        if( SUCCEEDED( ret ) )
        {
            CReqOpener oOrigReq( pReqPtr );
            ret = oOrigReq.GetCallFlags(
                dwFlags );
            if( ERROR( ret ) )
            {
                dwFlags = CF_WITH_REPLY
                   | DBUS_MESSAGE_TYPE_METHOD_CALL 
                   | CF_ASYNC_CALL;
                ret = 0;
            }
            else
            {
                if( !( dwFlags & CF_WITH_REPLY ) )
                {
                    bResp = false;
                    oReqCtx[ propNoReply ] = true;
                }
            }

            gint32 iRet =
                oOrigReq.GetTimeoutSec( dwtos );

            if( ERROR( iRet ) )
            {
                CCfgOpenerObj oCfg( this );
                oCfg.GetIntProp(
                    propTimeoutSec, dwtos );
            }

            if( bRelay )
            {
                if( dwAge >= dwtos )
                {
                    ret = -ETIMEDOUT;
                    break;
                }

                dwtos -= dwAge;
            }
        }

        oBuilder.SetCallFlags( dwFlags );

        // a tcp default round trip time
        oBuilder.SetTimeoutSec( dwtos ); 

        oBuilder[ propStreamId ] =
            TCP_CONN_DEFAULT_STM;

        oBuilder[ propProtoId ] = protoDBusRelay;

        CParamList oRespCfg;
        CfgPtr pRespCfg = oRespCfg.GetCfg();
        if( pRespCfg.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        if( bRelay )
        {
            // copy the bridge id for canceling
            // purpose when the bridge is down.
            oBuilder.CopyProp( propPortId,
                propConnHandle, pReqCtx );
        }

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

gint32 CRpcTcpBridgeProxy::FillRespDataFwrdReq(
    IRP* pIrp, CfgPtr& pResp )
{
    gint32 ret = 0;
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pResp.IsEmpty() )
        return -EINVAL;

    do{
        IrpCtxPtr& pCtx = pIrp->GetTopStack();
        if( pCtx->GetMajorCmd() != IRP_MJ_FUNC ||
            pCtx->GetMinorCmd() != IRP_MN_IOCTL )
        {
            ret = -ENOTSUP;
            break;
        }

        CParamList oParams( ( IConfigDb* )pResp );

        DMsgPtr pFwrdMsg;
        if( pCtx->m_pRespData.IsEmpty() ||
            pCtx->m_pRespData->empty() )
        {
            ret = -EBADMSG;
            break;
        }
        pFwrdMsg = ( DMsgPtr& )*pCtx->m_pRespData;

        gint32 iRet;
        ret = pFwrdMsg.GetIntArgAt( 0, ( guint32& )iRet );
        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        DMsgPtr pRespMsg;
        ret = pFwrdMsg.GetMsgArgAt( 1, pRespMsg );
        if( ERROR( ret ) )
            break;

        oParams.SetIntProp( propReturnValue, iRet );
        ret = oParams.Push( pRespMsg );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::FillRespData(
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

    guint32 dwCtrlCode = pCtx->GetCtrlCode();

    CParamList oParams( ( IConfigDb* )pResp );
    oParams.SetIntProp( propReturnValue,
        pIrp->GetStatus() );

    switch( dwCtrlCode )
    {
    case CTRLCODE_OPEN_STREAM_PDO:
        {
            CfgPtr pCfg;
            ret = pCtx->GetRespAsCfg( pCfg );
            if( ERROR( ret ) )
                break;

            ret = oParams.CopyProp(
                propReturnValue, pCfg );

            if( ERROR( ret ) )
                break;

            ret = oParams.CopyParams( pCfg );
            break;
        }
    case CTRLCODE_CLOSE_STREAM_PDO:
    case CTRLCODE_INVALID_STREAM_ID_PDO:
        {
            CfgPtr pCfg;
            ret = pCtx->GetRespAsCfg( pCfg );
            if( ERROR( ret ) )
                break;

            ret = oParams.CopyProp(
                propReturnValue, pCfg );

            if( ERROR( ret ) )
                break;

            break;
        }
    case CTRLCODE_SEND_REQ:
        {
            BufPtr pBuf = pCtx->m_pReqData;
            if( !pBuf.IsEmpty() &&
                pBuf->GetExDataType() == typeDMsg )
            {
                DMsgPtr& pMsg = *pBuf;

                if( pMsg.GetMember()
                    == SYS_METHOD_FORWARDREQ )
                {
                    ret = FillRespDataFwrdReq(
                        pIrp, pResp );
                    if( ERROR( ret ) )
                    {
                        DebugPrint( ret,
                            "FillRespDataFwrdReq failed" );
                    }
                    break;
                }
            }
            // fall through
        }
    default:
        {
            // SEND_DATA and FETCH_DATA will go here
            ret = super::FillRespData(
                pIrp, pResp );
            break;
        }
    }
    return ret;
}

gint32 CRpcTcpBridgeProxy::ForwardEvent(
    IConfigDb* pEvtCtx,
    DBusMessage* pEvtMsg,
    IEventSink* pCallback )
{
    if( pEvtMsg == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        InterfPtr pIf;
        CRpcRouter* pRouter = GetParent();
        if( unlikely( pRouter == nullptr ) )
            continue;

        CCfgOpener oEvtCtx( pEvtCtx );
        if( pRouter->HasReqForwarder() )
        {
            // forward event to local proxies
            CRpcRouterReqFwdr* pRtReqFwdr =
                static_cast< CRpcRouterReqFwdr* >
                    ( pRouter );

            pRtReqFwdr->GetReqFwdr( pIf );
            CRpcReqForwarder* pReqFwdr = pIf;
            if( pReqFwdr == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            ret = oEvtCtx.CopyProp(
                propConnHandle, propPortId, this );
            if( ERROR( ret ) )
                break;

            ret = pReqFwdr->ForwardEvent(
                pEvtCtx, pEvtMsg, pCallback );

            break;
        }

        if( !pRouter->HasBridge() )
            break;

        // dispatch the event to the remote
        // listeners
        TaskGrpPtr pTaskGrp;
        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );
        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        pTaskGrp->SetClientNotify( pCallback );
        pTaskGrp->SetRelation( logicNONE );

        CRpcRouterBridge* pRtBdge =
            static_cast< CRpcRouterBridge* >
                ( pRouter );

        std::string strPath;
        ret = oEvtCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        if( strPath.empty() ||
            strPath[ 0 ] != '/' )
        {
            ret = -ENOTDIR;
            break;
        }

        CCfgOpener oEvtCtx2;
        oEvtCtx2.CopyProp(
            propConnHandle, propPortId, this );

        string strNodeName;
        CCfgOpenerObj oIfCfg( this );
        ret = oIfCfg.GetStrProp(
            propNodeName, strNodeName );
        if( ERROR( ret ) )
            break;

        // prepend the node name to the router
        if( strPath != "/" )
        {
            strPath = string( "/" ) +
                strNodeName + strPath;
        }
        else
        {
            strPath += strNodeName;
        }

        // update the strPath
        oEvtCtx2.SetStrProp(
            propRouterPath, strPath );

        oEvtCtx.SetStrProp(
            propRouterPath, strPath );

        DMsgPtr fwdrMsg( pEvtMsg );
        std::set< guint32 > setPortIds;
        ret = pRtBdge->CheckEvtToFwrd(
            oEvtCtx2.GetCfg(), fwdrMsg,
            setPortIds );

        if( ERROR( ret ) )
            break;

        bool bSingleBdge =
            ( setPortIds.size() == 1 );

        for( auto elem : setPortIds )
        {
            InterfPtr pIf;
            ret = pRtBdge->GetBridge( elem, pIf );
            if( ERROR( ret ) )
                continue;

            CRpcTcpBridge* pBdge = pIf;
            if( pBdge == nullptr )
                continue;

            if( bSingleBdge )
            {
                ret = pBdge->ForwardEvent(
                    oEvtCtx2.GetCfg(), 
                    pEvtMsg, pCallback );
                break;
            }

            TaskletPtr pFwrdEvt;
            ret = DEFER_IFCALLEX_NOSCHED2(
                2, pFwrdEvt, ObjPtr( pBdge ),
                &CRpcTcpBridge::ForwardEvent,
                pEvtCtx, pEvtMsg,
                ( IEventSink* )0 );

            if( SUCCEEDED( ret ) )
            {
                ret = pTaskGrp->AppendTask(
                    pFwrdEvt );
            }
            else
            {
                DebugPrint( ret,
                    "CRpcReqForwarder's "
                    "ForwardEvent failed" );
            }
        }

        if( ERROR( ret ) )
            break;

        if( pTaskGrp->GetTaskCount() == 0 )
            break;

        TaskletPtr pTask = ObjPtr( pTaskGrp );
        pTask->MarkPending();
        ret = DEFER_CALL( GetIoMgr(), this,
            &CRpcServices::RunManagedTask,
            pTask, false );

        if( ERROR( ret ) )
        {
            pTaskGrp->MarkPending( false );
            ( *pTaskGrp )( eventCancelTask );
        }
        else if( SUCCEEDED( ret ) )
        {
            ret = STATUS_PENDING;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::DoInvoke(
    DBusMessage* pEvtMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;

    do{
        if( pEvtMsg == nullptr ||
            pCallback == nullptr )
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
            ret = OnKeepAlive( pCallback, KARelay );
            return ret;
        }
        else if( strMethod == SYS_EVENT_FORWARDEVT )
        {
            ObjPtr pObj;
            ret = pMsg.GetObjArgAt( 0, pObj );
            if( ERROR( ret ) )
                break;

            IConfigDb* pEvtCtx = pObj;

            DMsgPtr pUserEvtMsg;
            ret = pMsg.GetMsgArgAt(
                1, pUserEvtMsg );

            if( ERROR( ret ) )
                break;

            ret = ForwardEvent( pEvtCtx,
                pUserEvtMsg, pCallback );
        }
        else
        {
            ret = super::DoInvoke(
                pEvtMsg, pCallback );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::SetupReqIrp(
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

        ObjPtr pPortObj( GetPort() );
        if( unlikely( pPortObj.IsEmpty() ) )
        {
            ret = -EFAULT;
            break;
        }

        CPort* pPort = pPortObj;
        pPort = static_cast< CPort* >
            ( pPort->GetTopmostPort() );

        if( strMethod == IF_METHOD_LISTENING )
        {
            ret = SetupReqIrpListening(
                 pIrp, pReqCall, pCallback ); 
            break;
        }
        else if( strMethod == SYS_METHOD_ENABLERMTEVT ||
            strMethod == SYS_METHOD_DISABLERMTEVT )
        {
            IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
            pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
            pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
            pIrpCtx->SetCtrlCode( CTRLCODE_SEND_REQ );

            pPort->AllocIrpCtxExt( pIrpCtx );
            SetBdgeIrpStmId( pIrp,
                ( guint32& )oReq[ propStreamId ] );

            guint32 dwIoDir = IRP_DIR_OUT;
            if( oReq.HasReply() )
                dwIoDir = IRP_DIR_INOUT;

            pIrpCtx->SetIoDirection( dwIoDir ); 

            BufPtr pBuf( true );

            ret = BuildBufForIrp( pBuf, pReqCall );
            if( ERROR( ret ) )
                break;

            pIrpCtx->SetReqData( pBuf );
            pIrp->SetCallback( pCallback, 0 );
            pIrp->SetIrpThread( GetIoMgr() );

            guint32 dwTimeoutSec = 0;
            ret = oReq.GetTimeoutSec( dwTimeoutSec );

            if( SUCCEEDED( ret ) &&
                dwTimeoutSec != 0 )
            {
                pIrp->SetTimer(
                    dwTimeoutSec, GetIoMgr() );
            }
        }
        else if( strMethod == SYS_METHOD_CLEARRMTEVTS ||
            strMethod == SYS_METHOD_CHECK_ROUTERPATH )
        {
            IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
            pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
            pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
            pIrpCtx->SetCtrlCode( CTRLCODE_SEND_REQ );

            pPort->AllocIrpCtxExt( pIrpCtx );
            SetBdgeIrpStmId( pIrp, TCP_CONN_DEFAULT_STM );
            pIrpCtx->SetIoDirection( IRP_DIR_INOUT ); 

            BufPtr pBuf( true );
            ret = BuildBufForIrp( pBuf, pReqCall );
            if( ERROR( ret ) )
                break;

            pIrpCtx->SetReqData( pBuf );
            pIrp->SetCallback( pCallback, 0 );
            pIrp->SetIrpThread( GetIoMgr() );

            guint32 dwTimeoutSec = 0;
            ret = oReq.GetTimeoutSec( dwTimeoutSec );

            if( SUCCEEDED( ret ) &&
                dwTimeoutSec != 0 )
            {
                pIrp->SetTimer(
                    dwTimeoutSec, GetIoMgr() );
            }
        }
        else if( strMethod == BRIDGE_METHOD_OPENSTM ||
            strMethod == BRIDGE_METHOD_CLOSESTM )
        {
            guint32 dwCmdId = 0;
            ret = oReq.GetIntProp( propCmdId, dwCmdId );
            if( ERROR( ret ) )
                break;

            IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
            pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
            pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
            pIrpCtx->SetCtrlCode( dwCmdId );

            guint32 dwIoDir = IRP_DIR_OUT;
            if( oReq.HasReply() )
                dwIoDir = IRP_DIR_INOUT;

            pIrpCtx->SetIoDirection( dwIoDir ); 

            BufPtr pBuf( true );
            *pBuf = ( ObjPtr& )oReq.GetCfg();

            if( ERROR( ret ) )
                break;

            pIrpCtx->SetReqData( pBuf );
            pIrp->SetCallback( pCallback, 0 );
            pIrp->SetIrpThread( GetIoMgr() );

            guint32 dwTimeoutSec = 0;
            ret = oReq.GetTimeoutSec( dwTimeoutSec );

            if( SUCCEEDED( ret ) &&
                dwTimeoutSec != 0 )
            {
                pIrp->SetTimer(
                    dwTimeoutSec, GetIoMgr() );
            }
        }
        else if( strMethod == SYS_METHOD_FETCHDATA ||
            strMethod == SYS_METHOD_SENDDATA )
        {
            ret = super::SetupReqIrp( pIrp,
                pReqCall, pCallback );

            IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
            pPort->AllocIrpCtxExt( pIrpCtx );
            SetBdgeIrpStmId( pIrp,
                TCP_CONN_DEFAULT_STM );

            break;
        }
        else
        {
            ret = -ENOTSUP;
        }

    }while( 0 );

    if( ret == -ENOTSUP )
    {
        ret = super::SetupReqIrp( pIrp,
            pReqCall, pCallback );

        if( SUCCEEDED( ret ) )
        {
            IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
            ObjPtr pPortObj = GetPort();
            CPort* pPort = pPortObj;
            if( pPort == nullptr )
                return -EFAULT;

            pPort->AllocIrpCtxExt( pIrpCtx );
            SetBdgeIrpStmId( pIrp,
                TCP_CONN_DEFAULT_STM );
        }
    }

    return ret;
}

gint32 CRpcTcpBridgeProxy::OpenStream_Proxy(
    guint32 wProtocol,
    gint32& iNewStm,
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        CReqBuilder oBuilder( this );

        oBuilder.SetCallFlags( CF_WITH_REPLY |
            CF_NON_DBUS |
            DBUS_MESSAGE_TYPE_METHOD_CALL |
            CF_ASYNC_CALL );

        // a tcp default round trip time
        oBuilder.SetTimeoutSec(
            IFSTATE_DEFAULT_IOREQ_TIMEOUT ); 

        oBuilder.SetKeepAliveSec(
            IFSTATE_DEFAULT_IOREQ_TIMEOUT / 2 ); 

        oBuilder.SetMethodName(
            BRIDGE_METHOD_OPENSTM );

        string strIfName = DBUS_IF_NAME(
            IFNAME_TCP_BRIDGE );

        oBuilder.SetIfName( strIfName );

        oBuilder.SetIntProp(
            propStreamId, TCP_CONN_DEFAULT_CMD );

        // command stream to transfer this request
        oBuilder[ propProtoId ] = protoControl;

        oBuilder.SetIntProp( propCmdId,
            CTRLCODE_OPEN_STREAM_PDO );

        oBuilder.SetBoolProp(
            propSubmitPdo, true );

        // protocol for the new stream
        oBuilder.Push( ( guint32 )wProtocol );

        CfgPtr pRespCfg( true );

        ret = RunIoTask( oBuilder.GetCfg(),
            pRespCfg, pCallback );

        if( ret == STATUS_PENDING )
            break;

        if( SUCCEEDED( ret ) )
        {
            CCfgOpener oCfg(
                ( IConfigDb* )pRespCfg );

            gint32 iRet = 0;
            ret = oCfg.GetIntProp(
                propReturnValue,
                ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            ret = oCfg.GetIntProp(
                0, ( guint32& )iNewStm );

            if( ERROR( ret ) )
                break;

            CIfParallelTask* pTask =
                ObjPtr( pCallback );

            if( pTask != nullptr )
                pTask->SetRespData( pRespCfg );

            ret = iRet;
        }
    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::CloseStream_Proxy(
    gint32 iStreamId,
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        CReqBuilder oBuilder( this );

        oBuilder.SetCallFlags( CF_WITH_REPLY |
            CF_NON_DBUS |
            DBUS_MESSAGE_TYPE_METHOD_CALL  |
            CF_ASYNC_CALL );

        // a tcp default round trip time
        oBuilder.SetTimeoutSec(
            IFSTATE_DEFAULT_IOREQ_TIMEOUT ); 

        oBuilder.SetKeepAliveSec(
            IFSTATE_DEFAULT_IOREQ_TIMEOUT / 2 ); 

        oBuilder.SetMethodName(
            BRIDGE_METHOD_CLOSESTM );

        string strIfName = DBUS_IF_NAME(
            IFNAME_TCP_BRIDGE );

        oBuilder.SetIfName( strIfName );

        oBuilder.Push( ( guint32 )iStreamId );

        oBuilder.SetIntProp( propCmdId,
            CTRLCODE_CLOSE_STREAM_PDO );

        oBuilder.SetIntProp(
            propStreamId, TCP_CONN_DEFAULT_CMD );

        oBuilder[ propProtoId ] = protoControl;

        oBuilder.SetIntProp( propCmdId,
            CTRLCODE_CLOSE_STREAM_PDO );

        CfgPtr pRespCfg( true );
        ret = RunIoTask( oBuilder.GetCfg(),
            pRespCfg, pCallback );

        if( ret == STATUS_PENDING )
            break;

        if( SUCCEEDED( ret ) )
        {
            CCfgOpener oCfg(
                ( IConfigDb* )pRespCfg );

            gint32 iRet = 0;
            ret = oCfg.GetIntProp(
                propReturnValue,
                ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            CIfParallelTask* pTask =
                ObjPtr( pCallback );

            if( pTask != nullptr )
            {
                pTask->SetRespData( pRespCfg );
            }
            ret = iRet;
        }
    }while( 0 );

    return ret;
}

gint32 CBdgeProxyReadWriteComplete::OnIrpComplete(
    PIRP pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = super::OnIrpComplete( pIrp );
    if( ret == STATUS_MORE_PROCESS_NEEDED ||
        ret == STATUS_PENDING )
        return ret;

    do{
        CCfgOpenerObj oCfg( this );

        EventPtr pCallback;
        ret = GetInterceptTask( pCallback );
        if( ERROR( ret ) )
            break;
        
        guint32 dwCmdId = 0;
        ret = oCfg.GetIntProp(
            propCmdId, dwCmdId );

        if( ERROR( ret ) )
            break;

        if( dwCmdId == IRP_MN_READ )
        {
            pCallback->OnEvent( eventIrpComp,
                ( LONGWORD )pIrp, 0, 0 );
        }
        else if( dwCmdId == IRP_MN_WRITE )
        {
            pCallback->OnEvent( eventIrpComp,
                ( LONGWORD )pIrp, 0, 0 );
        }

        ClearClientNotify();

    }while( 0 );

    return ret;
}

#define GET_TARGET_PORT_SHARED( pPort ) \
do{ \
        CCfgOpener oReq; \
        bool _bPdo = false; \
        oReq.SetBoolProp( propSubmitPdo, true ); \
        ret = m_pParentIf->GetPortToSubmit( \
            oReq.GetCfg(), pPort, _bPdo ); \
}while( 0 ) 

gint32 CRpcTcpBridge::OnPostStart(
    IEventSink* pContext )
{
    gint32 ret = 0;
    do{
        MatchPtr pMatch;
        for( auto elem : m_vecMatches )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )elem ); 

            guint32 iid = clsid( Invalid );
            ret = oMatch.GetIntProp(
                propIid, ( guint32& )iid );

            if( ERROR( ret ) )
                continue;

            if( iid == iid( CRpcTcpBridge ) )
            {
                pMatch = elem;
                break;
            }
        }

        if( pMatch.IsEmpty() )
        {
            ret = -ENOENT;
            break;
        }

        OnPostStartShared(
            pContext, pMatch );

        this->m_pConnMgr.Clear();

        GenSessHash( m_strSess );
        

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::OnPostStart(
    IEventSink* pContext )
{
    gint32 ret = 0;
    do{
        MatchPtr pMatch;

        for( auto elem : m_vecMatches )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )elem ); 

            guint32 iid = clsid( Invalid );
            ret = oMatch.GetIntProp(
                propIid, iid );
            if( ERROR( ret ) )
                continue;

            if( iid == iid( CRpcTcpBridge ) )
            {
                pMatch = elem;
                break;
            }
        }

        if( pMatch.IsEmpty() )
        {
            ret = -ENOENT;
            break;
        }

        OnPostStartShared(
            pContext, pMatch );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::InitRfc(
    CCfgOpener& oParams )
{
    gint32 ret = 0;
    do{
        oParams[ propIfPtr ] =
            ObjPtr( m_pParentIf );

        ret = m_pGrpRfc.NewObj(
            clsid( CIfParallelTaskGrpRfc ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        // a placeholder task to prevent
        // m_pGrpRfc from completing itself
        TaskletPtr pPHTask;
        ret = pPHTask.NewObj(
            clsid( CIfCallbackInterceptor ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        CIfParallelTaskGrpRfc* pGrp = m_pGrpRfc;
        if( pGrp == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        pGrp->AppendTask( pPHTask );
        TaskletPtr pTask = ObjPtr( pGrp );
        ret = m_pParentIf->AddAndRun( pTask );
        if( ret == STATUS_PENDING )
            ret = 0;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::OnPostStartShared(
    IEventSink* pContext, MatchPtr& pMatch )
{
    // add to listen on the command stream
    gint32 ret = 0;
    if( pMatch.IsEmpty() )
        return -EINVAL;

    do{

        ret = RegMatchCtrlStream( 
            TCP_CONN_DEFAULT_CMD,
            pMatch, true );

        if( ERROR( ret ) )
            break;

        CParamList oExtInfo;
        oExtInfo.SetStrProp( propMethodName,
            IF_METHOD_LISTENING );

        oExtInfo.SetIntProp( propStreamId,
            TCP_CONN_DEFAULT_CMD );

        oExtInfo.SetIntProp( propProtoId,
            protoControl );

        oExtInfo.SetBoolProp(
            propSubmitPdo, true );

        CParamList osrm;
        osrm.SetPointer(
            propIfPtr, m_pParentIf );

        osrm.SetObjPtr(
            propMatchPtr, ObjPtr( pMatch ) );

        osrm.SetPointer( propExtInfo,
            ( IConfigDb* )oExtInfo.GetCfg() );

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CIfStartRecvMsgTask ),
            osrm.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = m_pParentIf->
            RunManagedTask( pTask );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxy::OnRmtSvrEvent(
    IEventSink* pCallback,
    IConfigDb* pEvtCtx,
    guint32 dwEventId )
{
    if( pEvtCtx == nullptr )
        return -EINVAL;

    if( dwEventId != eventRmtSvrOffline &&
        dwEventId != eventRmtSvrOnline )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oEvtCtx;

        ret = oEvtCtx.CopyProp(
            propRouterPath, pEvtCtx );

        std::string strNode;
        ret = oEvtCtx.CopyProp(
            propConnHandle, propPortId, this );
        if( ERROR( ret ) )
            break;

        CRpcRouter* pRouter = GetParent();
        if( pRouter->HasBridge() )
        {
            ret = oEvtCtx.CopyProp(
                propNodeName, this );
            if( ERROR( ret ) )
                break;
        }

        pEvtCtx = oEvtCtx.GetCfg();
        HANDLE hPort = GetPortHandle();
        CIoManager* pMgr = GetIoMgr();

        CConnPointHelper oConnPoint( pMgr );
        oConnPoint.BroadcastEvent(
            propRmtSvrEvent,
            ( EnumEventId )dwEventId,
            ( LONGWORD )pEvtCtx, 0,
            ( LONGWORD* )hPort );

    }while( 0 );

    return ret;
}
    
gint32 CRpcTcpBridgeProxy::OnInvalidStreamId(
    IEventSink* pCallback,
    gint32 iPeerStreamId,
    gint32 iStreamId,
    guint32 dwSeqNo,
    guint32 dwProtocol )
{
    if( pCallback == nullptr )
        return -EINVAL;

    return 0;
}

gint32 CRpcTcpBridgeProxy::OnPostStop(
    IEventSink* pCallback )
{
    if( IsRfcEnabled() )
        m_pGrpRfc.Clear();

    return 0;
}

gint32 CRpcTcpBridgeProxy::AddAndRun(
    TaskletPtr& pTask, bool bImmediate )
{
    do{
        gint32 ret = 0;

        if( !IsRfcEnabled() )
            break;

        CIfIoReqTask* pIoReq = pTask;
        if( pIoReq == nullptr )
            break;

        CfgPtr pReq;
        ret = pIoReq->GetReqCall( pReq );
        if( ERROR( ret ) )
            break;

        std::string strMethod;
        CReqOpener oReq( pReq );
        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        if( strMethod != SYS_METHOD_FORWARDREQ )
            break;

        CIfParallelTaskGrpRfc* pGrp = m_pGrpRfc;

        ret = pGrp->AddAndRun( pTask );
        if( ret == ERROR_QUEUE_FULL )
        {
            pTask->OnEvent( eventTaskComp,
                ret, 0, nullptr );
        }
        return ret;

    }while( 0 );

    return super::AddAndRun( pTask, bImmediate );
}

gint32 CRpcTcpBridgeProxy::RefreshReqLimit(
    IEventSink* pCallback,
    guint32 dwmr, guint32 dwmp )
{
    if( !IsRfcEnabled() )
        return 0;

    CIfParallelTaskGrpRfc* pGrpRfc =
        m_pGrpRfc;

    if( m_pGrpRfc.IsEmpty() )
        return 0;

    guint32 dwMaxReqs = std::min(
        RFC_MAX_REQS, dwmr + dwmp );

    pGrpRfc->SetLimit(
        dwMaxReqs, RFC_MAX_PENDINGS );

    return 0;
}

gint32 CRpcTcpBridgeProxy::RequeueTask(
    TaskletPtr& pTask )
{
    if( !IsRfcEnabled() )
        return ERROR_STATE;

    CIfParallelTaskGrpRfc* pGrpRfc =
        m_pGrpRfc;

    if( pGrpRfc != nullptr )
        return pGrpRfc->InsertTask( pTask );

    return -EFAULT;
}

bool CRpcInterfaceProxy::IsConnected(
    const char* szAddr )
{
    if( !super::IsConnected( szAddr ) )
        return false;
    return  m_pParent->IsConnected( szAddr );
}

gint32 CRpcInterfaceProxy::OnKeepAlive(
    IEventSink* pTask, EnumKAPhase iPhase )
{
    if( pTask == nullptr )
        return -EINVAL;

    if( iPhase == KAOrigin )
    {
        return OnKeepAliveOrig( pTask );
    }
    else if( iPhase == KARelay )
    {
        return OnKeepAliveRelay( pTask );
    }

    return -EINVAL;
}

gint32 CRpcInterfaceProxy::OnKeepAliveOrig(
    IEventSink* pTask )
{
    gint32 ret = 0;

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
            // NOTE: this is not a serious state
            // check
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
        guint64 iTaskId = 0;
        ret = oEvent.GetQwordProp( 0, iTaskId );
        if( ERROR( ret ) )
            break;

        TaskletPtr pTaskToKa;
        if( true )
        {
            TaskGrpPtr pTaskGrp = GetTaskGroup();
            if( pTaskGrp.IsEmpty() )
            {
                ret = -ENOENT;    
                break;
            }
            ret = pTaskGrp->FindTask(
                iTaskId, pTaskToKa );

            if( ERROR( ret ) )
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

    }while( 0 );

    // whether or not succeeded, we complete the
    // invoke task.
    return 0;
}

gint32 CRpcInterfaceProxy::OnKeepAliveRelay(
    IEventSink* pTask )
{
    return OnKeepAliveOrig( pTask );    
}

//------------------------------
//CRpcTcpBridge methods
//
CRpcTcpBridge::CRpcTcpBridge(
    const IConfigDb* pCfg )
    : CAggInterfaceServer( pCfg ), super( pCfg ),
    CRpcTcpBridgeShared( this )
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
            "Error occurs in CRpcTcpBridge ctor" );
        throw runtime_error( strMsg );
    }
}

CRpcTcpBridge::~CRpcTcpBridge()
{
}

gint32 CRpcTcpBridge::FindFwrdReqsAll(
    FWRDREQS& vecTasks, bool bTaskId )
{
    gint32 ret = 0;
    do{
        std::vector< TaskletPtr > vecInvTasks;
        TaskGrpPtr pGrp;
        if( IsRfcEnabled() )
        {
            pGrp = m_pGrpRfc;
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
                continue;

            if( pMsg.GetMember() != 
                SYS_METHOD_FORWARDREQ )
                continue;

            guint64 qwTid = 0;
            if( bTaskId )
            {
                ret = RetrieveTaskId( elem, qwTid );
                if( ERROR( ret ) )
                    continue;
            }
            vecTasks.push_back(
                { elem , qwTid } );
        }

        ret = 0;
        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::FindFwrdReqsByPrxyPortId(
    guint32 dwPrxyPortId,
    FWRDREQS& vecTasks )
{
    gint32 ret = 0;
    do{
        FWRDREQS vecTasksAll;
        ret = FindFwrdReqsAll(
            vecTasksAll, false );
        if( ERROR( ret ) )
            break;
        
        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( GetParent() );

        for( auto elem : vecTasksAll )
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

            stdstr strPath, strNode;
            ret = oReqCtx.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
                continue;

            ret = pRouter->GetNodeName(
                strPath, strNode );
            if( ERROR( ret ) )
                continue;

            guint32 dwPortId = 0;
            ret = pRouter->GetProxyIdByNodeName(
                strNode, dwPortId );
            if( ERROR( ret ) )
                continue;

            if( dwPortId != dwPrxyPortId )
                continue;

            vecTasks.push_back( elem );
        }

        ret = 0;
        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    if( ERROR( ret ) )
        vecTasks.clear();

    return ret;
}

gint32 CRpcInterfaceServer::RetrieveTaskId(
    IEventSink* pCallback,
    guint64& qwTaskId ) const
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CIfInvokeMethodTask* pInv =
        static_cast< CIfInvokeMethodTask* >
            ( pCallback );
    do{
        EnumTaskState iState =
            pInv->GetTaskState();
        if( iState == stateStarted )
        {
            CCfgOpenerObj oInv( pInv );
            IConfigDb* pReq = nullptr;
            ret = oInv.GetPointer(
                propReqPtr, pReq );
            if( ERROR( ret ) )
                break;

            CCfgOpener oReq( pReq );
            ret = oReq.GetQwordProp(
                propTaskId, qwTaskId );
            break;
        }

        if( iState != stateStarting )
        {
            ret = ERROR_STATE;
            break;
        }
        
        DMsgPtr pMsg;
        CCfgOpenerObj oInv( pInv );
        ret = oInv.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        CRpcRouter* pRouter = GetParent();
        if( pRouter->HasBridge() )
            ret = pMsg.GetObjArgAt( 0, pObj );
        else
        {
            DMsgPtr pCliMsg;
            ret = pMsg.GetMsgArgAt( 1, pCliMsg );
            if( SUCCEEDED( ret ) )
            {
                ret = pCliMsg.GetObjArgAt(
                    0, pObj );
            }
        }

        if( ERROR( ret ) )
            break;
        
        CCfgOpener oReqCtx( ( IConfigDb* )pObj );
        ret = oReqCtx.GetQwordProp(
            propTaskId, qwTaskId );

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceServer::RetrieveDest(
    IEventSink* pCallback,
    stdstr& strDest ) const
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CIfInvokeMethodTask* pInv =
        static_cast< CIfInvokeMethodTask* >
            ( pCallback );
    do{
        EnumTaskState iState =
            pInv->GetTaskState();
        if( iState == stateStarted )
        {
            CCfgOpenerObj oInv( pInv );
            IConfigDb* pReq = nullptr;
            ret = oInv.GetPointer(
                propReqPtr, pReq );
            if( ERROR( ret ) )
                break;

            guint64 qwTaskId = 0;
            CReqOpener oReq( pReq );
            oReq.GetDestination( strDest );
            break;
        }

        if( iState != stateStarting )
        {
            ret = ERROR_STATE;
            break;
        }
        
        DMsgPtr pMsg;
        CCfgOpenerObj oInv( pInv );
        ret = oInv.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        DMsgPtr pCliMsg;
        CRpcRouter* pRouter = GetParent();
        ret = pMsg.GetMsgArgAt( 1, pCliMsg );
        if( ERROR( ret ) )
            break;
        
        strDest = pCliMsg.GetDestination();

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::FindFwrdReqsByTaskId(
    std::vector< guint64 >& vecTaskIds,
    FWRDREQS& vecTasks )
{
    gint32 ret = 0;
    do{
        FWRDREQS vecInvTasks;
        ret = FindFwrdReqsAll( vecInvTasks );
        if( ERROR( ret ) )
            break;
        
        for( auto elem : vecInvTasks )
        {
            CCfgOpenerObj oInv(
                ( CObjBase* )elem.first );

            CIfInvokeMethodTask* pInv =
                elem.first;

            guint64 qwTid = elem.second;
            std::vector< guint64 >::iterator 
            itr = std::find( vecTaskIds.begin(),
                vecTaskIds.end(), qwTid );

            if( itr == vecTaskIds.end() )
                continue;

            vecTasks.push_back( elem );
        }

        ret = 0;
        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    if( ERROR( ret ) )
        vecTasks.clear();

    return ret;
}

gint32 CRpcTcpBridge::FindFwrdReqsByPath(
    const stdstr& strPath,
    FWRDREQS& vecTasks )
{
    gint32 ret = 0;
    do{
        FWRDREQS vecTasksAll;
        ret = FindFwrdReqsAll(
            vecTasksAll, false );
        if( ERROR( ret ) )
            break;
        
        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( GetParent() );

        for( auto elem : vecTasksAll )
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

            stdstr strFullPath;
            ret = oReqCtx.GetStrProp(
                propRouterPath, strFullPath );
            if( ERROR( ret ) )
                continue;

            ret = IsMidwayPath(
                strPath, strFullPath );
            if( ERROR( ret ) )
                continue;

            vecTasks.push_back( elem );
        }

        ret = 0;
        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    if( ERROR( ret ) )
        vecTasks.clear();

    return ret;
}

gint32 CRpcTcpBridge::FindFwrdReqsByDestAddr(
    const stdstr& strPath,
    const stdstr& strDest,
    FWRDREQS& vecTasks )
{
    gint32 ret = 0;
    do{
        FWRDREQS vecTasksAll;
        ret = FindFwrdReqsAll(
            vecTasksAll, false );
        if( ERROR( ret ) )
            break;
        
        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( GetParent() );

        for( auto elem : vecTasksAll )
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

            stdstr strFullPath;
            ret = oReqCtx.GetStrProp(
                propRouterPath, strFullPath );
            if( ERROR( ret ) )
                continue;

            if( strPath != strFullPath );
                continue;

            stdstr strDestInv;
            ret = RetrieveDest(
                elem.first, strDestInv );
            if( ERROR( ret ) )
                continue;

            if( strDestInv != strDest )
                continue;

            vecTasks.push_back( elem );
        }

        ret = 0;
        if( vecTasks.empty() )
            ret = -ENOENT;

    }while( 0 );

    if( ERROR( ret ) )
        vecTasks.clear();

    return ret;
}

gint32 CRpcTcpBridge::ClearRemoteEventsLocal(
    IEventSink* pCallback,
    ObjPtr& pvecMatches )
{
    if( pCallback == nullptr ||
        pvecMatches.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    ObjVecPtr pMatches( pvecMatches );
    CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );
    do{
        std::vector< ObjPtr >& vecMatches =
            ( *pMatches )();

        CParamList oParams;
        oParams[ propEventSink ] =
            ObjPtr( pCallback );

        oParams[ propIfPtr ] = ObjPtr( pRouter );

        TaskletPtr pRespTask;
        ret = pRespTask.NewObj(
            clsid( CRouterEventRelayRespTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        // run the task to set it to a proper
        // state
        ( *pRespTask )( eventZero );

        oParams[ propNotifyClient ] = true;
        oParams[ propEventSink ] =
            ObjPtr( pRespTask );

        TaskGrpPtr pTaskGrp;
        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        for( auto pObj : vecMatches )
        {
            MatchPtr pMatch( pObj );
            CCfgOpenerObj oMatch(
                ( CObjBase* )pObj );
            MatchPtr pRmtMatch;

            oMatch.CopyProp( propPortId, this );

            ret = pRouter->GetMatchToAdd(
                pMatch, true, pRmtMatch );

            if( ERROR( ret ) )
                continue;

            TaskletPtr pDisEvtTask;
            ret = pRouter->BuildDisEvtTaskGrp(
                nullptr, pRmtMatch, pDisEvtTask );
            if( ERROR( ret ) )
                continue;

            ret = pTaskGrp->AppendTask(
                pDisEvtTask );

            if( ERROR( ret ) )
                break;
        }

        if( ERROR( ret ) )
            break;

        if( pTaskGrp->GetTaskCount() == 0 )
        {
            // do nothing
            ret = 0;
            break;
        }

        TaskletPtr pGrpTask = pTaskGrp;
        ret = pRouter->AddSeqTask( pGrpTask, false );
        if( ERROR( ret ) )
        {
            ( *pGrpTask )( eventCancelTask );
            break;
        }

        ret = STATUS_PENDING;

    }while( 0 );

    if( ERROR( ret ) )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        SetResponse( pCallback, oResp.GetCfg() );
    }

    return ret;
}

gint32 CRpcTcpBridge::OnClearRemoteEventsComplete(
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

gint32 CRpcTcpBridge::ClearRemoteEvents(
    IEventSink* pCallback,
    ObjPtr& pVecMatchesAll,
    ObjPtr& pVecTaskIds )
{
    if( unlikely( pCallback == nullptr ) )
        return -EINVAL;

    ObjVecPtr pMatches( pVecMatchesAll );
    QwVecPtr pTaskIdsIn( pVecTaskIds );
    if( unlikely( pMatches.IsEmpty() &&
        pTaskIdsIn.IsEmpty() ) )
        return -EINVAL;

    if( unlikely( ( *pMatches )().size() == 0 &&
        ( *pTaskIdsIn )().size() == 0 ) )
        return -EINVAL;

    if( unlikely( !IsConnected() ) )
        return ERROR_STATE;

    gint32 ret = 0;

    CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );

    TaskGrpPtr pParaGrp;
    TaskletPtr pRespCb;
    do{
        std::vector< ObjPtr >& vecMatchesAll =
            ( *pMatches )();

        ObjVecPtr pvecMatchesLoc( true );
        ObjVecPtr pvecMatchesMH( true );

        std::vector< ObjPtr >& vecMatchesLoc =
            ( *pvecMatchesLoc )();

        std::vector< ObjPtr >& vecMatchesMH =
            ( *pvecMatchesMH )();

        std::string strPath;
        for( auto elem : vecMatchesAll )
        {
            CCfgOpenerObj oMatchCfg(
                ( CObjBase* ) elem );

            oMatchCfg.CopyProp( propPortId, this );

            ret = oMatchCfg.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
                continue;

            if( strPath == "/" )
                vecMatchesLoc.push_back( elem );
            else
                vecMatchesMH.push_back( elem );
        }

        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );

        ret = pParaGrp.NewObj( 
            clsid( CIfParallelTaskGrp ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        FWRDREQS vecTaskKill;
        QwVecPtr pvecTaskIds = pTaskIdsIn;
        FindFwrdReqsByTaskId(
            ( *pvecTaskIds )(), vecTaskKill );

        ObjMapPtr pmapTaskIdsMH( true );
        ObjMapPtr pmapTaskIdsLoc( true );
        std::map< ObjPtr, QwVecPtr >&
            mapTasksLoc = ( *pmapTaskIdsLoc )();

        GetFwrdReqs( vecTaskKill,
            pmapTaskIdsLoc, pmapTaskIdsMH );

        ObjVecPtr pvecTasks( true );
        FWRDREQS_ITER itr = vecTaskKill.begin();
        while( itr != vecTaskKill.end() )
        {
            ( *pvecTasks )().push_back(
                itr->first );
            ++itr;
        }

        // cancel all affected tasks locally
        if( ( *pvecTasks )().size() > 0 )
             CancelInvTasks( pvecTasks );

        for( auto elem : mapTasksLoc )
        {
            TaskletPtr pTask;
            guint64 qwTaskId = 0;
            ret = DEFER_IFCALLEX_NOSCHED2(
                2, pTask, ObjPtr( elem.first ),
                &CRpcReqForwarderProxy::ForceCancelRequests,
                ObjPtr( elem.second ),
                qwTaskId, nullptr );
            if( ERROR( ret ) )
                continue;
            pParaGrp->AppendTask( pTask );
        }

        if( vecMatchesMH.size() > 0 ||
            ( *pmapTaskIdsMH )().size() > 0 )
        {
            TaskletPtr pClearMH;
            ret = DEFER_IFCALLEX_NOSCHED2( 
                0, pClearMH, ObjPtr( pRouter ),
                &CRpcRouterBridge::ClearRemoteEventsMH,
                ( IEventSink* )nullptr,
                ObjPtr( pvecMatchesMH ),
                ObjPtr( pmapTaskIdsMH ),
                false );

            if( ERROR( ret ) )
                break;
            pParaGrp->AppendTask( pClearMH );
        }

        CCfgOpener oReqCtx;
        oReqCtx[ propReturnValue ] = 0;

        ret = NEW_PROXY_RESP_HANDLER2(
            pRespCb, ObjPtr( this ),
            &CRpcTcpBridge::OnClearRemoteEventsComplete,
            pCallback, oReqCtx.GetCfg() );

        if( ERROR( ret ) )
            break;

        pParaGrp->SetClientNotify( pRespCb );

        if( vecMatchesLoc.size() > 0 )
        {
            TaskletPtr pClearLocal;
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pClearLocal, ObjPtr( this ),
                &CRpcTcpBridge::ClearRemoteEventsLocal,
                ( IEventSink* )nullptr,
                ObjPtr( pvecMatchesLoc ) );

            if( ERROR( ret ) )
                break;
            pParaGrp->AppendTask( pClearLocal );
        }

        TaskletPtr pParaTask( pParaGrp );
        CIoManager* pMgr = GetIoMgr();
        ret = pMgr->RescheduleTask( pParaTask );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    if( ERROR( ret ) )
    {
        if( !pParaGrp.IsEmpty() )
            ( *pParaGrp )( eventCancelTask );
        if( !pRespCb.IsEmpty() )
            ( *pRespCb )( eventCancelTask );

        CCfgOpener oResp;
        oResp[ propReturnValue ] = ret;
        SetResponse( pCallback, oResp.GetCfg() );
    }

    return ret;
}

gint32 CRpcTcpBridge::EnableRemoteEvent(
    IEventSink* pCallback, IMessageMatch* pMatch )
{
    if( pMatch == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    return EnableRemoteEventInternal(
        pCallback, pMatch, true );
}

gint32 CRpcTcpBridge::DisableRemoteEvent(
    IEventSink* pCallback, IMessageMatch* pMatch )
{
    if( pMatch == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    return EnableRemoteEventInternal(
        pCallback, pMatch, false );
}

gint32 CRpcTcpBridge::EnableRemoteEventInternal(
    IEventSink* pCallback,
    IMessageMatch* pMatch,
    bool bEnable )
{
    if( pMatch == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        // NOTE: we use the original match for
        // dbus listening
        CCfgOpenerObj oMatch( pMatch );

        // add the propPortId property of this
        // object's port
        oMatch.CopyProp( propPortId, this );
        // remove propPrxyPortId if any
        oMatch.RemoveProperty( propPrxyPortId );

        std::string strPath;
        ret = oMatch.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( GetParent() );

        if( strPath != "/" )
        {
            InterfPtr pIf;
            ret = pRouter->GetBridgeProxyByPath(
                strPath, pIf );
            if( ERROR( ret ) )
            {
                ret = -ENOTDIR;
                break;
            }

            ret = oMatch.CopyProp(
                propPrxyPortId,
                propPortId, pIf );

            if( ERROR( ret ) )
                break;

            ret = EnableRemoteEventInternalMH(
                pCallback, pMatch, bEnable );

            break;
        }

        // trim the match properties to the
        // necessary ones
        MatchPtr pRtMatch;
        ret = pRouter->GetMatchToAdd(
            pMatch, true, pRtMatch );

        if( ERROR( ret ) )
            break;

        if( bEnable )
        {
            ret = pRouter->RunEnableEventTask(
                pCallback, pRtMatch );
        }
        else
        {
            ret = pRouter->RunDisableEventTask(
                pCallback, pRtMatch );
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        CCfgOpener oParams;
        oParams[ propReturnValue ] = ret;
        SetResponse( pCallback,
            oParams.GetCfg() );
    }

    return ret;
}

gint32 CRpcTcpBridge::BuildBufForIrpFwrdEvt(
    BufPtr& pBuf, IConfigDb* pReqCall )
{
    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        DMsgPtr pMsg;

        ret = pMsg.NewObj( ( EnumClsid )
            DBUS_MESSAGE_TYPE_SIGNAL );

        if( ERROR( ret ) )
            break;

        ret = pMsg.SetMember(
            SYS_EVENT_FORWARDEVT );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oIfCfg( this );
        string strVal;
        ret = oIfCfg.GetStrProp(
            propSrcDBusName, strVal );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetSender( strVal );
        if( ERROR( ret ) )
            break;

        ret = oIfCfg.GetStrProp(
            propObjPath, strVal );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetPath( strVal );
        if( ERROR( ret ) )
            break;

        strVal = DBUS_IF_NAME(
            IFNAME_TCP_BRIDGE );

        pMsg.SetInterface( strVal );

        CReqOpener oReq( pReqCall );
        DMsgPtr pEvtMsg;

        ret = oReq.GetMsgPtr( 1, pEvtMsg );
        if( ERROR( ret ) )
            break;

        IConfigDb* pEvtCtx;
        ret = oReq.GetPointer( 0, pEvtCtx );
        if( ERROR( ret ) )
            break;

        BufPtr pCtxBuf( true );
        ret = pEvtCtx->Serialize( *pCtxBuf );
        if( ERROR( ret ) )
            break;

        const char* pCtx = pCtxBuf->ptr();

        BufPtr pMsgBuf( true );
        ret = pEvtMsg.Serialize( pMsgBuf ); 
        if( ERROR( ret ) )
            break;

        const char* pData = pMsgBuf->ptr();
        if( !dbus_message_append_args( pMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pMsgBuf->size(),
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

        *pBuf = pMsg;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::CheckReqToRelay(
    IConfigDb* pReqCtx,
    DMsgPtr& pMsg,
    MatchPtr& pMatchHit )
{
    CRpcRouterBridge* pRouter =
        static_cast< CRpcRouterBridge* >
            ( GetParent() );

    gint32 ret = pRouter->CheckReqToRelay(
        pReqCtx, pMsg, pMatchHit );

    return ret;
}

gint32 CRpcTcpBridge::ClearFwrdReqsByDestAddr(
    const stdstr strPath, DMsgPtr& pMsg )
{
    gint32 ret = 0;

    do{
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

        stdstr strDest;
        ret = pMsg.GetStrArgAt( 0, strDest );
        if( SUCCEEDED( ret ) )
        {
            FWRDREQS vecReqs;
            ret = FindFwrdReqsByDestAddr(
                strPath, strDest, vecReqs );
            ObjVecPtr pvecReqs( true );
            for( auto elem : vecReqs )
            {
                ( *pvecReqs )().push_back(
                    elem.first );
            }
            CancelInvTasks( pvecReqs );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::ForwardEvent(
    IConfigDb* pEvtCtx,
    DBusMessage* pEvtMsg,
    IEventSink* pCallback )
{
    if( pEvtMsg == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CReqBuilder oBuilder( this );
        CCfgOpener oEvtCtx;
        ret = oEvtCtx.CopyProp(
            propRouterPath, pEvtCtx );
        if( ERROR( ret ) )
            break;

        string strRouterPath;
        ret = oEvtCtx.GetStrProp(
            propRouterPath, strRouterPath );

        if( ERROR( ret ) )
            break;

        DMsgPtr pMsg( pEvtMsg );

        if( IS_SVRMODOFFLINE_EVENT( pMsg ) )
        {
            ClearFwrdReqsByDestAddr(
                strRouterPath, pMsg );
        }

        oBuilder.Push( ( IConfigDb* )
            oEvtCtx.GetCfg() );

        oBuilder.Push( pMsg );

        oBuilder.SetMethodName(
            SYS_EVENT_FORWARDEVT );

        oBuilder.SetCallFlags( 
           DBUS_MESSAGE_TYPE_SIGNAL 
           | CF_ASYNC_CALL );

        // a tcp default round trip time
        oBuilder.SetTimeoutSec(
            IFSTATE_DEFAULT_IOREQ_TIMEOUT ); 

        oBuilder.SetKeepAliveSec(
            IFSTATE_DEFAULT_IOREQ_TIMEOUT / 2 ); 

        oBuilder[ propStreamId ] =
            TCP_CONN_DEFAULT_STM;

        CParamList oRespCfg;
        CfgPtr pRespCfg = oRespCfg.GetCfg();
        if( pRespCfg.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        ret = RunIoTask( oBuilder.GetCfg(),
            pRespCfg, pCallback );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceServer::SetupReqIrp(
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

        if( SUCCEEDED( ret ) )
        {
            if( strMethod == SYS_EVENT_FORWARDEVT ||
                strMethod == SYS_EVENT_KEEPALIVE ||
                strMethod == SYS_EVENT_RMTSVREVENT ||
                strMethod == SYS_EVENT_REFRESHREQLIMIT )
            {
                ret = SetupReqIrpFwrdEvt( pIrp,
                    pReqCall, pCallback );
            }
            else
            {
                ret = super::SetupReqIrp( pIrp,
                    pReqCall, pCallback );
            }
            break;
        }
        else
        {
            // possibly the method name will not
            // be set for some requests,
            // especially for CRpcTcpBridge and
            // CRpcTcpBridgeProxy
            ret = super::SetupReqIrp( pIrp,
                pReqCall, pCallback );

            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceServer::BuildBufForIrp(
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

        if( strMethod == SYS_EVENT_FORWARDEVT )
        {
            ret = BuildBufForIrpFwrdEvt(
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

gint32 CRpcInterfaceServer::OnKeepAlive(
    IEventSink* pTask, EnumKAPhase iPhase )
{
    if( iPhase == KARelay )
    {
        return OnKeepAliveRelay( pTask );
    }

    return -EINVAL;
}

gint32 CRpcInterfaceServer::CloneInvTask(
    IEventSink* pCallback,
    TaskletPtr& pTask ) const
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CParamList oParams;
        oParams.CopyProp( propIfPtr, pCallback );
        oParams.CopyProp( propMsgPtr, pCallback );
        oParams.CopyProp( propMatchPtr, pCallback );

        ret = pTask.NewObj(
            clsid( CIfInvokeMethodTask ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        ret = InstallQFCallback( pTask );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceServer::OnFwrdReqQueueFull(
    IEventSink* pCallback,
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pCallback == nullptr ||
        pIoReq == nullptr )
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
        if( ret != ERROR_QUEUE_FULL )
            break;

        TaskletPtr pTask;
        ret = CloneInvTask( pCallback, pTask );
        if( ERROR( ret ) )
            break;

        // calling RequeueInvTask directly could
        // cause infinite loop, and we have to
        // call it asynchronously.
        ret = DEFER_CALL( GetIoMgr(), this,
            &CRpcInterfaceServer::RequeueInvTask,
            pTask );

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceServer::InstallQFCallback(
    TaskletPtr& pInvTask ) const
{
    TaskletPtr pTask;
    ObjPtr pThis( ( CObjBase* )this );
    gint32 ret = NEW_PROXY_RESP_HANDLER2( 
        pTask, pThis,
        &CRpcInterfaceServer::OnFwrdReqQueueFull,
        pInvTask, nullptr );
    if( ERROR( ret ) )
        return ret;

    CIfRetryTask* pInv = pInvTask;
    return InterceptCallback( pTask, pInv );
}

gint32 CRpcTcpBridge::RequeueInvTask(
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

        pTask->MarkPending();
        CIfParallelTaskGrpRfc* pGrpRfc = m_pGrpRfc;
        CStdRTMutex oLock( pGrpRfc->GetLock() );
        gint32 ret = pGrpRfc->InsertTask( pTask );
        if( ERROR( ret ) )
            return ret;

        if( pGrpRfc->GetRunningCount() <
            pGrpRfc->GetMaxRunning() &&
            !pGrpRfc->IsNoSched() )
        {
            pTask = ObjPtr( m_pGrpRfc );
            oLock.Unlock();
            ( *pTask )( eventZero );
        }
    }while( 0 );
    
    if( ERROR( ret ) )
    {
        TaskletPtr pTask = ObjPtr( pCallback );
        if( !pTask.IsEmpty() )
            ( *pTask )( eventCancelTask );
    }

    return 0;
}

gint32 CRpcTcpBridge::OnKeepAliveRelay(
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

        if( !IsConnected() )
        {
            // NOTE: this is not a serious state
            // check
            ret = ERROR_STATE;
            break;
        }

        CReqBuilder okaReq( this );

        guint64 iTaskId = 0;
        ret = pMsg.GetInt64ArgAt( 2, iTaskId );
        if( ERROR( ret ) )
            break;

        okaReq.Push( iTaskId );
        okaReq.Push( ( guint32 )KARelay );

        okaReq.SetMethodName(
            SYS_EVENT_KEEPALIVE );

        okaReq.SetCallFlags( 
           DBUS_MESSAGE_TYPE_SIGNAL
           | CF_ASYNC_CALL );
        
        okaReq[ propStreamId ] =
            TCP_CONN_DEFAULT_STM;

        string strVal = pMsg.GetDestination( );
        if( strVal.empty() )
        {
            ret = -EINVAL;
            break;
        }
        okaReq.SetSender( strVal );

        strVal = pMsg.GetSender( );
        okaReq.SetDestination( strVal );

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

gint32 CBridgeForwardRequestTask::RunTask()
{
    gint32 ret = 0;

    ObjPtr pObj;
    CParamList oParams( GetConfig() );
    DMsgPtr pRespMsg;
    CRpcRouterBridge* pRouter = nullptr;

    do{
        CRpcTcpBridge* pIf;
        ret = oParams.GetPointer( propIfPtr, pIf );
        if( ERROR( ret ) )
            break;

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

        CCfgOpener oReqCtx( pReqCtx );
        std::string strRouterPath;
        ret = oReqCtx.GetStrProp(
            propRouterPath, strRouterPath );
        if( ERROR( ret ) )
            break;

        // port id for request canceling
        oReqCtx.CopyProp( propConnHandle,
            propPortId, pIf );

        if( strRouterPath != "/" )
        {
            ret = ForwardRequestMH(
                pReqCtx, pMsg, pRespMsg );
            break;
        }

        string strDest;
        ret = oParams.GetStrProp(
            2, strDest );

        if( ERROR( ret ) )
            break;

        // we have reach the destination
        InterfPtr proxyPtr;
        ret = pRouter->GetReqFwdrProxy(
            strDest, proxyPtr );

        CRpcReqForwarderProxy* pProxy =
            proxyPtr;

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }
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

gint32 CBridgeForwardRequestTask::ForwardRequestMH(
    IConfigDb* pReqCtx,
    DBusMessage* pReqMsg,           // [ in ]
    DMsgPtr& pRespMsg )
{
    if( pReqCtx == nullptr ||
        pReqMsg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{

        CCfgOpener oReqCtx( pReqCtx );
        CCfgOpener oTaskCfg(
            ( IConfigDb* )GetConfig() );

        std::string strRouterPath;
        ret = oReqCtx.GetStrProp(
            propRouterPath, strRouterPath );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter = nullptr;
        ret = oTaskCfg.GetPointer(
            propRouterPtr, pRouter );
        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        ret = pRouter->GetBridgeProxyByPath(
            strRouterPath, pIf );

        if( ERROR( ret ) )
            break;

        std::string strNode;
        CCfgOpenerObj oIfCfg( ( CObjBase* )pIf );
        ret = oIfCfg.GetStrProp(
            propNodeName, strNode );

        if( ERROR( ret ) )
            break;

        // strip current node's name from the
        // router path
        std::string strNext;
        if( strRouterPath.substr( 1 ) == strNode )
        {
            strNext = "/";
        }
        else
        {
            strNext = strRouterPath.substr(
                1 + strNode.size() );
        }

        oReqCtx.SetStrProp(
            propRouterPath, strNext );

        CRpcTcpBridgeProxy* pProxy =
            ObjPtr( pIf );

        ret = pProxy->ForwardRequest(
            pReqCtx, pReqMsg, pRespMsg, this );

    }while( 0 );

    return ret;
}

gint32 CBridgeForwardRequestTask::OnTaskCompleteRfc(
    gint32 iRetVal,
    TaskletPtr& pIoTask )
{ return -ENOTSUP; }

gint32 CRpcTcpBridge::IsMHTask(
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    if( pTask.IsEmpty() )
        return -EINVAL;

    do{
        CIfInvokeMethodTask* pInv = pTask;
        if( unlikely( pInv == nullptr ) )
        {
            ret = -EINVAL;
            break;
        }
        CCfgOpenerObj oInvCfg( pInv );
        DMsgPtr pMsg;
        ret = oInvCfg.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 0, pObj );
        if( ERROR( ret ) )
            break;

        stdstr strPath;
        CCfgOpener oReqCtx( ( IConfigDb* )pObj );
        ret = oReqCtx.GetStrProp(
            propRouterPath, strPath );

        if( ERROR( ret ) )
            break;

        if( strPath == "/" )
        {
            ret = ERROR_FALSE;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::GetFwrdReqs(
    FWRDREQS& vecReqs,
    ObjMapPtr& pmapTaskIdsLoc,
    ObjMapPtr& pmapTaskIdsMH )
{
    gint32 ret = 0;
    do{
        std::map< ObjPtr, QwVecPtr >&
            mapTasksLoc = ( *pmapTaskIdsLoc )();

        std::map< ObjPtr, QwVecPtr >&
            mapTasksMH = ( *pmapTaskIdsMH )();

        for( auto elem : vecReqs )
        {
            bool bQueued = false;
            InterfPtr pProxy;
            ret = GetInvTaskProxyMH(
                elem.first, pProxy, bQueued );
            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }
            if( bQueued )
                continue;

            ret = IsMHTask( elem.first );
            if( SUCCEEDED( ret ) )
            {
                QwVecPtr& pTaskIds =
                    mapTasksMH[ pProxy ];
                if( pTaskIds.IsEmpty() )
                    pTaskIds.NewObj();
                ( *pTaskIds )().push_back(
                    elem.second );
            }
            else if( ret == ERROR_FALSE )
            {
                QwVecPtr& pTaskIds =
                    mapTasksLoc[ pProxy ];
                if( pTaskIds.IsEmpty() )
                    pTaskIds.NewObj();
                ( *pTaskIds )().push_back(
                    elem.second );
            }
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::GetAllFwrdReqs(
    FWRDREQS& vecReqs,
    ObjMapPtr& pmapTaskIdsLoc,
    ObjMapPtr& pmapTaskIdsMH )
{
    gint32 ret = FindFwrdReqsAll( vecReqs );
    if( ERROR( ret ) )
        return ret;
    return GetFwrdReqs( vecReqs,
        pmapTaskIdsLoc, pmapTaskIdsMH );
}

gint32 CRpcTcpBridge::GetInvTaskProxyMH(
    TaskletPtr& pTask,
    InterfPtr& pIf,
    bool bQueued )
{
    gint32 ret = 0;
    if( pTask.IsEmpty() )
        return -EINVAL;
    do{
        CIfInvokeMethodTask* pInv = pTask;
        if( unlikely( pInv == nullptr ) )
        {
            ret = -EINVAL;
            break;
        }

        TaskletPtr pIoTask =
            pInv->GetEndFwrdTask();

        CIfParallelTask* pEndReq = pIoTask;

        CCfgOpenerObj oIoCfg( pEndReq );
        EnumTaskState iTaskState =
            pEndReq->GetTaskState();
        if( iTaskState == stateStarting )
        {
            bQueued = true;   
            break;
        }
        else if( iTaskState == stateStarted )
        {
            bQueued = false;
        }
        else
        {
            ret = ERROR_STATE;
            break;
        }

        CRpcServices* pSvc = nullptr;
        ret = oIoCfg.GetPointer( propIfPtr, pSvc );
        if( ERROR( ret ) )
            break;

        pIf = pSvc;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::ForwardRequest(
    IConfigDb* pReqCtx,
    DBusMessage* pFwdrMsg,
    DMsgPtr& pRespMsg,
    IEventSink* pCallback )
{
    return ForwardRequestInternal( pReqCtx,
        pFwdrMsg, pRespMsg, pCallback, false );
}

/*
 * pReqCtx contents across network:
 * propRouterPath: the current router path
 * propSessHash: the session id
 * propTimestamp: the request's birth date
 * propPath2: the client's router path
 * propNoReply if no-reply is true
 * propTaskId: the request's uniq-id
 *
 * copied after DoInvoke
 * propReqPtr
 *
 */
gint32 CRpcTcpBridge::ForwardRequestInternal(
    IConfigDb* pReqCtx,
    DBusMessage* pFwdrMsg,
    DMsgPtr& pRespMsg,
    IEventSink* pCallback,
    bool bSeqTask )
{
    if( pFwdrMsg == nullptr ||
        pCallback == nullptr ||
        pReqCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    pRespMsg.Clear();

    do{
        DMsgPtr fwdrMsg( pFwdrMsg );
        CCfgOpener oOrigCtx( pReqCtx );

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( GetParent() );

        CCfgOpener oReqCtx;
        oReqCtx.CopyProp(
            propRouterPath, pReqCtx );

        std::string strRouterPath;
        ret = oReqCtx.GetStrProp(
            propRouterPath, strRouterPath );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        CCfgOpenerObj oIfCfg( this );
        ret = oIfCfg.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        oReqCtx[ propConnHandle ] = dwPortId;

        MatchPtr pMatch;
        ret = CheckReqToRelay(
            oReqCtx.GetCfg(), fwdrMsg, pMatch );

        // invalid request
        if( ERROR( ret ) )
            break;

        if( !pReqCtx->exist( propSessHash ) )
        {
            oOrigCtx.SetStrProp(
                propSessHash, GetSessHash() );
        }

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
            propRouterPtr, pRouter );

        CCfgOpenerObj oMatch(
            ( CObjBase* )pMatch ); 

        std::string strDest;
        ret = oMatch.GetStrProp(
            propDestDBusName, strDest );
        if( ERROR( ret ) )
            break;

        oParams.Push( pReqCtx );
        oParams.Push( fwdrMsg );
        oParams.Push( strDest );

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CBridgeForwardRequestTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        if( bSeqTask )
        {
            ret = pRouter->AddSeqTask( pTask );
            if( ERROR( ret ) )
            {
                ( *pTask )( eventCancelTask );
                break;
            }
            ret = STATUS_PENDING;
        }
        else
        {
            // ret = GetIoMgr()->RescheduleTask( pTask );
            ( *pTask )( eventZero );
            ret = pTask->GetError();
        }

    }while( 0 );

    // the return value indicates if the response
    // message is generated or not.
    return ret;
}

bool CRpcInterfaceServer::IsConnected(
    const char* szAddr )
{
    if( !super::IsConnected( szAddr ) )
        return false;
    return  m_pParent->IsConnected( szAddr );
}

gint32 CRpcInterfaceServer::ValidateRequest_SendData(
    DBusMessage* pReqMsg,
    IConfigDb* pDataDesc )
{
    gint32 ret = 0;

    if( pDataDesc == nullptr ||
        pReqMsg == nullptr )
        return -EINVAL;

    do{
        EnumClsid iidClient = clsid( Invalid );

        CCfgOpener oDataDesc(
            ( IConfigDb* )pDataDesc );

        guint32* piid = ( guint32* )&iidClient;
        ret = oDataDesc.GetIntProp(
            propIid, *piid );

        if( ERROR( ret ) )
            break;

        if( iidClient != iid( IStream ) &&
            iidClient != iid( CFileTransferServer ) &&
            iidClient != iid( IStreamMH ) )
        {
            ret = -EBADMSG;
            break;
        }

        DMsgPtr pMsg( pReqMsg );
        string strMethod = pMsg.GetMember();
        if( strMethod.empty() )
        {
            ret = -EBADMSG;
            break;
        }

        // uncomment this if it is supported
        // in the future
        if( iidClient == iid( IStream ) &&
            strMethod == SYS_METHOD_SENDDATA )
        {
            ret = -EBADMSG;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceServer::DoInvoke(
    DBusMessage* pReqMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;
    if( pReqMsg == nullptr )
        return -EINVAL;

    do{
        CCfgOpenerObj oCfg( this );
        DMsgPtr pMsg( pReqMsg );
        CParamList oResp;

        string strMethod = pMsg.GetMember();
        if( strMethod.empty() )
        {
            ret = -EBADMSG;
            break;
        }

        vector< DMsgPtr::ARG_ENTRY > vecArgs;
        ret = pMsg.GetArgs( vecArgs );
        if( ERROR( ret ) )
            break;

        bool bResp = false;

        CfgPtr pCfg( true );
        do{
            if( strMethod == SYS_METHOD_FORWARDREQ )
            {
                // DebugPrint( 0, "probe: invoke method" );
                guint32 iArgCount = GetArgCount(
                    &IRpcReqProxyAsync::ForwardRequest );

                iArgCount -= 2;

                if( vecArgs.empty() ||
                    vecArgs.size() < iArgCount )
                {
                    ret = -EBADMSG;
                    break;
                }

                CfgPtr pReqCtx;
                ret = pReqCtx.Deserialize(
                    *vecArgs[ 0 ].second );
                if( ERROR( ret ) )
                    break;

                CRpcTcpBridge* pIf = ObjPtr( this );
                bool bBridge = ( pIf != nullptr );

                DMsgPtr pFwdrMsg;
                ret = pFwdrMsg.Deserialize(
                    *vecArgs[ 1 ].second );

                if( ERROR( ret ) )
                    break;

                guint64 qwFwdrTaskId = 0;
                if( bBridge && vecArgs.size() >
                    iArgCount + 1 )
                {
                    ret = -EINVAL;
                    break;
                }
                else if( bBridge )
                {
                    qwFwdrTaskId =
                        ( guint64& )*vecArgs[ 2 ].second;
                }

                // NOTE: bring the retrieving of the
                // req from the message ahead of the
                // ForwardRequest to avoid race
                // condition of pFwrdMsg between this
                // thread and CRpcReqForwarderProxy's
                // ForwardRequest. It could cause
                // dbus_message_iter_init to fail.
                ObjPtr pObj;
                ret =  pFwdrMsg.GetObjArgAt( 0, pObj );
                if( ERROR( ret ) )
                    break;

                bool bFlagValid = false;
                guint32 dwCallFlags = 0;
                CReqOpener oOrigReq( ( IConfigDb* )pObj );
                ret = oOrigReq.GetCallFlags( dwCallFlags );
                if( SUCCEEDED( ret ) )
                {
                    bFlagValid = true;
                    if( dwCallFlags & CF_WITH_REPLY )
                        bResp = true;
                }

                CCfgOpener oReqCtx(
                    ( IConfigDb* )pReqCtx );

                oReqCtx.CopyProp( propTaskId, pObj );
                // for use by the reqfwdrproxy
                // or tcpbridgeproxy 
                oReqCtx.SetObjPtr( propReqPtr, pObj );

                CCfgOpenerObj oTaskCfg( pCallback );
                // make sure there is a propReqPtr
                // for the message filter
                oTaskCfg.SetObjPtr( propReqPtr, pObj );

                DMsgPtr pRespMsg;
                ret = ForwardRequest( pReqCtx,
                    pFwdrMsg, pRespMsg, pCallback );

                if( ret == STATUS_PENDING )
                {

                    CReqBuilder oNewReq( this );
                    oNewReq.SetMethodName( SYS_METHOD_FORWARDREQ );

                    guint32 dwTimeout = 0;
                    gint32 iRet =
                        oOrigReq.GetTimeoutSec( dwTimeout );
                    if( SUCCEEDED( iRet ) )
                    {
                        if( dwTimeout > ( MAX_TIMEOUT_VALUE / 30 ) )
                            dwTimeout = ( MAX_TIMEOUT_VALUE / 30 );
                        if( dwTimeout > 120 )
                            oNewReq.SetTimeoutSec( dwTimeout );
                    }

                    iRet = oOrigReq.GetKeepAliveSec( dwTimeout );
                    if( SUCCEEDED( iRet ) )
                    {
                        if( dwTimeout > ( MAX_TIMEOUT_VALUE / 30 ) )
                            dwTimeout = ( MAX_TIMEOUT_VALUE / 30 );
                        if( dwTimeout > 120 )
                            oNewReq.SetKeepAliveSec( dwTimeout );
                    }

                    if( bFlagValid )
                    {
                        // don't start keep-alive actively. The
                        // keep-alive event should be initiated
                        // from the server side, but not from
                        // within the router
                        dwCallFlags &= ( ~CF_KEEP_ALIVE );
                        oNewReq.SetCallFlags( dwCallFlags );
                    }

                    // keep-alive settings
                    if( bBridge )
                    {
                        oTaskCfg.SetQwordProp(
                            propRmtTaskId, qwFwdrTaskId );
                    }
                    else
                    {
                        SET_RMT_TASKID(
                            ( IConfigDb* )pObj, oTaskCfg );
                    }

                    oNewReq.CopyProp(
                        propTaskId, ( IConfigDb* )pObj );

                    oNewReq.SetDestination(
                        pFwdrMsg.GetDestination() );

                    oTaskCfg.SetPointer( propReqPtr,
                        ( IConfigDb* )oNewReq.GetCfg() );

                    break;
                }
                else if( ERROR( ret ) )
                {
                    if( ret == -ENOTSUP )
                        ret = ERROR_FAIL;
                }

                oReqCtx.RemoveProperty( propReqPtr );
                // whether successful or not, we
                // put the result in the response
                if( bResp )
                {
                    oResp.SetIntProp(
                        propReturnValue, ret );

                    if( !pRespMsg.IsEmpty() )
                        oResp.Push( pRespMsg );
                }
                break;
            }
            else
            {
                ret = -ENOTSUP;
                break;
            }

        }while( 0 );

        if( ret == STATUS_PENDING )
            break;

        if( ret != -ENOTSUP && bResp )
        {
            // for not supported commands we will
            // try to handle them in the base
            // class
            if( IsRfcEnabled() &&
                ret == ERROR_QUEUE_FULL )
                break;
            SetResponse( pCallback, oResp.GetCfg() );
        }

    }while( 0 );

    if( ret == -ENOTSUP )
    {
        ret = super::DoInvoke(
            pReqMsg, pCallback );
    }

    return ret;
}

gint32 CRpcInterfaceServer::CancelInvTasks(
    ObjVecPtr& pTasks )
{
    if( pTasks.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        std::vector< ObjPtr > vecTasks =
            ( *pTasks )();

        for( auto elem : vecTasks )
        {
            CIfRetryTask* pInv = elem;
            if( pInv == nullptr )
                continue;

            TaskletPtr pTask =
                pInv->GetEndFwrdTask();

            pTask->OnEvent( eventCancelTask,
                0, 0, nullptr );
        }

    }while( 0 );

    return ret;
}

bool CRpcInterfaceServer::IsRfcEnabled() const
{
    CRpcRouter* pRouter =
        static_cast< CRpcRouter* >
            ( GetParent() );
    return pRouter->IsRfcEnabled();
}

gint32 CRpcInterfaceServer::AddAndRun(
    TaskletPtr& pTask, bool bImmediate )
{
    do{
        gint32 ret = 0;
        if( !IsRfcEnabled() )
            break;

        CIfInvokeMethodTask* pInv = pTask;
        if( pInv == nullptr )
            break;

        CCfgOpener oCfg(
            ( IConfigDb* )pInv->GetConfig() );

        DMsgPtr pMsg;
        oCfg.GetMsgPtr( propMsgPtr, pMsg );
        stdstr strMethod = pMsg.GetMember();
        if( strMethod != SYS_METHOD_FORWARDREQ )
            break;

        TaskGrpPtr pGrp;
        ret = GetGrpRfc( pMsg, pGrp );
        if( ERROR( ret ) )
            break;

        CIfParallelTaskGrpRfc* pGrpRfc = pGrp;
        ret = InstallQFCallback( pTask );
        if( ERROR( ret ) )
            break;

        ObjPtr pMatch;
        oCfg.GetObjPtr( propMatchPtr, pMatch );

        ret = pGrpRfc->AddAndRun( pTask );

        gint32 iRet = 0;
        if( SUCCEEDED( ret ) ||
            ret == STATUS_PENDING )
        {
            iRet = pTask->GetError();
            if( iRet == ERROR_QUEUE_FULL )
            {
                CStdRTMutex oLock(
                    pGrpRfc->GetLock() );
                pGrpRfc->SetLimit(
                    pGrpRfc->GetRunningCount(),
                    pGrpRfc->GetMaxPending() );
            }
            return ret;
        }
        else if( ret != ERROR_QUEUE_FULL )
        {
            ( *pTask )( eventCancelTask );
           return ret;
        }

        // notify the client we have reached the
        // limit
        bool bResp = true;
        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 0, pObj );
        if( SUCCEEDED( ret ) )
        {
            IConfigDb* pReqCtx = pObj;
            if( pReqCtx != nullptr )
            {
                bool bNoReply;
                CCfgOpener oReqCtx( pReqCtx );
                ret = oReqCtx.GetBoolProp(
                    propNoReply, bNoReply );
                if( SUCCEEDED( ret ) )
                    bResp = !bNoReply;
            }

        }

        EventPtr pEvt;
        ret = pInv->GetClientNotify( pEvt );
        if( SUCCEEDED( ret ) )
        {
            pInv->ClearClientNotify();
            TaskletPtr pQFTask = pEvt;
            ( *pQFTask )( eventCancelTask );
        }

        if( !bResp )
        {
            ( *pInv )( eventCancelTask );
            return ret;
        }

        CCfgOpener oResp;
        oResp[ propReturnValue ] = ERROR_QUEUE_FULL;
        OnServiceComplete( oResp.GetCfg(), pInv );

        return ret;

    }while( 0 );

    return super::AddAndRun( pTask, bImmediate );
}

gint32 CRpcTcpBridge::SetupReqIrp(
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

        if( strMethod == IF_METHOD_LISTENING )
        {
            ret = SetupReqIrpListening(
                pIrp, pReqCall, pCallback );
        }
        else
        {
            ret = -ENOTSUP;
        }
        break;

    }while( 0 );

    if( ret == -ENOTSUP )
    {
        ret = super::SetupReqIrp( pIrp,
            pReqCall, pCallback );
    }

    return ret;
}

gint32 CRpcTcpBridge::SetResponse(
    IEventSink* pCallback,
    CfgPtr& pRespData )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CReqBuilder oResp(
            ( IConfigDb* )pRespData );

        CTasklet* pTask = ObjPtr( pCallback );
        if( pTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CParamList oTask( pTask->GetConfig() );

        ObjPtr pObj;
        gint32 iType = 0;
        ret = pTask->GetConfig()->
            GetPropertyType( propMsgPtr, iType );

        if( ERROR( ret ) )
            break;

        if( iType == typeDMsg )
        {
            super::SetResponse(
                pTask, pRespData );
            break;
        }
        else if( iType != typeObj )
        {
            ret = -EFAULT;
            break;
        }

        ret = oTask.GetObjPtr(
            propMsgPtr, pObj );

        if( ERROR( ret ) )
            break;

        IConfigDb* pReq = pObj;
        if( pReq == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        oResp.CopyProp( propSeqNo, pReq );
        oResp.SetCallFlags( 
            DBUS_MESSAGE_TYPE_METHOD_RETURN );

        super::SetResponse( pCallback,
            oResp.GetCfg() );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::CloseStream_Server(
    IEventSink* pCallback,
    gint32 iStreamId )
{
    // server side processing of the
    // request
    gint32 ret = 0;

    do{
        if( IsReserveStm( iStreamId ) )
        {
            ret = -EINVAL;
            break;
        }

        PortPtr pPort;
        GET_TARGET_PORT( pPort );
        if( ERROR( ret ) )
            break;

        gint32 iRet = CloseLocalStream(
            pPort, iStreamId );

        if( pCallback != nullptr )
        {
            ObjPtr pObj;
            CCfgOpenerObj oTask( pCallback );

            ret = oTask.GetObjPtr(
                propMsgPtr, pObj );

            if( ERROR( ret ) )
                break;

            const IConfigDb* pReq = pObj;
            if( pReq == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            // inherit the information from the
            // request
            CReqBuilder oResp;

            oResp.CopyProp( propMethodName, pReq );
            oResp[ propStreamId ] = TCP_CONN_DEFAULT_CMD;
            oResp[ propProtoId ] = protoControl;
            oResp[ propCmdId ] = CTRLCODE_CLOSE_STREAM_PDO;
            oResp[ propSubmitPdo ] = (bool)true;
            oResp.CopyProp( propSeqNo, pReq );

            oResp.SetCallFlags( 
                CF_NON_DBUS |
                DBUS_MESSAGE_TYPE_METHOD_RETURN |
                CF_ASYNC_CALL );

            oResp.ClearParams();
            oResp.Push( iStreamId );
            oResp.SetReturnValue( iRet );
            ret = SetResponse(
                pCallback, oResp.GetCfg() );
        }

    }while( 0 );


    return ret;
}

gint32 CRpcTcpBridge::OpenStream_Server(
    IEventSink* pCallback,
    gint32 iPeerStm,
    guint32 dwProtocol )
{
    gint32 ret = 0;
    gint32  iNewStm = 0;

    do{
        if( !IsConnected() )
        {
            // NOTE: this is not a serious check
            ret = ERROR_STATE;
            break;
        }

        PortPtr pPort;
        GET_TARGET_PORT( pPort );
        if( ERROR( ret ) )
            break;

        gint32 iRet = OpenLocalStream( pPort,
            iPeerStm, dwProtocol, iNewStm );

        if( pCallback != nullptr )
        {
            ObjPtr pObj;
            CCfgOpenerObj oTask( pCallback );

            ret = oTask.GetObjPtr(
                propMsgPtr, pObj );

            if( ERROR( ret ) )
                break;

            const IConfigDb* pReq = pObj;
            if( pReq == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            // inherit the information from the
            // request
            CReqBuilder oResp;

            oResp.SetReturnValue( iRet );
            oResp.CopyProp( propMethodName, pReq );
            oResp[ propStreamId ] = TCP_CONN_DEFAULT_CMD;
            oResp[ propProtoId ] = protoControl;
            oResp[ propCmdId ] = CTRLCODE_OPEN_STREAM_PDO;
            oResp[ propSubmitPdo ] = (bool)true;
            oResp.CopyProp( propSeqNo, pReq );

            oResp.SetCallFlags( 
                CF_NON_DBUS |
                DBUS_MESSAGE_TYPE_METHOD_RETURN |
                CF_ASYNC_CALL );

            oResp.ClearParams();
            if( SUCCEEDED( iRet ) )
            {
                oResp.Push( iNewStm );
                oResp.Push( dwProtocol );
                oResp.Push( iPeerStm );
            }

            ret = SetResponse(
                pCallback, oResp.GetCfg() );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::InitUserFuncs()
{
    BEGIN_IFHANDLER_MAP( CRpcTcpBridge );

    ADD_SERVICE_HANDLER(
        CRpcTcpBridge::OpenStream,
        BRIDGE_METHOD_OPENSTM );

    ADD_SERVICE_HANDLER(
        CRpcTcpBridge::CloseStream,
        BRIDGE_METHOD_CLOSESTM );

    ADD_SERVICE_HANDLER(
        CRpcTcpBridge::EnableRemoteEvent,
        SYS_METHOD_ENABLERMTEVT );

    ADD_SERVICE_HANDLER(
        CRpcTcpBridge::DisableRemoteEvent,
        SYS_METHOD_DISABLERMTEVT );

    ADD_SERVICE_HANDLER(
        CRpcTcpBridge::ClearRemoteEvents,
        SYS_METHOD_CLEARRMTEVTS );

    ADD_SERVICE_HANDLER(
        CRpcTcpBridge::CheckRouterPath,
        SYS_METHOD_CHECK_ROUTERPATH );

    ADD_SERVICE_HANDLER(
        CRpcTcpBridge::Handshake,
        SYS_METHOD_HANDSAKE );

    END_HANDLER_MAP;

    return 0;
}

gint32 CRpcTcpBridge::SendResponse(
    IEventSink* pInvTask,
    IConfigDb* pReq,
    CfgPtr& pResp )
{
    if( pReq == nullptr || pResp.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CReqBuilder oResp(
            ( IConfigDb* )pResp );

        CReqOpener oReq( pReq );

        guint32 dwFlags = 0;
        ret = oReq.GetCallFlags( dwFlags );
        if( ERROR( ret ) )
            break;

        if( !( dwFlags & CF_WITH_REPLY ) )
            break;

        if( ( dwFlags & CF_MESSAGE_TYPE_MASK ) !=
            DBUS_MESSAGE_TYPE_METHOD_CALL )
            break;

        bool bPdo = true;
        PortPtr pPort;
        ret = GetPortToSubmit(
            pReq, pPort, bPdo );

        if( ERROR( ret ) )
            break;

        guint32 dwCmdId = 0;
        BufPtr pBuf( true );
        if( dwFlags & CF_NON_DBUS )
        {
            *pBuf = ObjPtr( pResp );
            ret = oReq.GetIntProp(
                propCmdId, dwCmdId );

            if( ERROR( ret ) )
                break;
        }
        else
        {
            DMsgPtr& pMsg = oResp[ 0 ];
            *pBuf = pMsg;
        }

        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pCtx = pIrp->GetTopStack(); 
        pCtx->SetMajorCmd( IRP_MJ_FUNC );
        pCtx->SetMinorCmd( IRP_MN_IOCTL );

        gint32 iStreamId = TCP_CONN_DEFAULT_STM;
        if( !bPdo )
        {
            pCtx->SetCtrlCode( CTRLCODE_SEND_RESP );
            oReq.GetIntProp( propStreamId,
                ( guint32& )iStreamId );
        }
        else
        {
            pCtx->SetCtrlCode( dwCmdId );
        }

        pPort->AllocIrpCtxExt(
            pCtx, ( PIRP )pIrp );

        pCtx->SetIoDirection( IRP_DIR_OUT ); 
        pCtx->SetReqData( pBuf );
        pCtx->SetNonDBusReq( true );

        CPort* pPort2 = ( CPort* )pPort;
        CIoManager* pMgr = pPort2->GetIoMgr();

        if( !bPdo )
            SetBdgeIrpStmId( pIrp, iStreamId );

        ret = pMgr->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ret == STATUS_PENDING )
            break;

        if( ret == ERROR_STATE )
            break;

        if( ret == -EAGAIN )
            ret = STATUS_MORE_PROCESS_NEEDED;

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::SetupReqIrpFwrdEvt(
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
        pIrpCtx->SetIoDirection( IRP_DIR_OUT ); 

        CCfgOpener oReq( pReqCall );
        SetBdgeIrpStmId( pIrp,
            ( guint32& )oReq[ propStreamId ] );

        // we don't need a timer
        if( pCallback != nullptr )
        {
            pIrp->SetCallback( pCallback, 0 );
            pIrp->SetIrpThread( GetIoMgr() );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::StartEx2(
    IEventSink* pCallback )
{
    return CRpcTcpBridgeShared::StartEx2(
        pCallback );
}

gint32 CRpcTcpBridge::CheckHsTimeout(
    IEventSink* pTask,
    IEventSink* pStartCb )
{
    gint32 ret = 0;
    do{
        CCfgOpenerObj oCfg(
            ( CObjBase* )pTask );

        guint32 dwRetries;
        ret = oCfg.GetIntProp(
            propRetries, dwRetries );
        if( ERROR( ret ) )
            break;

        CStdRMutex oIfLock( GetLock() );
        if( m_bHandshaked )
        {
            if( m_bHsFailed )
                ret = ERROR_FAIL;
            break;
        }

        if( dwRetries > 0 )
        {
            ret = STATUS_MORE_PROCESS_NEEDED;
            break;
        }

        // disconnect
        ret = -ETIMEDOUT;

    }while( 0 );

    if( ret != STATUS_MORE_PROCESS_NEEDED )
    {
        pStartCb->OnEvent( eventTaskComp,
            ret, 0, nullptr );
    }

    return ret;
}

gint32 CRpcTcpBridge::DoStartHandshake(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    TaskletPtr pHsTicker;
    do{
        CStdRMutex oIfLock( GetLock() );
        if( m_bHandshaked )
        {
            TaskletPtr pTask = m_pHsTicker;

            if( m_bHsFailed )
                ret = ERROR_FAIL;

            if( !m_pHsTicker.IsEmpty() )
            {
                m_pHsTicker.Clear();
                oIfLock.Unlock();
                DebugPrint( 0,
                    "Deferred the handshake "
                    "response till Start completes" );
                pTask->OnEvent(
                    eventTaskComp, ret, 0, nullptr );
            }
            break;
        }

        DebugPrint( 0, "start the handshake timer" );

        // set the session checker
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, m_pHsTicker, ObjPtr( this ),
            &CRpcTcpBridge::CheckHsTimeout,
            nullptr, pCallback );

        if( ERROR( ret ) )
            break;

        CIfRetryTask* pRetryTask = m_pHsTicker;
        pHsTicker = m_pHsTicker;

        CCfgOpenerObj oTaskCfg( pRetryTask );

        oTaskCfg.SetIntProp(
            propRetries, 3 );

        oTaskCfg.SetIntProp(
            propIntervalSec, 20 );

        CIoManager* pMgr = GetIoMgr();
        ret = pMgr->RescheduleTask( pHsTicker );

        if( ERROR( ret ) )
            break;

        ret = STATUS_PENDING;

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        if( !pHsTicker.IsEmpty() )
            ( *pHsTicker )( eventCancelTask );

        pCallback->OnEvent(
            eventTaskComp, ret, 0, nullptr );
    }

    return ret;
}

gint32 CRpcTcpBridge::Handshake(
    IEventSink* pCallback,
    IConfigDb* pInfo )
{
    if( pCallback == nullptr ||
        pInfo == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CParamList oResp;
    do{
        CStdRMutex oIfLock( GetLock() );
        if( m_bHandshaked == true )
        {
            ret = ERROR_STATE;
            return ret;
        }

        m_bHandshaked = true;
        CCfgOpener oInfo( pInfo );
        std::string strHs;
        ret = oInfo.GetStrProp( 0, strHs );
        if( ERROR( ret ) ||
            strHs != BRIDGE_PROXY_GREETINGS )
        {
            // notify to actively fail the start
            // sequence
            m_bHsFailed = true;
            ret = ERROR_FAIL;
        }
        else
        {
            timespec tv;
            clock_gettime( CLOCK_MONOTONIC, &tv );
            guint64 qwTs = tv.tv_sec;
            CParamList oParams;
            oParams.Push( BRIDGE_GREETINGS );
            oParams[ propTimestamp ] = qwTs;

            oResp.Push( oParams.GetCfg() );
            m_oTs.SetBase( qwTs );

            SetResponse(
                pCallback, oResp.GetCfg() );

            if( IsRfcEnabled() )
            {
                oParams.CopyProp( 
                    propMaxReqs, this );

                oParams.CopyProp( 
                    propMaxPendings, this );
            }
        }

        if( IsRfcEnabled() )
        {
            CCfgOpener oCfg;
            gint32 iRet = oCfg.CopyProp(
                propMaxReqs, this );
            if( ERROR( iRet ) )
            {
                oCfg.SetIntProp(
                    propMaxReqs, RFC_MAX_REQS );
            }
            iRet = oCfg.CopyProp(
                propMaxPendings, this );
            if( ERROR( iRet ) )
            {
                oCfg.SetIntProp(
                    propMaxPendings,
                    RFC_MAX_PENDINGS );
            }

            iRet = InitRfc( oCfg );
            if( ERROR( iRet ) )
            {
                // fatal error
                m_bHsFailed = true;
                if( SUCCEEDED( ret ) )
                    ret = iRet;
            }
        }

        oResp[ propReturnValue ] = ret;
        SetResponse( pCallback, oResp.GetCfg() );

        if( !m_pHsTicker.IsEmpty() )
        {
            // the ticker can now be completed by
            // revisiting CheckHsTimeout
            TaskletPtr pTask = m_pHsTicker;
            CIfParallelTask* ppt = pTask;
            m_pHsTicker.Clear();
            EnumTaskState iState =
                ppt->GetTaskState();
            oIfLock.Unlock();
            CStdRTMutex oTaskLock( ppt->GetLock() );
            if( iState == stateStopped )
            {
                // the handshake window has closed,
                // but the bridge is not down yet
                m_bHsFailed = true;
                ret = -ETIMEDOUT;
                oResp[ propReturnValue ] = -ETIMEDOUT;
                oResp.RemoveProperty( 0 );
            }
            else
            {
                ( *pTask )( eventZero );
            }
        }
        else
        {
            // wait till DoStartHandshake
            ret = STATUS_PENDING;
            m_pHsTicker = ObjPtr( pCallback );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::PostDisconnEvent()
{
    gint32 ret = 0;
    do{
        CCfgOpener oReqCtx;
        oReqCtx[ propRouterPath ] = "/";
        oReqCtx.CopyProp( propConnHandle,
            propPortId, this );

        PortPtr pPort;
        GET_TARGET_PORT( pPort ); 
        if( ERROR( ret ) )
            break;

        BufPtr pBuf( true );
        ret = GetPortProp( pPort,
            propPdoPtr, pBuf );
        if( ERROR( ret ) )
            break;

        IPort* pPdo = ( ObjPtr& )*pBuf;
        if( unlikely( pPdo == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        IConfigDb* pReqCtx = oReqCtx.GetCfg();
        CRpcRouter* pRouter = GetParent();
        // stop the underlying port
        pRouter->OnEvent( eventRmtSvrOffline,
            ( LONGWORD )pReqCtx, 0,
            ( LONGWORD* )PortToHandle( pPdo ) );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridge::OnPostStop(
    IEventSink* pCallback )
{
    if( IsRfcEnabled() )
        m_pGrpRfc.Clear();

    return 0;
}

gint32 CRpcTcpBridge::RefreshReqLimit(
    guint32 dwMaxReqs,
    guint32 dwMaxPendigns )
{
    gint32 ret = 0;
    do{
        CIfParallelTaskGrpRfc* pGrpRfc =
            m_pGrpRfc;

        pGrpRfc->SetLimit(
            dwMaxReqs, dwMaxPendigns );

    }while( 0 );

    return ret;
}
 
gint32 CRpcTcpBridge::GenSessHash(
    std::string& strSess ) const
{
    gint32 ret = 0;

    do{
        BufPtr pBuf( true );

        CCfgOpenerObj oIfCfg( this );
        IConfigDb* pConn;
        ret = oIfCfg.GetPointer(
            propConnParams, pConn );
        if( ERROR( ret ) )
            break;

        ret = AppendConnParams( pConn, pBuf );
        if( ERROR( ret ) )
            break;

        guint64 qwSalt = this->GetObjId();
        pBuf->Append( ( char* )&qwSalt,
            sizeof( qwSalt ) );

        stdstr strRet;
        ret = gen_sess_hash( pBuf, strRet );
        if( ERROR( ret ) )
            break;

        strSess = "NA"; // non-auth session
        strSess += strRet;

    }while( 0 );

    return ret;
}

CRpcTcpBridgeShared::CRpcTcpBridgeShared(
    CRpcServices* pIf )
{
    gint32 ret = 0;
    do{
        if( pIf == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        m_pParentIf = pIf;

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error occurs in CRpcTcpBridgeShared ctor" );
        throw runtime_error( strMsg );
    }
}

gint32 CRpcTcpBridgeShared::GetPortToSubmitShared(
    CObjBase* pCfg,
    PortPtr& pTarget,
    bool& bPdo )
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        PortPtr ptrPort = m_pParentIf->GetPort();
        CPort* pPort = ptrPort;
        if( pPort == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = m_pParentIf->CRpcServices::
            GetPortToSubmit( pCfg, pTarget, bPdo );

        std::string strPortClass;
        CCfgOpenerObj oPortCfg( pPort );

        ret = oPortCfg.GetStrProp(
            propPdoClass, strPortClass );
        if( ERROR( ret ) )
            break;

        if( strPortClass ==
            PORT_CLASS_TCP_STREAM_PDO )
            break;

        if( strPortClass !=
            PORT_CLASS_TCP_STREAM_PDO2 )
        {
            ret = -ENOTSUP;
            break;
        }

        if( !bPdo )
        {
            // pTarget is the desired port.
            break;
        }
        else
        {
            CCfgOpenerObj oCfg( pCfg );

            HANDLE hPort = INVALID_HANDLE;
            ret = oCfg.GetIntPtr( propSubmitTo,
                ( guint32*& )hPort );

            // pTarget holds the desired port.
            if( SUCCEEDED( ret ) )
                break;

            // for TcpStreamPdo2, the fdo has
            // the interface rather than pdo
            ret = pPort->GetFdoPort( pTarget );

            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::OpenLocalStream(
    IPort* pPort,
    gint32 iStreamId,
    guint32 dwProtoId,
    gint32& iStmIdOpened )
{
    if( pPort == nullptr )
        return -EINVAL;
        
    gint32 ret = 0;
    do{
        // start to read from the default
        // dbus stream
        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        if( IsReserveStm( iStreamId ) )
            return -EINVAL;

        IrpCtxPtr pCtx = pIrp->GetTopStack(); 

        pCtx->SetMajorCmd( IRP_MJ_FUNC );
        pCtx->SetMinorCmd( IRP_MN_IOCTL );
        pCtx->SetCtrlCode( CTRLCODE_OPEN_STREAM_LOCAL_PDO );
        pCtx->SetIoDirection( IRP_DIR_INOUT ); 

        BufPtr pBuf( true );
        CParamList oReq;

        oReq.Push( iStreamId ); // 0: peer stm id
        oReq.Push( dwProtoId ); // 1: protocol

        *pBuf = ObjPtr( 
            ( IConfigDb* )oReq.GetCfg() );

        pCtx->SetReqData( pBuf );

        // NOTE: this irp will only go through the
        // stream pdo, and not come down all the
        // way from the top of the port stack
        CPort* pPort2 = ( CPort* )pPort;
        CIoManager* pMgr = pPort2->GetIoMgr();
        ret = pMgr->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ERROR( ret ) )
            break;

        if( SUCCEEDED( ret ) )
        {
            ret = 0;
            pCtx = pIrp->GetTopStack();

            CfgPtr pCfg;
            ret = pCtx->GetRespAsCfg( pCfg );
            if( ERROR( ret ) )
                break;

            CCfgOpener oCfg( ( IConfigDb* )pCfg );
            gint32 iRet = 0;

            ret = oCfg.GetIntProp(
                propReturnValue, ( guint32& )iRet );

            if( ERROR( ret ) )
                break;
            
            if( ERROR( iRet ) )
            {
                ret = iRet;
                break;
            }

            gint32 iNewStmId = 0;
            ret = oCfg.GetIntProp(
                0, ( guint32& )iNewStmId ); // 0: stream opened
            
            if( ERROR( ret ) )
                break;

            iStmIdOpened = iNewStmId;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::CloseLocalStream(
    IPort* pPort, gint32 iStreamId )
{
    if( iStreamId < 0 )
        return -EINVAL;

    if( IsReserveStm( iStreamId ) )
        return -EINVAL;

    gint32 ret = 0;
    PortPtr pTargetPort;
    if( pPort == nullptr )
    {
        GET_TARGET_PORT_SHARED( pTargetPort );
        if( ERROR( ret ) )
            return ret;

        pPort = pTargetPort;
        if( unlikely( pPort == nullptr ) )
            return ERROR_FAIL;
    }
    else
    {
        pTargetPort = pPort;
    }
        
    do{
        // start to read from the default
        // dbus stream
        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr pCtx = pIrp->GetTopStack(); 

        pCtx->SetMajorCmd( IRP_MJ_FUNC );
        pCtx->SetMinorCmd( IRP_MN_IOCTL );
        pCtx->SetCtrlCode( CTRLCODE_CLOSE_STREAM_LOCAL_PDO );
        pCtx->SetIoDirection( IRP_DIR_OUT ); 

        BufPtr pBuf( true );

        // a local request, no need of CReqBuilder
        CParamList oParams;
        ret = oParams.Push( iStreamId );

        ObjPtr pObj = oParams.GetCfg();
        *pBuf = pObj;

        pCtx->SetReqData( pBuf );

        CPort* pPort2 = ( CPort* )pPort;
        CIoManager* pMgr = pPort2->GetIoMgr();
        pIrp->SetSyncCall( true );

        // NOTE: this irp will only go through the
        // stream pdo, and not come down all the
        // way from the top of the port stack
        ret = pMgr->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ret == ERROR_STATE )
            break;

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::ReadStream(
    gint32 iStreamId,
    BufPtr& pDestBuf,
    guint32 dwSizeToRead,
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    return ReadWriteStream( iStreamId,
        pDestBuf, dwSizeToRead, pCallback, true );
}

gint32 CRpcTcpBridgeShared::WriteStream(
    gint32 iStreamId,
    CBuffer* pSrcBuf,
    guint32 dwSizeToWrite,
    IEventSink* pCallback )
{
    if( pSrcBuf == nullptr || pSrcBuf->empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        bool bSync = false;
        TaskletPtr pCbTask;
        if( pCallback == nullptr )
        {
            bSync = true;
            ret = pCbTask.NewObj(
                clsid( CSyncCallback ) );

            if( ERROR( ret ) )
                break;

            pCallback = pCbTask;
        }

        BufPtr pBuf( pSrcBuf );
        ret = ReadWriteStream( iStreamId,
            pBuf, dwSizeToWrite,
            pCallback, false );

        if( ret == STATUS_PENDING && bSync )
        {
            CSyncCallback* pSyncCb = pCbTask;
            pSyncCb->WaitForComplete();
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::ReadWriteStream(
    gint32 iStreamId,
    BufPtr& pSrcBuf,
    guint32 dwSize,
    IEventSink* pCallback,
    bool bRead )
{
    gint32 ret = 0;
    TaskletPtr pCbTask;

    if( pCallback == nullptr )
        return -EINVAL;

    do{
        CCfgOpener oCfg;
        CRpcInterfaceBase* pIf = GetIfPtr();
        if( pIf == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        guint32 dwCmdId;
        guint32 dwIoDir;

        if( bRead )
        {
            dwCmdId = IRP_MN_READ;
            dwIoDir = IRP_DIR_IN;
        }
        else
        {
            dwCmdId = IRP_MN_WRITE;
            dwIoDir = IRP_DIR_OUT;
        }

        oCfg.SetObjPtr( propIfPtr,
            ObjPtr( GetIfPtr() ) );

        oCfg.SetObjPtr( propEventSink,
            ObjPtr( pCallback ) );

        oCfg.SetIntProp( propCmdId, dwCmdId );

        ret = pCbTask.NewObj(
            clsid( CBdgeProxyReadWriteComplete ),
            oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        // activatate the task to a intial state
        // without actual Read/Write
        ( *pCbTask )( eventZero );

        guint32 dwTimeoutSec = 0;

        CCfgOpenerObj oIfCfg( pIf ); 

        ret = oIfCfg.GetIntProp(
            propTimeoutSec, dwTimeoutSec );

        if( ERROR( ret ) )
            dwTimeoutSec = IFSTATE_DEFAULT_IOREQ_TIMEOUT;

        // start to read from the default
        // dbus stream
        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        if( iStreamId == TCP_CONN_DEFAULT_CMD )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr pCtx = pIrp->GetTopStack(); 

        pCtx->SetMajorCmd( IRP_MJ_FUNC );
        pCtx->SetMinorCmd( dwCmdId );
        pCtx->SetIoDirection( dwIoDir ); 

        BufPtr pBuf( true );
        CParamList oReq;

        oReq.SetIntProp(
            propStreamId, iStreamId );

        if( bRead )
        {
            *pBuf = ObjPtr( 
                ( IConfigDb* )oReq.GetCfg() );
        }
        else
        {
            dwSize = std::min(
                dwSize, pSrcBuf->size() );

            if( dwSize > MAX_BYTES_PER_TRANSFER )
            {
                ret = -E2BIG;
                break;
            }

            oReq.SetIntProp(
                propByteCount, dwSize );

            oReq.Push( pSrcBuf );
            *pBuf = ObjPtr( 
                ( IConfigDb* )oReq.GetCfg() );
        }

        pCtx->SetReqData( pBuf );

        EventPtr pEvent = pCbTask;
        pIrp->SetCallback( pEvent, 0 );
        pIrp->SetIrpThread( pIf->GetIoMgr() );
        if( dwTimeoutSec != 0 )
        {
            pIrp->SetTimer(
                dwTimeoutSec, pIf->GetIoMgr() );
        }

        // NOTE: this irp will only go through the
        // stream pdo, and not come down all the
        // way from the top of the port stack
        HANDLE hPort = pIf->GetPortHandle();
        CIoManager* pMgr = pIf->GetIoMgr();

        CCfgOpenerObj oCbCfg( ( CObjBase* )pCbTask  );
        oCbCfg.SetObjPtr( propIrpPtr, pIrp );

        ret = pMgr->SubmitIrp( hPort, pIrp );

        if( ret == STATUS_PENDING )
        {
            break;
        }
        else
        {
            if( SUCCEEDED( ret ) && bRead )
            {
                IrpCtxPtr pCtx = pIrp->GetTopStack();
                pSrcBuf = pCtx->m_pRespData;
            }
            oCbCfg.RemoveProperty( propIrpPtr );
            pIrp->RemoveCallback();
            pIrp->RemoveTimer();
            pIrp->ClearIrpThread();
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::SetupReqIrpListeningShared(
    IRP* pIrp,
    IConfigDb* pReqCall,
    IEventSink* pCallback )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pCallback == nullptr ||
        pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        InterfPtr pIf = GetIfPtr();
        if( pIf.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        CCfgOpenerObj oCbCfg( pCallback );
        IConfigDb* pExtInfo = nullptr;
        ret = oCbCfg.GetPointer(
            propExtInfo, pExtInfo );

        if( ERROR( ret ) )
        {
            // for non-stream listening or default
            // stream listening
            ret = -ENOTSUP;
            break;
        }

        CCfgOpener oExtInfo( pExtInfo );
        bool bPdo = false;
        ret = oExtInfo.GetBoolProp(
            propSubmitPdo, bPdo );

        if( !bPdo )
        {
            // the irps for the request to the tcpfido
            // will be built at other place
            ret = -ENOTSUP;
            break;
        }

        CIoManager* pMgr =
            m_pParentIf->GetIoMgr();

        guint32 dwStreamId = -1;
        ret = oExtInfo.GetIntProp(
            propStreamId, dwStreamId );

        if( ERROR( ret ) )
            break;

        CReqBuilder oReq( pReqCall );

        oReq.SetIntProp( propStreamId, dwStreamId );
        oReq.CopyProp( propMatchPtr, pCallback );
        oCbCfg.SetBoolProp( propSubmitPdo, bPdo );

        oReq.SetCallFlags( CF_WITH_REPLY |
            CF_NON_DBUS | CF_ASYNC_CALL |
            DBUS_MESSAGE_TYPE_METHOD_CALL );

        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack();

        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
        pIrpCtx->SetCtrlCode( CTRLCODE_LISTENING );

        pIrpCtx->SetNonDBusReq( true );

        BufPtr pBuf( true );
        *pBuf = ObjPtr( pReqCall );

        pIrpCtx->SetReqData( pBuf );
        pIrpCtx->SetIoDirection( IRP_DIR_IN );
        pIrp->SetCallback( pCallback, 0 );
        pIrp->SetIrpThread( pMgr );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::RegMatchCtrlStream(
        gint32 iStreamId,
        MatchPtr& pMatch,
        bool bReg )
{
    if( iStreamId < 0 || pMatch.IsEmpty() )
        return -EINVAL;
        
    gint32 ret = 0;
    do{
        // start to read from the default
        // dbus stream
        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        guint32 dwCtrlCode = CTRLCODE_REG_MATCH;
        if( !bReg )
            dwCtrlCode = CTRLCODE_UNREG_MATCH;

        IrpCtxPtr pCtx = pIrp->GetTopStack(); 

        pCtx->SetMajorCmd( IRP_MJ_FUNC );
        pCtx->SetMinorCmd( IRP_MN_IOCTL );
        pCtx->SetCtrlCode( dwCtrlCode );
        pCtx->SetIoDirection( IRP_DIR_OUT ); 

        BufPtr pBuf( true );

        // a local request, no need of CReqBuilder
        CReqBuilder oParams;

        ret = oParams.SetIntProp(
            propStreamId, iStreamId );

        oParams[ propMatchPtr ] = ObjPtr( pMatch );
        oParams.SetCallFlags( CF_WITH_REPLY
           | DBUS_MESSAGE_TYPE_METHOD_CALL 
           | CF_ASYNC_CALL );

        ObjPtr pObj = oParams.GetCfg();
        *pBuf = pObj;
        pCtx->SetReqData( pBuf );

        PortPtr pPort;
        GET_TARGET_PORT_SHARED( pPort );
        if( ERROR( ret ) )
            break;

        CIoManager* pMgr = m_pParentIf->GetIoMgr();
        pIrp->SetSyncCall( true );

        // NOTE: this irp will only go through the
        // stream pdo, and not come down all the
        // way from the top of the port stack
        ret = pMgr->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ret == ERROR_STATE )
            break;

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::GetPeerStmId(
    gint32 iStmId,
    gint32& iPeerStmid )
{
    gint32 ret = 0;
    do{
        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pNewCtx = pIrp->GetTopStack();
        pNewCtx->SetMajorCmd( IRP_MJ_FUNC );
        pNewCtx->SetMinorCmd( IRP_MN_IOCTL );

        pNewCtx->SetCtrlCode(
            CTRLCODE_GET_RMT_STMID );

        pNewCtx->SetIoDirection( IRP_DIR_INOUT );

        CParamList oParams;
        oParams.Push( iStmId );
        BufPtr pBuf( true );
        *pBuf = ObjPtr( oParams.GetCfg() );
        pNewCtx->SetReqData( pBuf );

        PortPtr pPort;
        CIoManager* pMgr =
            m_pParentIf->GetIoMgr();

        HANDLE hPort =
            m_pParentIf->GetPortHandle();

        ret = pMgr->GetPortPtr( hPort, pPort );
        if( ERROR( ret ) )
            break;

        ret = pMgr->SubmitIrp( hPort,
            pIrp, false );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeShared::StartExOrig(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;
    
    return m_pParentIf->
        CRpcServices::StartEx2( pCallback );
}

gint32 CRpcTcpBridgeShared::StartHandshake(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    if( m_pParentIf->IsServer() )
    {
        CRpcTcpBridge* pIf =
            ObjPtr( m_pParentIf );
        ret = pIf->DoStartHandshake(
            pCallback );
    }
    else
    {
        CRpcTcpBridgeProxy* pIf =
            ObjPtr( m_pParentIf );
        ret = pIf->DoStartHandshake(
            pCallback );
    }
    return ret;
}

gint32 CRpcTcpBridgeShared::StartEx2(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    TaskGrpPtr pTaskGrp;
    do{
        TaskletPtr pStart;
        TaskletPtr pHandshake;

        CParamList oParams;
        oParams[ propIfPtr ] =
            ObjPtr( m_pParentIf );

        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        pTaskGrp->SetRelation( logicAND );
        pTaskGrp->SetClientNotify( pCallback );

        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pStart , ObjPtr( m_pParentIf ),
            &CRpcTcpBridgeShared::StartExOrig,
            nullptr );
        if( ERROR( ret ) )
            break;
        pTaskGrp->AppendTask( pStart );

        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pHandshake, ObjPtr( m_pParentIf ),
            &CRpcTcpBridgeShared::StartHandshake,
            nullptr );
        if( ERROR( ret ) )
            break;

        pTaskGrp->AppendTask( pHandshake );
        TaskletPtr pGrp = ObjPtr( pTaskGrp );
        CIoManager* pMgr = m_pParentIf->GetIoMgr();
        ret = pMgr->RescheduleTask( pGrp );
        if( ERROR( ret ) )
        {
            ( *pTaskGrp )( eventCancelTask );
        }
        else 
        {
            ret = STATUS_PENDING;
        }

    }while( 0 );

    if( ERROR( ret ) )
    {
        if( !pTaskGrp.IsEmpty() )
            ( *pTaskGrp )( eventCancelTask );
    }

    return ret;
}

}
