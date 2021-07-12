/*
 * =====================================================================================
 *
 *       Filename:  k5proxy.cpp
 *
 *    Description:  implentation of the krb5 related classes 
 *
 *        Version:  1.0
 *        Created:  07/06/2020 11:38:12 AM
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

#include "rpc.h"
#include "ifhelper.h"
#include "security.h"
#include "k5proxy.h"

#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h> 
#include <krb5/locate_plugin.h>

// ---------------------------------
// from krb5's "os-proto.h"
typedef enum {
    TCP_OR_UDP = 0,
    TCP,
    UDP,
    HTTPS,
} k5_transport;

typedef enum {
    UDP_FIRST = 0,
    UDP_LAST,
    NO_UDP,
} k5_transport_strategy;

/*  A single server hostname or address. */
struct server_entry {
    char *hostname;             /*  NULL -> use addrlen/addr instead */
    int port;                   /*  Used only if hostname set */
    k5_transport transport;     /*  May be 0 for UDP/TCP if hostname set */
    char *uri_path;             /*  Used only if transport is HTTPS */
    int family;                 /*  May be 0 (aka AF_UNSPEC) if hostname set */
    int master;                 /*  True, false, or -1 for unknown. */
    size_t addrlen;
    struct sockaddr_storage addr;
};

/*  A list of server hostnames/addresses. */
struct serverlist {
    struct server_entry *servers;
    size_t nservers;
};

struct module_callback_data {
    int out_of_mem;
    struct serverlist *list;
};
// -------------------------------------

namespace rpcf
{

CKrb5InitHook::CKrb5InitHook(
    const IConfigDb* pCfg )
    : super()
{
    if( pCfg == nullptr )
        return;

    SetClassId( clsid( CKrb5InitHook ) );
    gint32 ret = 0;
    do{
        ObjPtr pMgr;
        CCfgOpener oCfg( pCfg );
        ret = oCfg.GetObjPtr(
            propIoMgr, pMgr );

        if( ERROR( ret ) )
            break;
        m_pMgr = pMgr;

    }while( 0 );

    return;
}

gint32 CKrb5InitHook::Start()
{
    if( m_bInRouter )
        return 0;

    if( m_bInited == true )
        return 0;

    // to start from krb5's client
    return StartProxy();
}

gint32 CKrb5InitHook::StartProxy()
{
    gint32 ret = 0;
    do{
        if( m_pMgr.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        CCfgOpener oCfg;
        InterfPtr pIf;

        CfgPtr pCfg = oCfg.GetCfg();

        // create the interface server
        oCfg.SetObjPtr( propIoMgr, m_pMgr );
        ret = CRpcServices::LoadObjDesc(
            DESC_FILE, OBJNAME_KDCCHANNEL,
            false, pCfg );

        ret = pIf.NewObj(
            clsid( CKdcChannelProxy ),
            oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        // start the server
        ret = pIf->Start();
        if( ERROR( ret ) )
            break;
        
        m_bInited = true;

        CRpcServices* pSvc = pIf;
        // waiting for requests
        while( pSvc->GetState() ==
            stateRecovery )
        {
            DebugPrint( 0,
                "remote server is not online?" );
            sleep( 1 );
        }

        if( !pSvc->IsConnected() )
            ret = ERROR_STATE;

    }while( 0 );

    return ret;
}

gint32 CKrb5InitHook::StopProxy()
{
    gint32 ret = 0;
    if( m_bInRouter )
        return 0;

    if( !m_bInited )
        return 0;

    do{
        CRpcServices* pIf = m_pProxy;
        if( pIf == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        // stop the server
        ret = pIf->Stop();
        m_pProxy.Clear();

    }while( 0 );

    return ret;
}

extern "C" {
krb5plugin_service_locate_ftable service_locator =
{
    0,
    &CInitHookMap::PluginInit,
    &CInitHookMap::PluginFinish, 
    &CInitHookMap::Lookup
};
};

CInitHookMap g_oHookMap;
gint32 CInitHookMap::Start()
{
    if( !m_pMgr.IsEmpty() )
        return 0;

    if( m_bInRouter )
        return 0;

    gint32 ret = 0;
    do{
        ret = CoInitialize( 0 );
        if( ERROR( ret ) )
            break;

        BufPtr pBuf( true );
        ObjPtr pMgr;
        CParamList oParams;

        ret = oParams.Push(
            std::string( "KdcChannel" ) );

        if( ERROR( ret ) )
            break;

        oParams.SetIntProp(
            propMaxIrpThrd, 0 );

        oParams.SetIntProp(
            propMaxTaskThrd, 1 );

        ret = pMgr.NewObj(
            clsid( CIoManager ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        IService* pSvc = pMgr;
        ret = pSvc->Start();
        if( ERROR( ret ) )
            break;

        m_pMgr = pMgr;

    }while( 0 );

    return ret;
}

gint32 CInitHookMap::Stop()
{
    m_mapInitHooks.clear();
    if( m_pMgr.IsEmpty() )
        return 0;

    if( m_bInRouter )
        return 0;

    IService* pSvc = m_pMgr;
    if( pSvc == nullptr )
        return 0;

    pSvc->Stop();
    m_pMgr.Clear();

    return CoUninitialize();
}

CInitHookMap::~CInitHookMap()
{
    Stop();
}

void CInitHookMap::OnKrb5CtxInit(
    krb5_context ctx, void* pobj )
{
    if( pobj == nullptr || ctx == nullptr )
        return;

    // set the send hook
    krb5_pre_send_fn send_hook =
        &Krb5SendHook;

    krb5_set_kdc_send_hook(
        ctx, send_hook, pobj );

    return;
}

gint32 CInitHookMap::CreateNewHook(
    ObjPtr& pHook )
{
    gint32 ret = 0;
    do{
        CParamList oParams;
        oParams.SetPointer(
            propIoMgr, ( CObjBase* )m_pMgr );

        CCfgOpener oCfg;
        InterfPtr pIf;

        CfgPtr pCfg = oCfg.GetCfg();
        oCfg.SetObjPtr( propIoMgr, m_pMgr );
        ret = CRpcServices::LoadObjDesc(
            DESC_FILE,
            OBJNAME_KDCCHANNEL,
            false, pCfg );
        if( ERROR( ret ) )
            break;

        // create the interface server
        oCfg.SetObjPtr( propIoMgr, m_pMgr );
        ret = pIf.NewObj(
            clsid( CKdcChannelProxy ),
            oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = pIf->Start();
        if( ERROR( ret ) )
        {
            pIf->Stop();
            break;
        }

        ret = pHook.NewObj(
            clsid( CKrb5InitHook ) );
        if( ERROR( ret ) )
        {
            pIf->Stop();
            break;
        }

        CKrb5InitHook* pKrbHook = pHook;
        pKrbHook->SetIoMgr( m_pMgr );
        pKrbHook->m_bInRouter = m_bInRouter;
        pKrbHook->m_bInited = true;
        pKrbHook->m_pProxy = pIf;

        g_oHookMap.SetInitHook( pHook );

    }while( 0 );

    if( ERROR( ret ) && !m_pMgr.IsEmpty() )
    {
        IService* pSvc = m_pMgr;
        pSvc->Stop();
        m_pMgr.Clear();
    }

    return ret;
}

krb5_error_code CInitHookMap::PluginInit(
    krb5_context ctx, void** )
{
    CKrb5InitHook* pHook = nullptr;
    ObjPtr pObj;
    g_oHookMap.Start();
    gint32 ret = g_oHookMap.GetInitHook( pHook );

    if( ERROR( ret ) )
    {
        if( g_oHookMap.IsInRouter() )
            return -ret;

        if( g_oHookMap.size() > 0 )
            pHook = g_oHookMap.GetFirst();
        else
        {
            ret =
            g_oHookMap.CreateNewHook( pObj );

            if( ERROR( ret ) )
                return -ret;

            pHook = pObj;
        }
    }

    CStdRMutex oLock( pHook->GetLock() );
    if( !pHook->IsInited() )
        return 0;

    CRpcServices* p =
        pHook->GetProxy();

    if( p == nullptr )
        return EFAULT;

    if( p != nullptr )
        OnKrb5CtxInit( ctx, p );

    return 0;
}

krb5_error_code CInitHookMap::Lookup(
    void *, enum locate_service_type svc,
    const char *realm,
    int socktype, int family,
    int (*cbfunc)(void *,int,struct sockaddr *), 
    void *cbdata )
{
    // We are not serious about address lookup
    // return NOT_HANDLE to pass on the
    // control
    gint32 iRet = KRB5_PLUGIN_NO_HANDLE;

    CKrb5InitHook* pHook; 
    gint32 ret = g_oHookMap.GetInitHook( pHook );

    if( ERROR( ret ) )
        return iRet;

    CStdRMutex oLock( pHook->GetLock() );
    if( !pHook->IsInited() )
        return iRet;

    CRpcServices* p = pHook->GetProxy();
    do{
        if( socktype != SOCK_STREAM &&
            socktype != SOCK_DGRAM )
            break;
        
        std::string strAuth;
        CCfgOpener oAuth(
            ( IConfigDb* )GET_AUTH( p ) );

        ret = oAuth.GetStrProp(
            propRealm, strAuth );

        if( ERROR( ret ) )
            break;

        if( strAuth == realm )
        {
            module_callback_data* pcb =
                ( module_callback_data* )cbdata;

            if( pcb == 0 )
                break;

            if( pcb->list == nullptr )
                break;

            if( pcb->list->nservers > 0 )
                break;

            pcb->list->servers =
                ( server_entry* )calloc(
                    1, sizeof( server_entry ) );

            if( pcb->list->servers == nullptr )
                break;

            pcb->list->nservers = 1;
            server_entry* pse = pcb->list->servers;
            pse->transport = TCP_OR_UDP;

            IConfigDb* pConnParams = nullptr;
            CCfgOpenerObj oCfg( p );
            ret = oCfg.GetPointer(
                propConnParams, pConnParams );
            if( ERROR( ret ) )
                break;

            CCfgOpener oConn( pConnParams );

            bool bip6 = false;
            std::string strFormat;
            ret = oConn.GetStrProp(
                propAddrFormat, strFormat );

            if( SUCCEEDED( ret ) &&
                strFormat == "ipv6"  )
                bip6 = true;

            pse->family = AF_INET;
            if( bip6 )
                pse->family = AF_INET6;

            std::string strIpAddr;
            ret = oConn.GetStrProp(
                propDestIpAddr, strIpAddr );
            if( ERROR( ret ) )
                break;

            guint32 dwPortNum = 0;
            ret = oConn.GetIntProp(
                propDestTcpPort, dwPortNum );
            if( ERROR( ret ) )
                break;
            pse->master = -1;

            addrinfo* pAddrInfo = nullptr;

            ret = CRpcSocketBase::GetAddrInfo(
                strIpAddr,
                dwPortNum,
                pAddrInfo );
            if( ERROR( ret ) )
                break;

            pse->addrlen =
                pAddrInfo->ai_addrlen;

            memcpy( &pse->addr,
                pAddrInfo->ai_addr,
                pse->addrlen );

            freeaddrinfo( pAddrInfo );
            iRet = 0;
        }

    }while( 0 );

    return iRet;
}

// Make sure to run this blocking method on a
// stand-alone thread.
krb5_error_code CInitHookMap::Krb5SendHook(
    krb5_context context, void *data,
    const krb5_data *realm,
    const krb5_data *message,
    krb5_data **new_message_out,
    krb5_data **reply_out)  
{
    gint32 ret = 0;

    if( data == nullptr || context == nullptr ||
        realm == nullptr || message == nullptr )
        return 0;

    do{
        BufPtr pRealm( true );
        if( realm->length >
            MAX_BYTES_PER_TRANSFER )
        {
            ret = -EINVAL;
            break;
        }

        pRealm->Resize( realm->length + 1);
        pRealm->ptr()[ realm->length ] = 0;
        memcpy( pRealm->ptr(),
            realm->data, realm->length );

        BufPtr pMsg( true );
        if( message->length >
            MAX_BYTES_PER_TRANSFER )
        {
            ret = -EINVAL;
            break;
        }

        pMsg->Resize( sizeof( guint32 ) );
        guint32 dwSize = ntohl( message->length );
        memcpy( pMsg->ptr(),
            &dwSize, sizeof( guint32 ) );
       
        ret = pMsg->Append(
            message->data, message->length );

        CRpcServices* pProxy = reinterpret_cast
            < CRpcServices* >( data );

        IConfigDb* pAuth =
            GET_AUTH( pProxy );

        CCfgOpener oCfg;
        ret = oCfg.CopyProp(
            propUserName, pAuth );

        if( ERROR( ret ) )
            break;
        
        CParamList oParams;

        oParams.SetStrProp(
            propMethodName, SYS_METHOD(
            AUTH_METHOD_MECHSPECREQ ) );

        oParams.Push( oCfg.GetCfg() );
        oParams.Push( pRealm );
        oParams.Push( pMsg );

        CfgPtr pResp;

        if( pProxy->GetClsid() ==
            clsid( CKdcChannelProxy ) )
        {
            CKdcChannelProxy* pkc = dynamic_cast
                < CKdcChannelProxy* >( pProxy );
            if( pkc == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            ret = pkc->MechSpecReq( nullptr,
                oParams.GetCfg(), pResp );

            if( ERROR( ret ) )
                break;
        }
        else if( pProxy->GetClsid() ==
            clsid( CAuthentProxyK5Impl ) )
        {
            CK5AuthProxy* pkc = dynamic_cast
                < CK5AuthProxy* >( pProxy );
            if( pkc == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            ret = pkc->MechSpecReq( nullptr,
                oParams.GetCfg(), pResp );

            if( ERROR( ret ) )
                break;
        }

        BufPtr pToken;
        CCfgOpener oResp( ( IConfigDb* )pResp );
        ret = oResp.GetProperty( 0, pToken );
        if( ERROR( ret ) )
            break;

        if( pToken->GetDataType() != DataTypeMem )
        {
            ret = -EINVAL;
            break;
        }

        if( pToken->empty() ||
            pToken->size() < sizeof( guint32 ) )
        {
            ret = ERROR_FAIL;
            break;
        }

        dwSize = htonl(
            *( guint32* )pToken->ptr() );

        if( dwSize != pToken->size() -
            sizeof( guint32 ) )
        {
            ret = -EBADMSG;
            break;
        }

        *reply_out = ( krb5_data* )calloc( 1,
            sizeof( krb5_data ) );

        if( reply_out == nullptr )
        {
            ret = -ENOMEM;
            break;
        }

        krb5_data* pdata = *reply_out;
        pdata->magic = KV5M_DATA;
        pdata->length = pToken->size() -
            sizeof( guint32 );

        pdata->data = ( char* )malloc( pdata->length );

        memcpy( pdata->data,
            pToken->ptr() + sizeof( guint32 ),
            pToken->size() );

    }while( 0 );

    return ret;
}

gint32 CInitHookMap::GetInitHook(
    CKrb5InitHook*& pHook )
{
    CStdRMutex oLock( GetLock() );
    guint32 dwTid = GetTid();
    if( m_mapInitHooks.find( dwTid ) ==
        m_mapInitHooks.end() )
        return -ENOENT;

    pHook = m_mapInitHooks[ dwTid ];
    return 0;
}

gint32 CInitHookMap::SetInitHook(
    CKrb5InitHook* pHook )
{
    if( pHook == nullptr )
        return -EINVAL;

    CStdRMutex oLock( GetLock() );
    guint32 dwTid = GetTid();
    if( m_mapInitHooks.find( dwTid ) !=
        m_mapInitHooks.end() )
        return -EEXIST;

    m_mapInitHooks[ dwTid ] = ObjPtr( pHook );
    return 0;
}

gint32 CInitHookMap::RemoveHook()
{
    CStdRMutex oLock( GetLock() );
    guint32 dwTid = GetTid();
    m_mapInitHooks.erase( dwTid );
    return 0;
}

gint32 IAuthenticateProxy::GetMicMsg2p(
    BufPtr& pInMsg,/* [ in ] */
    BufPtr& pOutMic )
{
    gint32 ret = 0;
    if( pInMsg.IsEmpty() || pInMsg->empty() )
        return -EINVAL;

    do{
        CRpcServices* pSvc = m_pParent;
        if( pSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CIoManager* pMgr = pSvc->GetIoMgr();
        CMainIoLoop* pLoop =
            pMgr->GetMainIoLoop();

        // append a timestamp as salt
        guint32 dwSize = pInMsg->size();
        guint64 qwts = pLoop->NowUsFast();

        pInMsg->Resize(
            dwSize + sizeof( qwts ) );

        char* pst =
            pInMsg->ptr() + dwSize;

        memcpy( pst, &qwts, sizeof( qwts ) );

        BufPtr pSig( true );
        ret = GetMicMsg( pInMsg, pSig );
        if( ERROR( ret ) )
            break;

        pInMsg->Resize( dwSize );

        std::string strSess;
        ret = GetSess( strSess );
        if( ERROR( ret ) )
            break;

        CCfgOpener oSecCtx;
        oSecCtx.SetProperty(
            propSignature, pSig );

        oSecCtx.SetStrProp(
            propSessHash, strSess );

        oSecCtx.SetQwordProp(
            propTimestamp, qwts );

        BufPtr pSecBuf( true );
        ret = oSecCtx.Serialize( *pSecBuf );
        if( ERROR( ret ) )
            break;

        pOutMic = pSecBuf;

    }while( 0 );

    return ret;
}

gint32 IAuthenticateProxy::VerifyMicMsg2p(
    BufPtr& pInMsg,/* [ in ] */
    BufPtr& pSecCtx )
{
    gint32 ret = 0;
    if( pInMsg.IsEmpty() ||
        pSecCtx.IsEmpty() )
        return -EINVAL;

    if( pInMsg->empty() ||
        pSecCtx->empty() )
        return -EINVAL;

    do{
        CCfgOpener oSecCtx;
        ret = oSecCtx.Deserialize( pSecCtx );
        if( ERROR( ret ) )
            break;

        guint64 qwts = 0;
        ret = oSecCtx.GetQwordProp(
            propTimestamp, qwts );
        if( ERROR( ret ) )
            break;

        std::string strSessIn;
        ret = oSecCtx.GetStrProp(
            propSessHash, strSessIn );
        if( ERROR( ret ) )
            break;

        std::string strCurSess;
        if( strCurSess != strSessIn )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwSize = pInMsg->size();
        pInMsg->Resize(
            dwSize + sizeof( qwts ) );
        char* pts =
            pInMsg->ptr() + dwSize;

        memcpy( pts, &qwts, sizeof( qwts ) );

        BufPtr pSig;
        ret = oSecCtx.GetProperty(
            propSignature, pSig );
        if( ERROR( ret ) )
            break;

        ret = VerifyMicMsg( pInMsg, pSig );
        pInMsg->Resize( dwSize );
        if( ERROR( ret ) )
            break;

        CRpcServices* pSvc = m_pParent;
        if( unlikely( pSvc == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        CIoManager* pMgr = pSvc->GetIoMgr();
        CMainIoLoop* pLoop =
            pMgr->GetMainIoLoop();
        guint64 qwNow = pLoop->NowUsFast();
        if( qwNow < qwts )
        {
            ret = -EINVAL;
            break;
        }

        CCfgOpenerObj oCfg( pSvc );
        guint32 dwTimeoutSec = 0;
        ret = oCfg.GetIntProp(
            propTimeoutSec, dwTimeoutSec );

        if( ERROR( ret ) )
            break;

        qwts += ( ( guint64 )dwTimeoutSec ) *
            1000 * 1000;

        if( qwNow > qwts )
        {
            // the msg is too old
            ret = -EINVAL;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CK5AuthProxy::InitEnvRouter(
    CIoManager* pMgr )
{
    g_oHookMap.SetInRouter( true );
    g_oHookMap.SetIoMgr( pMgr );
    g_oHookMap.Start();

    return 0;
}

CK5AuthProxy::CK5AuthProxy(
    const IConfigDb* pCfg ) :
    super( pCfg )
{
    gint32 ret = 0;
    do{
        CCfgOpener oCfg( pCfg );
        CRpcServices* pSvc = nullptr;

        ret = oCfg.GetPointer(
            propRouterPtr, pSvc );

        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        m_pRouter = ObjPtr( pSvc );

        CCfgOpenerObj oIfCfg( this );
        oIfCfg.RemoveProperty(
            propRouterPtr );

    }while( 0 );

    return;
}

bool CK5AuthProxy::IsConnected(
    const char* szDestAddr )
{
    if( !super::IsConnected( szDestAddr ) )
        return false;

    if( m_pParent.IsEmpty() )
        return true;

    CRpcServices* pSvc = m_pParent;
    if( pSvc == nullptr )
        return false;

    return pSvc->IsConnected( szDestAddr );
}

gint32 CK5AuthProxy::InitUserFuncs()
{
    BEGIN_IFPROXY_MAP( IAuthenticate, false );

    ADD_PROXY_METHOD_ASYNC( 1,
        CK5AuthProxy::Login,
        SYS_METHOD( AUTH_METHOD_LOGIN ) );

    ADD_PROXY_METHOD_ASYNC( 1,
        CK5AuthProxy::MechSpecReq,
        SYS_METHOD( AUTH_METHOD_MECHSPECREQ ) );

    END_IFPROXY_MAP;
    return 0;
}

gint32 CK5AuthProxy::Login(
    IEventSink* pCallback,
    IConfigDb* pInfo, /*[ in ]*/
    CfgPtr& pResp ) /*[ out ]*/
{
    if( pInfo == nullptr )
        return -EINVAL;

    EnumClsid iid = CK5AuthProxy::GetIid();

    return FORWARD_SYSIF_CALL(
        iid, 1, AUTH_METHOD_LOGIN,
        pInfo, pResp );
}

gint32 CK5AuthProxy::MechSpecReq(
    IEventSink* pCallback,
    IConfigDb* pInfo,/*[ in ]  pInfo */
    CfgPtr& pResp ) /* [ out ] pResp */
{
    if( pInfo == nullptr )
        return -EINVAL;

    EnumClsid iid = CK5AuthProxy::GetIid();

    return FORWARD_SYSIF_CALL(
        iid, 1, AUTH_METHOD_MECHSPECREQ,
        pInfo, pResp );
}

gint32 CK5AuthProxy::OnPostStop(
    IEventSink* pCallback )
{
    DeleteSecCtx();
    m_pParent.Clear();
    return 0;
}

gint32 CK5AuthProxy::OnPreStart(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        std::string strPath;
        CCfgOpenerObj oIfCfg( this );

        ret = oIfCfg.GetStrProp(
            propRouterPath, strPath );

        if( ERROR( ret ) )
            break;

        if( strPath != "/" )
        {
            // don't accept router path other
            // than root path.
            ret = -EINVAL;
            break;
        }

    }while( 0 );

    if( ERROR( ret ) )
        return ret;

    return 0;
}
/**
* @name AcquireCred
* @brief To get the credential for the user
* `strName'.
* @{
* @NOTE: gss_acquire_cred could fetch the tgt from
* the `KDC' during the call, which is synchronous.
* so the whole login process is made synchronous
* on a stand-alone thread.
*  */
/**  @} */

gint32 AcquireCred(
    const std::string& strName,
    gss_OID& oNameType,
    gss_cred_id_t* pServerCred )
{
    gint32 ret = 0;
    gss_name_t princ_name = GSS_C_NO_NAME;
    OM_uint32 maj_stat, min_stat;

    do{
        gss_buffer_desc name_buf;
        gss_OID_set_desc mech_oid_set;
        gss_OID mech_oid = gss_mech_krb5;

        gss_OID_set desired_mechs = &mech_oid_set;
        mech_oid_set.count = 1;
        mech_oid_set.elements = mech_oid;

        if( strName.size() > 0 )
        {
            BufPtr pNameBuf( true );
            pNameBuf->Resize( strName.size() );
            memcpy( pNameBuf->ptr(), strName.c_str(),
                strName.size() );

            name_buf.value = pNameBuf->ptr();
            name_buf.length = pNameBuf->size();

            maj_stat = gss_import_name(
                &min_stat, &name_buf,
                oNameType, &princ_name );

            if( maj_stat != GSS_S_COMPLETE )
            {
                ret = ERROR_FAIL;
                break;
            }
        }

        int cred_usage = GSS_C_INITIATE;
        if( &oNameType ==
            &GSS_C_NT_HOSTBASED_SERVICE )
            cred_usage = GSS_C_ACCEPT;

        maj_stat = gss_acquire_cred(
            &min_stat, princ_name, 0,
            desired_mechs, cred_usage,
            pServerCred, NULL, NULL );
        if( maj_stat != GSS_S_COMPLETE )
        {
            ret = -EACCES;
            break;
        }

    }while( 0 );

    gss_release_name( &min_stat, &princ_name );
    return ret;
}

gint32 CK5AuthProxy::Krb5Login(
    const std::string& strUserName,
    const std::string& strSvcName )
{
    gss_buffer_desc send_tok;
    gss_buffer_desc *in_token_ptr = nullptr;
    gss_buffer_desc in_token;
    gss_name_t target_name;
    OM_uint32 maj_stat, min_stat;
    gss_OID oid;
    bool bFirst = true;
    gint32 ret = 0;
    gss_ctx_id_t gss_context;
    OM_uint32 ret_flags;
    gss_cred_id_t pCred = GSS_C_NO_CREDENTIAL;

    do{
        ret = AcquireCred( strUserName,
            GSS_C_NT_USER_NAME, &pCred );
        if( ERROR( ret ) )
            break;

        if( strSvcName.empty() )
        {
            ret = -EINVAL;
            break;
        }

        BufPtr pNameBuf( true );
        pNameBuf->Resize( strSvcName.size() );
        memcpy( pNameBuf->ptr(),
            strSvcName.c_str(), strSvcName.size() );

        send_tok.value = pNameBuf->ptr();
        send_tok.length = pNameBuf->size();

        maj_stat = gss_import_name(
            &min_stat, &send_tok,
            ( gss_OID )GSS_C_NT_HOSTBASED_SERVICE,
            &target_name );

        if( maj_stat != GSS_S_COMPLETE )
        {
            ret = ERROR_FAIL;
            break;
        }

        in_token_ptr = GSS_C_NO_BUFFER;
        gss_context = GSS_C_NO_CONTEXT;
        CfgPtr pResp;
        guint64 qwSalt = 0;

        do{
            gss_OID mech_oid = gss_mech_krb5;
            maj_stat = gss_init_sec_context(
                &min_stat,
                pCred,
                &gss_context,
                target_name,
                mech_oid,
                GSS_C_MUTUAL_FLAG |
                GSS_C_REPLAY_FLAG,
                0,
                NULL,
                in_token_ptr,
                &oid,
                &send_tok,
                &ret_flags,
                NULL );

            if( gss_context == GSS_C_NO_CONTEXT )
            {
                ret = -EACCES;
                break;
            }

            if( maj_stat != GSS_S_COMPLETE &&
                maj_stat != GSS_S_CONTINUE_NEEDED )
            {
                ret = -EACCES;
                break;
            }

            CParamList oParams;

            if( send_tok.length > 0 )
            {
                BufPtr pArg1( true );
                pArg1->Append(
                    ( char* )send_tok.value,
                    send_tok.length );

                // there is a token to send to
                // peer
                oParams.Push( true ); 
                oParams.Push( bFirst );
                oParams.Push( pArg1 );

                bFirst = false;
                pResp.Clear();

                ret = Login( nullptr,
                    oParams.GetCfg(), pResp );
                if( ERROR( ret ) )
                    break;

                if( pResp.IsEmpty() )
                {
                    ret = -EFAULT;
                    break;
                }

                CCfgOpener oResp(
                    ( IConfigDb* )pResp );

                ret = oResp.GetQwordProp(
                    propSalt, qwSalt );

                if( ERROR( ret ) )
                    break;

                if( maj_stat == GSS_S_CONTINUE_NEEDED )
                {
                    BufPtr pToken;
                    ret = oResp.GetProperty(
                        0, pToken );
                    if( ERROR( ret ) )
                        break;

                    in_token.value =
                        pToken->ptr();

                    in_token.length =
                        pToken->size();

                    in_token_ptr = &in_token;

                    gss_release_buffer(
                        &min_stat, &send_tok );

                    continue;
                }

                // GSS_S_COMPLETE
                break;
            }
            else
            {
                if( maj_stat == GSS_S_COMPLETE &&
                    !bFirst )
                {
                    // normal complete
                    if( ERROR( ret ) )
                        ret = -EACCES;

                    break;
                }
                if( maj_stat == GSS_S_COMPLETE &&
                    bFirst )
                {
                    oParams.Push( false );

                    // both user requests and kdc
                    // traffics.
                    oParams.Push( false );
                    // use the conn handle to
                    // fetch a session hash
                    oParams.CopyProp(
                        propConnHandle, this );

                    // the session must have been
                    // existing in the reqfwdr
                    // router
                    ret = Login( nullptr,
                        oParams.GetCfg(), pResp );

                    if( ERROR( ret ) )
                        break;

                    CCfgOpener oResp(
                        ( IConfigDb* )pResp );
                    ret = oResp.GetQwordProp(
                        propSalt, qwSalt );

                    if( ERROR( ret ) )
                        break;

                }
                else // GSS_S_CONTINUE_NEEDED
                {
                    // don't know what to do,
                    // seems we are in endless
                    // wait.
                    ret = ERROR_FAIL;
                }
                break;
            }

        }while( 1 );

        if( SUCCEEDED( ret ) )
        {
            m_qwSalt = qwSalt;
            m_gssctx = gss_context;
            ret = GenSessHash(
                gss_context, m_strSess );
        }

        if( send_tok.length > 0 &&
            send_tok.value != nullptr )
        {
            gss_release_buffer(
                &min_stat, &send_tok );
        }

    }while( 0 );

    maj_stat = gss_release_cred(
        &min_stat, &pCred );

    if( ERROR( ret ) )
        return ret;

    CCfgOpenerObj oIfCfg( this );
    oIfCfg.SetBoolProp( propNoEnc, false );

    return ret;
}

gint32 CK5AuthProxy::InitPluginHook()
{
    gint32 ret = 0;

    do{
        ObjPtr pHook;
        ret = pHook.NewObj(
            clsid( CKrb5InitHook ) );
        if( ERROR( ret ) )
            break;

        CKrb5InitHook* pKrbHook = pHook;
        pKrbHook->SetIoMgr( GetIoMgr() );
        pKrbHook->m_bInRouter = true;
        pKrbHook->m_bInited = true;
        pKrbHook->m_pProxy = this;

        g_oHookMap.SetInitHook( pHook );

    }while( 0 );

    return ret;
}

gint32 wrap_message(
    gss_ctx_id_t context,
    BufPtr& pInBuf,
    BufPtr& pOutBuf )
{
    gint32 ret = 0;
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    if( context == GSS_C_NO_CONTEXT )
        return -EINVAL;

    do{
        OM_uint32 maj_stat, min_stat;
        int encrypt_flag = 1, state = 0;
        gss_buffer_desc in_buf, out_buf;
        in_buf.value = pInBuf->ptr();
        in_buf.length = pInBuf->size();

        maj_stat = gss_wrap(
            &min_stat,
            context,
            encrypt_flag,
            GSS_C_QOP_DEFAULT,
            &in_buf,
            &state,
            &out_buf);
        if( maj_stat != GSS_S_COMPLETE )
        {
            ret = ERROR_FAIL;
            break;
        }

        pOutBuf->Append(
            ( guint8* )out_buf.value,
            out_buf.length );

        gss_release_buffer(
            &min_stat, &out_buf );

    }while( 0 );

    return ret;
}

gint32 unwrap_message(
    gss_ctx_id_t context,
    BufPtr& pInBuf,
    BufPtr& pOutBuf )
{
    gint32 ret = 0;
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    if( context == GSS_C_NO_CONTEXT )
        return -EINVAL;

    do{
        OM_uint32 maj_stat, min_stat;
        int state = 0;
        gss_buffer_desc recv_buf, out_buf;
        recv_buf.value = pInBuf->ptr();
        recv_buf.length = pInBuf->size();

        maj_stat = gss_unwrap(
            &min_stat,
            context,
            &recv_buf,
            &out_buf,
            &state,
            (gss_qop_t *) NULL);

        if( maj_stat != GSS_S_COMPLETE )
        {
            ret = ERROR_FAIL;
            break;
        }

        pOutBuf->Append(
            ( guint8* )out_buf.value,
            out_buf.length );

        gss_release_buffer(
            &min_stat, &out_buf );

    }while( 0 );

    return ret;
}

gint32 get_mic(
    gss_ctx_id_t context,
    BufPtr& pInBuf,
    BufPtr& pOutBuf )
{
    gint32 ret = 0;
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    if( context == GSS_C_NO_CONTEXT )
        return -EINVAL;

    do{
        OM_uint32 maj_stat, min_stat;
        gss_buffer_desc recv_buf, out_buf;
        recv_buf.value = pInBuf->ptr();
        recv_buf.length = pInBuf->size();

        maj_stat = gss_get_mic(
            &min_stat,
            context,
            GSS_C_QOP_DEFAULT,
            &recv_buf,
            &out_buf );

        if( maj_stat != GSS_S_COMPLETE )
        {
            ret = ERROR_FAIL;
            break;
        }

        pOutBuf->Append(
            ( guint8* )out_buf.value,
            out_buf.length );

        gss_release_buffer(
            &min_stat, &out_buf );

    }while( 0 );

    return ret;
}

gint32 verify_message(
    gss_ctx_id_t context,
    BufPtr& pInBuf,
    BufPtr& pMicBuf )
{
    if( pInBuf.IsEmpty() || pMicBuf.IsEmpty() )
        return -EINVAL;

    if( context == GSS_C_NO_CONTEXT )
        return -EINVAL;

    gint32 ret = 0;
    do{
        OM_uint32 maj_stat, min_stat;
        gss_buffer_desc recv_buf, out_buf;
        gss_qop_t qop_state;

        recv_buf.value = pInBuf->ptr();
        recv_buf.length = pInBuf->size();
        out_buf.value = pMicBuf->ptr();
        out_buf.length = pMicBuf->size();

        maj_stat = gss_verify_mic(
            &min_stat,
            context,
            &recv_buf,
            &out_buf,
            &qop_state );

        if( maj_stat != GSS_S_COMPLETE )
            ret = -EBADMSG;

    }while( 0 );

    return ret;
}

gint32 CK5AuthProxy::WrapMessage(
    BufPtr& pInBuf, BufPtr& pOutBuf )
{
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        ret = wrap_message(
            GetGssCtx(), pInBuf, pOutBuf );

    }while( 0 );

    return ret;
}
        
gint32 CK5AuthProxy::UnwrapMessage(
    BufPtr& pInBuf, BufPtr& pOutBuf )
{
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        ret = unwrap_message(
            GetGssCtx(), pInBuf, pOutBuf );
    }while( 0 );

    return ret;
}

gint32 CK5AuthProxy::GetMicMsg(
    BufPtr& pInBuf, BufPtr& pOutBuf )
{
    if( pInBuf.IsEmpty() || pOutBuf.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        ret = get_mic(
            GetGssCtx(), pInBuf, pOutBuf );
    }while( 0 );

    return ret;
}


gint32 CK5AuthProxy::VerifyMicMsg(
    BufPtr& pInBuf,/* [ in ] */
    BufPtr& pInMic )/* [ in ] */
{
    if( pInBuf.IsEmpty() || pInMic.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oGssLock( GetGssLock() );
        ret = verify_message(
            GetGssCtx(), pInBuf, pInMic );

    }while( 0 );

    return ret;
}


gint32 CK5AuthProxy::NoTokenLogin()
{
    gint32 ret = 0;
    do{
        // no token to send
        CParamList oParams;
        oParams.Push( false ); 

        // only kdc traffics are allowed.
        oParams.Push( true ); 

        CfgPtr pResp( true );

        // this just tell the peer only kdc
        // traffic between this connection
        ret = Login( nullptr,
            oParams.GetCfg(), pResp );

        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( ( IConfigDb* )pResp );
        ret = oResp.GetQwordProp(
            propSalt, m_qwSalt );
        if( ERROR( ret ) )
            break;

        ret = GenSessHash(
            GSS_C_NO_CONTEXT, m_strSess );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oIfCfg( this );
        oIfCfg.SetBoolProp( propNoEnc, true );

    }while( 0 );

    return ret;
}

gint32 CK5AuthProxy::StartLogin(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    bool bHooked = false;
    do{
        std::string strMech = GET_MECH( this );
        if( strMech != "krb5" )
        {
            ret = -ENOTSUP;
            break;
        }

        ret = InitPluginHook();
        if( ERROR( ret ) )
            break;

        bHooked = true;
        // user principal
        std::string strUserName;
        std::string strSvcName;

        CCfgOpener oAuth(
            ( IConfigDb* )GET_AUTH( this ) );

        ret = oAuth.GetStrProp(
            propUserName, strUserName );

        if( ERROR( ret ) )
            strUserName.clear();

        ret = oAuth.GetStrProp(
            propServiceName, strSvcName );
        if( ERROR( ret ) )
            break;

        if( strUserName != USERNAME_NOLOGIN )
        {
            ret = Krb5Login(
                strUserName, strSvcName );
        }
        else
        {
            ret = NoTokenLogin();
        }

    }while( 0 );

    if( bHooked )
        g_oHookMap.RemoveHook();

    return ret;
}

gint32 CK5AuthProxy::DoLogin(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        TaskletPtr pDeferTask;

        CfgPtr pResp;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pDeferTask, ObjPtr( this ),
            &CK5AuthProxy::StartLogin,
            nullptr );

        if( ERROR( ret ) )
            break;

        CIfRetryTask* pRetryTask = pDeferTask;
        if( pRetryTask == nullptr )
        {
            ( *pDeferTask )( eventCancelTask );
            ret = -EFAULT;
            break;
        }

        pRetryTask->SetClientNotify( pCallback );

        // run this task on a standalone thread
        CIoManager* pMgr = GetIoMgr();
        ret = pMgr->RescheduleTask(
            pDeferTask, true );

        if( ERROR( ret ) )
        {
            ( *pDeferTask )( eventCancelTask );
            break;
        }

        ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CK5AuthProxy::RebuildMatches()
{
    // this interface does not I/O normally
    CCfgOpenerObj oIfCfg( this );
    if( m_vecMatches.empty() )
        return -ENOENT;

    gint32 ret = super::RebuildMatches();
    if( ERROR( ret ) )
        return ret;

    MatchPtr pMatch;
    for( auto& elem : m_vecMatches )
    {
        CCfgOpenerObj oMatch(
            ( CObjBase* )elem );

        std::string strIfName;

        ret = oMatch.GetStrProp(
            propIfName, strIfName );

        if( ERROR( ret ) )
            continue;

        if( DBUS_IF_NAME( "IAuthenticate" )
            == strIfName )
        {
            pMatch = elem;
        }
    }

    if( !pMatch.IsEmpty() )
    {
        // overwrite the destination info
        oIfCfg.CopyProp(
            propDestDBusName, pMatch );

        oIfCfg.CopyProp(
            propObjPath, pMatch );
    }

    // set the interface id for this match
    CCfgOpenerObj oMatchCfg(
        ( CObjBase* )m_pIfMatch );

    oMatchCfg.SetIntProp(
        propIid, GetClsid() );

    oMatchCfg.SetBoolProp(
        propDummyMatch, true );

    // also append the match for master
    // interface
    m_vecMatches.push_back( m_pIfMatch );

    return 0;
}

gint32 CK5AuthProxy::DeleteSecCtx()
{
    gss_ctx_id_t gss_ctx = GetGssCtx();
    if( gss_ctx == GSS_C_NO_CONTEXT )
        return 0;
    
    CRpcServices* pParent = GetParent();
    if( pParent == nullptr )
        return -EFAULT;

    CCfgOpenerObj oIfCfg( pParent );

    std::string strHash;
    gint32 ret = oIfCfg.GetStrProp(
        propSessHash, strHash );
    if( ERROR( ret ) )
        return ret;

    oIfCfg.RemoveProperty( propSessHash );

    CStdRMutex oGssLock( GetGssLock() );
    OM_uint32 minor, major;
    major = gss_delete_sec_context(
        &minor, &gss_ctx, GSS_C_NO_BUFFER );

    if( major )
        ret = ERROR_FAIL;

    return ret;
}

gint32 gss_sess_hash_partial(
    gss_ctx_id_t gssctx, BufPtr& pBuf )
{
    if( gssctx == GSS_C_NO_CONTEXT )
        return 0;

    gint32 ret = 0;
    if( pBuf.IsEmpty() )
    {
        ret = pBuf.NewObj();
        if( ERROR( ret ) )
            return ret;
    }

    OM_uint32       maj_stat, min_stat;
    gss_name_t      src_name, targ_name;
    gss_buffer_desc sname = {0}, tname = {0};
    OM_uint32       lifetime;
    gss_OID         mechanism, name_type;   
    OM_uint32       context_flags;
    int             is_local;
    int             is_open;

    do{
        maj_stat = gss_inquire_context(&min_stat, gssctx,
            &src_name, &targ_name, &lifetime,
            &mechanism, &context_flags,
            &is_local, &is_open);

        if (maj_stat != GSS_S_COMPLETE) {
            ret = ERROR_FAIL;
            break;
        }

        maj_stat = gss_display_name(
            &min_stat, src_name, &sname, &name_type);

        if (maj_stat != GSS_S_COMPLETE) {
            ret = ERROR_FAIL;
            break;
        }

        maj_stat = gss_display_name(&min_stat,
            targ_name, &tname,
            (gss_OID *) NULL);

        if (maj_stat != GSS_S_COMPLETE) {
            ret = ERROR_FAIL;
            break;
        }

        ret = pBuf->Append(
            ( guint8* )sname.value, sname.length );

        if( ERROR( ret ) )
            break;

        ret = pBuf->Append(
            ( guint8* )tname.value, tname.length );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    (void) gss_release_name(&min_stat, &src_name);
    (void) gss_release_name(&min_stat, &targ_name);
    (void) gss_release_buffer(&min_stat, &sname);
    (void) gss_release_buffer(&min_stat, &tname);

    return ret;
}

gint32 CK5AuthProxy::GenSessHash(
    gss_ctx_id_t gssctx,
    std::string& strSess )
{
    gint32 ret = 0;

    do{
        BufPtr pBuf( true );
        ret = gss_sess_hash_partial(
            gssctx, pBuf );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oIfCfg( this );
        IConfigDb* pConn;
        ret = oIfCfg.GetPointer(
            propConnParams, pConn );
        if( ERROR( ret ) )
            break;

        ret = AppendConnParams( pConn, pBuf );
        if( ERROR( ret ) )
            break;

        if( m_qwSalt == 0 )
        {
            ret = ERROR_STATE;
            break;
        }

        guint64 qwSalt = htonll( m_qwSalt );
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

gint32 CKdcChannelProxy::InitUserFuncs()
{
    BEGIN_IFPROXY_MAP( IAuthenticate, false );

    ADD_PROXY_METHOD_ASYNC( 1,
        CKdcChannelProxy::Login,
        SYS_METHOD( AUTH_METHOD_LOGIN ) );

    ADD_PROXY_METHOD_ASYNC( 1,
        CKdcChannelProxy::MechSpecReq,
        SYS_METHOD( AUTH_METHOD_MECHSPECREQ ) );

    END_PROXY_MAP;

    return 0;
}

gint32 CKdcChannelProxy::MechSpecReq(
    IEventSink* pCallback,
    IConfigDb* pInfo,/*[ in ]  pInfo */
    CfgPtr& pResp ) /* [ out ] pResp */
{
    if( pInfo == nullptr )
        return -EINVAL;

    EnumClsid iid = iid( IAuthenticate );
    return FORWARD_SYSIF_CALL(
        iid, 1, AUTH_METHOD_MECHSPECREQ,
        pInfo, pResp );
}

gint32 CKdcChannelProxy::Login(
    IEventSink* pCallback,
    IConfigDb* pInfo, /*[ in ]*/
    CfgPtr& pResp ) /*[ out ]*/
{ return -ENOTSUP; }

gint32 CKdcChannelProxy::RebuildMatches()
{
    // this interface does not I/O normally
    CCfgOpenerObj oIfCfg( this );
    if( m_vecMatches.empty() )
        return -ENOENT;

    gint32 ret = super::RebuildMatches();
    if( ERROR( ret ) )
        return ret;

    MatchPtr pMatch;
    for( auto& elem : m_vecMatches )
    {
        CCfgOpenerObj oMatch(
            ( CObjBase* )elem );

        std::string strIfName;

        ret = oMatch.GetStrProp(
            propIfName, strIfName );

        if( ERROR( ret ) )
            continue;

        if( DBUS_IF_NAME( "IAuthenticate" )
            == strIfName )
        {
            pMatch = elem;
        }
    }

    if( !pMatch.IsEmpty() )
    {
        // overwrite the destination info
        oIfCfg.CopyProp(
            propDestDBusName, pMatch );

        oIfCfg.CopyProp(
            propObjPath, pMatch );
    }

    // set the interface id for this match
    CCfgOpenerObj oMatchCfg(
        ( CObjBase* )m_pIfMatch );

    oMatchCfg.SetIntProp(
        propIid, GetClsid() );

    oMatchCfg.SetBoolProp(
        propDummyMatch, true );

    // also append the match for master
    // interface
    m_vecMatches.push_back( m_pIfMatch );

    return 0;
}

}
