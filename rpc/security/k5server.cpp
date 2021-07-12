/*
 * =====================================================================================
 *
 *       Filename:  k5server.cpp
 *
 *    Description:  implementation of CK5AuthServer object and the tcp port for kdc
 *                  connection.
 *
 *        Version:  1.0
 *        Created:  07/17/2020 04:11:37 PM
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

#include "k5proxy.h"
#include "k5server.h"
#include "sha1.h"

namespace rpcf
{

CKdcRelayProxy::CKdcRelayProxy(
    const IConfigDb* pCfg ) :
    super( pCfg )
{
    SetClassId( clsid( CKdcRelayProxy ) );
    gint32 ret = 0;
    do{
        CCfgOpenerObj oCfg( this );
        CRpcServices* pIf = nullptr;

        ret = oCfg.GetPointer(
            propParentPtr, pIf );
        if( ERROR( ret ) )
            break;

        m_pParent = pIf;
        oCfg.RemoveProperty(
            propParentPtr );

    }while( 0 );

    if( ERROR( ret ) )
    {
        std::string strMsg = DebugMsg( ret,
            "Error occurs in"
            "CKdcRelayProxy's ctor" );
        throw std::runtime_error( strMsg );
    }
}
gint32 CKdcRelayProxy::InitUserFuncs()
{
    BEGIN_PROXY_MAP( true );

    ADD_PROXY_METHOD_ASYNC( 1,
        CKdcRelayProxy::MechSpecReq,
        SYS_METHOD( AUTH_METHOD_MECHSPECREQ ) );

    END_PROXY_MAP;
    return 0;
}

gint32 CKdcRelayProxy::OnEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData )
{
    gint32 ret = 0;
    do{
        if( iEvent != eventConnErr )
        {
            ret = super::OnEvent( iEvent,
                dwParam1, dwParam2, pData );
            break;
        }

        HANDLE hPort = ( HANDLE )dwParam1;
        if( hPort != GetPortHandle() )
            break;
        TaskletPtr pStopTask;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pStopTask, ObjPtr( this ),
            &CRpcServices::Shutdown,
            nullptr );
        if( ERROR( ret ) )
            break;

        CRpcServices* pSvc = GetParent();
        if( unlikely( pSvc == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        pSvc->AddSeqTask( pStopTask );
        break;

    }while( 0 );

    return 0;
}

gint32 CKdcRelayProxy::SetupReqIrp(
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
        BufPtr pToken;
        ret = oReq.GetProperty(
            0, pToken );
        if( ERROR( ret ) )
            break;

        CIoManager* pMgr = GetIoMgr();

        pIrp->SetIrpThread( pMgr );
        IrpCtxPtr pCtx = pIrp->GetTopStack();
        pCtx->SetMajorCmd( IRP_MJ_FUNC );
        pCtx->SetMinorCmd( IRP_MN_IOCTL );
        pCtx->SetCtrlCode( CTRLCODE_SEND_REQ );
        pCtx->SetReqData( pToken );
        pCtx->SetIoDirection( IRP_DIR_INOUT ); 
        pIrp->SetCallback( pCallback, 0 );

        guint32 dwTimeout = 0;
        ret = oReq.GetTimeoutSec( dwTimeout );
        if( SUCCEEDED( ret ) )
        {
            // a random timeout cap
            if( unlikely( dwTimeout > 3600 ) )
            {
                ret = -EINVAL;
                break;
            }
            pIrp->SetTimer( dwTimeout, pMgr );
        }
        else
        {
            ret = 0;
        }

    }while( 0 );

    return ret;
}

gint32 CKdcRelayProxy::FillRespData(
    IRP* pIrp, CfgPtr& pResp )
{
    gint32 ret = 0;
    if( pIrp == nullptr ||
        pIrp->GetStackSize() != 1 )
        return -EINVAL;

    if( pResp.IsEmpty() )
        return -EINVAL;

    CParamList oParams( ( IConfigDb* )pResp );
    do{
        ret = pIrp->GetStatus();
        if( ERROR( ret ) )
            break;

        // retrieve the data from the irp
        IrpCtxPtr& pCtx = pIrp->GetTopStack();
        if( pCtx->GetMajorCmd() != IRP_MJ_FUNC ||
            pCtx->GetMinorCmd() != IRP_MN_IOCTL ||
            pCtx->GetCtrlCode() != CTRLCODE_SEND_REQ )
        {
            ret = -EINVAL;
            break;
        }

        BufPtr& pBuf = pCtx->m_pRespData;
        if( pBuf.IsEmpty() )
        {
            ret = -EBADMSG;
            break;
        }
        oParams.Push( pBuf );

    }while( 0 );

    oParams[ propReturnValue ] = ret;

    return ret;
}

gint32 CKdcRelayProxy::MechSpecReq(
    IEventSink* pCallback,
    BufPtr& pReq,
    BufPtr& pResp ) /* [ out ] pResp */
{

    gint32 ret = 0;
    if( pResp.IsEmpty() )
        pResp.NewObj();

    do{
        CCfgOpener oOption;
        ret = oOption.SetBoolProp(
            propSysMethod, true );

        if( ERROR( ret ) )
            break;

        std::string strIfName =
            CoGetIfNameFromIid(
                GetClsid(), "p" );

        oOption[ propIfName ] =
            DBUS_IF_NAME( strIfName );

        CfgPtr pRespParam;

        std::string strMethod = 
            AUTH_METHOD_MECHSPECREQ;

        // make the call
        ret = AsyncCall( pCallback,
            oOption.GetCfg(), pRespParam,
            strMethod, pReq );

        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) )
            break;

        if( SUCCEEDED( ret ) )
        {
            CCfgOpener oResp(
                ( IConfigDb* )pRespParam );

            ret = oResp[ propReturnValue ];
            if( SUCCEEDED( ret ) )
            {
                ret = oResp.GetProperty(
                    0, pResp );
            }
        }

    }while( 0 );

    return ret;
}

CK5AuthServer::CK5AuthServer(
    const IConfigDb* pCfg ) :
    super( pCfg )
{
    gint32 ret = 0;
    do{
        SetClassId( clsid( CK5AuthServer ) );
        CCfgOpenerObj oIfCfg( this );
        ret = oIfCfg.GetPointer(
            propRouterPtr, m_pRouter );
        if( ERROR( ret ) )
            break;

        oIfCfg.RemoveProperty( propRouterPtr );

    }while( 0 );

    if( ERROR( ret ) )
    {
        std::string strMsg = DebugMsg( ret,
            "Error occurs in"
            "CK5AuthServer's ctor" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CK5AuthServer::OnStartKdcProxyComplete(
    IEventSink* pInvTask, 
    IEventSink* pIoReq,
    IConfigDb* pReqCtx )
{
    if( pInvTask == nullptr ||
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
        {
            ret = iRet;
            break;
        }

    }while( 0 );

    if( ERROR( ret ) )
    {
        CCfgOpener oParams;
        oParams[ propReturnValue ] = ret;
        OnServiceComplete(
            oParams.GetCfg(), pInvTask );
    }

    return ret;
}

gint32 CK5AuthServer::OnSendKdcRequestComplete(
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
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            DebugPrint( iRet,
                "SendKdcRequest completed with"
                " error" );
            ret = iRet;
            break;
        }

        BufPtr pToken;
        ret = oResp.GetProperty(
            0, pToken );

        if( ERROR( ret ) )
            break;

        CParamList oArg0;
        oArg0.Push( pToken );
        oResp.SetPointer( 0,
            ( IConfigDb* )oArg0.GetCfg() );

        OnServiceComplete(
            pResp, pCallback );

    }while( 0 );

    if( ret == -STATUS_MORE_PROCESS_NEEDED )
        return ret;

    if( ERROR( ret ) )
    {
        oParams[ propReturnValue ] = ret;
        OnServiceComplete(
            oParams.GetCfg(), pCallback );

        gint32 iRet = 0;
        // stop the kdc proxy
        TaskletPtr pStopTask;
        iRet = DEFER_IFCALLEX_NOSCHED2( 0,
            pStopTask, ObjPtr( m_pKdcProxy ),
            &CRpcServices::Shutdown,
            nullptr );
        if( ERROR( iRet ) )
        {
            ( *pStopTask )( eventCancelTask );
        }
        else
        {
            iRet = this->AddSeqTask(
                pStopTask );
            if( ERROR( iRet ) )
                ( *pStopTask )( eventCancelTask );
        }
    }
    return ret;
}

gint32 CK5AuthServer::SendKdcRequest(
    IEventSink* pCallback,
    IConfigDb* pReq,
    bool bFirst ) /*[ in ]*/
{
    if( pCallback == nullptr ||
        pReq == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        IConfigDb* pCtx;
        CCfgOpener oReq( pReq );

        ret = oReq.GetPointer( 0, pCtx );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        BufPtr pRealm;
        ret = oReq.GetProperty( 1, pRealm );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        BufPtr pToken;
        ret = oReq.GetProperty( 2, pToken );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }
        
        BufPtr pRespBuf;
        CKdcRelayProxy* pProxy =
            m_pKdcProxy;
        if( pProxy != nullptr &&
            pProxy->IsConnected() )
        {
            ret = pProxy->MechSpecReq(
                pCallback, pToken,
                pRespBuf );

            if( ERROR( ret ) )
                break;

            if( ret == STATUS_PENDING )
                break;

            CParamList oResp;
            oResp[ propReturnValue ] = ret;
            if( !pRespBuf.IsEmpty() && 
                !pRespBuf->empty() )
            {
                CParamList oArg0;
                oArg0.Push( pRespBuf );
                oResp.Push(
                    oArg0.GetCfg() );
            }

            SetResponse(
                pCallback, oResp.GetCfg() );

            break;
        }
        else if( bFirst )
        {
            m_pKdcProxy.Clear();
            // start the kdcrelay proxy and
            // resend the request
            ret = STATUS_MORE_PROCESS_NEEDED;
        }
        else
        {
            // don't know how to handle
            ret = -EFAULT;
        }

    }while( 0 );

    if( ret == STATUS_MORE_PROCESS_NEEDED )
    {
        // move on to create the proxy
        return -STATUS_MORE_PROCESS_NEEDED;
    }

    if( ERROR( ret ) )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        SetResponse( pCallback,
            oResp.GetCfg() );
    }

    return ret;
}

gint32 CK5AuthServer::BuildSendReqTask(
    IEventSink* pInvTask,
    IConfigDb* pReq,
    bool bFirst,
    TaskletPtr& pTask )
{
    if( pInvTask == nullptr ||
        pReq == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        TaskletPtr pSendTask;
        ret = DEFER_CALL_NOSCHED(
            pSendTask, ObjPtr( this ),
            &CK5AuthServer::SendKdcRequest,
            ( IEventSink* ) nullptr,
            pReq, bFirst );

        if( ERROR( ret ) )
            break;

        TaskletPtr pIoCall;
        ret = NEW_PROXY_IOTASK(
            0, pIoCall, pSendTask, ObjPtr( this ),
            &CK5AuthServer::OnSendKdcRequestComplete,
            pInvTask, pReq );

        if( ERROR( ret ) )
            break;

        pTask = pIoCall;
            
    }while( 0 );

    return ret;
}


gint32 CK5AuthServer::StartKdcProxy(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CParamList oParams;

        oParams[ propIoMgr ] =
            ObjPtr( GetIoMgr() );

        ret = CRpcServices::LoadObjDesc(
            KDCRELAY_DESC_FILE,
            "KdcRelayServer",
            false,
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        oParams[ propParentPtr ] =
            ObjPtr( this );

        oParams[ propIfStateClass ] =
            clsid( CKdcRelayProxyStat );

        oParams[ propPortClass ] =
            PORT_CLASS_KDCRELAY_PDO;

        ret = m_pKdcProxy.NewObj(
            clsid( CKdcRelayProxy ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = m_pKdcProxy->StartEx( pCallback );

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::MechSpecReq(
    IEventSink* pCallback,
    IConfigDb* pReq,/*[ in ]*/
    CfgPtr& pResp )/*[ out ]*/
{
    if( pCallback == nullptr ||
        pReq == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oReq( pReq ); 
        std::string strMethod;
        ret = oReq.GetStrProp(
            propMethodName, strMethod );
        if( ERROR( ret ) )
            break;

        if( strMethod !=
            SYS_METHOD( AUTH_METHOD_MECHSPECREQ ) )
        {
            ret = -ENOTSUP;
            break;
        }

        TaskletPtr pSendTask;
        ret = BuildSendReqTask(
            pCallback, pReq, true, pSendTask );
        if( ERROR( ret ) )
            break;

        TaskletPtr pRespTask;
        ret = NEW_PROXY_RESP_HANDLER2(
            pRespTask, ObjPtr( this ),
            &CK5AuthServer::OnStartKdcProxyComplete,
            pCallback, pReq );

        if( ERROR( ret ) )
            break;

        TaskletPtr pStartTask;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pStartTask, ObjPtr( this ),
            &CK5AuthServer::StartKdcProxy,
            nullptr );

        if( ERROR( ret ) )
            break;
            
        CIfRetryTask* pRetryTask = pStartTask;
        pRetryTask->SetClientNotify( pRespTask );

        TaskletPtr pSendTask2;
        ret = BuildSendReqTask(
            pCallback, pReq, false, pSendTask2 );
        if( ERROR( ret ) )
            break;

        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );
        TaskGrpPtr pStartGrp;
        ret = pStartGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            return ret;

        pStartGrp->SetRelation( logicAND );
        pStartGrp->AppendTask( pStartTask );
        pStartGrp->AppendTask( pSendTask2 );

        TaskGrpPtr pSendGrp;
        ret = pSendGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            return ret;

        TaskletPtr pGrp = pStartGrp;
        pSendGrp->SetRelation( logicOR );
        pSendGrp->AppendTask( pSendTask );
        pSendGrp->AppendTask( pGrp );

        pSendTask = pSendGrp;
        ret = AddSeqTask( pSendTask );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::OnPostStart(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        gss_cred_id_t pCred =
            GSS_C_NO_CREDENTIAL;

        CCfgOpenerObj oIfCfg( this );

        IConfigDb* pAuth = nullptr;
        ret = oIfCfg.GetPointer(
            propAuthInfo, pAuth );
        if( ERROR( ret ) )
            break;

        CCfgOpener oAuth( pAuth );
        std::string strSvcName;
        ret = oAuth.GetStrProp(
            propServiceName, strSvcName );
        if( ERROR( ret ) )
            break;

        ret = AcquireCred(
            strSvcName,
            GSS_C_NT_HOSTBASED_SERVICE,
            &pCred );

        if( SUCCEEDED( ret ) )
            m_pSvcCred = pCred;

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::OnPreStop(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        if( m_pKdcProxy.IsEmpty() )
            break;

        CRpcServices* pSvc = m_pKdcProxy;
        if( !pSvc->IsConnected() )
            break;

        ret = m_pKdcProxy->StopEx( pCallback ); 

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::OnPostStop(
    IEventSink* pCallback )
{
    m_pKdcProxy.Clear();
    OM_uint32 min_stat;

    gss_release_cred(
        &min_stat, &m_pSvcCred );

    m_pSvcCred = nullptr;
    return 0;
}

gint32 CK5AuthServer::GenSessHash(
    gss_ctx_id_t gssctx,
    guint32 dwPortId,
    std::string& strSess )
{

    if( dwPortId == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        BufPtr pBuf( true );
        ret = gss_sess_hash_partial(
            gssctx, pBuf );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( GetRouter() );
        if( unlikely( pRouter == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        InterfPtr pIf;
        ret = pRouter->GetBridge( dwPortId, pIf );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oIfCfg( ( CObjBase* )pIf );
        IConfigDb* pConn;
        ret = oIfCfg.GetPointer(
            propConnParams, pConn );
        if( ERROR( ret ) )
            break;

        ret = AppendConnParams( pConn, pBuf );
        if( ERROR( ret ) )
            break;

        guint64 qwSalt = pIf->GetObjId();
        qwSalt = htonll( qwSalt );

        pBuf->Append( ( char* )&qwSalt,
            sizeof( qwSalt ) );

        stdstr strRet;
        ret = gen_sess_hash( pBuf, strRet );
        if( ERROR( ret ) )
            break;

        strSess = "AU";
        strSess += strRet;

        DebugPrint( 0, "Sess hash is %s",
            strSess.c_str() );

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::GetPortId(
    const std::string& strSess,
    guint32& dwPortId )
{
    CStdRMutex oGssLock( GetGssLock() );
    ITRSP itr =
        m_mapSessToPortId.find( strSess );
    if( itr == m_mapSessToPortId.end() )
        return -ENOENT;

    dwPortId = itr->second;
    return 0;
}

gint32 CK5AuthServer::GetSess(
    guint32 dwPortId,
    std::string& strSess )
{
    CStdRMutex oGssLock( GetGssLock() );
    ITRPS itr =
        m_mapPortIdToSess.find( dwPortId );
    if( itr == m_mapPortIdToSess.end() )
        return -ENOENT;

    strSess = itr->second;
    return STATUS_SUCCESS;
}

gint32 CK5AuthServer::GetGssCtx(
    const std::string& strSess,
    gss_ctx_id_t& context )
{
    ITRSP itr =
        m_mapSessToPortId.find( strSess );

    if( itr == m_mapSessToPortId.end() )
        return -ENOENT;

    guint32 dwPortId = itr->second;
    ITRGSS itr1 =
        m_mapSessions.find( dwPortId );
    if( itr1 == m_mapSessions.end() )
        return -ENOENT;

    context = itr1->second;
    return STATUS_SUCCESS;
}
    

gint32 CK5AuthServer::IsNoEnc(
    const std::string& strSess )
{
    gss_ctx_id_t gssctx = GSS_C_NO_CONTEXT;
    gint32 ret = GetGssCtx( strSess, gssctx );

    if( ERROR( ret ) )
        return STATUS_SUCCESS;

    return ERROR_FALSE;
}


gint32 CK5AuthServer::AddSession(
    guint32 dwPortId, 
    const std::string& strSess )
{
    m_mapPortIdToSess[ dwPortId ] = strSess;
    m_mapSessToPortId[ strSess ] = dwPortId;
    return STATUS_SUCCESS;
}

gint32 CK5AuthServer::RemoveSession(
    guint32 dwPortId )
{
    // the caller makes sure the gss lock is
    // acquired.
    OM_uint32 min_stat;
    ITRGSS itr = m_mapSessions.find( dwPortId);
    if( itr != m_mapSessions.end() )
    {
        gss_ctx_id_t gssctx = itr->second;
        m_mapSessions.erase( itr );
        gss_delete_sec_context( &min_stat,
            &gssctx, GSS_C_NO_BUFFER );
    }

    ITRPS itr1 =
        m_mapPortIdToSess.find( dwPortId );
    if( itr1 != m_mapPortIdToSess.end() ) 
    {
        std::string strSess = itr1->second;
        m_mapPortIdToSess.erase( itr1 );
        m_mapSessToPortId.erase( strSess );
    }

    return STATUS_SUCCESS;
}

gint32 CK5AuthServer::RemoveSession(
    const std::string& strSess )
{
    // the caller makes sure the gss lock is
    // acquired.
    CStdRMutex oGssLock( GetGssLock() );
    ITRSP itr2 =
        m_mapSessToPortId.find( strSess );
    if( itr2 == m_mapSessToPortId.end() )
        return -ENOENT;

    guint32 dwPortId = itr2->second;
    return RemoveSession( dwPortId );
}

gint32 CK5AuthServer::IsSessExpired(
    const std::string& strSess )
{
    gint32 ret = 0;
    do{
        // the caller makes sure the gss lock is
        // acquired.
        gss_ctx_id_t context = GSS_C_NO_CONTEXT;
        CStdRMutex oGssLock( GetGssLock() );
        ret = GetGssCtx( strSess, context );
        if( ERROR( ret ) )
            break;

        ret = ERROR_FALSE;
        OM_uint32 maj_stat, min_stat, lifetime; 
        maj_stat = gss_inquire_context(
            &min_stat, context, nullptr, nullptr,
            &lifetime, nullptr, nullptr,
            nullptr, nullptr );
        if( maj_stat == GSS_S_COMPLETE )
        {
            if( lifetime == 0 )
                ret = STATUS_SUCCESS;
        }
        else
        {
            ret = ERROR_FAIL;
        }

    }while( 0 );

    return ret;
}


gint32 CK5AuthServer::Krb5Login(
    IEventSink* pCallback,
    guint32 dwPortId,
    BufPtr& pToken,
    BufPtr& pRespTok )
{
    gint32 ret = STATUS_SUCCESS;
    bool bFirst = false;

    gss_buffer_desc send_tok, recv_tok;
    gss_name_t      client;
    gss_OID         doid;
    OM_uint32       ret_flags, maj_stat, min_stat;

    gss_ctx_id_t context = GSS_C_NO_CONTEXT;

    CStdRMutex oGssLock( GetGssLock() );
    do{
        if( m_mapSessions.find( dwPortId ) ==
            m_mapSessions.end() )
            bFirst = true;

        if( m_pSvcCred == GSS_C_NO_CREDENTIAL )
        {
            ret = ERROR_STATE;
            break;
        }

        gss_buffer_desc* in_tok_ptr = GSS_C_NO_BUFFER;

        if( !( pToken.IsEmpty() ||
            pToken->empty() ) )
        {
            recv_tok.value = pToken->ptr();
            recv_tok.length = pToken->size();
            in_tok_ptr = &recv_tok;
        }

        if( !bFirst )
            context = m_mapSessions[ dwPortId ];

        maj_stat = gss_accept_sec_context(
            &min_stat, &context,
            m_pSvcCred, in_tok_ptr,
            GSS_C_NO_CHANNEL_BINDINGS,
            &client, &doid, &send_tok,
            &ret_flags,
            NULL,  /*  time_rec */
            NULL); /*  del_cred_handle */

        if( maj_stat != GSS_S_COMPLETE &&
            maj_stat != GSS_S_CONTINUE_NEEDED )
        {
            if( bFirst && context != GSS_C_NO_CONTEXT )
            {
                gss_delete_sec_context( &min_stat,
                    &context, GSS_C_NO_BUFFER );
            }
            ret = -EACCES;
            break;
        }

        if( send_tok.length > 0 )
        {
            if( pRespTok.IsEmpty() )
                pRespTok.NewObj();

            pRespTok->Append(
                ( guint8* )send_tok.value,
                send_tok.length );

            gss_release_buffer(
                &min_stat, &send_tok );
        }

        m_mapSessions[ dwPortId ] = context;
        gss_release_name(&min_stat, &client);

        if( maj_stat == GSS_S_CONTINUE_NEEDED )
            ret = STATUS_MORE_PROCESS_NEEDED;
        else
        {
            std::string strSess;
            ret = GenSessHash(
                context, dwPortId, strSess );
            if( SUCCEEDED( ret ) )
            {
                ret = AddSession(
                    dwPortId, strSess );
            }
        }

    }while( 0 );

    if( ERROR( ret ) )
        RemoveSession( dwPortId );

    return ret;
}

gint32 CK5AuthServer::NoTokenLogin(
    IEventSink* pCallback,
    guint32 dwPortId )
{
    std::string strSess;
    gint32 ret = STATUS_SUCCESS;

    CStdRMutex oGssLock( GetGssLock() );
    do{
        ret = GenSessHash( GSS_C_NO_CONTEXT,
            dwPortId, strSess );
        if( ERROR( ret ) )
            break;
        AddSession( dwPortId, strSess );

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::Login(
    IEventSink* pCallback,
    IConfigDb* pInfo, /*[ in ]*/
    CfgPtr& pResp ) /*[ out ]*/
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pInfo == nullptr )
        return -EINVAL;
    do{
        CCfgOpener oInfo( pInfo );
        bool bKrbLogin = false;;

        ret = oInfo.GetBoolProp(
            0, bKrbLogin );

        if( ERROR( ret ) )
            break;

        bool bFirst = true;
        ret = oInfo.GetBoolProp(
            1, bFirst );

        if( ERROR( ret ) )
            break;

        BufPtr pToken;
        if( bKrbLogin )
        {
            ret = oInfo.GetProperty(
                2, pToken );
            if( ERROR( ret ) )
                break;
        }

        DMsgPtr pMsg;
        CCfgOpenerObj oInvCfg( pCallback );
        ret = oInvCfg.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 1, pObj );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        CCfgOpener oReqCtx(
            ( IConfigDb* )pObj );
        ret = oReqCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        BufPtr pRespTok;
        std::string strSess;
        if( bKrbLogin )
        {
            ret = Krb5Login( pCallback,
                dwPortId, pToken, pRespTok ); 
        }
        else
        {
            ret = NoTokenLogin(
                pCallback, dwPortId );
        }

        if( pResp.IsEmpty() )
            pResp.NewObj();

        CParamList oResp( pResp );
        bool bContinue = false;
        if( ret == STATUS_MORE_PROCESS_NEEDED )
        {
            bContinue = true;
            ret = 0;
        }
        oResp[ propReturnValue ] = ret;
        oResp[ propContinue ] = bContinue;

        if( SUCCEEDED( ret ) )
        {
            if( !pRespTok.IsEmpty() &&
                !pRespTok->empty() )
                oResp.Push( pRespTok );

            CRpcRouterBridge* pRouter =
                static_cast< CRpcRouterBridge* >
                    ( GetRouter() );
            if( unlikely( pRouter == nullptr ) )
            {
                ret = -EFAULT;
                break;
            }

            InterfPtr pIf;
            ret = pRouter->GetBridge( dwPortId, pIf );
            if( ERROR( ret ) )
                break;
            oResp.SetQwordProp(
                propSalt, pIf->GetObjId() );
        }

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::WrapMessage(
    const std::string& strSess,
    BufPtr& pInBuf,
    BufPtr& pOutBuf )
{
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        gss_ctx_id_t context;
        ret = GetGssCtx( strSess, context );
        if( ERROR( ret ) )
            break;

        ret = wrap_message(
            context, pInBuf, pOutBuf );

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::UnwrapMessage(
    const std::string& strSess,
    BufPtr& pInBuf,
    BufPtr& pOutBuf )
{
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        gss_ctx_id_t context;
        ret = GetGssCtx( strSess, context );
        if( ERROR( ret ) )
            break;

        ret = unwrap_message(
            context, pInBuf, pOutBuf );

    }while( 0 );

    return ret;
}

gint32 CK5AuthServer::GetMicMsg(
    const std::string& strSess,
    BufPtr& pInBuf,
    BufPtr& pOutMic )
{
    if( pInBuf.IsEmpty() || pOutMic.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        gss_ctx_id_t context;
        ret = GetGssCtx( strSess, context );
        if( ERROR( ret ) )
            break;

        ret = get_mic(
            context, pInBuf, pOutMic );

    }while( 0 );

    return ret;
}
    
gint32 CK5AuthServer::VerifyMicMsg(
    const std::string& strSess,
    BufPtr& pInMsg,
    BufPtr& pInMic )
{
    if( pInMsg.IsEmpty() || pInMic.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        gss_ctx_id_t context;
        ret = GetGssCtx( strSess, context );
        if( ERROR( ret ) )
            break;

        ret = verify_message(
            context, pInMsg, pInMic );

    }while( 0 );

    return ret;
}

gint32 IAuthenticateServer::GetMicMsg2s(
    const std::string& strSess,
    BufPtr& pInMsg,/* [ in ] */
    BufPtr& pSecBuf )
{
    gint32 ret = 0;
    if( strSess.empty() || pInMsg.IsEmpty() )
        return -EINVAL;

    do{
        BufPtr pSig( true );
        ret = GetMicMsg( strSess, pInMsg, pSig );
        if( ERROR( ret ) )
            break;

        CCfgOpener oSecCtx;
        oSecCtx.SetProperty(
            propSignature, pSig );

        oSecCtx.SetStrProp(
            propSessHash, strSess );

        BufPtr pOutMic( true );
        ret = oSecCtx.Serialize( *pOutMic );
        if( ERROR( ret ) )
            break;

        pSecBuf = pOutMic;

    }while( 0 );

    return ret;
}

gint32 IAuthenticateServer::VerifyMicMsg2s(
    const std::string& strSess,
    BufPtr& pInMsg,/* [ in ] */
    BufPtr& pSecBuf )
{
    gint32 ret = 0;
    if( pInMsg.IsEmpty() ||
        pSecBuf.IsEmpty() )
        return -EINVAL;

    do{
        CCfgOpener oSecCtx;
        ret = oSecCtx.Deserialize( pSecBuf );
        if( ERROR( ret ) )
            break;

        std::string strSessIn;
        ret = oSecCtx.GetStrProp(
            propSessHash, strSessIn );
        if( ERROR( ret ) )
            break;

        if( strSess != strSessIn )
        {
            ret = -EINVAL;
            break;
        }

        BufPtr pSig;
        ret = oSecCtx.GetProperty(
            propSignature, pSig );
        if( ERROR( ret ) )
            break;

        ret = VerifyMicMsg(
            strSessIn, pInMsg, pSig );

    }while( 0 );

    return ret;
}

}
