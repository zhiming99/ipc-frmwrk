/*
 * =====================================================================================
 *
 *       Filename:  rpcroute.cpp
 *
 *    Description:  implementation of CRpcRouter
 *
 *        Version:  1.0
 *        Created:  07/11/2017 11:34:37 PM
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

#include "sha1.h"
#include "configdb.h"
#include "defines.h"
#include "proxy.h"
#include "rpcroute.h"
#include "dbusport.h"
#include "connhelp.h"
#include "jsondef.h"

namespace rpcf
{

using namespace std;

CRpcRouter::CRpcRouter(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    gint32 ret = 0;

    do{
        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }
        CCfgOpener oCfg( pCfg );

        ret = oCfg.GetPointer(
            propRouterPtr, m_pParent );

        if( ERROR( ret ) )
        {
            m_pParent = nullptr;
            ret = 0;
            break;
        }

        RemoveProperty( propRouterPtr );

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg(
            ret, "Error occurs in CRpcRouter's ctor" );
        throw runtime_error( strMsg );
    }
}

CRpcRouter::~CRpcRouter()
{
}

CRpcRouterManager* CRpcRouter::GetRouterMgr() const
{
    return static_cast< CRpcRouterManager* >
        ( m_pParent );
}

gint32 CRpcRouter::IsEqualConn(
    const IConfigDb* pConn1,
    const IConfigDb* pConn2 )
{
    gint32 ret = 0;
    if( pConn1 == nullptr || pConn2 == nullptr )
        return -EINVAL;
    do{
        CCfgOpener oCfg( pConn1 );

        ret = oCfg.IsEqualProp(
            propDestIpAddr, pConn2 );
        if( ERROR( ret ) )
            break;

        ret = oCfg.IsEqualProp(
            propDestTcpPort, pConn2 );
        if( ERROR( ret ) )
            break;

        ret = oCfg.IsEqualProp(
            propEnableWebSock, pConn2 );
        if( ERROR( ret ) )
            break;
            
        ret = oCfg.IsEqualProp(
            propEnableSSL, pConn2 );
        if( ERROR( ret ) )
            break;

        ret = oCfg.IsEqualProp(
            propConnRecover, pConn2 );
        if( ERROR( ret ) )
            break;

        bool bSepConns = false;
        CIoManager* pMgr = GetIoMgr();
        gint32 iRet = pMgr->GetCmdLineOpt(
            propSepConns, bSepConns );
        if( SUCCEEDED( iRet ) &&
            HasReqForwarder() &&
            bSepConns )
        {
            ret = oCfg.IsEqualProp(
                propSrcUniqName, pConn2 );
        }

    }while( 0 );
    if( ERROR( ret ) )
        ret = ERROR_FALSE;

    return ret;
}

gint32 CRpcRouter::GetBridgeProxy(
    const IConfigDb* pConnParams,
    InterfPtr& pIf )
{
    if( pConnParams == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CCfgOpener oCfg( pConnParams );
        CStdRMutex oRouterLock( GetLock() );
        if( m_mapPid2BdgeProxies.empty() )
        {
            ret = -ENOENT;
            break;
        }
        std::map< guint32, InterfPtr >
            oMap = m_mapPid2BdgeProxies;
        oRouterLock.Unlock();

        for( auto elem : oMap )
        {
            CRpcServices* pSvc = elem.second;
            PortPtr pPort = pSvc->GetPort();
            PortPtr pPdo;

            ret = ( ( CPort* )pPort )->
                GetPdoPort( pPdo );
            if( ERROR( ret ) )
                continue;

            ObjPtr pcp;
            CCfgOpenerObj oPortCfg(
                ( CObjBase* )pPdo );

            ret = oPortCfg.GetObjPtr(
                propConnParams, pcp );
            if( ERROR( ret ) )
                continue;

            ret = IsEqualConn(
                pConnParams, pcp );

            if( SUCCEEDED( ret ) )
            {
                pIf = elem.second;
                break;
            }
        }

        if( ERROR( ret ) )
            ret = -ENOENT;

    }while( 0 );

    return ret;
}

gint32 CRpcRouter::GetBridgeProxy(
    guint32 dwPortId,
    InterfPtr& pIf )
{
    gint32 ret = 0;

    do{
        if( dwPortId == 0 )
        {
            ret = -EINVAL;
            break;
        }

        std::map< guint32, InterfPtr >*
            pMap = &m_mapPid2BdgeProxies;
        
        CStdRMutex oRouterLock( GetLock() );
        if( pMap->empty() )
        {
            ret = -ENOENT;
            break;
        }
        std::map< guint32, InterfPtr >::iterator 
            itr = pMap->find( dwPortId );

        if( itr == pMap->end() )
        {
            ret = -ENOENT;
            break;
        }

        pIf = itr->second;

    }while( 0 );

    return ret;
}

gint32 CRpcRouter::GetNodeName(
    const std::string& strPath,
    std::string& strNode )
{
    gint32 ret = 0;
    if( strPath.empty() )
        return -EINVAL;

    do{
        if( strPath[ 0 ] != '/' )
        {
            ret = -EINVAL;
            break;
        }

        size_t pos1 =
            strPath.find_first_not_of( "/", 1 );

        if( pos1 == std::string::npos )
        {
            ret = -EINVAL;
            break;
        }

        size_t pos2 =
            strPath.find_first_of( "/", pos1 );

        if( pos2 == std::string::npos )
            pos2 = strPath.size();

        strNode =
            strPath.substr( pos1, pos2 - pos1 );

    }while( 0 );

    return ret;
}

gint32 CRpcRouter::GetPortId(
    HANDLE hPort, guint32 dwPortId ) const
{
    if( hPort == INVALID_HANDLE )
        return -EINVAL;

    gint32 ret = 0;
    do{
        PortPtr pPort;
        CIoManager* pMgr = GetIoMgr();
        ret = pMgr->GetPortPtr( hPort, pPort );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oPortCfg(
            ( CObjBase* )pPort );

        ret = oPortCfg.GetIntProp(
            propPdoId, dwPortId );

    }while( 0 );

    return ret;
}


gint32 CRpcRouter::AddBridgeProxy( 
    IGenericInterface* pIf )
{
    if( pIf == nullptr )
        return -EINVAL;

    if( !IsConnected() )
        return ERROR_STATE;

    gint32 ret = 0;
    do{
        std::map< guint32, InterfPtr >* pMap =
            &m_mapPid2BdgeProxies;
        
        guint32 dwPortId = 0;
        CCfgOpenerObj oCfg( pIf );

        ret = oCfg.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        CStdRMutex oRouterLock( GetLock() );
        std::map< guint32, InterfPtr >::iterator 
            itr = pMap->find( dwPortId );
        if( itr != pMap->end() )
        {
            ret = -EEXIST;
            break;
        }

        ( *pMap )[ dwPortId ] = InterfPtr( pIf );

    }while( 0 );

    return ret;
}

gint32 CRpcRouter::RemoveBridgeProxy( 
    IGenericInterface* pIf )
{
    if( pIf == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        std::map< guint32, InterfPtr >* pMap =
            &m_mapPid2BdgeProxies;
        
        guint32 dwPortId = 0;

        CCfgOpenerObj oCfg( pIf );
        ret = oCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        CStdRMutex oRouterLock( GetLock() );
        std::map< guint32, InterfPtr >::iterator 
            itr = pMap->find( dwPortId );

        if( itr == pMap->end() )
        {
            ret = -ENOENT;
            break;
        }
        pMap->erase( itr );

    }while( 0 );

    return ret;
}

gint32 CRpcRouter::OnPostStart(
    IEventSink* pContext )
{
    string strIfName;

    strIfName = CoGetIfNameFromIid(
        iid( IStream ), "p" );
    if( strIfName.empty() )
    {
        CoAddIidName( "IStream",
            iid( IStream ), "p" );
    }

    strIfName = CoGetIfNameFromIid(
        iid( IStream ), "s" );
    if( strIfName.empty() )
    {
        CoAddIidName( "IStream",
            iid( IStream ), "s" );
    }

    strIfName = CoGetIfNameFromIid(
        iid( IStreamMH ), "p" );
    if( strIfName.empty() )
    {
        CoAddIidName( "IStreamMH",
            iid( IStreamMH ), "p" );
    }

    strIfName = CoGetIfNameFromIid(
        iid( IStreamMH ), "s" );
    if( strIfName.empty() )
    {
        CoAddIidName( "IStreamMH",
            iid( IStreamMH ), "s" );
    }
    return 0;
}

gint32 CRpcRouter::OnPreStop(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    return OnPreStopLongWait( pCallback );
}

gint32 CRpcRouter::GetMatchToAdd(
    IMessageMatch* pMatch,
    bool bRemote,
    MatchPtr& pMatchAdd ) const
{
    gint32 ret = 0;
    CRouterRemoteMatch* prlm = nullptr;
    MatchPtr ptrMatch;
    do{
        if( bRemote )
        {
            ret = ptrMatch.NewObj(
                clsid( CRouterRemoteMatch ) );
        }
        else
        {
            ret = ptrMatch.NewObj(
                clsid( CRouterLocalMatch ) );
        }

        if( ERROR( ret ) )
            break;

        prlm = ptrMatch;
        ret = prlm->CopyMatch( pMatch );

        if( ERROR( ret ) )
            break;

        pMatchAdd = ptrMatch;

    }while( 0 );

    return ret;
}

gint32 CRpcRouter::AddRemoveMatch(
    IMessageMatch* pMatch, bool bAdd,
    map< MatchPtr, gint32 >* plm )
{
    if( pMatch == nullptr ||
        plm == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    MatchPtr ptrMatch( pMatch );

    do{
        bool bFound = false;
        map< MatchPtr, gint32 >::iterator
            itr = plm->find( ptrMatch );

        if( itr != plm->end() )
            bFound = true;

        if( bAdd )
        {
            if( !bFound )
            {
                ( *plm )[ ptrMatch ] = 1;
            }
            else
            {
                ++( itr->second );
                ret = EEXIST;
            }
        }
        else
        {
            if( bFound )
            {
                --( itr->second );
                if( itr->second <= 0 )
                {
                    plm->erase( itr );
                }
                else
                {
                    ret = EEXIST;
                }
            }
        }

    }while( 0 );

    return ret;
}

bool CRpcRouter::IsRfcEnabled() const
{
    CRpcRouterManager* pMgr =
        static_cast< CRpcRouterManager* >
            ( GetParent() );
    return pMgr->IsRfcEnabled();
}

CRpcRouterBridge::CRpcRouterBridge(
    const IConfigDb* pCfg ) :
    CAggInterfaceServer( pCfg ), super( pCfg )
{
}

gint32 CRpcRouterBridge::GetBridge(
    guint32 dwPortId,
    InterfPtr& pIf )
{
    gint32 ret = 0;

    do{
        std::map< guint32, InterfPtr >*
            pMap = &m_mapPortId2Bdge;
        
        CStdRMutex oRouterLock( GetLock() );
        std::map< guint32, InterfPtr >::iterator 
            itr = pMap->find( dwPortId );

        if( itr == pMap->end() )
        {
            ret = -ENOENT;
            break;
        }
        pIf = itr->second;

    }while( 0 );

    return ret;
}

CRedudantNodes::CRedudantNodes(
    const IConfigDb* pCfg )
{
    SetClassId( clsid( CRedudantNodes ) );
    gint32 ret = 0;
    do{
        if( pCfg == nullptr )
            break;

        CCfgOpener oCfg( pCfg );
        ret = oCfg.GetPointer(
            propParentPtr, m_pParent );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    if( ERROR( ret ) )
    {
        throw std::invalid_argument(
            "CRedudantNodes did not find parent"
            "pointer, cannotproperly" );
    }
}

gint32 CRedudantNodes::LoadLBInfo(
    Json::Value& oLBInfo )
{
    for( guint32 i = 0 ; i < oLBInfo.size(); i++ )
    {
        Json::Value& oElem = oLBInfo[ i ];
        if( oElem.empty() || !oElem.isObject() )
            continue;

        std::vector< std::string > vecGrpNames =
            oElem.getMemberNames();
        for( auto strName : vecGrpNames )
        {
            Json::Value& arrNodes = oElem[ strName ];
            if( arrNodes == Json::Value::null ||
                !arrNodes.isArray() ||
                arrNodes.empty() )
                continue;
            guint32 j = 0;
            std::deque< std::string > listNodes;
            for( ;j < arrNodes.size(); ++j )
            {
                listNodes.push_back(
                    arrNodes[ j ].asString() );
            }
            m_mapLBGrp[ strName ] = listNodes;
        }
    }
    return 0;
}

gint32 CRedudantNodes::GetNodesAvail(
    const std::string& strGrpName,
    std::vector< std::string >& vecNodes )
{
    ITER_LBGRPMAP itr =
        m_mapLBGrp.find( strGrpName );
    if(  itr == m_mapLBGrp.end() )
        return -ENOENT;
    std::deque< std::string >&
        listNodes = itr->second;

    if( listNodes.size() == 0 )
        return -ENOENT;

    for( auto elem : listNodes )
        vecNodes.push_back( elem );

    if( listNodes.size() > 1 ) 
    {
        // simple rotation
        auto last = listNodes.back();
        listNodes.pop_back();
        listNodes.push_front( last );
    }
    return STATUS_SUCCESS;
}

gint32 CRpcRouterBridge::BuildNodeMap()
{
    gint32 ret = 0;
    do{
        std::string strObjName =
            OBJNAME_ROUTER_BRIDGE_AUTH;

        if( !HasAuth() )
            strObjName = OBJNAME_ROUTER_BRIDGE;

        std::string strObjDesc;
        CCfgOpenerObj oRouterCfg( this );
        ret = oRouterCfg.GetStrProp(
            propObjDescPath, strObjDesc );
        if( ERROR( ret ) )
            break;

        CCfgOpener oCfg;

        // Load the object decription file in json
        Json::Value valObjDesc;
        ret = ReadJsonCfg(
            strObjDesc, valObjDesc );
        if( ERROR( ret ) )
            break;

        // get object array
        Json::Value& oObjArray =
            valObjDesc[ JSON_ATTR_OBJARR ];

        if( oObjArray == Json::Value::null ||
            !oObjArray.isArray() ||
            oObjArray.empty() )
        {
            ret = -ENOENT;
            break;
        }

        string strVal;
        guint32 i = 0;
        for( ; i < oObjArray.size(); i++ )
        {
            if( oObjArray[ i ].empty() ||
                !oObjArray[ i ].isObject() )
                continue;

            Json::Value& oObjElem = oObjArray[ i ];
            if( !oObjElem.isMember( JSON_ATTR_OBJNAME ) )
                continue;

            // find the section with `ObjName' is
            // strObjName
            if( oObjElem[ JSON_ATTR_OBJNAME ].asString()
                != strObjName )
                continue;

            // overwrite the global port class and port
            // id if PROXY_PORTCLASS and PROXY_PORTID
            // exist
            Json::Value& oNodesArray =
                oObjElem[ JSON_ATTR_NODES ];

            if( oNodesArray == Json::Value::null ||
                !oNodesArray.isArray() ||
                oNodesArray.empty() )
                break;

            // set the default parameters
            for( guint32 i = 0; i < oNodesArray.size(); i++ )
            {
                CCfgOpener oConnParams;

                Json::Value& oNodeElem = oNodesArray[ i ];

                std::string strNode;
                bool bEnabled = false;

                if( oNodeElem.isMember( JSON_ATTR_ENABLED ) &&
                    oNodeElem[ JSON_ATTR_ENABLED ].isString() )
                {
                    strVal =
                       oNodeElem[ JSON_ATTR_ENABLED ].asString(); 
                    if( strVal == "true" )
                        bEnabled = true;
                    else if( strVal != "false" )
                    {
                        ret = -EINVAL;
                        break;
                    }
                }

                if( !bEnabled )
                    continue;

                // mandatory attribute NodeName
                if( oNodeElem.isMember( JSON_ATTR_NODENAME ) &&
                    oNodeElem[ JSON_ATTR_NODENAME ].isString() )
                {
                    strNode =
                       oNodeElem[ JSON_ATTR_NODENAME ].asString(); 
                }
                else
                {
                    ret = -EINVAL;
                    break;
                }

                std::string strFormat = "ipv4";
                oConnParams[ propAddrFormat ] = strFormat;
                oConnParams[ propEnableSSL ] = false;
                oConnParams[ propEnableWebSock ] = false;
                oConnParams[ propCompress ] = true;
                oConnParams[ propConnRecover ] = false;

                if( oNodeElem.isMember( JSON_ATTR_ADDRFORMAT ) &&
                    oNodeElem[ JSON_ATTR_ADDRFORMAT ].isString() )
                {
                    strFormat =
                       oNodeElem[ JSON_ATTR_ADDRFORMAT ].asString(); 
                    oConnParams[ propAddrFormat ] = strFormat;
                }

                // get ipaddr
                if( oNodeElem.isMember( JSON_ATTR_IPADDR ) &&
                    oNodeElem[ JSON_ATTR_IPADDR ].isString() )
                {
                    strVal = oNodeElem[ JSON_ATTR_IPADDR ].asString(); 
                    string strNormVal;
                    if( strFormat == "ipv4" )
                        ret = NormalizeIpAddr(
                            AF_INET, strVal, strNormVal );
                    else
                        ret = NormalizeIpAddr(
                            AF_INET6, strVal, strNormVal );

                    if( SUCCEEDED( ret ) )
                    {
                        oConnParams[ propDestIpAddr ] = strNormVal;
                    }
                    else
                    {
                        ret = -EINVAL;
                        break;
                    }
                }

                // tcp port number for router setting
                guint32 dwPortNum = 0xFFFFFFFF;
                if( oNodeElem.isMember( JSON_ATTR_TCPPORT ) &&
                    oNodeElem[ JSON_ATTR_TCPPORT ].isString() )
                {
                    strVal = oNodeElem[ JSON_ATTR_TCPPORT ].asString(); 
                    guint32 dwVal = 0;
                    if( !strVal.empty() )
                    {
                        dwVal = std::strtol(
                            strVal.c_str(), nullptr, 10 );
                    }
                    if( dwVal > 0 && dwVal < 0x10000 )
                    {
                        dwPortNum = dwVal;
                    }
                    else
                    {
                        ret = -EINVAL;
                        break;
                    }
                }

                if( dwPortNum == 0xFFFFFFFF )
                    dwPortNum = RPC_SVR_DEFAULT_PORTNUM;

                oConnParams[ propDestTcpPort ] = dwPortNum;

                if( oNodeElem.isMember( JSON_ATTR_ENABLE_SSL ) &&
                    oNodeElem[ JSON_ATTR_ENABLE_SSL ].isString() )
                {
                    strVal = oNodeElem[ JSON_ATTR_ENABLE_SSL  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propEnableSSL ] = false;
                    else if( strVal == "true" )
                        oConnParams[ propEnableSSL ] = true;
                }

                if( oNodeElem.isMember( JSON_ATTR_ENABLE_WEBSOCKET ) &&
                    oNodeElem[ JSON_ATTR_ENABLE_WEBSOCKET ].isString() )
                {
                    strVal = oNodeElem[ JSON_ATTR_ENABLE_WEBSOCKET  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propEnableWebSock ] = false;
                    else if( strVal == "true" )
                        oConnParams[ propEnableWebSock ] = true;
                }

                if( oNodeElem.isMember( JSON_ATTR_ENABLE_COMPRESS ) &&
                    oNodeElem[ JSON_ATTR_ENABLE_COMPRESS ].isString() )
                {
                    strVal = oNodeElem[ JSON_ATTR_ENABLE_COMPRESS  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propCompress ] = false;
                    else if( strVal == "true" )
                        oConnParams[ propCompress ] = true;
                }

                if( oNodeElem.isMember( JSON_ATTR_CONN_RECOVER ) &&
                    oNodeElem[ JSON_ATTR_CONN_RECOVER ].isString() )
                {
                    strVal = oNodeElem[ JSON_ATTR_CONN_RECOVER  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propConnRecover ] = false;
                    else if( strVal == "true" )
                        oConnParams[ propConnRecover ] = true;
                }

                if( oNodeElem.isMember( JSON_ATTR_DEST_URL ) &&
                    oNodeElem[ JSON_ATTR_DEST_URL ].isString() )
                {
                    strVal = oNodeElem[ JSON_ATTR_DEST_URL  ].asString(); 
                    oConnParams[ propDestUrl ] = strVal;
                }

                ret = AddConnParamsByNodeName(
                    strNode, oConnParams.GetCfg() );

                if( ERROR( ret ) )
                    break;

            }

            Json::Value& oLBInfo =
                oObjElem[ JSON_ATTR_LBGROUP ];

            if( oLBInfo == Json::Value::null ||
                !oLBInfo.isArray() ||
                oLBInfo.empty() )
                break;

            if( m_pLBGrp.IsEmpty() )
            {
                CParamList oParams;
                oParams[ propParentPtr ] = ObjPtr( this );
                ret = m_pLBGrp.NewObj(
                    clsid( CRedudantNodes ),
                    oParams.GetCfg() );
                if( ERROR( ret ) )
                    break;
            }

            CRedudantNodes* pLBGrp =
                ( CRedudantNodes* )m_pLBGrp;

            ret = pLBGrp->LoadLBInfo( oLBInfo );

            break;
        }

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::GetLBNodes(
    const std::string& strGrpName,
    std::vector< std::string >& vecNodes )
{
    if( strGrpName.empty() )
        return -EINVAL;

    CStdRMutex oRouterLock( GetLock() );

    CRedudantNodes* pLBGrp =
        ( CRedudantNodes* )m_pLBGrp;

    return pLBGrp->GetNodesAvail(
        strGrpName, vecNodes );

}

gint32 CRpcRouterBridge::OnPostStart(
    IEventSink* pContext )
{
    gint32 ret = super::OnPostStart( pContext );

    if( ERROR( ret ) )
        return ret;

    return BuildNodeMap();
}

gint32 CRpcRouterBridge::AddBridge( 
    IGenericInterface* pIf )
{
    if( pIf == nullptr )
        return -EINVAL;

    if( !IsConnected() )
        return ERROR_STATE;

    gint32 ret = 0;

    do{
        std::map< guint32, InterfPtr >* pMap =
            &m_mapPortId2Bdge;

        guint32 dwPortId = 0;
        CCfgOpenerObj oCfg( pIf );

        ret = oCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        CStdRMutex oRouterLock( GetLock() );
        std::map< guint32, InterfPtr >::iterator 
            itr = pMap->find( dwPortId );

        if( itr != pMap->end() )
        {
            ret = EEXIST;
            break;
        }

        ( *pMap )[ dwPortId ] = InterfPtr( pIf );

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::RemoveBridge( 
    IGenericInterface* pIf )
{
    if( pIf == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        std::map< guint32, InterfPtr >* pMap =
            &m_mapPortId2Bdge;

        guint32 dwPortId = 0;
        CCfgOpenerObj oCfg( pIf );

        ret = oCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        CStdRMutex oRouterLock( GetLock() );
        std::map< guint32, InterfPtr >::iterator 
            itr = pMap->find( dwPortId );
        if( itr == pMap->end() )
        {
            ret = -ENOENT;
            break;
        }
        pMap->erase( itr );

    }while( 0 );

    return ret;
}

#define ADD_STOPIF_TASK( _ret, _pTaskGrp, _pIf ) \
do{ \
    TaskletPtr pDeferTask; \
    _ret = DEFER_IFCALLEX_NOSCHED2( \
        0, pDeferTask, ObjPtr( ( _pIf ) ), \
        &CRpcServices::Shutdown, \
        ( IEventSink* )0 ); \
    if( ERROR( _ret ) ) \
        break; \
    _pTaskGrp->AppendTask( pDeferTask ); \
}while( 0 )

gint32 CRpcRouterBridge::OnPreStopLongWait(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    map< guint32, InterfPtr > mapId2Proxies;
    map< guint32, InterfPtr > mapId2Server;
    std::map< std::string, InterfPtr > mapReqProxies;

    // FIXME: need a task to accomodate all the StopEx
    // completion and then let the pCallback to run
    do{
        CStdRMutex oRouterLock( GetLock() );
        mapId2Proxies = m_mapPid2BdgeProxies;
        m_mapPid2BdgeProxies.clear();
        mapId2Server = m_mapPortId2Bdge;
        m_mapPortId2Bdge.clear();
        mapReqProxies = m_mapReqProxies;
        m_mapReqProxies.clear();

    }while( 0 );

    do{
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

        for( auto&& elem : mapId2Proxies )
        {
            ADD_STOPIF_TASK( ret,
                pTaskGrp, elem.second );
        }
        mapId2Proxies.clear();

        for( auto&& elem : mapId2Server )
        {
            ADD_STOPIF_TASK( ret,
                pTaskGrp, elem.second );
        }
        mapId2Server.clear(); 

        for( auto elem : mapReqProxies )
        {
            ADD_STOPIF_TASK( ret,
                pTaskGrp, elem.second );
        }
        mapReqProxies.clear();

        if( pTaskGrp->GetTaskCount() > 0 )
        {
            TaskletPtr pTask = pTaskGrp;
            ret = GetIoMgr()->
                RescheduleTask( pTask );
            if( SUCCEEDED( ret ) )
                ret = STATUS_PENDING;
        }

    }while( 0 );

    return ret;
}

gint32 CRouterStartReqFwdrProxyTask::
    OnTaskCompleteStart( gint32 iRetVal )
{
    gint32 ret = 0;
    do{
        if( ERROR( iRetVal ) )
            break;

        CRouterRemoteMatch* pMatch = nullptr;
        CParamList oTaskCfg(
            ( IConfigDb* )GetConfig() );

        ret = oTaskCfg.GetPointer( 1, pMatch );
        if( ERROR( ret ) )
            break;

        CRpcReqForwarderProxy* pIf = nullptr;
        ret = oTaskCfg.GetPointer( propIfPtr, pIf );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pIf->GetParent() );

        InterfPtr pIfPtr( pIf );
        ret = pRouter->AddProxy( pMatch, pIfPtr );
        if( ret == -EEXIST )
        {
            // some other guy has created this
            // interface. the one held by pIfPtr will
            // be released silentely.
            ret = pRouter->GetReqFwdrProxy(
                pMatch, pIfPtr );
            if( ERROR( ret ) )
                break;
        }
        else if( ERROR( ret ) )
        {
            DebugPrint( ret,
                "CRouterStartReqFwdrProxyTask bug "
                "cannot add newly created proxy!" );
            break;
        }

        ret = pIf->AddInterface( pMatch );
        if( ret == EEXIST )
        {
            // something wrong, don't move further
            pIf->RemoveInterface( pMatch );
            ret = -ret;
            break;
        }
        else if( ERROR( ret ) )
        {
            break;
        }

        // in case the transaction failed
        TaskletPtr pRbTask;
        ret = pRouter->BuildStartStopReqFwdrProxy(
            pMatch, false, pRbTask );

        if( SUCCEEDED( ret ) )
        {
            // it won't run if all the whole
            // tasks succeeds
            AddRollbackTask( pRbTask );
        }

    }while( 0 );

    return ret;
}

gint32 CRouterStartReqFwdrProxyTask::
    OnTaskComplete( gint32 iRetVal )
{
    CParamList oTaskCfg(
        ( IConfigDb* )GetConfig() );
    bool bStart = ( bool& )oTaskCfg[ 0 ];

    if( bStart )
        return OnTaskCompleteStart( iRetVal );

    oTaskCfg.ClearParams();
    oTaskCfg.RemoveProperty( propIfPtr );
    oTaskCfg.RemoveProperty( propRouterPtr );
    oTaskCfg.RemoveProperty( propMatchPtr );

    return iRetVal;
}

gint32 CRouterStartReqFwdrProxyTask::RunTask()
{
    gint32 ret = 0;
    do{
        CParamList oParams(
            ( IConfigDb* )GetConfig() );

        CRpcReqForwarderProxy* pIf = nullptr;
        ret = oParams.GetPointer( propIfPtr, pIf );
        if( ERROR( ret ) )
            break;

        IMessageMatch* pMatch = nullptr;
        bool bStart = ( bool& )oParams[ 0 ];

        ret = oParams.GetPointer( 1, pMatch );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pIf->GetParent() );

        InterfPtr pProxy;
        ret = pRouter->GetReqFwdrProxy(
            pMatch, pProxy );

        //already exist
        if( SUCCEEDED( ret ) && bStart )
        {
            ret = EEXIST;
            break;
        }
        else if( ERROR( ret ) && !bStart )
        {
            ret = -ENOENT;
            break;
        }

        if( bStart )
        {
            ret = pIf->StartEx( this );
        }
        else
        {
            CStdRMutex oIfLock( pIf->GetLock() );
            CStdRMutex oRouterLock(
                pRouter->GetLock() );

            pIf->RemoveInterface( pMatch );
            if( pIf->GetActiveIfCount() == 0 )
            {
                CRouterRemoteMatch*
                    pMatch = nullptr;

                ret = oParams.GetPointer(
                    1, pMatch );

                if( ERROR( ret ) )
                    break;

                InterfPtr pIfPtr( pIf );
                ret = pRouter->RemoveProxy(
                    pMatch, pIfPtr );

                oRouterLock.Unlock();
                oIfLock.Unlock();

                ret = pIf->SetStateOnEvent(
                    cmdShutdown );
                if( ERROR( ret ) )
                    break;

                ret = pIf->StopEx( this );
            }
            else
            {
                ret = 0;
            }
        }

    }while( 0 );

    if( SUCCEEDED( ret ) || ret == EEXIST )
        OnTaskComplete( ret );

    return ret;
}

gint32 CRpcRouterBridge::BuildStartStopReqFwdrProxy( 
    IMessageMatch* pMatch,
    bool bStart,
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    if( pMatch == nullptr )
        return -EINVAL;

    do{
        CParamList oIfParams;
        CIoManager* pMgr = GetIoMgr();
        oIfParams.SetPointer( propIoMgr, pMgr );

        CParamList oTaskCfg;

        InterfPtr pIf;
        CInterfaceProxy* pProxy = nullptr;
        // this call is probably to succeed, and
        // the task to build will check it again.
        ret = GetReqFwdrProxy( pMatch, pIf );
        if( !bStart )
        {
            if( ERROR( ret ) )
                break;
            oTaskCfg[ propIfPtr ] = ObjPtr( pIf );
        }
        else
        {
            if( SUCCEEDED( ret ) )
            {
                pProxy = pIf;
                if( pProxy == nullptr )
                {
                    ret = -EFAULT;
                    break;
                }

                if( !pProxy->IsConnected() &&
                    pProxy->GetState() != stateRecovery )
                {
                    ret = ERROR_STATE;
                    break;
                }
                // although the interface is
                // already started, we still need
                // to add the match to the
                // interface before the
                // EnableEvent can happen, so the
                // task still needs to run
                oTaskCfg[ propIfPtr ] = ObjPtr( pIf );
            }
            else
            {
                std::string strObjDesc;
                CCfgOpenerObj oRouterCfg( this );
                ret = oRouterCfg.GetStrProp(
                    propObjDescPath, strObjDesc );
                if( ERROR( ret ) )
                    break;

                ret = oIfParams.CopyProp(
                    propSvrInstName, this );
                if( ERROR( ret ) )
                    break;

                oIfParams.SetIntProp( propIfStateClass,
                    clsid( CIfReqFwdrPrxyState ) );

                oIfParams.SetPointer(
                    propIoMgr, pMgr );

                ret = CRpcServices::LoadObjDesc(
                    strObjDesc,
                    OBJNAME_REQFWDR,
                    false,
                    oIfParams.GetCfg() );

                if( ERROR( ret ) )
                    break;

                oIfParams.SetObjPtr(
                    propRouterPtr, ObjPtr( this ) );

                ret = oIfParams.CopyProp(
                    propObjPath, pMatch );

                if( ERROR( ret ) )
                    break;

                ret = oIfParams.CopyProp(
                    propDestDBusName, pMatch );

                if( ERROR( ret ) )
                    break;

                EnumClsid iClsid = clsid(
                    CRpcReqForwarderProxyImpl );

                std::string strAuthDest =
                    AUTH_DEST( this );

                ret = oIfParams.IsEqual(
                    propDestDBusName, strAuthDest );
                if( SUCCEEDED( ret ) )
                {
                    if( !HasAuth() )
                    {
                        ret = -EINVAL;
                        break;
                    }
                    iClsid = clsid(
                    CRpcReqForwarderProxyAuthImpl );
                }

                ret = pIf.NewObj( iClsid,
                    oIfParams.GetCfg() );

                if( ERROR( ret ) )
                    break;

                oTaskCfg[ propIfPtr ] = ObjPtr( pIf );
            }
        }

        oTaskCfg.SetPointer(
            propIoMgr, GetIoMgr() );

        oTaskCfg.Push( bStart );
        oTaskCfg.Push( ObjPtr( pMatch ) );

        ret = pTask.NewObj(
            clsid( CRouterStartReqFwdrProxyTask ),
            oTaskCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::AddToHistory(
    guint32 dwMaxReqs,
    guint32 dwMaxPendings )
{
    timespec tv;
    clock_gettime( CLOCK_MONOTONIC_COARSE, &tv );
    CStdRMutex oRouterLock( GetLock() );
    REQ_LIMIT rl ={ dwMaxReqs, dwMaxPendings };
    HISTORY_ELEM he( tv.tv_sec, rl );
    m_queHistory.push_back( he );
    while( m_queHistory.size() > RFC_HISTORY_LEN )
        m_queHistory.pop_front();

    return 0;
}

gint32 CRpcRouterBridge::GetCurConnLimit(
    guint32& dwMaxReqs,
    guint32& dwMaxPendings,
    bool bNew ) const
{
    gint32 ret = 0;
    do{
        CCfgOpenerObj oRouterCfg( this );

        ret = oRouterCfg.GetIntProp(
            propMaxReqs, dwMaxReqs );
        if( ERROR( ret ) )
            break;
        
        ret = oRouterCfg.GetIntProp(
            propMaxPendings, dwMaxPendings );
        if( ERROR( ret ) )
            break;

        guint32 dwCount = GetBridgeCount();

        if( bNew )
            dwCount += 1;

        if( dwCount > 0 )
        {
            dwMaxReqs = std::max( 1U,
                dwMaxReqs / dwCount );

            dwMaxPendings = std::max( 1U,
                dwMaxPendings / dwCount );

            dwMaxReqs = std::min(
                dwMaxReqs, RFC_MAX_REQS );

            dwMaxPendings = std::min(
                dwMaxPendings, RFC_MAX_PENDINGS );
        }
        else
        {
            dwMaxReqs = RFC_MAX_REQS;
            dwMaxPendings = RFC_MAX_PENDINGS;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::BuildRefreshReqLimit(
    CfgPtr& pRequest,
    guint32 dwMaxReqs,
    guint32 dwMaxPendings )
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
            SYS_EVENT_REFRESHREQLIMIT );

        oParams.SetIntProp( propStreamId,
            TCP_CONN_DEFAULT_STM );

        oParams.Push( dwMaxReqs );
        oParams.Push( dwMaxPendings );

        oParams.SetIntProp(
            propCmdId, CTRLCODE_SEND_EVENT );

        oParams.SetCallFlags( CF_ASYNC_CALL |
            DBUS_MESSAGE_TYPE_SIGNAL );

        oParams.SetIntProp(
            propTimeoutSec, 3 );

        pRequest = oParams.GetCfg();

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::RefreshReqLimit()
{
    guint32 ret = 0;
    do{
        guint32 dwMaxReqs = 0;
        guint32 dwMaxPendings = 0;
        CStdRMutex oRouterLock( GetLock() );

        GetCurConnLimit(
            dwMaxReqs, dwMaxPendings );

        HISTORY_ELEM& hl = m_queHistory[ 0 ];

        REQ_LIMIT& rl = hl.second;
        REQ_LIMIT rlNow =
            { dwMaxReqs, dwMaxPendings };

        AddToHistory( dwMaxReqs, dwMaxPendings );
        if( rl == rlNow )
            break;

        DebugPrint( 0, "RefreshReqLimit, "
            "dwMaxReqs=%d, dwMaxPendings=%d",
            dwMaxReqs, dwMaxPendings );

        std::vector< InterfPtr > vecBdges;
        for( auto elem : m_mapPortId2Bdge )
             vecBdges.push_back( elem.second );

        oRouterLock.Unlock();

        CReqBuilder oReq;
        BuildRefreshReqLimit( oReq.GetCfg(),
            dwMaxReqs, dwMaxPendings );

        CReqBuilder oAuthEvt;
        ret = oAuthEvt.Append( oReq.GetCfg() );
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

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );
        if( ERROR( ret ) )
            break;

        for( auto elem : vecBdges )
        {
            CRpcTcpBridge* pBridge = elem;
            if( !pBridge->IsConnected() )
                continue;

            bool bAuth = false;
            if( pBridge->GetClsid() ==
                clsid( CRpcTcpBridgeAuthImpl ) )
                bAuth = true;

            IConfigDb* pReq;
            if( bAuth )
                pReq = oAuthEvt.GetCfg();
            else
                pReq = oReq.GetCfg(); 

            pBridge->RefreshReqLimit(
                dwMaxReqs, dwMaxPendings );

            pBridge->BroadcastEvent(
                pReq, pDummyTask );
        }

    }while( 0 );

    return ret;
}

gint32 CRouterOpenBdgePortTask::CreateInterface(
    InterfPtr& pIf )
{
    gint32 ret = 0;
    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        CIoManager* pMgr = nullptr;
        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter = nullptr;
        ret = oCfg.GetPointer(
            propRouterPtr, pRouter );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oRouterCfg( pRouter );

        bool bServer = true;
        oCfg.GetBoolProp( 0, bServer );

        std::string strObjDesc;

        ret = oRouterCfg.GetStrProp(
            propObjDescPath, strObjDesc );
        if( ERROR( ret ) )
            break;

        CParamList oParams;
        string strRtName;

        ret = oParams.CopyProp(
            propConnParams, this );
        if( ERROR( ret ) )
            break;

        ret = oParams.CopyProp(
            propSvrInstName, pRouter );
        if( ERROR( ret ) )
            break;

        IConfigDb* pConnParams = nullptr;
        ret = oParams.GetPointer(
            propConnParams, pConnParams );
        if( ERROR( ret ) )
            break;

        CConnParams oConn( pConnParams );
        bool bAuth = false;
        if( bServer )
        {
            bAuth = ( pRouter->HasAuth() &&
                oConn.HasAuth() );
        }

        std::string strObjName;
        if( bAuth )
            strObjName = OBJNAME_TCP_BRIDGE_AUTH;
        else
            strObjName = OBJNAME_TCP_BRIDGE;

        oParams.SetPointer( propIoMgr, pMgr );

        ret = CRpcServices::LoadObjDesc(
            strObjDesc,
            strObjName,
            bServer ? true : false,
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        oParams.SetPointer(
            propRouterPtr, pRouter );

        EnumClsid iClsid = clsid( Invalid );

        if( bServer )
        {
            if( pRouter->IsRfcEnabled() )
            {
                guint32 dwMaxReqs = 0;
                guint32 dwMaxPendings = 0;

                pRouter->GetCurConnLimit(
                dwMaxReqs, dwMaxPendings, true );

                pRouter->AddToHistory(
                    dwMaxReqs, dwMaxPendings );

                oParams.SetIntProp(
                    propMaxReqs, dwMaxReqs );

                oParams.SetIntProp(
                    propMaxPendings, dwMaxPendings );
            }

            oParams.CopyProp( propPortId, this );
            oParams.SetIntProp( propIfStateClass,
                clsid( CIfTcpBridgeState ) );
            iClsid =
                clsid( CRpcTcpBridgeImpl );

            if( bAuth )
            {
                // copy the auth info from the
                // router object
                CCfgOpener oConn( pConnParams );
                oConn.MoveProp( propAuthInfo,
                ( IConfigDb* )oParams.GetCfg() );
                iClsid = clsid(
                    CRpcTcpBridgeAuthImpl );
            }
            ret = 0;
        }
        else
        {
            oParams.SetIntProp( propIfStateClass,
                clsid( CTcpBdgePrxyState ) );
            iClsid =
                clsid( CRpcTcpBridgeProxyImpl );
        }

        ret = pIf.NewObj(
            iClsid, oParams.GetCfg() );

    }while( 0 );

    return ret;
}

gint32 CRouterOpenBdgePortTask::AdvanceState()
{
    gint32 ret = 0;
    bool bServer = false;

    CCfgOpener oCfg(
        ( IConfigDb* )GetConfig() );

    oCfg.GetBoolProp( 0, bServer );

    switch( m_iState )
    {
    case stateInitialized:
        {
            if( bServer )
                m_iState = stateStartBridgeServer;
            else
                m_iState = stateStartBridgeProxy;
            break;
        }
    case stateStartBridgeServer:
    case stateStartBridgeProxy:
        {
            m_iState = stateDone;
            break;
        }
    default:
        {
            ret = ERROR_STATE;
            break;
        }
    }
    return ret;
}

gint32 CRouterOpenBdgePortTask::RunTaskInternal(
    gint32 iRetVal )
{
    gint32 ret = 0;
    CCfgOpener oCfg( ( IConfigDb* )GetConfig() );

    // five parameters needed:
    // propIoMgr
    // propIfPtr: the ptr to CRpcReqForwarder
    // propEventSink: the callback
    // propSrcDBusName: the source dbus
    // propConnParams: the destination ip address
    // propRouterPtr: the pointer to the router

    do{
        CRpcRouterBridge* pRouter = nullptr;
        ObjPtr pObj;
        ret = AdvanceState();
        if( ERROR( ret ) )
            break;

        bool bServer = false;
        oCfg.GetBoolProp( 0, bServer );

        ret = oCfg.GetPointer(
            propRouterPtr, pRouter );
        if( ERROR( ret ) )
            break;

        ObjPtr pConnParams;
        ret = oCfg.GetObjPtr(
            propConnParams, pConnParams );

        if( ERROR( ret ) )
            break;

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        CIoManager* pMgr =
            pRouter->GetIoMgr();

        if( m_iState == stateDone )
        {
            if( bServer )
            {
                if( SUCCEEDED( iRetVal ) ||
                    iRetVal == 0x7fffffff )
                {
                    ret = pRouter->
                        AddBridge( m_pServer ); 

                    CRpcServices* pSvr = m_pServer;
                    PortPtr pPort = pSvr->GetPort();

                    CCfgOpenerObj oIfCfg(
                        ( CObjBase* )m_pServer );

                    oIfCfg.CopyProp(
                        propPortId, propPdoId, 
                        ( CObjBase* )pPort );
                }
                else
                {
                    CGenericInterface* pIf = m_pServer;
                    if( unlikely( pIf == nullptr ) )
                        break;

                    EnumEventId iEvent = cmdShutdown;
                    EnumIfState iState = pIf->GetState();
                    if( iState == stateStartFailed )
                        iEvent = cmdCleanup;

                    ret = pIf->SetStateOnEvent(
                        iEvent );

                    if( ERROR( ret ) )
                        break;

                    // NOTE: Cannot call StopEx
                    // directly which may result
                    // in reentering this task,
                    // when ClearActiveTasks is
                    // trying to cancel the
                    // `start' taskgroup, whose
                    // callback finally hooks to
                    // this task to send back the
                    // response when the start is
                    // done.
                    DEFER_CALL( pMgr, ObjPtr( pIf ),
                        &CInterfaceServer::StopEx,
                        pDummyTask );

                    ret = iRetVal;
                }
            }
            else
            {
                ret = iRetVal;
                while( SUCCEEDED( ret ) ||
                    ret == 0x7fffffff )
                {
                    ret = pRouter->
                        AddBridgeProxy( m_pProxy ); 
                    
                    if( ERROR( ret ) )
                        break;

                    string strNode;
                    ret = oCfg.GetStrProp(
                        propNodeName, strNode );

                    if( ERROR( ret ) )
                        break;

                    CCfgOpenerObj oIfCfg(
                        ( CObjBase* )m_pProxy );

                    // the portid should be ready
                    // when the proxy is started
                    // successfully
                    guint32 dwProxyId = 0;
                    ret = oIfCfg.GetIntProp(
                        propPortId, dwProxyId );

                    if( ERROR( ret ) )
                        break;

                    oIfCfg.SetStrProp(
                        propNodeName, strNode );

                    // the bridge's portid
                    guint32 dwPortId = 0;
                    ret = oCfg.GetIntProp(
                        propPortId, dwPortId );

                    if( ERROR( ret ) )
                        break;

                    pRouter->AddToNodePidMap(
                        strNode, dwProxyId );

                    break;
                }

                if( ERROR( ret ) )
                {
                    CGenericInterface* pIf = m_pProxy;
                    if( unlikely( pIf == nullptr ) )
                        break;

                    EnumEventId iEvent = cmdShutdown;
                    EnumIfState iState = pIf->GetState();
                    if( iState == stateStartFailed )
                        iEvent = cmdCleanup;

                    ret = pIf->SetStateOnEvent(
                        iEvent );

                    if( ERROR( ret ) )
                        break;

                    DEFER_CALL( pMgr, ObjPtr( pIf ),
                        &CInterfaceProxy::StopEx,
                        pDummyTask );

                    ret = iRetVal;
                }
            }
        }
        else 
        {
            InterfPtr pBridgeIf;
            ret = CreateInterface( pBridgeIf );
            if( ERROR( ret ) )
            {
                if( !bServer )
                    break;

                HANDLE hPort = INVALID_HANDLE;
                gint32 iRet = oCfg.GetIntPtr(
                    1, ( guint32*& )hPort );
                if( ERROR( iRet ) )
                    break;

                pMgr->ClosePort( hPort, nullptr );
                break;
            }

            if( bServer )
                m_pServer = pBridgeIf;
            else
                m_pProxy = pBridgeIf;

            ret = pBridgeIf->StartEx( this );
            if( ret == STATUS_PENDING )
                break;

            iRetVal = ret;
            continue;
        }
        break;

    }while( 1 );

    if( ret != STATUS_PENDING )
    {
        m_pServer.Clear();
        m_pProxy.Clear();
    }

    return ret;
}

gint32 CRpcRouterBridge::OnRmtSvrEvent(
    EnumEventId iEvent,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    gint32 ret = 0;
    if( hPort == INVALID_HANDLE ||
        pEvtCtx == nullptr )
        return -EINVAL;

    TaskletPtr pDeferTask;
    switch( iEvent )
    {
    case eventRmtSvrOnline:
        {
            std::string strPath;
            CCfgOpener oEvtCtx( pEvtCtx );
            ret = oEvtCtx.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
                break;

            if( strPath == "/" )
            {
                PortPtr pPort;
                ret = GetIoMgr()->GetPortPtr(
                    hPort, pPort );

                if( ERROR( ret ) )
                    break;

                PortPtr pPdo;
                ret = ( ( CPort* )pPort )->
                    GetPdoPort( pPdo );

                if( ERROR( ret ) )
                    break;

                CCfgOpenerObj oPortCfg(
                    ( CObjBase* )pPdo );

                IConfigDb* pConnParams;
                ret = oPortCfg.GetPointer(
                    propConnParams, pConnParams );
                if( ERROR( ret ) )
                    break;

                CConnParams ocps( pConnParams );
                bool bServer = ocps.IsServer();

                // a bridge proxy is online, no
                // need to do more things.
                if( !bServer )
                    break;
            }

            ObjPtr pObj;
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pDeferTask,
                ObjPtr( this ),
                &CRpcRouterBridge::OnRmtSvrOnline,
                pObj, pEvtCtx, hPort );

            if( ERROR( ret ) )
                break;

            ret = AddSeqTask( pDeferTask, false );
            if( ERROR( ret ) )
                ( *pDeferTask )( eventCancelTask );

            break;
        }
    case eventRmtSvrOffline:
        {
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pDeferTask,
                ObjPtr( this ),
                &CRpcRouterBridge::OnRmtSvrOffline,
                nullptr, pEvtCtx, hPort );

            if( ERROR( ret ) )
                break;

            ret = AddSeqTask( pDeferTask, false );
            if( ERROR( ret ) )
                ( *pDeferTask )( eventCancelTask );

            break;
        }
    default:
        {
            ret = -ENOTSUP;
            break;
        }
    }

    return ret;
}

gint32 CRpcRouterBridge::OnRmtSvrOnline(
    IEventSink* pCallback,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( hPort == INVALID_HANDLE ||
        pEvtCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        InterfPtr pIf;
        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        CCfgOpener oEvtCtx( pEvtCtx );

        std::string strRouterPath;
        ret = oEvtCtx.GetStrProp(
            propRouterPath, strRouterPath );

        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        if( strRouterPath != "/" )
        {
            // rmtsvronline message should not be
            // forwarded.
            ret = -EINVAL;
            break;
        }

        guint32 dwPortId = 0;
        ret = oEvtCtx.GetIntProp(
            propConnHandle, dwPortId );

        if( ERROR( ret ) )
            break;

        // we need to sequentially GetBridge,
        // becasue it could be possible that two
        // OpenRemotePort requests for the same ip
        // address arrive at the same time.
        ret = GetBridge( dwPortId, pIf );
        if( SUCCEEDED( ret ) )
            break;

        if( ret == -ENOENT )
        {
            // passive bridge creation
            CParamList oParams;
            oParams[ propEventSink ] =
                ObjPtr( pCallback );

            PortPtr ptrPort;
            CIoManager* pMgr = GetIoMgr();
            ret = pMgr->GetPortPtr(
                hPort, ptrPort );
            if( ERROR( ret ) )
                break;

            CPort* pPort = ptrPort;
            PortPtr ptrPdo;
            ret = pPort->GetPdoPort( ptrPdo );
            if( ERROR( ret ) )
                break;

            pPort = ptrPdo;

            oParams.CopyProp( propConnParams, pPort );
            oParams.CopyProp( propPortId, pPort );
            oParams.SetPointer( propIoMgr, pMgr );

            oParams.SetObjPtr(
                propRouterPtr, ObjPtr( this ) );

            bool bServer = true;
            oParams.Push( bServer );
            oParams.Push( hPort );

            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CRouterOpenBdgePortTask  ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            ( *pTask )( eventZero );
            ret = pTask->GetError();
        }
        break;

    }while( 0 );

    return ret;
}

gint32 CRouterStopBridgeProxyTask::RunTask()
{
    gint32 ret = 0;    
    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        CRpcInterfaceProxy* pProxy = nullptr;
        ret = oCfg.GetPointer( 0, pProxy );
        if( ERROR( ret ) )
            break;

        CRpcRouter* pRouter =
            pProxy->GetParent();

        // remove the proxy to prevent further
        // reference, the ref count to the bridge
        // proxy will decrease in the reqfwdr's
        // OnRmtSvrOnline
        pRouter->RemoveBridgeProxy( pProxy );
        ret = pProxy->Shutdown( this );

    }while( 0 );

    if( ret != STATUS_PENDING )
        OnTaskComplete( ret );

    return ret;
}

gint32 CRouterStopBridgeProxyTask::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = 0;
    CParamList oTaskCfg(
        ( IConfigDb* )GetConfig() );

    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        CRpcInterfaceProxy* pProxy = nullptr;
        ret = oCfg.GetPointer( 0, pProxy );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        ret = oCfg.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        CRpcRouterReqFwdr* pRouter =
            static_cast< CRpcRouterReqFwdr* >
                ( pProxy->GetParent() );

        pRouter->RemoveLocalMatchByPortId(
            dwPortId );

        IConfigDb* pEvtCtx = nullptr;
        ret = oCfg.GetPointer( 1, pEvtCtx );
        if( ERROR( ret ) )
            break;

        HANDLE hPort =
            PortToHandle( pProxy->GetPort() );
        InterfPtr pIf;
        pRouter->GetReqFwdr( pIf );
        CRpcReqForwarder* pReqFwdr = pIf;
        pReqFwdr->OnEvent(
            eventRmtSvrOffline,
            ( LONGWORD )pEvtCtx,
            0, ( LONGWORD* )hPort );

    }while( 0 );

    oTaskCfg.ClearParams();
    oTaskCfg.RemoveProperty( propIfPtr );
    oTaskCfg.RemoveProperty( propRouterPtr );
    oTaskCfg.RemoveProperty( propMatchPtr );

    return iRetVal;
}

CIoManager* CRouterStopBridgeTask::GetIoMgr()
{
    CCfgOpenerObj oCfg( this );
    CIoManager* pMgr = nullptr;
    GET_IOMGR( oCfg, pMgr );
    return pMgr;
}

gint32 CRouterStopBridgeTask::SyncedTasks(
    IEventSink* pCallback,
    ObjVecPtr& pvecMatchsLoc )
{
    gint32 ret = 0;
    do{
        TaskletPtr pTask;
        ret = DEFER_OBJCALLEX_NOSCHED2(
            0, pTask, this,
            &CRouterStopBridgeTask::DoSyncedTask,
            nullptr, pvecMatchsLoc );
        if( ERROR( ret ) )
            break;

        CIfRetryTask* pRetry = pTask;
        pRetry->SetClientNotify( pCallback );

        CCfgOpenerObj oCfg( this );
        CRpcRouterBridge* pRouter = nullptr;
        ret = oCfg.GetPointer( propIfPtr, pRouter );
        if( ERROR( ret ) )
            break;

        ret = pRouter->AddSeqTask( pTask );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CRouterStopBridgeTask::DoSyncedTask(
    IEventSink* pCallback,
    ObjVecPtr& pvecMatchsLoc )
{
    gint32 ret = 0;
    TaskGrpPtr pTaskGrp;
    do{
        CCfgOpenerObj oCfg( this );
        CRpcTcpBridge* pBridge = nullptr;
        ret = oCfg.GetPointer( 0, pBridge );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        ret = oCfg.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pBridge->GetParent() );

        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( pRouter );

        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        pTaskGrp->SetRelation( logicNONE );
        pTaskGrp->SetClientNotify( pCallback );

        // put the bridge to the stopping state to
        // prevent further incoming requests
        ret = pBridge->SetStateOnEvent(
            cmdShutdown );
        if( ERROR( ret ) )
            break;

        // remove all the remote matches
        // registered via this bridge
        vector< std::string > vecNodes;
        std::vector< MatchPtr > vecMatches;

        CStdRMutex oRouterLock(
            pRouter->GetLock() );

        pRouter->RemoveBridge( pBridge );
        // pRouter->RemoveRemoteMatchByPortId(
        //     dwPortId, vecMatches );

        pRouter->ClearRefCountByPortId(
            dwPortId, vecNodes );

        oRouterLock.Unlock();

        std::vector< ObjPtr >& vecMatchesLoc =
            ( *pvecMatchsLoc )();

        TaskletPtr pDummy;
        pDummy.NewObj( clsid( CIfDummyTask ) );

        for( auto elem : vecMatchesLoc )
        {
            // the match belongs to
            // CRpcReqForwarderProxy 
            TaskletPtr pTask;
            ret = pRouter->BuildDisEvtTaskGrp(
                pDummy, elem, pTask );
            if( ERROR( ret ) )
                continue;
            pTaskGrp->AppendTask( pTask );
        }

        {
            TaskletPtr pTask;
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pTask, ObjPtr( pBridge ),
                &CRpcTcpBridge::StopEx,
                nullptr );
            if( ERROR( ret ) )
                break;
            pTaskGrp->AppendTask( pTask );
        }

        TaskletPtr pGrpTask = ObjPtr( pTaskGrp );
        CIoManager* pMgr = pRouter->GetIoMgr();

        ret = pMgr->RescheduleTask( pGrpTask );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;

    }while( 0 );

    if( ERROR( ret ) && !pTaskGrp.IsEmpty() )
        ( *pTaskGrp )( eventCancelTask );

    return ret;
}

gint32 CRouterStopBridgeTask::RunTask()
{
    gint32 ret = 0;    
    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        CRpcTcpBridge* pBridge = nullptr;
        ret = oCfg.GetPointer( 0, pBridge );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        ret = oCfg.GetIntProp(
            propPortId, dwPortId );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pBridge->GetParent() );

        // remove all the remote matches
        // registered via this bridge
        FWRDREQS vecReqs;
        ObjMapPtr pmapTaskIdsMH( true );
        ObjMapPtr pmapTaskIdsLoc( true );
        pBridge->GetAllFwrdReqs( vecReqs,
            pmapTaskIdsLoc, pmapTaskIdsMH );

        ObjVecPtr pvecTasks( true );
        for( auto elem : vecReqs )
            ( *pvecTasks )().push_back(
                ObjPtr( elem.first ) );

        if( ( *pvecTasks )().size() > 0 )
        {
            // cancel all the tasks through this
            // bridge
            pBridge->CancelInvTasks( pvecTasks );
        }

        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( pRouter );

        TaskGrpPtr pParaGrp;
        ret = pParaGrp.NewObj(
            clsid( CIfParallelTaskGrp ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        ObjVecPtr pvecMatches( true );
        std::vector< ObjPtr >& vecMatchesMH =
            ( *pvecMatches )();

        ObjVecPtr pvecMatchesLoc( true );
        std::vector< ObjPtr >& vecMatchesLoc =
            ( *pvecMatchesLoc )();

        vector< MatchPtr > vecMatches;
        pRouter->RemoveRemoteMatchByPortId(
            dwPortId, vecMatches );

        for( auto elem : vecMatches )
        {
            CCfgOpener oMatch(
                ( IConfigDb* )elem->GetCfg() );
            
            std::string strPath;
            ret = oMatch.GetStrProp(
                propRouterPath, strPath );
            if( ERROR( ret ) )
                continue;

            if( strPath == "/" )
            {
                vecMatchesLoc.push_back( elem );
            }
            else
            {
                vecMatchesMH.push_back( elem );
            }
        }

        if( vecMatchesMH.size() > 0 )
        {
            TaskletPtr pTask;
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pTask, ObjPtr( pRouter ),
                &CRpcRouterBridge::ClearRemoteEventsMH,
                nullptr, ObjPtr( pvecMatches ),
                ObjPtr( pmapTaskIdsMH ), true );

            if( SUCCEEDED( ret ) )
                pParaGrp->AppendTask( pTask );
        }

        if( ( *pmapTaskIdsLoc )().size() > 0 )
        {
            std::map< ObjPtr, QwVecPtr >&
            mapTaskIdsLoc = ( *pmapTaskIdsLoc )();

            for( auto elem : mapTaskIdsLoc )
            {
                InterfPtr pIf = elem.first;
                CRpcReqForwarderProxy* pProxy = pIf;
                if( pProxy == nullptr )
                    continue;

                TaskletPtr pTask;
                guint64 qwTaskId = 0;
                ret = DEFER_IFCALLEX_NOSCHED2(
                    2, pTask, ObjPtr( pProxy ),
                    &CRpcReqForwarderProxy::ForceCancelRequests,
                    ObjPtr( elem.second ),
                    qwTaskId, nullptr );

                if( ERROR( ret ) )
                    continue;
                pParaGrp->AppendTask( pTask );
            }
        }

        CIoManager* pMgr = pRouter->GetIoMgr();
        TaskletPtr pGrpTask = pParaGrp;
        if( pParaGrp->GetTaskCount() > 0 )
            pMgr->RescheduleTask( pGrpTask );

        ret = DoSyncedTask(
            this, pvecMatchesLoc );

    }while( 0 );

    if( ret != STATUS_PENDING )
        OnTaskComplete( ret );

    return ret;
}

gint32 CRouterStopBridgeTask::OnTaskComplete(
    gint32 iRetVal )
{
    CParamList oParams(
        ( IConfigDb* )GetConfig() );

    oParams.ClearParams();
    oParams.RemoveProperty( propIfPtr );
    oParams.RemoveProperty( propRouterPtr );
    oParams.RemoveProperty( propMatchPtr );

    return iRetVal;
}

gint32 CRouterStopBridgeProxyTask2::RunTask()
{
    // stop bridge proxy in the route bridge on
    // the event eventRmtSvrOffline
    gint32 ret = 0;    
    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        CRpcTcpBridgeProxy* pProxy = nullptr;
        ret = oCfg.GetPointer( 0, pProxy );
        if( ERROR( ret ) )
            break;

        std::string strNode;
        CCfgOpenerObj oIfCfg( pProxy );
        ret = oIfCfg.GetStrProp(
            propNodeName, strNode );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pProxy->GetParent() );

        // put the proxy to the stopping state to
        // prevent further incoming requests
        ret = pProxy->SetStateOnEvent(
            cmdShutdown );
        if( ERROR( ret ) )
            break;

        oCfg.SetStrProp(
            propNodeName, strNode );

        // remove all the remote matches
        // assocated with this bridge proxy
        vector< MatchPtr > vecMatches;
        std::set< guint32 > setBridges;

        CStdRMutex oRouterLock(
            pRouter->GetLock() );

        pRouter->RemoveRemoteMatchByNodeName(
            strNode, vecMatches );

        ObjSetPtr setIfs( true );
        pRouter->ClearRefCountByNodeName(
            strNode, setBridges );

        pRouter->RemoveFromNodePidMap(
            strNode  );

        InterfPtr pIf;
        for( auto elem : setBridges )
        {
            ret = pRouter->GetBridge( elem, pIf );
            if( SUCCEEDED( ret ) )
                ( *setIfs )().insert( pIf );
        }

        oCfg.SetObjPtr( propObjList,
            ObjPtr( setIfs ) );

        pRouter->RemoveBridgeProxy( pProxy );

        oRouterLock.Unlock();

        ret = pProxy->StopEx( this );
        if( ERROR( ret ) )
        {
            DebugPrint( ret, "StopBridge failed" );
            break;
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
        OnTaskComplete( ret );

    return ret;
}


gint32 CRouterStopBridgeProxyTask2::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = 0;
    do{
        // send out a proxy down event to all the
        // related bridges.
        ObjSetPtr setBridges;
        ObjPtr pObj;

        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );
        ret = oCfg.GetObjPtr(
            propObjList, pObj );
        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        guint32 dwPrxyPortId = 0;
        ret = oCfg.GetIntProp(
            propPortId, dwPrxyPortId );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxy* pProxy = nullptr;
        ret = oCfg.GetPointer( 0, pProxy );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pProxy->GetParent() );

        setBridges = pObj;
        if( setBridges.IsEmpty() )
            break;

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );
        if( ERROR( ret ) )
            break;

        std::string strNode;
        ret = oCfg.GetStrProp(
            propNodeName, strNode );
        if( ERROR( ret ) )
            break;

        CCfgOpener oEvtCtx;
        oEvtCtx[ propRouterPath ] =
            std::string( "/" ) + strNode;

        CfgPtr pReqCall;
        ret = pRouter->BuildRmtSvrEventMH(
            eventRmtSvrOffline,
            oEvtCtx.GetCfg(),
            pReqCall );

        CReqBuilder oAuthEvt;
        ret = oAuthEvt.Append( pReqCall );
        if( ERROR( ret ) )
            break;

        std::string strRtName;
        CCfgOpenerObj oRtCfg( pRouter );
        ret = oRtCfg.GetStrProp(
            propSvrInstName, strRtName );
        if( ERROR( ret ) )
            break;

        oAuthEvt.SetObjPath(
            DBUS_OBJ_PATH( strRtName,
            OBJNAME_TCP_BRIDGE_AUTH ) );

        for( auto elem : ( *setBridges )() )
        {
            CRpcTcpBridge* pBridge = elem;
            if( !pBridge->IsConnected() )
                continue;

            bool bAuth = false;
            if( pBridge->GetClsid() ==
                clsid( CRpcTcpBridgeAuthImpl ) )
                bAuth = true;

            IConfigDb* pReq = pReqCall; 
            if( bAuth )
                pReq = oAuthEvt.GetCfg();

            pBridge->BroadcastEvent(
                pReq, pDummyTask );

            CStreamServerRelayMH* pStmSvr = elem;
            if( pStmSvr == nullptr )
                continue;

            std::vector< HANDLE > vecHandles;
            ret = pStmSvr->GetStreamsByBridgeProxy(
                pProxy, vecHandles );
            if( ERROR( ret ) )
                continue;
            if( vecHandles.empty() )
                continue;

            for( auto elem : vecHandles )
                pStmSvr->OnClose( elem );

            FWRDREQS vecReqs;
            ret = pBridge->FindFwrdReqsByPrxyPortId(
                dwPrxyPortId, vecReqs );
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

    }while( 0 );

    RemoveProperty( propObjList );

    return ret;
}


gint32 CRpcRouterBridge::OnRmtSvrOffline(
    IEventSink* pCallback,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( hPort == INVALID_HANDLE ||
        pCallback == nullptr ||
        pEvtCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CIoManager* pMgr = GetIoMgr();
    TaskGrpPtr pTaskGrp;
    InterfPtr pIf;
    do{
        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        CCfgOpener oEvtCtx( pEvtCtx );

        std::string strPath;
        ret = oEvtCtx.GetStrProp(
            propRouterPath, strPath );

        if( ERROR( ret ) )
            break;

        if( strPath != "/" )
        {
            // run the tasks outside the seq task
            // queue
            this->OnRmtSvrOfflineMH(
                pCallback, pEvtCtx, hPort );
            break;
        }

        // a bridge or a bridge proxy
        // has lost the connection
        guint32 dwPortId = 0;
        ret = oEvtCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        bool bBridge = true;
        ret = GetBridge( dwPortId, pIf );
        if( ERROR( ret ) )
        {
            ret = GetBridgeProxy( dwPortId, pIf );
            if( ERROR( ret ) )
                break;
            bBridge = false;
        }

        TaskletPtr pStopTask;
        ret = DEFER_OBJCALLEX_NOSCHED2(
            2, pStopTask, pMgr,
            &CIoManager::ClosePort,
            hPort, nullptr, nullptr );
        if( ERROR( ret ) )
            break;

        CParamList oParams;
        oParams.SetPointer( propIfPtr, this );
        pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );
        
        pTaskGrp->SetClientNotify( pCallback );
        pTaskGrp->SetRelation( logicOR );
        pTaskGrp->AppendTask( pStopTask );

        oParams.Push( pIf );
        oParams[ propIfPtr ] = ObjPtr( this );

        oParams.SetIntProp(
            propPortId, dwPortId );

        oParams.Push( pEvtCtx );

        TaskletPtr pTask;

        if( bBridge )
        {
            // stop the bridge
            pTask.NewObj(
                clsid( CRouterStopBridgeTask ),
                oParams.GetCfg() );
            pTaskGrp->InsertTask( pTask );
        }
        else
        {
            ret = pTask.NewObj(
                clsid( CRouterStopBridgeProxyTask2  ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;
            pTaskGrp->InsertTask( pTask );
        }

        ret = ( *pTaskGrp )( eventZero );

    }while( 0 );

    if( ERROR( ret ) )
    {
        if( !pTaskGrp.IsEmpty() )
            ( *pTaskGrp )( eventCancelTask );
        // close the port if exists
        pMgr->ClosePort( hPort, nullptr );
    }

    return ret;
}

gint32 CRpcRouterBridge::OnModEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    if( strModule.empty() )
        return -EINVAL;

    if( strModule[ 0 ] == ':' )
    {
        // uniq name, not our call
        return 0;
    }
    return ForwardModOnOfflineEvent(
        iEvent, strModule );
}

gint32 CRpcRouterBridge::CheckEvtToFwrd(
    IConfigDb* pEvtCtx,
    DMsgPtr& pMsg,
    std::set< guint32 >& setPortIds )
{
    gint32 ret = ERROR_FALSE;
    MatchPtr pMatch;
    if( pMsg.IsEmpty() ||
        pEvtCtx == nullptr )
        return -EINVAL;

    CStdRMutex oRouterLock( GetLock() );
    for( auto&& oPair : m_mapRmtMatches )
    {
        CRouterRemoteMatch* pMatch =
            oPair.first;

        if( pMatch == nullptr )
            continue;

        ret = pMatch->IsMyEvtToForward(
            pEvtCtx, pMsg );

        if( SUCCEEDED( ret ) )
        {
            guint32 dwPortId = 0;
            CCfgOpenerObj oMatch( pMatch );
            ret = oMatch.GetIntProp(
                propPortId, dwPortId );
            if( ERROR( ret ) )
                continue;
            setPortIds.insert( dwPortId );
        }
    }

    return 0;
}

gint32 CRpcRouterBridge::CheckReqToRelay(
    IConfigDb* pReqCtx,
    DMsgPtr& pMsg,
    MatchPtr& pMatchHit )
{
    gint32 ret = ERROR_FALSE;
    MatchPtr pMatch;
    if( pMsg.IsEmpty() ||
        pReqCtx == nullptr )
        return -EINVAL;

    CStdRMutex oRouterLock( GetLock() );
    for( auto&& oPair : m_mapRmtMatches )
    {
        CRouterRemoteMatch* pMatch =
            oPair.first;

        if( pMatch == nullptr )
            continue;

        ret = pMatch->IsMyReqToRelay(
            pReqCtx, pMsg );

        if( SUCCEEDED( ret ) )
        {
            pMatchHit = pMatch;
            break;
        }
    }

    return ret;
}

gint32 CRpcRouterBridge::BuildEventRelayTask(
    IMessageMatch* pMatch,
    bool bAdd,
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    do{
        CParamList oParams;

        InterfPtr pIf;

        // this call could fail if the reqfwdr
        // proxy is not created yet. But the proxy
        // should be ready when the task starts to
        // run
        ret = GetReqFwdrProxy( pMatch, pIf );
        if( SUCCEEDED( ret ) )
        {
            oParams[ propIfPtr ] = ObjPtr( pIf );
        }
        else if( ERROR( ret ) && !bAdd )
        {
            // nothing to disable
            break;
        }

        CIoManager* pMgr = GetIoMgr();

        oParams.Push( bAdd );
        oParams.Push( ObjPtr( pMatch ) );

        oParams[ propRouterPtr ] = ObjPtr( this );
        oParams[ propIoMgr ] = ObjPtr( pMgr );

        ret = pTask.NewObj(
            clsid( CRouterEnableEventRelayTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::AddRemoteMatch(
    IMessageMatch* pMatch )
{
    MatchPtr ptrMatch;
    gint32 ret = GetMatchToAdd(
        pMatch, true, ptrMatch );

    if( ERROR( ret ) )
        return ret;

    CStdRMutex oRouterLock( GetLock() );
    return AddRemoveMatch(
        ptrMatch, true, &m_mapRmtMatches );
}

gint32 CRpcRouterBridge::RemoveRemoteMatch(
    IMessageMatch* pMatch )
{
    MatchPtr ptrMatch;
    gint32 ret = GetMatchToRemove(
        pMatch, true, ptrMatch );

    if( ERROR( ret ) )
        return ret;

    CStdRMutex oRouterLock( GetLock() );
    return AddRemoveMatch(
        ptrMatch, false, &m_mapRmtMatches );
}

gint32 CRpcRouterBridge::FindRemoteMatchByPortId(
    guint32 dwPortId,
    std::vector< MatchPtr >& vecMatches,
    bool bRemove )
{
    if( dwPortId == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStdRMutex oRouterLock( GetLock() );

        map< MatchPtr, gint32 >* plm =
            &m_mapRmtMatches;

        map< MatchPtr, gint32 >::iterator
            itr = plm->begin();
        while( itr != plm->end() )
        {
            IMessageMatch* pMatch = itr->first;
            CCfgOpener oMatch(
                ( IConfigDb* )pMatch->GetCfg() );

            guint32 dwVal = 0;
            ret = oMatch.GetIntProp(
                propPortId, dwVal );
            if( ERROR( ret ) )
            {
                ++itr;
                continue;
            }

            if( dwPortId == dwVal )
            {
                const MatchPtr& pMat = itr->first;
                vecMatches.push_back( pMat );
                if( bRemove )
                {
                    itr = plm->erase( itr );
                    continue;
                }
            }

            ++itr;
        }

    }while( 0 );

    return vecMatches.size();
}

gint32 CRpcRouterBridge::RemoveRemoteMatchByPortId(
    guint32 dwPortId,
    std::vector< MatchPtr >& vecMatches )
{
    return FindRemoteMatchByPortId(
        dwPortId, vecMatches, true );
}

gint32 CRpcRouterBridge::RemoveRemoteMatchByNodeName(
    const std::string& strNode,
    std::vector< MatchPtr >& vecMatches )
{
    if( strNode.empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        map< MatchPtr, gint32 >* plm =
            &m_mapRmtMatches;

        CStdRMutex oRouterLock( GetLock() );
        map< MatchPtr, gint32 >::iterator
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

            std::string strNode1;
            GetNodeName( strPath, strNode1 );
            if( strNode1 == strNode )
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

gint32 CRpcRouterBridge::BuildAddMatchTask(
    IMessageMatch* pMatch,
    bool bEnable,
    TaskletPtr& pTask )
{
    if( pMatch == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        // NOTE: we use the original match for
        // dbus listening
        CParamList oParams;

        oParams.Push( bEnable );
        oParams.Push( ObjPtr( pMatch ) );

        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        oParams.SetObjPtr(
            propIoMgr, GetIoMgr() );

        ret = pTask.NewObj(
            clsid( CRouterAddRemoteMatchTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::BuildStartRecvTask(
    IMessageMatch* pMatch,
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    if( pMatch == nullptr )
        return -EINVAL;

    do{
        CParamList oParams;

        ret = oParams.SetPointer(
            propIoMgr, GetIoMgr() );

        InterfPtr pIf;
        ret = GetReqFwdrProxy( pMatch, pIf );
        if( SUCCEEDED( ret ) )
        {
            ret = oParams.SetObjPtr(
                propIfPtr, pIf );
        }

        ret = oParams.SetPointer(
            propRouterPtr, this );

        ret = oParams.SetPointer(
            propMatchPtr, pMatch );

        ret = pTask.NewObj(
            clsid( CRouterStartRecvTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::RunEnableEventTask(
    IEventSink* pCallback,
    IMessageMatch* pMatch )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pMatch == nullptr )
        return -EINVAL;

    TaskletPtr pRespTask;
    TaskGrpPtr pTransGrp;

    do{
        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );

        oParams[ propEventSink ] =
            ObjPtr( pCallback );

        ret = pRespTask.NewObj(
            clsid( CRouterEventRelayRespTask ),
            oParams.GetCfg() );

        CRouterEventRelayRespTask* prerrt =
            ( CRouterEventRelayRespTask* )pRespTask;
        prerrt->SetEnable( true );   

        // run the task to set it to a proper
        // state
        ( *pRespTask )( eventZero );

        oParams[ propEventSink ] =
            ObjPtr( pRespTask );
        oParams[ propNotifyClient ] = true;

        ret = pTransGrp.NewObj(
            clsid( CIfTransactGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        TaskletPtr pAddMatchTask;
        ret = BuildAddMatchTask(
            pMatch, true, pAddMatchTask );
        if( ERROR( ret ) )
            break;

        TaskletPtr pStartProxyTask;
        ret = BuildStartStopReqFwdrProxy(
            pMatch, true, pStartProxyTask );
        if( ERROR( ret ) && ret != -EEXIST )
            break;

        TaskletPtr pEnableTask;
        ret = BuildEventRelayTask(
            pMatch, true, pEnableTask );
        if( ERROR( ret ) )
            break;

        TaskletPtr pRecvTask;
        ret = BuildStartRecvTask(
            pMatch, pRecvTask );

        if( ERROR( ret ) )
            break;

        pTransGrp->AppendTask( pAddMatchTask );
        pTransGrp->AppendTask( pStartProxyTask );
        pTransGrp->AppendTask( pEnableTask );
        pTransGrp->AppendTask( pRecvTask );

        TaskletPtr pGrpTask = pTransGrp;
        ret = AddSeqTask( pGrpTask, false );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;
        
    }while( 0 );

    if( ERROR( ret ) )
    {
        if( !pRespTask.IsEmpty() )
        {
            CIfInterceptTaskProxy* pIcpt = pRespTask;
            if( pIcpt != nullptr )
                pIcpt->SetInterceptTask( nullptr );
        }
        if( !pTransGrp.IsEmpty() )
        {
            ( *pTransGrp )( eventCancelTask );
        }
    }

    return ret;
}

gint32 CRpcRouterBridge::BuildDisEvtTaskGrp(
    IEventSink* pCallback,
    IMessageMatch* pMatch,
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    if( pMatch == nullptr )
        return -EINVAL;

    TaskGrpPtr pTaskGrp;
    do{
        TaskletPtr pRespTask;
        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );

        if( pCallback != nullptr )
        {
            oParams[ propEventSink ] =
                ObjPtr( pCallback );

            ret = pRespTask.NewObj(
                clsid( CRouterEventRelayRespTask ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;
            CRouterEventRelayRespTask* prerrt =
                ( CRouterEventRelayRespTask* )pRespTask;
            prerrt->SetEnable( false );   

            // run the task to set it to a proper
            // state
            ( *pRespTask )( eventZero );

            oParams[ propNotifyClient ] = true;
            oParams[ propEventSink ] =
                ObjPtr( pRespTask );
        }

        ret = pTaskGrp.NewObj(
            clsid( CIfTransactGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        TaskletPtr pAddMatchTask;
        ret = BuildAddMatchTask(
            pMatch, false, pAddMatchTask );
        if( ERROR( ret ) )
            break;

        TaskletPtr pStopProxyTask;
        ret = BuildStartStopReqFwdrProxy(
            pMatch, false, pStopProxyTask );
        if( ERROR( ret ) )
            break;

        TaskletPtr pDisableTask;
        ret = BuildEventRelayTask(
            pMatch, false, pDisableTask );
        if( ERROR( ret ) )
            break;

        pTaskGrp->SetRelation( logicNONE );
        pTaskGrp->AppendTask( pAddMatchTask );
        pTaskGrp->AppendTask( pDisableTask );
        pTaskGrp->AppendTask( pStopProxyTask );

        pTask = pTaskGrp;
        if( pTask.IsEmpty() )
            ret = -EFAULT;

    }while( 0 );

    if( pTask.IsEmpty() || ERROR( ret ) )
    {
        // free the resources the pTaskGrp claimed
        if( !pTaskGrp.IsEmpty() )
            ( *pTaskGrp )( eventCancelTask );

    }

    return ret;
}

gint32 CRpcRouterBridge::RunDisableEventTask(
    IEventSink* pCallback,
    IMessageMatch* pMatch )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pMatch == nullptr )
        return -EINVAL;

    do{
        TaskletPtr pTask;

        ret = BuildDisEvtTaskGrp(
            pCallback, pMatch, pTask );

        if( ERROR( ret ) )
            break;

        ret = AddSeqTask( pTask, false );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;
        
    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::ForwardModOnOfflineEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    gint32 ret = 0;
    do{
        bool bOffline = false;
        if( iEvent == eventModOffline )
            bOffline = true;
        else if( iEvent != eventModOnline )
        {
            ret = -EINVAL;
            break;
        }

        // find all the interested bridges
        std::map< MatchPtr, gint32 >* plm =
            &m_mapRmtMatches;

        std::set< guint32 > setPortIds;
        std::vector< MatchPtr > vecMatches;
        CStdRMutex oRouterLock( GetLock() );
        for( auto elem : *plm )
        {
            IMessageMatch* pMatch = elem.first;
            if( unlikely( pMatch == nullptr ) )
                continue;

            CCfgOpenerObj oMatchCfg( pMatch );
            string strDest;
            ret = oMatchCfg.GetStrProp(
                propDestDBusName, strDest );
            if( ERROR( ret ) )
                continue;

            if( strModule == strDest )
            {
                guint32 dwPortId = 0;
                ret = oMatchCfg.GetIntProp(
                    propPortId, dwPortId );
                if( ERROR( ret ) )
                    continue;

                setPortIds.insert( dwPortId );
                vecMatches.push_back( elem.first );
            }
        }
        oRouterLock.Unlock();

        if( setPortIds.empty() )
            break;

        DMsgPtr pMsg;
        ret = pMsg.NewObj(
            ( EnumClsid )DBUS_MESSAGE_TYPE_SIGNAL );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetPath(
            DBUS_SYS_OBJPATH );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetInterface(
            DBUS_SYS_INTERFACE );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetMember(
            "NameOwnerChanged" );
        if( ERROR( ret ) )
            break;

        string strRtName;
        CCfgOpenerObj oRtCfg( this );
        ret = oRtCfg.GetStrProp(
            propSvrInstName, strRtName );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetSender(
            DBUS_DESTINATION( strRtName ) );

        pMsg.SetSerial( iEvent );

        const char* szUnknown = "unknown";
        const char* szEmpty = "";
        const char* szModule = strModule.c_str();
        if( bOffline )
        {
            if( !dbus_message_append_args( pMsg,
                DBUS_TYPE_STRING, &szModule,
                DBUS_TYPE_STRING, &szUnknown,
                DBUS_TYPE_STRING, &szEmpty,
                DBUS_TYPE_INVALID ) )
            {
                ret = -ENOMEM;
                break;
            }
        }
        else
        {
            if( !dbus_message_append_args( pMsg,
                DBUS_TYPE_STRING, &szModule,
                DBUS_TYPE_STRING, &szEmpty,
                DBUS_TYPE_STRING, &szUnknown,
                DBUS_TYPE_INVALID ) )
            {
                ret = -ENOMEM;
                break;
            }
        }

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );
        if( ERROR( ret ) )
            break;

        // forward the event
        for( auto elem : setPortIds )
        {
            InterfPtr pIf;
            ret = GetBridge( elem, pIf );
            if( ERROR( ret ) )
                continue;
            CRpcTcpBridge* pBridge = pIf;
            if( unlikely( pBridge == nullptr ) )
                continue;

            CCfgOpener oEvtCtx;
            oEvtCtx[ propRouterPath ] =
                std::string( "/" );
            if( ERROR( ret ) )
                continue;

            pBridge->ForwardEvent(
                oEvtCtx.GetCfg(),
                pMsg, pDummyTask );

        }

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::AddToNodePidMap(
    const std::string& strNode,
    guint32 dwPortId )
{
    if( dwPortId == 0 || strNode.empty() )
        return -EINVAL;

    CStdRMutex oRouterLock( GetLock() );
    m_mapNode2Pid[ strNode ] = dwPortId;
    return 0;
}

gint32 CRpcRouterBridge::RemoveFromNodePidMap(
    const std::string& strNode )
{
    if( strNode.empty() )
        return -EINVAL;

    CStdRMutex oRouterLock( GetLock() );
    m_mapNode2Pid.erase( strNode );
    return 0;
}

gint32 CRpcRouterBridge::FindRefCount(
    const std::string& strNode,
    guint32 dwPortId,
    guint32 dwProxyId )
{
    if( dwPortId == 0 || strNode.empty() )
        return -EINVAL;
       
    gint32 ret = 0;
    do{
        CRegObjectBridge oRegObj(
            strNode, dwPortId, dwProxyId );

        CStdRMutex oRouterLock( GetLock() );
        std::map< CRegObjectBridge, gint32 >::iterator
            itr = m_mapRefCount.find( oRegObj );

        if( itr != m_mapRefCount.end() )
        {
            break;
        }
        else
        {
            ret = -ENOENT;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::AddRefCount(
    const std::string& strNode,
    guint32 dwPortId,
    guint32 dwProxyId )
{
    if( dwPortId == 0 || strNode.empty() )
        return -EINVAL;
       
    gint32 ret = 0;
    do{
        CRegObjectBridge oRegObj(
            strNode, dwPortId, dwProxyId );

        InterfPtr pIf;
        ret = GetBridgeProxy( dwProxyId, pIf );
        if( ERROR( ret ) )
            break;

        CStdRMutex oRouterLock( GetLock() );
        std::map< CRegObjectBridge, gint32 >::iterator
            itr = m_mapRefCount.find( oRegObj );

        if( itr != m_mapRefCount.end() )
        {
            ret = ++m_mapRefCount[ oRegObj ];
        }
        else
        {
            ret = m_mapRefCount[ oRegObj ] = 1;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::DecRefCount(
    const std::string& strNode,
    guint32 dwPortId )
{
    if( dwPortId == 0 ||
        strNode.empty() )
        return -EINVAL;

    gint32 ret = 0;
    CRegObjectBridge oReg( strNode, dwPortId );

    CStdRMutex oRouterLock( GetLock() );
    std::map< CRegObjectBridge, gint32 >::iterator
        itr = m_mapRefCount.find( oReg );

    if( itr != m_mapRefCount.end() )
    {
        gint32 iRef = --itr->second;
        if( iRef <= 0 )
        {
            m_mapRefCount.erase( itr );
            iRef = 0;
            for( auto elem : m_mapRefCount )
            {
                if( oReg.GetNodeName() == strNode )
                    iRef += elem.second;
            }
        }
        ret = std::max( iRef, 0 );
    }
    else
    {
        ret = -ENOENT;
    }

    return ret;
}

gint32 CRpcRouterBridge::ClearRefCountByPortId(
    guint32 dwPortId,
    std::vector< std::string >& vecNodes )
{
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        if( itr->first.GetPortId() == dwPortId )
        {
            vecNodes.push_back(
                itr->first.GetNodeName() );
            itr = m_mapRefCount.erase( itr );
            continue;
        }
        itr++;
    }
    return vecNodes.size();
}

gint32 CRpcRouterBridge::ClearRefCountByNodeName(
    const std::string& strNode,
    std::set< guint32 >& setPortIds )
{
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        if( itr->first.GetNodeName() == strNode )
        {
            setPortIds.insert(
                itr->first.GetPortId() );
            itr = m_mapRefCount.erase( itr );
            continue;
        }
        itr++;
    }

    return setPortIds.size();
}

// get the reference count to the bridge Object
// `dwPortId'
gint32 CRpcRouterBridge::GetRefCountByPortId(
    guint32 dwPortId )
{
    gint32 iRefCount = 0;
    CStdRMutex oRouterLock( GetLock() );
    for( auto elem : m_mapRefCount )
    {
        if( dwPortId == elem.first.GetPortId() )
            iRefCount += ( gint32 )elem.second;
    }
    return iRefCount;
}

// get the reference count to the bridge proxy
// object `strNode'
gint32 CRpcRouterBridge::GetRefCountByNodeName(
    const std::string& strNode )
{
    if( strNode.empty() )
        return -EINVAL;

    gint32 iRefCount = 0;
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        if( itr->first.GetNodeName() == strNode )
        {
            iRefCount += ( gint32 )itr->second;
            continue;
        }
        itr++;
    }

    return iRefCount;
}

// get bridgeproxy's portid by node name
gint32 CRpcRouterBridge::GetProxyIdByNodeName(
    const std::string& strNode,
    guint32& dwPortId ) const
{
    if( strNode.empty() )
        return -EINVAL;

    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapNode2Pid.find( strNode );
    if( itr == m_mapNode2Pid.end() )
        return -ENOENT;

    dwPortId = itr->second;
    return 0;
}

gint32 CRpcRouterBridge::GetBridgeProxy(
    const std::string& strNode,
    InterfPtr& pIf )
{
    if( strNode.empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        guint32 dwPortId = 0;
        ret = GetProxyIdByNodeName(
            strNode, dwPortId );
        if( ERROR( ret ) )
            break;

        ret = this->GetBridgeProxy(
            dwPortId, pIf );

    }while( 0 );

    return ret;
}

gint32 CRpcRouterBridge::GetBridgeProxyByPath(
    const std::string& strPath,
    InterfPtr& pIf )
{
    if( strPath.empty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        std::string strNode;
        ret = GetNodeName( strPath, strNode );
        if( ERROR( ret ) )
            break;

        ret = GetBridgeProxy( strNode, pIf );

    }while( 0 );

    return ret;
}

CRpcRouterReqFwdr::CRpcRouterReqFwdr(
    const IConfigDb* pCfg ) :
    CAggInterfaceServer( pCfg ), super( pCfg )
{
    gint32 ret = 0;
    do{
        CParamList oParams;
        oParams[ propIoMgr ] = 
            ObjPtr( GetIoMgr() );

        ret = m_pDBusSysMatch.NewObj(
            clsid( CDBusSysMatch ) );

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error occurs in"\
            "CRpcRouterReqFwdr's ctor" );
        throw runtime_error( strMsg );
    }

}

gint32 CRpcRouterReqFwdr::Start()
{
    // register this object
    gint32 ret = 0;
    try{
        do{
            ret = super::Start();
            if( ERROR( ret ) )
                break;

            InterfPtr pIf;
            ret = StartReqFwdr( pIf, nullptr );
            if( ERROR( ret ) )
                break;

        } while( 0 );

        return ret;
    }
    catch( system_error& e )
    {
        ret = e.code().value();
    }
    return ret;
}


gint32 CRpcRouterReqFwdr::OnRmtSvrEvent(
    EnumEventId iEvent,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( hPort == INVALID_HANDLE ||
        pEvtCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    guint32 dwPortId = 0;
    CCfgOpener oEvtCtx( pEvtCtx );

    ret = oEvtCtx.GetIntProp(
        propConnHandle, dwPortId );
    if( ERROR( ret ) )
        return ret;

    InterfPtr pIf;
    ret = GetBridgeProxy( dwPortId, pIf );
    if( ERROR( ret ) )
        return ret;

    TaskletPtr pDeferTask;
    switch( iEvent )
    {
    case eventRmtSvrOffline:
        {
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pDeferTask,
                ObjPtr( this ),
                &CRpcRouterReqFwdr::OnRmtSvrOffline,
                nullptr, pEvtCtx, hPort );

            if( ERROR( ret ) )
                break;

            ret = AddSeqTask( pDeferTask, false );
            if( ERROR( ret ) )
                ( *pDeferTask )( eventCancelTask );

            break;
        }
    case eventRmtSvrOnline:
    default:
        {
            ret = -ENOTSUP;
            break;
        }
    }

    return ret;
}
gint32 CRpcRouterReqFwdr::OnPreStopLongWait(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    map< guint32, InterfPtr > mapId2Proxies;

    // FIXME: need a task to accomodate all the StopEx
    // completion and then let the pCallback to run
    do{
        CStdRMutex oRouterLock( GetLock() );
        mapId2Proxies = m_mapPid2BdgeProxies;
        m_mapPid2BdgeProxies.clear();

    }while( 0 );

    CParamList oParams;
    oParams[ propIfPtr ] = ObjPtr( this );
    TaskGrpPtr pTaskGrp;
    ret = pTaskGrp.NewObj(
        clsid( CIfTaskGroup ),
        oParams.GetCfg() );
    if( ERROR( ret ) )
        return ret;

    pTaskGrp->SetClientNotify( pCallback );
    pTaskGrp->SetRelation( logicNONE );

    for( auto&& elem : mapId2Proxies )
    {
        ADD_STOPIF_TASK( ret,
            pTaskGrp, elem.second );
    }
    mapId2Proxies.clear();

    if( !m_pReqFwdr.IsEmpty() )
    {
        ADD_STOPIF_TASK( ret,
            pTaskGrp, m_pReqFwdr );
    }

    if( pTaskGrp->GetTaskCount() > 0 )
    {
        TaskletPtr pTask = pTaskGrp;
        ret = GetIoMgr()->
            RescheduleTask( pTask );
        if( SUCCEEDED( ret ) )
            ret = STATUS_PENDING;
    }

    return ret;
}

gint32 CRpcRouterReqFwdr::StartReqFwdr( 
    InterfPtr& pIf, IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        CParamList oParams;
        oParams.SetPointer( propIoMgr, GetIoMgr() );

        EnumClsid iClsid =
            clsid( CRpcReqForwarderImpl );

        std::string strObjDesc;
        CCfgOpenerObj oRouterCfg( this );
        ret = oRouterCfg.GetStrProp(
            propObjDescPath, strObjDesc );
        if( ERROR( ret ) )
            break;

        ret = oParams.CopyProp(
            propSvrInstName, this );
        if( ERROR( ret ) )
            break;

        oParams.SetIntProp( propIfStateClass,
            clsid( CIfReqFwdrState ) );

        string strObjName = OBJNAME_REQFWDR;
        if( HasAuth() )
        {
            strObjName = OBJNAME_REQFWDR_AUTH;
            iClsid =
                clsid( CRpcReqForwarderAuthImpl );
        }

        oParams.SetPointer(
            propIoMgr, GetIoMgr() );

        ret = CRpcServices::LoadObjDesc(
            strObjDesc,
            strObjName,
            true,
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        oParams.SetObjPtr(
            propRouterPtr, ObjPtr( this ) );

        ret = pIf.NewObj(
            iClsid, oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        m_pReqFwdr = pIf;
        ret = pIf->Start();

    }while( 0 );

    return ret;
}

gint32 CRpcRouterReqFwdr::OnRmtSvrOffline(
    IEventSink* pCallback,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    if( hPort == INVALID_HANDLE ||
        pEvtCtx == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CIoManager* pMgr = GetIoMgr();
    InterfPtr pIf;
    do{
        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        CCfgOpener oEvtCtx( pEvtCtx );

        std::string strPath;
        ret = oEvtCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        // not the immediate down-stream node
        if( strPath != "/" )
        {
            ret = OnRmtSvrOfflineMH(
                pCallback, pEvtCtx, hPort );
            if( ret != STATUS_MORE_PROCESS_NEEDED )
                break;
        }

        guint32 dwPortId = 0;
        ret = oEvtCtx.GetIntProp(
            propConnHandle, dwPortId );
        if( ERROR( ret ) )
            break;

        ret = GetBridgeProxy(
            dwPortId, pIf );
         
        if( ERROR( ret ) )
            break;

        TaskletPtr pTask;
        CParamList oParams;

        oParams[ propEventSink ] =
            ObjPtr( pCallback );

        oParams.Push( pIf );
        oParams[ propIfPtr ] =
            ObjPtr( this );

        oParams.SetIntProp(
            propPortId, dwPortId );

        oParams.Push( pEvtCtx );

        ret = pTask.NewObj(
            clsid( CRouterStopBridgeProxyTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = ( *pTask )( eventZero );

    }while( 0 );

    if( ERROR( ret ) )
    {
        // close the port if exists
        pMgr->ClosePort( hPort, pIf );
    }

    return ret;
}

gint32 CRpcRouterReqFwdr::CheckReqToFwrd(
    IConfigDb* pReqCtx,
    DMsgPtr& pMsg,
    MatchPtr& pMatchHit )
{
    gint32 ret = ERROR_FALSE;

    CStdRMutex oRouterLock( GetLock() );
    for( auto&& oPair : m_mapLocMatches )
    {
        CRouterLocalMatch* pMatch =
            oPair.first;

        if( pMatch == nullptr )
            continue;

        ret = pMatch->IsMyReqToForward(
            pReqCtx, pMsg );

        if( SUCCEEDED( ret ) )
        {
            pMatchHit = pMatch;
            break;
        }
    }

    return ret;
}

gint32 CRpcRouterReqFwdr::CheckEvtToRelay(
    IConfigDb* pEvtCtx, DMsgPtr& pMsg,
    vector< MatchPtr >& vecMatches )
{
    gint32 ret = ERROR_FALSE;

    do{
        CStdRMutex oRouterLock( GetLock() );

        ret = m_pDBusSysMatch->
            IsMyMsgIncoming( pMsg );

        if( SUCCEEDED( ret ) )
        {
            string strDest;
            string strMember;
            strMember = pMsg.GetMember();
            if( strMember != "NameOwnerChanged" )
            {
                // currently we don't support other
                // messages.
                break;
            }

            std::set< string > setSrcAddr;
            ret = pMsg.GetStrArgAt( 0, strDest );
            if( ERROR( ret ) )
                break;

            guint32 dwPortId = 0;
            CCfgOpener oEvtCtx( pEvtCtx );
            ret = oEvtCtx.GetIntProp(
                propConnHandle, dwPortId );
            if( ERROR( ret ) )
                break;

            for( auto&& oPair : m_mapLocMatches )
            {
                CRouterLocalMatch* pMatch =
                    oPair.first;

                if( unlikely( pMatch == nullptr ) )
                    continue;

                CCfgOpener oMatch(
                    ( IConfigDb* )pMatch->GetCfg() );

                ret = oMatch.IsEqualProp(
                    propRouterPath, pEvtCtx );

                if( ERROR( ret ) )
                    continue;

                ret = oMatch.IsEqual(
                    propPrxyPortId, dwPortId );
                if( ERROR( ret ) )
                    continue;

                ret = oMatch.IsEqual(
                    propDestDBusName, strDest );

                if( ERROR( ret ) )
                    continue;

                string strUniqName =
                    oMatch[ propSrcUniqName ];

                if( setSrcAddr.find( strUniqName ) ==
                    setSrcAddr.end() )
                {
                    setSrcAddr.insert( strUniqName );
                    vecMatches.push_back( pMatch );
                }
            }
            ret = 0;
            break;
        }

        for( auto&& oPair : m_mapLocMatches )
        {
            CRouterLocalMatch* pMatch =
                oPair.first;

            if( pMatch == nullptr )
                continue;

            ret = pMatch->IsMyEvtToRelay(
                pEvtCtx, pMsg );

            if( SUCCEEDED( ret ) )
            {
                vecMatches.push_back( pMatch );
            }
            else
            {
                ret = 0;
            }
        }
    }while( 0 );

    return ret;
}

gint32 CRpcRouterReqFwdr::IsMatchOnline(
    IMessageMatch* pMatch, bool& bOnline ) const
{
    MatchPtr ptrMatch;

    if( pMatch == nullptr )
        return -EINVAL;

    gint32 ret = GetMatchToAdd(
        pMatch, false, ptrMatch );

    if( ERROR( ret ) )
        return ret;

    const map< MatchPtr, gint32 >*
        plm = &m_mapLocMatches;

    do{
        CStdRMutex oRouterLock( GetLock() );
        map< MatchPtr, gint32 >::const_iterator
            itr = plm->find( ptrMatch );

        if( itr != plm->end() )
        {
            ret = -ENOENT;
            break;
        }

        CMessageMatch* pDstMat = itr->first;
        CCfgOpener oMatchCfg(
            ( IConfigDb* )pDstMat->GetCfg() );

        ret = oMatchCfg.GetBoolProp(
            propOnline, bOnline );

    }while( 0 );

    return ret;
}

gint32 CRpcRouterReqFwdr::SetMatchOnline(
    IMessageMatch* pMatch, bool bOnline )
{
    MatchPtr ptrMatch;
    if( pMatch == nullptr )
        return -EINVAL;

    CMessageMatch* pSrcMat =
        static_cast< CMessageMatch* >( pMatch );
    CCfgOpener oSrcMatch(
        ( IConfigDb* )pSrcMat->GetCfg() );

    gint32 ret = 0;
    do{
        std::string strDest;
        ret = oSrcMatch.GetStrProp(
            propDestDBusName, strDest );
        if( ERROR( ret ) )
            break;

        std::string strPath;
        ret = oSrcMatch.GetStrProp(
            propObjPath, strPath );
        if( ERROR( ret ) )
            break;

        CStdRMutex oRouterLock( GetLock() );
        for( auto oElem : m_mapLocMatches )
        {
            CMessageMatch* pDstMat = oElem.first;
            CCfgOpener oMatchCfg(
                ( IConfigDb* )pDstMat->GetCfg() );

            ret = oMatchCfg.IsEqualProp(
                propRouterPath, pMatch );

            if( ERROR( ret ) )
                continue;

            ret = oMatchCfg.IsEqualProp(
                propPrxyPortId, pMatch );

            if( ERROR( ret ) )
                continue;

            ret = oMatchCfg.IsEqual(
                propObjPath, strPath );

            if( ERROR( ret ) )
                continue;

            ret = oMatchCfg.IsEqual(
                propDestDBusName, strDest );

            if( ERROR( ret ) )
                continue;

            oMatchCfg.SetBoolProp(
                propOnline, bOnline );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcRouterReqFwdr::AddLocalMatch(
    IMessageMatch* pMatch )
{
    MatchPtr ptrMatch;
    gint32 ret = GetMatchToAdd(
        pMatch, false, ptrMatch );

    if( ERROR( ret ) )
        return ret;

    CStdRMutex oRouterLock( GetLock() );
    return AddRemoveMatch(
        ptrMatch, true, &m_mapLocMatches );
}

gint32 CRpcRouterReqFwdr::RemoveLocalMatch(
    IMessageMatch* pMatch )
{
    MatchPtr ptrMatch;
    gint32 ret = GetMatchToRemove(
        pMatch, false, ptrMatch );

    if( ERROR( ret ) )
        return ret;

    CStdRMutex oRouterLock( GetLock() );
    return AddRemoveMatch(
        ptrMatch, false, &m_mapLocMatches );
}

gint32 CRpcRouterReqFwdr::RemoveLocalMatchByPortId(
    guint32 dwPortId )
{
    gint32 ret = 0;
    gint32 iCount = 0;

    do{
        map< MatchPtr, gint32 >* plm =
            &m_mapLocMatches;

        CStdRMutex oRouterLock( GetLock() );
        map< MatchPtr, gint32 >::iterator
            itr = plm->begin();

        while( itr != plm->end() )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )itr->first );

            ret = oMatch.IsEqual(
                propPrxyPortId, dwPortId );

            if( ERROR( ret ) )
            {
                itr++;
                continue;
            }

            itr = plm->erase( itr );
            iCount++;
            continue;
        }

    }while( 0 );

    return iCount;
}

gint32 CRpcRouterReqFwdr::RemoveLocalMatchByUniqName(
    const std::string& strUniqName,
    std::vector< MatchPtr >& vecMatches )
{
    gint32 ret = 0;

    do{
        map< MatchPtr, gint32 >* plm =
            &m_mapLocMatches;

        CStdRMutex oRouterLock( GetLock() );
        map< MatchPtr, gint32 >::iterator
            itr = plm->begin();

        while( itr != plm->end() )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )itr->first );

            std::string strVal;
            ret = oMatch.GetStrProp(
                propSrcUniqName, strVal );

            if( ERROR( ret ) )
            {
                ++itr;
                continue;
            }

            if( strUniqName == strVal )
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

gint32 CRpcRouterReqFwdr::AddRefCount(
    guint32 dwPortId,
    const std::string& strSrcUniqName,
    const std::string& strSrcDBusName )
{
    if( dwPortId == 0 ||
        strSrcDBusName.empty() ||
        strSrcUniqName.empty() )
        return -EINVAL;
       
    gint32 ret = 0;
    RegObjPtr pRegObj( true );

    CCfgOpener oCfg( ( IConfigDb* )pRegObj );

    oCfg.SetStrProp(
        propSrcDBusName, strSrcDBusName );

    oCfg.SetIntProp(
        propPrxyPortId, dwPortId );

    oCfg.SetStrProp(
        propSrcUniqName, strSrcUniqName );

    CStdRMutex oRouterLock( GetLock() );
    if( m_mapRefCount.find( pRegObj ) !=
        m_mapRefCount.end() )
    {
        ret = ++m_mapRefCount[ pRegObj ];
    }
    else
    {
        ret = m_mapRefCount[ pRegObj ] = 1;
    }
    return ret;
}

// NOTE: the return value on success is the
// reference count to the bridge proxy, but not
// the reference count to the regobj.
gint32 CRpcRouterReqFwdr::DecRefCount(
    guint32 dwPortId,
    const std::string& strSrcUniqName,
    const std::string& strSrcDBusName )
{
    if( dwPortId == 0 ||
        strSrcDBusName.empty() ||
        strSrcUniqName.empty() )
        return -EINVAL;

    RegObjPtr pRegObj( true );

    CCfgOpener oCfg( ( IConfigDb* )pRegObj );

    oCfg.SetStrProp(
        propSrcDBusName, strSrcDBusName );

    oCfg.SetIntProp(
        propPrxyPortId, dwPortId );

    oCfg.SetStrProp(
        propSrcUniqName, strSrcUniqName );

    gint32 ret = 0;

    CStdRMutex oRouterLock( GetLock() );
    if( m_mapRefCount.find( pRegObj ) ==
        m_mapRefCount.end() )
    {
        return -ENOENT;
    }
    else
    {
        gint32 iRef = --m_mapRefCount[ pRegObj ];
        if( iRef <= 0 )
        {
            m_mapRefCount.erase( pRegObj );
            iRef = 0;
            for( auto elem : m_mapRefCount )
            {
                const RegObjPtr& pReg = elem.first;
                if( pReg->GetPortId() == dwPortId )
                    iRef += elem.second;
            }
        }
        ret = std::max( iRef, 0 );
    }

    return ret;
}

// for local client offline
gint32 CRpcRouterReqFwdr::GetRefCountBySrcDBusName(
    const std::string& strName )
{
    gint32 iCount = 0;
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        string strName2 =
            itr->first->GetSrcDBusName();

        if( strName2 == strName )
            iCount++;

        ++itr;
    }
    return iCount;
}

// for local client offline
gint32 CRpcRouterReqFwdr::ClearRefCountBySrcDBusName(
    const std::string& strName,
    std::set< guint32 >& setPortIds )
{
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        string strName2 =
            itr->first->GetSrcDBusName();

        if( strName2 == strName )
        {
            setPortIds.insert(
                itr->first->GetPortId() );
            itr = m_mapRefCount.erase( itr );
        }
        else
        {
            ++itr;
        }
    }
    return setPortIds.size();
}

// for local client offline
gint32 CRpcRouterReqFwdr::GetRefCountByUniqName(
    const std::string& strUniqName )
{
    gint32 iCount = 0;
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        string strUniqName2 =
            itr->first->GetUniqName();

        if( strUniqName2 == strUniqName )
            iCount++;

        ++itr;
    }
    return iCount;
}

// for local client offline
gint32 CRpcRouterReqFwdr::ClearRefCountByUniqName(
    const std::string& strUniqName,
    std::set< guint32 >& setPortIds )
{
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        string strUniqName2 =
            itr->first->GetUniqName();

        if( strUniqName2 == strUniqName )
        {
            setPortIds.insert(
                itr->first->GetPortId() );
            itr = m_mapRefCount.erase( itr );
        }
        else
        {
            ++itr;
        }
    }
    return setPortIds.size();
}

// for local client offline
gint32 CRpcRouterReqFwdr::FindUniqNamesByPortId(
    guint32 dwPortId,
    std::set< stdstr >& setNames )
{
    gint32 iRefCount = 0;
    CStdRMutex oRouterLock( GetLock() );
    for( auto elem : m_mapRefCount )
    {
        if( dwPortId == elem.first->GetPortId() )
        {
            setNames.insert(
                elem.first->GetUniqName() );
        }
    }
    return setNames.size();
}

// for local client offline
gint32 CRpcRouterReqFwdr::GetRefCountByPortId(
    guint32 dwPortId )
{
    gint32 iRefCount = 0;
    CStdRMutex oRouterLock( GetLock() );
    for( auto elem : m_mapRefCount )
    {
        if( dwPortId == elem.first->GetPortId() )
            iRefCount += ( gint32 )elem.second;
    }
    return iRefCount;
}

// for BridgeProxy offline
gint32 CRpcRouterReqFwdr::ClearRefCountByPortId(
    guint32 dwPortId,
    vector< string >& vecUniqNames )
{
    CStdRMutex oRouterLock( GetLock() );
    auto itr = m_mapRefCount.begin();
    while( itr != m_mapRefCount.end() )
    {
        if( itr->first->GetPortId() == dwPortId )
        {
            vecUniqNames.push_back(
                itr->first->GetUniqName() );
            itr = m_mapRefCount.erase( itr );
            continue;
        }
        itr++;
    }
    return vecUniqNames.size();
}

gint32 CRpcRouterReqFwdr::CheckMatch(
    IMessageMatch* pMatch )
{
    if( pMatch == nullptr )
        return -EINVAL;

    CStdRMutex oRouterLock( GetLock() );
    for( auto&& oPair : m_mapRefCount )
    {
        RegObjPtr pRegObj = oPair.first;
        gint32 ret = pRegObj->IsMyMatch( pMatch );
        if( SUCCEEDED( ret ) )
            return ret;
    }
    return -ENOENT;
}
static guint32 dwAddCount = 0;
static guint32 dwRemoveCount = 0;

gint32 CRouterEnableEventRelayTask::RunTask()
{
    gint32 ret = 0;
    do{

        CParamList oParams;
        CParamList oTaskCfg(
            ( IConfigDb* )GetConfig() );

        bool bEnable = false;
        ret = oTaskCfg.GetBoolProp( 0, bEnable );
        if( ERROR( ret ) )
            break;

        IMessageMatch* pMatch = nullptr;
        ret = oTaskCfg.GetPointer( 1, pMatch );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter = nullptr;
        ret = oTaskCfg.GetPointer(
            propRouterPtr, pRouter );
        if( ERROR( ret ) )
            break;

        CRpcReqForwarderProxy* pProxy = nullptr;

        ret = oTaskCfg.GetPointer(
            propIfPtr, pProxy );

        if( ERROR( ret ) && !bEnable )
        {
            break;
        }
        else if( ERROR( ret ) && bEnable )
        {
            InterfPtr pIf;
            ret = pRouter->GetReqFwdrProxy(
                pMatch, pIf );
            if( ERROR( ret ) )
                break;

            pProxy = pIf;
        }

        // enable or disable event
        oParams.Push( bEnable );

        oParams[ propIfPtr ] = ObjPtr( pProxy );
        oParams[ propMatchPtr ] = ObjPtr( pMatch );
        oParams[ propEventSink ] = ObjPtr( this );
        oParams[ propNotifyClient ] = true;

        TaskletPtr pEnableEvtTask;
        ret = pEnableEvtTask.NewObj(
            clsid( CIfEnableEventTask ),
            oParams.GetCfg() );

        ret = ( *pEnableEvtTask )( eventZero );
        if( !ERROR( ret ) )
        {
            bEnable ? dwAddCount++ : dwRemoveCount++;
        }

    }while( 0 );

    if( ret == STATUS_PENDING )
        return ret;

    if( Retriable( ret ) )
        return STATUS_MORE_PROCESS_NEEDED;

    ret = OnTaskComplete( ret );

    return ret;
}

gint32 CRouterEnableEventRelayTask::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = 0;
    CParamList oParams(
        ( IConfigDb* )GetConfig() );

    do{
        bool bEnable = false;

        ret = oParams.GetBoolProp( 0, bEnable );
        if( ERROR( ret ) )
            break;

        if( ERROR( iRetVal ) )
            break;

        if( !bEnable )
            break;

        IMessageMatch* pMatch = nullptr;
        ret = oParams.GetPointer( 1, pMatch );
        if( ERROR( ret ) )
            break;

        CRpcRouterBridge* pRouter = nullptr;
        ret = oParams.GetPointer(
            propRouterPtr, pRouter );
        if( ERROR( ret ) )
            break;

        CRpcReqForwarderProxy* pProxy = nullptr;
        ret = oParams.GetPointer(
            propIfPtr, pProxy );

        if( ERROR( ret ) )
        {
            InterfPtr pIf;
            ret = pRouter->GetReqFwdrProxy(
                pMatch, pIf );
            if( ERROR( ret ) )
                break;
            pProxy = pIf;
        }

        // in case somewhere the transaction failed,
        // add a rollback task
        TaskletPtr pRbTask;
        ret = pRouter->BuildEventRelayTask(
            pMatch, false, pRbTask );

        if( SUCCEEDED( ret ) )
            AddRollbackTask( pRbTask );

    }while( 0 );

    oParams.ClearParams();
    oParams.RemoveProperty( propIfPtr );
    oParams.RemoveProperty( propRouterPtr );
    oParams.RemoveProperty( propMatchPtr );

    return iRetVal;
}

gint32 CRouterAddRemoteMatchTask::AddRemoteMatchInternal(
    CRpcRouter* pRouter1,
    IMessageMatch* pMatch,
    bool bEnable )
{
    gint32 ret = 0;
    do{
        if( pRouter1 == nullptr ||
            pMatch == nullptr )
            return false;

        CRpcRouterBridge* pRouter =
            static_cast< CRpcRouterBridge* >
                ( pRouter1 ); 

        if( bEnable )
        {
            ret = pRouter->AddRemoteMatch( pMatch );
            if( ret == EEXIST )
            {
                ret = 0;
                InterfPtr pIf;
                ret = pRouter->GetReqFwdrProxy(
                    pMatch, pIf );

                if( ERROR( ret ) )
                    break;

                if( pIf->GetState() == stateRecovery )
                    ret = ENOTCONN;
                // stop further actions in this
                // transaction
                ChangeRelation( logicOR );
                break;
            }
            else if( ERROR( ret ) )
                break;

            // in case the rest transaction failed
            // add a rollback task
            TaskletPtr pRbTask;
            ret = pRouter->BuildAddMatchTask(
                pMatch, false, pRbTask );

            if( SUCCEEDED( ret ) )
                AddRollbackTask( pRbTask );
        }
        else
        {
            // a DisableEvent request
            ret = pRouter->RemoveRemoteMatch(
                pMatch );

            if( ret == EEXIST )
            {
                ret = 0;
                ChangeRelation( logicOR );
                break;
            }
        }

    }while( 0 );

    return ret;
}

// this task as a precondition of the following
// tasks
gint32 CRouterAddRemoteMatchTask::RunTask()
{
    gint32 ret = 0;
    do{
        CParamList oParams( m_pCtx );

        CRpcRouterBridge* pRouter = nullptr;
        ret = oParams.GetPointer(
            propIfPtr, pRouter );

        if( ERROR( ret ) )
            break;

        bool bEnable = false;
        IMessageMatch* pMatch;

        ret = oParams.GetBoolProp( 0, bEnable );
        if( ERROR( ret ) )
            break;

        ret = oParams.GetPointer( 1, pMatch );
        if( ERROR( ret ) )
            break;

        ret = AddRemoteMatchInternal(
            pRouter, pMatch, bEnable );

    }while( 0 );

    return ret;
}

gint32 CRouterStartRecvTask::RunTask()
{
    gint32 ret = 0;
    CParamList oParams(
        ( IConfigDb* )GetConfig() );

    do{
        IMessageMatch* pMatch = nullptr;
        ret = oParams.GetPointer(
            propMatchPtr, pMatch );
        if( ERROR( ret ) )
            break;
    
        CRpcReqForwarderProxy* pProxy = nullptr;
        ret = oParams.GetPointer(
            propIfPtr, pProxy );
        if( ERROR( ret ) )
        {
            CRpcRouterBridge* pRouter = nullptr;
            ret = oParams.GetPointer(
                propRouterPtr, pRouter );
            if( ERROR( ret ) )
                break;

            InterfPtr pIf;
            ret = pRouter->GetReqFwdrProxy(
                pMatch, pIf );
            if( ERROR( ret ) )
                break;

            pProxy = pIf;
        }

        CParamList oRecvParams;
        oRecvParams[ propMatchPtr ] =
            ObjPtr( pMatch );

        oRecvParams[ propIfPtr ] =
            ObjPtr( pProxy );

        TaskletPtr pRecvTask;
        pRecvTask.NewObj(
            clsid( CIfStartRecvMsgTask ),
            oRecvParams.GetCfg() );

        if( pProxy->GetState() == stateRecovery )
        {
            // server is not up
            ret = ENOTCONN;
            break;
        }
        // transfer the control to the proxy
        ret = pProxy->AddAndRun( pRecvTask );
        if( ERROR( ret ) )
            break;

        ret = pRecvTask->GetError();
        if( ret == STATUS_PENDING )
        {
            ret = 0;
            break;
        }
        if( ret == -ENOTCONN )
        {
            // server is not up
            ret = ENOTCONN;
            break;
        }

    }while( 0 );

    oParams.ClearParams();
    oParams.RemoveProperty( propIfPtr );
    oParams.RemoveProperty( propRouterPtr );
    oParams.RemoveProperty( propMatchPtr );

    return ret;
}

gint32 CRouterEventRelayRespTask::OnTaskComplete(
    gint32 iRetVal )
{
    gint32 ret = 0;
    CParamList oResp;
    oResp[ propReturnValue ] = iRetVal;

    CParamList oParams(
        ( IConfigDb* )GetConfig() );

    CRpcRouterBridge* pRouter = nullptr;
    ret = oParams.GetPointer(
        propIfPtr, pRouter );

    if( ERROR( ret ) )
        return ret;

    EventPtr pTask;
    ret = GetInterceptTask( pTask );
    if( ERROR( ret ) )
        return 0;

    return pRouter->SetResponse(
        pTask, oResp.GetCfg() );
}

CIfRouterState::CIfRouterState(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CIfRouterState ) );
}

gint32 CIfRouterState::SubscribeEvents()
{
    vector< EnumPropId > vecEvtToSubscribe = {
        propRmtSvrEvent,
    };
    return SubscribeEventsInternal(
        vecEvtToSubscribe );
}

CRpcRouterManager::CRpcRouterManager(
    const IConfigDb* pCfg ) :
    CAggInterfaceServer( pCfg ), super( pCfg )
{
    gint32 ret = 0;
    do{
        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwRole = 0;
        CCfgOpener oCfg( pCfg );
        ret = oCfg.GetIntProp(
            propRouterRole, dwRole );

        if( ERROR( ret ) )
            break;

        m_dwRole = dwRole;

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error occurs in"
            " CRpcRouterManager's ctor" );
        throw runtime_error( strMsg );
    }
}

gint32 CRpcRouterManager::GetRouters(
    std::vector< InterfPtr >& vecRouters ) const
{
    CStdRMutex oRouterLock( GetLock() );
    if( !m_pRouterReqFwdr.IsEmpty() )
        vecRouters.push_back( m_pRouterReqFwdr );

    if( m_vecRoutersBdge.size() > 0 )
        vecRouters.insert(
            vecRouters.end(),
            m_vecRoutersBdge.begin(),
            m_vecRoutersBdge.end() );

    return 0;
}

CRpcRouterManager::~CRpcRouterManager()
{
}

gint32 CRpcRouterManager::Start()
{
    gint32 ret = 0;
    do{
        std::string strObjDesc;
        CIoManager* pMgr = GetIoMgr();
        CCfgOpenerObj oRouterCfg( this );
        ret = oRouterCfg.GetStrProp(
            propObjDescPath, strObjDesc );
        if( ERROR( ret ) )
            break;

        ret = super::Start();
        if( ERROR( ret ) )
            break;

        bool bAuth = false;
        ret = pMgr->GetCmdLineOpt(
            propHasAuth, bAuth );
        if( ERROR( ret ) )
            ret = 0;

        ret = 0;

        CParamList oParams;
        oParams.CopyProp( propSvrInstName, this );
        oParams.SetPointer( propRouterPtr, this );

        bool bRfc = false;
        ret = pMgr->GetCmdLineOpt(
            propEnableRfc, bRfc );
        if( SUCCEEDED( ret ) )
            m_bRfc = bRfc;

        if( m_dwRole & 0x02 )
        {
            ret = 0;
            EnumClsid iClsid = clsid( Invalid );
            std::string strObjName; 

            if( bAuth )
            {
                iClsid = clsid(
                    CRpcRouterBridgeAuthImpl );
                strObjName =
                   OBJNAME_ROUTER_BRIDGE_AUTH;
            }
            else
            {
                iClsid = clsid(
                    CRpcRouterBridgeImpl );
                strObjName =
                    OBJNAME_ROUTER_BRIDGE;
            }

            oParams.SetPointer(
                propIoMgr, pMgr );

            ret = CRpcServices::LoadObjDesc(
                strObjDesc,
                strObjName,
                true,
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            oParams[ propIfStateClass ] =
                clsid( CIfRouterState );

            if( IsRfcEnabled() )
            {
                ret = oParams.CopyProp(
                    propMaxReqs, this );
                if( ERROR( ret ) )
                    break;

                ret = oParams.CopyProp(
                    propMaxPendings, this );
                if( ERROR( ret ) )
                    break;

                // set the session checker
                ret = DEFER_IFCALLEX_NOSCHED(
                    m_pRfcChecker, ObjPtr( this ),
                    &CRpcRouterManager::RefreshReqLimit );

                if( ERROR( ret ) )
                    break;

                CIfRetryTask* pRetryTask = m_pRfcChecker;
                if( unlikely( pRetryTask == nullptr ) )
                {
                    ret = -EFAULT;
                    break;
                }

                CCfgOpener oTaskCfg( ( IConfigDb* )
                    pRetryTask->GetConfig() );

                oTaskCfg.SetIntProp(
                    propRetries, MAX_NUM_CHECK );

                oTaskCfg.SetIntProp(
                    propIntervalSec, 60 );

                this->AddAndRun( m_pRfcChecker );
            }

            ObjPtr pRtObj;
            ret = pRtObj.NewObj( iClsid,
                oParams.GetCfg() );
            if( ERROR( ret ) )
                break;

            CRpcRouterBridge* prt = pRtObj;
            ret = prt->Start();
            if( ERROR( ret ) )
                break;

            m_vecRoutersBdge.push_back( prt );
        }

        if( m_dwRole & 0x01 )
        {
            std::string strObjName; 

            EnumClsid iClsid = clsid( Invalid );

            if( bAuth )
            {
                strObjName =
                    OBJNAME_ROUTER_REQFWDR_AUTH;
                iClsid = clsid(
                    CRpcRouterReqFwdrAuthImpl );
            }
            else
            {
                strObjName =
                    OBJNAME_ROUTER_REQFWDR;
                iClsid = clsid(
                    CRpcRouterReqFwdrImpl );
            }

            oParams.SetPointer( propIoMgr, pMgr );

            ret = CRpcServices::LoadObjDesc(
                strObjDesc,
                strObjName,
                true,
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            oParams.SetPointer(
                propIoMgr, pMgr );

            oParams[ propIfStateClass ] =
                clsid( CIfRouterState );
                
            ret = m_pRouterReqFwdr.NewObj( 
                iClsid, oParams.GetCfg() );
            if( ERROR( ret ) )
                break;

            CRpcRouterReqFwdr* prt =
                m_pRouterReqFwdr;

            ret = prt->Start();
            if( ERROR( ret ) )
            {
                for( auto elem : m_vecRoutersBdge )
                    elem->Stop();
                m_vecRoutersBdge.clear();
                break;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CRpcRouterManager::RefreshReqLimit()
{
    std::vector< InterfPtr > vecRoutersBdge;
    do{
        CStdRMutex oIfLock( GetLock() );
        vecRoutersBdge = m_vecRoutersBdge;
        oIfLock.Unlock();
        for( auto elem : vecRoutersBdge )
        {
            CRpcRouterBridge* pRouter = elem;
            if( pRouter == nullptr )
                continue;
            pRouter->RefreshReqLimit();
        }

    }while( 0 );

    return STATUS_MORE_PROCESS_NEEDED;
}

gint32 CRpcRouterManager::OnPreStop(
    IEventSink* pCallback )
{
    gint32 ret = 0;

    TaskGrpPtr pTaskGrp;
    do{
        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );
        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        pTaskGrp->SetClientNotify( pCallback );
        pTaskGrp->SetRelation( logicNONE );

        for( auto elem : m_vecRoutersBdge )
        {
            TaskletPtr pStopTask;
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pStopTask, ObjPtr( elem ),
                &CRpcServices::StopEx,
                ( IEventSink* )0 );

            if( SUCCEEDED( ret ) )
                pTaskGrp->AppendTask( pStopTask );
        }

        if( !m_pRouterReqFwdr.IsEmpty() )
        {
            TaskletPtr pStopTask;
            ret = DEFER_IFCALLEX_NOSCHED2(
                0, pStopTask,
                ObjPtr( m_pRouterReqFwdr ),
                &CRpcServices::StopEx,
                ( IEventSink* )0 );

            if( SUCCEEDED( ret ) )
            {
                ret = pTaskGrp->AppendTask(
                    pStopTask );
            }
        }

        if( pTaskGrp->GetTaskCount() > 0 )
        {
            CIoManager* pMgr = GetIoMgr();
            TaskletPtr pTask = pTaskGrp;
            ret = pMgr->RescheduleTask( pTask );

            if( SUCCEEDED( ret ) )
                ret = STATUS_PENDING;
        }

    }while( 0 );

    if( ERROR( ret ) )
        ( *pTaskGrp )( eventCancelTask );

    return ret;
}

gint32 CRpcRouterManager::RebuildMatches()
{
    gint32 ret = super::RebuildMatches();
    if( ERROR( ret ) )
        return ret;

    do{
        // to avoid name conflict between reqfwdr
        // and bridge when both runs on the same
        // device.
        CCfgOpenerObj oCfg( this );
        guint32 dwRole = 0;
        ret = oCfg.GetIntProp(
            propRouterRole, dwRole );
        if( ERROR( ret ) )
            break;
        
        std::string strSurffix;
        if( dwRole == 1 )
            strSurffix = "_1";
        else if( dwRole == 2 )
            strSurffix = "_2";

        std::string strObjPath;

        ret = oCfg.GetStrProp(
            propObjPath, strObjPath );
        if( ERROR( ret ) )
            break;

        strObjPath += strSurffix;
        string strDest;
        for( auto pMatch : m_vecMatches )
        {
            CCfgOpenerObj oMatch(
                ( CObjBase* )pMatch );

            oMatch.SetStrProp(
                propObjPath, strObjPath );

            ret = oMatch.GetStrProp(
                propSrcDBusName, strDest );
            if( SUCCEEDED( ret ) )
            {
                strDest += strSurffix;
                oMatch.SetStrProp(
                    propSrcDBusName, strDest );
            }
            else
            {
                ret = 0;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CIfRouterMgrState::SubscribeEvents()
{
    vector< EnumPropId > vecEvtToSubscribe = {
        propStartStopEvent,
        propDBusSysEvent,
        propAdminEvent,
    };
    return SubscribeEventsInternal(
        vecEvtToSubscribe );
}

CIfParallelTaskGrpRfc::CIfParallelTaskGrpRfc(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CIfParallelTaskGrpRfc ) );

    CCfgOpener oCfg(
        ( IConfigDb* )GetConfig() );
    gint32 ret = 0;
    do{
        guint32 dwMaxRunning = 0;
        guint32 dwMaxPending = 0;

        ret = oCfg.GetIntProp(
            propMaxReqs, dwMaxRunning );

        if( SUCCEEDED( ret ) )
        {
            m_dwMaxRunning = dwMaxRunning;
            oCfg.RemoveProperty( propMaxReqs );
        }

        ret = oCfg.GetIntProp(
            propMaxPendings, dwMaxPending );

        if( SUCCEEDED( ret ) )
        {
            m_dwMaxPending = dwMaxPending;
            oCfg.RemoveProperty( propMaxPendings );
        }

    }while( 0 );

    return;
}

gint32 CIfParallelTaskGrpRfc::OnChildComplete(
    gint32 iRet, CTasklet* pChild )
{

    CStdRTMutex oTaskLock( GetLock() );
    if( GetRunningCount() > GetMaxRunning() )
    {
        TaskletPtr taskPtr = pChild;
        RemoveTask( taskPtr );
        return 0;
    }
    return super::OnChildComplete( iRet, pChild );
}

gint32 CIfParallelTaskGrpRfc::RunTaskInternal(
    guint32 dwContext )
{
    gint32 ret = 0;

    CStdRTMutex oTaskLock( GetLock() );

    EnumTaskState iState = GetTaskState();
    if( iState == stateStopped )
        return  STATUS_PENDING;

    if( IsCanceling() )
        return STATUS_PENDING;

    if( !IsRunning() &&
        iState == stateStarted )
        return STATUS_PENDING;

    CCfgOpener oCfg( ( IConfigDb* )GetConfig() );
    if( IsNoSched() )
        return STATUS_PENDING;

    if( iState == stateStarting )
    {
        SetTaskState( stateStarted );
        SetRunning( true );
    }

    do{
        std::deque< TaskletPtr > queTasksToRun;

        if( GetTaskCount() == 0 )
            break;

        gint32 iCount = ( gint32 )( GetMaxRunning() -
            GetRunningCount() );

        if( iCount <= 0 )
        {
            ret = STATUS_PENDING;
            break;
        }

        if( GetPendingCount() == 0 )
        {
            ret = STATUS_PENDING;
            break;
        }

        iCount = std::min( ( size_t )iCount,
            ( size_t )GetPendingCount() );

        queTasksToRun.insert(
            queTasksToRun.begin(),
            m_quePendingTasks.begin(),
            m_quePendingTasks.begin() + iCount );

        m_quePendingTasks.erase(
            m_quePendingTasks.begin(),
            m_quePendingTasks.begin() + iCount );

        m_setTasks.insert(
            queTasksToRun.begin(),
            queTasksToRun.end() );

        SetNoSched( true );
        oTaskLock.Unlock();

        for( auto pTask : queTasksToRun )
        {
            if( !pTask.IsEmpty() )
                ( *pTask )( eventZero );
        }

        oTaskLock.Lock();
        SetNoSched( false );

        if( GetPendingCount() > 0 &&
            GetMaxRunning() > GetRunningCount() )
        {
            // there are free slot for more
            // request
            continue;
        }

        // pending means there are tasks running, does
        // not indicate a specific task's return value
        if( GetTaskCount() > 0 )
            ret = STATUS_PENDING;

        break;

    }while( 1 );

    if( ret != STATUS_PENDING )
        SetRunning( false );

    return ret;
}

#define CHECK_GRP_STATE \
    CStdRTMutex oTaskLock( GetLock() ); \
    EnumTaskState iState = GetTaskState(); \
    if( iState == stateStopped ) \
    { \
        ret = ERROR_STATE; \
        break; \
    } \
    if( IsCanceling() ) \
    { \
        ret = ERROR_STATE; \
        break; \
    } \
    if( !IsRunning() && \
        iState == stateStarted ) \
    { \
        ret = ERROR_STATE; \
        break; \
    }


gint32 CIfParallelTaskGrpRfc::AddAndRun(
    TaskletPtr& pTask )
{
    if( pTask.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CHECK_GRP_STATE;

        if( GetPendingCount() > GetMaxPending() )
        {
            m_dwTaskRejected++;
            ret = ERROR_QUEUE_FULL;
            DebugPrint( ret,
                "RFC: queue is full,"
                " AddAndRun failed" );
            break;
        }

        CCfgOpenerObj oCfg( ( CObjBase* )pTask );

        oCfg.SetPointer( propParentTask, this );
        m_quePendingTasks.push_back( pTask );
        m_dwTaskAdded++;

        if( IsNoSched() )
        {
            pTask->MarkPending();
            break;
        }

        if( !IsRunning() ||
            GetRunningCount() >= GetMaxRunning() )
        {
            pTask->MarkPending();
            break;
        }

        oTaskLock.Unlock();

        // re-run this task group immediately
        ret = ( *this )( eventZero );
        if( pTask->GetError() == STATUS_PENDING )
            pTask->MarkPending();

    }while( 0 );

    return ret;
}

gint32 CIfParallelTaskGrpRfc::AppendTask(
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    do{
        CStdRTMutex oTaskLock( GetLock() );
        if( GetPendingCount() >= GetMaxPending() )
        {
            DebugPrint( ret,
                "RFC: queue is full,"
                " append task failed" );
            ret = ERROR_QUEUE_FULL;
            m_dwTaskRejected++;
            break;
        }

        ret = super::AppendTask( pTask );
        m_dwTaskAdded++;

    }while( 0 );

    return ret;
}

gint32 CIfParallelTaskGrpRfc::InsertTask(
    TaskletPtr& pTask )
{
    gint32 ret = 0;
    do{
        CHECK_GRP_STATE;

        m_dwTaskAdded++;

        CCfgOpenerObj oTaskCfg(
            ( CObjBase* )pTask );

        oTaskCfg.SetObjPtr(
            propParentTask, ObjPtr( this ) );

        m_quePendingTasks.push_front( pTask );
        if( GetRunningCount() > GetMaxRunning() )
            break;

        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        CIoManager* pMgr = nullptr;
        ret = GET_IOMGR( oCfg, pMgr );
        if( ERROR( ret ) )
            break;

        oTaskLock.Unlock();

        TaskletPtr pThisTask( this );
        ret = pMgr->RescheduleTask( pThisTask );

    }while( 0 );

    return ret;
}

gint32 CIfParallelTaskGrpRfc::SetLimit(
    guint32 dwMaxRunning, guint32 dwMaxPending )
{
    if( dwMaxPending == 0 || dwMaxRunning == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CHECK_GRP_STATE;

        m_dwMaxPending = std::min(
            dwMaxPending, RFC_MAX_PENDINGS );

        m_dwMaxRunning = std::min(
            dwMaxRunning, RFC_MAX_REQS );

        if( IsNoSched() )
            break;

        CCfgOpener oCfg( ( IConfigDb* )GetConfig() );

        if( GetRunningCount() < GetMaxRunning() &&
            GetPendingCount() > 0 )
        {
            CCfgOpener oCfg(
                ( IConfigDb* )GetConfig() );

            CIoManager* pMgr = nullptr;
            gint32 ret = GET_IOMGR( oCfg, pMgr );
            if( ERROR( ret ) )
                break;

            TaskletPtr pThisTask( this );
            ret = pMgr->RescheduleTask( pThisTask );
        }

    }while( 0 );

    return ret;
}

gint32 gen_sess_hash(
    BufPtr& pBuf,
    std::string& strSess )
{
    BufPtr pTemp( true );
    pTemp->Resize( 20 );
    SHA1 sha;
    sha.Input( pBuf->ptr(), pBuf->size());
    if( !sha.Result( ( guint32* )pTemp->ptr() ) )
        return ERROR_FAIL;

    gint32 ret = BytesToString(
        ( guint8* )pTemp->ptr(),
        pTemp->size(), strSess );

    return ret;
}

gint32 AppendConnParams(
    IConfigDb* pConnParams,
    BufPtr& pBuf )
{
    if( pConnParams == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    if( pBuf.IsEmpty() )
    {
        ret = pBuf.NewObj();
        if( ERROR( ret ) )
            return ret;
    }

    do{
        CConnParams oConn( pConnParams );
        std::string strDestIp =
            oConn.GetDestIpAddr();

        std::string strSrcIp =
            oConn.GetSrcIpAddr();

        guint32 dwDestPort =
            oConn.GetDestPortNum();

        guint32 dwSrcPort =
            oConn.GetSrcPortNum();

        ret = pBuf->Append( strDestIp.c_str(),
            strDestIp.size() );
        if( ERROR( ret ) )
            break;

        ret = pBuf->Append(
            ( guint8* )&dwDestPort,
            sizeof( dwDestPort ) );
        if( ERROR( ret ) )
            break;

        ret = pBuf->Append( strSrcIp.c_str(),
            strSrcIp.size() );
        if( ERROR( ret ) )
            break;

        ret = pBuf->Append(
            ( guint8* )&dwSrcPort,
            sizeof( dwSrcPort ) );

        if( ERROR( ret ) )
            break;

#ifdef DEBUG
        if( false )
        {
            std::string strDump;
            gint32 iRet = BytesToString(
                ( guint8* )pBuf->ptr(),
                pBuf->size(), strDump );

            if( ERROR( iRet ) )
                break;

            DebugPrint( 0, "buf to hash: %s \n",
               strDump.c_str() );
        }
#endif
            
    }while( 0 );
        
    return ret;
}

}
