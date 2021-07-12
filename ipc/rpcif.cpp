/*
 * =====================================================================================
 *
 *       Filename:  rpcif.cpp
 *
 *    Description:  implementation of the rpc interface classes
 *
 *        Version:  1.0
 *        Created:  02/04/2017 05:01:32 PM
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

#include <deque>
#include <vector>
#include <string>
#include <semaphore.h>
#include "defines.h"
#include "frmwrk.h"
// #include "emaphelp.h"
#include "connhelp.h"
#include "ifhelper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "jsondef.h"
#include "dbusport.h"
#include <sys/socket.h>
#include <netdb.h>

namespace rpcf
{

using namespace std;

CGenericInterface::CGenericInterface(
    const IConfigDb* pCfg )
{
    CCfgOpener oNewCfg;
    *oNewCfg.GetCfg() = *pCfg;

    if( !oNewCfg.exist( propIfStateClass ) )
    {
        throw std::invalid_argument(
            DebugMsg( -EINVAL, "Error occurs" ) );
    }

    guint32 iStateClass = 0;
    if( !oNewCfg.exist( propIfPtr ) )
        oNewCfg[ propIfPtr ] = this;

    gint32 ret = oNewCfg.GetIntProp(
        propIfStateClass, iStateClass ) ;
        
    ret = m_pIfStat.NewObj( 
        ( EnumClsid )iStateClass,
        oNewCfg.GetCfg() );

    if( ERROR( ret ) )
    {
        throw std::runtime_error( 
            DebugMsg( ret, "Error occurs" ) );
    }
}

PortPtr CGenericInterface::GetPort() const
{
    HANDLE hPort = GetPortHandle();
    if( hPort == INVALID_HANDLE )
        return PortPtr();

    PortPtr pPort;
    gint32 ret = GetIoMgr()->GetPortPtr(
        hPort, pPort );

    if( SUCCEEDED( ret ) )
        return pPort;

    return PortPtr();
}

static CfgPtr InitIfProxyCfg(
    const IConfigDb * pCfg )
{
    CCfgOpener oOldCfg( pCfg );
    if( oOldCfg.exist( propIfStateClass ) )
        return CfgPtr( const_cast< IConfigDb* >( pCfg ) );

    EnumClsid iStateClass =
        clsid( CLocalProxyState );

    CCfgOpener oNewCfg;
    *oNewCfg.GetCfg() = *pCfg;
    if( oNewCfg.exist( propNoPort ) )
    {
        oNewCfg.SetIntProp(
            propIfStateClass, iStateClass );
        return oNewCfg.GetCfg();
    }

    string strPortClass;
    gint32 ret = oNewCfg.GetStrProp(
        propPortClass, strPortClass );

    if( ERROR( ret ) )
    {
        throw std::invalid_argument(
            DebugMsg( -EINVAL, "Error occurs" ) );
    }

    if( strPortClass ==
            PORT_CLASS_DBUS_PROXY_PDO ||
        strPortClass ==
            PORT_CLASS_DBUS_PROXY_PDO_LPBK )
    {
        // CRemoteProxyState will have an extra ip
        // address to match
        iStateClass = clsid( CRemoteProxyState );
    }
    else if( strPortClass ==
        PORT_CLASS_UXSOCK_STM_PDO )
    {
        iStateClass = clsid( CUnixSockStmState );
    }

    oNewCfg.SetIntProp(
        propIfStateClass, iStateClass );

    return oNewCfg.GetCfg();
}

CInterfaceProxy::CInterfaceProxy(
    const IConfigDb* pCfg ) :
    super( InitIfProxyCfg( pCfg ) )
{
    gint32 ret = 0;

    do{
        CCfgOpener oCfg( pCfg );
        string strPortClass;

        if( oCfg.exist( propNoPort ) )
            break;

        ret = oCfg.GetStrProp(
            propPortClass, strPortClass );

        if( ERROR( ret ) )
            break;

        ret = m_pIfMatch.NewObj(
            clsid( CMessageMatch ), pCfg );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj matchCfg(
            ( IMessageMatch* ) m_pIfMatch );

        ret = matchCfg.CopyProp(
            propDestDBusName, pCfg );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.CopyProp(
            propIfName, pCfg );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.CopyProp(
            propObjPath, pCfg );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.SetIntProp(
            propMatchType,
            ( guint32 )matchClient );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.CopyProp(
            propDummyMatch, pCfg );

        if( SUCCEEDED( ret ) )
            RemoveProperty( propDummyMatch );

        ret = 0;

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error in CInterfaceProxy ctor" );
        throw std::runtime_error( strMsg );
    }
}

CInterfaceProxy::~CInterfaceProxy()
{
    if( !m_pIfStat.IsEmpty() )
        m_pIfStat.Clear();

    if( !m_pIfMatch.IsEmpty() )
        m_pIfMatch.Clear();
}

gint32 CRpcBaseOperations::AddPortProps()
{
    gint32 ret = 0;
    if( !IsServer() )
    {
        // for proxy, we get a sender name from the
        // port
        BufPtr pBuf( true );
        ret = GetIoMgr()->GetPortProp(
            GetPortHandle(),
            propSrcDBusName,
            pBuf );

        if( SUCCEEDED( ret ) )
        {
            CInterfaceState* pStat = m_pIfStat;
            if( !pStat->exist( propSrcDBusName ) )
            {
                // Now the proxy will have a valid
                // sender name. And on server
                // side, the propSrcDBusName
                // should be obtained from the
                // server name and the object name
                ret = this->SetProperty(
                    propSrcDBusName, *pBuf );
            }
        }
        pBuf->Resize( 0 );
        ret = GetIoMgr()->GetPortProp(
            GetPortHandle(),
            propSrcUniqName,
            pBuf );
        if( SUCCEEDED( ret ) )
        {
            ret = this->SetProperty(
                propSrcUniqName, *pBuf );
        }

    }
    return ret;
}

gint32 CRpcBaseOperations::OpenPort(
    IEventSink* pCallback )
{
    EnumIfState iState = GetState();

    gint32 ret =
        m_pIfStat->OpenPort( pCallback );

    if( SUCCEEDED( ret ) )
    {
        AddPortProps();

        if( IsServer() &&
            iState == stateStarting )
        {
            CCfgOpenerObj oIfCfg( this );
            bool bPauseOnStart;

            ret = oIfCfg.GetBoolProp(
                propPauseOnStart, bPauseOnStart );

            if( ERROR( ret ) )
            {
                bPauseOnStart = false;
                ret = 0;
            }

            if( bPauseOnStart )
            {
                ret = SetStateOnEvent(
                    eventPaused );
            }
        }
    }
    return ret;
}

gint32 CRpcBaseOperations::ClosePort(
    IEventSink* pCallback )
{
    return m_pIfStat->ClosePort( pCallback );
}

// start listening the event from the interface
// server
gint32 CRpcBaseOperations::EnableEventInternal(
    IMessageMatch* pMatch,
    bool bEnable,
    IEventSink* pCallback )
{
    if( pMatch == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CIoManager* pMgr = GetIoMgr();
        IrpPtr pIrp;

        ret = pIrp.NewObj();
        if( ERROR( ret ) )
            break;

        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        CReqBuilder oReq( this );

        string strMethod = IF_METHOD_ENABLEEVT;

        if( !bEnable )
            strMethod = IF_METHOD_DISABLEEVT;

        oReq.SetMethodName( strMethod );

        oReq.SetCallFlags( CF_WITH_REPLY |
           DBUS_MESSAGE_TYPE_METHOD_CALL );

        oReq.SetObjPtr( propMatchPtr,
            ObjPtr( pMatch ) );

        ret = SetupReqIrp( pIrp,
            oReq.GetCfg(), pCallback );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oTaskCfg(
            ( CObjBase* ) pCallback );

        // for task canceling purpose
        ret = oTaskCfg.SetObjPtr(
            propIrpPtr, ObjPtr( pIrp ) );

        ret = pMgr->SubmitIrp(
            GetPortHandle(), pIrp );

        if( ret == STATUS_PENDING )
            break;

        oTaskCfg.RemoveProperty(
            propIrpPtr );

        pIrp->RemoveTimer();
        pIrp->RemoveCallback();

    }while( 0 );

    return ret;
}

gint32 CRpcBaseOperations::EnableEvent(
    IMessageMatch* pMatch,
    IEventSink* pCompletion )
{
    if( pMatch == nullptr )
        return -EINVAL;

    EnumIfState iState = GetState();

    if( iState == stateStarting ||
        iState == stateStopping ||
        iState == stateStopped ||
        iState == stateInvalid )
        return ERROR_STATE;

    // iState == stateRecovery does not prevent further
    // EnableEvent request to send. the request is
    // rather to setup a communication channel for the
    // match, and it is not quite important whether the
    // server is online or not
    return EnableEventInternal(
        pMatch, true, pCompletion );
}

gint32 CRpcBaseOperations::DisableEvent(
    IMessageMatch* pMatch,
    IEventSink* pCompletion )
{
    if( pMatch == nullptr )
        return -EINVAL;

    EnumIfState iState = GetState();

    if( iState == stateStarting ||
        iState == stateStopped ||
        iState == stateInvalid )
        return ERROR_STATE;

    return EnableEventInternal(
        pMatch, false, pCompletion );
}

gint32 CRpcBaseOperations::GetPortToSubmit(
    CObjBase* pCfg, PortPtr& pPort )
{
    if( pCfg == nullptr )
        return -EINVAL;
    bool bPdo = false;
    return GetPortToSubmit( pCfg, pPort, bPdo );
}

gint32 CRpcBaseOperations::GetPortToSubmit(
    CObjBase* pCfg, PortPtr& pPort, bool& bPdo )
{
    if( pCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpenerObj oCfg( pCfg );

        bPdo = false;
        ret = oCfg.GetBoolProp(
            propSubmitPdo, bPdo );

        if( ERROR( ret ) )
        {
            bPdo = false;
            ret = 0;
        }

        PortPtr pPdoPort;
        PortPtr pThisPort = GetPort();
        CPort* pThis = pThisPort;
        HANDLE hThis = PortToHandle( pThis );

        if( pThisPort.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        if( !bPdo )
        {
            // OK with any port on the port stack
            pPort = pThis->GetTopmostPort();
            break;
        }

        HANDLE hPort = INVALID_HANDLE;
        ret = oCfg.GetIntPtr( propSubmitTo,
            ( guint32*& )hPort );

        if( SUCCEEDED( ret ) && hPort != hThis )
        {
            PortPtr pTopmost =
                pThis->GetTopmostPort();

            PortPtr pTargetPort = pTopmost;
            do{
                HANDLE hVal =
                    PortToHandle( pTargetPort );
                if( hVal == hPort )
                    break;
                pTargetPort =
                    pTargetPort->GetLowerPort();

            }while( !pTargetPort.IsEmpty() );

            if( pTargetPort.IsEmpty() )
                ret = -EFAULT;
            else
                pPort = pTargetPort;

            break;
        }
        else if( SUCCEEDED( ret ) )
        {
            pPort = pThis;
            break;
        }

        ret = 0;
        if( pThis->GetPortType() ==
            PORTFLG_TYPE_PDO )
        {
            pPort = pThis;
            break;
        }

        pPort = pThis->GetBottomPort(); 

    }while( 0 );

    return ret;
}

gint32 CRpcBaseOperations::StartRecvMsg(
    IEventSink* pCompletion,
    IMessageMatch* pMatch )
{
    if( pCompletion == nullptr ||
        pMatch == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{

        IrpPtr pIrp;

        CIoManager* pMgr = GetIoMgr();
        CCfgOpenerObj oTaskCfg(
            ( CObjBase* ) pCompletion );

        //NOTE: complicated logic alert
        ret = pIrp.NewObj();
        if( ERROR( ret ) )
            break;

        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oMatchCfg( pMatch );

        string strIfName;
        ret = oMatchCfg.GetStrProp(
            propIfName, strIfName );

        if( ERROR( ret ) )
            break;

        if( strIfName == IFNAME_INVALID )
        {
            // this match is not for message
            // filtering
            ret = 0;
            break;
        }

        string strObjPath;
        string strObjPath2;

        ret = oMatchCfg.GetStrProp(
            propObjPath, strObjPath );

        if( ERROR( ret ) )
            break;

        strObjPath2 = DBUS_OBJ_PATH(
            pMgr->GetModName(), OBJNAME_INVALID );

        // an invalid obj path
        if( strObjPath == strObjPath2 )
        {
            // this match is not for message
            // filtering
            ret = 0;
            break;
        }

        CReqBuilder oReq( this );
        oReq.SetMethodName( IF_METHOD_LISTENING );

        oReq.SetCallFlags( CF_WITH_REPLY |
            CF_ASYNC_CALL |
            DBUS_MESSAGE_TYPE_METHOD_CALL );

        oReq.SetObjPtr(
            propMatchPtr, ObjPtr( pMatch ) );

        ret = SetupReqIrp( pIrp,
            oReq.GetCfg(), pCompletion );

        if( ERROR( ret ) )
            break;

        // set the completion thread
        pIrp->SetIrpThread( pMgr ); 
        pIrp->SetSyncCall( false );
        pIrp->RemoveTimer();

        // make a copy of the irp for canceling
        oTaskCfg.SetObjPtr(
            propIrpPtr, ObjPtr( pIrp ) );

        PortPtr pPort;
        ret = GetPortToSubmit( pCompletion, pPort );
        if( ERROR( ret ) )
            break;

        ret = pMgr->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) )
            break;

        // clear the irp reference
        TaskletPtr pTask;
        pTask = ObjPtr( pCompletion );
        if( !pTask.IsEmpty() )
        {
            pTask->RemoveProperty(
                propIrpPtr );

            if( pTask->GetClsid() ==
                clsid( CIfStartRecvMsgTask ) )
            {
                // mark the task to have done with the
                // IO
                IConfigDb* pTaskCfg =
                    pTask->GetConfig();
                CCfgOpener oTaskCfg( pTaskCfg );
                oTaskCfg.SetIntProp(
                    propTaskState, stateIoDone );
            }
        }

        if( pIrp->m_pCallback.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        ThreadPtr pThrd;
        ret = pIrp->GetIrpThread( pThrd );
        if( ERROR( ret ) )
        {
            // in case some server does not have
            // dedicated irp completion thread 
            CCfgOpener oCfg;
            oCfg.SetPointer( propIrpPtr,
                ( PIRP )pIrp );

            ret = pMgr->ScheduleTask(
                clsid( CIoMgrIrpCompleteTask ),
                oCfg.GetCfg() );

            if( SUCCEEDED( ret ) )
                ret = STATUS_PENDING;

            break;
        }

        pIrp->ClearIrpThread();

        CIrpCompThread* pIrpThrd = pThrd;
        // complete the irp in the irp completion
        // thread
        ret = pIrpThrd->AddIrp( pIrp );
        if( ERROR( ret ) )
        {
            pIrp->RemoveCallback();
            break;
        }

        // tell the tasklet to wait for the irp
        // completion
        ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}

gint32 CRpcBaseOperations::CancelRecvMsg()
{
    // not a serious cancel if more than one
    // pending irps
    if( m_pIrp.IsEmpty() )
        return 0;

    gint32 ret = GetIoMgr()->CancelIrp(
        m_pIrp, true, -ECANCELED );

    return ret;
}

gint32 CRpcBaseOperations::PauseResumeInternal(
    EnumEventId iEvent, IEventSink* pCallback )
{
    return m_pIfStat->OnAdminEvent(
        iEvent, string( "" ) );
}

gint32 CRpcBaseOperations::DoModEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    return m_pIfStat->OnModEvent(
        iEvent, strModule );
}

gint32 CRpcBaseOperations::DoRmtModEvent(
    EnumEventId iEvent,
    const std::string& strModule,
    IConfigDb* pEvtCtx )
{

    return m_pIfStat->OnRmtModEvent(
        iEvent, strModule, pEvtCtx );
}

gint32 CRpcBaseOperations::DoPause(
    IEventSink* pCallback )
{
    return PauseResumeInternal(
        cmdPause, pCallback );
}

gint32 CRpcBaseOperations::DoResume(
    IEventSink* pCallback )
{
    return PauseResumeInternal(
        cmdResume, pCallback );
}

gint32 CRpcInterfaceBase::Start()
{
    TaskletPtr pCallback;
    gint32 ret = 0;
    do{
        ret = SetStateOnEvent( cmdOpenPort );
        if( ERROR( ret ) )
            break;

        ret = pCallback.NewObj(
            clsid( CSyncCallback ) );

        if( ERROR( ret ) )
            break;

        ret = StartEx( pCallback );
        if( ERROR( ret ) )
            break;

        if( ret == STATUS_PENDING )
        {
            CSyncCallback* pSyncCb = pCallback;
            if( pSyncCb == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            ret = pSyncCb->WaitForComplete();
            if( SUCCEEDED( ret ) )
                ret = pSyncCb->GetError();
        }

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceBase::GetParallelGrp(
    TaskGrpPtr& pParaGrp )
{
    TaskGrpPtr pRootTaskGrp;
    CIfRootTaskGroup* pRootGrp = nullptr;

    if( true )
    {
        pRootTaskGrp = GetTaskGroup();
        if( pRootTaskGrp.IsEmpty() )
            return -ENOENT;

        // as a guard to the root group
        pRootGrp = pRootTaskGrp;
    }

    gint32 ret = 0;

    do{
        std::vector< TaskletPtr > vecTasks;
        ret = pRootGrp->FindTaskByClsid(
            clsid( CIfParallelTaskGrp ),
            vecTasks );
        if( ERROR( ret ) )
            break;

        // return the first one
        pParaGrp = vecTasks.front();

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceBase::StartRecvTasks(
    std::vector< MatchPtr >& vecMatches )
{
    gint32 ret = 0;

    if( vecMatches.empty() )
        return 0;

    do{
        // we cannot add the recvtask by AddAndRun
        // at this point, because it requires the
        // interface to be in the connected state,
        // while we may still in the starting or
        // stopped state
        CParamList oParams;
        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret  ) )
            break;

        TaskletPtr pRecvMsgTask;
        for( auto pMatch : vecMatches )
        {
            oParams[ propMatchPtr ] =
                ObjPtr( pMatch );

            ret = pRecvMsgTask.NewObj(
                clsid( CIfStartRecvMsgTask ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            ret = AddAndRun( pRecvMsgTask );
            if( ERROR( ret ) )
                break;

            if( ret == STATUS_PENDING )
                ret = 0;
        }

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CIfStartExCompletion::OnTaskComplete(
    gint32 iRet )
{
    if( ERROR( iRet ) )
        return iRet;

    gint32 ret = 0;
    do{
        CParamList oParams(
            ( IConfigDb*)GetConfig() );

        CRpcInterfaceBase* pIf = nullptr;

        ret = oParams.GetPointer(
            propIfPtr, pIf );

        if( ERROR( ret ) )
            break;

        if( !pIf->IsConnected() )
        {
            // FIXME: if we need to return ENOTCONN to
            // indicate the status?
            break;
        }

        intptr_t ptrMatches = oParams[ 0 ];
        std::vector< MatchPtr >* pMatches =
            ( std::vector< MatchPtr >* ) ptrMatches;

        if( unlikely( pMatches == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        ret = pIf->StartRecvTasks( *pMatches );

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceBase::StartEx(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    TaskGrpPtr pTaskGrp;

    if( pCallback == nullptr )
        return -EINVAL;

    do{
        TaskletPtr pOpenPortTask;
        TaskletPtr pEnableEvtTask;

        EnumIfState iState = GetState();

        if( iState != stateStarting )
        {
            ret = ERROR_STATE;
            break;
        }

        CParamList oParams;

        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        ret = m_pRootTaskGroup.NewObj(
            clsid( CIfRootTaskGroup ),
            oParams.GetCfg() ); 

        if( ERROR( ret ) )
            break;

        // non-IO interface
        ret = oParams.CopyProp(propNoPort, this );
        if( SUCCEEDED( ret ) )
        {
            ret = SetStateOnEvent(
                eventPortStarted );
            break;
        }

        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = pOpenPortTask.NewObj(
            clsid( CIfOpenPortTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        pTaskGrp->AppendTask( pOpenPortTask );

        // give a chance for running of customized
        // tasks immediately after the port is opened
        AddStartTasks( ( IEventSink* )pTaskGrp );

        // enable or disable event
        oParams.Push( true );

        for( auto pMatch : m_vecMatches )
        {
            oParams[ propMatchPtr ] =
                ObjPtr( pMatch );

            ret = pEnableEvtTask.NewObj(
                clsid( CIfEnableEventTask ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            pTaskGrp->AppendTask( pEnableEvtTask );
        }
        pTaskGrp->SetRelation( logicAND );

        CParamList oTaskParam;
        TaskletPtr pStartComp;
        if( pCallback != nullptr )
        {
            oTaskParam[ propEventSink ] =
                ObjPtr( pCallback );
        }

        oTaskParam[ propIfPtr ] =
            ObjPtr( this );

        ret = oTaskParam.Push(
            ( intptr_t )&m_vecMatches );

        if( ERROR( ret ) )
            break;

        ret = pStartComp.NewObj(
            clsid( CIfStartExCompletion ),
            oTaskParam.GetCfg() );

        if( ERROR( ret ) )
            break;

         ( *pStartComp )( eventZero );

        ret = pTaskGrp->SetClientNotify(
            pStartComp );

        if( ERROR( ret ) )
            break;

        TaskletPtr pTask = pTaskGrp;
        if( pTask.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        ret = AppendAndRun( pTask );
        if( ERROR( ret ) )
        {
            ( *pTask )( eventCancelTask );
            break;
        }

        ret = pTask->GetError();
        if( ret == STATUS_PENDING )
            break;

        if( SUCCEEDED( ret ) && IsConnected() )
            ret = StartRecvTasks( m_vecMatches );

    }while( 0 );

    if( ret == STATUS_MORE_PROCESS_NEEDED )
        ret = STATUS_PENDING;

    if( ERROR( ret ) )
    {
        ClearActiveTasks();
        m_pRootTaskGroup.Clear();
    }

    return ret;
}

gint32 CRpcInterfaceBase::Stop()
{
    gint32 ret = 0;
    do{
        EnumEventId iEvent = cmdShutdown;
        EnumIfState iState = GetState();
        if( iState == stateStartFailed )
            iEvent = cmdCleanup;

        ret = SetStateOnEvent( iEvent );
        if( ERROR( ret ) )
            break;

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CSyncCallback ) );

        if( ERROR( ret ) )
            break;

        IEventSink* pCallback = pTask;
        ret = StopEx( pCallback );

        if( ret == STATUS_PENDING )
        {
            CSyncCallback* pSyncTask = pTask;
            ret = pSyncTask->WaitForComplete();
            if( SUCCEEDED( ret ) )
                ret = pSyncTask->GetError();
        }

    }while( 0 );
    
    return ret;
}

gint32 CRpcInterfaceBase::StopEx(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
#ifdef DEBUG
    {
        DebugPrint( ret, "interface stopping" );
        string strDump;
        this->Dump( strDump );
        DebugPrint( ret, strDump );
    }
#endif
    do{
        CStdRMutex oIfLock( GetLock() );
        EnumIfState iState = GetState();
        if( iState == stateStartFailed )
        {
            ret = SetStateOnEvent( cmdCleanup );
            if( ERROR( ret ) )
                break;
        }
        else if( iState != stateStopping )
        {
            ret = SetStateOnEvent( cmdShutdown );
            if( ERROR( ret ) )
                break;
        }

        if( stateStopping != GetState() )
            return ERROR_STATE;

        TaskGrpPtr pRoot = m_pRootTaskGroup;
        if( pRoot.IsEmpty() )
        {
            CParamList oParams;
            ret = oParams.SetObjPtr(
                propIfPtr, ObjPtr( this ) );

            if( ERROR( ret ) )
                break;

            ret = m_pRootTaskGroup.NewObj(
                clsid( CIfRootTaskGroup ),
                oParams.GetCfg() ); 

            if( ERROR( ret ) )
                break;
        }
        oIfLock.Unlock();

        TaskletPtr pPreStop;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pPreStop, ObjPtr( this ),
            &CRpcInterfaceBase::OnPreStop,
            ( IEventSink* )nullptr );

        if( ERROR( ret ) )
            break;

        TaskletPtr pDoStop;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pDoStop, ObjPtr( this ),
            &CRpcInterfaceBase::DoStop,
            ( IEventSink* )nullptr );

        if( ERROR( ret ) )
            break;

        CParamList oParams;
        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        TaskGrpPtr pStopTasks;
        ret = pStopTasks.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        pStopTasks->AppendTask( pPreStop );
        pStopTasks->AppendTask( pDoStop );
        pStopTasks->SetRelation( logicNONE );

        if( pCallback != nullptr )
        {
            CIfRetryTask* pRetryTask = pStopTasks;
            // in case the DoStop completed immediately
                pRetryTask->SetClientNotify( pCallback );
        }

        CIoManager* pMgr = GetIoMgr();
        TaskletPtr pStopGrp = ObjPtr( pStopTasks );
        ret = pMgr->RescheduleTask( pStopGrp );
        if( ERROR( ret ) )
        {
            ( *pStopGrp )( eventCancelTask );
        }
        else
        {
            ret = STATUS_PENDING;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceBase::OnPreStop(
    IEventSink* pCallback )
{ return 0; }

gint32 CRpcInterfaceBase::DoStop(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    TaskGrpPtr pTaskGrp;
    TaskletPtr pCompletion;

    if( pCallback == nullptr )
        return -EINVAL;

    do{
        TaskletPtr pClosePortTask;
        TaskletPtr pDisableEvtTask;

        if( stateStopping != GetState() )
            return ERROR_STATE;

        ClearActiveTasks();
        CParamList oParams;
        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = pClosePortTask.NewObj(
            clsid( CIfStopTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        HANDLE hPort = GetPortHandle();
        if( hPort != INVALID_HANDLE )
        {
            // NOTE that, when stopping, we may
            // have a partially initialized
            // interface due to a failed start
            //
            // disable event
            oParams.Push( false );
            // Change Interface state or not
            oParams.Push( false );

            for( auto pMatch : m_vecMatches )
            {
                oParams[ propMatchPtr ] =
                    ObjPtr( pMatch );

                ret = pDisableEvtTask.NewObj(
                    clsid( CIfEnableEventTask ),
                    oParams.GetCfg() );

                if( ERROR( ret ) )
                    break;

                pTaskGrp->AppendTask( pDisableEvtTask );
            }

            if( ERROR( ret ) )
                break;
        }
        AddStopTasks( pTaskGrp );
        pTaskGrp->AppendTask( pClosePortTask );
        pTaskGrp->SetRelation( logicNONE );

        CCfgOpener oCompCfg;
        ret = oCompCfg.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        ret = pCompletion.NewObj(
            clsid( CIfCleanupTask ),
            oCompCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        if( pCallback )
        {
            CIfRetryTask* pCompTask = pCompletion;
            pCompTask->SetClientNotify( pCallback );
            pCompTask->MarkPending();
        }

        pTaskGrp->SetClientNotify( pCompletion );
        TaskletPtr pTask = pTaskGrp;
        if( unlikely( pTask.IsEmpty() ) )
        {
            ret = -EFAULT;
            break;
        }

        ret = AppendAndRun( pTask );
        if( ERROR( ret ) )
            break;

        ret = pTask->GetError();
        if( ret != STATUS_PENDING )
        {
            // the callback was not called
            pCompletion->MarkPending( false );
            ( *pCompletion )( eventZero );
        }

    }while( 0 );


    if( ret == STATUS_MORE_PROCESS_NEEDED )
    {
        ret = STATUS_PENDING;
    }

    return ret;
}

gint32 CRpcInterfaceBase::PauseResume(
    bool bResume, IEventSink* pCallback )
{
    gint32 ret = 0;
    TaskletPtr pTask;

    do{
        CParamList oParams;
        oParams.Push( bResume );

        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        ret = pTask.NewObj(
            clsid( CIfPauseResumeTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        CIfRetryTask* pRetryTask = pTask;
        pRetryTask->SetClientNotify(
            pCallback );

        // force to run on this thread
        ret = AddAndRun( pTask, true );
        if( ERROR( ret ) )
            break;

        ret = pTask->GetError();

    }while( 0 );

    if( ret == STATUS_MORE_PROCESS_NEEDED )
        ret = STATUS_PENDING;

    return ret;
}

gint32 CRpcInterfaceBase::Resume(
    IEventSink* pCallback )
{
    gint32 ret = SetStateOnEvent( cmdResume );
    if( ERROR( ret ) )
        return ret;
    // the interface is in Resuming state
    return PauseResume( true, pCallback );
}

gint32 CRpcInterfaceBase::Pause(
    IEventSink* pCallback )
{
    gint32 ret = SetStateOnEvent( cmdPause );
    if( ERROR( ret ) )
        return ret;
    // the interface is in Pausing state
    return PauseResume( false, pCallback );
}

gint32 CRpcInterfaceBase::Shutdown(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    EnumEventId iEvent = cmdShutdown;
    EnumIfState iState = GetState();
    if( iState == stateStartFailed )
        iEvent = cmdCleanup;

    ret = SetStateOnEvent( iEvent );

    if( ERROR( ret ) )
        return ret;

    return StopEx( pCallback );
}

gint32 CRpcInterfaceBase::OnPortEvent(
    EnumEventId iEvent,
    HANDLE hPort )
{
    gint32 ret = 0;

    if( !IsMyPort( hPort ) )
        return -EINVAL;

    switch( iEvent )
    {
    case eventPortStopping:
        {
            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CIfDummyTask ) );

            ret = SetStateOnEvent( iEvent );
            if( ERROR( ret ) )
                break;

            // we need an asynchronous call
            ret = StopEx( pTask );
            break;
        }
    case eventPortStarted:
        {
            // called from CIfOpenPortTask
            EnumIfState iState = GetState();

            ret = m_pIfStat->OnPortEvent(
                iEvent, hPort );

            if( ERROR( ret ) )
                break;

            AddPortProps();

            if( IsServer() &&
                iState == stateStarting )
            {
                CCfgOpenerObj oIfCfg( this );
                bool bPauseOnStart;

                ret = oIfCfg.GetBoolProp(
                    propPauseOnStart, bPauseOnStart );

                if( ERROR( ret ) )
                {
                    bPauseOnStart = false;
                    ret = 0;
                }

                if( bPauseOnStart )
                {
                    ret = SetStateOnEvent(
                        eventPaused );
                }
            }
            break;
        }
    case eventPortStartFailed:
    case eventPortStopped:
    default:
        {
            ret = m_pIfStat->OnPortEvent(
                iEvent, hPort );
            break;
        }
    }

    return ret;
}

gint32 CRpcInterfaceBase::OnModEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    gint32 ret = 0;

    do{
        TaskletPtr pTask;

        if( !IsMyDest( strModule ) )
            break;

        if( TestSetState( iEvent )
            == ERROR_STATE )
            break;

        CParamList oParams;
        ret = oParams.Push( iEvent );

        if( ERROR( ret ) )
            break;

        oParams.Push( strModule );

        if( ERROR( ret ) )
            break;

        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret  ) )
            break;

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        ret = pTask.NewObj(
            clsid( CIfCpEventTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ( ( CIfRetryTask* )pTask )->
            SetClientNotify( pDummyTask );

        ret = AppendAndRun( pTask );

    }while( 0 );

    return ret;
}

#define FILTER_RMT_EVT( _pEvtCtx ) \
do{ \
    CCfgOpenerObj oCfg( this ); \
    ret = oCfg.IsEqualProp( \
        propConnHandle, _pEvtCtx ); \
    if( ERROR( ret ) ) \
        break; \
    ret = oCfg.IsEqualProp( \
        propRouterPath, _pEvtCtx ); \
    if( ERROR( ret ) ) \
        break; \
}while( 0 )

/**
* @name OnRmtModEvent: Works only if the interface
* connecting to the remote server, otherwise this
* event will be filtered out.
* @{ */
/**  @} */

gint32 CRpcInterfaceBase::OnRmtModEvent(
    EnumEventId iEvent,
    const std::string& strModule,
    IConfigDb* pEvtCtx )
{
    gint32 ret = 0;

    FILTER_RMT_EVT( pEvtCtx );
    if( ERROR( ret ) )
        return ret;

    do{
        TaskletPtr pTask;

        if( !IsMyDest( strModule ) )
            break;

        if( TestSetState( iEvent )
            == ERROR_STATE )
            break;

        CParamList oParams;
        oParams.Push( iEvent );
        oParams.Push( strModule );
        oParams.Push( pEvtCtx );

        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret  ) )
            break;

        TaskletPtr pDummyTask;
        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        ret = pTask.NewObj(
            clsid( CIfCpEventTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ( ( CIfRetryTask* )pTask )->
            SetClientNotify( pDummyTask );

        ret = AppendAndRun( pTask );

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceBase::OnDBusEvent(
    EnumEventId iEvent )
{
    gint32 ret = 0;

    ret = SetStateOnEvent( iEvent );
    if( ERROR( ret ) )
        return ret;

    switch( iEvent )
    {
    case eventDBusOffline:
        {
            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CIfDummyTask ) );

            if( ERROR( ret ) )
                break;

            ret = DEFER_CALL(
                GetIoMgr(), this,
                &CRpcInterfaceBase::StopEx,
                ( IEventSink* )pTask );
                
            break;
        }
    case eventDBusOnline:
        {
            // no effect
            break;
        }
    default:
        {
            ret = -ENOTSUP;
        }
    }
    return ret;
}

gint32 IsMidwayPath(
    const std::string& strTest,
    const std::string& strDest )
{
    gint32 ret = STATUS_SUCCESS;
    do{
        if( strTest == strDest || strTest == "/" )
            break;

        if( strTest.size() > strDest.size() ||
            strTest > strDest )
        {
            // NOTE: trailing '/', such as '/xxx/'
            // is not allowed
            ret = ERROR_FALSE;
            break;
        }
        if( strDest.substr(
            0, strTest.size() ) != strTest )
        {
            ret = ERROR_FALSE;
            break;
        }
        if( strTest.size() < strDest.size() )
        {
            if( strDest[ strTest.size() ] != '/' )
                ret = ERROR_FALSE;
        }

    }while( 0 );

    return ret;
}

#define FILTER_RMT_EVT2( _pEvtCtx ) \
do{ \
    CCfgOpenerObj oCfg( this ); \
    ret = oCfg.IsEqualProp( \
        propConnHandle, _pEvtCtx ); \
    if( ERROR( ret ) ) \
        break; \
    CCfgOpener oEvtCtx( _pEvtCtx ); \
    std::string strVal1, strVal2; \
    ret = oEvtCtx.GetStrProp( \
        propRouterPath, strVal1 ); \
    if( ERROR( ret ) ) \
        break; \
    ret = oCfg.GetStrProp( \
        propRouterPath, strVal2 ); \
    if( ERROR( ret ) )\
        break; \
    ret = IsMidwayPath( strVal1, strVal2 ); \
}while( 0 )

/**
* @name OnRmtSvrEvent: Works only if the interface
* connects to a remote server. otherwise, it is
* filtered out. However, it does not hold if you
* override this method, such as those objects in
* the router.
* @{ */
/**  @} */

gint32 CRpcInterfaceBase::OnRmtSvrEvent(
    EnumEventId iEvent,
    IConfigDb* pEvtCtx,
    HANDLE hPort )
{
    gint32 ret = 0;

    if( pEvtCtx == nullptr )
        return -EINVAL;

    FILTER_RMT_EVT2( pEvtCtx );
    if( ERROR( ret ) )
        return ret;

    switch( iEvent )
    {
    case eventRmtDBusOffline:
    case eventRmtSvrOffline:
        {
            OnDBusEvent( eventDBusOffline );
            break;
        }
    case eventRmtDBusOnline:
    case eventRmtSvrOnline:
        {
            OnDBusEvent( eventDBusOnline );
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

gint32 CRpcInterfaceBase::OnAdminEvent(
    EnumEventId iEvent,
    const std::string& strParams )
{
    gint32 ret = 0;

    // to avoid synchronous call
    // we put  dumb task there
    TaskletPtr pTask;
    ret = pTask.NewObj(
        clsid( CIfDummyTask ) );

    if( ERROR( ret ) )
        return ret;

    switch( iEvent )
    {
    case cmdShutdown:
        {
            ret = SetStateOnEvent( iEvent );
            if( ERROR( ret ) )
                break;

            ret = StopEx( pTask );
            break;
        }
    case cmdPause:
        {
            if( !IsServer() )
                break;

            ret = Pause( pTask );
            break;
        }
    case cmdResume:
        {
            if( !IsServer() )
                break;

            ret = Resume( pTask );
            break;
        }
    default:
        ret = -ENOTSUP;

    }
    return ret;
}

gint32 CRpcInterfaceBase::OnEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData )
{
    gint32 ret = 0;

    switch( iEvent )
    {
    case eventPortStarted:
    case eventPortStopping:
    case eventPortStartFailed:
    case eventPortStopped:
        {
            ret = OnPortEvent( iEvent, dwParam1 );
#ifdef DEBUG
            if( ERROR( ret ) )
            {
                HANDLE hPort = dwParam1;
                IPort* pPort = HandleToPort( hPort );
                if( pPort != nullptr )
                {
                    string strDump;
                    pPort->Dump( strDump );
                    DebugPrint( ret, strDump );
                }
            }
#endif
            break;
        }
    case eventModOnline:
    case eventModOffline:
        {
            string strMod = reinterpret_cast
                < const char* >( pData );

            bool bInterested = false;
            if( dwParam1 & MOD_ONOFFLINE_IRRELEVANT )
                bInterested = true;

            // for proxy, filter off those irrelevant
            // on/offline events
            if( !IsServer() && !bInterested )
                return 0;

            ret = OnModEvent( iEvent, strMod ); 
            break;
        }
    case eventRmtModOnline:
    case eventRmtModOffline:
        {
            string strMod = reinterpret_cast
                < const char* >( pData );

            IConfigDb* pEvtCtx = reinterpret_cast
                < IConfigDb* >( dwParam1 );

            ret = OnRmtModEvent(
                iEvent, strMod, pEvtCtx ); 

            break;
        }
    case eventDBusOnline:
    case eventDBusOffline:
        {
            ret = OnDBusEvent( iEvent );
            break;
        }
    case eventRmtDBusOffline:
    case eventRmtDBusOnline:
    case eventRmtSvrOnline:
    case eventRmtSvrOffline:
        {
            HANDLE hPort = ( HANDLE )pData;
            IConfigDb* pEvtCtx = reinterpret_cast
                < IConfigDb* >( dwParam1 );

            ret = OnRmtSvrEvent(
                iEvent, pEvtCtx, hPort ); 
            break;
        }
    case cmdPause:
    case cmdResume:
    case cmdShutdown:
        {
            ret = OnAdminEvent(
                iEvent, string( "" ) );
            break;
        }
    default:
        {
            ret = -ENOTSUP;
            break;
        }
    }

#ifdef DEBUG
    if( ERROR( ret ) )
    {
        DebugPrint( ret,
            "Error occured on Event %d, %s@0x%x",
            iEvent,
            CoGetClassName( GetClsid() ),
            this );
        
    }
#endif
    return ret;
}

gint32 CRpcInterfaceBase::ClearActiveTasks()
{
    gint32 ret = 0;

    do{
        TaskGrpPtr pRoot = m_pRootTaskGroup;
        if( pRoot.IsEmpty() )
            break;

        ret = ( *pRoot )( eventCancelTask );
        if( ret == STATUS_PENDING )
        {
            // sometimes, the system is busy and
            // the root task group could also runs
            // on another thread, wait for it to
            // be available
            usleep( 1000 );
            continue;
        }
        break;

    }while( 1 );

    return ret;
}


gint32 CRpcInterfaceBase::SetReqQueSize(
    IMessageMatch* pMatch, guint32 dwSize )
{
    gint32 ret = 0;
    if( pMatch == nullptr )
        return -EINVAL;

    if( dwSize > MAX_DBUS_REQS )
        return -EINVAL;

    do{
        CIoManager* pMgr = GetIoMgr();
        IrpPtr pIrp;

        ret = pIrp.NewObj();
        if( ERROR( ret ) )
            break;

        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        guint32 dwCtrlCode = CTRLCODE_SET_REQUE_SIZE;
        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 

        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
        pIrpCtx->SetCtrlCode( dwCtrlCode );
        pIrpCtx->SetIoDirection( IRP_DIR_OUT ); 

        CParamList oParams;
        oParams[ propQueSize ] = dwSize;
        oParams[ propMatchPtr ] = ObjPtr( pMatch );

        BufPtr pBuf( true );
        *pBuf = oParams.GetCfg();
        pIrpCtx->SetReqData( pBuf );

        // an immediate request
        ret = pMgr->SubmitIrp(
            GetPortHandle(), pIrp );

    }while( 0 );

    return ret;
}
// clear those CIfStartRecvMsgTask on the paused
// interfaces
gint32 CRpcInterfaceBase::ClearPausedTasks()
{
    gint32 ret = 0;
    do{
        TaskletPtr pTask;
        TaskGrpPtr pRootGrp = m_pRootTaskGroup;
        if( pRootGrp.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }
        CIfRootTaskGroup* pRoot = m_pRootTaskGroup;

        ret = pRoot->GetHeadTask( pTask );
        if( ERROR( ret ) )
            break;

        CIfParallelTaskGrp* pParaTaskGrp = pTask;
        if( unlikely( pParaTaskGrp == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }
        std::vector< TaskletPtr > vecTasks;
        ret = pParaTaskGrp->FindTaskByClsid(
            clsid( CIfStartRecvMsgTask ),
            vecTasks );
        if( ERROR( ret ) )
            break;

        for( auto elem : vecTasks )
        {
            CCfgOpenerObj oTaskCfg(
                ( CObjBase* )elem );

            CMessageMatch* pMatch;
            ret = oTaskCfg.GetPointer(
                propMatchPtr, pMatch );

            if( ERROR( ret ) )
                continue;

            bool bPausable = false;
            CCfgOpenerObj oMatchCfg( pMatch );
            ret = oMatchCfg.GetBoolProp(
                propPausable, bPausable );

            if( ERROR( ret ) )
                continue;

            if( bPausable )
            {
                SetReqQueSize( pMatch, 0 );
            }
        }

    }while( 0 );

    return ret;
}

gint32 CRpcInterfaceBase::AppendAndRun(
    TaskletPtr& pTask )
{
    if( pTask.IsEmpty() )
        return -EINVAL;

    // possibly the initialization failed
    TaskGrpPtr pRoot = m_pRootTaskGroup;
    if( pRoot.IsEmpty() )
        return -EFAULT;

    return pRoot->AppendAndRun( pTask );
}

gint32 CRpcInterfaceBase::AddAndRun(
    TaskletPtr& pIoTask, bool bImmediate )
{
    // usually I/O tasks are the task that will
    // run concurrently parallelly
    if( pIoTask.IsEmpty() )
        return -EINVAL;

    TaskGrpPtr pRootGrp;
    CIfRootTaskGroup* pRootTaskGroup = nullptr;
    if( true )
    {
        pRootTaskGroup = GetTaskGroup();
        if( pRootTaskGroup == nullptr )
            return -EFAULT;

        // as a guard to the root group
        pRootGrp = pRootTaskGroup;
    }

    gint32 ret = 0;

    do{
        TaskletPtr pTail;
        bool bTail = false;

        CStdRTMutex oRootLock(
            pRootTaskGroup->GetLock() );

        guint32 dwCount =
            pRootTaskGroup->GetTaskCount();

        ret = pRootTaskGroup->
            GetTailTask( pTail );

        stdrtmutex* pParaLock = nullptr;
        CIfParallelTaskGrp* pParaGrp = nullptr;
        if( SUCCEEDED( ret ) )
        {
            pParaGrp = pTail;
            if( pParaGrp != nullptr )
                bTail = true;
        }

        if( bTail )
        {
            oRootLock.Unlock();
            pParaLock = &pParaGrp->GetLock();
            pParaLock->lock();
            EnumTaskState iTaskState =
                pParaGrp->GetTaskState();
            if( iTaskState == stateStopped ||
                ( iTaskState == stateStarted &&
                    !pParaGrp->IsRunning() ) )
            {
                // the group has stopped
                bTail = false;
                oRootLock.Lock();
                pParaLock->unlock();
                // recheck if the taskgroup
                // changed during the period the
                // lock is released
                dwCount =
                    pRootTaskGroup->GetTaskCount();
                TaskletPtr pTail1;
                ret = pRootTaskGroup->
                    GetTailTask( pTail1 );
                if( SUCCEEDED( ret ) )
                {
                    if( pTail1 != pTail )
                    {
                        // something changed
                        pParaGrp = pTail1;
                        if( pParaGrp != nullptr )
                            continue;
                        // a non-paragrp added
                        // fall through
                    }
                }
                else
                {
                    ret = 0;
                }
            }
        }

        TaskletPtr pParaTask;
        if( bTail )
        {
            pParaTask = pTail;
        }
        else
        {
            // add a new parallel task group
            CCfgOpener oCfg;

            ret = oCfg.SetObjPtr(
                propIfPtr, ObjPtr( this ) );

            if( ERROR( ret  ) )
                break;

            ret = pParaTask.NewObj(   
                clsid( CIfParallelTaskGrp ),
                oCfg.GetCfg() );

            if( ERROR( ret ) )
                break;
                
            ret = pRootTaskGroup->
                AppendTask( pParaTask );
            if( ERROR( ret ) )
            {
                DebugPrint( ret, "Fatal error, "
                "can not add parallel task group" );
                break;
            }

            pParaGrp = pParaTask;
            oRootLock.Unlock();

            pParaLock= &pParaGrp->GetLock();
            pParaLock->lock();

            EnumTaskState iTaskState =
                pParaGrp->GetTaskState();

            if( iTaskState == stateStopped ||
                ( iTaskState == stateStarted &&
                    !pParaGrp->IsRunning() ) )
            {
                pParaLock->unlock();
                continue;
            }
        }

        // oRootLock.Lock();

        bool bRunning = pParaGrp->IsRunning();

        // add the task to the pending queue or run it
        // immediately
        //
        // immediate  running  dwcount
        //     0       0       0       run root
        //     0       0       1       pending
        //     0       1       0       error
        //     0       1       1       run root
        //     1       0       0       run root
        //     1       0       1       TBD
        //     1       1       0       error
        //     1       1       1       run direct
        if( !bImmediate || ( !bRunning && dwCount == 0 ) )
        {
            ret = pParaGrp->AppendTask( pIoTask );
            if( ERROR( ret ) )
            {
                pParaLock->unlock();
                break;
            }

            // actually the taskgroup can also run on
            // the parallel taskgroup
            CIfRetryTask* pRetryTask = pIoTask;
            if( unlikely( pRetryTask == nullptr ) )
            {
                pParaLock->unlock();
                ret = -EFAULT;
                break;
            }

            if( dwCount == 0 && bRunning )
            {
                pParaLock->unlock();
                ret = ERROR_STATE;
                DebugPrint( GetTid(),
                    "root task is in wrong state, dwCount=%d, bRunning=%d",
                    dwCount, bRunning );
                break;
            }
            else if( dwCount > 0 && !bRunning )
            {
                // don't lock the io task, because it
                // could cause deadlock
                pIoTask->MarkPending();
                pParaLock->unlock();
                ret = 0;

                DebugPrintEx( logInfo, GetTid(),
                    "root task not run immediately, dwCount=%d, bRunning=%d",
                    dwCount, bRunning );
            }
            else
            {
                pParaLock->unlock();

                // NOTE: there could be deep nesting.
                // Reschedule can fix, but performance
                // hurts
                //
                // FIXME: if more than one threads
                // are running `para group'
                // simultaneously, ERROR_STATE
                // could be returned when the task
                // appended on current thread are
                // run and completed by other
                // thread, though the tasks should
                // succeed as normal.
                ( *pParaGrp )( eventZero );
                ret = pIoTask->GetError();
                if( ret == STATUS_PENDING )
                {
                    // in case the io task is scheduled
                    // on the this thread.
                    pIoTask->MarkPending();
                }
                ret = 0;
            }
        }
        else if( bImmediate && bRunning && dwCount > 0 )
        {
            pParaGrp->AppendTask( pIoTask );
            pParaLock->unlock();

            // force the task to run immediately
            ( *pParaGrp )( eventZero );
            ret = pIoTask->GetError();
            if( ret == STATUS_PENDING )
                pIoTask->MarkPending();
            ret = 0;
        }
        else if( bImmediate && !bRunning && dwCount > 0 )
        {
            EnumTaskState iTaskState =
                pParaGrp->GetTaskState();
            if( iTaskState == stateStarting )
            {
                // the old pParaGrp is about to
                // quit, the new pParaGrp is
                // waiting.
                pParaGrp->AppendTask( pIoTask );
                pIoTask->MarkPending();
                pParaLock->unlock();
                ret = STATUS_PENDING;
                DebugPrint( GetTid(),
                    "old paragrp is about to quit,"
                    "dwCount=%d, bRunning=%d",
                    dwCount, bRunning );
            }
            else
            {
                pParaLock->unlock();
                ret = ERROR_STATE;
                DebugPrint( GetTid(),
                    "failed to run task immediately,"
                    "dwCount=%d, bRunning=%d",
                    dwCount, bRunning );
            }
        }
        else
        {   // bImmediate && bRunning && dwCount == 0
            pParaLock->unlock();
            ret = ERROR_STATE;
        }

        break;

    }while( 1 );

    // don't rely on `ret' to determine the return
    // value from the task pIoTask, you need to
    // get the return value from
    // pIoTask->GetError() in case the task is
    // scheduled to run later
    return ret;
}

gint32 CRpcServices::GetIidFromIfName(
    const std::string& strIfName,
    EnumClsid& iid )
{
    string strIfName2 =
        IF_NAME_FROM_DBUS( strIfName );

    string strSuffix = IsServer() ? "s" : "p";
    iid = CoGetIidFromIfName(
        strIfName2, strSuffix );

    if( iid == clsid( Invalid ) )
        return -ENOENT;

    return 0;
}

gint32 CRpcServices::RebuildMatches()
{
    gint32 ret = 0;
    do{
        // add interface id to all the matches
        CCfgOpenerObj oIfCfg( this );
        bool bPauseOnStart = false;
        if( IsServer() )
        {
            ret = oIfCfg.GetBoolProp(
                propPauseOnStart, bPauseOnStart );

            if( ERROR( ret ) )
                bPauseOnStart = false;
        }
        else
        {
            oIfCfg.RemoveProperty(
                propPauseOnStart );
        }
        
        bool bPausable = false;
        guint32 dwQueSize = MAX_DBUS_REQS;

        for( auto pMatch : m_vecMatches )
        {
            CCfgOpenerObj oMatchCfg(
                ( CObjBase* )pMatch );

            string strIfName;
            ret = oMatchCfg.GetStrProp(
                propIfName, strIfName );

            if( ERROR( ret ) )
                continue;
            
            EnumClsid iid = clsid( Invalid );

            ret = GetIidFromIfName(
                strIfName, iid );

            if( ERROR( ret ) )
                continue;

            oMatchCfg.SetIntProp( propIid, iid );

            if( iid == iid( CFileTransferServer ) )
                m_pFtsMatch = pMatch;

            if( iid == iid( IStream ) )
                m_pStmMatch = pMatch;

            // set the request que size
            ret = oMatchCfg.GetBoolProp(
                propPausable, bPausable );

            if( ERROR( ret ) )
                bPausable = false;

            dwQueSize = MAX_DBUS_REQS;

            if( bPausable && bPauseOnStart )
                dwQueSize = 0;

            oMatchCfg.SetIntProp(
                propQueSize, dwQueSize );
        }

        // set the interface id for this match
        CCfgOpenerObj oMatchCfg(
            ( CObjBase* )m_pIfMatch );

        oMatchCfg.SetIntProp( propIid,
            GetClsid() );

        dwQueSize = MAX_DBUS_REQS;

        // set the request que size for the
        // interface
        ret = oMatchCfg.GetBoolProp(
            propPausable, bPausable );

        if( ERROR( ret ) )
            bPausable = false;

        if( bPausable && bPauseOnStart )
            dwQueSize = 0;

        oMatchCfg.SetIntProp(
            propQueSize, dwQueSize );

        // also append the match for master interface
        m_vecMatches.push_back( m_pIfMatch );

    }while( 0 );

    return 0;
}

gint32 CRpcServices::StartEx2(
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    return super::StartEx( pCallback );
}

gint32 CRpcServices::StartEx(
    IEventSink* pCallback )
{
    gint32 ret = 0;
    if( true )
    {
        CStdRMutex oIfLock( GetLock() );
        if( GetState() == stateStopped )
        {
            ret = SetStateOnEvent( cmdOpenPort );
            if( ERROR( ret ) )
                return ret;
        }
        if( GetState() != stateStarting )
        {
            return ERROR_STATE;
        }
    }

    do{

        TaskletPtr pPreStart;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pPreStart, ObjPtr( this ),
            &CRpcServices::OnPreStart,
            nullptr );

        if( ERROR( ret ) )
            break;

        TaskletPtr pStartEx2;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pStartEx2, ObjPtr( this ),
            &CRpcServices::StartEx2,
            nullptr );

        if( ERROR( ret ) )
            break;

        TaskletPtr pPostStart;
        ret = DEFER_IFCALLEX_NOSCHED2(
            0, pPostStart, ObjPtr( this ),
            &CRpcServices::OnPostStart,
            nullptr );

        if( ERROR( ret ) )
            break;

        CParamList oParams;
        oParams.SetPointer( propIfPtr, this );

        TaskletPtr pTaskGrp;
        ret = pTaskGrp.NewObj(
            clsid( CIfTaskGroup ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        CIfTaskGroup* pGrp = pTaskGrp;
        if( unlikely( pGrp == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        pGrp->SetClientNotify( pCallback );
        pGrp->SetRelation( logicAND );
        pGrp->AppendTask( pPreStart );
        pGrp->AppendTask( pStartEx2 );
        pGrp->AppendTask( pPostStart );
        
        CIoManager* pMgr = GetIoMgr();
        ret = pMgr->RescheduleTask( pTaskGrp );
        if( ERROR( ret ) )
        {
            ( *pTaskGrp )( eventCancelTask );
        }
        else 
        {
            ret = STATUS_PENDING;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcServices::OnPreStart(
    IEventSink* pCallback )
{
    gint32 ret = InitUserFuncs();

    if( ERROR( ret ) )
        return ret;

    return RebuildMatches();
}

gint32 CRpcServices::OnPostStart(
    IEventSink* pCallback )
{
    return 0;
}

gint32 CRpcServices::OnPostStop(
    IEventSink* pCallback )
{
    CStdRMutex oIfLock( GetLock() );
    m_vecMatches.clear();
    m_mapFuncs.clear();
    m_mapProxyFuncs.clear();

    m_pFtsMatch.Clear();
    m_pStmMatch.Clear();
    if( !m_pSeqTasks.IsEmpty() &&
        m_pSeqTasks->GetTaskCount() > 0 )
    {
        TaskGrpPtr pSeqTasks = m_pSeqTasks;
        m_pSeqTasks.Clear(); 
        oIfLock.Unlock();
        ( *pSeqTasks )( eventCancelTask );
    }
    // should be the last to remove
    m_pRootTaskGroup.Clear();

    return 0;
}

typedef std::pair< gint32, BufPtr > ARG_ENTRY;

/**
* @name InvokeMethod to invoke the specified
* method in the pReqMsg. 
*
* parameter:
*   pReqMsg: the incoming method call or the event
*
*   pCallback: the callback object. It is required
*   to call this callback when the method is done
*   asynchronously, with eventTaskComp and return
*   value as the input parameters. Note that you
*   CANNOT call this callback from within the
*   context of InvokeMethod.
* @{ */
/**  @} */

template< typename T1, typename T >
gint32 CRpcServices::InvokeMethod(
    T* pReqMsg, IEventSink* pCallback )
{
    gint32 ret = 0;
    CCfgOpenerObj oCfg( this );

    if( pReqMsg == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    do{
        CIfInvokeMethodTask* pInvokeTask = 
            ObjPtr( pCallback );

        ret = DoInvoke( pReqMsg, pCallback );
        if( ret == STATUS_PENDING )
        {
            // for async call, we have timer and
            // keep-alive to set
            if( pInvokeTask == nullptr )
                break;
            // no need to lock, since we are
            // already within the lock of
            // pInvokeTask
            if( pInvokeTask != nullptr )
                pInvokeTask->SetAsyncCall();

            break;
        }

    }while( 0 );

    return ret;
}

template
gint32 CRpcServices::InvokeMethod<DBusMessage>(
    DBusMessage* pReqMsg, IEventSink* pCallback );

template
gint32 CRpcServices::InvokeMethod<IConfigDb>(
    IConfigDb* pReqMsg, IEventSink* pCallback );

/**
* @name RunIoTask
* Schedule a CIfIoReqTask with the parameters in
* pReqCall, and response data in pRespData. Every
* CIfIoReqTask will fill the response data
* OnComplete whether sync or async.
* @{ */
/** 
 * 
 *  You should extract the return value of the io
 *  task from the response data on success.
 *
 *  If error code returned, it means the operation
 *  is prematured and no response data is
 *  returned.
 *  
 *  The status for this io task is not returnred
 *  from this method, because we don't know if the
 *  return value in the response data have
 *  different protocol to interpret the error code
 *
 * @} */

                
gint32 CRpcServices::RunIoTask(
    IConfigDb* pReqCall,
    CfgPtr& pRespData,
    IEventSink* pCallback,
    guint64* pIoTaskId )
{
    gint32 ret = 0;
    do{
        CParamList oParams;
        ret = oParams.SetObjPtr(
            propIfPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
            break;

        TaskletPtr pTask;   
        ret = pTask.NewObj(
            clsid( CIfIoReqTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        CIfIoReqTask* pIoTask = pTask;
        if( pIoTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        pIoTask->SetClientNotify(
            pCallback );

        // set the req and the resp
        pIoTask->SetReqCall( pReqCall );
        pIoTask->SetRespData( pRespData );

        guint64 qwTaskId = pTask->GetObjId();
        CReqBuilder oCfg( pReqCall );

        // propTaskId is for KEEP_ALIVE, and the
        // event notification
        oCfg.SetTaskId( qwTaskId );

        if( pIoTaskId != nullptr )
            *pIoTaskId = qwTaskId;

        CustomizeRequest(
            pReqCall, pCallback );

        // schedule the task or run it immediately
        ret = AddAndRun( pTask );

        if( ERROR( ret ) )
            break;

        ret = pTask->GetError();

        if( ret == STATUS_PENDING &&
            pCallback == nullptr )
        {
            CIfIoReqTask* pIoTask = pTask;
            if( pIoTask == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            ret = pIoTask->WaitForComplete();
            if( SUCCEEDED( ret ) )
            {
                // the response data should
                // already in the pRespData.
                //
                // no more action here
            }
        }

    }while( 0 );

    return ret;
}

gint32 CInterfaceProxy::RestartListening(
    EnumIfState iStateOld  )
{
    gint32 ret = 0;
    CParamList oParams;
    TaskletPtr pRecvMsgTask;

    EnumIfState iState = GetState();
    gint32 iStartType = 0;
    if( iState == stateConnected && 
        iStateOld == statePaused )
        iStartType = 1;

    else if( iState == stateConnected &&
        iStateOld == stateRecovery )
        iStartType = 2;

    else if( iState == statePaused &&
        iStateOld == stateRecovery )
        iStartType = 3;
    else
    {
        return 0;
    }

    if( iStartType == 3 )
        return 0;

    oParams[ propIfPtr ] = ObjPtr( this );
    for( auto pMatch : m_vecMatches )
    {
        oParams[ propMatchPtr ] =
            ObjPtr( pMatch );

        ret = pRecvMsgTask.NewObj(
        clsid( CIfStartRecvMsgTask ),
        oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        AddAndRun( pRecvMsgTask );
    }

    return ret;
}

gint32 CInterfaceProxy::SendData_Proxy(
    IConfigDb* pDataDesc,           // [in]
    gint32 fd,                      // [in]
    guint32 dwOffset,               // [in]
    guint32 dwSize,                 // [in]
    IEventSink* pCallback )
{
    CCfgOpener oCfg( pDataDesc );
    if( !oCfg.exist( propMethodName ) )
        oCfg[ propMethodName ] = 
            string( SYS_METHOD_SENDDATA );

    return SendFetch_Proxy( pDataDesc,
        fd, dwOffset, dwSize, pCallback );
}

gint32 CInterfaceProxy::FetchData_Proxy(
    IConfigDb* pDataDesc,           // [in, out]
    gint32& fd,                     // [out]
    guint32& dwOffset,              // [in, out]
    guint32& dwSize,                // [in, out]
    IEventSink* pCallback )
{
    if( pDataDesc == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    CCfgOpener oCfg( pDataDesc );
    if( !oCfg.exist( propMethodName ) )
        oCfg[ propMethodName ] = 
            string( SYS_METHOD_FETCHDATA );

    return SendFetch_Proxy( pDataDesc,
        fd, dwOffset, dwSize, pCallback );
}

gint32 CInterfaceProxy::SendFetch_Proxy(
    IConfigDb* pDataDesc,           // [in, out]
    gint32& fd,                     // [out]
    guint32& dwOffset,              // [in, out]
    guint32& dwSize,                // [in, out]
    IEventSink* pCallback )
{

    gint32 ret = 0;
    if( pDataDesc == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    try{
        do{
            CParamList oResp;
            bool bFetch = false;

            CReqBuilder oDesc( this );
            oDesc.Append( pDataDesc );

            // Send/Fetch belongs to interface
            // CFileTransferServer or CStreamServer
            EnumClsid iid = ( EnumClsid )
                ( ( guint32& )oDesc[ propIid ] );

            const std::string strIfName =
                CoGetIfNameFromIid( iid, "p" );

            if( strIfName.empty() )
            {
                // the interface is not supported by
                // this object
                ret = -ENOTSUP;
                break;
            }

            oDesc.SetIfName(
                DBUS_IF_NAME( strIfName ) );

            if( string( SYS_METHOD_FETCHDATA ) ==
                string( oDesc[ propMethodName ] ) )
            {
                bFetch = true;
            }
            if( !bFetch && dwSize == 0 )
            {
                ret = -EINVAL;
                break;
            }

            CReqBuilder oBuilder( this );
            oBuilder.SetMethodName(
                oDesc[ propMethodName ] );

            oBuilder.SetIfName(
                oDesc[ propIfName ] );

            oBuilder.SetCallFlags( CF_WITH_REPLY
               | DBUS_MESSAGE_TYPE_METHOD_CALL 
               | CF_ASYNC_CALL
               | CF_KEEP_ALIVE );

            oBuilder.CopyProp( propNonFd, pDataDesc );

            // well, the call option are required by
            // keep-alive, and since the content of
            // oBuilder for SEND/FETCH cannot go over
            // to the peer side, we have to put it in
            // the data desc. 
            oDesc.CopyProp( propCallOptions,
                ( IConfigDb* )oBuilder.GetCfg() );

            oBuilder.Push( ObjPtr( oDesc.GetCfg() ) );
            oBuilder.Push( fd );
            oBuilder.Push( dwOffset );
            oBuilder.Push( dwSize );

            guint64 qwTaskId = 0;
            ret = RunIoTask( oBuilder.GetCfg(),
                oResp.GetCfg(), pCallback, &qwTaskId );

            if( ret == STATUS_PENDING )
            {
                // the parameter list does not have a
                // place for the task id, so put it in
                // the data desc.
                CCfgOpener oUserDesc( pDataDesc );
                oUserDesc[ propTaskId ] = qwTaskId;
                break;
            }

            gint32 iRet = 0;
            ret = oResp.GetIntProp(
                propReturnValue, ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            ret = iRet;

            if( SUCCEEDED( ret ) && bFetch )
            {
                oResp.Pop( dwSize );
                oResp.Pop( dwOffset );
                oResp.Pop( fd );
                ObjPtr pObj;
                oResp.Pop( pObj );

                IConfigDb* pRespDesc = pObj;
                if( pRespDesc != nullptr )
                {
                    // overwrite the pDataDesc
                    *pDataDesc = *pRespDesc;
                }
            }
        }while( 0 );
    }
    catch( std::runtime_error& e )
    {
        ret = -ENOENT;
    }

    return ret;
}

CRpcServices::CRpcServices( const IConfigDb* pCfg )
    : super( pCfg )
{
    do{
        gint32 ret = 0;
        CCfgOpenerObj oCfg( this );
        ret = oCfg.CopyProp( propTimeoutSec, pCfg );
        if( ERROR( ret ) )
        {
            oCfg.SetIntProp( propTimeoutSec,
                IFSTATE_DEFAULT_IOREQ_TIMEOUT );
        }
        ret = oCfg.CopyProp( propKeepAliveSec, pCfg );
        if( ERROR( ret ) )
        {
            oCfg.SetIntProp( propKeepAliveSec,
                IFSTATE_DEFAULT_IOREQ_TIMEOUT / 2 );
        }

        ret = oCfg.CopyProp(
            propPauseOnStart, pCfg );

        if( oCfg.exist( propObjList ) )
        {
            ObjPtr pObj;
            ret = oCfg.GetObjPtr( propObjList, pObj );
            if( ERROR( ret ) )
                break;

            ObjVecPtr pObjVec( pObj );
            if( pObjVec.IsEmpty() )
            {
                ret = -EFAULT;
                break;
            }
            for( auto pElem : ( *pObjVec )() )
            {
                MatchPtr pMatch( pElem );
                m_vecMatches.push_back( pMatch );
            }

            RemoveProperty( propObjList );
        }

    }while( 0 );

    return;
}

gint32 CRpcServices::BuildBufForIrpIfMethod(
    BufPtr& pBuf, IConfigDb* pReqCall )
{
    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( pBuf.IsEmpty() )
        {
            ret = pBuf.NewObj();
            if( ERROR( ret ) )
                break;
        }

        // the requests have a match object in the
        // req ptr
        CCfgOpener oCfg( pReqCall );
        ObjPtr pObj;
        ret = oCfg.GetObjPtr(
            propMatchPtr, pObj );

        if( ERROR( ret ) )
            break;

        *pBuf = pObj;

    }while( 0 );

    return ret;
}

gint32 CRpcServices::BuildBufForIrp(
    BufPtr& pBuf, IConfigDb* pReqCall )
{
    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( pBuf.IsEmpty() )
        {
            ret = pBuf.NewObj();
            if( ERROR( ret ) )
                break;
        }

        bool bRequest = false;

        CReqOpener oReq( pReqCall );
        gint32 iReqType;
        ret = oReq.GetReqType(
            ( guint32& )iReqType );

        if( ERROR( ret ) )
            break;

        if( iReqType == 
                DBUS_MESSAGE_TYPE_METHOD_RETURN )
        {
            ret = -EINVAL;
            break;
        }
        if( iReqType ==
            DBUS_MESSAGE_TYPE_METHOD_CALL )
        {
            bRequest = true;
        }

        string strMethod;
        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        if( strMethod == SYS_METHOD_SENDDATA ||
            strMethod == SYS_METHOD_FETCHDATA )
        {
            ObjPtr pObj( pReqCall );
            *pBuf = pObj;
            break;
        }
        else if( strMethod == IF_METHOD_ENABLEEVT ||
            strMethod == IF_METHOD_DISABLEEVT ||
            strMethod == IF_METHOD_LISTENING )
        {
            // the m_reqData is an MatchPtr
            ret = BuildBufForIrpIfMethod(
                pBuf, pReqCall );
            break;
        }

        // let's build a message
        DMsgPtr pMsg;
        ret = pMsg.NewObj( ( EnumClsid )iReqType );

        string strObjPath;
        ret = oReq.GetObjPath( strObjPath );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetPath( strObjPath );
        if( ERROR( ret ) )
            break;

        string strIfName;
        ret = oReq.GetIfName( strIfName );
        if( ERROR( ret ) )
            break;

        ret = pMsg.SetInterface( strIfName );
        if( ERROR( ret ) )
            break;

        string strDest;
        ret = oReq.GetDestination( strDest );
        if( ERROR( ret ) && bRequest )
            break;

        if( SUCCEEDED( ret ) )
        {
            ret = pMsg.SetDestination( strDest );
            if( ERROR( ret ) && bRequest )
                break;
        }

        string strSender;
        ret = oReq.GetSender( strSender );

        if( SUCCEEDED( ret ) )
        {
            ret = pMsg.SetSender( strSender );
            if( ERROR( ret ) ) 
                break;
        }

        ret = pMsg.SetMember( strMethod );
        if( ERROR( ret ) )
            break;

        // remove redudant information from the
        // request to send
        oReq.RemoveProperty( propMethodName );
        oReq.RemoveProperty( propObjPath );
        oReq.RemoveProperty( propIfName );
        oReq.RemoveProperty( propDestDBusName );
        oReq.RemoveProperty( propSrcDBusName );

        BufPtr pReqBuf( true );
        ret = pReqCall->Serialize( *pReqBuf );
        if( ERROR( ret ) )
            break;

        oReq.SetStrProp(
            propMethodName, strMethod );

        oReq.SetStrProp(
            propIfName, strIfName );

        oReq.SetStrProp(
            propObjPath, strObjPath );

        if( !strDest.empty() )
        {
            oReq.SetStrProp(
                propSrcDBusName, strDest );
        }

        if( !strSender.empty() )
        {
            oReq.SetStrProp(
                propSrcDBusName, strSender );
        }

        const char* pData = pReqBuf->ptr();
        if( !dbus_message_append_args( pMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pReqBuf->size(),
            DBUS_TYPE_INVALID ) )
        {
            ret = -ENOMEM;
            break;
        }
        *pBuf = pMsg;

    }while( 0 );

    return ret;
}

/**
* @name Fill pResp with the content in pIrp
* @{ */
/**  @} */

gint32 CRpcServices::FillRespData(
    IRP* pIrp, CfgPtr& pResp )
{
    gint32 ret = 0;
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( pResp.IsEmpty() )
        return -EINVAL;

    CParamList oParams( ( IConfigDb* )pResp );
    if( SUCCEEDED( pIrp->GetStatus() ) )
    {
        // retrieve the data from the irp
        IrpCtxPtr& pCtx = pIrp->GetTopStack();
        guint32 dwDirection = pCtx->GetIoDirection();
        if( dwDirection == IRP_DIR_OUT )
            return -EINVAL;

        if( pCtx->IsNonDBusReq() )
        {
            CfgPtr pMsg = nullptr;
            ret = pCtx->GetRespAsCfg( pMsg );
            if( SUCCEEDED( ret ) )
            {
                pResp = pMsg;
            }
            else
            {
                oParams[ propReturnValue ] = ret;
            }
        }
        else
        {
            BufPtr& pBuf = pCtx->m_pRespData;
            if( !pBuf.IsEmpty() )
            {
                DMsgPtr pMsg = *pBuf;
                if( !pMsg.IsEmpty() )
                {
                    ret = FillRespData( pMsg, pResp );
                    if( ERROR( ret ) )
                    {
                        oParams[ propReturnValue ] = ret;
                    }
                }
            }
        }
    }

    if( !oParams.exist( propReturnValue ) )
    {
        // for those cancelled irp or bad response
        // returned
        oParams[ propReturnValue ] =
            pIrp->GetStatus();
    }
    return ret;
}

gint32 CRpcServices::FillRespData(
    DMsgPtr& pMsg, CfgPtr& pResp )
{
    gint32 ret = 0;
    if( pMsg.IsEmpty() || pResp.IsEmpty() )
        return -EINVAL;

    CCfgOpener oCfg( ( IConfigDb* )pResp );
    if( pMsg.GetType() !=
        DBUS_MESSAGE_TYPE_METHOD_RETURN )
    {
        ret = -EBADMSG;
        oCfg.SetIntProp(
            propReturnValue, ret );
        return ret;
    }

    do{
        string strMethod = pMsg.GetMember();
        if( strMethod == SYS_METHOD_SENDDATA )
        {
            gint32 iRet = 0;
            ret = pMsg.GetIntArgAt(
                0, ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            ret = oCfg.SetIntProp(
                propReturnValue, iRet );

            if( ERROR( ret ) )
                break;
        }
        else if( strMethod == SYS_METHOD_FETCHDATA )
        {
            
            gint32 iRet = 0;
            CParamList oParams( pResp );
            ret = pMsg.GetIntArgAt(
                0, ( guint32& )iRet );

            if( ERROR( ret ) )
                break;

            ret = oParams.SetIntProp(
                propReturnValue, iRet );

            if( ERROR( ret ) )
                break;

            if( ERROR( iRet ) )
                break;

            gint32 iFd = 0;
            guint32 dwOffset = 0;
            guint32 dwSize = 0;
            ObjPtr pObj;

            ret = pMsg.GetObjArgAt( 1, pObj );
            if( ERROR( ret ) )
                break;

            oParams.Push( pObj );

            ret = pMsg.GetFdArgAt( 2, iFd );

            if( ERROR( ret ) )
            {
                ret = pMsg.GetIntArgAt( 2,
                    ( guint32& )iFd );
                if( ERROR( ret ) )
                    break;
            }

            oParams.Push( iFd );

            ret = pMsg.GetIntArgAt( 3, dwOffset );
            if( ERROR( ret ) )
                break;
            oParams.Push( dwOffset );

            ret = pMsg.GetIntArgAt( 4, dwSize );
            if( ERROR( ret ) )
                break;

            oParams.Push( dwSize );
        }
        else
        {
            // for the default requests, a
            // configDb ptr is returned
            ObjPtr pObj;
            gint32 iRet = 0;
            ret = pMsg.GetIntArgAt( 0, ( guint32& )iRet );
            if( ERROR( ret ) )
                break;

            if( ERROR( iRet ) )
            {
                oCfg[ propReturnValue ] = iRet;
                break;
            }

            ret = pMsg.GetObjArgAt( 1, pObj );
            if( ERROR( ret ) )
                break;

            IConfigDb* pCfg = pObj;
            if( pCfg == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            /* Fixed a bug by copying the response data
             * from GetObjArgAt to CIfIoReqTask's
             * propRespPtr, which was so but changed to
             * copying ptr sometimes for performance
             * consideration. Now there are chances the
             * CIfIoReqTask can complete immediately,
             * that is the CRpcServices::AddAndRun can
             * return successfully some times, which
             * requires the the response data in the
             * CIfIoReqTask to be filled with
             * meaningful return value, instead of
             * replacing the given response data ptr
             * from the top request caller with a newly
             * allocated response data. So we have to
             * copy the content from the config db
             * created by DMsgPtr::GetObjArgAt to the
             * pResp held by the CIfIoReqTask.
            */
            *pResp = *pCfg;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcServices::SetupReqIrp( IRP* pIrp,
    IConfigDb* pReqCall,
    IEventSink* pCallback )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CReqOpener oReq( pReqCall );
        pIrp->SetIrpThread( GetIoMgr() );

        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );

        guint32 dwIoDir = IRP_DIR_OUT;
        guint32 dwCallFlags = 0;

        ret = oReq.GetCallFlags( dwCallFlags );
        if( SUCCEEDED( ret ) )
        {
            if( dwCallFlags & CF_WITH_REPLY )
                dwIoDir = IRP_DIR_INOUT;

            if( dwCallFlags & CF_NON_DBUS )
                pIrpCtx->SetNonDBusReq( true );
        }

        bool bSet = false;
        string strMethod;
        ret = oReq.GetMethodName( strMethod );
        if( SUCCEEDED( ret ) )
        {
            bSet = true;
            if( strMethod ==
                IF_METHOD_DISABLEEVT )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_UNREG_MATCH );
            }
            else if( strMethod ==
                IF_METHOD_ENABLEEVT )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_REG_MATCH );
            }
            else if( strMethod ==
                IF_METHOD_LISTENING )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_LISTENING );
                dwIoDir = IRP_DIR_IN;
            }
            else if( strMethod ==
                SYS_METHOD_FETCHDATA )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_FETCH_DATA );
            }
            else if( strMethod ==
                SYS_METHOD_SENDDATA )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_SEND_DATA );
            }
            else
            {
                bSet = false;
            }
        }

        if( !bSet )
        {
            gint32 iReqType = 0;
            ret = oReq.GetReqType(
                ( guint32& )iReqType );

            if( ERROR( ret ) )
                break;

            if( iReqType ==
                DBUS_MESSAGE_TYPE_METHOD_CALL )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_SEND_REQ );
            }
            else if( iReqType ==
                DBUS_MESSAGE_TYPE_SIGNAL )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_SEND_EVENT );
            }
            else if( iReqType ==
                DBUS_MESSAGE_TYPE_METHOD_RETURN ||
                iReqType ==
                DBUS_MESSAGE_TYPE_ERROR )
            {
                pIrpCtx->SetCtrlCode(
                    CTRLCODE_SEND_RESP );
            }
            else
            {
                ret = -EINVAL;
                break;
            }
        }

        pIrpCtx->SetIoDirection( dwIoDir ); 

        BufPtr pBuf( true );
        ret = BuildBufForIrp( pBuf, pReqCall );
        if( ERROR( ret ) )
            break;

        pIrpCtx->SetReqData( pBuf );

        if( pCallback != nullptr )
            pIrp->SetCallback( pCallback, 0 );

        guint32 dwTimeoutSec = 0;
        ret = oReq.GetTimeoutSec( dwTimeoutSec );

        if( SUCCEEDED( ret ) &&
            dwTimeoutSec != 0 )
        {
            pIrp->SetTimer(
                dwTimeoutSec, GetIoMgr() );
        }
        else
        {
            ret = 0;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcServices::InvokeUserMethod(
    IConfigDb* pParams,
    IEventSink* pCallback )
{
    if( pParams == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        EnumSeriProto dwSeriProto = seriNone;
        ret = GetSeriProto(
            pParams, dwSeriProto );
        if( dwSeriProto != seriNone &&
            dwSeriProto != seriRidl )
        {
            ret = -EBADMSG;
            break;
        }
        ret = 0;
        CReqOpener oReq( pParams );
        string strMethod;
        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        TaskletPtr pTask;

        CCfgOpenerObj oCfg( pCallback );
        string strIfName;

        ret = oCfg.GetStrProp(
            propIfName, strIfName );
        if( ERROR( ret ) ) 
            break;

        EnumClsid iid = clsid( Invalid );

        ret = GetIidFromIfName(
            strIfName, iid );

        if( ERROR( ret ) )
            break;

        ret = GetDelegate(
            strMethod, pTask, iid );

        if( ERROR( ret ) )
            break;

        CDelegateBase* pDelegate = pTask;
        if( pDelegate == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = ( *pDelegate )(
            this, pCallback, pParams ); 

    }while( 0 );

    return ret;
}

gint32 CRpcServices::SendMethodCall(
    IConfigDb* pReqCall,
    CfgPtr& pRespData,
    IEventSink* pCallback )
{
    gint32 ret = 0;
    if( pCallback == nullptr ||
        pReqCall == nullptr )
        return -EINVAL;

    if( pRespData.IsEmpty() )
        return -EFAULT;

    do{
        CStdRMutex oIfLock( GetLock() );
        if( !IsConnected() )
        {
            ret = ERROR_STATE;
            break;
        }

        PortPtr pPort;
        ret = GetPortToSubmit( pReqCall, pPort );
        if( ERROR( ret ) )
            break;

        oIfLock.Unlock();

        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        ret = SetupReqIrp(
            pIrp, pReqCall, pCallback );

        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 
        pPort->AllocIrpCtxExt(
            pIrpCtx, ( PIRP )pIrp );

        // set an irp for canceling purpose
        CCfgOpenerObj oCfg( pCallback );
        oCfg.SetObjPtr( propIrpPtr,
            ObjPtr( ( IRP* )pIrp ) );

        ret = GetIoMgr()->SubmitIrpInternal(
            pPort, pIrp, false );

        if( ret == STATUS_PENDING )
            break;

        oCfg.RemoveProperty( propIrpPtr );
        pIrp->RemoveTimer();

        if( ERROR( ret ) )
        {
            pIrp->RemoveCallback();
            break;
        }

        CReqOpener oReq( pReqCall );
        if( oReq.HasReply() ) 
        {
            gint32 iRet = FillRespData(
                pIrp, pRespData );
            if( ERROR( iRet ) && SUCCEEDED( ret ) )
                ret = iRet;
        }
        // on success, the callback carries the
        // response, so it should be the last one
        // to be removed
        pIrp->RemoveCallback();

    }while( 0 );

    return ret;
}

FUNC_MAP* CRpcServices::GetFuncMap(
    EnumClsid iIfId )
{
    EnumClsid iEffectiveId = iIfId;
    if( iIfId == clsid( Invalid ) )
        iEffectiveId = GetClsid();

    FUNC_MAP* pMap = nullptr;

    std::map< gint32, FUNC_MAP >::iterator itr = 
        m_mapFuncs.find( iEffectiveId );

    if( itr == m_mapFuncs.end() )
        return nullptr;

    pMap = &itr->second;
    return pMap;
}

gint32 CRpcServices::SetFuncMap(
    FUNC_MAP& oMap, EnumClsid iIfId )
{
    if( oMap.empty() )
        return -EINVAL;

    EnumClsid iEffectiveId = iIfId;
    if( iIfId == clsid( Invalid ) )
        iEffectiveId = GetClsid();

    FUNC_MAP* pMap = GetFuncMap( iEffectiveId );
    if( pMap == nullptr )
    {
        m_mapFuncs[ iEffectiveId ] = oMap;
    }
    else
    {
        // to add new or override the existing
        pMap = &m_mapFuncs[ iEffectiveId ];
        for( auto elem : oMap )
            ( *pMap )[ elem.first ] = elem.second;
    }
    return 0;
}

PROXY_MAP* CRpcServices::GetProxyMap(
    EnumClsid iIfId )
{
    EnumClsid iEffectiveId = iIfId;
    if( iIfId == clsid( Invalid ) )
        iEffectiveId = GetClsid();

    std::map< gint32, PROXY_MAP >::iterator itr =
        m_mapProxyFuncs.find( iEffectiveId );

    if( itr == m_mapProxyFuncs.end() )
        return nullptr;

    PROXY_MAP* pMap = &itr->second;
    return pMap;
}

gint32 CRpcServices::SetProxyMap(
    PROXY_MAP& oMap, EnumClsid iIfId )
{
    if( oMap.empty() )
        return -EINVAL;

    EnumClsid iEffectiveId = iIfId;
    if( iIfId == clsid( Invalid ) )
        iEffectiveId = GetClsid();

    PROXY_MAP* pMap = GetProxyMap( iEffectiveId );
    if( pMap == nullptr )
    {
        m_mapProxyFuncs[ iEffectiveId ] = oMap;
    }
    else
    {
        // to add new or override the existing
        pMap = &m_mapProxyFuncs[ iEffectiveId ];
        for( auto elem : oMap )
            ( *pMap )[ elem.first ] = elem.second;
    }
    return 0;
}

gint32 CRpcServices::GetDelegate(
    const std::string& strFunc,
    TaskletPtr& pFunc,
    EnumClsid iIfId )
{
    EnumClsid iEffectiveId = iIfId;
    if( iIfId == clsid( Invalid ) )
        iEffectiveId = GetClsid();

    FUNC_MAP* pMap =
        GetFuncMap( iEffectiveId );

    if( pMap == nullptr )
        return -ERANGE;

    FUNC_MAP::iterator itrFunc =
        pMap->find( strFunc );

    if( itrFunc == pMap->end() )
        return -ERANGE;

    pFunc = itrFunc->second;
    return 0;
}

gint32 CRpcServices::GetProxy(
    const std::string& strFunc,
    ObjPtr& pProxy,
    EnumClsid iIfId )
{
    EnumClsid iEffectiveId = iIfId;
    if( iIfId == clsid( Invalid ) )
        iEffectiveId = GetClsid();

    PROXY_MAP* pMap =
        GetProxyMap( iEffectiveId );

    if( pMap == nullptr )
        return -ERANGE;

    PROXY_MAP::iterator itrFunc =
        pMap->find( strFunc );

    if( itrFunc == pMap->end() )
        return -ERANGE;

    pProxy = itrFunc->second;
    return 0;
}

/**
* @name Load the object settings from an object
* description file.
* @{ */
/**
 * Parameters:
 *
 *  strFile: [ in ] the path name of the object
 *  description file.
 *   
 *  strObjName: [ in ] the object name, which specify
 *  the description block in the description to load.
 *  if there is no instance name in the pCfg, this name
 *  will be used to build the propObjPath, which is
 *  registered with the dbus as the dbus-level address.
 *
 *  bServer: [ in ] a flag to indicate if the object is
 *  a a server object to create. 
 *
 *  properties in pCfg: 
 *      [ in ] propObjInstName if exists, will
 *      override the objectname from the desc file
 *      and be used to build the propObjPath
 *
 *      [ in ] propSvrInstName if exists, will
 *      override the server name from the desc
 *      file and be used to build the propObjPath
 *
 *      [ out ] contains all the settings for
 *      the object creation.
 * @} */

gint32 CRpcServices::LoadObjDesc(
    const std::string& strFile,
    const std::string& strObjName,
    bool bServer,
    CfgPtr& pCfg )
{
    gint32 ret = 0;

    if( strObjName.empty() ||
        strFile.empty() )
        return -EINVAL;

    if( pCfg.IsEmpty() )
        ret = pCfg.NewObj();

    if( ERROR( ret ) )
        return ret;

    string strInstName;
    do{
        CCfgOpener oCfg( ( IConfigDb* )pCfg );

        CIoManager* pMgr = nullptr;
        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
        {
            pMgr = nullptr;
            ret = 0;
        }

        ret = oCfg.GetStrProp(
            propObjInstName, strInstName );

        if( ERROR( ret ) )
        {
            ret = 0;
            strInstName = strObjName;
        }

        strInstName = strInstName.substr(
            0, DBUS_MAXIMUM_NAME_LENGTH );

        // Load the object decription file in json
        Json::Value valObjDesc;
        std::string strPath = strFile;
        ret = ReadJsonCfg( strFile, valObjDesc );
        if( ERROR( ret ) )
        {
            if( pMgr == nullptr ||
                strFile[ 0 ] == '/' )
            {
                DebugPrint( ret, "Failed to open "
                    "descprtion file %s",
                    strFile.c_str() );
                break;
            }

            ret = pMgr->TryFindDescFile(
                strFile, strPath );
            if( ERROR( ret ) )
            {
                DebugPrint( ret, "Failed to open "
                    "descprtion file %s",
                    strFile.c_str() );
                break;
            }

            ret = ReadJsonCfg(
                strPath, valObjDesc );
            if( ERROR( ret ) )
                break;
        }

        // get ServerName
        Json::Value& oServerName =
            valObjDesc[ JSON_ATTR_SVRNAME ];

        if( oServerName == Json::Value::null )
        {
            ret = -ENOENT;
            break;
        }

        oCfg.SetStrProp(
            propObjDescPath, strPath );

        string strSvrName;
        ret = oCfg.GetStrProp(
            propSvrInstName, strSvrName );

        if( ERROR( ret ) )
        {
            ret = 0;
            strSvrName = oServerName.asString();
        }

        strSvrName = strSvrName.substr(
            0, DBUS_MAXIMUM_NAME_LENGTH );

        // dbus related information
        string strObjPath = DBUS_OBJ_PATH(
            strSvrName, strInstName );

        string strDest = DBUS_DESTINATION2(
            strSvrName, strInstName );
        if( strDest.empty() )
        {
            ret = -EINVAL;
            break;
        }

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

            // get BusName
            if( oObjElem.isMember( JSON_ATTR_BUSNAME ) &&
                oObjElem[ JSON_ATTR_BUSNAME ].isString() )
            {
                Json::Value& oBusName =
                    oObjElem[ JSON_ATTR_BUSNAME ];

                strVal = oBusName.asString().substr(
                    0, DBUS_MAXIMUM_NAME_LENGTH );

                oCfg[ propBusName ] = strVal;
            }
            else
            {
                ret = -ENOENT;
                break;
            }
            
            // get PortClass
            if( oObjElem.isMember( JSON_ATTR_PORTCLASS  ) &&
                oObjElem[ JSON_ATTR_PORTCLASS ].isString() )
            {
                Json::Value& oPortClass =
                    oObjElem[ JSON_ATTR_PORTCLASS ];

                strVal = oPortClass.asString().substr(
                    0, DBUS_MAXIMUM_NAME_LENGTH );
                oCfg[ propPortClass ] = strVal;
            }
            else
            {
                ret = -ENOENT;
                break;
            }

            // get PortId
            if( oObjElem.isMember( JSON_ATTR_PORTID ) &&
                oObjElem[ JSON_ATTR_PORTID ].isString() )
            {
                Json::Value& oPortId =
                    oObjElem[ JSON_ATTR_PORTID ];
                strVal = oPortId.asString().substr(
                    0, DBUS_MAXIMUM_NAME_LENGTH );
                oCfg[ propPortId ] = std::stoi( strVal );
            }
            else
            {
                oCfg[ propPortId ] = ( guint32 )-1;
            }
            
            // get the RequestTimeoutSec
            if( oObjElem.isMember( JSON_ATTR_REQ_TIMEOUT ) &&
                oObjElem[ JSON_ATTR_REQ_TIMEOUT ].isString() )
            {
                strVal = oObjElem[ JSON_ATTR_REQ_TIMEOUT ].asString(); 
                oCfg[ propTimeoutSec ] = std::stoi( strVal );
            }

            // get the KeepAliveTimeoutSec
            if( oObjElem.isMember( JSON_ATTR_KA_TIMEOUT ) &&
                oObjElem[ JSON_ATTR_KA_TIMEOUT ].isString() )
            {
                strVal = oObjElem[ JSON_ATTR_KA_TIMEOUT ].asString(); 
                oCfg[ propKeepAliveSec ] = std::stoi( strVal );
            }

            if( oObjElem.isMember( JSON_ATTR_PAUSE_START ) &&
                oObjElem[ JSON_ATTR_PAUSE_START ].isString() )
            {
                strVal = oObjElem[ JSON_ATTR_PAUSE_START ].asString(); 
                if( strVal == "true" )
                    oCfg[ propPauseOnStart ] = true;
                else if( strVal == "false" )
                    oCfg[ propPauseOnStart ] = false;
            }

            if( oObjElem.isMember( JSON_ATTR_MAXREQS ) &&
                oObjElem[ JSON_ATTR_MAXREQS ].isString() )
            {
                strVal = oObjElem[ JSON_ATTR_MAXREQS ].asString();
                oCfg[ propMaxReqs ] = std::stoi( strVal );
            }

            if( oObjElem.isMember( JSON_ATTR_MAXPENDINGS ) &&
                oObjElem[ JSON_ATTR_MAXPENDINGS ].isString() )
            {
                strVal = oObjElem[ JSON_ATTR_MAXPENDINGS ].asString();
                oCfg[ propMaxPendings ] = std::stoi( strVal );
            }

            // overwrite the global port class and port
            // id if PROXY_PORTCLASS and PROXY_PORTID
            // exist
            bool bProxyPdo = false;
            if( oObjElem.isMember( JSON_ATTR_PROXY_PORTCLASS ) &&
                oObjElem[ JSON_ATTR_PROXY_PORTCLASS ].isString() &&
                !bServer )
            {
                strVal = oObjElem[ JSON_ATTR_PROXY_PORTCLASS ].asString(); 
                oCfg[ propPortClass ] = strVal;
                if( strVal == PORT_CLASS_DBUS_PROXY_PDO ||
                    strVal == PORT_CLASS_DBUS_PROXY_PDO_LPBK )
                    bProxyPdo = true;
            }

            // get ipaddr
            if( oObjElem.isMember( JSON_ATTR_IPADDR ) &&
                oObjElem[ JSON_ATTR_IPADDR ].isString() &&
                !bServer )
            {
                strVal = oObjElem[ JSON_ATTR_IPADDR ].asString(); 
                string strNormVal;
                ret = NormalizeIpAddr(
                    AF_INET, strVal, strNormVal );
                if( ERROR( ret ) )
                {
                    ret = NormalizeIpAddr(
                        AF_INET6, strVal, strNormVal );
                }

                if( SUCCEEDED( ret ) )
                    oCfg[ propDestIpAddr ] = strNormVal;
                else if( !bProxyPdo )
                    ret = 0;
                else
                {
                    addrinfo hints, *result = nullptr;
                    memset( &hints, 0, sizeof( addrinfo ) );
                    hints.ai_family = AF_UNSPEC;
                    hints.ai_socktype = SOCK_STREAM;

                    ret = getaddrinfo( strVal.c_str(),
                        nullptr, &hints, &result );
                    if( ret < 0 )
                        break;
                    // use the first address only
                    sockaddr_in* h =
                        ( sockaddr_in *)result->ai_addr;
                    char szIpAddr[ INET6_ADDRSTRLEN + 1 ];
                    szIpAddr[ INET6_ADDRSTRLEN ] = 0;

                    const char* szRet = inet_ntop(
                        result->ai_family,
                        &h->sin_addr,
                        szIpAddr,
                        INET6_ADDRSTRLEN );

                    if( szRet == nullptr )
                    {
                        ret = -errno;
                        freeaddrinfo( result );
                        break;
                    }
                    oCfg[ propDestIpAddr ] = szRet;
                    freeaddrinfo( result );
                }
            }

            // tcp port number for router setting
            guint32 dwPortNum = 0xFFFFFFFF;
            if( oObjElem.isMember( JSON_ATTR_TCPPORT ) &&
                oObjElem[ JSON_ATTR_TCPPORT ].isString() )
            {
                strVal = oObjElem[ JSON_ATTR_TCPPORT ].asString(); 
                guint32 dwVal = 0;
                if( !strVal.empty() )
                {
                    dwVal = std::strtol(
                        strVal.c_str(), nullptr, 10 );
                }
                if( dwVal > 0 && dwVal < 0x10000 )
                {
                    oCfg[ propDestTcpPort ] = dwVal;
                    dwPortNum = dwVal;
                }
                else
                {
                    ret = -EINVAL;
                    break;
                }
                oCfg.SetBoolProp(
                    propIsServer, bServer );
            }

            if( oObjElem.isMember( JSON_ATTR_PROXY_PORTID ) &&
                oObjElem[ JSON_ATTR_PROXY_PORTID ].isString() &&
                !bServer )
            {
                strVal = oObjElem[ JSON_ATTR_PROXY_PORTID ].asString(); 
                oCfg[ propPortId ] = std::stoi( strVal );
            }

            // authentication information
            CCfgOpener oAuth;
            while( oObjElem.isMember( JSON_ATTR_AUTHINFO ) &&
                oObjElem[ JSON_ATTR_AUTHINFO ].isObject() )
            {
                Json::Value& oObjAuth = oObjElem[ JSON_ATTR_AUTHINFO ];
                if( !oObjAuth.isMember( JSON_ATTR_AUTHMECH ) )
                    break;

                string strVal =
                    oObjAuth[ JSON_ATTR_AUTHMECH ].asString();

                if( !( strVal == "krb5" ||
                    strVal == "ntlm" ||
                    strVal == "password" ) )
                    break;

                string strMech = strVal;

                oAuth.SetStrProp( propAuthMech, strVal );

                if( oObjAuth.isMember( JSON_ATTR_SVCNAME ) &&
                    oObjAuth[ JSON_ATTR_SVCNAME ].isString() )
                {
                    strVal = oObjAuth[ JSON_ATTR_SVCNAME ].asString();
                    oAuth.SetStrProp( propServiceName, strVal );
                }

                if( oObjAuth.isMember( JSON_ATTR_REALM ) &&
                    oObjAuth[ JSON_ATTR_REALM ].isString() )
                {
                    strVal = oObjAuth[ JSON_ATTR_REALM ].asString();
                    oAuth.SetStrProp( propRealm, strVal );
                }

                strVal.clear();
                if( oObjAuth.isMember( JSON_ATTR_SIGN_MSG ) &&
                    oObjAuth[ JSON_ATTR_SIGN_MSG ].isString() )
                {
                    strVal = oObjAuth[ JSON_ATTR_SIGN_MSG ].asString();
                    if( strVal.size() > 0 && strVal == "true" )
                        oAuth.SetBoolProp( propSignMsg, true );
                    else
                        oAuth.SetBoolProp( propSignMsg, false );
                }

                strVal.clear();
                if( oObjAuth.isMember( JSON_ATTR_USERNAME ) &&
                    oObjAuth[ JSON_ATTR_USERNAME ].isString() )
                {
                    strVal = oObjAuth[ JSON_ATTR_USERNAME ].asString();
                    if( strVal.size() > 0 )
                        oAuth.SetStrProp( propUserName, strVal );
                }
                if( strVal.empty() )
                {
                    strVal = getenv( "LOGNAME" );
                    if( strVal.size() > 0 )
                        oAuth.SetStrProp( propUserName, strVal );
                }
                break;
            }

            // set the default parameters
            if( bProxyPdo )
            {
                CCfgOpener oConnParams;
                std::string strFormat = "ipv4";
                oConnParams[ propAddrFormat ] = strFormat;
                oConnParams[ propEnableSSL ] = false;
                oConnParams[ propEnableWebSock ] = false;
                oConnParams[ propCompress ] = true;
                oConnParams[ propConnRecover ] = false;
                oCfg[ propRouterPath ] = std::string( "/" );

                if( dwPortNum == 0xFFFFFFFF )
                    dwPortNum = RPC_SVR_DEFAULT_PORTNUM;

                oConnParams[ propDestTcpPort ] = dwPortNum;

                if( oObjElem.isMember( JSON_ATTR_ENABLE_SSL ) &&
                    oObjElem[ JSON_ATTR_ENABLE_SSL ].isString() &&
                    !bServer )
                {
                    strVal = oObjElem[ JSON_ATTR_ENABLE_SSL  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propEnableSSL ] = false;
                    else if( strVal == "true" )
                    {
                        oConnParams[ propEnableSSL ] = true;
                    }
                }

                if( oObjElem.isMember( JSON_ATTR_ADDRFORMAT ) &&
                    oObjElem[ JSON_ATTR_ADDRFORMAT ].isString() &&
                    !bServer )
                {
                    strFormat =
                        oObjElem[ JSON_ATTR_ADDRFORMAT ].asString(); 
                    oConnParams[ propAddrFormat ] =
                        oObjElem[ JSON_ATTR_ADDRFORMAT ].asString(); 
                }

                if( oObjElem.isMember( JSON_ATTR_ENABLE_WEBSOCKET ) &&
                    oObjElem[ JSON_ATTR_ENABLE_WEBSOCKET ].isString() &&
                    !bServer )
                {
                    strVal = oObjElem[ JSON_ATTR_ENABLE_WEBSOCKET  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propEnableWebSock ] = false;
                    else if( strVal == "true" )
                        oConnParams[ propEnableWebSock ] = true;
                }

                if( oObjElem.isMember( JSON_ATTR_ENABLE_COMPRESS ) &&
                    oObjElem[ JSON_ATTR_ENABLE_COMPRESS ].isString() &&
                    !bServer )
                {
                    strVal = oObjElem[ JSON_ATTR_ENABLE_COMPRESS  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propCompress ] = false;
                    else if( strVal == "true" )
                        oConnParams[ propCompress ] = true;
                }

                if( oObjElem.isMember( JSON_ATTR_CONN_RECOVER ) &&
                    oObjElem[ JSON_ATTR_CONN_RECOVER ].isString() &&
                    !bServer )
                {
                    strVal = oObjElem[ JSON_ATTR_CONN_RECOVER  ].asString(); 
                    if( strVal == "false" )
                        oConnParams[ propConnRecover ] = false;
                    else if( strVal == "true" )
                        oConnParams[ propConnRecover ] = true;
                }

                if( oObjElem.isMember( JSON_ATTR_DEST_URL ) &&
                    oObjElem[ JSON_ATTR_DEST_URL ].isString() &&
                    !bServer )
                {
                    strVal = oObjElem[ JSON_ATTR_DEST_URL  ].asString(); 
                    oConnParams[ propDestUrl ] = strVal;
                }

                if( oObjElem.isMember( JSON_ATTR_HASAUTH ) &&
                    oObjElem[ JSON_ATTR_HASAUTH ].isString() )
                {
                    string strVal =
                        oObjElem[ JSON_ATTR_HASAUTH ].asString();
                    if( strVal == "true" )
                        oConnParams.SetBoolProp( propHasAuth, true );
                    else
                        oConnParams.SetBoolProp( propHasAuth, false );
                }

                if( oObjElem.isMember( JSON_ATTR_ROUTER_PATH ) &&
                    oObjElem[ JSON_ATTR_ROUTER_PATH ].isString() &&
                    !bServer )
                {
                    strVal = oObjElem[ JSON_ATTR_ROUTER_PATH  ].asString(); 
                    oCfg[ propRouterPath ] = strVal;
                }

                oConnParams.CopyProp( propDestIpAddr, ( IConfigDb* )pCfg );
                oConnParams.CopyProp( propIsServer, ( IConfigDb* )pCfg );

                if( oAuth.GetCfg()->size() > 0 )
                    oConnParams.SetObjPtr( propAuthInfo, oAuth.GetCfg() );

                oCfg[ propConnParams ] =
                    ObjPtr( oConnParams.GetCfg() );
                oCfg.RemoveProperty( propDestIpAddr );
                oCfg.RemoveProperty( propDestTcpPort );

            }
            else
            {
                if( oAuth.GetCfg()->size() > 0 )
                    oCfg.SetObjPtr( propAuthInfo, oAuth.GetCfg() );
            }

            if( oObjElem.isMember( JSON_ATTR_QUEUED_REQ ) &&
                oObjElem[ JSON_ATTR_QUEUED_REQ ].isString() &&
                bServer )
            {
                strVal = oObjElem[ JSON_ATTR_QUEUED_REQ  ].asString(); 
                if( strVal == "false" )
                    oCfg[ propQueuedReq ] = false;
                else if( strVal == "true" )
                    oCfg[ propQueuedReq ] = true;
            }

            if( oObjElem.isMember( JSON_ATTR_ROUTER_ROLE ) &&
                oObjElem[ JSON_ATTR_ROUTER_ROLE ].isString() )
            {
                strVal = oObjElem[ JSON_ATTR_ROUTER_ROLE ].asString(); 
                oCfg[ propRouterRole ] = std::strtol(
                    strVal.c_str(), nullptr, 10 );
            }
            else
            {
                oCfg[ propRouterRole ] = 0x03;
            }

            // set the destination dbus name
            if( bServer )
            {
                // useful when sending out event
                oCfg[ propSrcDBusName ] = strDest;
                oCfg[ propObjPath ] = strObjPath;
            }
            else
            {
                std::string strRmtModName;
                std::string strRmtObjName;
                std::string strNewObjPath;
                std::string strRmtSvrName;

                if( oObjElem.isMember( JSON_ATTR_RMTMODNAME ) &&
                    oObjElem[ JSON_ATTR_RMTMODNAME ].isString() )
                {
                    strRmtModName = 
                        oObjElem[ JSON_ATTR_RMTMODNAME ].asString();
                }

                if( oObjElem.isMember( JSON_ATTR_RMTOBJNAME ) &&
                    oObjElem[ JSON_ATTR_RMTOBJNAME ].isString() )
                {
                    strRmtObjName =
                        oObjElem[ JSON_ATTR_RMTOBJNAME ].asString();
                }

                // this will be required by the 
                // CRpcPdoPort::SetupDBusSetting
                if( strRmtObjName.empty() && strRmtModName.empty() )
                {
                    // important for proxy
                    oCfg[ propDestDBusName ] = strDest;
                    oCfg[ propObjPath ] = strObjPath;
                }
                else
                {

                    if( strRmtObjName.empty() )
                        strRmtObjName = strInstName;

                    else if( strRmtModName.empty() )
                        strRmtModName = strSvrName;

                    strNewObjPath = DBUS_OBJ_PATH(
                        strRmtModName, strRmtObjName );
                    if( strNewObjPath.empty() )
                    {
                        ret = -EINVAL;
                        break;
                    }

                    strRmtSvrName = DBUS_DESTINATION2(
                        strRmtModName, strRmtObjName );
                    if( strRmtSvrName.empty() )
                    {
                        ret = -EINVAL;
                        break;
                    }

                    oCfg[ propDestDBusName ] = strRmtSvrName;
                    oCfg[ propObjPath ] = strNewObjPath;
                }
            }

            Json::Value& oIfArray = 
                oObjElem[ JSON_ATTR_IFARR ];            

            if( oIfArray == Json::Value::null ||
                !oIfArray.isArray() ||
                oIfArray.empty() )
                ret = -ENOENT;

            break;
        }

        if( ERROR( ret ) )
            break;

        if( i == oObjArray.size() )
        {
            ret = -ENOENT;
            break;
        }

        EnumMatchType iType =
            bServer ? matchServer : matchClient;
        oCfg[ propMatchType ] = iType;

        Json::Value& oIfArray =
            oObjArray[ i ][JSON_ATTR_IFARR ];
        
        i = 0;

        ObjVecPtr pObjVec( true );

        for( ; i < oIfArray.size(); i++ )
        {
            Json::Value& oIfDesc = oIfArray[ i ];

            // get the interface name
            Json::Value& oIfName =
                oIfDesc[ JSON_ATTR_IFNAME ];

            if( oIfName == Json::Value::null ||
                !oIfName.isString() )
                continue;
            
            bool bPausable = false;
            if( oIfDesc.isMember( JSON_ATTR_PAUSABLE ) &&
                oIfDesc[ JSON_ATTR_PAUSABLE ].isString() &&
                oIfDesc[ JSON_ATTR_PAUSABLE ].asString() == "true" )
            {
                bPausable = true;
            }

            bool bDummy = false;
            if( oIfDesc.isMember( JSON_ATTR_DUMMYIF ) &&
                oIfDesc[ JSON_ATTR_DUMMYIF ].isString() &&
                oIfDesc[ JSON_ATTR_DUMMYIF ].asString() == "true" )
            {
                bDummy = true;
            }

            if( oIfDesc.isMember( JSON_ATTR_BIND_CLSID ) &&
                oIfDesc[ JSON_ATTR_BIND_CLSID ].isString() &&
                oIfDesc[ JSON_ATTR_BIND_CLSID ].asString() == "true" )
            {
                // the master interface
                strVal = oIfName.asString().substr(
                    0, DBUS_MAXIMUM_NAME_LENGTH );
                if( strVal.empty() )
                {
                    ret = -EINVAL;
                    break;
                }
                oCfg[ propIfName ] = 
                    DBUS_IF_NAME( oIfName.asString() );

                if( bPausable )
                    oCfg[ propPausable ] = bPausable;

                if( bDummy )
                    oCfg[ propDummyMatch ] = true;

                continue;
            }

            MatchPtr pMatch;
            pMatch.NewObj( clsid( CMessageMatch ) );

            CCfgOpenerObj oMatch(
                ( CObjBase* )pMatch );

            oMatch.CopyProp( propObjPath, pCfg );

            oMatch.SetIntProp( propMatchType, iType );

            oMatch.SetStrProp( propIfName,
                DBUS_IF_NAME( oIfName.asString() ) );

            oMatch.SetBoolProp(
                propPausable, bPausable );

            if( bDummy )
            {
                oMatch.SetBoolProp(
                    propDummyMatch, true );
            }

            if( bServer )
            {
                oMatch.SetStrProp(
                    propSrcDBusName, strDest );
            }
            else
            {
                // allow each interface to have
                // its own destination
                std::string strRmtModName;
                std::string strRmtObjName;
                std::string strNewObjPath;
                std::string strRmtSvrName;

                if( oIfDesc.isMember( JSON_ATTR_RMTMODNAME ) &&
                    oIfDesc[ JSON_ATTR_RMTMODNAME ].isString() )
                {
                    strRmtModName = 
                        oIfDesc[ JSON_ATTR_RMTMODNAME ].asString();
                }

                if( oIfDesc.isMember( JSON_ATTR_RMTOBJNAME ) &&
                    oIfDesc[ JSON_ATTR_RMTOBJNAME ].isString() )
                {
                    strRmtObjName =
                        oIfDesc[ JSON_ATTR_RMTOBJNAME ].asString();
                }

                // this will be required by the 
                // CRpcPdoPort::SetupDBusSetting
                if( strRmtObjName.empty() || strRmtModName.empty() )
                {
                    oMatch.CopyProp( propDestDBusName, pCfg );
                }
                else
                {
                    strNewObjPath = DBUS_OBJ_PATH(
                        strRmtModName, strRmtObjName );

                    strRmtSvrName = DBUS_DESTINATION2(
                        strRmtModName, strRmtObjName );

                    oMatch.SetStrProp(
                        propDestDBusName, strRmtSvrName );

                    oMatch.SetStrProp(
                        propObjPath, strNewObjPath );
                }
            }

            ( *pObjVec )().push_back( pMatch );
        }

        if( ERROR( ret ) )
            break;

        if( ( *pObjVec )().size() )
        {
            ObjPtr pObj( pObjVec );
            oCfg.SetObjPtr( propObjList, pObj );
        }


        // get object array
        Json::Value& oFactores =
            valObjDesc[ JSON_ATTR_FACTORIES ];

        if( oFactores == Json::Value::null ||
            !oFactores.isArray() ||
            oFactores.empty() )
            break;

        for( i = 0; i < oFactores.size(); i++ )
        {
            if( oFactores[ i ].empty() ||
                !oFactores[ i ].isString() )
                continue;

            strPath = oFactores[ i ].asString();
            if( pMgr == nullptr )
            {
                CoLoadClassFactory(
                    strPath.c_str() );
            }
            else
            {
                pMgr->TryLoadClassFactory(
                    strPath );
            }
        }

    }while( 0 );

    return ret;
}

gint32 CRpcServices::PackEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData,
    CfgPtr& pDestCfg  )
{
    gint32 ret = 0;

    CParamList oParams;
    oParams.Push( ( guint32 )iEvent );

    switch( iEvent )
    {
    case eventPortStarted:
    case eventPortStopping:
    case eventPortStartFailed:
        {
            // push the port handle
            oParams.Push( dwParam1 );
            break;
        }
    case eventPortStopped:
        {
            break;
        }
    case eventModOnline:
    case eventModOffline:
        {
            oParams.Push( 0 );
            oParams.Push( 0 );

            string strMod =
                reinterpret_cast< char* >( pData );
            oParams.Push( strMod );

            break;
        }
    case eventRmtModOnline:
    case eventRmtModOffline:
        {

            IConfigDb* pEvtCtx = reinterpret_cast
                < IConfigDb* >( dwParam1 );

            oParams.Push( pEvtCtx );
            oParams.Push( 0 );

            string strMod =
                reinterpret_cast< char* >( pData );
            oParams.Push( strMod );

            break;
        }
    case eventDBusOnline:
    case eventDBusOffline:
        {
            break;
        }
    case eventRmtDBusOffline:
    case eventRmtDBusOnline:
    case eventRmtSvrOnline:
    case eventRmtSvrOffline:
        {
            IConfigDb* pEvtCtx = reinterpret_cast
                < IConfigDb* >( dwParam1 );
            oParams.Push( pEvtCtx );

            oParams.Push( 0 );

            HANDLE hPort = ( HANDLE )pData;
            oParams.Push( hPort );
            break;
        }
    default:
        {
            ret = -ENOTSUP;
            break;
        }
    }

    if( SUCCEEDED( ret ) )
        pDestCfg = oParams.GetCfg();
    return ret;
}

gint32 CRpcServices::UnpackEvent(
    CfgPtr& pSrcCfg,
    EnumEventId& iEvent,
    LONGWORD& dwParam1,
    LONGWORD& dwParam2,
    LONGWORD*& pData )
{
    gint32 ret = 0;

    CParamList oParams(
        ( IConfigDb*) pSrcCfg );

    iEvent = oParams[ 0 ];
    dwParam1 = 0;
    dwParam2 = 0;
    pData = nullptr;

    switch( iEvent )
    {
    case eventPortStarted:
    case eventPortStopping:
    case eventPortStartFailed:
        {
            // push the port handle
            dwParam1 = oParams[ 1 ];
            break;
        }
    case eventPortStopped:
        {
            break;
        }
    case eventModOnline:
    case eventModOffline:
        {
            BufPtr pBuf;
            ret = oParams.GetProperty( 3, pBuf );
            if( ERROR( ret ) )
                break;
            pData = ( LONGWORD* )pBuf->ptr();

            break;
        }
    case eventRmtModOnline:
    case eventRmtModOffline:
        {
            IConfigDb* pEvtCtx = nullptr;
            ret = oParams.GetPointer( 1, pEvtCtx );
            if( ERROR( ret ) )
                break;

            dwParam1 = ( LONGWORD )pEvtCtx;
            BufPtr pBuf;
            ret = oParams.GetProperty( 3, pBuf );
            if( ERROR( ret ) )
                break;
            pData = ( LONGWORD* )pBuf->ptr();
            break;
        }
    case eventDBusOnline:
    case eventDBusOffline:
        {
            break;
        }
    case eventRmtDBusOffline:
    case eventRmtDBusOnline:
    case eventRmtSvrOnline:
    case eventRmtSvrOffline:
        {
            IConfigDb* pEvtCtx = nullptr;
            ret = oParams.GetPointer( 1, pEvtCtx );
            if( ERROR( ret ) )
                break;

            dwParam1 = ( LONGWORD )pEvtCtx;

            HANDLE hPort = oParams[ 3 ];
            pData = ( LONGWORD* )hPort;
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

gint32 CRpcServices::OnEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData  )
{
    gint32 ret = 0;

    bool bProcess = false;

    CStdRMutex oIfLock( GetLock() );
    if( m_queEvents.size() == 0 )
        bProcess = true;

    CfgPtr pParams;
    ret = PackEvent( iEvent, dwParam1,
        dwParam2, pData, pParams );

    if( ERROR( ret ) )
        return ret;

    m_queEvents.push_back( pParams );

    if( !bProcess )
        return STATUS_PENDING;

    oIfLock.Unlock();

    do{
        super::OnEvent( iEvent,
            dwParam1, dwParam2, pData );
        do{
            oIfLock.Lock();
            m_queEvents.pop_front();
            if( m_queEvents.empty() )
                return 0;

            CfgPtr pParams =
                m_queEvents.front();

            oIfLock.Unlock();
            ret = UnpackEvent( pParams, iEvent,
                dwParam1, dwParam2, pData );

            if( SUCCEEDED( ret ) )
                break;

        }while( 1 );

    }while( 1 );

    return ret;
}

gint32 CRpcServices::DoModEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    EnumIfState iOldState = GetState();

    gint32 ret = m_pIfStat->OnModEvent(
        iEvent, strModule );

    if( SUCCEEDED( ret ) )
    {
        EnumIfState iNewState = GetState();
        if( ( iNewState == stateConnected ||
                iNewState == statePaused ) &&
            iOldState == stateRecovery )
        {
            // let's send new request to listen on the
            // events
            ret = RestartListening(
                stateRecovery );
        }
    }
    return ret;
}

gint32 CRpcServices::DoRmtModEvent(
    EnumEventId iEvent,
    const std::string& strModule,
    IConfigDb* pEvtCtx )
{

    EnumIfState iOldState = GetState();

    gint32 ret = m_pIfStat->OnRmtModEvent(
        iEvent, strModule, pEvtCtx );

    if( SUCCEEDED( ret ) )
    {
        EnumIfState iNewState = GetState();
        if( ( iNewState == stateConnected ||
                iNewState == statePaused ) &&
            iOldState == stateRecovery )
        {
            // let's send new request to listen on the
            // events
            ret = RestartListening(
                stateRecovery );
        }
    }
    return ret;
}

// a helper for deferred task to run in the
// interface's taskgroup
gint32 CRpcServices::RunManagedTask(
    IEventSink* pTask,
    const bool& bRoot )
{
    if( pTask == nullptr )
        return -EINVAL;

    TaskletPtr ptrTask(
        ( CTasklet* )pTask );

    if( ptrTask.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    if( bRoot )
    {
        ret = AppendAndRun( ptrTask );
    }
    else
    {
        ret = AddAndRun( ptrTask );
    }
    
    return ret;
}

gint32 CRpcServices::AddSeqTaskInternal(
    TaskGrpPtr& pQueuedTasks,
    TaskletPtr& pTask,
    bool bLong )
{
    return AddSeqTaskTempl(
        this, pQueuedTasks, pTask, bLong );
}

gint32 CRpcServices::AddSeqTask(
    TaskletPtr& pTask, bool bLong )
{
    return AddSeqTaskInternal(
        m_pSeqTasks, pTask, bLong );
}

gint32 CRpcServices::SetPortProp(
    IPort* pPort,
    guint32 dwPropId,
    BufPtr& pVal )
{
    gint32 ret = 0;
    if( pPort == nullptr || pVal.IsEmpty() )
        return -EINVAL;

    do{
        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr pCtx = pIrp->GetTopStack();
        pCtx->SetMajorCmd( IRP_MJ_SETPROP );

        CParamList oParams;
        oParams.Push( ObjPtr( pPort ) );

        oParams.Push( ( guint32 )dwPropId );
        oParams.Push( pVal );

        BufPtr pBuf( true );
        *pBuf = ObjPtr( oParams.GetCfg() );
        pCtx->SetReqData( pBuf );

        ret = GetIoMgr()->SubmitIrpInternal(
            pPort, pIrp, false );

    }while( 0 );

    return ret;
}

gint32 CRpcServices::GetPortProp(
    IPort* pPort,
    guint32 dwPropId,
    BufPtr& pVal )
{
    gint32 ret = 0;
    if( pPort == nullptr )
        return -EINVAL;

    do{
        IrpPtr pIrp( true );
        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr pCtx = pIrp->GetTopStack();
        pCtx->SetMajorCmd( IRP_MJ_GETPROP );

        CParamList oParams;
        oParams.Push( ObjPtr( pPort ) );
        oParams.Push( dwPropId );

        BufPtr pBuf( true );
        *pBuf = ObjPtr( oParams.GetCfg() );
        pCtx->SetReqData( pBuf );

        ret = GetIoMgr()->SubmitIrpInternal(
            pPort, pIrp, false );

        if( SUCCEEDED( ret ) )
            pVal = pCtx->m_pRespData;

    }while( 0 );

    return ret;

}
/**
* @name DoInvoke
* find a event handler for the event message
* pEvtMsg and handle it.
* This method is usually not expected to return
* STATUS_PENDING unless you are aware what you are
* doing
* @{ */
/** pEvtMsg [ in ]: the dbus message with the
 * event
 * pCallback [ in ]: the callback carrying the
 * related CIfInvokeMethodTask with the related
 * config parameters, and the callback in case the
 * call will complete asynchronously @} */
gint32 CInterfaceProxy::DoInvoke(
    DBusMessage* pEvtMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;

    do{
        if( pEvtMsg == nullptr ||
            pCallback == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        DMsgPtr pMsg( pEvtMsg );

        if( pMsg.GetType()
            != DBUS_MESSAGE_TYPE_SIGNAL )
        {
            ret = -EINVAL;
            break;
        }

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 0, pObj );
        if( ERROR( ret ) )
            break;

        IConfigDb* pCfg = pObj;
        if( pCfg == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        string strMethod = pMsg.GetMember();
        if( strMethod.empty() )
        {
            ret = -EBADMSG;
            break;
        }
        else if( strMethod ==
            SYS_EVENT_KEEPALIVE )
        {
            ret = OnKeepAlive(
                pCallback, KATerm );
            break;
        }

        do{
            CParamList oParams( pCfg );
            string strVal;
            strVal = pMsg.GetMember();
            if( !strVal.empty() )
                oParams.SetStrProp(
                    propMethodName, strVal );

            strVal = pMsg.GetInterface();
            if( !strVal.empty() )
                oParams.SetStrProp(
                    propIfName, strVal );

            strVal = pMsg.GetPath();
            if( !strVal.empty() )
                oParams.SetStrProp(
                    propObjPath, strVal );

            strVal = pMsg.GetSender();
            if( !strVal.empty() )
                oParams.SetStrProp(
                    propSrcDBusName, strVal );

            strVal = pMsg.GetDestination();
            if( !strVal.empty() )
                oParams.SetStrProp(
                    propDestDBusName, strVal );
        }while( 0 );

        ret = InvokeUserMethod(
            pCfg, pCallback );

    }while( 0 );

    return ret;
}

gint32 CInterfaceProxy::DoInvoke(
    IConfigDb* pEvtMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;

    do{
        if( pEvtMsg == nullptr ||
            pCallback == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CReqOpener oReq( pEvtMsg );
        gint32 iReqType = 0;

        ret = oReq.GetReqType(
            ( guint32& )iReqType ); 

        if( ERROR( ret ) )
            break;

        if( iReqType != DBUS_MESSAGE_TYPE_SIGNAL )
        {
            ret = -EINVAL;
            break;
        }

        string strMethod;
        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        ret = InvokeUserMethod(
            pEvtMsg, pCallback );

    }while( 0 );

    return ret;
}

gint32 CInterfaceProxy::OnCancel_Proxy(
    IConfigDb* pReqMsg,
    IEventSink* pCallback )
{
    if( pReqMsg == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        TaskletPtr pTasklet;
        pTasklet = ObjPtr( pCallback ); 
        CIfParallelTask* pTask = pTasklet;

        if( pTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CInterfaceProxy::OnKeepAlive(
    IEventSink* pTask, EnumKAPhase iPhase )
{
    if( pTask == nullptr )
        return -EINVAL;

    if( iPhase == KATerm )
    {
        return OnKeepAliveTerm( pTask );
    }

    return -EINVAL;
}

gint32 CInterfaceProxy::OnKeepAliveTerm(
    IEventSink* pTask )
{
    gint32 ret = 0;

    // servicing the incoming request/event
    // message
    do{
        DMsgPtr pMsg;
        CCfgOpenerObj oCfg( pTask );

        ret = oCfg.GetMsgPtr( propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        if( pMsg.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 0, pObj );
        if( ERROR( ret ) )
            break;

        
        IConfigDb* pCfg = pObj;
        if( pCfg == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CReqOpener oEvent( pCfg );
        guint64 iTaskId = 0;
        ret = oEvent.GetQwordProp( 0, iTaskId );
        if( ERROR( ret ) )
            break;

        guint32 iPhase = 0;
        ret = oEvent.GetIntProp( 1, iPhase );
        if( ERROR( ret ) )
            break;

        if( iPhase != ( guint32 )KATerm )
        {
            ret = -EINVAL;
            break;
        }

        TaskGrpPtr pRootGrp;
        TaskletPtr pTaskToKa;
        if( true )
        {
            CStdRMutex oIfLock( GetLock() );
            if( !IsConnected() )
            {
                ret = ERROR_STATE;
                break;
            }
            pRootGrp = GetTaskGroup();
            if( pRootGrp.IsEmpty() )
            {
                ret = -ENOENT;    
                break;
            }
        }

        ret = pRootGrp->FindTask(
            iTaskId, pTaskToKa );

        if( ERROR( ret ) )
            break;

        if( pTaskToKa.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        CIfIoReqTask* pReqTask = pTaskToKa;
        if( pReqTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        // send a keepalive event
        EnumEventId iEvent = ( EnumEventId )
            ( eventTryLock | eventKeepAlive );

        pTaskToKa->OnEvent( iEvent,
            KATerm, 0, nullptr );

    }while( 0 );

    return ret;
}

gint32 CInterfaceProxy::SendResponse(
    IEventSink* pInvTask,
    DBusMessage* pReqMsg,
    CfgPtr& pRespData )
{
    // called to send back the response
    return 0;
}

gint32 CInterfaceProxy::SendResponse(
    IEventSink* pInvTask,
    IConfigDb* pReqMsg,
    CfgPtr& pRespData )
{
    // called to send back the response
    return 0;
}

// don't use this method unless you know what you are doing
gint32 CInterfaceProxy::SendProxyReq(
    IEventSink* pCallback,
    bool bNonDBus,
    const string& strMethod,
    std::vector< BufPtr >& vecParams,
    guint64& qwIoTaskId )
{
    gint32 ret = 0;
    bool bSync = false;

    if( pCallback == nullptr )
    {
        // we won't let the call afterwards
        // without a callback
        bSync = true;
        TaskletPtr pTask;
        ret = pTask.NewObj( clsid( CSyncCallback ) );
        if( ERROR( ret ) )
            return ret;
        pCallback = pTask;
    }

    do{
        CReqBuilder oReq( this );

        oReq.SetMethodName( strMethod );

        for( auto& pBuf: vecParams )
            oReq.Push( pBuf );

        guint32 dwFlags = CF_ASYNC_CALL |
            DBUS_MESSAGE_TYPE_METHOD_CALL |
            CF_WITH_REPLY | CF_KEEP_ALIVE;

        if( bNonDBus )
        {
            dwFlags |= CF_NON_DBUS;
        }
        else
        {
            // FIXME: what does this means
            oReq.SetIntProp( propCmdId,
                CTRLCODE_SEND_REQ );
        }

        oReq.SetCallFlags( dwFlags );

        // overwrite the strIfName if exist in the
        // pCallback
        CopyUserOptions(
            oReq.GetCfg(), pCallback );

        CfgPtr pResp( true );
        gint32 iRet = 0;

        ret = RunIoTask( oReq.GetCfg(),
            pResp, pCallback, &qwIoTaskId );

        if( ret == STATUS_PENDING )
        {
            if( bSync )
            {
                CSyncCallback* pSyncCallback =
                    ObjPtr( pCallback );
                if( pSyncCallback == nullptr )
                {
                    ret = -EFAULT;
                    break;
                }
                ret = pSyncCallback->WaitForComplete();
                if( SUCCEEDED( ret ) )
                    ret = pSyncCallback->GetError();
            }
            else
            {
                break;
            }
        }

        if( ERROR( ret ) )
            break;

        CReqOpener oReq2( oReq.GetCfg() );
        oReq2.GetCallFlags( dwFlags );
        if( !( dwFlags & CF_WITH_REPLY ) )
            break;

        CReqOpener oResp( ( IConfigDb* )pResp );
        ret = oResp.GetReturnValue( iRet );

        if( ERROR( ret ) )
            break;

        if( !bSync )
        {
            // sometimes we can land here successfully
            // in the highly concurrent environment
            CCfgOpenerObj oCallback( pCallback );
            oCallback.SetObjPtr( propRespPtr,
                ObjPtr( pResp ) );
        }

        ret = iRet;

    }while( 0 );

    return ret;
}

gint32 CInterfaceProxy::CustomizeRequest(
    IConfigDb* pReqCfg,
    IEventSink* pCallback )
{
    // this is the last moment to make changes to the
    // request before being executed.
    CParamList oParams( pReqCfg );
    std::string strMethod;

    gint32 ret = oParams.GetStrProp(
        propMethodName, strMethod );

    if( ERROR( ret ) )
        return ret;

    if( strMethod == SYS_METHOD_SENDDATA ||
        strMethod == SYS_METHOD_FETCHDATA )
        return CustomizeSendFetch(
            pReqCfg, pCallback );

    return 0;
}

gint32 CInterfaceProxy::CustomizeSendFetch(
    IConfigDb* pReqCfg,
    IEventSink* pCallback )
{
    gint32 ret = 0;
    do{
        CParamList oParams( pReqCfg );
        IConfigDb* pDesc;
        ret = oParams.GetPointer( 0, pDesc );
        if( ERROR( ret ) )
            break;

        // note: the remote handler can only have the
        // taskid from the parameter desc, but not the
        // pReqCfg.
        CReqOpener oDesc( pDesc );
        oDesc.CopyProp( propTaskId, pReqCfg );

        EnumClsid iid = clsid( Invalid );
        guint32* piid = ( guint32* )&iid;
        ret = oDesc.GetIntProp(
            propIid, *piid );

        if( ERROR( ret ) )
            break;

        // IStream has different keep-alive method
        if( iid == iid( IStream ) )
        {
            guint32 dwFlags = 0;
            ret = oDesc.GetCallFlags( dwFlags );
            if( ERROR( ret ) )
                break;
            dwFlags &= ~CF_KEEP_ALIVE;
            CReqBuilder oBuilder( pDesc );
            oBuilder.SetCallFlags( dwFlags );
            break;
        }
    }while( 0 );

    return ret;
}

gint32 CInterfaceProxy::InitUserFuncs()
{
    gint32 ret = super::InitUserFuncs();
    if( ERROR( ret ) )
        return ret;

    // since we are contacting the remote
    // IInterfaceServer, so the interface name should
    // be IInterfaceServer
    BEGIN_IFPROXY_MAP( IInterfaceServer, false );

    ADD_PROXY_METHOD( SYS_METHOD_USERCANCELREQ, guint64 );
    ADD_PROXY_METHOD( SYS_METHOD_PAUSE );
    ADD_PROXY_METHOD( SYS_METHOD_RESUME );
    ADD_PROXY_METHOD( SYS_METHOD_KEEPALIVEREQ, guint64 );

    END_IFPROXY_MAP;

    return 0;
}

// a user-initialized cancel request
gint32 CInterfaceProxy::UserCancelRequest(
	guint64& qwThisTaskId,
	guint64 qwTaskToCancel )
{
	if( qwTaskToCancel == 0 )
		return -EINVAL;

    const string& strIfName = CoGetIfNameFromIid(
        iid( IInterfaceServer ), "p" );

    if( strIfName.empty() )
        return ERROR_FAIL;

    CParamList oOptions;
    // FIXME: not a good solution
    oOptions[ propIfName ] =
        DBUS_IF_NAME( strIfName );

    oOptions[ propSysMethod ] = true;

    CfgPtr pResp;
    // make the call
    std::string strMethod( __func__ );
    gint32 ret = SyncCallEx(
        oOptions.GetCfg(),
        pResp, strMethod,
        qwTaskToCancel );

    if( ERROR( ret ) )
        return ret;

    // fill the return code
    gint32 iRet = 0;
    ret = FillArgs( pResp, iRet );

    if( SUCCEEDED( ret ) )
        ret = iRet;

    if( SUCCEEDED( ret ) )
    {
        DebugPrint( 0,
            "Req Task Canceled, 0x%llx",
            qwTaskToCancel );
    }
    return ret;
}

gint32 CInterfaceProxy::Pause_Proxy()
{
    return PauseResume_Proxy( "Pause" );
}
gint32 CInterfaceProxy::Resume_Proxy()
{
    return PauseResume_Proxy( "Resume" );
}

gint32 CInterfaceProxy::PauseResume_Proxy(
    const std::string& strMethod )
{
    CfgPtr pResp;

    // we are from IInterfaceServer
    CParamList oOptions;

    const std::string& strIfName =
        CoGetIfNameFromIid(
            iid( IInterfaceServer ), "p" );

    if( strIfName.empty() )
        return -ENOTSUP;

    oOptions[ propIfName ] =
        DBUS_IF_NAME( strIfName );

    oOptions[ propSysMethod ] = true;

    // make the call
    gint32 ret = SyncCallEx(
        oOptions.GetCfg(),
        pResp, strMethod );

    if( ERROR( ret ) )
        return ret;

    // fill the return code
    gint32 iRet = 0;
    ret = FillArgs( pResp, iRet );

    if( SUCCEEDED( ret ) )
        ret = iRet;

    return ret;
}

// reset the timer on server side
gint32 CInterfaceProxy::KeepAliveRequest(
    guint64 qwTaskId )
{
	if( qwTaskId == 0 )
		return -EINVAL;

    const string& strIfName = CoGetIfNameFromIid(
        iid( IInterfaceServer ), "p" );

    if( strIfName.empty() )
        return ERROR_FAIL;

    CParamList oOptions;

    oOptions[ propIfName ] =
        DBUS_IF_NAME( strIfName );
    oOptions[ propSysMethod ] = true;

    // this is a one way request
    oOptions[ propNoReply ] = true;

    CfgPtr pResp;
    // make the call
    std::string strMethod( __func__ );

    TaskletPtr pDummy;
    pDummy.NewObj( clsid( CIfDummyTask ) );

    return AsyncCall( pDummy,
        oOptions.GetCfg(),
        pResp, strMethod, qwTaskId );
}

gint32 CInterfaceProxy::CancelRequest(
    guint64 qwTaskId )
{
    if( qwTaskId == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        // this is a built-in request
        guint64 qwThisTask = 0;
        ret = UserCancelRequest(
            qwThisTask, qwTaskId );

        if( SUCCEEDED( ret ) )
            break;

        TaskGrpPtr pTaskGrp;
        ret = GetParallelGrp( pTaskGrp );
        if( ERROR( ret ) )
            break;

        TaskletPtr pTaskCancel;
        ret = pTaskGrp->FindTask(
            qwTaskId, pTaskCancel );

        if( ERROR( ret ) )
            break;

        ret = DEFER_CALL(
            GetIoMgr(), pTaskCancel,
            &IEventSink::OnEvent,
            eventUserCancel, ERROR_USER_CANCEL,
            0, nullptr );

   }while( 0 ); 

   return ret;
}

gint32 CInterfaceProxy::CopyUserOptions(
    CObjBase* pDest,
    CObjBase* pSrc )
{
    if( pDest == nullptr || pSrc == nullptr )
        return -EINVAL;
    
    CCfgOpenerObj oCfg( pDest );
    oCfg.CopyProp( propIfName, pSrc );
    oCfg.CopyProp( propSeriProto, pSrc );
    if( oCfg.exist( propCallOptions ) )
    {
        IConfigDb* pCfg = nullptr;
        oCfg.GetPointer(
            propCallOptions, pCfg );
        CCfgOpener oCallOpt( pCfg );
        oCallOpt.CopyProp(
            propKeepAliveSec, pSrc);
        oCallOpt.CopyProp(
            propTimeoutSec, pSrc);

        CCfgOpenerObj oSrc( pSrc );
        bool bNoReply = false;
        gint32 ret = oSrc.GetBoolProp(
            propNoReply, bNoReply );
        if( SUCCEEDED( ret ) && bNoReply )
        {
            guint32 dwFlags;
            ret = oCallOpt.GetIntProp(
                propCallFlags, dwFlags );
            if( SUCCEEDED( ret ) )
            {
                dwFlags &= ~CF_WITH_REPLY;
                oCallOpt.SetIntProp(
                    propCallFlags, dwFlags );
            }
        }
    }
    else
    {
        oCfg.CopyProp( propKeepAliveSec, pSrc );
        oCfg.CopyProp( propTimeoutSec, pSrc );
        oCfg.CopyProp( propNoReply, pSrc );
    }
    return 0;
}

static CfgPtr InitIfSvrCfg(
    const IConfigDb * pCfg )
{
    CCfgOpener oOldCfg( pCfg );
    if( oOldCfg.exist( propIfStateClass ) )
        return CfgPtr( const_cast
            < IConfigDb* >( pCfg ) );

    gint32 ret = 0;
    CCfgOpener oNewCfg;
    do{
        *oNewCfg.GetCfg() = *pCfg;

        string strPortClass;
        ret = oNewCfg.GetStrProp(
            propPortClass, strPortClass );

        if( ERROR( ret ) )
            break;

        guint32 iStateClass = clsid( CIfServerState );
        if( strPortClass == PORT_CLASS_UXSOCK_STM_PDO )
            iStateClass = clsid( CUnixSockStmState );

        oNewCfg.SetIntProp(
            propIfStateClass,
            iStateClass );

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error in InitIfSvrCfg ctor" );
        throw std::runtime_error( strMsg );
    }

    return oNewCfg.GetCfg();
}

gint32 CInterfaceServer::RestartListening(
    EnumIfState iStateOld  )
{
    gint32 ret = 0;

    EnumIfState iState = GetState();
    gint32 iStartType = 0;
    if( iState == stateConnected && 
        iStateOld == statePaused )
        iStartType = 1;

    else if( iState == stateConnected &&
        iStateOld == stateRecovery )
        iStartType = 2;

    else if( iState == statePaused &&
        iStateOld == stateRecovery )
        iStartType = 3;
    else
    {
        return  -ENOTSUP;
    }

    CStdRMutex oIfLock( GetLock() );
    std::vector< MatchPtr > vecMatches = m_vecMatches;
    oIfLock.Unlock();

    for( auto pMatch : vecMatches )
    {
        CMessageMatch* pMsgMatch = pMatch;
        if( pMsgMatch == nullptr )
            continue;

            // check if propPausable exist
        if( pMsgMatch->exist( propPausable ) )
        {
            bool bPausable = false;

            CCfgOpenerObj oMatch(
                ( CObjBase* ) pMatch );

            oMatch.GetBoolProp(
                propPausable, bPausable );
            if( iStartType == 1 && !bPausable )
            {
                // start paused listening
                continue;
            }
            else if( iStartType == 3 && bPausable )
            {
                // start unpausable interfaces
                continue;
            }
            else
            {
                // start all the interfaces
            }
        }
        SetReqQueSize( pMsgMatch, MAX_DBUS_REQS );
    }

    return ret;
}

CInterfaceServer::CInterfaceServer(
    const IConfigDb* pCfg )
    : super( InitIfSvrCfg( pCfg ) )
{
    gint32 ret = 0;

    do{
        CCfgOpener oCfg( pCfg );
        string strPortClass;

        if( !oCfg.exist( propPortClass ) )
        {
            ret = -EINVAL;
            break;
        }

        ret = m_pIfMatch.NewObj(
            clsid( CMessageMatch ), pCfg );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj matchCfg(
            ( IMessageMatch* ) m_pIfMatch );

        // no propDestDBusName
        ret = matchCfg.SetStrProp(
            propDestDBusName,
            DBUS_DESTINATION( MODNAME_INVALID ) );
        

        ret = matchCfg.CopyProp(
            propIfName, pCfg );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.CopyProp(
            propObjPath, pCfg );

        if( ERROR( ret ) )
            break;

        matchCfg.CopyProp(
            propPausable, pCfg );

        ret = matchCfg.SetIntProp(
            propMatchType,
            ( guint32 )matchServer );

        if( ERROR( ret ) )
            break;

        ret = matchCfg.CopyProp(
            propDummyMatch, pCfg );

        if( SUCCEEDED( ret ) )
            RemoveProperty( propDummyMatch );

        ret = 0;

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error in CInterfaceServer ctor" );
        throw std::runtime_error( strMsg );
    }
}

CInterfaceServer::~CInterfaceServer()
{
    if( !m_pIfStat.IsEmpty() )
        m_pIfStat.Clear();

    if( !m_pIfMatch.IsEmpty() )
        m_pIfMatch.Clear();
}

// called to hook the response to the task for
// sending back
gint32 CInterfaceServer::SetResponse(
    IEventSink* pTask,
    CfgPtr& pRespData )
{
    gint32 ret = 0;
    if( pTask == nullptr ||
        pRespData.IsEmpty() )
        return -EINVAL;
    
    CCfgOpenerObj oTaskCfg( pTask );
    ObjPtr pObj;
    pObj = pRespData;
    if( pObj.IsEmpty() )
        return -EFAULT;

    // set the response pointer
    ret = oTaskCfg.SetObjPtr(
        propRespPtr, pObj );

    return ret;
}

gint32 CInterfaceServer::ValidateRequest_SendData(
    DBusMessage* pReqMsg, IConfigDb* pDataDesc )
{
    gint32 ret = 0;

    if( pDataDesc == nullptr ||
        pReqMsg == nullptr )
        return -EINVAL;

    do{
        EnumClsid iid = clsid( Invalid );
        EnumClsid iidClient = clsid( Invalid );

        CCfgOpener oDataDesc(
            ( IConfigDb* )pDataDesc );

        guint32* piid = ( guint32* )&iidClient;
        ret = oDataDesc.GetIntProp(
            propIid, *piid );

        if( ERROR( ret ) )
            break;

        // we need filter further to confirm
        // the SENDDATA is bound to the correct
        // interface
        if( m_pFtsMatch.IsEmpty() &&
            m_pStmMatch.IsEmpty() )
        {
            // the interface is not up
            ret = -EBADMSG;
            break;
        }

        if( !m_pFtsMatch.IsEmpty() )
        {
            ret = m_pFtsMatch->
                IsMyMsgIncoming( pReqMsg );

            if( SUCCEEDED( ret ) )
            {
                CCfgOpenerObj oMatchCfg(
                    ( CObjBase* )m_pFtsMatch );

                guint32* piid = ( guint32* )&iid;
                oMatchCfg.GetIntProp(
                    propIid, *piid );

                if( iid != iidClient )
                {
                    ret = -EBADMSG;
                    break;
                }
            }
        }

        gint32 ret2 = STATUS_SUCCESS;

        if( !m_pStmMatch.IsEmpty() )
        {
            ret2 = m_pStmMatch->
                IsMyMsgIncoming( pReqMsg );

            if( SUCCEEDED( ret2 ) )
            {
                CCfgOpenerObj oMatchCfg(
                    ( CObjBase* )m_pStmMatch );

                guint32* piid = ( guint32* )&iid;
                oMatchCfg.GetIntProp(
                    propIid, *piid );

                if( iid != iidClient )
                {
                    ret = -EBADMSG;
                    break;
                }
            }
        }

        if( ERROR( ret  ) && ERROR( ret2 ) )
        {
            ret = -EBADMSG;
            break;
        }

        DMsgPtr pMsg( pReqMsg );
        string strMethod = pMsg.GetMember();
        if( strMethod.empty() )
        {
            ret = -EBADMSG;
            break;
        }

        // uncomment this if it is supported
        // in the future
        if( iidClient == iid( IStream ) &&
            strMethod == SYS_METHOD_SENDDATA )
        {
            ret = -EBADMSG;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::DoInvoke_SendData(
    DBusMessage* pReqMsg,
    IEventSink* pCallback,
    CParamList& oResp )
{
    gint32 ret = 0;

    do{
        CCfgOpenerObj oCfg( this );
        DMsgPtr pMsg( pReqMsg );

        string strMethod = pMsg.GetMember();
        if( strMethod.empty() )
        {
            ret = -EBADMSG;
            break;
        }

        string strSender = pMsg.GetSender();

        vector< ARG_ENTRY > vecArgs;
        ret = pMsg.GetArgs( vecArgs );
        if( ERROR( ret ) )
            break;

        CfgPtr pDataDesc( true );

        ret = pDataDesc->Deserialize(
            *( CBuffer* )vecArgs[ 0 ].second );

        if( ret == -ENOTSUP )
        {
            // ENOTSUP is used for default
            // handling, and this is not the case
            ret = -EINVAL;
        }

        if( ERROR( ret ) )
            break;

        do{
            ret = ValidateRequest_SendData(
                pReqMsg, pDataDesc );

            if( ERROR( ret ) )
                break;

            if( strMethod == SYS_METHOD_SENDDATA )
            {
                guint32 iArgCount = GetArgCount(
                    &CInterfaceServer::SendData );

                if( vecArgs.empty()
                    || vecArgs.size() != iArgCount )
                {
                    ret = -EBADMSG;
                    break;
                }

                int fd =
                    ( gint32& )*vecArgs[ 1 ].second;

                guint32 dwOffset =
                    ( guint32& )*vecArgs[ 2 ].second;

                guint32 dwSize =
                    ( guint32& )*vecArgs[ 3 ].second;

                if( pDataDesc->exist( propCallOptions ) )
                {
                    CfgPtr pCallOpt( true );
                    if( !pCallOpt.IsEmpty() )
                    {
                        ret = oCfg.CopyProp(
                            propCallOptions,
                            ( IConfigDb* )pDataDesc );
                    }
                }

                ret = SendData( pDataDesc, fd,
                    dwOffset, dwSize, pCallback );

                if( ret == STATUS_PENDING )
                {
                    // keep-alive settings
                    CCfgOpenerObj oTaskCfg( pCallback );
                    SET_RMT_TASKID(
                        ( IConfigDb* )pDataDesc, oTaskCfg );
                    oTaskCfg.SetObjPtr(
                        propReqPtr, ObjPtr( pDataDesc ) );
                    break;
                }
                ret = oResp.SetIntProp(
                    propReturnValue, ret );

                if( SUCCEEDED( ret ) )
                {
                    SetResponse( pCallback,
                        oResp.GetCfg() );
                }

                if( fd > 0 )
                {
                    close( fd );
                    fd = -1;
                }
                break;
            }
            else if( strMethod == SYS_METHOD_FETCHDATA )
            {
                guint32 iArgCount = GetArgCount(
                    &CInterfaceServer::FetchData );

                if( vecArgs.empty()
                    || vecArgs.size() != iArgCount )
                {
                    ret = -EBADMSG;
                    break;
                }

                if( pDataDesc->exist( propCallOptions ) )
                {
                    CfgPtr pCallOpt( true );
                    if( !pCallOpt.IsEmpty() )
                    {
                        ret = oCfg.CopyProp(
                            propCallOptions,
                            ( IConfigDb* )pDataDesc );
                    }
                }

                int fd =
                    ( guint32& )*vecArgs[ 1 ].second;

                guint32 dwOffset =
                    ( guint32& )*vecArgs[ 2 ].second;

                guint32 dwSize =
                    ( guint32& )*vecArgs[ 3 ].second;

                ret = FetchData( pDataDesc, fd,
                    dwOffset, dwSize, pCallback );

                if( ret == STATUS_PENDING )
                {
                    // keep-alive settings
                    CCfgOpenerObj oTaskCfg( pCallback );
                    SET_RMT_TASKID(
                        ( IConfigDb* )pDataDesc, oTaskCfg );

                    oTaskCfg.SetObjPtr(
                        propReqPtr, ObjPtr( pDataDesc ) );

                    break;
                }

                gint32 ret2 = oResp.SetIntProp(
                    propReturnValue, ret );

                if( SUCCEEDED( ret2 ) &&
                    SUCCEEDED( ret ) )
                {
                    oResp.Push( ObjPtr( pDataDesc ) );
                    oResp.Push( fd );
                    oResp.Push( dwOffset );
                    oResp.Push( dwSize );
                    // bCloseFile = true;
                    // iFd2Close = fd;
                    
                }
                else if( SUCCEEDED( ret ) )
                {
                    ret = ret2;
                }

                if( SUCCEEDED( ret ) )
                {
                    // on success , we need to set the
                    // response immediately
                    SetResponse( pCallback,
                        oResp.GetCfg() );
                }
                else
                {
                    // SetResponse at the end and here
                    // close the fd only
                    if( fd >= 0 )
                        close( fd );
                }
                break;
            }

        }while( 0 );

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::CheckReqCtx(
    IEventSink* pCallback,
    DMsgPtr& pMsg )
{
    if( pCallback == nullptr || pMsg.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        ObjPtr pCtxObj;
        IConfigDb* pReqCtx;
        ret = pMsg.GetObjArgAt( 1, pCtxObj );
        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        pReqCtx = pCtxObj;
        if( pReqCtx == nullptr )
        {
            // ipc clients
            break;
        }

        TaskletPtr pCb = ObjPtr( pCallback );
        CCfgOpener oTaskCfg(
            ( IConfigDb* )pCb->GetConfig() );

        oTaskCfg.CopyProp(
            propRouterPath, pReqCtx );

        oTaskCfg.CopyProp(
            propSessHash, pReqCtx );

        CCfgOpener oReqCtx( pReqCtx );
        oTaskCfg.CopyProp(
            propTimestamp, pReqCtx );

        CIfInvokeMethodTask* pInv =
            ObjPtr( pCallback );

        if( likely( pInv != nullptr ) &&
            pInv->GetTimeLeft() <= 0 )
        {
            ret = -ETIMEDOUT;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::DoInvoke(
    DBusMessage* pReqMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;

    do{
        bool bResp = false;
        DMsgPtr pMsg( pReqMsg );
        CParamList oResp;

        string strMethod = pMsg.GetMember();
        if( strMethod.empty() )
        {
            ret = -EBADMSG;
            break;
        }

        string strSender = pMsg.GetSender();

        if( strMethod == SYS_METHOD_SENDDATA ||
            strMethod == SYS_METHOD_FETCHDATA )
        {
            ret = DoInvoke_SendData(
                pReqMsg, pCallback, oResp );
        }
        else do{

            ret = 0;
            ObjPtr pObj;

            ret = pMsg.GetObjArgAt( 0, pObj );
            if( ERROR( ret ) )
            {
                ret = -EBADMSG;
                break;
            }

            IConfigDb* pCfg = pObj;
            if( pCfg == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            SvrConnPtr pConnMgr = GetConnMgr();
            if( !pConnMgr.IsEmpty() )
            {
                pConnMgr->OnInvokeMethod(
                    ( HANDLE )( ( CObjBase* )pCallback ),
                    strSender );
            }

            do{
                CParamList oParams( pCfg );
                string strVal;
                strVal = pMsg.GetMember();
                if( !strVal.empty() )
                    oParams.SetStrProp(
                        propMethodName, strVal );

                strVal = pMsg.GetInterface();
                if( !strVal.empty() )
                    oParams.SetStrProp(
                        propIfName, strVal );

                strVal = pMsg.GetPath();
                if( !strVal.empty() )
                    oParams.SetStrProp(
                        propObjPath, strVal );

                strVal = pMsg.GetSender();
                if( !strVal.empty() )
                    oParams.SetStrProp(
                        propSrcDBusName, strVal );

                strVal = pMsg.GetDestination();
                if( !strVal.empty() )
                    oParams.SetStrProp(
                        propDestDBusName, strVal );
            }while( 0 );

            // for keep-alive purpose
            TaskletPtr pCb = ObjPtr( pCallback );
            CCfgOpener oTaskCfg(
                ( IConfigDb* )pCb->GetConfig() );
            SET_RMT_TASKID( pCfg, oTaskCfg );

            // copy the request for reference
            // later
            oTaskCfg.SetObjPtr(
                propReqPtr, ObjPtr( pCfg ) );

            ret = CheckReqCtx( pCallback, pMsg );
            if( ERROR( ret ) )
                break;

            ret = InvokeUserMethod(
                pCfg, pCallback );

            CReqOpener oReq( pCfg );
            guint32 dwFlags = 0;
            gint32 iRet = oReq.GetCallFlags( dwFlags );

            if( SUCCEEDED( iRet ) && 
                ( dwFlags & CF_WITH_REPLY ) )
                bResp = true;

            if( ret != STATUS_PENDING )
            {
                oTaskCfg.RemoveProperty(
                    propRmtTaskId );
            }

        }while( 0 );

        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) && bResp )
        {
            oResp.SetIntProp(
                propReturnValue, ret );

            SetResponse( pCallback,
                oResp.GetCfg() );
        }

    }while( 0 );


    return ret;
}

gint32 CInterfaceServer::DoInvoke(
    IConfigDb* pEvtMsg,
    IEventSink* pCallback )
{
    gint32 ret = 0;

    do{
        if( pEvtMsg == nullptr ||
            pCallback == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CReqOpener oReq( pEvtMsg );
        gint32 iReqType = 0;

        ret = oReq.GetReqType(
            ( guint32& )iReqType ); 

        if( ERROR( ret ) )
            break;

        if( iReqType != DBUS_MESSAGE_TYPE_METHOD_CALL )
        {
            ret = -EINVAL;
            break;
        }

        string strMethod;
        ret = oReq.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;

        ret = InvokeUserMethod(
            pEvtMsg, pCallback );

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::OnServiceComplete(
    IConfigDb* pResp,
    IEventSink* pCallback )
{
    // this method is called from outside the
    // CIfInvokeMethodTask if the invoke method is
    // complete asynchronously
    if( pResp == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CfgPtr pRespPtr( pResp );
    ret = SetResponse(
        pCallback, pRespPtr );

    if( ERROR( ret ) )
        return ret;

    gint32 iRet = 0;
    CCfgOpener oCfg( pResp );

    ret = oCfg.GetIntProp(
        propReturnValue, ( guint32& )iRet );

    if( ERROR( ret ) )
        return ret;

    pCallback->OnEvent(
        eventTaskComp, iRet, 0, 0 );

    return ret;
}

gint32 CInterfaceServer::SendResponse(
    IEventSink* pInvTask,
    DBusMessage* pReqMsg,
    CfgPtr& pRespData )
{
    // called to send back the response
    if( pReqMsg == nullptr ||
        pRespData.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0; 
    do{
        DMsgPtr pMsg( pReqMsg );
        if( pMsg.GetType() ==
            DBUS_MESSAGE_TYPE_METHOD_RETURN )
        {
            ret = -EINVAL;
            break;
        }

        string strMethod = pMsg.GetMember( );
        DMsgPtr pRespMsg;
        ret = pRespMsg.NewResp( pReqMsg );
        if( ERROR( ret ) )
            break;

        //DebugPrint( 0, "%s resp...", strMethod.c_str() );
        CParamList oParams(
            ( IConfigDb* )pRespData );

        gint32 iRet = 0;
        ret = oParams.GetIntProp(
            propReturnValue, ( guint32& )iRet );

        if( ERROR( ret ) )
            break;

        bool bNonFd = false;
        if( strMethod == SYS_METHOD_SENDDATA )
        {
            
            if( !dbus_message_append_args( pRespMsg,
                DBUS_TYPE_UINT32, &iRet ,
                DBUS_TYPE_INVALID ) )
            {
                ret = -ENOMEM;
                break;
            }
        }
        else if( strMethod == SYS_METHOD_FETCHDATA )
        {
            guint32 dwSize = 0;
            guint32 dwOffset = 0;
            gint32 iFd = -1;
            ObjPtr pDataDesc;
            BufPtr pBuf( true );

            if( SUCCEEDED( iRet ) )
            {
                ret = oParams.Pop( dwSize );
                if( ERROR( ret ) )
                    break;

                ret = oParams.Pop( dwOffset );
                if( ERROR( ret ) )
                    break;

                ret = oParams.Pop( iFd );
                if( ERROR( ret ) )
                    break;

                ret = oParams.Pop( pDataDesc );
                if( ERROR( ret ) )
                    break;

                ret = pDataDesc->Serialize( *pBuf );
                if( ERROR( ret ) )
                    break;

                oParams.GetBoolProp(
                    propNonFd, bNonFd );
            }

            gint32 iFdType = DBUS_TYPE_UNIX_FD;

#ifdef _WIN32
            if( iFd >= 0 )
            {
                // No fd transfer in the Windows
                // world yet
                ret = -ENOTSUP;
                break;
            }
#else
            if( iFd <= 0 || bNonFd )
                iFdType = DBUS_TYPE_UINT32;
#endif

            if( SUCCEEDED( iRet ) )
            {
                const char* pData = pBuf->ptr();
                if( !dbus_message_append_args( pRespMsg,
                    DBUS_TYPE_UINT32, &iRet,
                    DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                    &pData, pBuf->size(),
                    iFdType, &iFd,
                    DBUS_TYPE_UINT32, &dwOffset,
                    DBUS_TYPE_UINT32, &dwSize,
                    DBUS_TYPE_INVALID ) )
                {
                    ret = -ENOMEM;
                    break;
                }
                close( iFd );
            }
            else
            {
                if( !dbus_message_append_args( pRespMsg,
                    DBUS_TYPE_UINT32, &iRet,
                    DBUS_TYPE_INVALID ) )
                {
                    ret = -ENOMEM;
                    break;
                }
            }
        }
        else if( strMethod == SYS_METHOD_FORWARDREQ )
        {
            DMsgPtr pMsg2Fwdr;
            BufPtr pBuf( true );

            ret = oParams.Pop( pMsg2Fwdr );
            if( SUCCEEDED( ret ) )
            {
                if( pMsg2Fwdr.IsEmpty() )
                {
                    ret = -EFAULT;
                    break;
                }
                ret = pMsg2Fwdr.Serialize( *pBuf );
                if( ERROR( ret ) )
                    break;
            }

            if( pBuf->size() )
            {
                const char* pData = pBuf->ptr();
                if( !dbus_message_append_args( pRespMsg,
                    DBUS_TYPE_UINT32, &iRet,
                    DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                    &pData, pBuf->size(),
                    DBUS_TYPE_INVALID ) )
                {
                    ret = -ENOMEM;
                    break;
                }
            }
            else
            {
                if( !dbus_message_append_args( pRespMsg,
                    DBUS_TYPE_UINT32, &iRet,
                    DBUS_TYPE_INVALID ) )
                {
                    ret = -ENOMEM;
                    break;
                }
            }
        }
        else
        {
            TaskletPtr pTask;

            // NOTE: no need to check if the method is
            // valid or not. It has be done already
            BufPtr pBuf( true );
            ret = pRespData->Serialize( *pBuf );  
            if( ERROR( ret ) )
            {
                ret = ERROR_FAIL;
                if( !dbus_message_append_args( pRespMsg,
                    DBUS_TYPE_UINT32, &ret,
                    DBUS_TYPE_INVALID ) )
                {
                    ret = -ENOMEM;
                    break;
                }
            }
            else
            {
                const char* pData = pBuf->ptr();
                if( !dbus_message_append_args( pRespMsg,
                    DBUS_TYPE_UINT32, &iRet,
                    DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                    &pData, pBuf->size(),
                    DBUS_TYPE_INVALID ) )
                {
                    ret = -ENOMEM;
                    break;
                }
            }
        }

        CIoManager* pMgr = GetIoMgr();
        IrpPtr pIrp;

        ret = pIrp.NewObj();

        if( ERROR( ret ) )
            break;

        ret = pIrp->AllocNextStack( nullptr );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 

        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
        pIrpCtx->SetCtrlCode( CTRLCODE_SEND_RESP );
        pIrpCtx->SetIoDirection( IRP_DIR_OUT ); 

        BufPtr pBuf( true );
        *pBuf = pRespMsg;
        pIrpCtx->SetReqData( pBuf );

        TaskletPtr pTask;
        ret = pTask.NewObj( clsid( CIfDummyTask ) );

        if( SUCCEEDED( ret ) )
        {
            EventPtr pEvent;
            pEvent = pTask;
            // NOTE: we have fed a dummy task here
            pIrp->SetCallback( pEvent, 0 );
        }

        pIrp->SetTimer(
            IFSTATE_ENABLE_EVENT_TIMEOUT, GetIoMgr() );

        ret = pMgr->SubmitIrp(
            GetPortHandle(), pIrp );

        if( ret == STATUS_PENDING )
            break;

        if( ret == -EAGAIN )
            ret = STATUS_MORE_PROCESS_NEEDED;

        pIrp->RemoveTimer();
        pIrp->RemoveCallback();

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::SendResponse(
    IEventSink* pInvTask,
    IConfigDb* pReqMsg,
    CfgPtr& pRespData )
{
    // called to send back the response
    if( pReqMsg == nullptr ||
        pRespData.IsEmpty() )
        return -EINVAL;

    return -ENOTSUP;
}

// send a dbus signal, called from the user
// code
gint32 CInterfaceServer::BroadcastEvent(
    IConfigDb* pReqCall,
    IEventSink* pCallback )
{
    if( pReqCall == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{

        // NOTE: if the connection is gone, we can
        // only stop the keep-alive heart beat
        // here, but the invoke request is still
        // in process.
        string strVal;
        CReqOpener oReq( pReqCall );
        ret = oReq.GetDestination( strVal );
        if( SUCCEEDED( ret ) )
        {
            SvrConnPtr pConnMgr = GetConnMgr();
            if( !pConnMgr.IsEmpty() )
            {
                ret = pConnMgr->CanResponse( strVal );
                if( ret == ERROR_FALSE )
                {
                    ret = -ENOTCONN;
                    break;
                }
            }
        }

        CParamList oRespCfg;
        CfgPtr pRespCfg = oRespCfg.GetCfg();
        if( pRespCfg.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        ret = RunIoTask( pReqCall,
            pRespCfg, pCallback );

        if( ret == STATUS_PENDING )
            break;

        if( SUCCEEDED( ret ) )
        {
            if( oReq.HasReply() )
            {
                CCfgOpener oCfg(
                    ( IConfigDb* )pRespCfg );

                gint32 iRet = 0;
                ret = oCfg.GetIntProp(
                    propReturnValue,
                    ( guint32& )iRet );

                if( ERROR( ret ) )
                    break;

                ret = iRet;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::OnKeepAlive(
    IEventSink* pTask, EnumKAPhase iPhase )
{
    if( iPhase == KAOrigin )
    {
        return OnKeepAliveOrig( pTask );
    }

    return -EINVAL;
}

/**
* @name OnKeepAliveOrig
* a event handler when the keep-alive timer is out
* @{ */
/** 
 * this is where the keepalive event originates.
 * @} */
gint32 CInterfaceServer::OnKeepAliveOrig(
    IEventSink* pTask )
{
    if( pTask == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CCfgOpenerObj oCfg( pTask );
        DMsgPtr pMsg;
        ret = oCfg.GetMsgPtr( propMsgPtr, pMsg );
        if( ERROR( ret ) )
            break;

        if( pMsg.GetType() !=
            DBUS_MESSAGE_TYPE_METHOD_CALL )
        {
            ret = -ENOTSUP;
            break;
        }
        string strSender = pMsg.GetSender();
        if( !IsConnected( strSender.c_str() ) )
        {
            // NOTE: this is not a serious state
            // check
            ret = ERROR_STATE;
            break;
        }

        // test if the interface is paused
        string strIfName = pMsg.GetInterface();
        if( IsPaused( strIfName ) )
        {
            ret = ERROR_PAUSED;
            break;
        }

        ObjPtr pObj;
        ret = pMsg.GetObjArgAt( 0, pObj );
        if( ERROR( ret ) )
            break;

        IConfigDb* pCfg = pObj;
        if( pCfg == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CReqOpener oReq( pCfg );
        if( !oReq.IsKeepAlive() )
            break;

        guint64 iTaskId = 0;
        ret = oReq.GetTaskId( iTaskId );
        if( ERROR( ret ) )
            break;

        CReqBuilder okaReq( this );

        okaReq.SetIfName( strIfName );
        okaReq.SetMethodName(
            SYS_EVENT_KEEPALIVE );

        okaReq.SetCallFlags( 
           DBUS_MESSAGE_TYPE_SIGNAL
           | CF_ASYNC_CALL );

        string strVal = pMsg.GetDestination( );
        if( strVal.empty() )
        {
            ret = -EINVAL;
            break;
        }

        okaReq.SetSender( strVal );
        strVal = pMsg.GetSender( );
        okaReq.SetDestination( strVal );

        guint32 dwSerial = 0;
        pMsg.GetSerial( dwSerial );
        if( dwSerial != 0 )
        {
            // the serial number is for the router to
            // find the task for the request to KA
            okaReq.SetIntProp(
                propSeqNo, dwSerial );
        }

        okaReq.Push( iTaskId );
        okaReq.Push( ( guint32 )KATerm );

        TaskletPtr pDummyTask;

        ret = pDummyTask.NewObj(
            clsid( CIfDummyTask ) );

        if( ERROR( ret ) )
            break;

        ret = BroadcastEvent(
            okaReq.GetCfg(), pDummyTask );

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::InitUserFuncs()
{
    gint32 ret = super::InitUserFuncs();
    if( ERROR( ret ) )
        return ret;

    BEGIN_IFHANDLER_MAP( IInterfaceServer );

    ADD_SERVICE_HANDLER(
        CInterfaceServer::UserCancelRequest,
        SYS_METHOD_USERCANCELREQ );

    ADD_SERVICE_HANDLER(
        CInterfaceServer::Pause_Server,
        SYS_METHOD_PAUSE );

    ADD_SERVICE_HANDLER(
        CInterfaceServer::Resume_Server,
        SYS_METHOD_RESUME );

    ADD_SERVICE_HANDLER(
        CInterfaceServer::KeepAliveRequest,
        SYS_METHOD_KEEPALIVEREQ );

    ADD_SERVICE_HANDLER(
        CInterfaceServer::ForceCancelRequests,
        SYS_METHOD_FORCECANCELREQS );

    END_IFHANDLER_MAP;

    return ret;
}

gint32 CInterfaceServer::UserCancelRequest(
    IEventSink* pCallback,
    guint64 qwIoTaskId )
{
    // let's search through the tasks to find the
    // specified task to cancel
    gint32 ret = 0;
    do{
        TaskGrpPtr pTaskGrp = GetTaskGroup();
        if( pTaskGrp.IsEmpty() )
            break;

        TaskletPtr pTask;
        ret = pTaskGrp->FindTaskByRmtId(
            qwIoTaskId, pTask );

        if( ERROR( ret ) )
            break;

        CfgPtr pResp;
        CIfInvokeMethodTask* pInvTask = pTask;
        gint32 iRet = ERROR_USER_CANCEL;

        if( pInvTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        else
        {
            CStdRTMutex oTaskLock(
                pInvTask->GetLock() );

            if( pInvTask->GetTaskState() ==
                stateStopped )
                return ERROR_STATE;
           
            CCfgOpenerObj oCfg( pInvTask ); 
            
            gint32 iType = 0;
            ret = pInvTask->GetPropertyType(
                propMsgPtr, iType );

            if( ERROR( ret ) )
                break;

            if( iType == typeObj )
            {
                ObjPtr pMsg;
                ret = oCfg.GetObjPtr( propMsgPtr, pMsg );
                if( ERROR( ret ) )
                    break;

                IConfigDb* pReqCfg = pMsg;
                if( pReqCfg == nullptr )
                {
                    ret = -EFAULT;
                    break;
                }

                CReqBuilder oResp( pReqCfg );

                oResp.ClearParams();
                ret = oResp.SwapProp(
                    propDestDBusName, propSrcDBusName );

                oResp.SetCallFlags( CF_ASYNC_CALL |
                    DBUS_MESSAGE_TYPE_METHOD_RETURN |
                    CF_NON_DBUS );

                oResp.SetReturnValue( iRet );
                pResp = oResp.GetCfg();
            }
            else
            {
                // typeDMsg
                CReqBuilder oResp( this );
                DMsgPtr pMsg;
                ret = oCfg.GetMsgPtr( propMsgPtr, pMsg );
                if( ERROR( ret ) )
                    break;

                string strVal; 
                oResp.SetMethodName(
                    pMsg.GetMember() );

                oResp.SetDestination(
                     pMsg.GetSender() );

                oResp.SetCallFlags( CF_ASYNC_CALL |
                    DBUS_MESSAGE_TYPE_METHOD_RETURN );

                oResp.SetReturnValue( iRet );
                pResp = oResp.GetCfg();
            }

            if( ERROR( ret ) )
                break;
        }

        if( !pResp.IsEmpty() )
        {
            // note we do not use OnServiceComplete to
            // complete the invoke task, force to
            // deliver the `cancel event' to the task.
            SetResponse( pInvTask, pResp );
            if( pInvTask->IsInProcess() )
            {
                // the task is running, and we
                // cannot cancel it
                ret = ERROR_FAIL;
                break;
            }
            pInvTask->OnEvent(
                eventUserCancel, iRet, 0, 0 );

            DebugPrint( 0,
                "Inv Task Canceled, 0x%llx",
                qwIoTaskId );
        }

    }while( 0 );

    // set the response for this cancel request
    CParamList oMyResp;
    oMyResp[ propReturnValue ] = ret;

    SetResponse( pCallback,
        oMyResp.GetCfg() );

    return ret;
}

gint32 CInterfaceServer::ForceCancelRequests(
    IEventSink* pCallback,
    ObjPtr& pTaskIds )
{
    // let's search through the tasks to find the
    // specified task to cancel
    gint32 ret = 0;
    do{
        if( pTaskIds.IsEmpty() )
        {
            ret = -ENOENT;
            break;
        }

        QwVecPtr pvecTasks = pTaskIds;
        if( pvecTasks.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        std::vector< guint64 >& vecTaskIds =
            ( *pvecTasks )();

        TaskGrpPtr pTaskGrp;
        GetParallelGrp( pTaskGrp );
        if( pTaskGrp.IsEmpty() )
            break;

        for( auto elem : vecTaskIds )
        {
            TaskletPtr pTask;

            ret = pTaskGrp->FindTaskByRmtId(
                elem, pTask );
            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }

            CIfInvokeMethodTask* pInvTask = pTask;
            gint32 iRet = ERROR_CANCEL;

            pInvTask->OnEvent(
                eventCancelTask, iRet, 0, 0 );

            DebugPrint( 0,
                "Inv Task Canceled silently, 0x%llx",
                elem );
        }

    }while( 0 );

    // set the response for this cancel request
    CParamList oMyResp;
    oMyResp[ propReturnValue ] = ret;
    SetResponse( pCallback, oMyResp.GetCfg() );

    return ret;
}

gint32 CInterfaceServer::KeepAliveRequest(
    IEventSink* pCallback,
    guint64 qwTaskId )
{
    // let's search through the tasks to find the
    // specified task to cancel
    gint32 ret = 0;
    do{
        TaskGrpPtr pTaskGrp = GetTaskGroup();
        if( pTaskGrp.IsEmpty() )
            break;

        TaskletPtr pTask;
        ret = pTaskGrp->FindTaskByRmtId(
            qwTaskId, pTask );
        if( ERROR( ret ) )
            break;

        CfgPtr pResp;
        CIfInvokeMethodTask* pInvTask = pTask;
        gint32 iRet = ERROR_USER_CANCEL;

        if( pInvTask == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CStdRTMutex oTaskLock(
            pInvTask->GetLock() );

        if( pInvTask->GetTaskState() ==
            stateStopped )
        {
            ret = ERROR_STATE;
            break;
        }
       
        pInvTask->ResetTimer();
        DebugPrint( 0,
            "timer is reset of Task %lld",
            qwTaskId );

    }while( 0 );

    return ret;
}

gint32 CInterfaceServer::SendData_Server(
    IConfigDb* pDataDesc,           // [in]
    gint32 fd,                      // [in]
    guint32 dwOffset,               // [in]
    guint32 dwSize,                 // [in]
    IEventSink* pCallback )
{
    return -ENOTSUP;
}

gint32 CInterfaceServer::FetchData_Server(
    IConfigDb* pDataDesc,           // [in]
    gint32& fd,                     // [out]
    guint32& dwOffset,              // [in, out]
    guint32& dwSize,                // [in, out]
    IEventSink* pCallback )
{
    return -ENOTSUP;
}

gint32 CInterfaceServer::OnModEvent(
    EnumEventId iEvent,
    const std::string& strModule )
{
    if( iEvent == eventModOffline )
    {
        SvrConnPtr pConnMgr = GetConnMgr();
        if( !pConnMgr.IsEmpty() )
            pConnMgr->OnDisconn( strModule );
    }

    return super::OnModEvent(
        iEvent, strModule );
}

gint32 CInterfaceServer::OnPostStop(
    IEventSink* pCallback )
{
    gint32 ret = super::OnPostStop( pCallback );
    m_pConnMgr.Clear();
    return ret;
}

gint32 CInterfaceServer::OnPreStart(
    IEventSink* pCallback )
{
    CParamList oParams;
    oParams[ propIfPtr ] = ObjPtr( this );
    gint32 ret = m_pConnMgr.NewObj(
        clsid( CIfSvrConnMgr ),
        oParams.GetCfg() );

    if( ERROR( ret ) )
        return ret;

    return super::OnPreStart( pCallback );
}

bool CInterfaceServer::IsConnected(
    const char* szDestAddr )
{
    EnumIfState iState = GetState();
    if( iState != stateConnected &&
        iState != statePaused &&
        iState != statePausing &&
        iState != stateResuming )
        return false;

    if( szDestAddr != nullptr &&
        !m_pConnMgr.IsEmpty() )
    {
        gint32 ret = m_pConnMgr->
            CanResponse( szDestAddr );
        if( ret == ERROR_FALSE )
            return false;
    }

    return true;
}

bool CInterfaceServer::IsPaused(
    const std::string& strIfName )
{
    EnumIfState iState = GetState();
    if( iState != statePaused )
        return false;

    EnumClsid iid = clsid( Invalid );
    gint32 ret = GetIidFromIfName(
        strIfName, iid );

    // pause all the unknown interface
    if( ERROR( ret ) )
        return true;
    
    for( auto pMatch : m_vecMatches )
    {
        CMessageMatch* pMsgMatch = pMatch;
        CfgPtr& pCfg = pMsgMatch->GetCfg();   
        CCfgOpener oMatch(
            ( IConfigDb* )pCfg );

        EnumClsid iidMatch = clsid( Invalid );
        guint32* piid = ( guint32* )&iidMatch;
        ret = oMatch.GetIntProp( propIid, *piid );
        if( ERROR( ret ) )
            continue;

        if( iidMatch == iid )
        {
            bool bPausable = false;
            ret = oMatch.GetBoolProp(
                propPausable, bPausable );
            if( ERROR( ret ) )
                break;
            if( bPausable )
                return true;
            break;
        }
    }
    return false;
}

bool CInterfaceServer::IsPaused(
    IMessageMatch* pMatch )
{
    if( pMatch == nullptr )
        return true;

    EnumIfState iState = GetState();
    if( iState != statePaused )
        return false;

    CMessageMatch* pMsgMatch = static_cast
        < CMessageMatch* >( pMatch );

    CfgPtr& pCfg = pMsgMatch->GetCfg();   
    CCfgOpener oMatch(
        ( IConfigDb* )pCfg );

    bool bPausable = false;
    gint32 ret = oMatch.GetBoolProp(
        propPausable, bPausable );

    if( ERROR( ret ) )
        return false;

    if( bPausable )
        return true;

    return false;
}

gint32 CInterfaceServer::StartRecvMsg(
    IEventSink* pCompletion,
    IMessageMatch* pMatch )
{
    // CMessageMatch* pMsgMatch = static_cast
    //     < CMessageMatch* >( pMatch );
    // string strIfName;
    // pMsgMatch->GetIfName( strIfName );
    if( !IsConnected() )
        return ERROR_STATE;

    return super::StartRecvMsg(
        pCompletion, pMatch );
}

gint32 CInterfaceServer::DoPause(
    IEventSink* pCallback )
{
    gint32 ret = super::DoPause( pCallback );

    if( SUCCEEDED( ret ) )
        ClearPausedTasks();

    return ret;
}

gint32 CInterfaceServer::DoResume(
    IEventSink* pCallback )
{
    gint32 ret = super::DoResume( pCallback );

    if( SUCCEEDED( ret ) )
        RestartListening( statePaused );

    return ret;
}

gint32 CInterfaceServer::Pause_Server(
    IEventSink* pCallback )
{
    return PauseResume_Server( pCallback, true );
}

gint32 CInterfaceServer::Resume_Server(
    IEventSink* pCallback )
{
    return PauseResume_Server( pCallback, false );
}

gint32 CInterfaceServer::PauseResume_Server(
    IEventSink* pCallback,
    bool bPause )
{
    if( pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        if( bPause )
            ret = super::Pause( pCallback );
        else
            ret = super::Resume( pCallback );

        if( ret == STATUS_PENDING )
            break;

        ret = FillAndSetResponse(
            pCallback, ret );         

    }while( 0 );

    return ret;
}

}
