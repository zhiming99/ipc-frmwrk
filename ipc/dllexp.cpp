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
#include "tasklets.h"
#include "utils.h"
#include "ifhelper.h"
#include "port.h"
#include "frmwrk.h"
#include "dbusport.h"
#include "objfctry.h"
#include "iiddict.h"
#include "counters.h"
#include "stream.h"
#include "tractgrp.h"
#include "uxport.h"
#include "uxstream.h"
#include "streamex.h"
#include "portex.h"

#include <dlfcn.h>

namespace rpcf
{

using namespace std;

CInterfIdDict  g_oIidDict;

// c++11 required

static FactoryPtr InitClassFactory()
{

    BEGIN_FACTORY_MAPS;

    INIT_MAP_ENTRY( CIrpCompThread );
    INIT_MAP_ENTRY( IoRequestPacket );
    INIT_MAP_ENTRY( IRP_CONTEXT );
    INIT_MAP_ENTRY( CTaskQueue );
    INIT_MAP_ENTRY( CStlIrpVector );
    INIT_MAP_ENTRY( CStlIrpVector2 );
    INIT_MAP_ENTRY( CStlPortVector );
    INIT_MAP_ENTRY( COneshotTaskThread );
    INIT_MAP_ENTRY( CTaskWrapper );

    INIT_MAP_ENTRYCFG( CTaskThread );
    INIT_MAP_ENTRYCFG( CMainIoLoop );
    INIT_MAP_ENTRYCFG( CCancelIrpsTask );
    INIT_MAP_ENTRYCFG( CDBusBusDriver );
    INIT_MAP_ENTRYCFG( CDBusBusPort );
    INIT_MAP_ENTRYCFG( CDBusBusPortCreatePdoTask );
    INIT_MAP_ENTRYCFG( CDBusBusPortDBusOfflineTask );
    INIT_MAP_ENTRYCFG( CDBusDisconnMatch );
    INIT_MAP_ENTRYCFG( CDBusLocalPdo );
    INIT_MAP_ENTRYCFG( CDBusLoopbackMatch );
    INIT_MAP_ENTRYCFG( CDBusLoopbackPdo );
    INIT_MAP_ENTRYCFG( CDBusSysMatch );
    INIT_MAP_ENTRYCFG( CDBusTransLpbkMsgTask );
    INIT_MAP_ENTRYCFG( CDriverManager );
    INIT_MAP_ENTRYCFG( CGenBusPortStopChildTask );
    INIT_MAP_ENTRYCFG( CIfOpenPortTask );
    INIT_MAP_ENTRYCFG( CIfCleanupTask );
    INIT_MAP_ENTRYCFG( CIfCpEventTask );
    INIT_MAP_ENTRYCFG( CIfDummyTask );
    INIT_MAP_ENTRYCFG( CIfEnableEventTask );
    INIT_MAP_ENTRYCFG( CIfInvokeMethodTask );
    INIT_MAP_ENTRYCFG( CIfIoReqTask );
    INIT_MAP_ENTRYCFG( CIfParallelTaskGrp );
    INIT_MAP_ENTRYCFG( CIfPauseResumeTask );
    INIT_MAP_ENTRYCFG( CIfRootTaskGroup );
    INIT_MAP_ENTRYCFG( CIfStartRecvMsgTask );
    INIT_MAP_ENTRYCFG( CIfStopRecvMsgTask );
    INIT_MAP_ENTRYCFG( CIfStopTask );
    INIT_MAP_ENTRYCFG( CIfTaskGroup );
    INIT_MAP_ENTRYCFG( CIfFetchDataTask );
    INIT_MAP_ENTRYCFG( CIoManager );
    INIT_MAP_ENTRYCFG( CIoMgrIrpCompleteTask );
    INIT_MAP_ENTRYCFG( CIoMgrPostStartTask );
    INIT_MAP_ENTRYCFG( CIoMgrStopTask );
    INIT_MAP_ENTRYCFG( CIrpThreadPool );
    INIT_MAP_ENTRYCFG( CLocalProxyState );
    INIT_MAP_ENTRYCFG( CIfServerState );
    INIT_MAP_ENTRYCFG( CRemoteProxyState );
    INIT_MAP_ENTRYCFG( CMessageMatch );
    INIT_MAP_ENTRYCFG( CPnpManager );
    INIT_MAP_ENTRYCFG( CPnpMgrDoStopNoMasterIrp );
    INIT_MAP_ENTRYCFG( CPnpMgrQueryStopCompletion );
    INIT_MAP_ENTRYCFG( CPnpMgrStartPortCompletionTask );
    INIT_MAP_ENTRYCFG( CPnpMgrStopPortAndDestroyTask );
    INIT_MAP_ENTRYCFG( CBusPortStopSingleChildTask );
    INIT_MAP_ENTRYCFG( CPortAttachedNotifTask );
    INIT_MAP_ENTRYCFG( CPortStartStopNotifTask );
    INIT_MAP_ENTRYCFG( CPortState );
    INIT_MAP_ENTRYCFG( CPortStateResumeSubmitTask );
    INIT_MAP_ENTRYCFG( CProxyMsgMatch );
    INIT_MAP_ENTRYCFG( CSyncCallback );
    INIT_MAP_ENTRYCFG( CTaskThreadPool );
    INIT_MAP_ENTRYCFG( CTimerService );
    INIT_MAP_ENTRYCFG( CUtilities );
    INIT_MAP_ENTRYCFG( CWorkitemManager );
    INIT_MAP_ENTRYCFG( CIoReqSyncCallback );
    INIT_MAP_ENTRYCFG( CSchedTaskCallback );
    INIT_MAP_ENTRYCFG( CTimerWatchCallback );
    INIT_MAP_ENTRYCFG( CDBusConnFlushTask );
    INIT_MAP_ENTRYCFG( CIfSvrConnMgr );
    INIT_MAP_ENTRYCFG( CMessageCounterTask );
    INIT_MAP_ENTRYCFG( CIoWatchTask );
    INIT_MAP_ENTRYCFG( CIfTransactGroup );
    INIT_MAP_ENTRYCFG( CRpcBasePortModOnOfflineTask );
    INIT_MAP_ENTRYCFG( CIfStartExCompletion );
    INIT_MAP_ENTRYCFG( CIfDeferCallTask );
    INIT_MAP_ENTRYCFG( CIfDeferredHandler );
    INIT_MAP_ENTRYCFG( CUnixSockBusDriver );
    INIT_MAP_ENTRYCFG( CUnixSockBusPort );
    INIT_MAP_ENTRYCFG( CUnixSockStmPdo );
    INIT_MAP_ENTRYCFG( CUnixSockStmState );
    INIT_MAP_ENTRYCFG( CUnixSockStmProxy );
    INIT_MAP_ENTRYCFG( CUnixSockStmServer );
    INIT_MAP_ENTRYCFG( CIfCallbackInterceptor );
    INIT_MAP_ENTRYCFG( CIfUxPingTask );
    INIT_MAP_ENTRYCFG( CIfUxPingTicker );
    INIT_MAP_ENTRYCFG( CIfUxSockTransTask );
    INIT_MAP_ENTRYCFG( CIfUxListeningTask );
    INIT_MAP_ENTRYCFG( CIfCreateUxSockStmTask );
    INIT_MAP_ENTRYCFG( CIfStartUxSockStmTask );
    INIT_MAP_ENTRYCFG( CIfStopUxSockStmTask );
    INIT_MAP_ENTRYCFG( CIfResponseHandler );
    INIT_MAP_ENTRYCFG( CIfDeferCallTaskEx );
    INIT_MAP_ENTRYCFG( CIfStmReadWriteTask );
    INIT_MAP_ENTRYCFG( CReadWriteWatchTask );
    INIT_MAP_ENTRYCFG( CIfIoCallTask );
    INIT_MAP_ENTRYCFG( CDummyInterfaceState );
    INIT_MAP_ENTRYCFG( CSimpleSyncIf );
    INIT_MAP_ENTRYCFG( CIfAsyncCancelHandler );

#ifdef _USE_LIBEV
    INIT_MAP_ENTRYCFG( CDBusLoopHooks );
    INIT_MAP_ENTRYCFG( CDBusTimerCallback );
    INIT_MAP_ENTRYCFG( CDBusIoCallback );
    INIT_MAP_ENTRYCFG( CDBusDispatchCallback );
    INIT_MAP_ENTRYCFG( CDBusWakeupCallback );
    INIT_MAP_ENTRYCFG( CDBusLoopHooks );
    INIT_MAP_ENTRYCFG( CEvLoopStopCb );
    INIT_MAP_ENTRYCFG( CEvLoopAsyncCallback );

#endif

    END_FACTORY_MAPS;
};

std::string CoGetIfNameFromIid(
    EnumClsid Iid, 
    const std::string& strSuffix )
{
    if( strSuffix.empty() )
        return strSuffix;

    guint64 qwIid = strSuffix[ 0 ] ;
    qwIid <<= 32;
    qwIid += Iid;

    std::string strRet =
        g_oIidDict.GetName( qwIid );
    if( strRet.empty() )
        return strRet;

    size_t pos = strRet.rfind( ':' );
    if( pos == std::string::npos )
    {
        strRet.clear();
        return strRet;
    }
    return strRet.substr( 0, pos );
}

EnumClsid CoGetIidFromIfName(
    const std::string& strName,
    const std::string& strSuffix )
{
    guint64 qwIid = g_oIidDict.GetIid(
        strName + ":" + strSuffix );
    guint32 dwIid = ( guint32 )qwIid;
    return ( EnumClsid )dwIid;
}

gint32 CoAddIidName(
    const std::string& strName,
    EnumClsid Iid,
    const std::string& strSuffix )
{
    std::string strNew =
        strName + ":" + strSuffix;
    guint64 qwIid = Iid;
    guint64 qwPrefix = strSuffix[ 0 ];
    qwIid |= ( qwPrefix << 32 );

    return g_oIidDict.AddIid(
        strNew, qwIid );
}

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

extern "C"
gint32 DllUnload()
{
    return 0;
}

