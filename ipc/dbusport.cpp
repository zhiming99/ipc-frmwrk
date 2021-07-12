/*
 * =====================================================================================
 *
 *       Filename:  dbusport.cpp
 *
 *    Description:  implementation of dbus port classes
 *
 *        Version:  1.0
 *        Created:  07/09/2016 11:22:40 AM
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
#include <regex>
#include "defines.h"
#include "port.h"
#include "dbusport.h"
#include "stlcont.h"
//#include "connhelp.h"
#include "emaphelp.h"
#include "ifhelper.h"

namespace rpcf
{

using namespace std;

gint32 IpAddrToBytes(
    const string& strIpAddr,
    guint8* pBytes,
    guint32& dwSize )
{
    gint32 ret = 0;

    do{
        if( pBytes == nullptr ||
            dwSize < IPV6_ADDR_BYTES ||  // to accomodate the size of ipv6
            strIpAddr.empty() )
        {
            ret = -EINVAL;
            break;
        }

        ret = inet_pton( AF_INET,
            strIpAddr.c_str(), pBytes );
        if( ret == 1 )
        {
            dwSize = IPV4_ADDR_BYTES;
            ret = 0;
            break;
        }

        ret = inet_pton( AF_INET6,
            strIpAddr.c_str(), pBytes );
        if( ret == 1 )
        {
            dwSize = IPV6_ADDR_BYTES;
            ret = 0;
            break;
        }
        else if( ret == -1 )
        {
            ret = -errno;
            break;
        }

        ret = -EINVAL;

    }while( 0 );

    return ret;
}

gint32 NormalizeIpAddr(
    gint32 dwProto,
    const std::string strIn,
    std::string& strOut )
{
    if( dwProto != AF_INET &&
        dwProto != AF_INET6 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( dwProto == AF_INET )
        {
            in_addr oAddr;
            ret = inet_aton(
                strIn.c_str(), &oAddr );
            if( ret == 0 )
            {
                ret = -EINVAL;
                break;
            }
            ret = 0;
            strOut = inet_ntoa( oAddr );
            break;
        }

        // ipv6
        in6_addr oAddr;
        ret = inet_pton( AF_INET6,
            strIn.c_str(), &oAddr );
        if( ret == 0 )
        {
            ret = -EINVAL;
            break;
        }
        else if( ret == -1 )
        {
            ret = -errno;
            break;
        }

        constexpr auto iLen =
            INET6_ADDRSTRLEN;
        char buf[ iLen ];
        const char* szRet = inet_ntop(
            AF_INET6, &oAddr, buf, iLen );

        if( szRet == nullptr )
        {
            ret = -errno;
            break;
        }
        strOut = szRet;
        ret = 0;

    }while( 0 );

    return ret;
}

/**
* @name ByteToHexChar
* @{ */
/** 
 * pRet must be a ptr to a byte array * of 2 bytes or longer @}
 * byte contains the binary byte to convert
 * the return value is stored in two chars, that is,
 * the higher 4 bits are converted to a hex digit in pRet[ 0 ],
 * and the lower 4 bits are converted to a hex digit in
 * pRet[ 1 ] */

static void BinToHexChar( guint8 byte, guint8* pRet )
{
    if( pRet == nullptr )
        return;

    if( ( byte & 0x0F ) < 0x0A )
        pRet[ 1 ] = ( byte & 0x0F ) + 0x30;
    else
        pRet[ 1 ] = ( byte & 0x0F ) - 9 + 0x40;
    
    if( ( byte & 0xF0 ) < 0xA0 )
        pRet[ 0 ] = ( byte >> 4 ) + 0x30;
    else
        pRet[ 0 ] = ( byte >> 4 ) - 9 + 0x40;

    return;
}


gint32 BytesToString(
    const guint8* bytes,
    guint32 dwSize,
    string& strRet )
{
    if( bytes == nullptr
        || dwSize == 0
        || dwSize > 1024 )
    {
        return -EINVAL;
    }

    guint8* pszRet =
        ( guint8* )alloca( dwSize * 2 + 1 );

    pszRet[ dwSize * 2 ] = 0;

    for( guint32 i = 0; i < dwSize * 2; i += 2 )
    {
        BinToHexChar(
            bytes[ ( i >> 1 ) ],
            pszRet + i );
    }

    strRet = ( char* )pszRet;

    return 0;
}

gint32 IpAddrToByteStr(
    const string& strIpAddr,
    string& strRet )
{

    // MAX_IPADDR_SIZE is far enough for an ip addr
    // whether ipv4 or ipv6
    guint8 bytes[ MAX_IPADDR_SIZE ];
    guint32 dwSize = MAX_IPADDR_SIZE;
    gint32 ret = 0;

    do{
        ret = IpAddrToBytes(
            strIpAddr, bytes, dwSize );

        if( ret < 0 )
            break;

        ret = BytesToString(
            bytes, dwSize, strRet );

    }while( 0 );

    return ret;
}

gint32 AddrIdMap::Find(
    CConnParamsProxy& ocp,
    guint32& dwPortId )
{
    if( !m_bSepConns )
    {
        ADDRID_MAP::iterator itr =
            m_pMap->find( ocp );
        if( itr == m_pMap->end() )
            return -ENOENT ;
        dwPortId = itr->second;
    }
    else
    {
        CConnParamsProxySepConns
            ocpsc( ocp.GetCfg() );
        ADDRID_MAP2::iterator itr =
            m_pMap2->find( ocpsc );
        if( itr == m_pMap2->end() )
            return -ENOENT;
        dwPortId = itr->second;
    }
    return 0;
}

gint32 AddrIdMap::Add(
    CConnParamsProxy& ocp,
    guint32 dwPortId )
{
    if( !m_bSepConns )
    {
        ADDRID_MAP::iterator itr =
            m_pMap->find( ocp );
        if( itr != m_pMap->end() )
        {
            if( itr->second == dwPortId )
                return -EEXIST;
        }
        ( *m_pMap )[ ocp ] = dwPortId;
    }
    else
    {
        CConnParamsProxySepConns
            ocpsc( ocp.GetCfg() );
        ADDRID_MAP2::iterator itr =
            m_pMap2->find( ocpsc );
        if( itr != m_pMap2->end() )
        {
            if( itr->second == dwPortId )
                return -EEXIST;
        }
        ( *m_pMap2 )[ ocpsc ] = dwPortId;
    }
    return 0;
}

gint32 AddrIdMap::Erase(
    CConnParamsProxy& ocp )
{
    if( !m_bSepConns )
    {
        m_pMap->erase( ocp );
    }
    else
    {
        CConnParamsProxySepConns
            ocpsc( ocp.GetCfg() );
        m_pMap2->erase( ocpsc );
    }
    return 0;
}

CDBusBusPort::CDBusBusPort( const IConfigDb* pConfig )
    : super( pConfig ),
    m_pDBusConn( nullptr ),
    m_iLocalPortId( -1 ),
    m_iLpbkPortId( -1 )
{
    gint32 ret = 0;
    SetClassId( clsid( CDBusBusPort ) );
    CIoManager* pMgr = GetIoMgr();
    bool bSepConns = false;
    guint32 dwRole = 0;
    ret = pMgr->GetCmdLineOpt(
        propRouterRole, dwRole );
    if( SUCCEEDED( ret ) && ( dwRole & 0x01 ) )
    {
        ret = pMgr->GetCmdLineOpt(
            propSepConns, bSepConns );
        if( ERROR( ret ) )
            bSepConns = false;
    }
    m_mapAddrToId.Initialize( bSepConns );
}

CDBusBusPort::~CDBusBusPort()
{ 
}

gint32 CDBusBusPort::BuildPdoPortName(
    IConfigDb* pCfg,
    string& strPortName )
{
    gint32 ret = 0;
    if( pCfg == nullptr )
        return -EINVAL;

    do{
        CCfgOpener oCfgOpener( pCfg );
        if( pCfg->exist( propPortClass ) )
        {
            // port name is composed in the following
            // format:
            // <PortClassName> + "_" + <PortId>
            string strClass;
            ret = oCfgOpener.GetStrProp(
                propPortClass, strClass );

            if( ERROR( ret ) )
                break;

            if( strClass != PORT_CLASS_DBUS_PROXY_PDO &&
                strClass != PORT_CLASS_DBUS_PROXY_PDO_LPBK &&
                strClass != PORT_CLASS_LOCALDBUS_PDO &&
                strClass != PORT_CLASS_LOOPBACK_PDO )
            {
                // We only support to port classes
                // RpcProxy and LocalDBusPdo
                ret = -EINVAL;
                break;
            }
            if( strClass == PORT_CLASS_DBUS_PROXY_PDO ||
                strClass == PORT_CLASS_DBUS_PROXY_PDO_LPBK )
            {
                guint32 dwPortId = ( guint32 )-1;
                if( pCfg->exist( propPortId ) )
                {
                    dwPortId = oCfgOpener[ propPortId ];
                }
                if( dwPortId == ( guint32 )-1 ||
                    dwPortId < 2 )
                {

                    IConfigDb* pConnParams = nullptr;
                    ret = oCfgOpener.GetPointer(
                        propConnParams, pConnParams );
                    if( ERROR( ret ) )
                        break;

                    CStdRMutex oPortLock( GetLock() );
                    CConnParamsProxy ocps( pConnParams );

                    ret = m_mapAddrToId.Find(
                        ocps, dwPortId );

                    if( ERROR( ret ) )
                    {
                        dwPortId = NewPdoId();
                        ret = 0;
                    }
                }
                oCfgOpener[ propPortId ] = dwPortId;
                strPortName = strClass + "_" +
                    std::to_string( dwPortId );
            }
            else if( strClass == PORT_CLASS_LOCALDBUS_PDO
                || strClass == PORT_CLASS_LOOPBACK_PDO )
            {
                if( pCfg->exist( propPortId ) )
                {
                    guint32 dwPortId = 0;
                    string strRet;
                    ret = oCfgOpener.GetIntProp(
                        propPortId, dwPortId );

                    if( ERROR( ret ) )
                        break;

                    strPortName = strClass + string( "_" )
                        + std::to_string( dwPortId );
                }
                else
                {
                    if( strClass == PORT_CLASS_LOCALDBUS_PDO )
                        strPortName = strClass + string( "_0" ); 
                    else
                    {
                        guint32 dwPortId = NewPdoId();
                        oCfgOpener[ propPortId ] = dwPortId;
                        strPortName = strClass + "_" +
                            std::to_string( dwPortId );
                    }
                }
            }
            else
            {
                ret = -ENOTSUP;
            }
        }
        else if( pCfg->exist( propPortName ) )
        {
            ret = oCfgOpener.GetStrProp(
                propPortName, strPortName );

            if( ERROR( ret ) )
                break;
        }
        else
        {
            ret = ERROR_FAIL;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::CreatePdoPort(
    IConfigDb* pCfg,
    PortPtr& pNewPort )
{
    gint32 ret = 0;

    // please be aware we are within a
    // port lock
    do{
        if( pCfg == nullptr )
            return -EINVAL;

        if( !pCfg->exist( propPortClass ) )
        {
            ret = -EINVAL;
            break;
        }

        string strPortClass;

        CCfgOpener oExtCfg( ( IConfigDb* )pCfg );

        ret = oExtCfg.GetStrProp(
            propPortClass, strPortClass );

        if( ERROR( ret ) )
            break;

        if( strPortClass
            == PORT_CLASS_DBUS_PROXY_PDO_LPBK )
        {
            ret = CreateRpcProxyPdoLpbk(
                pCfg, pNewPort );
        }
        else if( strPortClass
            == PORT_CLASS_DBUS_PROXY_PDO )
        {
            ret = CreateRpcProxyPdo(
                pCfg, pNewPort );
        }
        else if( strPortClass
            == PORT_CLASS_LOCALDBUS_PDO )
        {
            ret = CreateLocalDBusPdo(
                pCfg, pNewPort );
        }
        else if( strPortClass
            == PORT_CLASS_LOOPBACK_PDO )
        {
            ret = CreateLoopbackPdo(
                pCfg, pNewPort );
        }
        else
        {
            ret = -ENOTSUP;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::CreateRpcProxyPdoShared(
    const IConfigDb* pCfg,
    PortPtr& pNewPort,
    EnumClsid iClsid )
{
    if( pCfg == nullptr ||
        iClsid == clsid( Invalid ) )
        return -EINVAL;

    gint32 ret = 0;
    do{
        string strPortName;

        CCfgOpener oExtCfg;
        *oExtCfg.GetCfg() = *pCfg;
        guint32 dwPortId = 0;
        if( !oExtCfg.exist( propPortId ) )
        {
            ret = -EINVAL;
            break;
        }

        dwPortId = oExtCfg[ propPortId ];
        oExtCfg.SetIntPtr( propDBusConn,
            ( guint32* )m_pDBusConn );

        ret = pNewPort.NewObj(
            iClsid, oExtCfg.GetCfg() );

        if( SUCCEEDED( ret ) )
        {
            IConfigDb* pConnParams;
            ret = oExtCfg.GetPointer(
                propConnParams, pConnParams );
            if( ERROR( ret ) )
                break;

            CConnParamsProxy ocp( pConnParams );

            // bind the ip addr and the port-id
            CStdRMutex oPortLock( GetLock() );
            ret = m_mapAddrToId.Add(
                ocp, dwPortId );
        }
        // the pdo port `Start()' will be deferred
        // till the complete port stack is built.
        //
        // And well, we are done here
    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::CreateLocalDBusPdo(
    const IConfigDb* pCfg,
    PortPtr& pNewPort )
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        string strPortName;
        guint32 dwPortId = 0;

        CCfgOpener oExtCfg;
        *oExtCfg.GetCfg() = *pCfg;

        if( this->PortExist( dwPortId ) )
            return -EEXIST;

        // must-have properties in creation
        if( dwPortId > 0 )
        {
            // we support just one
            ret = -ERANGE;
            break;
        }

        oExtCfg[ propPortId ] = dwPortId;

        ret = pNewPort.NewObj(
            clsid( CDBusLocalPdo ), oExtCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        m_iLocalPortId = dwPortId;
        // the pdo port `Start()' will be deferred
        // till the complete port stack is built.
        //
        // And well, we are done here

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::CreateLoopbackPdo(
    const IConfigDb* pCfg,
    PortPtr& pNewPort )
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        string strPortName;
        guint32 dwPortId = 0;

        CCfgOpener oExtCfg;
        *oExtCfg.GetCfg() = *pCfg;

        ret = oExtCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
        {
            dwPortId = NewPdoId();

            // mandatory property for creation
            oExtCfg[ propPortId ] = dwPortId;
            ret = 0;
        }

        if( this->PortExist( dwPortId ) )
        {
            ret = -EEXIST;
            break;
        }

        ret = pNewPort.NewObj(
            clsid( CDBusLoopbackPdo ),
            oExtCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        m_iLpbkPortId = dwPortId;
        // the pdo port `Start()' will be deferred
        // till the complete port stack is built.
        //
        // And well, we are done here

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::Start( IRP *pIrp )
{

    gint32 ret = 0;
    if( !GetLowerPort().IsEmpty() )
    {
        // nothing to do here if it is
        // sitting on some other port
        pIrp->GetCurCtx()->SetStatus( 0 );
        return 0;
    }

    // let's create the DBus connection
    CDBusError error;

    do{
        string strModName;
        string strPortName;

        strModName = GetIoMgr()->GetModName();
        CMainIoLoop* pMloop =
            GetIoMgr()->GetMainIoLoop();

        // NOTE: at this point, the mainloop is
        // already started
        string strInstNo = std::to_string(
            pMloop->GetThreadId() );

        CCfgOpener oCfg( ( IConfigDb* )m_pCfgDb );
        ret = oCfg.GetStrProp(
            propPortName, strPortName );

        if( ERROR( ret ) )
            break;

        string strBusName = string( DBUS_NAME_PREFIX )
            + strModName
            + string( "-" )
            + strInstNo
            + string( "." )
            + strPortName;

        if( !dbus_threads_init_default() )
        {
            ret = -ENOMEM;
            break;
        }

        m_pDBusConn = dbus_bus_get_private(
            DBUS_BUS_SESSION, error );

        if ( nullptr == m_pDBusConn )
        {
            // fatal error, we cannot run without
            // the dbus-daemon.
            ret = error.Errno();
            break;
        }
        
        ret = oCfg.SetStrProp(
            propBusName, strBusName );

        BufPtr pBuf( true );
        *pBuf = strBusName;
        ret = this->SetProperty(
            propSrcDBusName, *pBuf );
        
        ret = InitLpbkMatch();
        if( ERROR( ret ) )
            break;

        // request the name for this connection
        ret = RegBusName(
            strBusName.c_str(),
            DBUS_NAME_FLAG_REPLACE_EXISTING
            | DBUS_NAME_FLAG_DO_NOT_QUEUE );

        if( ERROR( ret ) )
            break;

        // don't exit on disconnection, we need to
        // do some clean-up work
        dbus_connection_set_exit_on_disconnect(
            m_pDBusConn, FALSE );

        // add the master callback for this connection
        ret = dbus_connection_add_filter(
            m_pDBusConn,
            DBusMessageCallback,
            this, NULL );

        if( FALSE == ret )
        {
            // according dbus's document
            ret = -ENOMEM;
            break;
        }
        this->AddRef();

        const char* szName =
            dbus_bus_get_unique_name( m_pDBusConn );

        DebugPrint( 0,
            "dbus unique name: %s", szName );

        CParamList oParams;

        oParams.SetPointer(
            propIoMgr, GetIoMgr() );
        oParams.SetPointer(
            propPortPtr, this );
        oParams.SetIntPtr( propDBusConn,
            ( guint32* )m_pDBusConn );

        // flush connection every 500ms
        oParams.Push( 5000 );

        ret = m_pFlushTask.NewObj(
            clsid( CDBusConnFlushTask ),
            oParams.GetCfg() );

    }while( 0 );

    if( ERROR( ret ) )
    {
        if( m_pDBusConn )
        {
            dbus_connection_close( m_pDBusConn );
            dbus_connection_unref( m_pDBusConn );
            m_pDBusConn = nullptr;
        }
    }

    if( pIrp != nullptr )
    {
        pIrp->GetCurCtx()->SetStatus( ret );
    }
    return ret;
}

DBusHandlerResult
CDBusBusPort::DBusMessageCallback(
     DBusConnection *pConn,
     DBusMessage *pMsg,
     void *pData)
{
    if( pConn == nullptr
        || pData == nullptr 
        || pMsg == nullptr )
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    CDBusBusPort *pBus =
        reinterpret_cast< CDBusBusPort* >( pData );

    return pBus->OnMessageArrival( pMsg );
}

gint32 CDBusBusPortDBusOfflineTask::operator()(
    guint32 dwContext )
{
    gint32 ret = 0;
    CCfgOpener a( ( IConfigDb* )GetConfig() );
    CIoManager* pMgr = nullptr;
    ObjPtr pObj;

    do{
        ret = a.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        ret = a.GetObjPtr( propPortPtr, pObj );
        if( ERROR( ret ) )
            break;

        // broadcast this event
        // the pnp manager will stop all the
        // ports
        CDBusBusPort* pPort = pObj;
        if( pPort == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CEventMapHelper< CPort > a( pPort );
        ret = a.BroadcastEvent(
            eventConnPoint,
            eventDBusOffline, 0,
            nullptr );

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::HandleDBusDisconn()
{
    CParamList a;
    gint32 ret = 0;
    ret = a.SetPointer( propIoMgr, GetIoMgr() );
    if( ERROR( ret ) )
        return ret;

    ObjPtr pObj( this );
    ret = a.SetObjPtr( propPortPtr, pObj );
    if( ERROR( ret ) )
        return ret;

    // FIXME: do we need to broadcast this message to
    // the child PDO's ?
    ret = GetIoMgr()->ScheduleTask(
        clsid( CDBusBusPortDBusOfflineTask ),
        a.GetCfg() );

    return ret;
}
gint32 CDBusBusPort::ReplyWithError(
    DBusMessage* pMessage )
{
    if( pMessage == nullptr )
        return -EINVAL;

    DMsgPtr pMsg( pMessage ); 
    string strPath = pMsg.GetPath();
    if( strPath.empty()  )
        return -EINVAL;

    string strModName =
        GetIoMgr()->GetModName();

    string strDest =
        DBUS_DESTINATION( strModName );

    string strIfName =
        pMsg.GetInterface();

    string strError;
    string strDesc;
    if( strPath.substr( 0, strDest.size() ) == strDest )
    {
        strError = DBUS_ERROR_NO_SERVER;
        strDesc = "Please retry later";
    }
    else
    {
        strError = DBUS_ERROR_NO_REPLY;
        strDesc = "Please check the parameters";
    }

    DBusMessage* pReply = dbus_message_new_error(
        pMessage, strError.c_str(), strDesc.c_str() );

    DMsgPtr pReMsg( pReply );
    pReMsg.SetPath( strPath.c_str() );

    if( !strIfName.empty() )
        pReMsg.SetInterface( strIfName );

    return SendDBusMsg( pReply, nullptr );
}

DBusHandlerResult CDBusBusPort::OnMessageArrival(
    DBusMessage* pMessage )
{
    DBusHandlerResult ret =
        DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    do{
        if( pMessage == nullptr )
            break;

        DMsgPtr pMsg( pMessage );

        BufPtr pBuf( true );
        gint32 iType = pMsg.GetType();

        if( iType == DBUS_MESSAGE_TYPE_SIGNAL) 
        {
            // handle the disconnection message
            gint32 ret1 = 
                m_pMatchDisconn->IsMyMsgIncoming( pMsg );

            if( SUCCEEDED( ret1 ) )
            {
                if( ERROR( HandleDBusDisconn() ) )
                    break;

                // don't go forward
                ret = DBUS_HANDLER_RESULT_HANDLED;
                break;
            }
        }

        ( *pBuf ) = pMsg;

        vector< PortPtr > vecPorts;

        if( !pBuf.IsEmpty() )
        {
            CStdRMutex oPortLock( GetLock() );
            guint32 dwPortState = GetPortState();

            if( !( dwPortState == PORT_STATE_READY
                 || dwPortState == PORT_STATE_BUSY_SHARED
                 || dwPortState == PORT_STATE_BUSY
                 || dwPortState == PORT_STATE_BUSY_RESUME ) )
            {
                ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                break;
            }

            for( auto elem : m_mapId2Pdo )
            {
                guint32 dwPortId = elem.first;
                if( dwPortId == ( guint32 )m_iLocalPortId ||
                    dwPortId == ( guint32 )m_iLpbkPortId )
                    continue;

                PortPtr& pPrxyPdo = elem.second;
                if( pPrxyPdo->GetClsid() != 
                    clsid( CDBusProxyPdo ) )
                    continue;

                vecPorts.push_back( elem.second );
            }

            // place the localdbus port at the
            // last. To guarantee the
            // order of the message processing
            vecPorts.push_back(
                m_mapId2Pdo[ m_iLocalPortId ] );
        }

        if( vecPorts.empty() )
        {
            ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            break;
        }

        gint32 ret1 = 0;

        for( guint32 i = 0; i < vecPorts.size(); ++i )
        {
            PortPtr pPort = vecPorts[ i ];
            if( pPort.IsEmpty() )
                continue;

            IRpcPdoPort* pPdoPort =
                dynamic_cast< IRpcPdoPort* >( ( IPort* )pPort );

            if( pPdoPort == nullptr )
                continue;

            LONGWORD* pData = ( LONGWORD* )( ( CBuffer* )pBuf );
            ret1 = pPdoPort->OnEvent(
                cmdDispatchData, 0, 0, pData );

            switch( iType )
            {
            case DBUS_MESSAGE_TYPE_METHOD_CALL:
                {
                    if( SUCCEEDED( ret1 ) || ret1 != -ENOENT )
                        ret = DBUS_HANDLER_RESULT_HANDLED;
                    break;
                }
            case DBUS_MESSAGE_TYPE_SIGNAL:
                {
                    // iterate through all the pdo ports
                    ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                    if( ret1 == ERROR_PREMATURE )
                    {
                        ret = DBUS_HANDLER_RESULT_HANDLED;
                    }
                    break;
                }
            case DBUS_MESSAGE_TYPE_ERROR:
            case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                {
                    if( SUCCEEDED( ret1 ) || ret1 != -ENOENT)
                        ret = DBUS_HANDLER_RESULT_HANDLED;

                    if( ERROR( ret1 ) )
                    {
                        DebugPrint( ret1,
                            "Error handling return values" );
                    }
                    break;
                }
            default:
                {
                    ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                    break;
                }
            }

            if( ret == DBUS_HANDLER_RESULT_HANDLED )
                break;
        }

        if( iType == DBUS_MESSAGE_TYPE_METHOD_CALL
            && ret == DBUS_HANDLER_RESULT_NOT_YET_HANDLED )
        {
            // reply with dbus error
            DMsgPtr pDumpMsg( pMsg );
            DebugPrint( GetPortState(),
                "Error cannot find irp for response messages\n%s\n, port=%s, 0x%x",
                pDumpMsg.DumpMsg().c_str(),
                CoGetClassName( GetClsid() ), 
                ( LONGWORD )this );
            ReplyWithError( pMessage );
            ret = DBUS_HANDLER_RESULT_HANDLED;
        }

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::SendDBusMsg(
    DBusMessage *pDBusMsg, guint32* pdwSerial )
{
    gint32 ret = 0;

    if( pDBusMsg == nullptr )
        return -EINVAL;

    ret = FilterLpbkMsg( pDBusMsg, pdwSerial );
    if( ret != ERROR_NOT_HANDLED )
        return ret;

    ret = 0;
    if( !dbus_connection_send(
        m_pDBusConn, pDBusMsg, pdwSerial ) )
    {
        ret = -ENOMEM;
        DebugPrint( ret,
            "dbus_connection_send failed" );
    }
    return ret;
}

void CDBusBusPort::ReleaseDBus()
{
    if( m_pDBusConn )
    {
        CCfgOpener oCfg( ( IConfigDb* )m_pCfgDb );
        string strBusName;
        if( oCfg.exist( propBusName ) )
            strBusName = oCfg[ propBusName ];

        ReleaseBusName( strBusName );

        dbus_connection_remove_filter( m_pDBusConn,
            DBusMessageCallback, this );

        dbus_connection_close( m_pDBusConn );
        dbus_connection_unref( m_pDBusConn );

        m_pDBusConn = nullptr;
        this->Release();
    }

    return;
}

extern void SetPnpState( IRP* pIrp, guint32 state );
gint32 CDBusBusPort::AsyncStopDBusConn(
    IRP* pIrp, gint32 dwPos )
{
    gint32 ret = 0;

    // the message is allowed to come in
    CIoManager* pMgr = GetIoMgr();
    MloopPtr pLoop = pMgr->GetMainIoLoop();
    pLoop->StopDBusConn();
    ReleaseDBus();
    if( true )
    {
        CStdRMutex oIrpLock(
            pIrp->GetLock() );
        ret = pIrp->CanContinue(
            IRP_STATE_READY );
        if( ERROR( ret ) )
            return ret;

        if( pIrp->GetStackSize() > 1 )
        {
            DebugPrint( -EINVAL,
                "Irp stack should not be more than one" );
        }
        pIrp->SetCurPos( dwPos );
        SetPnpState( pIrp, PNP_STATE_STOP_PRE );
    }

    return pMgr->CompleteIrp( pIrp );
}

gint32 CDBusBusPort::PreStop( IRP* pIrp )
{
    // graceful shutdown
    TaskletPtr pTask;
    gint32 ret = DEFER_CALL_NOSCHED( pTask, this,
        &CDBusBusPort::AsyncStopDBusConn,
        pIrp, pIrp->GetCurPos() );

    CIoManager* pMgr = GetIoMgr();
    ret = pMgr->RescheduleTaskMainLoop( pTask );
    if( SUCCEEDED( ret ) )
        return STATUS_MORE_PROCESS_NEEDED;

    return ret;
}

gint32 CDBusBusPort::Stop( IRP *pIrp )
{
    CDBusConnFlushTask* pTask = m_pFlushTask;
    if( pTask != nullptr )
        pTask->Stop();
    m_pFlushTask.Clear();

    gint32 ret = super::Stop( pIrp );
    // NOTE: the child ports are stopped ahead of
    // this method is called

    m_pMatchDisconn.Clear();
    m_pMatchLpbkServer.Clear();
    m_pMatchLpbkProxy.Clear();
    return ret;
}

gint32 CDBusBusPort::GetChildPorts(
        vector< PortPtr >& vecChildPorts )
{
    return super::GetChildPorts( vecChildPorts );
}

gint32 CDBusBusPort::SchedulePortsAttachNotifTask(
    std::vector< PortPtr >& vecChildPdo,
    guint32 dwEventId,
    IRP* pMasterIrp )
{
    gint32 ret = 0;

    do{
        if( pMasterIrp != nullptr  )
        {
            if( pMasterIrp->IsPending() )
            {
                CStdRMutex oIrpLock(
                    pMasterIrp->GetLock() );

                ret = pMasterIrp->CanContinue(
                    IRP_STATE_READY );

                if( ERROR( ret ) )
                    break;

                pMasterIrp->SetMinSlaves(
                    vecChildPdo.size() );
            }
            else
            {
                pMasterIrp->SetMinSlaves(
                    vecChildPdo.size() );
            }
        }

        for( auto pNewPort : vecChildPdo ) 
        {
            CParamList oAttachArgs;

            // NOTE: at this moment, the pNewPort is
            // not registered in the registry so no
            // event map for it yet.  we will
            // broadcast the event via the event map
            // from the bus port
            ObjPtr pObj( pNewPort );
            oAttachArgs.Push( dwEventId );
            oAttachArgs.Push( pObj );

            CEventMapHelper< CGenericBusPort > b( this );
            ret = b.GetEventMap( pObj );

            if( ERROR( ret ) )
                break;

            oAttachArgs.Push( pObj );
            if( pMasterIrp != nullptr )
            {
                oAttachArgs.SetPointer(
                    propMasterIrp, pMasterIrp );
            }

            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CPortAttachedNotifTask ),
                oAttachArgs.GetCfg() );

            if( ERROR( ret ) )
                break;

            ( *pTask )( dwEventId );
        }

        if( pMasterIrp != nullptr )
        {
            if( pMasterIrp->IsPending() )
            {
                CStdRMutex oIrpLock(
                    pMasterIrp->GetLock() );

                ret = pMasterIrp->CanContinue(
                    IRP_STATE_READY );

                if( ERROR( ret ) )
                {
                    ret = 0;
                    break;
                }

                // NOTE: the master irp will finally be
                // timed out if the slave count cannot
                // drop to zero
                if( pMasterIrp->GetSlaveCount() > 0 )
                {
                    // the irp will be complete by some
                    // other people
                    ret = STATUS_PENDING;
                }
                else
                {
                    // we won't complete it here,
                    // someone else will do the job,
                    // but not now.
                }
            }
            else
            {
                if( pMasterIrp->GetSlaveCount() > 0 )
                    ret = STATUS_PENDING;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::OnPortReady( IRP* pIrp )
{
    if( pIrp == nullptr )
        return -EINVAL;

    gint32 ret = super::OnPortReady( pIrp );
    vector< PortPtr > vecChildren;
    
    {
        CStdRMutex oPortLock( GetLock() );
        GetChildPorts( vecChildren );
    }

    // the message is allowed to come in
    MloopPtr pLoop = GetIoMgr()->GetMainIoLoop();
    ret = pLoop->SetupDBusConn( m_pDBusConn );

    if( ERROR( ret ) )
        return ret;

    ret = SchedulePortsAttachNotifTask(
        vecChildren, eventPortAttached, pIrp );

    return ret;
}

void CDBusBusPort::OnPortStartFailed(
    IRP* pIrp, gint32 ret )
{
    super::OnPortStartFailed( pIrp, ret );
    ReleaseDBus();
    return;
}

gint32 CDBusBusPortCreatePdoTask::operator()(
    guint32 dwContext )
{

    // no ref increment, to match the
    // addref before schedule this event
    CCfgOpener oCfg( ( IConfigDb* )GetConfig() );

    PortPtr pNewPort;
    ObjPtr pObj;

    gint32 ret = oCfg.GetObjPtr(
        propBusPortPtr, pObj );

    if( ERROR( ret ) )
        return ret;

    CGenericBusPort* pPort = pObj;
    if( pPort )
    {
        ret = pPort->OpenPdoPort(
            GetConfig(), pNewPort );
    }
    else
    {
        ret = -EINVAL;
    }

    return SetError( ret );
}

gint32 CDBusBusPort::PostStart( IRP* pIrp )
{
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        //
        // an attempt failed to defer the irp
        // complete after all the children ports
        // are created, simply because there is a
        // moment when children are started, this
        // port is still in the starting state
        //
        // but if we complete the irp after this
        // method returns, the pdo is yet to
        // create and if there is a `stop port'
        // coming, the situation will become
        // complicated
        //

        // create the local dbus pdo here
        //
        // NOTE: do we need to add the registry?
        // Let's schedule a tasklet to create the
        // pdo
        CCfgOpener newCfg;

        // param port class
        ret = newCfg.SetStrProp(
            propPortClass, PORT_CLASS_LOCALDBUS_PDO );

        if( ERROR( ret ) )
            break;

        // param bus name
        ret = newCfg.CopyProp( propBusName, m_pCfgDb );
        if( ERROR( ret ) )
            break;

        // param io manager
        ret = newCfg.SetPointer( propIoMgr, GetIoMgr() );

        if( ERROR( ret ) )
            break;

        // param dbus connection
        ret = newCfg.SetIntPtr(
            propDBusConn, ( guint32* )m_pDBusConn );

        if( ERROR( ret ) )
            break;

        // param parent port
        ret = newCfg.SetObjPtr(
            propBusPortPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        ObjPtr pEvent;
        ret = pEvent.NewObj( clsid( CIfDummyTask ) );
        if( ERROR( ret ) )
            break;

        newCfg.SetObjPtr(
            propEventSink, pEvent );

        // no driver ptr because this is a 
        // pdo

        TaskletPtr pTask;
        pTask.NewObj(
            clsid( CDBusBusPortCreatePdoTask ),
            newCfg.GetCfg() );

        // although a synchrous call of this task
        // the PORT_START process will begin after
        // this call returns.
        ( *pTask )( eventZero );
        ret = pTask->GetError();
        if(  ERROR( ret ) )
            break;

        ret = m_pMatchDisconn.NewObj(
            clsid( CDBusDisconnMatch ),
            nullptr );

        if( ERROR( ret ) )
            break;

        // create the loopback pdo
        ret = newCfg.SetStrProp(
            propPortClass, PORT_CLASS_LOOPBACK_PDO ); 

        if( ERROR( ret ) )
            break;

        pTask.Clear();

        ret = pEvent.NewObj( clsid( CIfDummyTask ) );
        if( ERROR( ret ) )
            break;

        newCfg.SetObjPtr(
            propEventSink, pEvent );

        ret = pTask.NewObj(
            clsid( CDBusBusPortCreatePdoTask ),
            newCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        ( *pTask )( eventZero );
        ret = pTask->GetError();
        if( ret == STATUS_PENDING )
            ret = 0;

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::OnEvent( EnumEventId iEvent,
        LONGWORD dwParam1,
        LONGWORD dwParam2,
        LONGWORD* pData )
{
    return super::OnEvent(
        iEvent, dwParam1, dwParam2, pData );
}

//-------------------------------------
// Loopback related methods
//-------------------------------------
gint32 CDBusBusPort::InitLpbkMatch()
{
    gint32 ret = 0;

    do{
        CIoManager* pMgr = GetIoMgr();
        CParamList oParams;
        oParams.SetPointer( propIoMgr, pMgr );

        string strUniqName;
        CCfgOpenerObj oPortCfg( this );
        ret = oPortCfg.GetStrProp(
            propSrcUniqName, strUniqName );

        if( ERROR( ret ) )
            break;

        ret = oParams.SetStrProp(
            propDestUniqName, strUniqName );

        if( ERROR( ret ) )
            break;

        ret = oParams.SetStrProp(
            propSrcUniqName, strUniqName );

        if( ERROR( ret ) )
            break;

        ret = oParams.SetIntProp(
            propMatchType, matchClient );

        if( ERROR( ret ) )
            break;

        ret = m_pMatchLpbkProxy.NewObj(
            clsid( CDBusLoopbackMatch ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = oParams.SetIntProp(
            propMatchType, matchServer );

        ret = m_pMatchLpbkServer.NewObj(
            clsid( CDBusLoopbackMatch ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = AddBusNameLpbk( strUniqName );

    }while( 0 );

    return ret;
}

gint32 CDBusTransLpbkMsgTask::operator()(
    guint32 dwContext ) 
{
    gint32 ret = 0;
    do{
        CParamList oParams( GetConfig() );

        bool bReq = false;
        ret = oParams.Pop( bReq );
        if( ERROR( ret ) )
            break;

        DMsgPtr pMsg;
        ret = oParams.GetMsgPtr(
            propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = oParams.GetObjPtr(
            propPortPtr, pObj );

        if( ERROR( ret ) )
            break;

        CDBusBusPort* pPort = pObj;
        if( pPort == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        if( bReq )
        {
            ret = pPort->OnMessageArrival( pMsg );
        }
        else
        {
            ret = pPort->OnLpbkMsgArrival( pMsg );
        }

    }while( 0 );

    return ret;
}

extern gint64 GetRandom();
static std::atomic< guint32 > dwDBusSerial( GetRandom() >> 32 );

guint32 CDBusBusPort::LabelMessage(
    DMsgPtr& pMsg )
{
    guint32 dwSerial = 0;
    pMsg.GetSerial( dwSerial );
    if( dwSerial == 0 )
    {
        dwSerial = dwDBusSerial++;
        pMsg.SetSerial( dwSerial );
    }
    return dwSerial;
}

gint32 CDBusBusPort::ScheduleLpbkTask(
    MatchPtr& pFilter,
    DBusMessage *pDBusMsg,
    guint32* pdwSerial )
{
    gint32 ret = 0;
    do{
        gint32 iType = pFilter->GetType();
        if( true )
        {
            CStdRMutex oPortLock( GetLock() );
            ret = pFilter->IsMyMsgOutgoing( pDBusMsg );
            if( ERROR( ret ) )
                break;
        }

        CParamList oParams;
        ret = oParams.SetObjPtr(
            propPortPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        ret = oParams.SetMsgPtr(
            propMsgPtr, pDBusMsg );

        if( ERROR( ret ) )
            break;

        bool bReq = true;

        if( iType == matchServer )
            bReq = false;

        oParams.Push( bReq );
        DMsgPtr pMsg( pDBusMsg );

        guint32 dwSerial; 
        if( DBUS_MESSAGE_TYPE_METHOD_CALL
            == pMsg.GetType()
            && iType == matchClient )
        {
            // sending request, a serial number is
            // needed
            dwSerial = LabelMessage( pMsg );
            if( pdwSerial )
                *pdwSerial = dwSerial;
        }
        else
        {
            dwSerial = LabelMessage( pMsg );
        }

        CIoManager* pMgr = GetIoMgr();

        ret = pMgr->ScheduleTask(
            clsid( CDBusTransLpbkMsgTask ),
            oParams.GetCfg() );

    }while( 0 );

    if( ERROR( ret ) )
        ret = ERROR_NOT_HANDLED;

    return ret;
}

gint32 CDBusBusPort::SendLpbkMsg(
    DBusMessage *pDBusMsg, guint32* pdwSerial )
{
    if( pDBusMsg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        MatchPtr pFilter = m_pMatchLpbkProxy;
        ret = ScheduleLpbkTask(
            pFilter, pDBusMsg, pdwSerial );

    }while( 0 );

    return ret;
}

DBusHandlerResult
CDBusBusPort::OnLpbkMsgArrival(
    DBusMessage* pMsg )
{
    gint32 ret = 0;

    if( pMsg == nullptr )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DMsgPtr ptrMsg( pMsg );
    bool bEvent = false;
    if( ptrMsg.GetType() ==
        DBUS_MESSAGE_TYPE_SIGNAL )
        bEvent = true;

    PortPtr pPort;
    BufPtr pBuf( true );
    std::vector< PortPtr > vecPorts;

    do{
        CStdRMutex oPortLock( GetLock() );
        guint32 dwPortState = GetPortState();
        if( !( dwPortState == PORT_STATE_READY
             || dwPortState == PORT_STATE_BUSY_SHARED
             || dwPortState == PORT_STATE_BUSY
             || dwPortState == PORT_STATE_BUSY_RESUME ) )
        {
            ret = ERROR_STATE;
            break;
        }

        for( auto elem : m_mapId2Pdo )
        {
            guint32 dwPortId = elem.first;
            if( dwPortId == ( guint32 )m_iLocalPortId ||
                dwPortId == ( guint32 )m_iLpbkPortId )
                continue;

            PortPtr& pPrxyPdo = elem.second;
            if( pPrxyPdo->GetClsid() != 
                clsid( CDBusProxyPdoLpbk ) )
                continue;

            vecPorts.push_back( elem.second );
        }

        // place the loopback port at the
        // last. To guarantee the
        // order of the message processing
        ret = GetPdoPort( m_iLpbkPortId, pPort );
        if( ERROR( ret ) )
            break;

        vecPorts.push_back( pPort );
        *pBuf = pMsg;
        if( pBuf.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

    }while( 0 );

    if( ERROR( ret ) )
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    for( auto pPort : vecPorts )
    {
        CRpcPdoPort* pPdoPort = pPort;
        if( pPdoPort == nullptr )
            continue;

        LONGWORD* pData = ( LONGWORD* )( ( CBuffer* )pBuf );

        ret = pPdoPort->OnEvent(
            cmdDispatchData, 0, 0, pData );

        if( ret == -ENOENT )
            continue;

        if( !bEvent )
            return DBUS_HANDLER_RESULT_HANDLED;

        if( ret == ERROR_PREMATURE )
            return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

gint32 CDBusBusPort::FilterLpbkMsg(
    DBusMessage* pDBusMsg, guint32* pdwSerial )
{
    DMsgPtr pMsg( pDBusMsg );
    if( pMsg.IsEmpty() )
        return -EINVAL;

    MatchPtr pFilter = m_pMatchLpbkServer;
    gint32 ret = ScheduleLpbkTask(
        pFilter, pDBusMsg, pdwSerial );

    if( ERROR( ret ) )
        return ret;

    if( pMsg.GetType() ==
        DBUS_MESSAGE_TYPE_SIGNAL )
    {
        std::string strDest =
            pMsg.GetDestination();

        if( strDest.empty() )
            ret = ERROR_NOT_HANDLED;
    }

    return ret;
}

// methods from CObjBase

gint32 CDBusBusPort::EnumProperties(
    std::vector< gint32 >& vecProps ) const
{
    vecProps.push_back( propSrcUniqName );
    return super::EnumProperties( vecProps );
}

gint32 CDBusBusPort::GetProperty(
        gint32 iProp,
        CBuffer& oBuf ) const
{
    gint32 ret = 0;
    switch( iProp )
    {
    case propSrcUniqName:
        {
            if( m_pDBusConn == nullptr )
            {
                ret = ERROR_STATE;
                break;
            }
            const char* szName =
                dbus_bus_get_unique_name( m_pDBusConn );

            oBuf = szName;
            break;
        }
    default:
        {
            ret = super::GetProperty( iProp, oBuf );
            break;
        }
    }
    return ret;
}

gint32 CDBusBusPort::SetProperty(
        gint32 iProp,
        const CBuffer& oBuf )
{
    gint32 ret = 0;
    switch( iProp )
    {
    case propSrcUniqName:
        {
            ret = -ENOTSUP;
            break;
        }
    default:
        {
            ret = super::SetProperty(
                iProp, oBuf );
            break;
        }
    }
    return ret;
}

gint32 CDBusBusPort::AllocIrpCtxExt(
    IrpCtxPtr& pIrpCtx,
    void* pContext ) const
{
    gint32 ret = 0;
    switch( pIrpCtx->GetMajorCmd() )
    {
    case IRP_MJ_ASSOC:
        {
            switch( pIrpCtx->GetMinorCmd() )
            {
            case IRP_MN_ASSOC_RUN:
                {
                    CStartStopPdoCtx* pExt = nullptr;

                    BufPtr pBuf( true );

                     pBuf->Resize( 4 +
                        sizeof( CStartStopPdoCtx ) );

                    if( ERROR( ret ) )
                        break;

                    pExt = new (pBuf->ptr() ) CStartStopPdoCtx;
                    if( pExt == nullptr )
                    {
                        ret = -ENOMEM;
                        break;
                    }

                    ret = pIrpCtx->SetExtBuf( pBuf );
                    break;
                }
            default:
                {   
                    ret = -ENOTSUP;
                    break;
                }
            }
            break;
        }
    default:
        ret = -ENOTSUP;
        break;
    }

    if( ret == -ENOTSUP )
    {
        ret = super::AllocIrpCtxExt(
            pIrpCtx, pContext );
    }

    return ret;
}

gint32 CDBusBusPort::ReleaseIrpCtxExt(
    IrpCtxPtr& pIrpCtx,
    void* pContext )
{
    gint32 ret = 0;
    switch( pIrpCtx->GetMajorCmd() )
    {
    case IRP_MJ_ASSOC:
        {
            switch( pIrpCtx->GetMinorCmd() )
            {
            case IRP_MN_ASSOC_RUN:
                {

                    BufPtr pBuf;
                    pIrpCtx->GetExtBuf( pBuf );
                    if( pBuf.IsEmpty() )
                        break;

                    CStartStopPdoCtx* pspc =
                        ( CStartStopPdoCtx* )pBuf->ptr();

                    pspc->~CStartStopPdoCtx();
                    break;
                }
            default:
                {   
                    ret = -ENOTSUP;
                    break;
                }
            }
            break;
        }
    default:
        ret = -ENOTSUP;
        break;
    }

    if( ret == -ENOTSUP )
    {
        ret = super::ReleaseIrpCtxExt(
            pIrpCtx, pContext );
    }

    return ret;
}

gint32 CDBusBusPort::OnCompleteSlaveIrp(
    IRP* pMaster, IRP* pSlave )
{
    if( pMaster == nullptr )
        return -EINVAL;

    IrpCtxPtr& pIrpCtx =
        pMaster->GetTopStack();

    bool bLast = false;
    if( pMaster->GetSlaveCount() == 0 )
        bLast = true;

    gint32 ret = 0;

    do{
        size_t iCount =
            pMaster->m_vecCtxStack.size();

        if( iCount < 2 )
        {
            // bad stack layout
            ret = -EINVAL;
            break;
        }

        BufPtr pBuf;
        pIrpCtx->GetExtBuf( pBuf );
        if( pBuf.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        CStartStopPdoCtx* pspc = reinterpret_cast
            < CStartStopPdoCtx* >( pBuf->ptr() );

        IPort* pPdo = pSlave->GetTopPort();
        if( pPdo == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        guint32 dwPortId = 0xFFFFFFFF;

        CCfgOpenerObj oPortCfg( pPdo );
        ret = oPortCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        pspc->m_mapIdToRes[ dwPortId ] =
            pSlave->GetStatus();

        if( !bLast )
        {
            ret = STATUS_PENDING;
            break;
        }

        if( pspc->m_mapIdToRes.find( m_iLocalPortId )
            == pspc->m_mapIdToRes.end() )
        {
            ret = ERROR_FAIL;
            break;
        }
        
        ret = pspc->m_mapIdToRes[ m_iLocalPortId ];

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::AddRemoveBusNameLpbk(
    const std::string& strName, bool bRemove )
{
    gint32 ret = 0;
    if( strName.empty() )
        return -EINVAL;

    do{
        CStdRMutex oPortLock( GetLock() );

        CDBusLoopbackMatch* pMatch =
            m_pMatchLpbkProxy;

        CDBusLoopbackMatch* pMatchSvr =
            m_pMatchLpbkServer;

        if( unlikely( pMatch == nullptr ||
            pMatchSvr == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        if( bRemove )
        {
            ret = pMatch->RemoveBusName( strName );
            pMatchSvr->RemoveBusName( strName );
        }
        else
        {
            ret = pMatch->AddBusName( strName );
            if( ERROR( ret ) && ret != -EEXIST )
                break;

            ret = 0;
            pMatchSvr->AddBusName( strName );
        }

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::AddBusNameLpbk(
    const std::string& strName )
{
    return AddRemoveBusNameLpbk(
        strName, false );
}

gint32 CDBusBusPort::RemoveBusNameLpbk(
    const std::string& strName )
{
    return AddRemoveBusNameLpbk(
        strName, true );
}

gint32 CDBusBusPort::IsRegBusName(
    const std::string& strName )
{
    CStdRMutex oPortLock( GetLock() );
    CDBusLoopbackMatch* pMatch =
        m_pMatchLpbkProxy;

    return pMatch->IsRegBusName( strName );
}

gint32 CDBusBusPort::RegBusName(
    const std::string& strName,
    guint32 dwFlags )
{
    gint32 ret = 0;

    do{
        if( m_pDBusConn == nullptr )
            return ERROR_STATE;

        CDBusError error;
        // request the name for this connection
        ret = dbus_bus_request_name(
            m_pDBusConn,
            strName.c_str(),
            dwFlags,
            error );

        if( ret == -1 )
        {
            ret = error.Errno();
            break;
        }

        switch( ret )
        {
        case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
        case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
            ret = 0;
            break;

        case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
        case DBUS_REQUEST_NAME_REPLY_EXISTS:
            {
                ret = -EAGAIN;
                break;
            }
        default:
            {
                ret = ERROR_FAIL;
                break;
            }
        }

        if( ERROR( ret ) )
            break;

        AddBusNameLpbk( strName );

        DMsgPtr pMsg;
        ret = BuildModOnlineMsgLpbk(
            strName, true, pMsg );
        if( ERROR( ret ) )
            break;

        guint32 dwSerial = 0;
        ret = FilterLpbkMsg( pMsg, &dwSerial );
        if( ret == ERROR_NOT_HANDLED )
            ret = 0;

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::BuildModOnlineMsgLpbk(
    const std::string& strModName,
    bool bOnline, DMsgPtr& pMsg )
{
    gint32 ret = 0;
    do{
        ret = pMsg.NewObj(
            ( EnumClsid )DBUS_MESSAGE_TYPE_SIGNAL );

        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        MatchPtr pMatch;
        pMatch.NewObj(
            clsid( CDBusSysMatch ) );

        CCfgOpenerObj matchCfg(
            ( CObjBase*) pMatch );

        string strObjPath;

        matchCfg.GetStrProp(
            propObjPath, strObjPath );

        pMsg.SetPath( strObjPath );

        string strIfName;
        matchCfg.GetStrProp(
            propIfName, strIfName );

        pMsg.SetInterface( strIfName );
        pMsg.SetMember( "NameOwnerChanged" );
        
        CCfgOpenerObj oPortCfg( this );

        string strUniqName;
        ret = oPortCfg.GetStrProp(
            propSrcUniqName, strUniqName );

        if( ERROR( ret ) )
            break;

        string strSender;
        ret = oPortCfg.GetStrProp(
            propSrcDBusName, strSender );

        if( ERROR( ret ) )
            strSender = strUniqName;

        pMsg.SetSender( strSender );
        pMsg.SetDestination( strUniqName );
        pMsg.SetSerial( 10 );

        string strNewOwner = "";

        const char* pszModName = strModName.c_str();
        const char* pszOldOwner = strUniqName.c_str();
        const char* pszNewOwner = ""; 

        if( bOnline )
        {
            pszOldOwner = "";
            pszNewOwner = strUniqName.c_str();
        }

        if( !dbus_message_append_args( pMsg,
            DBUS_TYPE_STRING, &pszModName,
            DBUS_TYPE_STRING, &pszOldOwner,
            DBUS_TYPE_STRING, &pszNewOwner,
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }

    }while( 0 );

    if( ERROR( ret ) )
        pMsg.Clear();

    return ret;
}

gint32 CDBusBusPort::ReleaseBusName(
    const std::string& strName )
{
    gint32 ret = 0;

    do{
        CDBusError dbusError;
        ret = dbus_bus_release_name(
            m_pDBusConn,
            strName.c_str(),
            dbusError );

        if( ret == DBUS_RELEASE_NAME_REPLY_RELEASED )
        {
            ret = 0;
        }
        else
        {
            ret = ERROR_FAIL;
            break;
        }
        RemoveBusNameLpbk( strName );

        DMsgPtr pMsg;

        ret = BuildModOnlineMsgLpbk(
            strName, false, pMsg );
        if( ERROR( ret ) )
            break;

        guint32 dwSerial = 0;
        ret = FilterLpbkMsg( pMsg, &dwSerial );
        if( ret == ERROR_NOT_HANDLED )
            ret = 0;

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::AddRules(
    const std::string& strRules )
{
    gint32 ret = 0;
    do{
        CStdRMutex oPortLock( GetLock() );
        std::map< std::string, gint32 >::iterator
            itr = m_mapRules.find( strRules );

        if( itr != m_mapRules.end() )
        {
            itr->second++;
            break;
        }

        m_mapRules[ strRules ] = 1;
        oPortLock.Unlock();

        CDBusError dbusError;
        dbus_bus_add_match(
            m_pDBusConn,
            strRules.c_str(),
            dbusError );

        if( dbusError.GetName() != nullptr )
        {
            ret = dbusError.Errno();
            oPortLock.Lock();
            --m_mapRules[ strRules ];
            if( m_mapRules[ strRules ] == 0 )
                m_mapRules.erase( strRules );
        }

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::RemoveRules(
    const std::string& strRules )
{
    gint32 ret = 0;

    do{
        CStdRMutex oPortLock( GetLock() );
        std::map< std::string, gint32 >::iterator
            itr = m_mapRules.find( strRules );

        if( itr != m_mapRules.end() )
        {
            --( itr->second );
            if( itr->second > 0 )
                break;

            m_mapRules.erase( itr );
        }

        oPortLock.Unlock();

        CDBusError dbusError;
        dbus_bus_remove_match( 
            m_pDBusConn,
            strRules.c_str(),
            dbusError );

        if( dbusError.GetName() != nullptr )
            ret = dbusError.Errno();

    }while( 0 );

    return ret;
}

gint32 CDBusBusPort::IsDBusSvrOnline(
    const std::string& strDest )
{
    gint32 ret = 0;
    do{
        CDBusError dbusError;
        if( !dbus_bus_name_has_owner(
            m_pDBusConn,
            strDest.c_str(),
            dbusError ) )
        {
            // detect if the server is online
            // or not
            if( dbusError.GetName() == nullptr )
            {
                ret = ENOTCONN;
            }
            else
            {
                ret = dbusError.Errno();
            }
            break;
        }

    }while( 0 );

    return ret;
}

void CDBusBusPort::RemovePdoPort(
        guint32 iPortId )
{
    PortPtr pPort;
    CStdRMutex oPortLock( GetLock() );
    gint32 ret = GetPdoPort( iPortId, pPort );
    if( ERROR( ret ) )
        return;

    super::RemovePdoPort( iPortId );

    CCfgOpenerObj oPortCfg(
        ( IPort* )pPort );

    IConfigDb* pConnParams;
    ret = oPortCfg.GetPointer(
        propConnParams, pConnParams );

    if( ERROR( ret ) )
        return;

    CConnParamsProxy ocps( pConnParams );
    m_mapAddrToId.Erase( ocps );
}

CDBusConnFlushTask::CDBusConnFlushTask(
    const IConfigDb* pCfg )
    : m_pMgr( nullptr ),
    m_pBusPort( nullptr ),
    m_pDBusConn( nullptr ),
    m_dwIntervalMs( 0 ),
    m_hTimer( INVALID_HANDLE )
{
    gint32 ret = 0;
    SetClassId( clsid( CDBusConnFlushTask ) );
    do{
        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }
        CCfgOpener oCfg( pCfg );
        ret = oCfg.GetPointer(
            propIoMgr, m_pMgr );
        if( ERROR( ret ) )
            break;

        guint32* pVal = nullptr;
        ret = oCfg.GetIntPtr( propDBusConn, pVal );
        if( ERROR( ret ) )
            break;

        m_pDBusConn = reinterpret_cast
            < DBusConnection* >( pVal );

        ret = oCfg.GetPointer(
            propPortPtr, m_pBusPort );
        if( ERROR( ret ) )
            break;

        ret = oCfg.GetIntProp(
            0, m_dwIntervalMs );
        if( ERROR( ret ) )
            break;

        CMainIoLoop* pMloop =
            m_pMgr->GetMainIoLoop();

        CParamList oParams(
            ( IConfigDb* )GetConfig() );

        // interval in MS
        oParams.Push( m_dwIntervalMs );
        // true to repeat the timer
        oParams.Push( true );
        // true to start immediately
        oParams.Push( true );

        TaskletPtr pTask( this );
        ret = pMloop->AddTimerWatch(
            pTask, m_hTimer );

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error in CInterfaceServer ctor" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CDBusConnFlushTask::operator()(
    guint32 dwContext )
{
    if( dwContext != eventTimeout )
        return SetError( -EINVAL );
    dbus_connection_flush( m_pDBusConn );

    return G_SOURCE_CONTINUE;
}

CDBusConnFlushTask::~CDBusConnFlushTask()
{
    Stop();
}
gint32 CDBusConnFlushTask::Stop()
{
    if( m_hTimer != INVALID_HANDLE &&
        m_pMgr != nullptr )
    {
        CMainIoLoop* pMloop =
            m_pMgr->GetMainIoLoop();
        if( pMloop == nullptr )
            return -EFAULT;
        pMloop->RemoveTimerWatch( m_hTimer );
        m_hTimer = INVALID_HANDLE;
    }
    return 0;
}

}
