/*
 * =====================================================================================
 *
 *       Filename:  clsids.cpp
 *
 *    Description:  a map of class id to class name
 *
 *        Version:  1.0
 *        Created:  08/07/2016 01:18:27 PM
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
#include <map>
#include <vector>
#include "clsids.h"
#include "defines.h"
#include "stlcont.h"
#include <dlfcn.h>
#include "frmwrk.h"
#include "prxyport.h"
#include "rpcroute.h"
#include "tcpport.h"
#include "objfctry.h"
#include "fdodrv.h"
#include "stmrelay.h"
#include "tcportex.h"


namespace rpcf
{

using namespace std;

// c++11 required

static FactoryPtr InitClassFactory()
{

    BEGIN_FACTORY_MAPS;

    INIT_MAP_ENTRY( COutgoingPacket );
    INIT_MAP_ENTRY( CIncomingPacket );

    INIT_MAP_ENTRYCFG( CDBusProxyFdo );
    INIT_MAP_ENTRYCFG( CDBusProxyPdo );
    INIT_MAP_ENTRYCFG( CDBusProxyPdoLpbk );
    INIT_MAP_ENTRYCFG( CFidoRecvDataTask );
    INIT_MAP_ENTRYCFG( CProxyFdoListenTask );
    INIT_MAP_ENTRYCFG( CProxyFdoModOnOfflineTask );
    INIT_MAP_ENTRYCFG( CProxyFdoRmtDBusOfflineTask );
    INIT_MAP_ENTRYCFG( CProxyMsgMatch );
    INIT_MAP_ENTRYCFG( CProxyPdoConnectTask );
    INIT_MAP_ENTRYCFG( CProxyPdoDisconnectTask );
    INIT_MAP_ENTRYCFG( CRegisteredObject );
    INIT_MAP_ENTRYCFG( CRemoteProxyState );
    INIT_MAP_ENTRYCFG( CReqFwdrEnableRmtEventTask );
    INIT_MAP_ENTRYCFG( CReqFwdrForwardRequestTask );
    INIT_MAP_ENTRYCFG( CBridgeForwardRequestTask );
    // INIT_MAP_ENTRYCFG( CReqFwdrSendDataTask );
    INIT_MAP_ENTRYCFG( CReqFwdrFetchDataTask );
    INIT_MAP_ENTRYCFG( CReqFwdrOpenRmtPortTask );
    INIT_MAP_ENTRYCFG( CReqFwdrCloseRmtPortTask );
    INIT_MAP_ENTRYCFG( CRouterEnableEventRelayTask );
    INIT_MAP_ENTRYCFG( CRouterLocalMatch );
    INIT_MAP_ENTRYCFG( CRouterOpenBdgePortTask );
    INIT_MAP_ENTRYCFG( CRouterRemoteMatch );
    INIT_MAP_ENTRYCFG( CRpcControlStream );
    INIT_MAP_ENTRYCFG( CRpcListeningSock );
    INIT_MAP_ENTRYCFG( CRpcReqForwarderImpl );
    INIT_MAP_ENTRYCFG( CRpcReqForwarderProxyImpl );
    INIT_MAP_ENTRYCFG( CRpcRfpForwardEventTask );
    INIT_MAP_ENTRYCFG( CRfpModEventRespTask );
    INIT_MAP_ENTRYCFG( CRpcStream );
    INIT_MAP_ENTRYCFG( CRpcStreamSock );
    INIT_MAP_ENTRYCFG( CRpcTcpBridgeImpl );
    INIT_MAP_ENTRYCFG( CRpcTcpBridgeProxyImpl );
    INIT_MAP_ENTRYCFG( CRpcTcpBusDriver );
    INIT_MAP_ENTRYCFG( CRpcTcpBusPort );
    INIT_MAP_ENTRYCFG( CRpcTcpFido );
    INIT_MAP_ENTRYCFG( CStmPdoSvrOfflineNotifyTask );
    INIT_MAP_ENTRYCFG( CStmSockCompleteIrpsTask );
    INIT_MAP_ENTRYCFG( CStmSockConnectTask );
    INIT_MAP_ENTRYCFG( CStmSockInvalStmNotifyTask );
    INIT_MAP_ENTRYCFG( CTcpStreamPdo );
    INIT_MAP_ENTRYCFG( CBdgeProxyReadWriteComplete );
    // INIT_MAP_ENTRYCFG( CBdgeProxyOpenStreamTask );
    // INIT_MAP_ENTRYCFG( CBdgeProxyStartSendTask );
    // INIT_MAP_ENTRYCFG( CBdgeStartRecvDataTask );
    // INIT_MAP_ENTRYCFG( CBdgeStartFetchDataTask );
    INIT_MAP_ENTRYCFG( CRpcSockWatchCallback );
    INIT_MAP_ENTRYCFG( CProxyFdoDriver );
    INIT_MAP_ENTRYCFG( CRpcRouterManagerImpl );
    INIT_MAP_ENTRYCFG( CRpcRouterBridgeImpl );
    INIT_MAP_ENTRYCFG( CRpcRouterReqFwdrImpl );
    INIT_MAP_ENTRYCFG( CRouterOpenBdgePortTask );
    INIT_MAP_ENTRYCFG( CRouterStopBridgeProxyTask2 );
    INIT_MAP_ENTRYCFG( CIfTcpBridgeState );
    INIT_MAP_ENTRYCFG( CIfReqFwdrState );
    INIT_MAP_ENTRYCFG( CIfReqFwdrPrxyState );

    INIT_MAP_ENTRYCFG( CRouterStartReqFwdrProxyTask );
    INIT_MAP_ENTRYCFG( CRouterStopBridgeProxyTask );
    INIT_MAP_ENTRYCFG( CRouterStopBridgeTask );
    INIT_MAP_ENTRYCFG( CRouterStartRecvTask );
    INIT_MAP_ENTRYCFG( CRouterAddRemoteMatchTask );
    INIT_MAP_ENTRYCFG( CRouterEventRelayRespTask );
    INIT_MAP_ENTRYCFG( CRpcTcpFidoDrv );
    INIT_MAP_ENTRYCFG( CIfRouterState );
    INIT_MAP_ENTRYCFG( CIfRouterMgrState );
    INIT_MAP_ENTRYCFG( CTcpFidoListenTask );
    INIT_MAP_ENTRYCFG( CTcpBdgePrxyState );
    INIT_MAP_ENTRYCFG( CIfStartUxSockStmRelayTask );
    INIT_MAP_ENTRYCFG( CStreamProxyRelay );
    INIT_MAP_ENTRYCFG( CStreamServerRelay );
    INIT_MAP_ENTRYCFG( CUnixSockStmProxyRelay );
    INIT_MAP_ENTRYCFG( CUnixSockStmServerRelay );
    INIT_MAP_ENTRYCFG( CIfUxListeningRelayTask );
    INIT_MAP_ENTRYCFG( CIfUxSockTransRelayTask );
    INIT_MAP_ENTRYCFG( CIfTcpStmTransTask );
    INIT_MAP_ENTRYCFG( CTcpStreamPdo2 );
    INIT_MAP_ENTRYCFG( CRpcConnSock );
    INIT_MAP_ENTRYCFG( CRpcNativeProtoFdo );
    INIT_MAP_ENTRYCFG( CRpcStream2 );
    INIT_MAP_ENTRYCFG( CRpcControlStream2 );
    INIT_MAP_ENTRYCFG( CFdoListeningTask );
    INIT_MAP_ENTRYCFG( CStmSockConnectTask2 );
    INIT_MAP_ENTRYCFG( CRpcNatProtoFdoDrv );

    INIT_MAP_ENTRYCFG( CStreamServerRelayMH );
    INIT_MAP_ENTRYCFG( CRpcTcpBridgeProxyStream );
    INIT_MAP_ENTRYCFG( CIfTcpStmTransTaskMH );
    INIT_MAP_ENTRYCFG( CIfUxSockTransRelayTaskMH );
    INIT_MAP_ENTRYCFG( CIfUxListeningRelayTaskMH );
    INIT_MAP_ENTRYCFG( CIfStartUxSockStmRelayTaskMH );
    INIT_MAP_ENTRYCFG( CIfDeferCallTaskEx2 );
    INIT_MAP_ENTRYCFG( CRedudantNodes );

    INIT_MAP_ENTRYCFG( CIfParallelTaskGrpRfc );

    END_FACTORY_MAPS;
};

}

using namespace rpcf;

extern "C"
gint32 DllLoadFactory( FactoryPtr& pFactory )
{
    pFactory = InitClassFactory();
    if( pFactory.IsEmpty() )
        return -EFAULT;

    return 0;
}
