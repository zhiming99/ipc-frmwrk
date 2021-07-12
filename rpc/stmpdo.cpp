/*
 * =====================================================================================
 *
 *       Filename:  stmpdo.cpp
 *
 *    Description:  implementation of CTcpStreamPdo, CTcpBusPort, and CRpcTcpFido
 *
 *        Version:  1.0
 *        Created:  05/19/2017 07:03:49 PM
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

#include "defines.h"
#include "stlcont.h"
#include "dbusport.h"
#include "tcpport.h"
#include "emaphelp.h"
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <limits.h>
#include "reqopen.h"
#include "jsondef.h"
#include "ifhelper.h"

namespace rpcf
{

using namespace std;

CRpcTcpBusPort::CRpcTcpBusPort(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CRpcTcpBusPort ) );
    CCfgOpener oCfg( pCfg );
    gint32 ret = oCfg.GetIntProp(
        propMaxConns, m_dwConns );
    if( SUCCEEDED( ret ) )
    {
        CCfgOpenerObj oPortCfg( this );
        oPortCfg.RemoveProperty( propMaxConns );
    }
}

gint32 CRpcTcpBusPort::GetPdoAddr(
    guint32 dwPortId, PDOADDR& oAddr )
{
    std::map< guint32, PDOADDR >::iterator
        itr = m_mapIdToAddr.find( dwPortId );
    if( itr == m_mapIdToAddr.end() )
        return -ENOENT;
    oAddr = itr->second;
    return 0;
}

gint32 CRpcTcpBusPort::GetPortId(
    PDOADDR& oAddr, guint32& dwPortId )
{
    std::map< PDOADDR, guint32 >::iterator
        itr = m_mapAddrToId.find( oAddr );
    if( itr == m_mapAddrToId.end() )
        return -ENOENT;

    dwPortId = itr->second;
    return 0;
}

gint32 CRpcTcpBusPort::BindPortIdAndAddr(
    guint32 dwPortId, PDOADDR oAddr )
{
    CStdRMutex oPortLock( GetLock() );
    m_mapAddrToId[ oAddr ] = dwPortId;
    m_mapIdToAddr[ dwPortId ] = oAddr;
    return 0;
}

gint32 CRpcTcpBusPort::RemovePortId(
    guint32 dwPortId )
{
    PDOADDR oAddr;
    gint32 ret = GetPdoAddr( dwPortId, oAddr );
    if( ERROR( ret ) )
        return ret;

    m_mapAddrToId.erase( oAddr );
    m_mapIdToAddr.erase( dwPortId );
    return 0;
}

gint32 CRpcTcpBusPort::RemovePortAddr(
    PDOADDR& oAddr )
{
    guint32 dwPortId = 0;
    gint32 ret = GetPortId( oAddr, dwPortId );
    if( ERROR( ret ) )
        return ret;

    m_mapAddrToId.erase( oAddr );
    m_mapIdToAddr.erase( dwPortId );
    return 0;
}
void CRpcTcpBusPort::RemovePdoPort(
        guint32 iPortId )
{
    CStdRMutex oPortLock( GetLock() );
    super::RemovePdoPort( iPortId );
    RemovePortId( iPortId );
}

gint32 CRpcTcpBusPort::BuildPdoPortName(
    IConfigDb* pCfg,
    string& strPortName )
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpener oCfgOpener( pCfg );

        if( pCfg->exist( propPortName ) )
        {
            ret = oCfgOpener.GetStrProp(
                propPortName, strPortName );

            if( SUCCEEDED( ret ) )
                break;
        }

        // port name is composed in the following
        // format:
        // <PortClassName> + "_" + <IPADDR in hex>
        string strClass;
        ret = oCfgOpener.GetStrProp(
            propPortClass, strClass );

        if( strClass != PORT_CLASS_TCP_STREAM_PDO &&
            strClass != PORT_CLASS_TCP_STREAM_PDO2 )
        {
            StrSetPtr psetClasses( true );
            // search the config to see if we
            // support it.
            ret = EnumPdoClasses( psetClasses );
            if( ERROR( ret ) )
            {
                ret = -ENOTSUP;
                break;
            }
            if( ( *psetClasses )().find( strClass ) ==
                ( *psetClasses )().end() )
            {
                ret = -ENOTSUP;
                break;
            }
        }

        guint32 dwPortId = ( guint32 )-1;
        if( pCfg->exist( propPortId ) )
        {
            ret = oCfgOpener.GetIntProp(
                propPortId, dwPortId ) ;

            if( ERROR( ret ) )
                dwPortId = ( guint32 )-1;
        }

        if( dwPortId != ( guint32 )-1 )
        {
            // server side
            strPortName = strClass + "_" +
                std::to_string( dwPortId );
            break;
        }
        else if( pCfg->exist( propConnParams ) )
        {
            // proxy side
            // ip addr must exist
            IConfigDb* pcp = nullptr;
            ret = oCfgOpener.GetPointer(
                propConnParams, pcp );
            if( ERROR( ret ) )
                break;

            CStdRMutex oPortLock( GetLock() );
            CConnParams ocps( pcp );
            PDOADDR oAddr( ocps );
            std::map< PDOADDR, guint32 >::iterator
                itr = m_mapAddrToId.find( oAddr );
            
            if( itr != m_mapAddrToId.end() )
            {
                dwPortId = itr->second;
            }
            else
            {
                dwPortId = NewPdoId();
            }
            oCfgOpener[ propPortId ] = dwPortId;
            strPortName = strClass + "_" +
                std::to_string( dwPortId );
        }
        else
        {
            ret = -EINVAL;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusPort::CreatePdoPort(
    IConfigDb* pCfg,
    PortPtr& pNewPort )
{
    gint32 ret = 0;

    // Note that we are within a port lock
    do{
        if( pCfg == nullptr )
            return -EINVAL;

        if( !pCfg->exist( propPortClass ) )
        {
            ret = -EINVAL;
            break;
        }

        string strClass;
        CCfgOpener oExtCfg( ( IConfigDb* )pCfg );

        ret = oExtCfg.GetStrProp(
            propPortClass, strClass );

        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        if( strClass ==
            PORT_CLASS_TCP_STREAM_PDO )
        {
            ret = CreateTcpStreamPdo(
                pCfg, pNewPort,
                clsid( CTcpStreamPdo ) );
        }
        else if( strClass ==
            PORT_CLASS_TCP_STREAM_PDO2 )
        {
            ret = CreateTcpStreamPdo(
                pCfg, pNewPort,
                clsid( CTcpStreamPdo2 ) );
        }
        else
        {
            strClass =
                ExClassToInClass( strClass );

            EnumClsid iClsid = CoGetClassId(
                strClass.c_str() );
            if( iClsid == clsid( Invalid ) )
            {
                ret = -ENOTSUP;
                break;
            }
            ret = CreateTcpStreamPdo(
                pCfg, pNewPort, iClsid );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusPort::CreateTcpStreamPdo(
    IConfigDb* pCfg,
    PortPtr& pNewPort,
    const EnumClsid& iClsid ) const
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        string strPortName;
        guint32 dwPortId = ( guint32 )-1;

        CCfgOpener oExtCfg( ( IConfigDb* )pCfg );
        oExtCfg.GetIntProp(
            propPortId, dwPortId );

        // verify if the port already exists
        if( this->PortExist( dwPortId ) )
        {
            ret = -EEXIST;
            break;
        }

        ret = pNewPort.NewObj(
            iClsid, pCfg );

        // the pdo port `Start()' will be deferred
        // till the complete port stack is built.
        //
        // And well, we are done here

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusPort::LoadPortOptions(
    IConfigDb* pCfg )
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = -ENOENT;
    do{
        CIoManager* pMgr = GetIoMgr();

        guint32 dwRole = 0;

        ret = pMgr->GetCmdLineOpt(
            propRouterRole, dwRole );

        if( ERROR( ret ) )
            dwRole = 1;

        CCfgOpener oCfg( pCfg );
        if( dwRole & 0x02 )
            oCfg[ propListenSock ] = true;
        else
            oCfg[ propListenSock ] = false;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusPort::PostStart(
    IRP* pIrp )
{
    gint32 ret = 0;
    do{
        ObjPtr pObj;
        CCfgOpenerObj oPortCfg( this );
        ret = oPortCfg.GetObjPtr(
            propConnParams, pObj );

        if( ERROR( ret ) )
            break;

        ObjVecPtr pParams = pObj;
        if( pParams.IsEmpty() ||
            ( *pParams )().empty() )
        {
            ret = -EINVAL;
            break;
        }

        CCfgOpener oOptions;
        LoadPortOptions( oOptions.GetCfg() );

        bool bListening = false;
        oOptions.GetBoolProp(
            propListenSock, bListening );

        if( !bListening )
            break;

        CStlObjVector::MyType& vecParams =
            ( *pParams )();

        for( auto elem : vecParams )
        {
            CfgPtr pCfg( true );
            SockPtr pListenSock;

            CCfgOpener oCfg(
                ( IConfigDb* )pCfg );

            oCfg.SetPointer( propIoMgr, GetIoMgr() );
            oCfg.SetPointer( propPortPtr, this );
            oCfg.SetObjPtr( propConnParams, elem );

            ret = pListenSock.NewObj(
                clsid( CRpcListeningSock ), pCfg );

            if( ERROR( ret ) )
                break;

            ret = pListenSock->Start();
            if( ERROR( ret ) )
                break;

            m_vecListenSocks.push_back(
                pListenSock );
        }

        if( ERROR( ret ) )
            break;

        oPortCfg.RemoveProperty( propConnParams );
        ret = super::PostStart( pIrp );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusPort::PreStop(
    IRP* pIrp )
{
    // 
    // NOTE: this routine could be repeated many
    // times till it returns
    // STATUS_MORE_PROCESS_NEEDED
    //
    gint32 ret = super::PreStop( pIrp );
    if( ret != STATUS_MORE_PROCESS_NEEDED )
    {
        for( auto elem : m_vecListenSocks )
        {
            ret = elem->Stop();
            elem.Clear();
        }
    }
    return ret;
}

gint32 CRpcTcpBusPort::OnEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData )
{
    gint32 ret = 0;

    switch( iEvent )
    {
    case eventNewConn:
        {
            CRpcListeningSock* pSock;
            pSock = reinterpret_cast
                < CRpcListeningSock* >( pData );

            ret = OnNewConnection(
                dwParam1, pSock );

            break;
        }
    default:
        {
            ret = super::OnEvent( iEvent,
                dwParam1, dwParam2, pData );
            break;
        }
    }

    return ret;
}

gint32 CRpcTcpBusPort::OnNewConnection(
    gint32 iSockFd, CRpcListeningSock* pSock )
{
    if( iSockFd < 0 || pSock == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( true )
        {
            CStdRMutex oPortLock( GetLock() );
            guint32 dwConns =
                GetConnections() + 1;
            if( dwConns > GetMaxConns() )
            {
                oPortLock.Unlock();
                ret = shutdown( iSockFd, SHUT_RDWR );
                if( ERROR( ret ) )
                {
                    DebugPrint( -errno,
                        "Error shutdown socket[%d]",
                        iSockFd );
                }
                ret = close( iSockFd );
                break;
            }
        }
        // passive connection
        CCfgOpener oCfg;
        CCfgOpenerObj oPortCfg( this );
        CCfgOpenerObj oSockCfg( pSock );

        IConfigDb* pConnParams;
        ret = oSockCfg.GetPointer(
            propConnParams, pConnParams );
        if( ERROR( ret ) )
            break;

        CParamList oNewConn;
        oNewConn.Append( pConnParams );

        string strPortName;
        ret = oPortCfg.GetStrProp(
            propPortName, strPortName );

        if( ERROR( ret ) )
            break;
        
        ret = oCfg.SetStrProp(
            propBusName, strPortName );

        if( ERROR( ret ) )
            break;

        sockaddr_in6 oAddr;
        socklen_t iSize = sizeof( oAddr );
        ret = getpeername( iSockFd,
            ( sockaddr* )&oAddr, &iSize );

        if( ret == -1 )
        {
            ret = -errno;
            break;
        }

        char szNode[ 32 ];
        char szServ[ 8 ];

        ret = getnameinfo( ( sockaddr* )&oAddr,
            iSize, szNode, sizeof( szNode ),
            szServ, sizeof( szServ ),
            NI_NUMERICHOST | NI_NUMERICSERV );

        if( ret != 0 )
            break;
            
        string strSrcIp = szNode;
        if( strSrcIp.empty() )
        {
            ret = -EFAULT;
            break;
        }

        guint32 dwSrcPortNum =
            std::stoi( szServ );

        guint32 dwPortId = NewPdoId();

        if( ERROR( ret ) )
            break;

        ret = oCfg.SetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        ret = oNewConn.SetStrProp(
            propSrcIpAddr, strSrcIp );

        if( ERROR( ret ) )
            break;

        ret = oNewConn.SetIntProp(
            propSrcTcpPort, dwSrcPortNum );

        if( ERROR( ret ) )
            break;

        std::string strClass;

        ret = oPortCfg.GetStrProp(
            propChildPdoClass, strClass );

        if( ERROR( ret ) )
            strClass = PORT_CLASS_TCP_STREAM_PDO2;

        ret = oCfg.SetStrProp(
            propPortClass,
            strClass );

        if( ERROR( ret ) )
            break;

        ret = oCfg.SetIntProp(
            propFd, iSockFd );

        if( ERROR( ret ) )
            break;

        oCfg.SetPointer( propConnParams,
            ( IConfigDb* )oNewConn.GetCfg() );

        PortPtr pPort;
        ret = OpenPdoPort(
            oCfg.GetCfg(), pPort );

        if( ret == -EAGAIN )        
        {
            // Performance Alert: running on the
            // mainloop thread
            usleep( 1000 );
        }
        else
        {
            break;
        }

    }while( 1 );

    return ret;
}

// methods from CObjBase
gint32 CRpcTcpBusPort::GetProperty(
        gint32 iProp,
        CBuffer& oBuf ) const
{
    gint32 ret = 0;
    switch( iProp )
    {
    case propSrcDBusName:
    case propSrcUniqName:
        {
            ret = -ENOENT;
            break;
        }
    default:
        {
            ret = super::GetProperty(
                iProp, oBuf );
            break;
        }
    }
    return ret;
}

gint32 CRpcTcpBusPort::SetProperty(
        gint32 iProp,
        const CBuffer& oBuf )
{
    gint32 ret = 0;
    switch( iProp )
    {
    case propSrcDBusName:
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

gint32 CRpcTcpBusPort::CreateMLoopPool()
{
    gint32 ret = 0; 
    do{
        CIoManager* pMgr = GetIoMgr();
        guint32 dwCount = 1;

        guint32 dwRole = 2;
        ret = pMgr->GetCmdLineOpt(
            propRouterRole, dwRole );

        if( ERROR( ret ) )
            ret = 0;

        if( dwRole & 0x2 )
            dwCount = pMgr->GetNumCores();

        for( int i = 0; i < dwCount; ++i )
        {
            CParamList oCfg;
            oCfg.SetPointer( propIoMgr,
                this->GetIoMgr() );
            std::string strName( "SockLoop-" );
            strName += std::to_string( i );

            // thread name
            oCfg.Push( strName );

            // don't start new thread
            oCfg.Push( true );

            MloopPtr pLoop;
            ret = pLoop.NewObj(
                clsid( CMainIoLoop ),
                oCfg.GetCfg() );

            if( ERROR( ret ) )
                break;

            ret = pLoop->Start();
            if( ERROR( ret ) )
                break;

            m_mapLoops[ pLoop ] = 0;
        }
            
    }while( 0 );

   return ret;
}

gint32 CRpcTcpBusPort::DestroyMLoopPool()
{
    gint32 ret = 0; 
    do{
        guint32 dwCount =
            GetIoMgr()->GetNumCores();
        for( auto elem : m_mapLoops )
        {
            CMainIoLoop* pLoop = elem.first;
            gint32 iRet = pLoop->GetError();
            if( iRet == STATUS_PENDING )
                iRet = 0;
            // don't call pLoop->Stop, which is
            // dedicated to stop the mainloop
            pLoop->WakeupLoop( aevtStop, iRet );
            pLoop->WaitForQuit();
        }
        m_mapLoops.clear();

    }while( 0 );
    return ret;
}

gint32 CRpcTcpBusPort::AllocMainLoop(
    MloopPtr& pLoop )
{
    if( !m_bRfc )
    {
        pLoop = GetIoMgr()->GetMainIoLoop();
        return STATUS_SUCCESS;
    }

    gint32 ret = STATUS_SUCCESS;
    do{
        CStdRMutex oLock( GetLock() );
        guint32 dwCount = 10000;
        LOOPMAP_ITR itr = m_mapLoops.begin();
        LOOPMAP_ITR itrRet = itr;
        for( ; itr != m_mapLoops.end(); ++itr )
        {
            if( dwCount > itr->second )
            {
                itrRet = itr;
                dwCount = itr->second;
            }
        }

        ++itrRet->second;
        pLoop = itrRet->first;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusPort::ReleaseMainLoop(
    MloopPtr& pLoop )
{
    if( !m_bRfc )
        return STATUS_SUCCESS;

    if( pLoop.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oLock( GetLock() );
        LOOPMAP_ITR itr =
            m_mapLoops.find( pLoop );
        if( itr == m_mapLoops.end() )
            break;
        --itr->second;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusPort::PreStart( IRP* pIrp )
{
    gint32 ret = 0;
    do{
        CIoManager* pMgr = GetIoMgr();
        bool bRfc = false;
        ret = pMgr->GetCmdLineOpt(
            propEnableRfc, bRfc );
        if( ERROR( ret ) )
        {
            m_bRfc = false;
            ret = 0;
            break;
        }

        m_bRfc = bRfc;
        if( !m_bRfc )
            break;

        ret = CreateMLoopPool();
        if( ERROR( ret ) )
            return ret;

    }while( 0 );

    return super::PreStart( pIrp );
}

gint32 CRpcTcpBusPort::PostStop( IRP* pIrp )
{
    gint32 ret = 0;
    if( m_bRfc )
        ret = DestroyMLoopPool();
    if( ERROR( ret ) )
        return ret;
    return super::PostStop( pIrp );
}

CRpcTcpBusDriver::CRpcTcpBusDriver(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CRpcTcpBusDriver ) );
}

gint32 CRpcTcpBusDriver::GetTcpSettings(
    IConfigDb* pCfg ) const
{
    if( pCfg == nullptr )
        return -EINVAL;

    CIoManager* pMgr = GetIoMgr();
    CDriverManager& oDrvMgr = pMgr->GetDrvMgr();
    Json::Value& ojc = oDrvMgr.GetJsonCfg();
    Json::Value& oPorts = ojc[ JSON_ATTR_PORTS ];

    if( oPorts == Json::Value::null )
        return -ENOENT;

    if( !oPorts.isArray() || oPorts.size() == 0 )
        return -ENOENT;

    CCfgOpener oCfg( pCfg );
    gint32 ret = 0;

    do{
        guint32 i = 0;
        for( ; i < oPorts.size(); i++ )
        {
            Json::Value& elem = oPorts[ i ];
            if( elem == Json::Value::null )
                continue;

            string strPortClass =
                elem[ JSON_ATTR_PORTCLASS ].asString();
            if( strPortClass != PORT_CLASS_RPC_TCPBUS )
                continue;

            guint32 dwConns = TCP_MAX_CONNS;
            if( ( elem.isMember( JSON_ATTR_TCPCONN ) &&
                elem[ JSON_ATTR_TCPCONN ].isString() ) )
            {
                std::string strConns =
                    elem[ JSON_ATTR_TCPCONN ].asString();
                char* pEnd = nullptr;
                const char* pBegin = strConns.c_str();
                dwConns = strtol( pBegin, &pEnd, 10 );
                if( dwConns == 0 && pEnd == pBegin )
                    dwConns = TCP_MAX_CONNS;
                else if( dwConns == LONG_MIN ||
                    dwConns == LONG_MAX )
                    dwConns = TCP_MAX_CONNS;
            }
            oCfg.SetIntProp( propMaxConns, dwConns );        

            if( !( elem.isMember( JSON_ATTR_PARAMETERS ) &&
                elem[ JSON_ATTR_PARAMETERS ].isArray() ) &&
                elem[ JSON_ATTR_PARAMETERS ].size() > 0 )
            {
                ret = -ENOENT;
                break;
            }

            Json::Value& arrParams =
                elem[ JSON_ATTR_PARAMETERS ];

            ObjVecPtr pParams( true );
            CStlObjVector::MyType& vecParams = ( *pParams )();
            for( guint32 j = 0; j < arrParams.size(); j++ )
            {
                CCfgOpener oElemCfg;
                Json::Value& oParams = arrParams[ j ];

                bool bEnabled = true;
                // protocol, refer to propProtocol for detail
                if( oParams.isMember( JSON_ATTR_ENABLED ) &&
                    oParams[ JSON_ATTR_ENABLED ].isString() )
                {
                    string strEnabled =
                        oParams[ JSON_ATTR_ENABLED ].asString();
                    if( strEnabled == "false" )
                    {
                        // disabled
                        bEnabled = false;
                    }
                    else if( strEnabled != "true" )
                    {
                        // invalid value
                        bEnabled = false;
                    }
                }

                if( !bEnabled )
                    continue;

                // protocol, refer to propProtocol for detail
                if( oParams.isMember( JSON_ATTR_PROTOCOL ) &&
                    oParams[ JSON_ATTR_PROTOCOL ].isString() )
                {
                    string strProto =
                        oParams[ JSON_ATTR_PROTOCOL ].asString();
                    oElemCfg.SetStrProp( propProtocol, strProto );
                }
                // tcp port
                guint32 dwPort = 0xFFFFFFFF;
                if( oParams.isMember( JSON_ATTR_TCPPORT ) &&
                    oParams[ JSON_ATTR_TCPPORT ].isString() )
                {
                    string strPort =
                        oParams[ JSON_ATTR_TCPPORT ].asString();
                    dwPort = std::stol( strPort );
                    if( dwPort < 1024 || dwPort >= 0x10000 )
                    {
                        oElemCfg.RemoveProperty( propProtocol );
                        ret = -ENOENT;
                        break;
                    }
                    oElemCfg.SetIntProp( propDestTcpPort, dwPort );
                }
                else
                {
                    dwPort = RPC_SVR_DEFAULT_PORTNUM;
                    oElemCfg.SetIntProp( propDestTcpPort, dwPort );
                }

                // address to listen on
                if( oParams.isMember( JSON_ATTR_BINDADDR ) &&
                    oParams[ JSON_ATTR_BINDADDR ].isString() )
                {
                    string strAddr =
                        oParams[ JSON_ATTR_BINDADDR ].asString();

                    string strNormVal;
                    ret = NormalizeIpAddr(
                        AF_INET, strAddr, strNormVal );
                    if( ERROR( ret ) )
                    {
                        ret = NormalizeIpAddr(
                            AF_INET6, strAddr, strNormVal );
                    }

                    oElemCfg.SetStrProp( propDestIpAddr, strNormVal );
                }

                // address format, for detail, refer to propAddrFormat
                if( oParams.isMember( JSON_ATTR_ADDRFORMAT ) &&
                    oParams[ JSON_ATTR_ADDRFORMAT ].isString() )
                {
                    string strFormat =
                        oParams[ JSON_ATTR_ADDRFORMAT ].asString();
                    oElemCfg.SetStrProp( propAddrFormat, strFormat );
                }

                if( oParams.isMember( JSON_ATTR_PDOCLASS ) &&
                    oParams[ JSON_ATTR_PDOCLASS ].isString() )
                {
                    string strClass =
                        oParams[ JSON_ATTR_PDOCLASS ].asString();
                    oElemCfg.SetStrProp( propChildPdoClass, strClass );
                }

                oElemCfg.SetBoolProp( propEnableWebSock, false );
                oElemCfg.SetBoolProp( propEnableSSL, false );
                oElemCfg.SetBoolProp( propCompress, false );
                oElemCfg.SetBoolProp( propConnRecover, false );
                oElemCfg.SetBoolProp( propIsServer, true );

                if( oParams.isMember( JSON_ATTR_ENABLE_SSL ) &&
                    oParams[ JSON_ATTR_ENABLE_SSL ].isString() )
                {
                    string strVal =
                        oParams[ JSON_ATTR_ENABLE_SSL ].asString();
                    if( strVal == "true" )
                    {
                        oElemCfg.SetBoolProp( propEnableSSL, true );
                    }
                }
                if( oParams.isMember( JSON_ATTR_ENABLE_WEBSOCKET ) &&
                    oParams[ JSON_ATTR_ENABLE_WEBSOCKET ].isString() )
                {
                    string strVal =
                        oParams[ JSON_ATTR_ENABLE_WEBSOCKET ].asString();
                    if( strVal == "true" )
                        oElemCfg.SetBoolProp( propEnableWebSock, true );
                }
                if( oParams.isMember( JSON_ATTR_ENABLE_COMPRESS ) &&
                    oParams[ JSON_ATTR_ENABLE_COMPRESS ].isString() )
                {
                    string strVal =
                        oParams[ JSON_ATTR_ENABLE_COMPRESS ].asString();
                    if( strVal == "true" )
                        oElemCfg.SetBoolProp( propCompress, true );
                }
                if( oParams.isMember( JSON_ATTR_CONN_RECOVER ) &&
                    oParams[ JSON_ATTR_CONN_RECOVER ].isString() )
                {
                    string strVal =
                        oParams[ JSON_ATTR_CONN_RECOVER ].asString();
                    if( strVal == "true" )
                        oElemCfg.SetBoolProp( propConnRecover, true );
                }

                if( oParams.isMember( JSON_ATTR_HASAUTH ) &&
                    oParams[ JSON_ATTR_HASAUTH ].isString() )
                {
                    string strVal =
                        oParams[ JSON_ATTR_HASAUTH ].asString();
                    if( strVal == "true" )
                        oElemCfg.SetBoolProp( propHasAuth, true );
                }

                vecParams.push_back( ObjPtr( oElemCfg.GetCfg() ) );
            }

            oCfg.SetObjPtr( propConnParams, ObjPtr( pParams ) );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBusDriver::Probe(
        IPort* pLowerPort,
        PortPtr& pNewPort,
        const IConfigDb* pConfig )
{
    gint32 ret = 0;

    do{
        // we don't have dynamic bus yet
        CfgPtr pCfg( true );

        if( pConfig != nullptr )
            *pCfg = *pConfig;

        GetTcpSettings( pCfg );
        CCfgOpener oCfg( ( IConfigDb* )pCfg );
        ret = oCfg.SetStrProp( propPortClass,
            PORT_CLASS_RPC_TCPBUS );

        if( ERROR( ret ) )
            break;

        ret = oCfg.SetIntProp( propClsid,
            clsid( CRpcTcpBusPort ) );

        if( ERROR( ret ) )
            break;

        ret = CreatePort( pNewPort, pCfg );
        if( ERROR( ret ) )
            break;

        if( pLowerPort != nullptr )
        {
            ret = pNewPort->AttachToPort(
                pLowerPort );
        }

        if( SUCCEEDED( ret ) )
        {
            CEventMapHelper< CPortDriver >
                oEvtHelper( this );
            oEvtHelper.BroadcastEvent(
                eventPnp,
                eventPortAttached,
                PortToHandle( pNewPort ),
                nullptr );
        }
    }while( 0 );

    return ret;
}

CTcpStreamPdo::CTcpStreamPdo(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CTcpStreamPdo ) );
    m_dwFlags &= ~PORTFLG_TYPE_MASK;
    m_dwFlags |= PORTFLG_TYPE_PDO;

    CCfgOpener oCfg( pCfg );
    gint32 ret = 0;

    do{
        ret = oCfg.GetPointer(
            propBusPortPtr, m_pBusPort );

        if( ERROR( ret ) )
            break;

        ret = Sem_Init(
            &m_semFireSync, 0, 0 );

    }while( 0 );

    if( ERROR( ret ) )
    {
        throw std::runtime_error(
            "Error in CTcpStreamPdo ctor" );
    }
}

CTcpStreamPdo::~CTcpStreamPdo()
{
    sem_destroy( &m_semFireSync );
}

gint32 GetPreStopStep(
    PIRP pIrp, guint32& dwStepNo )
{
    BufPtr pBuf;
    pIrp->GetCurCtx()->GetExtBuf( pBuf );
    if( pBuf.IsEmpty() || pBuf->empty() )
        return -EINVAL;

    PORT_START_STOP_EXT* psse =
        ( PORT_START_STOP_EXT* )pBuf->ptr();

    dwStepNo = *( guint32* )&psse[ 1 ];
    return 0;
}

gint32 SetPreStopStep(
    PIRP pIrp, guint32 dwStepNo )
{
    BufPtr pBuf;
    pIrp->GetCurCtx()->GetExtBuf( pBuf );
    if( pBuf.IsEmpty() || pBuf->empty() )
        return -EINVAL;

    PORT_START_STOP_EXT* psse =
        ( PORT_START_STOP_EXT* )pBuf->ptr();

    *( guint32* )&psse[ 1 ] = dwStepNo;
    return 0;
}

gint32 CTcpStreamPdo::PreStop(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CIoManager* pMgr = GetIoMgr();
        guint32 dwStepNo = 0;
        GetPreStopStep( pIrp, dwStepNo );
        while( dwStepNo == 0 )
        {
            SockPtr pSock;
            CStdRMutex oSockLock(
                m_pStmSock->GetLock() );

            EnumSockState iState =
                m_pStmSock->GetState();

            if( iState == sockStopped )
            {
                SetPreStopStep( pIrp, 1 );
                break;
            }

            pSock = m_pStmSock;
            if( pSock.IsEmpty() )
            {
                SetPreStopStep( pIrp, 1 );
                break;
            }

            if( pMgr->RunningOnMainThread() )
            {
                // we need to run on the mainloop
                m_pStmSock.Clear();
                oSockLock.Unlock();
                SetPreStopStep( pIrp, 1 );
                ret = pSock->OnEvent(
                    eventStop, 0, 0, nullptr );
            }
            else
            {
                TaskletPtr pTask;
                DEFER_CALL_NOSCHED(
                    pTask, ObjPtr( pMgr ),
                    &CIoManager::CompleteIrp,
                    pIrp );

                ret = pMgr->RescheduleTaskMainLoop( pTask );
                if( SUCCEEDED( ret ) )
                    ret = STATUS_PENDING;
            }
            break;
        }

        // don't move away from PreStop
        if( ret == STATUS_PENDING )
            ret = STATUS_MORE_PROCESS_NEEDED;

        if( ret == STATUS_MORE_PROCESS_NEEDED )
            break;

        GetPreStopStep( pIrp, dwStepNo );
        if( dwStepNo == 1 )
        {
            SetPreStopStep( pIrp, 2 );
            ret = super::PreStop( pIrp );
        }

        if( ret == STATUS_PENDING ||
            ret == STATUS_MORE_PROCESS_NEEDED )
            break;

        GetPreStopStep( pIrp, dwStepNo );
        if( dwStepNo == 2 )
        {
            CCfgOpenerObj oPortCfg( this );

            guint32 dwPortId = 0;
            oPortCfg.GetIntProp(
                propPortId, dwPortId );

            DebugPrint( ret,
                "CTcpStreamPdo PreStop, portid=%d", dwPortId );
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::PostStart(
    IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CParamList oParams;
        CCfgOpenerObj oPortCfg( this );
        ret = oParams.CopyProp(
            propConnParams, this );

        if( ERROR( ret ) )
            break;

        ret = oParams.CopyProp(
            propFd, this );

        oParams.SetPointer(
            propIoMgr, GetIoMgr() );

        oParams.SetObjPtr(
            propPortPtr, ObjPtr( this ) );

        ret = m_pStmSock.NewObj(
            clsid( CRpcStreamSock ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = m_pStmSock->OnEvent( eventStart,
            ( LONGWORD )pIrp, 0, nullptr );

    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::CancelFuncIrp(
    IRP* pIrp, bool bForce )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oPortLock( GetLock() );
        CRpcStreamSock* pSock = m_pStmSock;
        if( pSock == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = pSock->RemoveIrpFromMap( pIrp );

    }while( 0 );

    if( SUCCEEDED( ret ) )
    {
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        pCtx->SetStatus( ERROR_CANCEL );
        if( pIrp->MinorCmd() == IRP_MN_READ )
        {
            CfgPtr pCfg;
            ret = pCtx->GetReqAsCfg( pCfg );
            if( ERROR( ret ) )
                return ret;

            CCfgOpener oCfg(
                ( IConfigDb* )pCfg );

            gint32 iStreamId = 0;
            ret = oCfg.GetIntProp(
                0, ( guint32& )iStreamId );

            if( SUCCEEDED( ret ) )
            {
                // remove the callback on
                // stream iStreamId 
                pIrp->RemoveCallback();
            }
        }
        super::CancelFuncIrp( pIrp, bForce );
    }

    return ret;
}

gint32 CTcpStreamPdo::SubmitReadIrp(
    IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( pIrp->MinorCmd() != IRP_MN_READ )
        {
            ret = -ENOTSUP;
            break;
        }
        if( m_pStmSock.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }
        CRpcStreamSock* pSock = m_pStmSock;
        if( pSock == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        ret = pSock->HandleReadIrp( pIrp );

    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::SubmitWriteIrp(
    IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( pIrp->MinorCmd() != IRP_MN_WRITE )
        {
            ret = -ENOTSUP;
            break;
        }
        if( m_pStmSock.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }
        CRpcStreamSock* pSock = m_pStmSock;
        if( pSock == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        ret = pSock->HandleWriteIrp( pIrp );
        if( ERROR( ret ) )
            break;

        // don't check the return code here
        ret = CheckAndSend( pIrp, ret );

    }while( 0 );

    return ret;
}

bool CTcpStreamPdo::IsImmediateReq(
    IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return true;

    bool ret = false;
    do{
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        guint32 dwCtrlCode = pCtx->GetCtrlCode();
        if( dwCtrlCode == CTRLCODE_CLOSE_STREAM_LOCAL_PDO ||
            dwCtrlCode == CTRLCODE_OPEN_STREAM_LOCAL_PDO ||
            dwCtrlCode == CTRLCODE_GET_RMT_STMID || 
            dwCtrlCode == CTRLCODE_GET_LOCAL_STMID ||
            dwCtrlCode == CTRLCODE_REG_MATCH ||
            dwCtrlCode == CTRLCODE_UNREG_MATCH )
            ret = true;
    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::CheckAndSend(
    IRP* pIrp, gint32 ret )
{
    CRpcStreamSock* pSock = m_pStmSock;
    if( pSock == nullptr )
        return -EINVAL;
    do{
        if( ERROR( ret ) )
            break;

        ret = pSock->StartSend( pIrp );
        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        if( pCtx->GetMajorCmd() == IRP_MJ_FUNC &&
            pCtx->GetMinorCmd() == IRP_MN_WRITE )
            break;

        ret = CompleteIoctlIrp( pIrp );

    }while( 0 );

    if( ERROR( ret ) )
    {
        // we need to remove the irp from
        // the outgoing queue
        pSock->RemoveIrpFromMap( pIrp );
    }

    return ret;
}

gint32 CTcpStreamPdo::SubmitIoctlCmd(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( m_pStmSock.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }
        CRpcStreamSock* pSock = m_pStmSock;
        if( pSock == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = pSock->HandleIoctlIrp( pIrp );

        if( ERROR( ret ) )
            break;

        if( IsImmediateReq( pIrp ) )
            break;

        IrpCtxPtr pCtx = pIrp->GetCurCtx();
        if( pCtx->GetIoDirection() == IRP_DIR_IN )
            break;

        // start to send the queued buffer
        ret = CheckAndSend( pIrp, ret );

    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::CompleteReadIrp(
    IRP* pIrp )
{
    return 0;
}

gint32 CTcpStreamPdo::CompleteWriteIrp(
    IRP* pIrp )
{
    return 0;
}

gint32 CTcpStreamPdo::CompleteOpenStmIrp(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    IrpCtxPtr pCtx = pIrp->GetCurCtx();
    CRpcStreamSock* pSock = m_pStmSock;
    if( pSock == nullptr )
        return -EFAULT;

    ret = pCtx->GetStatus();
    if( ERROR( ret ) )
        return ret;

    do{
        CfgPtr pReqCfg;
        ret = pCtx->GetReqAsCfg( pReqCfg );
        if( ERROR( ret ) )
            break;

        CCfgOpener oReqCfg( ( IConfigDb* )pReqCfg );

        gint32 iReqStmId = 0;

        ret = oReqCfg.GetIntProp( 0,
            ( guint32& )iReqStmId );

        if( ERROR( ret ) )
            break;

        CfgPtr pCfg;
        ret = pCtx->GetRespAsCfg( pCfg );
        if( ERROR( ret ) )
            break;

        gint32 iRet = 0;
        CReqOpener oCfg( ( IConfigDb* )pCfg );

        do{
            ret = oCfg.GetIntProp(
                propReturnValue, ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            if( ERROR( iRet ) )
            {
                ret = iRet;
                break;
            }

            gint32 iLocalId = 0;
            ret = oCfg.GetIntProp(
                2, ( guint32& )iLocalId );

            if( ERROR( ret ) )
                break;

            if( iReqStmId != iLocalId )
            {
                ret = -EINVAL;
                break;
            }

            guint32 dwProtoId = 0;
            ret = oCfg.GetIntProp( 1,
                ( guint32& )dwProtoId );

            if( ERROR( ret ) )
                break;

            guint32 dwProtoIdReq = 0;
            ret = oReqCfg.GetIntProp( 1,
                ( guint32& )dwProtoIdReq );

            if( ERROR( ret ) )
                break;

            if( dwProtoIdReq != dwProtoId )
            {
                ret = -EINVAL;
                break;
            }

            gint32 iPeerId = 0;
            ret = oCfg.GetIntProp(
                0, ( guint32& )iPeerId );

            if( ERROR( ret ) )
                break;

            ret = pSock->AddStream( iLocalId,
                dwProtoId, iPeerId );

            if( ERROR( ret ) )
                break;

            // this is the returned parameter
            // oCfg.ClearParams();
            // swap the local and peer id
            // that is, local is at 0, and peer at 2
            oCfg.SwapProp( 0, 2 );

        }while( 0 );

        oCfg.SetIntProp(
            propReturnValue, ret );
        
    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::CompleteCloseStmIrp(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    IrpCtxPtr& pCtx = pIrp->GetCurCtx();
    CRpcStreamSock* pSock = m_pStmSock;
    if( pSock == nullptr )
        return -EFAULT;

    do{
        CfgPtr pReqCfg;
        ret = pCtx->GetReqAsCfg( pReqCfg );
        if( ERROR( ret ) )
            break;

        CCfgOpener oReqCfg( ( IConfigDb* )pReqCfg );

        gint32 iReqPeerId = 0;

        ret = oReqCfg.GetIntProp( 1,
            ( guint32& )iReqPeerId );

        if( ERROR( ret ) )
            break;

        CfgPtr pCfg;
        ret = pCtx->GetRespAsCfg( pCfg );
        if( ERROR( ret ) )
            break;

        gint32 iRet = 0;
        CReqOpener oCfg( ( IConfigDb* )pCfg );

        do{
            ret = oCfg.GetIntProp(
                propReturnValue, ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            if( ERROR( iRet ) )
            {
                ret = iRet;
            }

            gint32 iPeerId = 0;
            ret = oCfg.GetIntProp(
                0, ( guint32& )iPeerId );

            if( ERROR( ret ) )
                break;

            if( iReqPeerId != iPeerId )
            {
                ret = -EINVAL;
                break;
            }

            gint32 iLocalStmId = 0;
            ret = oReqCfg.GetIntProp( 0,
                ( guint32& )iLocalStmId );

            if( ERROR( ret ) )
                break;

            ret = pSock->RemoveStream(
                iLocalStmId );

        }while( 0 );

        oCfg.ClearParams();
        oCfg.SetIntProp(
            propReturnValue, ret );
        
    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::CompleteInvalStmIrp(
    IRP* pIrp )
{
    // successfully send the the notification
    return 0;
}

gint32 CTcpStreamPdo::CompleteIoctlIrp(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    IrpCtxPtr pCtx = pIrp->GetCurCtx();
    CRpcStreamSock* pSock = m_pStmSock;
    if( pSock == nullptr )
        return -EFAULT;

    EnumIoctlStat iState = reqStatInvalid;
    ret = GetIoctlReqState( pIrp, iState );
    if( ERROR( ret ) )
        return ret;

    guint32 dwCtrlCode = pIrp->CtrlCode();
    switch( dwCtrlCode )
    {
    case CTRLCODE_INVALID_STREAM_ID_PDO:
    case CTRLCODE_OPEN_STREAM_PDO:
    case CTRLCODE_CLOSE_STREAM_PDO:
    case CTRLCODE_RMTSVR_OFFLINE_PDO:
    case CTRLCODE_RMTMOD_OFFLINE_PDO:
        {
            guint32 dwIoDir =
                pCtx->GetIoDirection();

            if( iState == reqStatOut &&
                dwIoDir == IRP_DIR_INOUT )
            {
                ret = SetIoctlReqState(
                    pIrp, reqStatIn );

                if( ERROR( ret ) )
                    break;

                BufPtr pEmpty;

                pCtx->SetRespData( pEmpty );
                ret = pSock->QueueIrpForResp( pIrp );
                if( ERROR( ret ) )
                    break;

                if( !pCtx->m_pRespData.IsEmpty() &&
                    !pCtx->m_pRespData->empty() )
                {
                    // fantastic, the response has
                    // arrived
                    ret = pCtx->GetStatus();
                    iState = reqStatIn;
                }
                else
                {
                    // we need to continue waiting for
                    // the response
                    ret = STATUS_PENDING;
                }
            }
            else if( iState == reqStatOut &&
                dwIoDir == IRP_DIR_OUT )
            {
                // this should be a response
                SetIoctlReqState(
                    pIrp, reqStatDone );

                if( dwCtrlCode ==
                    CTRLCODE_INVALID_STREAM_ID_PDO )
                {
                    ret = CompleteInvalStmIrp( pIrp );
                }
                else if( dwCtrlCode ==
                    CTRLCODE_RMTMOD_OFFLINE_PDO ||
                    dwCtrlCode ==
                    CTRLCODE_RMTSVR_OFFLINE_PDO )
                {
                    // do nothing
                }
                else if( dwCtrlCode ==
                    CTRLCODE_OPEN_STREAM_PDO ||
                    dwCtrlCode ==
                    CTRLCODE_CLOSE_STREAM_PDO )
                {
                    // this is the server side
                    // response
                    //
                    // do nothing
                }
            }

            if( iState == reqStatIn )
            {
                SetIoctlReqState(
                    pIrp, reqStatDone );

                if( dwCtrlCode ==
                    CTRLCODE_OPEN_STREAM_PDO )
                {
                    // this is the proxy side
                    // received the response and
                    // about to complete
                    ret = CompleteOpenStmIrp( pIrp );
                }
                else if( dwCtrlCode ==
                    CTRLCODE_CLOSE_STREAM_PDO )
                {
                    ret = CompleteCloseStmIrp( pIrp );
                }
            }
            break;
        }
    case CTRLCODE_LISTENING:
        {
            ret = SetIoctlReqState(
                pIrp, reqStatDone );
            break;
        }
    default:
        {
            ret = -ENOTSUP;
        }
    }

    return ret;
}

gint32 CTcpStreamPdo::CompleteFuncIrp(
    IRP* pIrp )
{

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( pIrp->MajorCmd() != IRP_MJ_FUNC )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        ret = pCtx->GetStatus();
        if( ERROR( ret ) )
            break;

        switch( pCtx->GetMinorCmd() )
        {
        case IRP_MN_READ:
            {
                ret = CompleteReadIrp( pIrp );
                break;
            }
        case IRP_MN_WRITE:
            {
                ret = CompleteWriteIrp( pIrp );
                break;
            }
        case IRP_MN_IOCTL:
            {
                ret = CompleteIoctlIrp( pIrp );    
                break;
            }
        default:
            ret = -ENOTSUP;
            break;
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        IrpCtxPtr& pCtx =
            pIrp->GetCurCtx();

        if( !pIrp->IsIrpHolder() )
        {
            IrpCtxPtr& pCtxLower = pIrp->GetCurCtx();

            ret = pCtxLower->GetStatus();
            pCtx->SetStatus( ret );
            pIrp->PopCtxStack();
        }
        else
        {
            ret = pCtx->GetStatus();
        }
    }
    return ret;
}

gint32 CTcpStreamPdo::OnSubmitIrp(
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

        // let's process the func irps
        if( pIrp->MajorCmd() != IRP_MJ_FUNC )
        {
            ret = -EINVAL;
            break;
        }

        switch( pIrp->MinorCmd() )
        {
        case IRP_MN_READ:
            {
                // the parameter is a IConfigDb*
                // and contains
                // 0: stream id
                ret = SubmitReadIrp( pIrp );
                break;
            }
        case IRP_MN_WRITE:
            {
                // the parameter is a IConfigDb*
                // and contains
                // 0: stream id
                // 1: the payload buffer
                ret = SubmitWriteIrp( pIrp );
                break;
            }
        case IRP_MN_IOCTL:
            {
                switch( pIrp->CtrlCode() )
                {
                case CTRLCODE_INVALID_STREAM_ID_PDO:
                case CTRLCODE_OPEN_STREAM_PDO:
                case CTRLCODE_CLOSE_STREAM_PDO:
                case CTRLCODE_RMTSVR_OFFLINE_PDO:
                case CTRLCODE_RMTMOD_OFFLINE_PDO:
                case CTRLCODE_LISTENING:
                case CTRLCODE_REG_MATCH:
                case CTRLCODE_UNREG_MATCH:
                case CTRLCODE_OPEN_STREAM_LOCAL_PDO:
                case CTRLCODE_CLOSE_STREAM_LOCAL_PDO:
                case CTRLCODE_GET_LOCAL_STMID:
                case CTRLCODE_GET_RMT_STMID:
                    {
                        ret = SubmitIoctlCmd( pIrp );
                        break;
                    }
                default:
                    {
                        ret = PassUnknownIrp( pIrp );
                        break;
                    }
                }
                break;
            }
        default:
            {
                ret = PassUnknownIrp( pIrp );
                break;
            }
        }
    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        pIrp->GetCurCtx()->SetStatus( ret );
    }

    return ret;
}

gint32 CTcpStreamPdo::OnPortReady(
    IRP* pIrp )
{
    gint32 ret = 0;
    do{
        CCfgOpenerObj oPortCfg( this );

        guint32 dwPortId = 0;
        ret = oPortCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        IConfigDb* pcp = nullptr;
        ret = oPortCfg.GetPointer(
            propConnParams, pcp );
        
        if( ERROR( ret ) )
            break;

        CConnParams oConnParams( pcp );
        if( m_pBusPort == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CRpcTcpBusPort* pBus =
            ObjPtr( m_pBusPort );

        if( pBus == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        // at this moment the connection
        // information is complete, especially for
        // client side pdo.
        ret = pBus->BindPortIdAndAddr(
            dwPortId, oConnParams );

    }while( 0 );

    if( ERROR( ret ) )
        return ret;

    return super::OnPortReady( pIrp );
}

gint32 FireRmtSvrEvent(
    IPort* pPort, EnumEventId iEvent )
{
    // this is an event detected by the
    // socket
    gint32 ret = 0;

    switch( iEvent )
    {
    case eventRmtSvrOffline:
    case eventRmtSvrOnline:
        {
            CCfgOpenerObj oCfg( pPort );
            CPort* pcPort = static_cast
                < CPort* >( pPort );

            guint32 dwPortId = 0;
            ret = oCfg.GetIntProp( propPdoId,
                dwPortId );

            CCfgOpener oEvtCtx;
            oEvtCtx[ propRouterPath ] =
                std::string( "/" );

            oEvtCtx[ propConnHandle ] =
                dwPortId;

            IConfigDb* pEvtExt =
                oEvtCtx.GetCfg();

            // pass on this event to the pnp
            // manager
            CEventMapHelper< CPort >
                oEvtHelper( pcPort );

            HANDLE hPort =
                PortToHandle( pPort );

            oEvtHelper.BroadcastEvent(
                eventConnPoint, iEvent,
                ( LONGWORD )pEvtExt,
                ( LONGWORD* )hPort );

            break;
        }
    default:
        {
            ret = -EINVAL;
            break;
        }
    }
    return ret;
}

// main entry for socket event handler
gint32 CTcpStreamPdo::OnEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData )
{
    gint32 ret = 0;
    switch( iEvent )
    {
    case eventDisconn:
        {
            // passive disconnection detected
            CStdRMutex oPortLock( GetLock() );
            if( m_bSvrOnline == true )
            {
                oPortLock.Unlock();
                Sem_Wait( &m_semFireSync );
                ret = FireRmtSvrEvent(
                    this, eventRmtSvrOffline );
            }
            break;
        }
    default:
        ret = -ENOTSUP;
        break;
    }

    return ret;
}

gint32 CTcpStreamPdo::AllocIrpCtxExt(
    IrpCtxPtr& pIrpCtx,
    void* pContext ) const
{
    gint32 ret = 0;
    switch( pIrpCtx->GetMajorCmd() )
    {
    case IRP_MJ_FUNC:
        {
            switch( pIrpCtx->GetMinorCmd() )
            {
            case IRP_MN_IOCTL:
                {
                    STMPDO_IRP_EXT oExt; 
                    guint32 dwCtrlCode =
                        pIrpCtx->GetCtrlCode();

                    switch( dwCtrlCode )
                    {
                    case CTRLCODE_OPEN_STREAM_PDO:
                    case CTRLCODE_CLOSE_STREAM_PDO:
                    case CTRLCODE_INVALID_STREAM_ID_PDO:
                        {
                            oExt.m_iState = reqStatOut;
                            break;
                        }
                    case CTRLCODE_LISTENING:
                        {
                            oExt.m_iState = reqStatIn;
                            break;
                        }
                    default:
                        ret = -ENOTSUP;
                        break;
                    }

                    if( ERROR( ret ) )
                        break;

                    BufPtr pBuf( true );
                    *pBuf = oExt;
                    pIrpCtx->SetExtBuf( pBuf );
                    break;
                }
            default:
                ret = -ENOTSUP;
                break;
            }
            break;
        }
    case IRP_MJ_PNP:
        {
            switch( pIrpCtx->GetMinorCmd() )
            {
            case IRP_MN_PNP_STOP:
                {
                    BufPtr pBuf( true );
                    if( ERROR( ret ) )
                        break;

                    pBuf->Resize( sizeof( guint32 ) +
                        sizeof( PORT_START_STOP_EXT ) );

                    memset( pBuf->ptr(),
                        0, pBuf->size() ); 

                    pIrpCtx->SetExtBuf( pBuf );
                    break;
                }
            default:
                ret = -ENOTSUP;
                break;
            }
            break;
        }
    default:
        {
            ret = -ENOTSUP;
            break;
        }
    }

    if( ret == -ENOTSUP )
    {
        // let's try it on CPort
        ret = super::AllocIrpCtxExt(
            pIrpCtx, pContext );
    }

    return ret;
}

gint32 CTcpStreamPdo::GetIoctlReqState(
    IRP* pIrp, EnumIoctlStat& iState )
{
    // please make sure the irp is locked
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    guint32 dwCtrlCode = pIrp->CtrlCode();

    switch( dwCtrlCode )
    {
    case CTRLCODE_CLOSE_STREAM_PDO:
    case CTRLCODE_OPEN_STREAM_PDO:
    case CTRLCODE_LISTENING:
    case CTRLCODE_INVALID_STREAM_ID_PDO:
        {
            IrpCtxPtr& pCtx =
                pIrp->GetCurCtx();
            BufPtr pBuf;
            pCtx->GetExtBuf( pBuf );
            if( pBuf.IsEmpty() || pBuf->empty() )
            {
                ret = -EFAULT;
                break;
            }
            STMPDO_IRP_EXT* pExt =
                ( STMPDO_IRP_EXT* )pBuf->ptr();
            iState = pExt->m_iState;
            break;
        }
    default:
        break;
    }

    return ret;
}

gint32 CTcpStreamPdo::SetIoctlReqState(
    IRP* pIrp, EnumIoctlStat iState )
{
    // please make sure the irp is locked
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( iState <= reqStatInvalid ||
        iState >= reqStatMax )
        return -EINVAL;

    gint32 ret = 0;
    do{
        IrpCtxPtr& pCtx = pIrp->GetTopStack();
        if( pCtx->GetMajorCmd() != IRP_MJ_FUNC ||
            pCtx->GetMinorCmd() != IRP_MN_IOCTL )
        {
            ret = -EINVAL;
            break;
        }
        switch( pCtx->GetCtrlCode() )
        {
        case CTRLCODE_CLOSE_STREAM_PDO:
        case CTRLCODE_OPEN_STREAM_PDO:
        case CTRLCODE_LISTENING:
        case CTRLCODE_INVALID_STREAM_ID_PDO:
            {
                BufPtr pBuf;
                pCtx->GetExtBuf( pBuf );
                if( pBuf.IsEmpty() || pBuf->empty() )
                {
                    ret = -EFAULT;
                    break;
                }
                STMPDO_IRP_EXT* pExt =
                   ( STMPDO_IRP_EXT* )pBuf->ptr();
                pExt->m_iState = iState;
                break;
            }
        default:
            ret = -ENOTSUP;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CTcpStreamPdo::CancelStartIrp(
        IRP* pIrp, bool bForce )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( !m_pStmSock.IsEmpty() )
    {
        m_pStmSock->OnEvent( eventCancelTask,
            ERROR_CANCEL, 0, nullptr );
    }

    return super::CancelStartIrp( pIrp, bForce );
}

gint32 CStmPdoSvrOfflineNotifyTask::OnIrpComplete(
    gint32 iRet )
{

    gint32 ret = 0;
    do{
        CCfgOpenerObj oCfg( this );

        ObjPtr pObj;
        ret = oCfg.GetObjPtr(
            propIrpPtr, pObj );

        if( ERROR( ret ) )
            break;

        IrpPtr pIrp = pObj;
        if( pIrp.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        CIoManager* pMgr = nullptr;
        ret = oCfg.GetPointer( propIoMgr, pMgr );

        if( SUCCEEDED( ret ) )
        {
            CStdRMutex oIrpLock(
                pIrp->GetLock() );

            ret = pIrp->CanContinue(
                IRP_STATE_READY );

            if( ERROR( ret ) )
                break;

            IrpCtxPtr& pCtx =
                pIrp->GetTopStack();

            pCtx->SetStatus( iRet );
        }
        else
        {
            break;
        }
        // resume the completion of the irp
        pMgr->CompleteIrp( pIrp );

    }while( 0 );

    return ret;
}

gint32 CStmPdoSvrOfflineNotifyTask::Process(
    guint32 dwContext )
{
    gint32 ret = 0;
    switch( dwContext )
    {
    case 0:
    case eventTaskThrdCtx:
        {
            ret = SendNotify();
            break;
        }
    case eventIrpComp:
        {
            IrpPtr pIrp;
            gint32 iRet = 0;
            ret = GetIrpFromParams( pIrp );
            if( SUCCEEDED( ret ) )
            {
                iRet = pIrp->GetStatus();
            }
            ret = OnIrpComplete( iRet );
            break;
        }
    case eventTimeout:
        {
            ret = OnIrpComplete( -ETIMEDOUT );
        }
    default:
        {
            ret = -ENOTSUP;
        }
    }
    return SetError( ret );
}

gint32 CStmPdoSvrOfflineNotifyTask::SendNotify()
{
    // parameters: 
    //
    // propPortPtr: the port to send this request
    // to
    //
    // propIoMgr: the pointer to the io manager
    //
    gint32 ret = 0;
    do{
        CCfgOpener oCfg( ( IConfigDb* )m_pCtx );

        ObjPtr pObj;
        ret = oCfg.GetObjPtr(
            propPortPtr, pObj );

        if( ERROR( ret ) )
            break;

        IPort* pPort = pObj;
        CIoManager* pMgr = nullptr;
        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        CReqBuilder oParams;

        std::string strRtName;
        pMgr->GetRouterName( strRtName );

        oParams.SetIfName(
            DBUS_IF_NAME( IFNAME_TCP_BRIDGE ) );

        oParams.SetObjPath( DBUS_OBJ_PATH(
            strRtName, OBJNAME_TCP_BRIDGE ) );

        oParams.SetSender(
            DBUS_DESTINATION( strRtName ) );

        oParams.SetMethodName( BRIDGE_EVENT_SVROFF );

        oParams.SetIntProp( propCmdId,
            CTRLCODE_RMTSVR_OFFLINE_PDO );

        oParams.SetIntProp(
            propStreamId, TCP_CONN_DEFAULT_CMD );

        oParams.SetCallFlags( CF_ASYNC_CALL |
            DBUS_MESSAGE_TYPE_SIGNAL |
            CF_NON_DBUS );

        oParams.SetReturnValue( 0 );

        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pCtx = pIrp->GetTopStack(); 
        pCtx->SetMajorCmd( IRP_MJ_FUNC );
        pCtx->SetMinorCmd( IRP_MN_IOCTL );
        pCtx->SetCtrlCode( CTRLCODE_RMTSVR_OFFLINE_PDO );

        // NOTE: this request is one-way only, we
        // don't expect a response and we don't
        // set the callback and the timer.
        pCtx->SetIoDirection( IRP_DIR_OUT ); 

        BufPtr pBuf( true );
        pObj = oParams.GetCfg();
        *pBuf = pObj;

        pCtx->SetReqData( pBuf );
        pIrp->SetSyncCall( false );
        pIrp->SetCallback( this, 0 );

        // give it 20 seconds to send out the
        // notification
        pIrp->SetTimer( 20, pMgr );

        // NOTE: this irp will only go through the
        // stream pdo, and not walk through the
        // whole port stack
        ret = pMgr->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ret == STATUS_PENDING )
            break;

        pIrp->RemoveCallback();
        pIrp->RemoveTimer();

        // retry this task if not at the right time
        if( ret == -EAGAIN )
            ret = STATUS_MORE_PROCESS_NEEDED;

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        OnIrpComplete( ret );
    }

    return ret;
}

gint32 CTcpStreamPdo::OnQueryStop(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    // test if the sock is already stopped. it
    // could be a case if the socket is detected
    // to be disconnected
    EnumSockState iState =
        m_pStmSock->GetState();

    if( iState != sockStarted )
    {
        gint32 ret = ERROR_STATE;
        DebugPrint( ret,
            "OnQueryStop: The socket is not working and"
            " unable to send notification to peer..." );

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        pCtx->SetStatus( ret );
        return ret;
    }

    if( true )
    {
        // query stop has be done once
        CStdRMutex oPortLock( GetLock() );
        if( m_bStopReady )
            return 0;
        m_bStopReady = true;
    }

    gint32 ret = 0;
    // if the socket is still OK, the PreStop
    // should either be triggered from remote
    // server's OFFLINE notification, or a call
    // from the local PORT_STOP request
    CParamList oParams;
    ret = oParams.SetPointer(
        propIoMgr, GetIoMgr() );

    ret = oParams.SetObjPtr(
        propPortPtr, ObjPtr( this ) );

    ret = oParams.SetObjPtr(
        propIrpPtr, ObjPtr( pIrp ) );

    ret = GetIoMgr()->ScheduleTask(
        clsid( CStmPdoSvrOfflineNotifyTask ),
        oParams.GetCfg() );

    if( ERROR( ret ) )
    {
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        pCtx->SetStatus( ret );
    }
    else
    {
        ret = STATUS_PENDING;
    }

    return ret;
}

gint32 CTcpStreamPdo::OnPortStackReady(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( true )
        {
            CStdRMutex oPortLock( GetLock() );

            EnumSockState iState =
                m_pStmSock->GetState();

            if( iState != sockStarted )
            {
                ret = ERROR_PORT_STOPPED;
                break;
            }
            m_bSvrOnline = true;
        }

        FireRmtSvrEvent( this, eventRmtSvrOnline );
        // Note: using the semaphore to enforce
        // the online event to happen strictly
        // ahead of the offline event. It could
        // happen occationally offline before
        // online otherwise and causes memory leak
        // or unexpected behavior.
        Sem_Post( &m_semFireSync );

    }while( 0 );

    pIrp->GetCurCtx()->SetStatus( ret );

    return ret;
}

}
