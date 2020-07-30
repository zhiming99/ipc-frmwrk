/*
 * =====================================================================================
 *
 *       Filename:  security.cpp
 *
 *    Description:  implementation of authentication related classes 
 *
 *        Version:  1.0
 *        Created:  06/10/2020 11:32:32 PM
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
#include "proxy.h"
#include "rpcroute.h"
#include "security.h"
#include "k5proxy.h"

gint32 CRpcTcpBridgeAuth::OnLoginTimeout(
    IEventSink* pCallback,
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pIoReq == nullptr )
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
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( iRet != ERROR_TIMEOUT )
        {
            ret = iRet;
            break;
        }

        OnLoginFailed( pCallback );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::StartLoginTimer(
    IEventSink* pCallback,
    guint32 dwTimeoutSec )
{
    gint32 ret = 0;
    do{
        CIfDeferCallTaskEx* pTask =
            ObjPtr( pCallback );
        if( pTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = pTask->EnableTimer( dwTimeoutSec );
        if( ERROR( ret ) )
            break;

        ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::OnPostStart(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )    
        return -EINVAL;

    gint32 ret = 0;
    do{
        TaskletPtr pRespCb;
        ret = NEW_PROXY_RESP_HANDLER2(
            pRespCb, ObjPtr( this ),
            &CRpcTcpBridgeAuth::OnLoginTimeout,
            nullptr, nullptr );
        if( ERROR( ret ) )
            break;

        TaskletPtr pTask;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pTask, ObjPtr( this ),
            &CRpcTcpBridgeAuth::StartLoginTimer,
            nullptr, 0 );

        if( ERROR( ret ) )
        {
            ( *pRespCb )( eventCancelTask );
            break;
        }

        CIfRetryTask* pRetryTask = pTask;
        pRetryTask->SetClientNotify( pRespCb );
        ret = RunManagedTask( pTask );
        if( SUCCEEDED( ret ) )
            ret = pTask->GetError();

        if( ERROR( ret ) )
        {
            ( *pTask )( eventCancelTask );
            ( *pRespCb )( eventCancelTask );
        }

        if( ret == STATUS_PENDING )
        {
            m_pLoginTimer = pTask;
            ret = 0;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::OnEnableComplete(
    IEventSink* pCallback,
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pIoReq == nullptr )
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
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        CCfgOpener oReqCtx( pReqCtx );
        CStlObjVector* pVec;
        ret = oReqCtx.GetPointer( 0, pVec );
        if( ERROR( ret ) )
            break;

        std::vector< MatchPtr > vecMatches;
        for( auto elem : ( *pVec )() )
        {
            MatchPtr pMatch = elem;
            vecMatches.push_back( pMatch );
        }

        ret = StartRecvTasks( vecMatches );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::EnableInterfaces()
{
    gint32 ret = 0;
    TaskGrpPtr pTaskGrp;
    TaskletPtr pRespCb;
    do{
        if( IsKdcChannel() )
        {
            ret = 0;
            break;
        }
        std::vector< MatchPtr > vecMatches;
        for( auto& elem : m_vecMatches )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )elem );
            std::string strIfName;
            bool bDummy = true;
            ret = oMatch.GetBoolProp(
                propDummyMatch, bDummy );
            if( ERROR( ret ) )
                continue;

            if( bDummy )
            {
                oMatch.RemoveProperty(
                    propDummyMatch );
                vecMatches.push_back( elem );
            }
        }

        if( vecMatches.empty() )
            break;

        CParamList oParams;            
        oParams[ propIfPtr ] = ObjPtr( this );

        pTaskGrp->SetRelation( logicAND );

        ret = pTaskGrp.NewObj( 
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ObjVecPtr pObjVec;
        ret = pObjVec.NewObj(
            clsid( CStlObjVector ) );

        if( ERROR( ret ) )
            break;

        for( auto elem : vecMatches )
        {
            oParams[ propMatchPtr ] =
                ObjPtr( elem );

            TaskletPtr pEnableEvtTask;
            ret = pEnableEvtTask.NewObj(
                clsid( CIfEnableEventTask ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            pTaskGrp->AppendTask(
                pEnableEvtTask );

            ( *pObjVec )().push_back(
                ObjPtr( elem ) );
        }

        CParamList oReqCtx;
        oReqCtx.Push( ObjPtr( pObjVec ) );
        ret = NEW_PROXY_RESP_HANDLER2(
            pRespCb, ObjPtr( this ),
            &CRpcTcpBridgeAuth::OnEnableComplete,
            nullptr,
            ( IConfigDb* )oReqCtx.GetCfg() );

        if( ERROR( ret ) )
            break;

        pTaskGrp->SetClientNotify(
            ( IEventSink* )pRespCb );

        TaskletPtr pTask = pTaskGrp;
        ret = AddAndRun( pTask );

    }while( 0 );

    if( !ERROR( ret ) )
        return ret;

    if( !pTaskGrp.IsEmpty() )
        ( *pTaskGrp )( eventCancelTask );

    if( pRespCb.IsEmpty() )
        ( *pRespCb )( eventCancelTask );

    return ret;
};

gint32 CRpcTcpBridgeAuth::OnPostStop(
    IEventSink* pCallback )
{
    return SetSessHash( "", false );
}

gint32 CRpcTcpBridgeAuth::SetSessHash(
    const std::string& strHash,
    bool bNoEnc )
{
    // notify the bridge the authentication
    // context is setup.
    gint32 ret = 0;
    do{
        CRpcRouterBridgeAuthImpl* pRouter =
            ObjPtr( GetParent() );

        std::string strHash1;
        CCfgOpenerObj oIfCfg( this );
        ret = oIfCfg.GetStrProp(
            propSessHash, strHash1 );
        if( SUCCEEDED( ret ) )
        { 
            if( !strHash.empty() )
            {
                ret = -EEXIST;
                break;
            }

            oIfCfg.RemoveProperty( propSessHash );
            m_oSecCtx.Clear();

            // clear the the session
            CAuthentServer* pAs;
            pAs = ObjPtr( pRouter );

            pAs->RemoveSession( strHash1 );
            if( m_pLoginTimer.IsEmpty() )
            {
                // at this point, the
                // m_pLoginTimer is already
                // canceled.
                m_pLoginTimer.Clear();
            }
            break;
        }

        ret = 0;
        if( strHash.empty() )
            break;

        ObjPtr pAuthImpl;
        ret = pRouter->GetAuthImpl( pAuthImpl );
        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        CCfgOpener& oCtx = m_oSecCtx;

        if( !bNoEnc )
        {
            oCtx.SetObjPtr(
                propObjPtr, pAuthImpl );
        }

        oCtx.SetBoolProp( propNoEnc, bNoEnc );
        oCtx.SetBoolProp( propIsServer, true );
        oCtx.SetStrProp( propSessHash, strHash );

        PortPtr pPort;
        GET_TARGET_PORT( pPort );
        if( ERROR( ret ) )
            break;

        BufPtr pBuf( true );
        bool bFound = false;
        while( !pPort.IsEmpty() )
        {
            ret = GetPortProp( pPort,
                propLowerPortPtr, pBuf );    
            if( ERROR( ret ) )
            {
                ret = 0;
                break;
            }

            pPort = ( ObjPtr& )*pBuf;
            if( pPort.IsEmpty() )
                break;

            ret = GetPortProp( pPort,
                propPortClass, pBuf );
            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }
            std::string strClass = *pBuf;
            if( strClass != PORT_CLASS_SEC_FIDO )
                continue;

            bFound = true;
            break;
        }

        if( !bFound )
        {
            oCtx.Clear();
            break;
        }

        oIfCfg.SetStrProp( propSessHash, strHash);

        *pBuf = ObjPtr( m_oSecCtx.GetCfg() );
        SetPortProp( pPort, propSecCtx, pBuf );

        // stop the login timer
        if( !m_pLoginTimer.IsEmpty() )
        {
            ( *m_pLoginTimer )( eventCancelTask );
            m_pLoginTimer.Clear();
        }

        // start all the rest interfaces,
        // espacially the stream interfaces
        EnableInterfaces();

    }while( 0 );

    return ret;
}


gint32 CRpcTcpBridgeAuth::IsAuthRequest(
    IConfigDb* pReqCtx,
    DBusMessage* pDMsg )
{
    if( pReqCtx == nullptr || pDMsg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        DMsgPtr pMsg( pDMsg );
        std::string strVal = CoGetIfNameFromIid(
            iid( IAuthenticate ), "s" );
        strVal = DBUS_IF_NAME( strVal );
        if( pMsg.GetInterface() != strVal )
        {
            ret = ERROR_FALSE;
            break;
        }

        CIoManager* pMgr = GetIoMgr();
        strVal = AUTH_DEST( pMgr );
        if( strVal != pMsg.GetDestination() )
        {
            ret = -EACCES;
            break;
        }

        CCfgOpener oReqCtx( pReqCtx );
        ret = oReqCtx.GetStrProp(
            propRouterPath, strVal );
        if( ERROR( ret ) )
        {
            ret = ERROR_FALSE;
            break;
        }

        if( strVal != "/" )
        {
            ret = -EACCES;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::OnLoginFailed(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{

        CRpcRouter* pRouter = GetParent();
        if( !m_pLoginTimer.IsEmpty() )
            ( *m_pLoginTimer )( eventCancelTask );

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
        // stop the tcp port
        pRouter->OnEvent( eventRmtSvrOffline,
            PortToHandle( pPdo ),
            0, ( LONGWORD* )pReqCtx );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::OnLoginComplete(
    IEventSink* pCallback,
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pIoReq == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );

        if( ERROR( ret ) )
            break;

        // FIXME: we cannot 100% guarantee that
        // the request is sent immediately. So if
        // the request is pending on a port above
        // the secfido, the request could be
        // encrypted and the client will fill the
        // login because of unable to decypt the
        // response message. However, at this
        // point, there is no more traffic at
        // login stage, and most likely the login
        // response can be sent without encrypted.
        //
        OnServiceComplete( pResp, pCallback );

        gint32 iRet = 0;
        CCfgOpener oResp( pResp );
        ret = oReq.GetIntProp(
            propRespPtr, ( guint32& )iRet );

        if( ERROR( ret ) || ERROR( iRet ) )
            break;

        // this is a forward-request, we need to
        // unwrap the response message first.
        DMsgPtr pRespMsg;
        ret = oResp.GetMsgPtr( 0, pRespMsg );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = pRespMsg.GetObjArgAt( 0, pObj );
        if( ERROR( ret ) )
            break;

        IConfigDb* pLoginResp = pObj;
        if( pLoginResp == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CCfgOpener oLoginResp( pLoginResp );
        bool bContinue = false;
        ret = oLoginResp.GetBoolProp(
            propContinue, bContinue );
        if( ERROR( ret ) )
            break;

        // the login is not complete yet.
        if( bContinue )
            break;

        // login is done.
        CRpcRouterBridgeAuthImpl* pRouter =
            ObjPtr( GetParent() );

        ObjPtr pAuthImpl;
        ret = pRouter->GetAuthImpl( pAuthImpl );
        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        CCfgOpenerObj oIfCfg( this );
        guint32 dwPortId = 0;
        ret = oIfCfg.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        CAuthentServer* pAuth =
        dynamic_cast< CAuthentServer* >
            ( this );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        std::string strSess;
        ret = pAuth->GetSess( dwPortId, strSess );
        if( ERROR( ret ) )
            break;
 
        bool bNoEnc = false;
        ret = pAuth->IsNoEnc( strSess );
        if( SUCCEEDED( ret ) )
            bNoEnc = true;

        ret = SetSessHash( strSess, bNoEnc );

    }while( 0 );

    if( ERROR( ret ) )
    {
        DEFER_CALL_DELAY( GetIoMgr(), 1, 
            ObjPtr( this ),
            &CRpcTcpBridgeAuth::OnLoginFailed,
            nullptr );
    }

    return ret;
}

// add a session hash to the pFwdrMsg
gint32 CRpcTcpBridgeAuth::ForwardRequest(
    IConfigDb* pReqCtx,
    DBusMessage* pFwdrMsg,
    DMsgPtr& pRespMsg,
    IEventSink* pCallback )
{
    if( pReqCtx == nullptr ||
        pFwdrMsg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpenerObj oIfCfg( this );
        DMsgPtr pMsg( pFwdrMsg );
        std::string strHash;

        bool bAuthed = false;
        ret = oIfCfg.GetStrProp(
            propSessHash, strHash );

        if( SUCCEEDED( ret ) )
            bAuthed = true;

        ret = IsAuthRequest(
            pReqCtx, pFwdrMsg );

        if( ret == -EACCES )
            break;

        if( SUCCEEDED( ret ) )
        {
            bool bLogin = false;
            TaskletPtr pRespCb;
            std::string strMember =
                SYS_METHOD( AUTH_METHOD_LOGIN );

            if( pMsg.GetMember() == strMember )
                bLogin = true;

            if( !bAuthed )
            {
                if( !bLogin )
                {
                    ret = -EACCES;
                    break;
                }
                ret = NEW_PROXY_RESP_HANDLER2(
                    pRespCb, ObjPtr( this ),
                    &CRpcTcpBridgeAuth::OnLoginComplete,
                    pCallback, pReqCtx );

                if( ERROR( ret ) )
                    break;

                pCallback = pRespCb;
            }
            else
            {
                if( bLogin )
                {
                    // duplicated login not
                    // allowed
                    ret = -EACCES;
                    break;
                }
                std::string strMember =
                    AUTH_METHOD_MECHSPECREQ;
                if( pMsg.GetMember() !=
                    SYS_METHOD( strMember ) )
                {
                    ret = -EACCES;
                    break;
                }
            }

            ret = super::ForwardRequest(
                pReqCtx, pFwdrMsg,
                pRespMsg, pCallback );

            if( ERROR( ret ) &&
                !pRespCb.IsEmpty() )
            {
                ( *pRespCb )( eventCancelTask );
            }
            break;
        }

        // not an authentication request
        if( !bAuthed )
        {
            ret = -EACCES;
            break;
        }
        // append a session hash for access
        // control
        CCfgOpener oReqCtx;
        oReqCtx.SetStrProp(
            propSessHash, strHash );

        oReqCtx.CopyProp(
            propRouterPath, pReqCtx );

        DMsgPtr pNewMsg;
        pNewMsg.CopyHeader( pFwdrMsg );
        BufPtr pArg( true );
        gint32 iType = 0;
        ret = pMsg.GetArgAt( 0, pArg, iType );

        BufPtr pCtxBuf( true );
        ret = oReqCtx.Serialize( pCtxBuf );
        if( ERROR( ret ) )
            break;

        const char* pData = pArg->ptr();
        const char* pCtx = pCtxBuf->ptr();
        if( !dbus_message_append_args(pNewMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pArg->size(),
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

        ret = super::ForwardRequest(
                pReqCtx, pNewMsg,
                pRespMsg, pCallback );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::DoInvoke(
    DBusMessage* pReqMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;    
    do{
        if( !IsKdcChannel() )
        {
            ret = super::DoInvoke(
                pReqMsg, pCallback );
            break;
        }

        DMsgPtr pMsg( pReqMsg );
        CParamList oResp;
        std::string strMethod = pMsg.GetMember();
        if( strMethod != SYS_METHOD_FORWARDREQ )
        {
            ret = -EACCES;
            break;
        }

        // further filter will be done in
        // ForwardRequest
        ret = super::DoInvoke(
            pReqMsg, pCallback );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeAuth::DoInvoke(
    IConfigDb* pEvtMsg,
    IEventSink* pCallback )
{
    if( IsKdcChannel() )
        return -EACCES;

    return CInterfaceServer::DoInvoke(
        pEvtMsg, pCallback );
}

gint32 CRpcTcpBridgeAuth::FetchData_Server(
    IConfigDb* pDataDesc,           // [in]
    gint32& fd,                     // [out]
    guint32& dwOffset,              // [in, out]
    guint32& dwSize,                // [in, out]
    IEventSink* pCallback )
{
    if( pDataDesc == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oDataDesc( pDataDesc );
        IConfigDb* pTransCtx = nullptr;
        ret = oDataDesc.GetPointer(
            propTransCtx, pTransCtx );
        if( ERROR( ret ) )
            break;

        CCfgOpener oTransCtx( pTransCtx );
        ret = oTransCtx.CopyProp(
            propSessHash, this );
        if( ERROR( ret ) )
            break;

        ret = super::FetchData_Server(
            pDataDesc, fd, dwOffset,
            dwSize, pCallback );

    }while( 0 );

    return ret;
}

bool CRpcTcpBridgeAuth::IsKdcChannel()
{
    bool bNoEnc = false;
    gint32 ret = m_oSecCtx.GetBoolProp(
        propNoEnc, bNoEnc );

    if( ERROR( ret ) )
        return false;

    return bNoEnc;
}

gint32 CRpcReqForwarderProxyAuth::ForwardLogin(
    IConfigDb* pReqCtx,
    DBusMessage* pMsgRaw,
    DMsgPtr& pRespMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        DMsgPtr pNewMsg;
        DMsgPtr pOldMsg( pMsgRaw );
        ret = pNewMsg.CopyHeader( pMsgRaw );
        if( ERROR( ret ) )
            break;

        BufPtr pReqBuf( true );
        gint32 iType = 0;
        ret = pOldMsg.GetArgAt( 0, pReqBuf, iType );
        if( ERROR( ret ) )
            break;

        // insert a the propConnHandle, which
        // will be a component to make up the
        // session id.
        CCfgOpener oNewCtx;
        ret = oNewCtx.CopyProp(
            propConnHandle, pReqCtx );
        if( ERROR( ret ) )
            break;

        BufPtr pCtxBuf( true );
        ret = oNewCtx.Serialize( *pCtxBuf );
        if( ERROR( ret ) )
            break;

        const char* pData = pReqBuf->ptr();
        const char* pCtx = pCtxBuf->ptr();
        if( !dbus_message_append_args( pNewMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pReqBuf->size(),
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

        return super::ForwardRequest( pReqCtx,
            pNewMsg, pRespMsg, pCallback );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderProxyAuth::ForwardRequest(
    IConfigDb* pReqCtx,
    DBusMessage* pMsgRaw,
    DMsgPtr& pRespMsg,
    IEventSink* pCallback )
{
    if( pReqCtx == nullptr ||
        pMsgRaw == nullptr )
        return -EINVAL;

    std::string strDest =
        AUTH_DEST( GetIoMgr() );

    DMsgPtr pMsg( pMsgRaw );
    gint32 ret = 0;
    do{
        if( strDest != pMsg.GetDestination() )
            break;

        CRpcRouter* pRouter = GetParent();
        if( !pRouter->HasAuth() )
        {
            ret = -EBADMSG;
            break;
        }

        std::string strIfName;
        strIfName = pMsg.GetInterface();
        if( strIfName != DBUS_IF_NAME(
                "IAuthenticate" ) )
        {
            ret = -EINVAL;
            break;
        }

        std::string strPath;
        CCfgOpener oReqCtx( pReqCtx );
        ret = oReqCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        if( strPath != "/" )
        {
            ret = -EINVAL;
            break;
        }
        
        if( pMsg.GetMember() != SYS_METHOD(
            AUTH_METHOD_LOGIN ) )
            break;

        return ForwardLogin( pReqCtx,
            pMsgRaw, pRespMsg, pCallback );

    }while( 0 );

    if( ERROR( ret ) )
        return ret;

    return super::ForwardRequest( pReqCtx,
        pMsgRaw, pRespMsg, pCallback );
}

gint32 CRpcReqForwarderAuth::LocalLogin(
    IEventSink* pCallback,
    const IConfigDb* pcfg )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        TaskletPtr pDeferTask;

        IConfigDb* pCfg = 
            const_cast< IConfigDb* >( pcfg );

        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pDeferTask, ObjPtr( this ),
            &CRpcReqForwarderAuth::LocalLoginInternal,
            nullptr, pCfg, pCallback );

        if( ERROR( ret ) )
            break;

        // queue the task in a seq taskgroup
        ret = AddLoginSeqTask(
            pDeferTask, false );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderAuth::OpenRemotePort(
    IEventSink* pCallback,
    const IConfigDb* pCfg )
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oCfg( pCfg );

        IConfigDb* pConnParams = nullptr;
        ret = oCfg.GetPointer(
            propConnParams, pConnParams );
        if( ERROR( ret ) )
            break;

        CConnParamsProxy oConn( pConnParams );
        if( !oConn.HasAuth() )
        {
            ret = -EINVAL;
            break;
        }

        ret = super::OpenRemotePort(
            pCallback, pCfg );

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderAuth::LocalLoginInternal(
    IEventSink* pCallback,
    IConfigDb* pCfg,
    IEventSink* pInvTask )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    bool bExist = false;
    CCfgOpener oReqCtx;
    CCfgOpener oResp;

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

        CConnParamsProxy oConn( pConnParams );
        if( !oConn.HasAuth() )
        {
            ret = -EINVAL;
            break;
        }
        std::string strSender;
        ret = oCfg.GetStrProp(
            propSrcDBusName, strSender );

        if( ERROR( ret ) )
            break;

        std::string strSrcUniqName;
        ret = oCfg.GetStrProp(
            propSrcUniqName, strSrcUniqName );

        if( ERROR( ret ) )
            break;

        std::string strRouterPath;
        ret = oCfg.GetStrProp(
            propRouterPath, strRouterPath );
        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        CRpcRouter* pRouter = GetParent();
        ret = pRouter->GetBridgeProxy(
            pConnParams, pIf );

        if( SUCCEEDED( ret ) )
        {
            CRpcServices* pSvc = pIf;
            PortPtr pPort = pSvc->GetPort();
            CCfgOpenerObj oPortCfg(
                ( CObjBase* )pPort );

            ret = oPortCfg.GetIntProp(
                propPdoId, dwPortId );
            
            if( ERROR( ret ) )
                break;

            CCfgOpenerObj oIfCfg(
                ( CObjBase* )pIf );

            oIfCfg.SetIntProp(
                propPortId, dwPortId );

            ret = AddRefCount( dwPortId,
                strSrcUniqName, strSender );

            if( ERROR( ret ) )
                break;

            // schedule a checkrouter path task
            oReqCtx.SetIntProp(
                propConnHandle, dwPortId );

            oReqCtx.SetStrProp(
                propRouterPath, strRouterPath );

            oReqCtx.CopyProp( propConnParams,
                ( CObjBase* )pIf );

            // for rollback
            oReqCtx.SetStrProp(
                propSrcUniqName, strSrcUniqName );

            oReqCtx.SetStrProp(
                propSrcDBusName, strSender );

            bExist = true;

            if( strRouterPath == "/" )
            {
                oResp.SetIntProp(
                    propConnHandle, dwPortId );

                oResp.CopyProp( propConnParams,
                    ( CObjBase* )pIf );

                DebugPrint( ret,
                    "The bridge proxy already"
                    " exists..., portid=%d,"
                    " uniqName=%s, sender=%s",
                    dwPortId,
                    strSrcUniqName.c_str(),
                    strSender.c_str() );

                ret = 0;
                break;
            }

            oReqCtx.CopyProp( propSessHash,
                ( CObjBase* )pIf );

            // schedule a checkrouterpath task
            TaskletPtr pChkRt;
            ret = DEFER_HANDLER_NOSCHED(
                pChkRt, ObjPtr( this ),
                &CRpcReqForwarder::CheckRouterPath,
                pInvTask, oReqCtx.GetCfg() );
            if( ERROR( ret ) )
                break;

            // schedule a new task to release the
            // login seq task queue
            CIoManager* pMgr = pSvc->GetIoMgr();
            ret = pMgr->RescheduleTask( pChkRt );
        }
        else if( ret == -ENOENT )
        {
            // create the authenticate proxy which
            // will finally create the bridge
            // proxy on success.
            CParamList oParams;
            ret = CreateBridgeProxyAuth(
                pCallback, pCfg, pInvTask );
        }
        break;

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        oResp[ propReturnValue ] = ret;

        if( ERROR( ret ) )
        {
            oResp.RemoveProperty(
                propConnHandle );

            oResp.RemoveProperty(
                propConnParams );

            if( bExist )
            {
                SchedToStopBridgeProxy(
                    oReqCtx.GetCfg() );
            }
        }

        OnServiceComplete(
            oResp.GetCfg(), pInvTask );
    }

    return ret;
}

gint32 CRpcReqForwarderAuth::InitUserFuncs()
{
    gint32 ret = super::InitUserFuncs();
    if( ERROR( ret ) )
        return ret;

    BEGIN_IFHANDLER_MAP( CRpcReqForwarderAuth );

    ADD_SERVICE_HANDLER(
        CRpcReqForwarderAuth::LocalLogin,
        SYS_METHOD_LOCALLOGIN );

    END_IFHANDLER_MAP;

    return 0;
}

gint32 CAuthentProxy::BuildLoginTask(
    IEventSink* pIf,
    IEventSink* pCallback,
    TaskletPtr& pTask )
{
    if( pIf == nullptr || pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        std::string strMech = GET_MECH( pIf );

        if( strMech != "krb5" &&
            strMech != "ntlm" )
        {
            ret = -ENOTSUP;
            break;
        }

        TaskletPtr pLoginTask;
        ret = DEFER_IFCALLEX2_NOSCHED2(
            0, pLoginTask, ObjPtr( pIf ),
            &CK5AuthProxy::DoLogin,
            nullptr );

        if( ERROR( ret ) )
            break;

        CIfRetryTask* pRetryTask = pLoginTask;
        pRetryTask->SetClientNotify( pCallback );

        pTask = pLoginTask;

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::CreateSessImpl(
    const IConfigDb* pConnParams,
    CRpcRouter* pRouter,
    InterfPtr& pImpl )
{
    if( pConnParams == nullptr ||
        pRouter == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oConn;
        ret = oConn.GetCfg()->Clone( *pConnParams );
        if( ERROR( ret ) )
            break;

        // force the router path to be the root
        oConn.SetStrProp( propRouterPath, "/" );

        IConfigDb* pAuth = nullptr;
        ret = oConn.GetPointer(
            propAuthInfo, pAuth );
        if( ERROR( ret ) )
            break;

        CCfgOpener oAuth( pAuth );
        std::string strMech;
        ret = oAuth.GetStrProp(
            propAuthMech, strMech );

        if( ERROR( ret ) )
            break;

        std::string strObjName;
        if( strMech == "krb5" ||
            strMech == "ntlm" )
            strObjName = OBJNAME_AUTHSVR;
        else
        {
            ret = -ENOTSUP;
            break;
        }

        CCfgOpener oCfg;

        ret = CRpcServices::LoadObjDesc(
            DESC_FILE, strObjName,
            false, oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        // replace the conn params from the desc
        // file with what we have.
        oCfg.SetObjPtr( propConnParams,
            ObjPtr( oConn.GetCfg() ) );

        oCfg.SetPointer(
            propRouterPtr, pRouter );

        oCfg.SetPointer(
            propIoMgr, pRouter->GetIoMgr() );

        // use a special ifstate
        oCfg.SetIntProp( propIfStateClass,
            clsid( CRemoteProxyStateAuth ) );

        InterfPtr pIf;
        ret = pIf.NewObj(
            clsid( CAuthentProxyK5Impl ),
            oCfg.GetCfg() );

        if( SUCCEEDED( ret ) )
            pImpl = pIf;

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::StopSessImpl()
{
    gint32 ret = 0;
    do{
        ObjPtr pSessImpl;
        ret = GetSessImpl( pSessImpl );
        if( ERROR( ret ) )
            break;

        CParamList oParams;
        oParams.SetObjPtr(
            propIfPtr, pSessImpl );

        TaskletPtr pDummy;
        ret = pDummy.NewObj( 
            clsid( CIfDummyTask ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        CRpcServices* pSvc = pSessImpl;
        ret = pSvc->TestSetState( cmdShutdown );
        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }
        ret = DEFER_CALL( GetIoMgr(),
            ObjPtr( pSvc ),
            &CRpcServices::Shutdown,
            ( IEventSink* )pDummy );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::OnPostStop(
    IEventSink* pCallback )
{
    // stop the auth proxy if it is still
    // in connected state.
    StopSessImpl();

    // remove the binding
    m_pSessImpl.Clear();
    return 0;
}

gint32 CAuthentProxy::Login(
    IEventSink* pCallback,
    IConfigDb* pReq,
    CfgPtr& pResp )
{
    gint32 ret = 0;
    do{
        CRpcServices* pSvc = m_pSessImpl;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        
        IAuthenticateProxy* pProxy =
        dynamic_cast< IAuthenticateProxy* >
            ( pSvc );

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = pProxy->Login(
            pCallback, pReq, pResp );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::MechSpecReq(
    IEventSink* pCallback,
    IConfigDb* pReq,
    IConfigDb* pResp )
{
    gint32 ret = 0;
    do{
        CRpcServices* pSvc = m_pSessImpl;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        
        IAuthenticateProxy* pProxy =
        dynamic_cast< IAuthenticateProxy* >
            ( pSvc );

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = pProxy->MechSpecReq(
            pCallback, pReq, pResp );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::GetSess(
    std::string& strSess ) const
{
    gint32 ret = 0;
    do{
        CRpcServices* pSvc = m_pSessImpl;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        
        IAuthenticateProxy* pProxy =
        dynamic_cast< IAuthenticateProxy* >
            ( pSvc );

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret =  pProxy->GetSess( strSess );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::WrapMessage(
    BufPtr& pInMsg,
    BufPtr& pOutMsg )
{
    gint32 ret = 0;
    do{
        CRpcServices* pSvc = m_pSessImpl;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        
        IAuthenticateProxy* pProxy =
        dynamic_cast< IAuthenticateProxy* >
            ( pSvc );

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret =  pProxy->WrapMessage(
            pInMsg, pOutMsg );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::UnwrapMessage(
    BufPtr& pInMsg,
    BufPtr& pOutMsg )
{
    gint32 ret = 0;
    do{
        CRpcServices* pSvc = m_pSessImpl;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        
        IAuthenticateProxy* pProxy =
        dynamic_cast< IAuthenticateProxy* >
            ( pSvc );

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret =  pProxy->UnwrapMessage(
            pInMsg, pOutMsg );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::GetMicMsg(
    BufPtr& pInMsg,/* [ in ] */
    BufPtr& pOutMic )/* [ out ] */
{
    gint32 ret = 0;
    do{
        CRpcServices* pSvc = m_pSessImpl;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        
        IAuthenticateProxy* pProxy =
        dynamic_cast< IAuthenticateProxy* >
            ( pSvc );

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret =  pProxy->GetMicMsg(
            pInMsg, pOutMic );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::VerifyMicMsg(
    BufPtr& pInMsg,/* [ in ] */
    BufPtr& pInMic )/* [ in ] */
{
    gint32 ret = 0;
    do{
        CRpcServices* pSvc = m_pSessImpl;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        
        IAuthenticateProxy* pProxy =
        dynamic_cast< IAuthenticateProxy* >
            ( pSvc );

        if( pProxy == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret =  pProxy->VerifyMicMsg(
            pInMsg, pInMic );

    }while( 0 );

    return ret;
}

gint32 CAuthentProxy::InitEnvRouter(
    CIoManager* pMgr )
{
    return CK5AuthProxy::InitEnvRouter( pMgr );
}

gint32 CRpcTcpBridgeProxyAuth::OnPostStop(
    IEventSink* pCallback )
{
    SetSessHash( "", false );
    return 0;
}

gint32 CRpcTcpBridgeProxyAuth::SetSessHash(
    const std::string& strHash, bool bNoEnc )
{
    // notify the bridge the authentication
    // context is setup.
    gint32 ret = 0;
    do{
        std::string strHash1;
        CCfgOpenerObj oIfCfg( this );
        ret = oIfCfg.GetStrProp(
            propSessHash, strHash1 );
        if( SUCCEEDED( ret ) )
        { 
            if( strHash.empty() )
            {
                // clear the hash
                oIfCfg.RemoveProperty(
                    propSessHash );
                m_oSecCtx.Clear();
                ret = 0;
            }
            else
            {
                ret = -EEXIST;
            }
            break;
        }

        ret = 0;
        if( strHash.empty() )
            break;

        CAuthentProxy* psp = ObjPtr( this );
        ObjPtr pSessImpl;
        ret = psp->GetSessImpl( pSessImpl );
        if( ERROR( ret ) )
            break;

        CCfgOpener& oCtx = m_oSecCtx;
        if( !bNoEnc )
        {
            oCtx.SetObjPtr(
                propObjPtr, pSessImpl );
        }

        oIfCfg.SetStrProp(
            propSessHash, strHash );

        oCtx.SetBoolProp(
            propNoEnc, bNoEnc );

        oCtx.SetStrProp(
            propSessHash, strHash );

        oCtx.SetBoolProp(
            propIsServer, false );

        PortPtr pPort;
        GET_TARGET_PORT( pPort );
        if( ERROR( ret ) )
            break;

        BufPtr pBuf( true );
        bool bFound = false;
        while( !pPort.IsEmpty() )
        {
            ret = GetPortProp( pPort,
                propLowerPortPtr, pBuf );    
            if( ERROR( ret ) )
            {
                ret = 0;
                break;
            }

            pPort = ( ObjPtr& )*pBuf;
            if( pPort.IsEmpty() )
                break;

            ret = GetPortProp( pPort,
                propPortClass, pBuf );
            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }
            std::string strClass = *pBuf;
            if( strClass != PORT_CLASS_SEC_FIDO )
                continue;

            bFound = true;
            break;
        }

        if( !bFound )
        {
            ret = ERROR_FAIL;
            break;
        }

        *pBuf = ObjPtr( m_oSecCtx.GetCfg() );
        SetPortProp( pPort, propSecCtx, pBuf );

    }while( 0 );

    return ret;
}

bool CRpcTcpBridgeProxyAuth::IsKdcChannel()
{
    bool bNoEnc = false;
    gint32 ret = 0;
    ret = m_oSecCtx.GetBoolProp(
        propNoEnc, bNoEnc );
    if( ERROR( ret ) )
        return false;
    return bNoEnc;
}

gint32 CRpcReqForwarderAuth::OnSessImplStarted(
    IEventSink* pInvTask,
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pInvTask == nullptr ||
        pIoReq == nullptr ||
        pReqCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CParamList oParams;
    CCfgOpener oReqCtx( pReqCtx );

    do{
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        ObjPtr pAuthImpl;
        ret = oReqCtx.GetObjPtr(
            propIfPtr, pAuthImpl );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        CCfgOpenerObj oIfCfg(
            ( CObjBase* )pAuthImpl );

        ret = oIfCfg.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        InterfPtr pbp;
        CRpcRouter* pRouter = GetParent();
        ret = pRouter->GetBridgeProxy(
            dwPortId, pbp );
        if( ERROR( ret ) )
            break;

        // bind the bridge proxy and the auth
        // proxy
        CAuthentProxy* pBdgePrxy = ObjPtr( pbp );
        InterfPtr pTemp = pAuthImpl;
        pBdgePrxy->SetSessImpl( pTemp );

        IAuthenticateProxy* pAuth =
            dynamic_cast < IAuthenticateProxy* >
                ( ( CObjBase* )pAuthImpl );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        pAuth->SetParent( pBdgePrxy );

    }while( 0 );

    if( ERROR( ret ) )
    {
        oParams[ propReturnValue ] = ret;
        OnServiceComplete(
            oParams.GetCfg(), pInvTask );
    }

    if( ret == STATUS_PENDING )
        ret = 0;

    return ret;
}

gint32 CRpcReqForwarderAuth::OnSessImplLoginComplete(
    IEventSink* pInvTask,
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pInvTask == nullptr ||
        pIoReq == nullptr ||
        pReqCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CParamList oParams;
    CCfgOpener oReqCtx( pReqCtx );

    do{
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        CRpcServices* pap = nullptr;
        ret = oReqCtx.GetPointer(
            propIfPtr, pap );
        if( ERROR( ret ) )
            break;

        IAuthenticateProxy* pAuth =
            dynamic_cast < IAuthenticateProxy* >
            ( pap );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        CRpcServices* pbp = pAuth->GetParent();
        if( unlikely( pbp == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        // at this moment, the auth proxy should
        // have the correct connhandle in place.
        guint32 dwPortId = 0;
        CCfgOpenerObj oapCfg( pap );
        ret = oapCfg.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.SetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.CopyProp(
            propSessHash, ( CObjBase* )pbp );
        if( ERROR( ret ) )
            break;

        bool bNoEnc;
        ret = oapCfg.GetBoolProp(
            propNoEnc, bNoEnc );
        if( ERROR( ret ) )
            bNoEnc = false;

        std::string strHash;
        pAuth->GetSess( strHash );

        CRpcTcpBridgeProxyAuth* pProxy =
            ObjPtr( pbp );

        ret = pProxy->SetSessHash(
            strHash, bNoEnc );
        if( ERROR( ret ) )
            break;

        if( true )
        {
            std::string strSender;
            std::string strUniqName;

            ret = oReqCtx.GetStrProp(
                propSrcDBusName, strSender );
            if( ERROR( ret ) )
                break;

            ret = oReqCtx.GetStrProp(
                propSrcUniqName, strUniqName );
            if( ERROR( ret ) )
                break;

            CCfgOpenerObj oIfCfg(
                ( CObjBase* )pProxy );

            guint32 dwPortId = 0;
            ret = oIfCfg.GetIntProp(
                propPortId, dwPortId );
            
            if( ERROR( ret ) )
                break;

            ret = AddRefCount( dwPortId,
                strUniqName, strSender );

            if( ERROR( ret ) )
                break;
        }
        // update the connparams with the newest
        // one from the bridge proxy.
        oReqCtx.CopyProp(
            propConnParams, pbp );

        std::string strPath;
        ret = oReqCtx.GetStrProp(
            propRouterPath, strPath );

        if( ERROR( ret ) )
            break;

        // ok the LocalLogin request is done
        if( strPath == "/" )
            break;

        // schedule a checkrouterpath task
        TaskletPtr pChkRt;
        ret = DEFER_HANDLER_NOSCHED(
            pChkRt, ObjPtr( this ),
            &CRpcReqForwarder::CheckRouterPath,
            pInvTask, oReqCtx.GetCfg() );
        if( ERROR( ret ) )
            break;

        // a new task to release the seq task
        // queue
        CAuthentProxy* pabp =
            dynamic_cast< CAuthentProxy* >
                ( pProxy );

        CIoManager* pMgr = pabp->GetIoMgr();
        ret = pMgr->RescheduleTask( pChkRt );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    oReqCtx.RemoveProperty( propIfPtr );
    oParams[ propReturnValue ] = ret;
    if( SUCCEEDED( ret ) )
    {
        oParams.CopyProp(
            propConnHandle, pReqCtx );

        oParams.CopyProp(
            propConnParams, pReqCtx );
    }

    if( ret != STATUS_PENDING )
    {
        OnServiceComplete(
            oParams.GetCfg(), pInvTask );
    }

    if( ret == STATUS_PENDING )
    {
        // release the login task queue the rest
        // part, CheckRouterPath can run freely.
        ret = 0;
    }

    return ret;
}

gint32 CRpcReqForwarderAuth::BuildStartAuthProxyTask(
    IEventSink* pInvTask,
    IConfigDb* pCfg,
    TaskletPtr& pTask )
{
    if( pInvTask == nullptr ||
        pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    TaskGrpPtr pTaskGrp;
    TaskletPtr pRespCb;
    TaskletPtr pRespCb1;
    do{
        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );
        ret = pTaskGrp.NewObj(
            clsid( CIfTransactGroup ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        CIfTransactGroup* pTransGrp = pTaskGrp;
        pTransGrp->SetTaskRelation( logicAND );

        IConfigDb* pConnParams = nullptr;
        CCfgOpener oCfg( pCfg );

        ret = oCfg.GetPointer(
            propConnParams, pConnParams );
        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        CRpcRouter* pRouter = GetParent();
        ret = CAuthentProxy::CreateSessImpl(
            pConnParams, pRouter, pIf );
        if( ERROR( ret ) )
            break;

        CParamList oReqCtx;

        oReqCtx.SetObjPtr( propIfPtr, pIf );

        ret = oReqCtx.SetPointer(
            propConnParams, pConnParams );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.CopyProp(
            propRouterPath, pCfg );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.CopyProp(
            propSrcDBusName, pCfg );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.CopyProp(
            propSrcUniqName, pCfg );
        if( ERROR( ret ) )
            break;

        IConfigDb* pReqCtx = oReqCtx.GetCfg();

        ret = NEW_PROXY_RESP_HANDLER2(
            pRespCb, ObjPtr( this ),
            &CRpcReqForwarderAuth::OnSessImplStarted,
            pInvTask, pReqCtx );
        if( ERROR( ret ) )
            break;

        TaskletPtr pStartIfTask;
        ret = DEFER_IFCALLEX2_NOSCHED2(
            0, pStartIfTask, ObjPtr( pIf ),
            &CRpcServices::StartEx,
            nullptr );

        if( ERROR( ret ) )
            break;

        CIfRetryTask* pStart = pStartIfTask;
        pStart->SetClientNotify( pRespCb );
        pTaskGrp->AppendTask( pStartIfTask );

        // just to check if the token exchange is
        // done. if failed, the invoke task will
        // be completed here.
        ret = NEW_PROXY_RESP_HANDLER2(
            pRespCb1, ObjPtr( this ),
            &CRpcReqForwarderAuth::OnSessImplLoginComplete,
            pInvTask, pReqCtx );
        if( ERROR( ret ) )
            break;

        TaskletPtr pLoginTask;
        ret = CAuthentProxy::BuildLoginTask(
            pIf, pRespCb1, pLoginTask );

        if( ERROR( ret ) )
            break;

        pTaskGrp->AppendTask( pLoginTask );

        TaskletPtr pStopIf;
        ret = DEFER_IFCALLEX2_NOSCHED2(
            0, pStopIf, ObjPtr( pIf ),
            &CRpcServices::StopEx,
            nullptr );

        if( ERROR( ret ) )    
            break;
       
        CIfTransactGroup* pTractGrp = pTaskGrp; 
        pTractGrp->AddRollback( pStopIf );
        pTask = ObjPtr( pTaskGrp );

    }while( 0 );

    if( ERROR( ret ) )
    {
        if( !pRespCb.IsEmpty() )
            ( *pRespCb )( eventCancelTask );

        if( !pRespCb1.IsEmpty() )
            ( *pRespCb1 )( eventCancelTask );

        if( !pTaskGrp.IsEmpty() )
            ( *pTaskGrp )( eventCancelTask );
    }

    return ret;
}

gint32 CRpcReqForwarderAuth::CreateBridgeProxyAuth(
    IEventSink* pSeqTask,
    IConfigDb* pCfg,
    IEventSink* pInvTask )
{
    if( pSeqTask == nullptr ||
        pCfg == nullptr ||
        pInvTask == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        TaskletPtr pApTask;
        ret = BuildStartAuthProxyTask(
            pInvTask, pCfg, pApTask );
        if( ERROR( ret ) )
            break;

        CIfRetryTask* pTask = pApTask;
        pTask->SetClientNotify( pSeqTask );

        CIoManager* pMgr = GetIoMgr();
        ret = pMgr->RescheduleTask( pApTask );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}


gint32 CRpcRouterReqFwdrAuth::IsEqualConn(
    const IConfigDb* pConn1,
    const IConfigDb* pConn2 )
{
    gint32 ret = super::IsEqualConn(
        pConn1, pConn2 );
    if( ERROR( ret ) )
        return ret;

    do{
        CCfgOpener oConn1( pConn1 );
        IConfigDb* pAuth1 = nullptr;
        ret = oConn1.GetPointer(
            propAuthInfo, pAuth1 );
        if( ERROR( ret ) )
            break;

        IConfigDb* pAuth2 = nullptr;
        CCfgOpener oConn2( pConn2 );
        ret = oConn2.GetPointer(
            propAuthInfo, pAuth2 );
        if( ERROR( ret ) )
            break;

        CCfgOpener oAuth1( pAuth1 );
        CCfgOpener oAuth2( pAuth2 );
        std::string strMech;
        ret = oAuth1.GetStrProp(
            propAuthMech, strMech );
        if( ERROR( ret ) )
            break;

        if( !oAuth2.IsEqual(
            propAuthMech, strMech ) )
        {
            ret = ERROR_FALSE;
            break;
        }

        if( !oAuth1.IsEqualProp(
            propUserName, pAuth2 ) )
        {
            ret = ERROR_FALSE;
            break;
        }

        if( strMech == "krb5" )
        {
            if( !oAuth1.IsEqualProp(
                propServiceName, pAuth2 ) )
            {
                ret = ERROR_FALSE;
                break;
            }
        }

    }while( 0 );

    if( ERROR( ret ) )
        ret = ERROR_FALSE;

    return ret;
}

gint32 CRpcRouterReqFwdrAuth::GetBridgeProxy(
    const IConfigDb* pConnParams,
    InterfPtr& pIf )
{
    if( pConnParams == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oConn( pConnParams );
        IConfigDb* pAuth = nullptr;
        ret = oConn.GetPointer(
            propAuthInfo, pAuth );
        if( ERROR( ret ) )
            break;

        ret = super::GetBridgeProxy(
            propConnParams, pIf );

    }while( 0 );

    return ret;
}

gint32 CRpcRouterReqFwdrAuth::DecRefCount(
        guint32 dwPortId,
        const std::string& strSrcUniqName,
        const std::string& strSrcDBusName )
{
    gint32 ret = super::DecRefCount( dwPortId,
        strSrcUniqName, strSrcDBusName );

    if( ERROR( ret ) )
        return ret;

    if( ret != 1 )
        return ret;

    do{
        CStdRMutex oRouterLock( GetLock() );
        RegObjPtr pReg;
        for( auto elem : m_mapRefCount )
        {
            if( elem.first->GetPortId() == dwPortId )
            {
                pReg = elem.first;
                break;
            }
        }
        if( pReg.IsEmpty() )
        { 
            ret = -EFAULT;
            break;
        }

        std::string strName =
            pReg->GetUniqName();

        oRouterLock.Unlock();

        PortPtr pPort = GetPort();
        CCfgOpenerObj oPortCfg(
            ( CObjBase* )pPort );

        std::string strName2;
        ret = oPortCfg.GetStrProp(
            propSrcUniqName, strName2 );

        if( ERROR( ret ) )
            break;

        // the last reference is held by the
        // authproxy, OK to remove
        if( strName == strName2 )
        {
            // notify the authprxy to stop
            InterfPtr pIf;
            ret = GetBridgeProxy( dwPortId, pIf );
            CAuthentProxy* pspp = pIf;
            oRouterLock.Unlock();
            pspp->StopSessImpl();
        }
        ret = 1;

    }while( 0 );

    return ret;
}

gint32 CRpcReqForwarderAuth::ClearRefCountByPortId(
    guint32 dwPortId,
    std::vector< std::string >& vecUniqNames )
{
    gint32 ret = super::ClearRefCountByPortId(
        dwPortId, vecUniqNames );

    if( ret < 2 )
        return ret;

    PortPtr pPort = GetPort();
    if( pPort.IsEmpty() )
        return -EINVAL;

    CCfgOpenerObj oPortCfg(
        ( CObjBase* )pPort );

    std::string strUniqName;
    ret = oPortCfg.GetStrProp(
        propSrcUniqName, strUniqName );

    if( ERROR( ret ) )
        return ret;

    std::vector< std::string >::iterator itr =
        vecUniqNames.begin();

    while( itr != vecUniqNames.end() )
    {
        // remove the local one, to avoid authprxy
        // receiving the eventRmtSvrOnline event.
        if( *itr == strUniqName )
        {
            vecUniqNames.erase( itr );
            break;
        }
        ++itr;
    }

    return vecUniqNames.size();
}

gint32 CAuthentServer::InitUserFuncs()
{
    BEGIN_IFHANDLER_MAP( IAuthenticate );
    
    ADD_SERVICE_HANDLER_EX( 1,
       CAuthentServer::Login,
       AUTH_METHOD_LOGIN );

    ADD_SERVICE_HANDLER_EX( 1,
       CAuthentServer::MechSpecReq,
       AUTH_METHOD_LOGIN );

    END_IFHANDLER_MAP;
    return 0;
}

gint32 CAuthentServer::Login(
    IEventSink* pCallback,
    IConfigDb* pInfo, /*[ in ]*/
    CfgPtr& pResp )
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticate* pAuth =
        dynamic_cast< IAuthenticate* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->Login(
            pCallback, pInfo, pResp );

    }while( 0 );

    return ret;
}

gint32 CAuthentServer::IsNoEnc(
    const std::string& strSess )
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticateServer* pAuth =
        dynamic_cast< IAuthenticateServer* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->IsNoEnc( strSess );

    }while( 0 );

    return ret;
}

gint32 CAuthentServer::GetSess(
    guint32 dwPortId,
    std::string& strSess )
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticateServer* pAuth =
        dynamic_cast< IAuthenticateServer* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->GetSess(
            dwPortId, strSess );

    }while( 0 );

    return ret;
}

gint32 CAuthentServer::MechSpecReq(
    IEventSink* pCallback,
    IConfigDb* pInfo,/*[ in ]*/
    IConfigDb* pResp )/*[ out ]*/
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticate* pAuth =
        dynamic_cast< IAuthenticate* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->MechSpecReq(
            pCallback, pInfo, pResp );

    }while( 0 );

    return ret;
}

gint32 CAuthentServer::WrapMessage(
    const std::string& strSess,
    BufPtr& pInMsg,
    BufPtr& pOutMsg )
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticateServer* pAuth =
        dynamic_cast< IAuthenticateServer* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->WrapMessage(
            strSess, pInMsg, pOutMsg );

    }while( 0 );

    return ret;
}
    
gint32 CAuthentServer::UnwrapMessage(
    const std::string& strSess,
    BufPtr& pInMsg,
    BufPtr& pOutMsg )
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticateServer* pAuth =
        dynamic_cast< IAuthenticateServer* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->UnwrapMessage(
            strSess, pInMsg, pOutMsg );

    }while( 0 );

    return ret;
}

gint32 CAuthentServer::GetMicMsg(
    const std::string& strSess,
    BufPtr& pInMsg,
    BufPtr& pOutMic )
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticateServer* pAuth =
        dynamic_cast< IAuthenticateServer* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->GetMicMsg(
            strSess, pInMsg, pOutMic );

    }while( 0 );

    return ret;
}   

gint32 CAuthentServer::VerifyMicMsg(
    const std::string& strSess,
    BufPtr& pInMsg,
    BufPtr& pInMic )
{
    gint32 ret = 0;
    do{
        CRpcServices* pObj = m_pAuthImpl;
        if( pObj == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IAuthenticateServer* pAuth =
        dynamic_cast< IAuthenticateServer* >
                ( pObj );

        if( unlikely( pAuth == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        ret = pAuth->VerifyMicMsg(
            strSess, pInMsg, pInMic );

    }while( 0 );

    return ret;
}

gint32 CAuthentServer::OnStartAuthImplComplete(
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
    CCfgOpener oReqCtx( pReqCtx );

    do{
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        CRpcServices* pSvc;
        ret = oReqCtx.GetPointer(
            propIfPtr, pSvc );
        if( ERROR( ret ) )
            break;

        m_pAuthImpl = pSvc;

    }while( 0 );

    pCallback->OnEvent( eventTaskComp,
        0, 0, ( LONGWORD* )this );

    return ret;
}

gint32 CAuthentServer::StartAuthImpl(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        //FIXME: just mechanism is krb5 only
        CCfgOpener oCfg;
        ret = CRpcServices::LoadObjDesc(
            DESC_FILE, "K5AuthServer",
            true, oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        CRpcRouter* pRouter = ObjPtr( this );

        oCfg.SetPointer(
            propRouterPtr, pRouter );

        oCfg.SetPointer(
            propIoMgr, pRouter->GetIoMgr() );

        ret = oCfg.CopyProp(
            propAuthInfo, this );
        if( ERROR( ret ) )
            break;

        InterfPtr pIf;
        ret = pIf.NewObj(
            clsid( CK5AuthServer ),
            oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        CCfgOpener oReqCtx;
        oReqCtx[ propIfPtr ] = ObjPtr( pIf );
        TaskletPtr pRespCb;
        ret = NEW_PROXY_RESP_HANDLER2(
            pRespCb, ObjPtr( this ),
            &CAuthentServer::OnStartAuthImplComplete,
            pCallback,
            ( IConfigDb* )oReqCtx.GetCfg() );
        if( ERROR( ret ) )
            break;

        ret = pIf->StartEx( pRespCb );
        if( ret != STATUS_PENDING )
            ( *pRespCb )( eventCancelTask );

        if( SUCCEEDED( ret ) )
            m_pAuthImpl = pIf;

    }while( 0 );

    return ret;
}

gint32 CAuthentServer::OnPostStart(
    IEventSink* pCallback )
{
    return StartAuthImpl( pCallback );
}

gint32 CAuthentServer::OnPreStopComplete(
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
    CCfgOpener oReqCtx( pReqCtx );

    do{
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        m_pAuthImpl.Clear();

    }while( 0 );

    pCallback->OnEvent( eventTaskComp,
        0, 0, ( LONGWORD* )this );

    return ret;
}

gint32 CAuthentServer::OnPreStop(
    IEventSink* pCallback )
{
    if( m_pAuthImpl.IsEmpty() )
        return -EINVAL;

    TaskletPtr pRespCb;
    gint32 ret = NEW_PROXY_RESP_HANDLER2(
        pRespCb, ObjPtr( this ),
        &CAuthentServer::OnPreStopComplete,
        pCallback, nullptr );
    if( ERROR( ret ) )
        return ret;

    CRpcServices* pSvc = m_pAuthImpl;
    if( pSvc->GetState() == stateConnected )
    {
        ret = pSvc->Shutdown( pRespCb );
        if( ret != STATUS_PENDING )
            ( *pRespCb )( eventCancelTask );
    }

    if( ret != STATUS_PENDING )
        m_pAuthImpl.Clear();

    return ret;
}

gint32 CAuthentServer::RemoveSession(
    const std::string& strSess )
{
    CRpcServices* pSvc = m_pAuthImpl;

    IAuthenticateServer* pAuth =
    dynamic_cast< IAuthenticateServer* >
            ( pSvc );

    if( pAuth == nullptr )
        return -EFAULT;

    return pAuth->RemoveSession( strSess );
}

gint32 CRpcRouterBridgeAuth::OnPostStart(
    IEventSink* pCallback )
{
    gint32 ret = super::OnPostStart(
        pCallback );

    if( ERROR( ret ) )
        return ret;
    do{
        TaskGrpPtr pTransGrp;
        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );
        ret = pTransGrp.NewObj(
            clsid( CIfTransactGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        // NOTE:this match does not have
        // propPortId, so cannot be enabled to
        // receive events on.  For the reqfwdr
        // proxy, it is OK to deliver the auth
        // request to the AuthentServer without
        // the match
        MatchPtr& pMatch = m_pAuthMatch;

        TaskletPtr pStartTask;
        ret = BuildStartStopReqFwdrProxy(
            pMatch, true, pStartTask );
        if( ERROR( ret ) )
            break;

        pTransGrp->AppendTask( pStartTask );

        CIfRetryTask* pGrp = pTransGrp;
        pGrp->SetClientNotify( pCallback );
        TaskletPtr pTask( pGrp );
        ret = AddSeqTask( pTask );
        if( SUCCEEDED( ret ) )
            ret = pTask->GetError();
        
    }while( 0 );

    return ret;
}