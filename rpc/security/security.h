/*
 * =====================================================================================
 *
 *       Filename:  security.h
 *
 *    Description:  Declaration of security related classes 
 *
 *        Version:  1.0
 *        Created:  05/01/2020 02:15:54 PM
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

#pragma once

#include "configdb.h"
#include "defines.h"
#include "autoptr.h"
#include "rpcroute.h"
#include "secclsid.h"
#include <krb5.h>
#include <gssapi.h>

#define AUTH_METHOD_LOGIN            "AuthReq_Login"
#define AUTH_METHOD_MECHSPECREQ      "AuthReq_MechSpecReq"
#define SESS_METHOD_LOGOUT           "SessReq_Logout"
#define SESS_METHOD_REVOKESESS       "SessReq_RevokeSess"
#define SESS_METHOD_ISVALIDSESS      "SessReq_IsValidSess"
#define SESS_METHOD_REFRESHSESS      "SessReq_RefreshSess"
#define SESS_METHOD_GETSESSINFO      "SessReq_GetSessInfo"

#define SESS_EVENT_ONSESSEXPIRE      "RpcEvt_OnSessExpire"

#define GET_AUTH( _pIf ) \
({ \
    CCfgOpenerObj oIfCfg( _pIf ); \
    IConfigDb* pConn = nullptr; \
    IConfigDb* pAuth = nullptr;         \
    ret = oIfCfg.GetPointer( \
        propConnParams, pConn ); \
    if( SUCCEEDED( ret ) ) \
    {\
        CCfgOpener oConn( pConn ); \
        ret = oConn.GetPointer( \
            propAuthInfo, pAuth ); \
        if( ERROR( ret ) ) \
            pAuth = nullptr;\
    }\
    pAuth;\
})

#define GET_MECH( _pIf )\
({\
    CCfgOpener oAuth( \
         GET_AUTH( _pIf ) );\
    std::string strMech;\
    oAuth.GetStrProp(\
        propAuthMech, strMech );\
    strMech;\
})

struct IAuthenticate
{
    virtual gint32 Login(
        IEventSink* pCallback,
        IConfigDb* pInfo, /*[ in ]*/
        CfgPtr& pResp ) = 0; /*[ out ]*/

    virtual gint32 MechSpecReq(
        IEventSink* pCallback,
        IConfigDb* pReq,/*[ in ]*/
        IConfigDb* pResp ) = 0;/*[ out ]*/
};

struct IAuthenticateProxy : public IAuthenticate
{
    InterfPtr m_pParent;

    virtual gint32 GetSess(
        std::string& strSess ) const = 0;

    virtual gint32 WrapMessage(
        BufPtr& pInMsg,
        BufPtr& pOutMsg ) = 0;
        
    virtual gint32 UnwrapMessage(
        BufPtr& pInMsg,
        BufPtr& pOutMsg ) = 0;

    virtual gint32 GetMicMsg(
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pOutMic ) = 0;/* [ out ] */

    virtual gint32 VerifyMicMsg(
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pInMic ) = 0;/* [ in ] */

    void SetParent( CRpcServices* pParent )
    {
        if( pParent == nullptr )
            m_pParent.Clear();
        else
            m_pParent = pParent;
    }

    inline CRpcServices* GetParent()
    { return m_pParent; }

    gint32 GetMicMsg2p(
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pOutMic );

    gint32 VerifyMicMsg2p(
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pSecCtx );
};

//
// class as the session provider to integrate with
// user declared interface proxy. It is actually
// the wrapper of CSessCacheProxy, and provides
// authentication related operations, especially
// to create the proper auth mechanism as the
// m_pSessImpl.
//
class CAuthentProxy :
    public virtual CAggInterfaceProxy,
    public IAuthenticateProxy
{
    // the instance of the authentication
    // mechanism object CK5AuthProxy
    InterfPtr m_pSessImpl;

    public:
    typedef CAggInterfaceProxy super;
    CAuthentProxy( const IConfigDb* pCfg ) :
        super( pCfg )
    {;}

    // override the GetIid
    const EnumClsid GetIid() const
    { return iid( IAuthenticate ); }

    static gint32 InitEnvRouter(
        CIoManager* pMgr );

    gint32 GetSessImpl(
        ObjPtr& pSessImpl ) const
    {
        if( m_pSessImpl.IsEmpty() )
            return -EFAULT;
        pSessImpl = m_pSessImpl;
        return 0;
    }

    gint32 SetSessImpl(
        InterfPtr& pSessImpl )
    {
        m_pSessImpl = pSessImpl;
        return 0;
    }

    gint32 OnPostStop(
        IEventSink* pCallback );

    gint32 StopSessImpl();

    static gint32 CreateSessImpl(
        const IConfigDb* pConnParams,
        CRpcRouter* pRouter,
        InterfPtr& pIf );

    static gint32 BuildLoginTask(
        IEventSink* pIf,
        IEventSink* pCallback,
        TaskletPtr& pTask );

    virtual gint32 GetSess(
        std::string& strSess ) const;

    virtual gint32 WrapMessage(
        BufPtr& pInMsg,
        BufPtr& pOutMsg );
        
    virtual gint32 UnwrapMessage(
        BufPtr& pInMsg,
        BufPtr& pOutMsg );

    virtual gint32 GetMicMsg(
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pOutMic );/* [ out ] */

    virtual gint32 VerifyMicMsg(
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pInMic );/* [ in ] */

    virtual gint32 Login(
        IEventSink* pCallback,
        IConfigDb* pInfo, /*[ in ]*/
        CfgPtr& pResp );

    virtual gint32 MechSpecReq(
        IEventSink* pCallback,
        IConfigDb* pReq,/*[ in ]*/
        IConfigDb* pResp );/*[ out ]*/
};

struct IAuthenticateServer : public IAuthenticate
{
    virtual gint32 WrapMessage(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pOutMsg ) = 0;
        
    virtual gint32 UnwrapMessage(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pOutMsg ) = 0;

    virtual gint32 GetMicMsg(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pOutMic ) = 0;
        
    virtual gint32 VerifyMicMsg(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pInMic ) = 0;

    gint32 GetMicMsg2s(
        const std::string& strSess,
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pSecBuf );

    gint32 VerifyMicMsg2s(
        const std::string& strSess,
        BufPtr& pInMsg,/* [ in ] */
        BufPtr& pSecBuf );

    virtual gint32 RemoveSession(
        const std::string& strSess ) = 0;

    virtual gint32 GetSess(
        guint32 dwPortId,
        std::string& strSess ) = 0;

    virtual gint32 IsNoEnc(
        const std::string& strSess ) = 0;

};

class CAuthentServer:
    public virtual CAggInterfaceServer,
    public IAuthenticateServer
{
    ObjPtr m_pAuthImpl;

    gint32 StartAuthImpl(
            IEventSink* pCallback );

    gint32 OnStartAuthImplComplete(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 OnPreStopComplete(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    public:
    typedef CAggInterfaceServer super;
    CAuthentServer( const IConfigDb* pCfg ) :
        super( pCfg )
    {}

    const EnumClsid GetIid() const
    { return iid( IAuthenticate ); }

    // request handlers
    gint32 InitUserFuncs();

    gint32 GetSessionByPortId(
        gint32 dwPortId,
        std::string& strSess );

    inline gint32 GetAuthImpl(
        ObjPtr& pAuthImpl ) const
    {
        if( m_pAuthImpl.IsEmpty() )
            return -EFAULT;
        pAuthImpl = m_pAuthImpl;
        return 0;
    }

    virtual gint32 Login(
        IEventSink* pCallback,
        IConfigDb* pInfo, /*[ in ]*/
        CfgPtr& pResp );

    virtual gint32 MechSpecReq(
        IEventSink* pCallback,
        IConfigDb* pReq,/*[ in ]*/
        IConfigDb* pResp );/*[ out ]*/

    virtual gint32 OnPostStart(
        IEventSink* pCallback );

    virtual gint32 OnPreStop(
        IEventSink* pCallback );

    gint32 RemoveSession(
        const std::string& strSess );

    virtual gint32 GetSess(
        guint32 dwPortId,
        std::string& strSess );

    gint32 IsNoEnc(
        const std::string& strSess );

    virtual gint32 WrapMessage(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pOutMsg );
        
    virtual gint32 UnwrapMessage(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pOutMsg );

    virtual gint32 GetMicMsg(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pOutMic );
        
    virtual gint32 VerifyMicMsg(
        const std::string& strSess,
        BufPtr& pInMsg,
        BufPtr& pInMic );
};

/**
* @name CRpcReqForwarderProxyAuth, the special
* reqfwdr-proxy for authentication only
* @{ */
/**  @} */

class CRpcReqForwarderProxyAuth :
    public CRpcReqForwarderProxy
{
    protected:
    gint32 ForwardLogin(
        IConfigDb* pReqCtx,
        DBusMessage* pMsgRaw,
        DMsgPtr& pRespMsg,
        IEventSink* pCallback );

    public:
    typedef CRpcReqForwarderProxy super;
    CRpcReqForwarderProxyAuth( const IConfigDb* pCfg ) :
       CAggInterfaceProxy( pCfg ), super( pCfg )
    {;}

    gint32 ForwardRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pMsgRaw,
        DMsgPtr& pRespMsg,
        IEventSink* pCallback );
};

class CRpcTcpBridgeAuth :
    public CRpcTcpBridge
{
    CCfgOpener m_oSecCtx;
    TaskletPtr m_pLoginTimer;

    gint32 OnLoginFailed(
        IEventSink* pCallback );

    gint32 OnLoginTimeout(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 StartLoginTimer(
        IEventSink* pCallback,
        guint32 dwTimeoutSec );

    gint32 OnEnableComplete(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 EnableInterfaces();

    gint32 IsAuthRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pMsg );

    gint32 OnLoginComplete(
        IEventSink* pCallback,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    public:
    typedef CRpcTcpBridge super;
    CRpcTcpBridgeAuth( const IConfigDb* pCfg ) :
        CAggInterfaceServer( pCfg ), super( pCfg )
    {}

    virtual gint32 OnPostStart(
        IEventSink* pCallback );

    virtual gint32 OnPostStop(
        IEventSink* pCallback );

    gint32 SetSessHash(
        const std::string& strHash,
        bool bEncrypt );

    // add a session hash to the pFwdrMsg
    virtual gint32 ForwardRequest(
        IConfigDb* pReqCtx,
        DBusMessage* pFwdrMsg,
        DMsgPtr& pRespMsg,
        IEventSink* pCallback );

    virtual gint32 FetchData_Server(
        IConfigDb* pDataDesc,           // [in]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [in, out]
        guint32& dwSize,                // [in, out]
        IEventSink* pCallback );

    gint32 DoInvoke(
        DBusMessage* pReqMsg,
        IEventSink* pCallback );

    gint32 DoInvoke(
        IConfigDb* pEvtMsg,
        IEventSink* pCallback );

    bool IsKdcChannel();
};

class CRpcReqForwarderAuth :
    public CRpcReqForwarder
{
    TaskGrpPtr m_pLoginQue;

    gint32 LocalLoginInternal(
        IEventSink* pCallback,
        IConfigDb* pCfg,
        IEventSink* pInvTask );

    gint32 OnSessImplStarted(
        IEventSink* pInvTask,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    gint32 OnSessImplLoginComplete(
        IEventSink* pInvTask,
        IEventSink* pIoReq,
        IConfigDb* pReqCtx );

    public:
    typedef CRpcReqForwarder super;
    CRpcReqForwarderAuth( const IConfigDb* pCfg ) :
        CAggInterfaceServer( pCfg ), super( pCfg )
    {
        CIoManager* pMgr = GetIoMgr();
        CAuthentProxy::InitEnvRouter( pMgr );
    }

    const EnumClsid GetIid() const
    { return iid( CRpcReqForwarderAuth ); }

    bool SupportIid( EnumClsid iid ) const
    {
        if( iid == iid( CRpcReqForwarder ) )
            return true;
        return false;
    }

    gint32 InitUserFuncs();

    gint32 OpenRemotePort(
        IEventSink* pCallback,
        const IConfigDb* pcfg );

    gint32 BuildStartAuthProxyTask(
        IEventSink* pInvTask,
        IConfigDb* pCfg,
        TaskletPtr& pTask );

    gint32 CreateBridgeProxyAuth(
        IEventSink* pCallback,
        IConfigDb* pCfg,
        IEventSink* pInvTask );

    inline guint32 AddLoginSeqTask(
        TaskletPtr& pTask, bool bLong )
    {
        return AddSeqTaskInternal(
            m_pLoginQue, pTask, bLong );
    }

    gint32 LocalLogin(
        IEventSink* pCallback,
        const IConfigDb* pcfg );

    virtual gint32 ClearRefCountByPortId(
        guint32 dwPortId,
        std::vector< std::string >& vecUniqNames );
};

struct CRpcRouterAuthShared
{
    MatchPtr m_pAuthMatch;
    CRpcServices* m_pOwner = 0;
    CRpcRouterAuthShared(
        CRpcServices* pOwner )
    {
        m_pOwner = pOwner;
        InitAuthMatch( m_pAuthMatch );
    }

    gint32 InitAuthMatch( MatchPtr& pMatch )
    {
        gint32 ret = 0;
        do{
            std::string strIfName =
                DBUS_IF_NAME( "IAuthenticate" );

            CIoManager* pMgr =
                m_pOwner->GetIoMgr();

            std::string strRouter;
            pMgr->GetRouterName( strRouter );
            std::string strDest =
                DBUS_DESTINATION2( strRouter,
                    OBJNAME_ROUTER_BRIDGE_AUTH  );

            std::string strObjPath =
                DBUS_OBJ_PATH( strRouter,
                    OBJNAME_ROUTER_BRIDGE_AUTH  );

            CCfgOpener oParams;
            oParams.SetStrProp(
                propIfName, strIfName );

            oParams.SetStrProp(
                propObjPath, strObjPath );

            oParams.SetIntProp(
                propMatchType, matchServer );

            oParams.SetStrProp(
                propRouterPath, "/" );

            oParams.SetIntProp(
                propPortId, 0xffffffff );

            ret = pMatch.NewObj(
                clsid( CRouterRemoteMatch ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            CCfgOpenerObj oMatch(
                ( CObjBase* ) pMatch );

            oMatch.SetStrProp(
                propDestDBusName, strDest );

        }while( 0 );

        return ret;
    }

    gint32 IsAuthMsg(
        IConfigDb* pTransCtx,
        DMsgPtr& pMsg ) const
    {
        gint32 ret = 0;
        do{
            if( pTransCtx == nullptr ||
                pMsg.IsEmpty() )
            {
                ret = -EINVAL;
                break;
            }

            std::string strPath;
            CCfgOpener oTransCtx( pTransCtx );
            ret = oTransCtx.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
                break;

            if( strPath != "/" )
            {
                ret = ERROR_FALSE;
                break;
            }

            ret = m_pAuthMatch->
                IsMyMsgIncoming( pMsg );

        }while( 0 );

        return ret;
    }
};

class CRpcRouterReqFwdrAuth :
    public CRpcRouterReqFwdr,
    public CRpcRouterAuthShared
{
    public:
    typedef CRpcRouterReqFwdr super;
    CRpcRouterReqFwdrAuth( const IConfigDb* pCfg )
        : CAggInterfaceServer( pCfg ), super( pCfg ),
        CRpcRouterAuthShared( this )
    {}

    virtual gint32 CheckReqToFwrd(
        IConfigDb* pTransCtx,
        DMsgPtr& pMsg,
        MatchPtr& pMatchHit )
    {
        gint32 ret = super::CheckReqToFwrd(
            pTransCtx, pMsg, pMatchHit );
        if( SUCCEEDED( ret ) )
            return ret;

        ret = IsAuthMsg( pTransCtx, pMsg );
        if( SUCCEEDED( ret ) )
            pMatchHit = m_pAuthMatch;

        return ret;
    }

    virtual bool HasAuth() const
    { return true; }


    virtual gint32 IsEqualConn(
        const IConfigDb* pConn1,
        const IConfigDb* pConn2 );

    virtual gint32 GetBridgeProxy(
        const IConfigDb* pConnParams,
        InterfPtr& pIf );

    virtual gint32 DecRefCount(
        guint32 dwPortId,
        const std::string& strSrcUniqName,
        const std::string& strSrcDBusName );

    using CRpcRouter::GetBridgeProxy;
};

class CRpcRouterBridgeAuth :
    public CRpcRouterBridge,
    public CRpcRouterAuthShared
{
    public:
    typedef CRpcRouterBridge super;
    CRpcRouterBridgeAuth( const IConfigDb* pCfg )
        : CAggInterfaceServer( pCfg ), super( pCfg ),
        CRpcRouterAuthShared( this )
    {}

    virtual gint32 CheckReqToRelay(
        IConfigDb* pReqCtx,
        DMsgPtr& pMsg,
        MatchPtr& pMatchHit )
    {
        gint32 ret = super::CheckReqToRelay(
            pReqCtx, pMsg, pMatchHit );

        if( SUCCEEDED( ret ) )
            return ret;

        ret = IsAuthMsg( pReqCtx, pMsg );
        if( SUCCEEDED( ret ) )
            pMatchHit = m_pAuthMatch;

        return ret;
    }

    virtual bool HasAuth() const
    { return true; }

    virtual gint32 OnPostStart(
        IEventSink* pCallback );
};

class CRpcTcpBridgeProxyAuth :
    public CRpcTcpBridgeProxy
{
    CCfgOpener m_oSecCtx;

    public:
    typedef CRpcTcpBridgeProxy super;
    CRpcTcpBridgeProxyAuth(
        const IConfigDb* pCfg ) :
        CAggInterfaceProxy( pCfg ),
        super( pCfg )
    {;}

    gint32 SetSessHash(
        const std::string& strHash,
        bool bEncrypt );

    gint32 OnPostStop(
        IEventSink* pCallback );

    bool IsKdcChannel();
};

DECLARE_AGGREGATED_PROXY(
    CRpcReqForwarderProxyAuthImpl,
    CRpcReqForwarderProxyAuth,
    CStatCountersProxy );

DECLARE_AGGREGATED_PROXY(
    CRpcTcpBridgeProxyAuthImpl,
    CRpcTcpBridgeProxyAuth,
    CAuthentProxy,
    CStatCountersProxy,
    CStreamProxyRelay );

DECLARE_AGGREGATED_SERVER(
    CRpcReqForwarderAuthImpl,
    CRpcReqForwarderAuth,
    CStatCountersServer ); 

DECLARE_AGGREGATED_SERVER(
    CRpcTcpBridgeAuthImpl,
    CRpcTcpBridgeAuth,
    CStatCountersServer,
    CStreamServerRelay,
    CStreamServerRelayMH ); 

DECLARE_AGGREGATED_SERVER(
    CRpcRouterReqFwdrAuthImpl,
    CRpcRouterReqFwdrAuth,
    CStatCountersServer ); 

// authent server implementation
DECLARE_AGGREGATED_SERVER(
    CRpcRouterBridgeAuthImpl,
    CRpcRouterBridgeAuth,
    CAuthentServer,
    CStatCountersServer ); 