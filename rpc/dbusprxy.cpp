/*
 * =====================================================================================
 *
 *       Filename:  dbusprxy.cpp
 *
 *    Description:  implementation of CDBusProxyPdo
 *
 *        Version:  1.0
 *        Created:  11/10/2016 08:33:53 PM
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

#include <dbus/dbus.h>
#include <vector>
#include <string>
#include <map>
#include "defines.h"
#include "namespc.h"
#include "port.h"
#include "prxyport.h"
#include "emaphelp.h"
#include "ifhelper.h"

namespace rpcf
{

#define PROXYPDO_CONN_RETRIES       ( ( guint32 )12 )
#define PROXYPDO_CONN_INTERVAL      ( ( guint32 )30 )

using namespace std;

#define IS_AUTH_PROXY( _pPort ) \
({ \
    bool bOpenPort; \
    CCfgOpenerObj _oPortCfg( _pPort ); \
    gint32 iRet = _oPortCfg.GetBoolProp( \
        propOpenPort, bOpenPort ); \
    if( ERROR( iRet ) ) \
        bOpenPort = false; \
    m_bAuth && bOpenPort; \
})

CConnParamsProxy GetConnParams(
    const CObjBase* pObj )
{
    CCfgOpenerObj oCfg( pObj );
    IConfigDb* pConnParams = nullptr;

    oCfg.GetPointer(
        propConnParams, pConnParams );

    return CConnParamsProxy( pConnParams );
}

std::string CDBusProxyPdo::GetReqFwdrName() const
{
    std::string strVal;
    if( m_bAuth )
    {
        strVal = OBJNAME_REQFWDR_AUTH;
    }
    else
    {
        strVal = OBJNAME_REQFWDR;
    }

    return strVal;
}

CDBusProxyPdo::CDBusProxyPdo(
    const IConfigDb* pCfg ) : super( pCfg ),
        m_atmInitDone( false )
{
    SetClassId( clsid( CDBusProxyPdo ) );
    gint32 ret = 0;

    do{
        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        ObjPtr pObj;

        CCfgOpener oCfg( pCfg );

        guint32* pdwValue;

        ret = oCfg.GetIntPtr(
            propDBusConn, pdwValue );

        if( ERROR( ret ) )
            break;

        m_atmReqSent = 0;
        m_pDBusConn = ( DBusConnection* )pdwValue;
        if( m_pDBusConn == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        // we did not add ref to the reference in 
        // the config block, so we explictly add one
        dbus_connection_ref( m_pDBusConn );

        CConnParamsProxy oConnParams =
            GetConnParams( m_pCfgDb );

        if( oConnParams.IsEmpty() )
        {
            ret = -EINVAL;
            break;
        }

        m_bAuth = oConnParams.HasAuth();
        SetConnected( false );

        CCfgOpener oMyCfg( ( IConfigDb* )m_pCfgDb );
        if( !m_pCfgDb->exist( propSingleIrp ) )
        {
            // the property indicates one event irp will
            // be completed on one incoming signal message
            // without this flag, the default behavor is
            // to complete all the waiting irps which is
            // waiting on that interface
            oMyCfg.SetBoolProp( propSingleIrp, true );
        }

        std::string strRouterPath;
        ret = oMyCfg.GetStrProp(
            propRouterPath, strRouterPath );

        if( ERROR( ret ) )
            break;

        oMyCfg.CopyProp(
            propSrcDBusName, m_pBusPort );

        oMyCfg.CopyProp(
            propSrcUniqName, m_pBusPort );

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret, 
            "fatal error, error in CDBusProxyPdo ctor " );
        throw std::runtime_error( strMsg );
    }
    return;
}

CDBusProxyPdo::~CDBusProxyPdo()
{
    if( m_pDBusConn )
    {
        dbus_connection_unref( m_pDBusConn );
        m_pDBusConn = nullptr;
    }
}

gint32 CDBusProxyPdo::CheckConnCmdResp(
    DBusMessage* pMessage, gint32& iMethodReturn )
{
    gint32 ret = 0;

    if( pMessage == nullptr )
        return -EINVAL;

    DMsgPtr pMsg( pMessage );

    do{
        string strMember = pMsg.GetMember();
        if( strMember.empty() )
        {
            ret = -EINVAL;
            break;
        }

        if( strMember != SYS_METHOD_OPENRMTPORT && 
            strMember != SYS_METHOD_CLOSERMTPORT &&
            strMember != SYS_METHOD_LOCALLOGIN )
        {
            ret = -EINVAL;
            break;
        }

        bool bClose = false;
        if( strMember == SYS_METHOD_CLOSERMTPORT )
            bClose = true;

        ret = pMsg.GetIntArgAt( 0,
            ( guint32& )iMethodReturn );

        if( ERROR( ret ) )
            break;

        if( ERROR( iMethodReturn ) )
            break;

        if( bClose )
            break;

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 1, pObj );
        if( ERROR( ret ) )
            break;

        IConfigDb* pResp = pObj;
        if( pResp == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        CCfgOpener oResp( pResp );
        ret = oResp.GetIntProp(
            propReturnValue,
            ( guint32& )iMethodReturn );

        if( ERROR( ret ) )
            break;

        if( ERROR( iMethodReturn ) )
            break;

        CCfgOpenerObj oPortCfg( this );        
        ret = oPortCfg.CopyProp(
            propConnHandle, pResp );
        if( ERROR( ret ) )
            break;

        // update the existing propConnParams
        ret = oPortCfg.CopyProp(
            propConnParams, pResp );

        if( ERROR( ret ) )
            break;

        // overwrite the local router path with
        // the fresh router path
        ret = oPortCfg.CopyProp(
            propRouterPath, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oMatchCfg(
            ( CObjBase* )m_pMatchFwder );

        oMatchCfg.CopyProp(
            propRouterPath, this );

        oMatchCfg.CopyProp(
            propConnHandle, this );

        // add the only match for this proxy pdo,
        // the listening IRP will be queued from
        // upper ProxyFdo port
        ret = AddMatch( m_mapEvtTable,
            m_pMatchFwder );

        if( ERROR( ret ) )
            break;

        SetInitDone();

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::CompleteConnReq(
    IRP* pIrp )
{

    if( pIrp == nullptr 
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    IrpCtxPtr& pCtx = pIrp->GetCurCtx();

    do{
        // clear what we have saved in the m_pReqData

        DMsgPtr& pMsg = *pCtx->m_pRespData;

        if( pMsg.IsEmpty() )
        {
            ret = -EINVAL;
            break;
        }

        do{

            gint32 iMethodReturn = 0;
            ret = CheckConnCmdResp( pMsg, iMethodReturn );

            if( ERROR( ret ) )
                break;

            if( SUCCEEDED( iMethodReturn ) )
            {
                CStdRMutex oPortLock( GetLock() );
                if( pIrp->CtrlCode() == CTRLCODE_CONNECT )
                {
                    SetConnected( true );
                    // we don't call OnPortReady here
                    // because this is not the right
                    // place to do so
                }
                else
                {
                    SetConnected( false );
                }
            }
            else if( iMethodReturn == ERROR_TIMEOUT )
            {
                // let the upper software to retry
                // NOTE: it is an interface definition
                ret = -EAGAIN;
            }
            else
            {
                ret = iMethodReturn;
            }

        }while( 0 );

        // set the return status
        pCtx->SetStatus( ret );

        if( pIrp->CtrlCode() == CTRLCODE_DISCONN )
        {
            // active disconnection is done
            break;
        }

        // the reqData is set by this port
        // so we can remove it
        pCtx->m_pReqData.Clear();

        break;

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::CompleteRmtRegMatch(
    IRP* pIrp )

{
    gint32 ret = 0;

    do{
        if( pIrp == nullptr ||
            pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        ret = pCtx->GetStatus();

        if( ERROR( ret ) )
            break;

        DMsgPtr& pRespMsg =
            *pCtx->m_pRespData;

        string strMember =
            pRespMsg.GetMember();

        if( strMember.empty() )
        {
            ret = -EINVAL;
            break;
        }

        if( strMember == SYS_METHOD_ENABLERMTEVT ||
            strMember == SYS_METHOD_DISABLERMTEVT )
        {
            CDBusError dbusError;
            gint32 iMethodReturn = 0;

            if( !dbus_message_get_args(
                pRespMsg, dbusError,
                DBUS_TYPE_UINT32, &iMethodReturn,
                DBUS_TYPE_INVALID ) )
            {
                ret = dbusError.Errno();
                break;
            }
            ret = iMethodReturn;
            pCtx->SetStatus( ret );
        }
        else
        {
            pCtx->m_pRespData.Clear();
            ret = -ENOTSUP;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::CompleteFwrdReq(
    IRP* pIrp )
{
    // the data has been prepared in the
    // DispatchRespMsg, we don't need further
    // actions
    gint32 ret = 0;

    do{
        if( pIrp == nullptr 
           || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        ret = pCtx->GetStatus();

        if( ERROR( ret ) )
            break;

        DMsgPtr& pRespMsg =
            *pCtx->m_pRespData;

        string strMember = pRespMsg.GetMember();

        if( strMember.empty() )
        {
            ret = -EINVAL;
            break;

        }

        if( strMember == SYS_METHOD_FORWARDREQ )
        {
            ret = UnpackFwrdRespMsg( pIrp );
        }
        else
        {
            pCtx->m_pRespData.Clear();
            ret = -ENOTSUP;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::CompleteListening(
    IRP* pIrp )
{
    // the data has been prepared in the
    // DispatchRespMsg, we don't need further
    // actions
    gint32 ret = 0;

    do{
        if( pIrp == nullptr 
           || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        ret = pCtx->GetStatus();

        if( ERROR( ret ) )
            break;

        DMsgPtr& pRespMsg =
            *pCtx->m_pRespData;

        string strMember =
            pRespMsg.GetMember() ;

        if( strMember.empty() )
        {
            ret = -EINVAL;
            break;
        }

        if( strMember == SYS_EVENT_FORWARDEVT )
        {
            ret = UnpackFwrdEventMsg( pIrp );
        }
        else if( strMember == SYS_EVENT_RMTSVREVENT )
        {
            break;
        }
        else
        {
            pCtx->m_pRespData.Clear();
            pCtx->SetStatus( ret );
            ret = -ENOTSUP;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::CompleteIoctlIrp(
    IRP* pIrp )
{

    gint32 ret = 0;

    do{
        if( pIrp == nullptr 
           || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        if( pIrp->MajorCmd() != IRP_MJ_FUNC 
            && pIrp->MinorCmd() != IRP_MN_IOCTL )
        {
            ret = -EINVAL;
            break;
        }
        
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        ret = pCtx->GetStatus();
        if( ERROR( ret ) )
            break;

        switch( pIrp->CtrlCode() )
        {
        case CTRLCODE_CONNECT:
        case CTRLCODE_DISCONN:
            {
                ret = CompleteConnReq( pIrp );
                break;
            }
        case CTRLCODE_FORWARD_REQ:
            {
                ret = CompleteFwrdReq( pIrp );
                break;
            }
        case CTRLCODE_LISTENING:
            {
                ret = CompleteListening( pIrp );
                break;
            }

        case CTRLCODE_RMT_REG_MATCH:
            {
                ret = CompleteRmtRegMatch( pIrp );
                break;
            }

        default:
            ret = super::CompleteIoctlIrp( pIrp );
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::HandleConnRequest(
    IRP* pIrp, bool bConnect )
{

    // send a request to the router to make the
    // physical connection to remote server if
    // necessary, and wait for the response
    CParamList oParams;
    CCfgOpenerObj oPortCfg( this );

    gint32 ret = 0;

    do{
        if( pIrp == nullptr ||
            pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        if( m_pBusPort == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack();

        string strBusName; 
        ret = oPortCfg.GetStrProp(
            propSrcUniqName, strBusName );

        if( ERROR( ret ) )
            break;

        CParamList oMethodArgs;
        oMethodArgs.SetStrProp(
            propSrcUniqName, strBusName );

        // the uniq name of the dbus connection
        oMethodArgs.CopyProp(
            propSrcDBusName, this );

        ret = oMethodArgs.CopyProp(
            propRouterPath, this );
        if( ERROR( ret ) )
            break;

        if( bConnect )
        {
            ret = oMethodArgs.CopyProp(
                propConnParams, this );
        }
        else
        {
            ret = oMethodArgs.CopyProp(
                propConnHandle, this );
        }

        if( ERROR( ret ) )
            break;

        oParams.Push(
            ObjPtr( oMethodArgs.GetCfg() ) );

        string strIf = DBUS_IF_NAME(
            IFNAME_REQFORWARDER );

        string strCmd;

        if( bConnect )
        {
            strCmd = SYS_METHOD_OPENRMTPORT;
        }
        else
            strCmd = SYS_METHOD_CLOSERMTPORT;

        if( m_bAuth && !IS_AUTH_PROXY( this ) )
        {
            strCmd = SYS_METHOD_LOCALLOGIN;
            // the method is from an interface
            // other than IFNAME_REQFORWARDER
            strIf = DBUS_IF_NAME(
                IFNAME_REQFORWARDERAUTH );
        }
        
        oParams.SetStrProp( propMethodName, strCmd );

        BufPtr pBuf( true );
        ret = oParams.GetCfg()->Serialize( *pBuf );
        if( ERROR( ret ) )
            break;

        DMsgPtr pMsg;

        // a method call message
        ret = pMsg.NewObj();

       if( ERROR( ret ) )
            break;

        ret = pMsg.SetInterface( strIf );
        if( ERROR( ret ) )
            break;

        string strRtName;
        GetIoMgr()->GetRouterName( strRtName );
        string strDest = DBUS_DESTINATION2(
                strRtName, GetReqFwdrName() );

        ret = pMsg.SetDestination( strDest );

        if( ERROR( ret ) )
            break;

        ret = pMsg.SetMember( strCmd );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetSender( strBusName );

        if( ERROR( ret ) )
            break;

        string strObjPath = DBUS_OBJ_PATH(
            strRtName, GetReqFwdrName() );

        ret = pMsg.SetPath( strObjPath );

        if( ERROR( ret ) )
            break;

        const char* pData = pBuf->ptr();
        if( !dbus_message_append_args( pMsg,
                DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                &pData, pBuf->size(),
                DBUS_TYPE_INVALID ) )
        {
            ret = ERROR_FAIL;
            break;
        }

        *pBuf = pMsg;
        ret = pIrpCtx->SetReqData( pBuf );
        if( ERROR( ret ) )
            break;

        ret = HandleSendReq( pIrp );
        if( ERROR( ret ) )
            break;

        if( ret != STATUS_PENDING )
        {
            ret = ERROR_FAIL;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::BuildMsgHeader(
    DMsgPtr& pMsg ) const
{
    gint32 ret = 0;
    do{
        string strIfName;
        string strObjPath;

        CCfgOpenerObj oPortCfg( this );
        string strSender; 
        ret = oPortCfg.GetStrProp(
            propSrcDBusName, strSender );

        if( ERROR( ret ) )
            break;

        string strRtName;
        GetIoMgr()->GetRouterName( strRtName );
        string strDest = DBUS_DESTINATION2(
            strRtName, GetReqFwdrName() );

        if( true )
        {
            CCfgOpenerObj oCfg(
                ( IMessageMatch* )m_pMatchFwder );

            ret = oCfg.GetStrProp(
                propIfName, strIfName );

            if( ERROR( ret ) )
                break;

            ret = oCfg.GetStrProp(
                propObjPath, strObjPath );

            if( ERROR( ret ) )
                break;
        }


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

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::PackupReqMsg(
    DMsgPtr& pReqMsg,
    DMsgPtr& pOutMsg,
    bool bNoReply ) const
{
    gint32 ret = 0;
    do{
        ret = BuildMsgHeader( pOutMsg );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oCfg( this );

        CCfgOpener oReqCtx;

        ret = oReqCtx.CopyProp(
            propConnHandle, this );
        if( ERROR( ret ) )
            break;

        ret = oReqCtx.CopyProp(
            propRouterPath, this );

        if( ERROR( ret ) )
            break;

        if( bNoReply )
            oReqCtx[ propNoReply ] = true;

        ret = pOutMsg.SetMember(
            SYS_METHOD_FORWARDREQ );

        if( ERROR( ret ) )
            break;

        BufPtr pBuf( true );
        ret = pReqMsg.Serialize( *pBuf );

        if( ERROR( ret ) )
            break;

        BufPtr pCtxBuf( true );
        ret = oReqCtx.GetCfg()->
            Serialize( *pCtxBuf );
        if( ERROR( ret ) )
            break;

        const char* pCtx = pCtxBuf->ptr();
        const char* pData = pBuf->ptr();
        if( !dbus_message_append_args( pOutMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pBuf->size(),
            DBUS_TYPE_INVALID ) )
        {
            ret = ERROR_FAIL;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::HandleFwrdReq( IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    do{
        // wrapping the incoming message in
        // a forward request message to the
        // CRpcReqForwarder
        DMsgPtr pMsg;
        ret = pMsg.NewObj();
        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        DMsgPtr& pUsrReqMsg = *pCtx->m_pReqData;
        if( pUsrReqMsg.IsEmpty() )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwIoDir = pCtx->GetIoDirection();
        ret = PackupReqMsg( pUsrReqMsg, pMsg,
            ( dwIoDir == IRP_DIR_OUT ) );

        if( ERROR( ret ) )
            break;

        *pCtx->m_pReqData = pMsg;
        ret = HandleSendReq( pIrp );
        if( ret >= 0 )
            m_atmReqSent++;

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::SubmitIoctlCmd( IRP* pIrp )
{
    gint32 ret = 0;

    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
    {
        return -EINVAL;
    }

    // let's process the func irps
    IrpCtxPtr pCtx = pIrp->GetCurCtx();

    do{
        if( pIrp->MajorCmd() != IRP_MJ_FUNC ||
            pIrp->MinorCmd() != IRP_MN_IOCTL )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwCtrlCode = pIrp->CtrlCode();

        switch( dwCtrlCode )
        {
        case CTRLCODE_CONNECT:
        case CTRLCODE_DISCONN:
            {
                // the CONNECT command will be
                // sent when the port enters ready
                // state
                bool bConnect = true;

                if( dwCtrlCode == CTRLCODE_DISCONN )
                    bConnect = false;

                ret = HandleConnRequest(
                    pIrp, bConnect );

                break;
            }
        case CTRLCODE_LISTENING:
            {
                if( !IsConnected() )
                {
                    ret = -ENOTCONN;
                    // Reconnect();
                    break;
                }
                // what we can do is to listen to the
                // CRpcReqForwarder's events from the
                // rpc router with the ip address as is
                // assigned to this proxy pdo. So we
                // don't need external match object as
                // event filter
                if( !pCtx->m_pReqData.IsEmpty() )
                {
                    ret = -EINVAL;
                    break;
                }
                ObjPtr pObj;
                pObj = m_pMatchFwder;

                BufPtr pBuf( true );
                *pBuf = pObj;
                pCtx->m_pReqData = pBuf;
                ret = HandleListening( pIrp );

                break;
            }
        case CTRLCODE_FORWARD_REQ:
            {
                if( !IsConnected() )
                {
                    ret = -ENOTCONN;
                    break;
                }
                ret = HandleFwrdReq( pIrp );
                break;
            }
        case CTRLCODE_RMT_REG_MATCH:
        case CTRLCODE_RMT_UNREG_MATCH:
            {
                bool bReg;

                if( !IsConnected() )
                {
                    ret = -ENOTCONN;
                    break;
                }
                if( dwCtrlCode == CTRLCODE_RMT_UNREG_MATCH )
                    bReg = false;
                else
                    bReg = true;

                ret = HandleRmtRegMatch( pIrp, bReg );
                break;
            }
        case CTRLCODE_REG_MATCH:
        case CTRLCODE_UNREG_MATCH:
            {
                ret = -ENOTSUP;
                break; 
            }
        case CTRLCODE_SEND_DATA:
        case CTRLCODE_FETCH_DATA:
        case CTRLCODE_SEND_REQ:
            {
                if( !IsConnected() )
                {
                    ret = -ENOTCONN;
                    break;
                }
                ret = super::HandleSendReq( pIrp );
                break;
            }
        case CTRLCODE_SEND_EVENT:
            {
                if( !IsConnected() )
                {
                    ret = -ENOTCONN;
                    break;
                }
                ret = super::HandleSendEvent( pIrp );
                break;
            }
        default:
            {
                ret = super::SubmitIoctlCmd( pIrp ); 
                break;
            }
        }
    }while( 0 );

    if( ret != STATUS_PENDING )
        pCtx->SetStatus( ret );

    return ret;
}

gint32 CProxyPdoConnectTask::CompleteReconnect(
    gint32 iRet )
{
    if( ERROR( iRet ) )
        return iRet;

    gint32 ret = 0;
    do{

        CParamList oParams(
            ( IConfigDb*)GetConfig() );

        // build a svr online message
        DMsgPtr pMsg;
        ret = pMsg.NewObj(
            ( EnumClsid )DBUS_MESSAGE_TYPE_SIGNAL );

        if( ERROR( ret ) )
            break;

        CDBusProxyPdo* pPdo = nullptr;
        ret = oParams.GetPointer(
            propPortPtr, pPdo );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oPortCfg( pPdo );
        std::string strDest;
        ret = oPortCfg.GetStrProp(
            propSrcUniqName, strDest );

        guint32 dwPortId = 0;
        ret = oPortCfg.GetIntProp(
            propPortId, dwPortId );

        pMsg.SetInterface( DBUS_IF_NAME(
            IFNAME_REQFORWARDER ) );

        pMsg.SetMember(
            SYS_EVENT_RMTSVREVENT );

        pMsg.SetDestination( strDest );
        pMsg.SetSerial( 131 );
        pMsg.SetSender( strDest );

        guint32 dwOnline = true;
        if( ERROR( iRet ) )
            dwOnline = false;

        CCfgOpener oEvtCtx;
        oEvtCtx[ propConnHandle ] = dwPortId;
        oEvtCtx.CopyProp( propRouterPath, pPdo );

        BufPtr pCtxBuf( true );
        ret = oEvtCtx.GetCfg()->
            Serialize( *pCtxBuf );

        if( ERROR( ret ) )
            break;

        const char* pCtx = pCtxBuf->ptr();
        if( !dbus_message_append_args( pMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pCtx, pCtxBuf->size(),
            DBUS_TYPE_BOOLEAN, &dwOnline,
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

        ret = pPdo->OnRmtSvrOnOffline( pMsg );

    }while( 0 );

    return ret;
}

gint32 CProxyPdoConnectTask::CompleteMasterIrp(
    IRP* pMasterIrp, gint32 iRetConn )
{
    gint32 ret                  = 0;
    CIoManager* pMgr            = nullptr;
    bool bComplete              = false;
    CDBusProxyPdo* pProxyPort   = nullptr;

    ObjPtr pObj;
    CCfgOpener oParams( ( const IConfigDb* )m_pCtx );

    do{
        ret = oParams.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        if( pMasterIrp == nullptr )
        {
            // unexpected condition, stop further
            // processing

           ret = -EINVAL; 
           break;
        }

        ret = oParams.GetObjPtr( propPortPtr, pObj );

        if( SUCCEEDED( ret ) )
        {
            pProxyPort = pObj;
        }

        // let's check if master irp need to be completed
        CStdRMutex oLock( pMasterIrp->GetLock() );
        ret = pMasterIrp->CanContinue( IRP_STATE_READY );

        if( ERROR( ret ) )
        {
            // master irp is busy in other states
            // cannot complete, and we need to stop
            break;
        }

        // at this point, it is now ok to complete
        // master irp 
        bComplete = true;
        IrpCtxPtr& pCtx = pMasterIrp->GetTopStack();

        // set status before master irp is completed
        pCtx->SetStatus( iRetConn );

        if( SUCCEEDED( iRetConn ) )
        {
            // broadcast the event that this port
            // is successfully started
            // we call super::OnPortReady here bacause
            // the context is similiar to where it
            // should be called, and with the irp
            // locked
            //
            // the port should be in the proper state
            // for this task, otherwise pMasterIrp will
            // not be accessable here
            //
            if( pProxyPort != nullptr )
            {
                ret = pProxyPort->
                    super::OnPortReady( pMasterIrp );
            }
        }

    }while( 0 );

    if( ret == STATUS_PENDING ||
        ret == STATUS_MORE_PROCESS_NEEDED )
        return ret;

    if( !bComplete )
        return ret;

    if( pMgr != nullptr && pMasterIrp != nullptr )
    {
        ret = pMgr->CompleteIrp( pMasterIrp );
        // finally add a match to listen to the
        // events from routers CRpcReqForwarder
    }
    return ret;
}

gint32 CProxyPdoConnectTask::ExtendIrpTimer(
    IRP* pMasterIrp )
{

    gint32 ret                  = 0;
    CIoManager* pMgr            = nullptr;

    ObjPtr pObj;
    CCfgOpener oParams( ( IConfigDb* )m_pCtx );
    do{
        SetError( 0 );

        ret = oParams.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        if( pMasterIrp == nullptr
            || pMasterIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        // extend the timer for the connection request
        CStdRMutex oLock( pMasterIrp->GetLock() );

        ret = pMasterIrp->CanContinue( IRP_STATE_READY );

        if( ERROR( ret ) )
            break;

        pMasterIrp->ResetTimer();

    }while( 0 );

    return ret;
}

/**
* @name CProxyPdoConnectTask::Process
* @{ send down the connect irp to the pdo */
/** this method is running in a tasklet context
 * and it will be retried 3 times when the request
 * fails. 
 *
 * ALERT: the detail is a little bit complicated.
 * NOTE: we don't need a cancel routine for the
 * connection irp because this tasklet will always
 * be executed, either completed or canceled
 *
 * parameters:
 *
 *  propRetries: the times left to retry, required
 *      by the CTaskletRetriable
 *
 *  propIntervalSec: the interval between retries
 *      required by the CTaskletRetriable
 *
 *  propIoMgr: the pointer to the IoManager
 *
 *  propIrpPtr: the pointer to the irp from the
 *      OnPortReady if any. If it does not exist,
 *      this task is scheduled from the client
 *      interface instead.
 *
 *  propPortPtr: the pointer to the CDBusProxyPdo
 *
 * This method is the major handler of the
 * retriable task This task could be called from a
 * timer callback, an irp completion, or the task
 * scheduler. You can use dwContext to decide the
 * caller.
 * @} */

gint32 CProxyPdoConnectTask::Process(
    guint32 dwContext )
{
    gint32 ret                  = 0;
    IRP* pMasterIrp             = nullptr;
    CIoManager* pMgr            = nullptr;
    CDBusProxyPdo* pProxyPort   = nullptr; 

    ObjPtr pObj;

    CStdRTMutex oTaskLock( GetLock() );

    CParamList oParams(
        ( IConfigDb* )GetConfig() );

    ret = oParams.GetPointer( propIoMgr, pMgr );
    if( ERROR( ret ) )
    {
        // nothing we can do without IoManager
        return SetError( ret );
    }

    ret = oParams.GetObjPtr( propIrpPtr, pObj );
    if( SUCCEEDED( ret ) )
        pMasterIrp = pObj;

    bool bExpired = false;
    if( pMasterIrp != nullptr )
    {
        ret = ExtendIrpTimer( pMasterIrp );
        // the master irp could canceled at this
        // point, anyway, let's move on
        if( ret == -EPERM )
            bExpired = true;
    }

    bool bRetry = CanRetry();

    do{
        if( dwContext == eventIrpComp )
        {
            // we are called from irp completion.
            // Extract the irp and let's inspect
            // the return value of the irp
            IRP* pIrp = nullptr;
            ret = oParams.GetPointer( 0, pIrp );
            if( ERROR( ret ) )
                break;

            ret = pIrp->GetStatus();

            // The request irp succeeded. The port
            // should be in good shape.
            if( SUCCEEDED( ret ) )
                break;

            // the port indicates to try again.
            if( ret == -EAGAIN ||
                ret == -ENOTCONN ||
                ret == -ETIMEDOUT )
            {
                if( bRetry )
                {
                    DebugPrint( ret,
                        "Server is not up, retry in %d seconds",
                        PROXYPDO_CONN_INTERVAL );
                    oParams.ClearParams();
                    ret = STATUS_MORE_PROCESS_NEEDED;
                }
                else
                {
                    DebugPrint( ret,
                        "Retried many times, Give up now",
                        PROXYPDO_CONN_INTERVAL );
                }
            }

            break;
        }
        else if( dwContext == eventCancelTask )
        {
            IRP* pIrp = nullptr;
            ret = oParams.GetPointer( 0, pIrp );
            if( ERROR( ret ) )
                break;

            CStdRMutex oIrpLock( pIrp->GetLock() );
            ret = pIrp->CanContinue(
                IRP_STATE_READY );
            if( ERROR( ret ) )
                break;

            pIrp->RemoveCallback();
            pIrp->RemoveTimer();
            oIrpLock.Unlock();

            ret = pMgr->CancelIrp(
                pIrp, true, -ECANCELED );

            break;
        }
        else if( dwContext == eventTimeout )
        {
            // do nothing
        }

        ret = oParams.GetPointer(
            propPortPtr, pProxyPort );

        if( SUCCEEDED( ret ) )
        {
            CStdRMutex oPortLock(
                pProxyPort->GetLock() );

            if( pProxyPort->IsConnected() )
                break;
        }
        else
        {
            break;
        }

        if( !bRetry )
        {
            ret = -ETIMEDOUT;
            break;
        }

        if( bExpired )
        {
            ret = -ETIMEDOUT;
            break;
        }

        DecRetries();

        IrpPtr pIrp( true );
        pIrp->AllocNextStack( nullptr );
        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 

        // make the physical connection to remote
        // machine
        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
        pIrpCtx->SetCtrlCode( CTRLCODE_CONNECT );

        pIrpCtx->SetIoDirection( IRP_DIR_INOUT ); 
        pIrp->SetSyncCall( false );

        // set a timer
        pIrp->SetTimer( PORT_START_TIMEOUT_SEC, 0 );
        pIrp->SetCallback( this, eventZero );
        pIrp->SetIrpThread( pMgr );

        // NOTE: we don't use association here because
        // it could cause to lock both the master irp
        // and the slave irp in the completion, which
        // is not necessarily problemic, but it is
        // possible. Current implementation avoids this
        //
        // using SubmitIrpInternal to bypass the upper
        // ports over this port if any which remain in
        // PORT_STATE_STARTING waiting for masterirp to
        // complete and cannot let this request pass

        // keep a copy for canceling purpose
        oParams.Push( ObjPtr( pIrp ) );
        ret = pMgr->SubmitIrpInternal(
            pProxyPort, pIrp, false );

        if( ret == STATUS_PENDING )
        {
            // we will rely on the irp callback to
            // retry if error occurs in the completion
            break;
        }
            
        // remove the timer
        pIrp->RemoveTimer();
        pIrp->RemoveCallback();

    }while( 0 );

    if( ret == STATUS_PENDING ||
        ret == STATUS_MORE_PROCESS_NEEDED )
        return ret;

    if( pMasterIrp != nullptr )
    {
        CompleteMasterIrp( pMasterIrp, ret );
    }
    else
    {
        CompleteReconnect( ret );
    }

    gint32 iRet = oParams.GetPointer(
        propPortPtr, pProxyPort );

    if( SUCCEEDED( iRet ) )
    {
        pProxyPort->ClearConnTask();
    }

    RemoveProperty( propIrpPtr );
    RemoveProperty( propPortPtr );
    oParams.ClearParams();

    return SetError( ret );
}

gint32 CDBusProxyPdo::PostStart( IRP* pIrp )
{
    gint32 ret = 0;

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    do{
        CCfgOpener matchCfg;

        string strRtName;
        GetIoMgr()->GetRouterName( strRtName );

        string strPath = DBUS_OBJ_PATH(
            strRtName, GetReqFwdrName() );

        string strIfName =
            DBUS_IF_NAME( IFNAME_REQFORWARDER );

        // any match going to the event map or req
        // map must have propDestDBusName set to
        // handle the module online/offline
        string strDest = DBUS_DESTINATION2(
                strRtName, GetReqFwdrName() );

        ret = matchCfg.SetStrProp(
            propDestDBusName, strDest );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.SetStrProp(
            propObjPath, strPath );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.SetStrProp(
            propIfName, strIfName );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.SetIntProp(
            propMatchType, matchClient );

        if( ERROR( ret ) )
            break;

        ret = m_pMatchFwder.NewObj(
            clsid( CProxyMsgMatch ),
            matchCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = m_pMatchDBus.NewObj(
            clsid( CDBusSysMatch ) );

        if( ERROR( ret ) )
            break;

        ret = super::PostStart( pIrp );

    }while( 0 );


    return ret;
}

/**
* @name OnPortReady
* @{ the event when the port turns from starting
* to ready state
* */
/* param: pIrp[ in ], the pointer to the request
 * Packet of the PNP_START cmd.
 *
 * Return 0 on success, and errno if failed. 
 *
 * It will return STATUS_PENDING if the operation
 * cannot complete within this method. Usually
 * waiting for some IO event or long time task to
 * complete.
 * @} */

gint32 CDBusProxyPdo::OnPortReady( IRP* pIrp )
{
    gint32 ret = 0;

    do{
        if( pIrp == nullptr
            || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        CIoManager* pMgr = GetIoMgr();
        CParamList oParams;

        // retry parameters
        if( IS_AUTH_PROXY( this ) )
        {
            oParams.SetIntProp( propRetries, 2 );
        }
        else
        {
            oParams.SetIntProp( propRetries,
                PROXYPDO_CONN_RETRIES );
        }

        oParams.SetIntProp(
            propIntervalSec, PROXYPDO_CONN_INTERVAL );
        
        // context parameters
        oParams.SetPointer( propIrpPtr, pIrp );
        oParams.SetPointer( propPortPtr, this );
        oParams.SetPointer( propIoMgr, pMgr );

        ret = m_pConnTask.NewObj(
            clsid( CProxyPdoConnectTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) ) 
            break;

        // to avoid the complexity, we schedule a
        // task for reconnection, instead of
        // immediately exec the task
        ret = GetIoMgr()->RescheduleTask(
            m_pConnTask );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

void CDBusProxyPdo::OnPortStopped()
{
    RemoveMatch( m_mapEvtTable, m_pMatchFwder );
    m_pMatchFwder.Clear();
    super::OnPortStopped();
}

gint32 CDBusProxyPdo::UnpackFwrdRespMsg( IRP* pIrp )
{
    // unpackage the message to retrieve the
    // response message from the remote client
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    IrpCtxPtr& pCtx = pIrp->GetTopStack();

    do{
        DMsgPtr pMsg = *pCtx->m_pRespData;
        gint32 iMethodReturn = 0;

        ret = pMsg.GetIntArgAt( 0,
            ( guint32& )iMethodReturn );

        if( ERROR( ret ) )
        {
            ret = -EBADMSG;
            break;
        }

        if( ERROR( iMethodReturn ) )
        {
            ret = iMethodReturn;
            break;
        }

        DMsgPtr pUserResp;
        ret = pMsg.GetMsgArgAt( 1, pUserResp );
        if( ERROR( ret ) )
            break;

        // replace the carrier dbus message with
        // the payload dbus message
        BufPtr pNewBuf( true );
        *pNewBuf = pUserResp;
        pCtx->SetRespData( pNewBuf );

    }while( 0 );

    pCtx->SetStatus( ret );

    return ret;
}

gint32 CDBusProxyPdo::UnpackFwrdEventMsg(
    IRP* pIrp )
{
    // unpackage the message to retrieve the
    // response message from the remote client
    gint32 ret = 0;

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    IrpCtxPtr& pCtx = pIrp->GetTopStack();

    CDBusError dbusError;

    do{

        DMsgPtr pMsg = *pCtx->m_pRespData;

        vector< DMsgPtr::ARG_ENTRY > vecArgs;
        ret = pMsg.GetArgs( vecArgs );
        if( ERROR( ret ) )
            break;

        if( vecArgs.size() < 2 )
        {
            ret = -EBADMSG;
            break;
        }

        CfgPtr pEvtCtx;
        BufPtr pCtxBuf = vecArgs[ 0 ].second;
        ret = pEvtCtx.Deserialize( *pCtxBuf );
        if( ERROR( ret ) )
            break;

        CCfgOpener oEvtCtx(
            ( IConfigDb* )pEvtCtx );

        gint32 ret1 = oEvtCtx.IsEqualProp(
            propRouterPath, this );

        ret = oEvtCtx.IsEqualProp(
            propConnHandle, this );

        if( ERROR( ret ) || ERROR( ret1 ) )
        {
            pCtx->m_pRespData.Clear();
            ret = super::HandleListening( pIrp );
            if( SUCCEEDED( ret ) )
                continue;
            break;
        }

        DMsgPtr pUserEvent;
        BufPtr& pMsgBuf = vecArgs[ 1 ].second;
        ret = pUserEvent.Deserialize( pMsgBuf );
        if( ERROR( ret ) )
            break;

        gint32 iType = pUserEvent.GetType();
        if( iType != DBUS_MESSAGE_TYPE_SIGNAL )
        {
            ret = -EINVAL;
            break;
        }

        BufPtr pNewBuf( true );
        *pNewBuf = pUserEvent;
        pCtx->SetRespData( pNewBuf );

        break;

    }while( 1 );

    if( ret != STATUS_PENDING )
        pCtx->SetStatus( ret );

    return ret;
}

void CDBusProxyPdo::SetConnected(
    bool bConnected )
{
    m_bConnected = bConnected;
    if( !m_pMatchFwder.IsEmpty() )
    {
        if( m_mapEvtTable.find( m_pMatchFwder ) ==
            m_mapEvtTable.end() )
            return;
        MATCH_ENTRY& oMe =
            m_mapEvtTable[ m_pMatchFwder ];
        oMe.SetConnected( bConnected );
    }
}

gint32 CDBusProxyPdo::OnRmtSvrOnOffline(
    DBusMessage* pMsg )
{
    if( pMsg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        DMsgPtr ptrMsg( pMsg );
        bool bOnline = false;

        ObjPtr pFwrdCtx;
        ret = ptrMsg.GetObjArgAt( 0, pFwrdCtx );
        if( ERROR( ret ) )
            break;

        ret = ptrMsg.GetBoolArgAt( 1, bOnline );
        if( ERROR( ret ) )
            break;

        // block the io message or resume the IO
        // requests
        CStdRMutex oPortLock( GetLock() );
        SetConnected( bOnline );

        // notify the upper port
        IrpPtr pIrp;
        if( !bOnline )
        {
            // any node on the router path down
            // will cause this pdo offline
            MatchMap* pMatchMap = nullptr;
            ret = GetMatchMap(
                m_pMatchFwder, pMatchMap );
            if( ERROR( ret ) )
                break;

            if( !pMatchMap->empty() )
            {
                MatchMap::iterator itr =
                    pMatchMap->find( m_pMatchFwder );

                if( itr != pMatchMap->end() )
                {
                    deque< IrpPtr >& irpQue =
                        itr->second.m_queWaitingIrps;
                    if( !irpQue.empty() )
                    {
                        pIrp = irpQue.front();
                        irpQue.pop_front();
                    }
                }
            }
        }

        oPortLock.Unlock();

        if( !pIrp.IsEmpty() )
        {
            CStdRMutex oIrpLock( pIrp->GetLock() );
            ret = pIrp->CanContinue(
                IRP_STATE_READY );

            if( SUCCEEDED( ret ) &&
                pIrp->GetStackSize() > 0 )
            {
                IrpCtxPtr& pCtx =
                    pIrp->GetTopStack(); 

                BufPtr pBuf( true );
                *pBuf = ptrMsg;
                pCtx->SetRespData( pBuf );
                pCtx->SetStatus( -ENOTCONN );
                oIrpLock.Unlock();
                GetIoMgr()->CompleteIrp( pIrp );
            }
            else
            {
                //NOTE: what can I do?
            }
        }

        // clear all the pending irps
        if( !bOnline )
            ret = CancelAllIrps( -ENOTCONN );

        EnumEventId eventId = eventRmtSvrOffline;
        if( IS_AUTH_PROXY( this ) )
        {
            // if we are within the router, don't
            // send out the eventRmtSvrOffline
            // again.
            break;
        }

        if( bOnline )
            eventId = eventRmtSvrOnline;

        CCfgOpener oEvtCtx;
        ret = oEvtCtx.CopyProp(
            propConnHandle, this );
        if( ERROR( ret ) )
            break;

        ret = oEvtCtx.CopyProp(
            propRouterPath, pFwrdCtx );

        if( ERROR( ret ) )
            break;

        IConfigDb* pEvtCtx = oEvtCtx.GetCfg();

        // send this event to the pnp manager
        CEventMapHelper< CPort >
            oEvtHelper( this );

        oEvtHelper.BroadcastEvent(
            eventConnPoint, eventId,
            ( LONGWORD )pEvtCtx,
            ( LONGWORD* )PortToHandle( this ) );

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdo::Reconnect()
{
    gint32 ret = 0;

    do{
        // reconnect
        CStdRMutex oPortLock( GetLock() );
        if( !m_pConnTask.IsEmpty() )
        {
            // a connect task is already
            // running
            break;
        }
        if( m_bStopReady )
            break;

        guint32 dwPortState = GetPortState();
        if( dwPortState != PORT_STATE_READY &&
            dwPortState != PORT_STATE_BUSY_SHARED )
            break;

        CParamList oParams;
        CIoManager* pMgr = GetIoMgr();

        if( IS_AUTH_PROXY( this ) )
            oParams[ propRetries ] = 2;
        else
        {
            oParams[ propRetries ] =
                PROXYPDO_CONN_RETRIES;
        }

        oParams[ propIntervalSec ] =
            PROXYPDO_CONN_INTERVAL;
        
        oParams[ propPortPtr ] =
            ObjPtr( this );

        oParams[ propIoMgr ] =
            ObjPtr( pMgr );

        ret = m_pConnTask.NewObj(
            clsid( CProxyPdoConnectTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) ) 
            break;

        // to avoid the complexity, we
        // schedule a task for
        // reconnection, instead of
        // immediately exec the task
        pMgr->RescheduleTask( m_pConnTask );

    }while( 0 );
    
    return ret;
}

DBusHandlerResult CDBusProxyPdo::PreDispatchMsg(
    gint32 iType, DBusMessage* pMessage )
{
    DBusHandlerResult ret =
        DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if( pMessage == nullptr )
        return ret;

    if( iType != DBUS_MESSAGE_TYPE_SIGNAL )
        return ret;

    if( !IsInitDone() )
        return ret;

    DMsgPtr pMsg( pMessage );

    do{
        gint32 ret1 =
            m_pMatchDBus->IsMyMsgIncoming( pMsg );

        if( SUCCEEDED( ret1 ) )
        {
            ret = DispatchDBusSysMsg( pMsg );
            if( ret == DBUS_HANDLER_RESULT_HANDLED )
                break;
        }

        ret1 =
            m_pMatchFwder->IsMyMsgIncoming( pMsg );

        if( SUCCEEDED( ret1 ) )
        {
            string strMember = pMsg.GetMember();

            if( strMember != SYS_EVENT_RMTSVREVENT )
                break;

            OnRmtSvrOnOffline( pMsg );

            // don't continue on the rest pdo
            // ports from the parent bus
            ret = DBUS_HANDLER_RESULT_HALT;
            break;
        }
        else if( ret1 == ERROR_FALSE )
        {
            // don't continue on this port
            ret = DBUS_HANDLER_RESULT_HANDLED;
            break;
        }

    }while( 0 );

    if( ret == DBUS_HANDLER_RESULT_NOT_YET_HANDLED )
    {
        ret = super::PreDispatchMsg(
            iType, pMessage );
    }

    return ret;
}

gint32 CDBusProxyPdo::HandleListening(
    IRP* pIrp )
{

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 
        super::HandleListening( pIrp );

    if( SUCCEEDED( ret ) )
    {
        // we need to unpack the message before
        // returning irp to the upper port
        ret = CompleteListening( pIrp );
    }

    return ret;
}

gint32 CDBusProxyPdo::HandleRmtRegMatch(
    IRP* pIrp, bool bReg )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpenerObj oCfg( this );
        
        // let's process the func irps
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        IMessageMatch* pMatch =
            *( ObjPtr* )( *pCtx->m_pReqData );

        MatchPtr pMatchToSend;

        ret = pMatchToSend.NewObj(
            pMatch->GetClsid() );

        if( ERROR( ret ) )
            break;

        *pMatchToSend = *pMatch;
        CCfgOpenerObj oMatch(
            ( IMessageMatch* )pMatchToSend );

        // routing info
        ret = oMatch.CopyProp(
            propRouterPath, this );

        if( ERROR( ret ) )
            break;

        oMatch.CopyProp( propPrxyPortId,
            propConnHandle, this );

        // source info
        oMatch.CopyProp(
            propSrcDBusName, this );

        oMatch.CopyProp(
            propSrcUniqName, this );

        CParamList oParams;
        oParams.Push( ObjPtr( pMatchToSend ) );

        DMsgPtr pMsg;
        ret = pMsg.NewObj();
        if( ERROR( ret ) )
            break;

        ret = BuildMsgHeader( pMsg );
        string strMember;
        if( bReg )
            strMember = SYS_METHOD_ENABLERMTEVT;
        else
            strMember = SYS_METHOD_DISABLERMTEVT;

        pMsg.SetMember( strMember );

        oParams[ propMethodName ] = strMember;
        oParams.CopyProp(
            propSrcUniqName, pMatchToSend );

        // the uniq name of the dbus connection
        oParams.CopyProp(
            propSrcDBusName, this );

        CCfgOpener oCallOptions;
        guint32 dwCallFlags = ( CF_WITH_REPLY |
            DBUS_MESSAGE_TYPE_METHOD_CALL );

        oCallOptions[ propCallFlags ] =
            dwCallFlags;

        oParams.SetObjPtr( propCallOptions,
            ObjPtr( oCallOptions.GetCfg() ) );

        BufPtr pBuf( true );
        ret = oParams.GetCfg()->
            Serialize( *pBuf );

        if( ERROR( ret ) )
            break;

        const char* pData = pBuf->ptr();
        if( !dbus_message_append_args( pMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pBuf->size(),
            DBUS_TYPE_INVALID ) ) 
        {
            ret = ERROR_FAIL;
            break;
        }

        *pCtx->m_pReqData = pMsg;
        ret = HandleSendReq( pIrp );

    }while( 0 );

    return ret;
}

gint32 CProxyPdoDisconnectTask::operator()(
    guint32 dwContext )
{
    gint32 ret = 0;
    CCfgOpener oParams( ( IConfigDb* )m_pCtx );
    CIoManager* pMgr = nullptr;

    ret = oParams.GetPointer( propIoMgr, pMgr );
    if( ERROR( ret ) )
        return ret;

    IRP* pMasterIrp = nullptr;
    ret = oParams.GetPointer( propIrpPtr, pMasterIrp );
    if( ERROR( ret ) )
        return ret;

    do{

        CPort* pPort = nullptr;
        ret = oParams.GetPointer( propPortPtr, pPort );
        if( ERROR( ret ) )
            break;

        if( dwContext == eventIrpComp )
        {
            // we are called from an IRP callback
            // Let's mark this port disconnected
            // either remote end stopped or not

            CStdRMutex oPortLock( pPort->GetLock() );
            CDBusProxyPdo* pProxyPdo =
                static_cast< CDBusProxyPdo* >( pPort );

            pProxyPdo->SetConnected( false );
            ret = 0;
            break;
        }

        IrpPtr pIrp( true );
        pIrp->AllocNextStack( nullptr );
        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 

        // make the physical connection to remote
        // machine
        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
        pIrpCtx->SetCtrlCode( CTRLCODE_DISCONN );
        pIrpCtx->SetIoDirection( IRP_DIR_INOUT ); 
        pIrp->SetSyncCall( false );
        pIrp->SetCallback( this, 0 );

        // set a timer
        pIrp->SetTimer( PORT_START_TIMEOUT_SEC / 4, pMgr );

        // send this irp to the pdo only
        ret = pMgr->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ret == STATUS_PENDING )
        {
            // the master irp will be completed
            // later through associated irp
            break;
        }
        else
        {
            // immediate return
            CStdRMutex oPortLock( pPort->GetLock() );
            CDBusProxyPdo* pProxyPdo =
                static_cast< CDBusProxyPdo* >( pPort );

            pProxyPdo->SetConnected( false );
        }

        // remove the timer
        pIrp->RemoveTimer();
        pIrp->RemoveCallback();

        
    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        CStdRMutex oIrpLock( pMasterIrp->GetLock() );

        gint32 ret2 = pMasterIrp->
            CanContinue( IRP_STATE_READY );

        if( ERROR( ret2 ) )
        {
            // irp is not available
            return ERROR_STATE;
        }

        IrpCtxPtr& pTopCtx =
            pMasterIrp->GetTopStack();
        pTopCtx->SetStatus( ret );

        oIrpLock.Unlock();
        ret = pMgr->CompleteIrp( pMasterIrp );
    }

    return ret;
}

gint32 CDBusProxyPdo::OnQueryStop( IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 ) 
        return -EINVAL;

    gint32 ret = 0;

    do{
        CStdRMutex oPortLock( GetLock() );

        if( m_bStopReady )
            break;
        m_bStopReady = true;

        if( !m_pConnTask.IsEmpty() )
        {
            TaskletPtr pConnTask = m_pConnTask;
            ClearConnTask();
            oPortLock.Unlock();

            // cancel the ongoing connect task
            ( *pConnTask )( eventCancelTask );

            oPortLock.Lock();
        }

        if( !IsConnected() )
            break;

        // we need to disconnect with the local
        // forwader and remote bridge if we have an
        // active connection
        CCfgOpener oCfg;
        ret = oCfg.SetPointer(
            propIoMgr, GetIoMgr() );
        if( ERROR( ret ) )
            break;

        ret = oCfg.SetPointer(
            propIrpPtr, pIrp );
        if( ERROR( ret ) )
            break;

        ret = oCfg.SetPointer(
            propPortPtr, this );

        if( ERROR( ret ) )
            break;

        ret = GetIoMgr()->ScheduleTask(
            clsid( CProxyPdoDisconnectTask ),
            oCfg.GetCfg() );

        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 ); 
    
    return ret;
}

gint32 CDBusProxyPdo::NotifyRouterOffline()
{
    gint32 ret = 0;
    do{
        // send this event to the pnp manager
        CCfgOpenerObj oCfg( this );

        CCfgOpener oEvtCtx;

        oEvtCtx.CopyProp( propConnHandle, this );
        oEvtCtx.SetStrProp( propRouterPath, "/" );
        IConfigDb* pEvtCtx = oEvtCtx.GetCfg();
        CEventMapHelper< CPort > oEvtHelper( this );
        oEvtHelper.BroadcastEvent(
            eventConnPoint,
            eventRmtSvrOffline,
            ( LONGWORD )pEvtCtx,
            ( LONGWORD* )PortToHandle( this ) );

    }while( 0 );

    return ret;
}

// local dbus message handler
gint32 CDBusProxyPdo::OnModOnOffline(
    DBusMessage* pDBusMsg )
{
    if( pDBusMsg == nullptr )
        return -EINVAL;

    gint32 ret = ERROR_NOT_HANDLED;

    do{
        bool bOnline = false;
        char* pszModName = nullptr;
        CDBusError oError;
        DMsgPtr pMsg( pDBusMsg );

        if( pMsg.GetMember() == "NameOwnerChanged" )
        {
            // remote module online/offline
            char* pszOldOwner = nullptr;
            char* pszNewOwner = nullptr; 

            if( !dbus_message_get_args(
                pDBusMsg, oError,
                DBUS_TYPE_STRING, &pszModName,
                DBUS_TYPE_STRING, &pszOldOwner,
                DBUS_TYPE_STRING, &pszNewOwner,
                DBUS_TYPE_INVALID ) )
            {
                ret = oError.Errno();
                break;
            }

            if( pszNewOwner[ 0 ] != 0 )
            {
                // the name has been assigned to
                // some process
                bOnline = true;
            }

            // do nothing since it should never happen
            if( bOnline )
                break;

            string strModName = pszModName;
            if( m_pMatchFwder->IsMyDest( strModName ) ||
                m_pMatchFwder->IsMyObjPath( strModName ) )
            {
                DEFER_CALL( GetIoMgr(), this,
                    &CDBusProxyPdo::NotifyRouterOffline );
            }
        }
        else
        {
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusProxyPdoLpbk::GetSender(
    std::string& strSender ) const
{
    strSender = LOOPBACK_DESTINATION;
    strSender += ".Proxy_";

    CCfgOpenerObj oCfg( this );
    guint32 dwPortId = 0;
    gint32 ret = oCfg.GetIntProp(
        propPortId, dwPortId );

    if( ERROR( ret ) )
        return ret;

    strSender += std::to_string(
        dwPortId );

    return 0;
}

gint32 CDBusProxyPdoLpbk::SendDBusMsg(
    DBusMessage* pMsg,
    guint32* pdwSerial )
{
    if( pMsg == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    if( m_pBusPort->GetClsid()
        == clsid( CDBusBusPort ) )
    {
        CDBusBusPort* pBus = static_cast
            < CDBusBusPort* >( m_pBusPort );

        DMsgPtr pMsgPtr( pMsg );
        std::string strSender;
        ret = GetSender( strSender );
        if( ERROR( ret ) )
            return ret;

        pMsgPtr.SetSender( strSender );
        ret = pBus->SendLpbkMsg( pMsg,
            pdwSerial );
    }
    else
    {
        ret = -EINVAL;
    }

    return ret;
}

gint32 CDBusProxyPdoLpbk::PackupReqMsg(
    DMsgPtr& pReqMsg,
    DMsgPtr& pOutMsg,
    bool bNoReply ) const
{
    // correct the sender of the inner
    std::string strSender;
    gint32 ret = GetSender( strSender );
    if( ERROR( ret ) )
        return ret;

    pReqMsg.SetSender( strSender );

    return super::PackupReqMsg(
        pReqMsg, pOutMsg, bNoReply );
}

CDBusProxyPdoLpbk::CDBusProxyPdoLpbk(
    const IConfigDb* pCfg ) :
    super( pCfg )
{
    SetClassId( clsid( CDBusProxyPdoLpbk ) );
    CCfgOpener oPortCfg( ( IConfigDb* )m_pCfgDb );
    std::string strSender;
    GetSender( strSender );
    oPortCfg.SetStrProp(
        propSrcUniqName, strSender );
}

gint32 CDBusProxyPdoLpbk::GetProperty(
    gint32 iProp, CBuffer& oBuf ) const
{
    gint32 ret = 0;

    CStdRMutex oPortLock( GetLock() );
    switch( PropIdFromInt( iProp ) )
    {
    case propSrcUniqName:
        {
            std::string strSender;
            ret = GetSender( strSender );
            if( ERROR( ret ) )
                break;
            oBuf = strSender;
            break;
        }
    default:
        {
            ret = super::GetProperty(
                iProp, oBuf );
        }
    }
    return ret;
}

}
