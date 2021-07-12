/*
 * =====================================================================================
 *
 *       Filename:  clsids.h
 *
 *    Description:  definitions of class ids
 *
 *        Version:  1.0
 *        Created:  06/18/2016 09:31:16 PM
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
#pragma once

#include <stdint.h>

typedef int8_t gint8;
typedef uint8_t guint8;

typedef int16_t gint16;
typedef uint16_t guint16;

typedef int32_t gint32;
typedef uint32_t guint32;

typedef int64_t gint64;
typedef uint64_t guint64;

typedef bool	gboolean;
typedef void* 	gpointer;
typedef guint32   GIOChannel;

#include <nsdef.h>

#define DECL_CLSID( __classname__ ) \
    Clsid_ ## __classname__

#define clsid( classname ) \
    ( ( EnumClsid )DECL_CLSID( classname ) )

#define DECL_IID( __interface_name__ ) \
    Iid_ ##  __interface_name__

#define iid( interface_name ) \
    ( ( EnumClsid )DECL_IID( interface_name  ) )

namespace rpcf
{
/**
* @name Class id declarations
* @{ */
/** Please don't forget to add an entry to the
 * map from Class id to class name in the clsids.cpp
 * the order must be kept the same in favor of
 * binary search @} */

typedef enum : guint32
{
    DECL_CLSID( MinClsid ) = 0x0823,
    DECL_CLSID( CObjBase ),
    DECL_CLSID( CBuffer ),
    DECL_CLSID( CMsgQueue ),
    DECL_CLSID( CPort ),
    DECL_CLSID( CBusPort ),
    DECL_CLSID( CDriverManager ),
    DECL_CLSID( CMainIoLoop ),
    DECL_CLSID( CUtilities ),
    DECL_CLSID( CTimerService ),
    DECL_CLSID( CWorkitemManager ),
    DECL_CLSID( CRegistry ),
    DECL_CLSID( CConfigDb ),
    DECL_CLSID( IoRequestPacket ),
    DECL_CLSID( CInterfaceServer ),
    DECL_CLSID( CRpcLocalReqServer ), 
    DECL_CLSID( CIrpCompThread ),
    DECL_CLSID( CIrpThreadPool ),
    DECL_CLSID( CPnpManager ),
    DECL_CLSID( CDBusLocalPdo ),
    DECL_CLSID( CDBusProxyPdo ),
    DECL_CLSID( CDBusProxyFdo ),
    DECL_CLSID( IRP_CONTEXT ),
    DECL_CLSID( CIoManager ),
    DECL_CLSID( CDBusBusPort ),
    DECL_CLSID( CDBusBusDriver ),
    DECL_CLSID( CIrpSlaveMap ),
    DECL_CLSID( CTaskThread ),
    DECL_CLSID( COneshotTaskThread ),
    DECL_CLSID( CTaskThreadPool ),
    DECL_CLSID( CIoMgrStopTask ),
    DECL_CLSID( CIoMgrPostStartTask ),
    DECL_CLSID( CTaskQueue ),
    DECL_CLSID( CStlEventMap ),
    DECL_CLSID( CStlIntMap ),
    DECL_CLSID( CStlIntVector ),
    DECL_CLSID( CStlIrpVector ),
    DECL_CLSID( CStlIrpVector2 ),
    DECL_CLSID( CStlIntQueue ),
    DECL_CLSID( CStlPortVector ),
    DECL_CLSID( CStlBufVector ),
    DECL_CLSID( CStlObjSet ),
    DECL_CLSID( CClassFactories ),
    DECL_CLSID( CPnpMgrQueryStopCompletion ),
    DECL_CLSID( CPnpMgrDoStopNoMasterIrp ),
    DECL_CLSID( CPnpMgrStartPortCompletionTask ),
    DECL_CLSID( Reserved1 ),
    DECL_CLSID( CPnpMgrStopPortAndDestroyTask ),
    DECL_CLSID( CGenBusPortStopChildTask ),
    DECL_CLSID( CPortStartStopNotifTask ),
    DECL_CLSID( CPortAttachedNotifTask ),
    DECL_CLSID( CDBusBusPortCreatePdoTask ),
    DECL_CLSID( CIoMgrIrpCompleteTask ),
    DECL_CLSID( CProxyPdoConnectTask ),
    DECL_CLSID( CProxyPdoDisconnectTask ),
    DECL_CLSID( CRpcReqForwarderImpl ),
    DECL_CLSID( CRpcReqForwarderProxyImpl ),
    DECL_CLSID( CRpcTcpBridgeProxyImpl ),
    DECL_CLSID( CRpcTcpBridgeImpl ),
    DECL_CLSID( CMessageMatch ),
    DECL_CLSID( CProxyMsgMatch ),
    DECL_CLSID( CDBusDisconnMatch ),
    DECL_CLSID( CDBusSysMatch ),
    DECL_CLSID( CPortStateResumeSubmitTask ),
    DECL_CLSID( CDBusBusPortDBusOfflineTask ),
    DECL_CLSID( CRpcBasePortModOnOfflineTask ),
    DECL_CLSID( CProxyFdoModOnOfflineTask ),
    DECL_CLSID( CProxyFdoRmtDBusOfflineTask ),
    DECL_CLSID( CProxyFdoListenTask ),
    DECL_CLSID( CUnallocatedClsid_1 ),
    DECL_CLSID( CPortState ),
    DECL_CLSID( CLocalProxyState ),
    DECL_CLSID( CRemoteProxyState ),
    DECL_CLSID( CIfServerState ),
    DECL_CLSID( CIfEnableEventTask ),
    DECL_CLSID( CIfOpenPortTask ),
    DECL_CLSID( CIfStartRecvMsgTask ),
    DECL_CLSID( CIfTaskGroup ),
    DECL_CLSID( CIfRootTaskGroup ),
    DECL_CLSID( CIfStopTask ),
    DECL_CLSID( CIfStopRecvMsgTask ),
    DECL_CLSID( CIfPauseResumeTask ),
    DECL_CLSID( CIfCpEventTask ),
    DECL_CLSID( CIfDummyTask ),
    DECL_CLSID( CIfCleanupTask ),
    DECL_CLSID( CIfParallelTaskGrp ),
    DECL_CLSID( CIfIoReqTask ),
    DECL_CLSID( CIfInvokeMethodTask ),
    DECL_CLSID( CUnallocatedClsid_2 ),
    DECL_CLSID( CIfFetchDataTask ),
    DECL_CLSID( CDBusLoopbackPdo ),
    DECL_CLSID( CDBusLoopbackMatch ),
    DECL_CLSID( CDBusTransLpbkMsgTask ),
    DECL_CLSID( CRpcTcpFido ),
    DECL_CLSID( CTcpStreamPdo ),
    DECL_CLSID( CRpcStreamSock ),
    DECL_CLSID( CRpcListeningSock ),
    DECL_CLSID( CRpcStream ),
    DECL_CLSID( CRpcControlStream ),
    DECL_CLSID( CCancelIrpsTask ),
    DECL_CLSID( CStmSockCompleteIrpsTask ),
    DECL_CLSID( COutgoingPacket ),
    DECL_CLSID( CIncomingPacket ),
    DECL_CLSID( CRpcTcpBusPort ),
    DECL_CLSID( CRpcTcpBusDriver ),
    DECL_CLSID( CStmSockConnectTask ),
    DECL_CLSID( CStmSockInvalStmNotifyTask ),
    DECL_CLSID( CStmPdoSvrOfflineNotifyTask ),
    DECL_CLSID( CFidoRecvDataTask ),
    DECL_CLSID( CReqFwdrOpenRmtPortTask ),
    DECL_CLSID( CReqFwdrEnableRmtEventTask ),
    DECL_CLSID( CBridgeAddRemoteMatchTask ),
    DECL_CLSID( CRouterOpenBdgePortTask ),
    DECL_CLSID( CRegisteredObject ),
    DECL_CLSID( CRouterLocalMatch ),
    DECL_CLSID( CRouterRemoteMatch ),
    DECL_CLSID( CRouterEnableEventRelayTask ),
    DECL_CLSID( CReqFwdrForwardRequestTask ),
    DECL_CLSID( CRfpModEventRespTask ),
    DECL_CLSID( CReqFwdrFetchDataTask ),
    DECL_CLSID( CRpcRfpForwardEventTask ),
    DECL_CLSID( CBdgeProxyReadWriteComplete ),
    DECL_CLSID( CRpcRouterReqFwdrImpl ),
    DECL_CLSID( CRpcRouterBridgeImpl ),
    DECL_CLSID( CRpcRouterManagerImpl  ),
    DECL_CLSID( CIfRouterMgrState ),
    DECL_CLSID( CSyncCallback ),
    DECL_CLSID( CTaskWrapper ),
    DECL_CLSID( CClassFactory ),
    DECL_CLSID( CDirEntry ),
    DECL_CLSID( CPortInterfaceMap ),
    DECL_CLSID( CMethodProxy ),
    DECL_CLSID( CMethodServer ),
    DECL_CLSID( CAsyncCallback ),
    DECL_CLSID( CDeferredCall ),
    DECL_CLSID( CIoReqSyncCallback ),
    DECL_CLSID( CStlObjVector ),
    DECL_CLSID( CStlStringSet ),
    DECL_CLSID( CSchedTaskCallback ),
    DECL_CLSID( CTimerWatchCallback ),
    DECL_CLSID( CRpcSockWatchCallback ),
    DECL_CLSID( CDBusLoopHooks ),
    DECL_CLSID( CDBusTimerCallback ),
    DECL_CLSID( CDBusIoCallback ),
    DECL_CLSID( CDBusDispatchCallback ),
    DECL_CLSID( CDBusWakeupCallback ),
    DECL_CLSID( CEvLoopStopCb ),
    DECL_CLSID( CEvLoopAsyncCallback ),
    DECL_CLSID( CDBusConnFlushTask ),
    DECL_CLSID( CIfSvrConnMgr ),
    DECL_CLSID( CMethodServerEx ),
    DECL_CLSID( CMessageCounterTask ),
    DECL_CLSID( CIoWatchTask ),
    DECL_CLSID( CIoWatchTaskServer ),
    DECL_CLSID( CProxyFdoDriver ),
    DECL_CLSID( CRouterStopBridgeProxyTask2 ),
    DECL_CLSID( CRouterStartReqFwdrProxyTask ),
    DECL_CLSID( CRpcTcpFidoDrv ),
    DECL_CLSID( CBusPortStopSingleChildTask ),
    DECL_CLSID( CReqFwdrCloseRmtPortTask ),
    DECL_CLSID( CRouterStopBridgeProxyTask ),
    DECL_CLSID( CRouterStopBridgeTask ),
    DECL_CLSID( CRouterStartRecvTask ),
    DECL_CLSID( CRouterAddRemoteMatchTask ),
    DECL_CLSID( CRouterEventRelayRespTask ),
    DECL_CLSID( CIfTransactGroup ),
    DECL_CLSID( CIfRouterState ),
    DECL_CLSID( CIfStartExCompletion ),
    DECL_CLSID( CTcpFidoListenTask ),
    DECL_CLSID( CIfDeferCallTask ),
    DECL_CLSID( CIfDeferredHandler ),
    DECL_CLSID( CTcpBdgePrxyState ),
    DECL_CLSID( CUnixSockBusDriver ),
    DECL_CLSID( CUnixSockBusPort ),
    DECL_CLSID( CUnixSockStmPdo ),
    DECL_CLSID( CUnixSockStmState ),
    DECL_CLSID( CUnixSockStmProxy ),
    DECL_CLSID( CUnixSockStmProxyRelay ),
    DECL_CLSID( CUnixSockStmServer ),
    DECL_CLSID( CUnixSockStmServerRelay ),
    DECL_CLSID( CIfCallbackInterceptor ),
    DECL_CLSID( CIfUxPingTask ),
    DECL_CLSID( CIfUxPingTicker ),
    DECL_CLSID( CIfUxSockTransTask ),
    DECL_CLSID( CIfUxListeningTask ),
    DECL_CLSID( CIfCreateUxSockStmTask ),
    DECL_CLSID( CIfStartUxSockStmTask ),
    DECL_CLSID( CIfStopUxSockStmTask ),
    DECL_CLSID( CStreamProxyRelay ),
    DECL_CLSID( CStreamServerRelay ),
    DECL_CLSID( CIfResponseHandler ),
    DECL_CLSID( CIfStartUxSockStmRelayTask ),
    DECL_CLSID( CIfUxListeningRelayTask ),
    DECL_CLSID( CIfUxSockTransRelayTask ),
    DECL_CLSID( CIfTcpStmTransTask ),
    DECL_CLSID( CIfDeferCallTaskEx ),
    DECL_CLSID( CIfStmReadWriteTask ),
    DECL_CLSID( CReadWriteWatchTask ),
    DECL_CLSID( CStlLongWordVector ),
    DECL_CLSID( CDBusProxyPdoLpbk ),
    DECL_CLSID( CTcpStreamPdo2 ),
    DECL_CLSID( CRpcConnSock ),
    DECL_CLSID( CRpcNativeProtoFdo ),
    DECL_CLSID( CRpcStream2 ),
    DECL_CLSID( CRpcControlStream2 ),
    DECL_CLSID( CFdoListeningTask ),
    DECL_CLSID( CStmSockConnectTask2 ),
    DECL_CLSID( CRpcNatProtoFdoDrv ),
    DECL_CLSID( CIfReqFwdrPrxyState ),
    DECL_CLSID( CIfReqFwdrState ),
    DECL_CLSID( CIfTcpBridgeState ),
    DECL_CLSID( CBridgeForwardRequestTask ),
    DECL_CLSID( CStreamServerRelayMH ),
    DECL_CLSID( CRpcTcpBridgeProxyStream ),
    DECL_CLSID( CIfTcpStmTransTaskMH ),
    DECL_CLSID( CIfUxSockTransRelayTaskMH ),
    DECL_CLSID( CIfUxListeningRelayTaskMH ),
    DECL_CLSID( CIfStartUxSockStmRelayTaskMH ),
    DECL_CLSID( CIfIoCallTask ),
    DECL_CLSID( CDummyInterfaceState ),
    DECL_CLSID( CRpcTcpBridgeEx ),
    DECL_CLSID( CStlQwordVector ),
    DECL_CLSID( CRpcRouterBridgeAuthImpl ),
    DECL_CLSID( CRpcRouterReqFwdrAuthImpl ),
    DECL_CLSID( CRpcReqForwarderProxyAuthImpl ),
    DECL_CLSID( CRpcTcpBridgeProxyAuthImpl ),
    DECL_CLSID( CRpcTcpBridgeAuthImpl ),
    DECL_CLSID( CRpcReqForwarderAuthImpl ),
    DECL_CLSID( CIfDeferCallTaskEx2 ),
    DECL_CLSID( CSimpleSyncIf ),
    DECL_CLSID( CRedudantNodes ),
    DECL_CLSID( CPythonProxy ),
    DECL_CLSID( CPythonServer ),
    DECL_CLSID( CPythonProxyImpl ),
    DECL_CLSID( CPythonServerImpl ),
    DECL_CLSID( CIfAsyncCancelHandler ),
    DECL_CLSID( CIfParallelTaskGrpRfc ),
    DECL_CLSID( CStlObjMap ),
    DECL_CLSID( MaxClsid ),
    DECL_CLSID( ClassFactoryStart ) = 0x02000000,
    DECL_CLSID( ReservedClsidEnd ) = 0x0FFFFFFF,
    DECL_CLSID( ReservedIidStart ) = 0x10000000,
    DECL_IID( IInterfaceServer ),
    DECL_IID( IUnknown ),
    DECL_IID( IStream ),
    DECL_IID( IStreamMH ),
    DECL_IID( CFileTransferServer ),
    DECL_IID( CStatCounters ),
    DECL_IID( CRpcReqForwarder ),
    DECL_IID( CRpcTcpBridge ),
    DECL_IID( CRpcRouterBridge ),
    DECL_IID( CRpcRouterReqFwdr ),
    DECL_IID( CRpcRouterManager ),
    DECL_IID( IAuthenticate ),
    DECL_IID( ISessionProvider ),
    DECL_CLSID( ReservedIidEnd ) = 0x1FFFFFFF,
    DECL_CLSID( UserClsidStart ) = 0x20000000,

    DECL_CLSID( Invalid ) = 0xFFFFFFFF

}EnumClsid;

}
