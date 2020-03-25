/*
 * =====================================================================================
 *
 *       Filename:  streammh.cpp
 *
 *    Description:  implementation stream support for multi-hop 
 *
 *        Version:  1.0
 *        Created:  03/11/2020 08:57:29 PM
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

#include "rpcroute.h"
#include "streammh.h"

CRpcTcpBridgeProxyStream::CRpcTcpBridgeProxyStream(
    const IConfigDb* pCfg ) : super( pCfg )
{
    SetClassId( clsid(
        CRpcTcpBridgeProxyStream ) );

    gint32 ret = 0;
    do{
        m_oFlowCtrlRmt.SetAsMonitor( true );
        CCfgOpenerObj oCfg( this  );
        ret = oCfg.GetIntProp( propPeerStmId,
            ( guint32& )m_iBdgePrxyStmId );
        if( ERROR( ret ) )
            break;

        CRpcServices* pBridgeProxy = nullptr;
        ret = oCfg.GetPointer(
            propIfPtr, pBridgeProxy );
        if( ERROR( ret ) )
            break;

        m_pBridgeProxy = pBridgeProxy;
        oCfg.RemoveProperty( propIfPtr );
        oCfg.RemoveProperty( propPeerStmId );

    }while( 0 );

    if( ERROR( ret ) )
    {
        std::string strMsg = DebugMsg( ret,
            "Error occurs in "
            "CRpcTcpBridgeProxyStream ctor" );
        throw std::runtime_error( strMsg );
    }

    return;
}

gint32 CRpcTcpBridgeProxyStream::StartTicking(
    IEventSink* pContext )
{
    gint32 ret = 0;

    do{
        // start the event listening
        CParamList oParams;

        oParams.SetPointer( propIfPtr, this );
        oParams.SetPointer( propIoMgr,
            this->GetIoMgr() );

        oParams.CopyProp( propTimeoutSec, this );
        oParams.SetIntProp(
            propStreamId, m_iBdgePrxyStmId );

        ret = this->m_pListeningTask.NewObj(
            clsid( CIfUxListeningRelayTaskMH ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = this->RunManagedTask(
            this->m_pListeningTask );

        if( ERROR( ret ) )
            break;

        // start the ping ticker
        ret = this->m_pPingTicker.NewObj(
            clsid( CIfUxPingTicker ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        // RunManagedTask makes the
        // m_pPingTicker as a place holder in
        // the parallel task group, in order
        // which can avoid continuously
        // destroyed when the group is
        // temporarily empty, and waiting for
        // new tasks.
        ret = this->RunManagedTask(
            this->m_pPingTicker );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxyStream::OnPostStart(
    IEventSink* pContext )
{
    gint32 ret = 0;

    do{
        CCfgOpenerObj oIfCfg( this );
        ret = oIfCfg.GetIntProp( propStreamId,
            ( guint32& )m_iBdgeStmId );
        if( ERROR( ret ) )
            break;

        ret = StartTicking( pContext );
        if( ERROR( ret ) )
            break;

        // start the data reading
        CParamList oParams;
        oParams.SetPointer( propIfPtr, this );
        oParams.SetPointer(
            propIoMgr, this->GetIoMgr() );

        oParams.CopyProp( propTimeoutSec, this );
        oParams.CopyProp(
            propKeepAliveSec, this );

        oParams.SetIntProp(
            propStreamId, m_iBdgePrxyStmId );

        // ux stream writer
        oParams.Push( false );
        ret = m_pWritingTask.NewObj(
            clsid( CIfUxSockTransRelayTaskMH ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        ret = this->GetIoMgr()->
            RescheduleTask( this->m_pWritingTask );

        if( ERROR( ret ) )
            break;

        oParams.SetIntProp(
            propStreamId, m_iBdgeStmId );

        // tcp stream writer
        ret = m_pWrTcpStmTask.NewObj(
            clsid( CIfTcpStmTransTaskMH ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        ret = this->GetIoMgr()->
            RescheduleTask( m_pWrTcpStmTask );

        if( ERROR( ret ) )
            break;

        // tcp stream reader
        oParams.SetBoolProp( 0, true );
        ret = m_pRdTcpStmTask.NewObj(
            clsid( CIfTcpStmTransTaskMH ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        ret = this->GetIoMgr()->
            RescheduleTask( m_pRdTcpStmTask );

    }while( 0 );

    if( ERROR( ret ) )
    {
        // cancel all the tasks
        OnPreStop( pContext );
    }

    return ret;
}

gint32 CRpcTcpBridgeProxyStream::OnFlowControl()
{
    gint32 ret =
        m_oFlowCtrl.IncFCCount();

    if( ret == fcsKeep )
        return 0;

    CStdRMutex oIfLock( GetLock() );
    ObjPtr pObj = m_pWritingTask;
    if( pObj.IsEmpty() )
        return -EFAULT;

    CIfUxTaskBase* pTask =
        m_pWritingTask;

    if( pTask == nullptr )
        return -EFAULT;

    oIfLock.Unlock();
    return pTask->Pause();
}

gint32 CRpcTcpBridgeProxyStream::OnFCLifted()
{
    gint32 ret = m_oFlowCtrl.DecFCCount();
    if( ret == fcsKeep )
        return 0;

    CStdRMutex oIfLock( this->GetLock() );
    ObjPtr pObj = m_pWritingTask;
    if( pObj.IsEmpty() )
        return -EFAULT;

    CIfUxTaskBase* pTask =
        m_pWritingTask;

    if( pTask == nullptr )
        return -EFAULT;

    oIfLock.Unlock();
    return pTask->Resume();
}

gint32 CRpcTcpBridgeProxyStream::GetReqToLocal(
    guint8& byToken, BufPtr& pBuf )
{
    gint32 ret = 0;

    do{
        CIfUxTaskBase* pReadTask =
            m_pRdTcpStmTask;

        CStdRTMutex oReadLock(
            pReadTask->GetLock() );

        CStdRMutex oIfLock( this->GetLock() );
        SENDQUE* pQueue = &m_queToLocal;

        if( pQueue->empty() )
        {
            ret = -ENOENT;
            break;
        }

        if( !this->CanSend() )
        {

            byToken = pQueue->front().first;
            pBuf = pQueue->front().second;

            if( byToken != tokFlowCtrl &&
                byToken != tokLift &&
                byToken != tokClose )
            {
                ret = -ENOENT;
                break;
            }
        }

        bool bFull =
            pQueue->size() >= QueueLimit();

        byToken = pQueue->front().first;
        pBuf = pQueue->front().second;
        pQueue->pop_front();

        if( pQueue->size() >= QueueLimit() )
            break;

        if( bFull )
        {
            // reading from tcp stream can resume
            BufPtr pEmptyBuf( true );
            ForwardToRemoteUrgent( tokLift, pEmptyBuf );
            oIfLock.Unlock();
        }

        if( pReadTask->IsPaused() )
            PauseResumeTask( pReadTask, true );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxyStream::GetReqToRemote(
    guint8& byToken, BufPtr& pBuf )
{
    gint32 ret = 0;

    do{
        CIfUxTaskBase* pReadTask =
            this->m_pListeningTask;

        CStdRTMutex oTaskLock(
            pReadTask->GetLock() );

        CStdRMutex oIfLock( this->GetLock() );
        SENDQUE* pQueue = &m_queToRemote;
        if( pQueue->empty() )
        {
            ret = -ENOENT;
            break;
        }

        byToken = pQueue->front().first;
        pBuf = pQueue->front().second;

        if( !m_oFlowCtrlRmt.CanSend() )
        {
            if( byToken != tokFlowCtrl &&
                byToken != tokLift &&
                byToken != tokClose )
            {
                ret = -ENOENT;
                break;
            }
        }

        bool bFull =
            pQueue->size() >= QueueLimit();

        pQueue->pop_front();

        if( pQueue->size() >= QueueLimit() )
            break;

        if( bFull )
        {
            BufPtr pEmptyBuf( true );
            ForwardToLocalUrgent( tokLift, pEmptyBuf );
            oIfLock.Unlock();
        }

        // the listening task can resume
        if( pReadTask->IsPaused() )
            PauseResumeTask( pReadTask, true );

    }while( 0 );

    return ret;
}

gint32 CRpcTcpBridgeProxyStream::OnFlowControlRemote()
{
    // this event comes over tcp stream
    CStdRMutex oIfLock( this->GetLock() );
    gint32 ret =
        m_oFlowCtrlRmt.IncFCCount();

    if( ret == fcsFlowCtrl )
    {
        BufPtr pBuf( true );
        SendBdgeStmEvent(
            tokFlowCtrl, pBuf );
    }
    return 0;
}

gint32 CRpcTcpBridgeProxyStream::OnFCLiftedRemote()
{
    // this event comes over tcp stream
    CStdRMutex oIfLock( this->GetLock() );
    gint32 ret =
        m_oFlowCtrlRmt.DecFCCount();

    if( ret != fcsLift )
        return 0;

    ObjVecPtr pvecTasks( true );

    if( !m_queToRemote.empty() )
    {
        ( *pvecTasks )().push_back(
            ObjPtr( m_pWrTcpStmTask ) );
    }

    oIfLock.Unlock();
    ObjPtr pTasks = pvecTasks;

    // wake up ux stream reader task
    if( ( *pvecTasks )().empty() )
        return 0;

    return PauseResumeTasks( pTasks, true );
}

gint32 CRpcTcpBridgeProxyStream::ForwardToRemote(
    guint8 byToken, BufPtr& pBuf )
{
    return ForwardToQueue( byToken, pBuf,
        m_queToRemote, m_queToLocal,
        m_pWrTcpStmTask, m_pWritingTask );
}

gint32 CRpcTcpBridgeProxyStream::OnPostStop(
    IEventSink* pCallback ) 
{
    super::OnPostStop( pCallback );
    m_pBridgeProxy.Clear();
    return 0;
}

gint32 CRpcTcpBridgeProxyStream::OnDataReceived(
    CBuffer* pBuf )
{
    this->m_oFlowCtrl.IncRxBytes(
        pBuf->size() - UXPKT_HEADER_SIZE );
    return 0;
}

gint32 CRpcTcpBridgeProxyStream::OnDataReceivedRemote(
    CBuffer* pBuf )
{
    this->m_oFlowCtrlRmt.IncRxBytes(
        pBuf->size() - UXPKT_HEADER_SIZE );
    return 0;
}

gint32 CIfUxRelayTaskHelperMH::GetBridgeIf(
    InterfPtr& pIf, bool bProxy )
{
    if( m_pSvc == nullptr )
        return -EFAULT;

    CRpcTcpBridgeProxyStream* pStream = 
    static_cast< CRpcTcpBridgeProxyStream* >
        ( m_pSvc );

    if( bProxy )
    {
        pIf = pStream->GetBridgeProxy();
    }
    else
    {
        pIf = pStream->GetBridge();
    }

    if( pIf.IsEmpty() )
        return -EFAULT;

    return 0;
}

gint32 CIfUxRelayTaskHelperMH::WriteTcpStream(
    gint32 iStreamId,
    CBuffer* pSrcBuf,
    guint32 dwSizeToWrite,
    IEventSink* pCallback )
{
    if( pCallback == nullptr )
        return -EINVAL;

    bool bProxy = true;
    if( pCallback->GetClsid() ==
        clsid( CIfTcpStmTransTaskMH ) )
        bProxy = false;

    InterfPtr pIf; 
    gint32 ret = GetBridgeIf( pIf, bProxy );

    if( ERROR( ret ) )
        return ret;

    if( pIf.IsEmpty() )
        return -EFAULT;

    if( pIf->IsServer() )
    {
        CRpcTcpBridge*
            pBdge = ObjPtr( pIf );

        ret = pBdge->WriteStream(
            iStreamId, pSrcBuf,
            dwSizeToWrite, pCallback );
    }
    else
    {
        CRpcTcpBridgeProxy*
            pBdge = ObjPtr( pIf );

        ret = pBdge->WriteStream(
            iStreamId, pSrcBuf,
            dwSizeToWrite, pCallback );
    }
    return ret;
}

gint32 CIfUxRelayTaskHelperMH::ReadTcpStream(
    gint32 iStreamId,
    BufPtr& pSrcBuf,
    guint32 dwSizeToWrite,
    IEventSink* pCallback )
{
    bool bProxy = true;
    if( pCallback->GetClsid() ==
        clsid( CIfTcpStmTransTaskMH ) )
        bProxy = false;

    InterfPtr pIf; 
    gint32 ret = GetBridgeIf( pIf, bProxy );
    if( ERROR( ret ) )
        return ret;

    if( pIf.IsEmpty() )
        return -EFAULT;

    if( pIf->IsServer() )
    {
        CRpcTcpBridge*
            pBdge = ObjPtr( pIf );

        ret = pBdge->ReadStream(
            iStreamId, pSrcBuf,
            dwSizeToWrite, pCallback );
    }
    else
    {
        CRpcTcpBridgeProxy*
            pBdge = ObjPtr( pIf );

        ret = pBdge->ReadStream(
            iStreamId, pSrcBuf,
            dwSizeToWrite, pCallback );
    }

    return ret;
}

CIfUxSockTransRelayTaskMH::CIfUxSockTransRelayTaskMH(
    const IConfigDb* pCfg ) : super( pCfg )
{
    SetClassId( clsid(
        CIfUxSockTransRelayTaskMH ) );
}

CRpcTcpBridgeProxyStream*
CIfUxSockTransRelayTaskMH::GetOwnerIf()
{
    gint32 ret = 0;
    CCfgOpener oTaskCfg(
        ( IConfigDb* )GetConfig() );

    CRpcTcpBridgeProxyStream *pStm = nullptr;
    ret = oTaskCfg.GetPointer(
        propIfPtr, pStm );
    if( ERROR( ret ) )
        return nullptr;

    return pStm;
}

gint32 CIfUxSockTransRelayTaskMH::RunTask()
{
    gint32 ret = 0;
    do{
        CIfUxRelayTaskHelperMH oHelper( this );
        if( IsReading() )
        {
            // don't support reading
            ret = OnTaskComplete( -EINVAL );
        }
        else
        {
            CCfgOpener oCfg(
                ( IConfigDb* )GetConfig() );

            gint32 iStmId = -1;
            ret = oCfg.GetIntProp( propStreamId,
                ( guint32& )iStmId );

            if( ERROR( ret ) )
                break;

            guint8 byToken = 0;
            BufPtr pPayload;
            ret = oHelper.PeekReqToLocal(
                byToken, pPayload );

            if( ret == -ENOENT )
            {
                ret = STATUS_PENDING;
                Pause();
                break;
            }

            ret = oHelper.WriteTcpStream(
                iStmId, pPayload,
                pPayload->size(), this );

            // note that this flow control comes from
            // uxport rather than the uxstream.
            if( ERROR_QUEUE_FULL == ret )
                break;

            CRpcTcpBridgeProxyStream* pIf =
                GetOwnerIf();
            if( pIf != nullptr )
            {
                pIf->IncTxBytes(
                    pPayload->size() );
            }

            // remove the packet from the queue
            BufPtr pBuf;
            guint8 byToken2 = 0;
            gint32 iRet = oHelper.GetReqToLocal(
                byToken2, pBuf );

            if( ERROR( iRet ) && SUCCEEDED( ret ) )
                ret = iRet;

            else if( byToken != byToken2 )
                ret = -EBADMSG;

            else if( byToken == tokData ||
                byToken == tokProgress )
            {
                if( !( pBuf == pPayload ) )
                    ret = -EBADMSG;
            }
            if( ERROR( ret ) )
                break;
        }

        if( ERROR( ret ) )
            break;

        if( ret == STATUS_PENDING )
            break;

    }while( 1 );

    return ret;
}

CIfTcpStmTransTaskMH::CIfTcpStmTransTaskMH(
    const IConfigDb* pCfg ) : super( pCfg )
{
    SetClassId( clsid(
        CIfTcpStmTransTaskMH ) );
}

CRpcTcpBridgeProxyStream*
CIfTcpStmTransTaskMH::GetOwnerIf()
{
    gint32 ret = 0;
    CCfgOpener oTaskCfg(
        ( IConfigDb* )GetConfig() );

    CRpcTcpBridgeProxyStream *pStm = nullptr;
    ret = oTaskCfg.GetPointer(
        propIfPtr, pStm );
    if( ERROR( ret ) )
        return nullptr;

    return pStm;
}

gint32 CIfTcpStmTransTaskMH::RunTask()
{
    gint32 ret = 0;
    do{
        CIfUxRelayTaskHelperMH oHelper( this );

        CCfgOpenerObj oCfg( this );

        gint32 iStmId = -1;
        ret = oCfg.GetIntProp( propStreamId,
            ( guint32& )iStmId );

        if( ERROR( ret ) )
            break;

        BufPtr pNewBuf;

        if( IsReading() )
        {
            BufPtr pBuf;
            ret = oHelper.ReadTcpStream(
                iStmId, pBuf, 20, this );

            if( ret == STATUS_PENDING )
                break;

            if( ERROR( ret ) )
                break;

            ret = PostEvent( pBuf );
        }
        else
        {
            guint8 byToken = 0;
            BufPtr pPayload;
            ret = oHelper.GetReqToRemote(
                byToken, pPayload );

            if( ret == -ENOENT )
            {
                ret = STATUS_PENDING;
                Pause();
                break;
            }
            
            ret = oHelper.WriteTcpStream(
                iStmId, pPayload,
                pPayload->size(), this );

            if( !ERROR( ret ) )
            {
                CRpcTcpBridgeProxyStream* pIf =
                    GetOwnerIf();
                if( pIf != nullptr )
                {
                    pIf->IncTxBytesRemote(
                        pPayload->size() );
                }
            }
        }

        if( ERROR( ret ) )
            break;

        if( ret == STATUS_PENDING )
            break;

    }while( 1 );

    return ret;
}

gint32 CIfTcpStmTransTaskMH::PostEvent(
    BufPtr& pBuf )
{
    if( pBuf.IsEmpty() ||
        pBuf->empty() )
        return -EINVAL;

    gint32 ret = 0;
    BufPtr pNewBuf( true );
    guint8 byToken = pBuf->ptr()[ 0 ];

    CIfUxRelayTaskHelperMH oHelper( this );
    switch( byToken )
    {
    case tokPing:
    case tokPong:
    case tokData:
    case tokProgress:
        {
            ret = oHelper.ForwardToLocal(
                byToken, pBuf );
            break;
        }
    case tokClose:
    case tokFlowCtrl:
    case tokLift:
        {
            break;
        }
    default:
        {
            ret = -EINVAL;
            break;
        }
    }

    if( ret == ERROR_QUEUE_FULL )
    {
        ret = STATUS_PENDING;
        Pause();
    }

    if( ERROR( ret ) )
        return ret;

    oHelper.PostTcpStmEvent(
        byToken, pNewBuf );

    return ret;
}

CIfUxListeningRelayTaskMH::CIfUxListeningRelayTaskMH(
    const IConfigDb* pCfg ) : super( pCfg )
{
    SetClassId( clsid(
        CIfUxListeningRelayTaskMH ) );
}

gint32 CIfUxListeningRelayTaskMH::RunTask()
{
    gint32 ret = 0;
    do{
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        gint32 iStmId = 0;
        ret = oCfg.GetIntProp(
            propStreamId, ( guint32& )iStmId );
        if( ERROR( ret ) )
            break;

        CIfUxRelayTaskHelperMH oHelper( this );

        BufPtr pSrcBuf( true );
        ret = oHelper.ReadTcpStream(
            iStmId, pSrcBuf, 20, this );
        if( ret == STATUS_PENDING )
            break;

        CParamList oParams;
        oParams[ propReturnValue ] = ret;
        if( SUCCEEDED( ret ) )
            oParams.Push( pSrcBuf );

        oCfg.SetPointer( propRespPtr,
            ( IConfigDb* )oParams.GetCfg() );

        ret = OnTaskComplete( ret );
        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) )
            break;

    }while( 1 );

    return ret;
}

gint32 CIfUxListeningRelayTaskMH::PostEvent(
    BufPtr& pBuf )
{
    if( pBuf.IsEmpty() ||
        pBuf->empty() )
        return -EINVAL;

    gint32 ret = 0;
    CIfUxRelayTaskHelperMH oHelper( this );
    guint8 byToken = pBuf->ptr()[ 0 ];
    BufPtr pNewBuf = pBuf;
    switch( byToken )
    {
    case tokPing:
    case tokPong:
    case tokData:
    case tokProgress:
        {
            ret = oHelper.ForwardToRemote(
                byToken, pBuf );
        }
    case tokError:
        {
            gint32 iError;
            memcpy( &iError, pBuf->ptr() + 1,
                sizeof( gint32 ) );

            iError = ntohl( iError );

            ret = pNewBuf.NewObj();
            if( ERROR( ret ) )
                break;

            *pNewBuf = ( guint32& )iError;
            break;
        }
    case tokClose:
    case tokFlowCtrl:
    case tokLift:
        {
            break;
        }
    default:
        {
            ret = -EINVAL;
            break;
        }
    }

    if( ret == ERROR_QUEUE_FULL )
    {
        ret = STATUS_PENDING;
        Pause();
    }

    if( ERROR( ret ) )
        return ret;

    oHelper.PostUxSockEvent(
        byToken, pNewBuf );

    return ret;
}

gint32 CIfUxListeningRelayTaskMH::Pause()
{
    return CIfUxTaskBase::Pause();
}

gint32 CIfUxListeningRelayTaskMH::Resume()
{
    return CIfUxTaskBase::Resume();
}

gint32 CStreamServerRelayMH::OnOpenStreamComplete(
    IEventSink* pCallback,
    IEventSink* pIoReqTask,
    IConfigDb* pContext )
{
    if( pCallback == nullptr )
        return -EINVAL;

    if( !IsConnected() )
        return ERROR_STATE;

    gint32 ret = 0;
    gint32 iStmId = -1;

    do{
        if( pIoReqTask == nullptr ||
            pContext == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        CCfgOpenerObj oIoReq( pIoReqTask );
        IConfigDb* pResp = nullptr;
        ret = oIoReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        gint32 iRet = 0;
        CCfgOpener oResp( pResp );
        ret = oResp.GetIntProp(
            propReturnValue, ( guint32& )iRet );
        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        ret = oResp.GetIntProp(
            0, ( guint32& )iStmId );
        if( ERROR( ret ) )
            break;

        guint32 dwProtocol;
        ret = oResp.GetIntProp(
            1, dwProtocol );
        if( ERROR( ret ) )
            break;

        if( dwProtocol != protoStream )
        {
            ret = -EPROTO;
            break;
        }

        CParamList oContext( pContext );
        IConfigDb* pDataDesc = nullptr;

        ret = oContext.GetPointer(
            0, pDataDesc );

        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        std::string strPath = 0;
        oContext.GetIntProp( 1, dwPortId );
        oContext.GetStrProp( 2, strPath );
        oContext.Push( iStmId );

        CRpcRouterBridge* pRouter = 
        static_cast< CRpcRouterBridge* >
            ( GetParent() );

        // strip the component for current node
        // from the router path.
        std::string strNode; 
        ret = pRouter->GetNodeName(
            strPath, strNode );
        if( ERROR( ret ) )
            break;

        std::string strNewPath =
            strPath.substr( 1 + strNode.size() );

        EnumClsid iClsid = iid( IStreamMH );
        if( strNewPath.empty() )
        {
            // routing to the local stream
            // interface
            strNewPath = "/";
            iClsid = iid( IStream );
        }

        // update the path and iids
        CCfgOpener oDesc( pDataDesc );
        IConfigDb* pTransCtx = nullptr;
        ret = oDesc.GetPointer(
            propTransCtx, pTransCtx );
        if( ERROR( ret ) )
            break;

        // update the router path
        CCfgOpener oTransCtx( pTransCtx );
        oTransCtx.SetStrProp(
            propRouterPath, strNewPath );

        if( iClsid == iid( IStream ) )
        {
            // update the iid
            oDesc[ propIid ] = iid( IStream );
        }

        // find the bridge proxy
        InterfPtr pIf;
        ret = pRouter->GetBridgeProxy(
            dwPortId, pIf );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxy* pProxy = pIf;
        if( unlikely( pProxy == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        TaskletPtr pWrapper;
        ret = NEW_PROXY_RESP_HANDLER2(
            pWrapper, ObjPtr( this ),
            &CStreamServerRelayMH::OnFetchDataComplete,
            pCallback, oContext.GetCfg() );

        if( ERROR( ret ) )
            break;

        // emit the FETCH_DATA request
        guint32 dwSize = 0x20;
        guint32 dwOffset = 0;
        ret = pProxy->super::FetchData_Proxy(
            pDataDesc, iStmId, dwOffset, dwSize,
            pWrapper );
        
        if( ERROR( ret ) )
            ( *pWrapper )( eventCancelTask );

    }while( 0 );

    if( ERROR( ret ) )
    {
        // complete the invoke task from the
        // reqfwdr
        CParamList oResp;
        oResp[ propReturnValue ] = ret;

        CCfgOpenerObj oTaskCfg(
            ( CObjBase* )pCallback );

        oTaskCfg.SetObjPtr( propRespPtr,
            ObjPtr( oResp.GetCfg() ) );

        if( iStmId > 0 )
        {
            CRpcTcpBridgeProxy* pProxy =
                ObjPtr( this );

            pProxy->CloseLocalStream(
                nullptr, iStmId );
        }
        pCallback->OnEvent( eventTaskComp,
            ret, 0, 0 );
    }

    return ret;
}

gint32 CStreamServerRelayMH::FetchData_Server(
    IConfigDb* pDataDesc,           // [in]
    gint32& fd,                     // [out]
    guint32& dwOffset,              // [in, out]
    guint32& dwSize,                // [in, out]
    IEventSink* pCallback )
{
    // 1. open a stream with the next `Node'
    //
    // 2. send the FETCH_DATA request with the
    // stream id
    //
    // 3. create channel object BridgeProxyStream
    // object and start it
    //
    // 4. complete FETCH_DATA request.

    if( pDataDesc == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CParamList oContext;

    do{
        CRpcRouterBridge* pRouter = 
        static_cast< CRpcRouterBridge* >
            ( GetParent() );

        CCfgOpener oDataDesc( pDataDesc );
        IConfigDb* pTransCtx = nullptr;
        ret = oDataDesc.GetPointer(
            propTransCtx, pTransCtx );
        if( ERROR( ret ) )
            break;

        CCfgOpener oTransCtx( pTransCtx );
        std::string strPath;
        ret = oTransCtx.GetStrProp(
            propRouterPath, strPath );
        if( ERROR( ret ) )
            break;

        if( strPath == "/" )
        {
            // should go to the IStream interface
            // not this one
            ret = -EINVAL;
            break;
        }

        InterfPtr pIf;
        ret= pRouter->GetBridgeProxyByPath(
            strPath, pIf );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        CRpcTcpBridgeProxy* pProxy = pIf;
        if( unlikely( pProxy == nullptr ) )
        {
            ret = -EFAULT;
            break;
        }

        gint32 iStmId = -1;
        guint32 dwPortId = 0;

        CCfgOpenerObj oIfCfg( pProxy );
        ret = oIfCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        oContext.Push( ObjPtr( pDataDesc ) );
        oContext.Push( dwPortId );
        oContext.Push( strPath );

        TaskletPtr pWrapper;
        ret = NEW_PROXY_RESP_HANDLER2(
            pWrapper, ObjPtr( this ),
            &CStreamServerRelayMH::OnOpenStreamComplete,
            pCallback, oContext.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = pProxy->OpenStream(
            protoStream, iStmId, pWrapper );

        if( ERROR( ret ) )
            ( *pWrapper )( eventCancelTask );

    }while( 0 );

    if( ret != STATUS_PENDING )
        oContext.ClearParams();

    return ret;
}

gint32 CStreamServerRelayMH::CreateUxStream(
    IConfigDb* pDataDesc,
    gint32 iPrxyStmId, EnumClsid iClsid,
    bool bServer,
    InterfPtr& pIf )
{
    if( pDataDesc == nullptr ||
        iClsid == clsid( Invalid ) ||
        iPrxyStmId < 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CParamList oNewCfg;

        oNewCfg.SetBoolProp(
            propIsServer, bServer );

        oNewCfg.SetPointer( propParentPtr,
            ( CObjBase* )GetInterface() ); 

        oNewCfg[ propIfStateClass ] = 
            PORT_CLASS_LOOPBACK_PDO;

        std::string strBusName =
            std::string( PORT_CLASS_LOCALDBUS );
        strBusName += "_0";

        oNewCfg[ propBusName ] = strBusName;

        // loopback port id 
        oNewCfg[ propPortId ] = 1;

        ret = oNewCfg.CopyProp(
            propKeepAliveSec, pDataDesc );

        if( ERROR( ret ) )
            break;

        oNewCfg.CopyProp(
            propTimeoutSec, pDataDesc );

        if( ERROR( ret ) )
            break;

        oNewCfg.Push( ObjPtr( pDataDesc ) );

        // pass the pointer to bridge proxy
        oNewCfg[ propIfPtr ] = pIf;

        // pass the proxy stream id
        oNewCfg.SetIntProp(
            propPeerStmId, iPrxyStmId );

        ret = pIf.NewObj( iClsid,
            oNewCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

    }while( 0 );
    
    return ret;
}
// request to the remote server completed
gint32 CStreamServerRelayMH::OnFetchDataComplete(
    IEventSink* pCallback,
    IEventSink* pIoReq,
    IConfigDb* pContext )
{
    if( pCallback == nullptr ||
        pIoReq == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    gint32 iFd = -1;
    gint32 iStmId = -1;

    do{
        if( pContext == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        CCfgOpenerObj oReq( pIoReq );
        IConfigDb* pResp = nullptr;
        ret = oReq.GetPointer(
            propRespPtr, pResp );
        if( ERROR( ret ) )
            break;

        CCfgOpener oResp( pResp );
        gint32 iRet = 0;
        ret = oResp.GetIntProp( propReturnValue,
            ( guint32& ) iRet );

        if( ERROR( ret ) )
            break;

        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        ret = oResp.GetIntProp(
            1, ( guint32& )iFd );
        if( ERROR( ret ) )
            break;

        CCfgOpener oContext( pContext );
        IConfigDb* pDataDesc = nullptr;
        ret = oContext.GetPointer( 0, pDataDesc );
        if( ERROR( ret ) )
            break;

        ret = oContext.GetIntProp( 3,
            ( guint32& )iStmId );
        if( ERROR( ret ) )
            break;

        guint32 dwPortId = 0;
        ret = oContext.GetIntProp( 1,
            dwPortId );
        if( ERROR( ret ) )
            break;

        InterfPtr pProxy;
        CRpcRouterBridge* pRouter = 
        static_cast< CRpcRouterBridge* >
            ( GetParent() );

        ret = pRouter->GetBridgeProxy(
            dwPortId, pProxy );
        if( ERROR( ret ) )
            break;

        CCfgOpener oDesc( pDataDesc );
        IConfigDb* pTransCtx = nullptr;
        ret = oDesc.GetPointer(
            propTransCtx, pTransCtx );
        if( ERROR( ret ) )
            break;

        CCfgOpener oTransCtx( pTransCtx );

        // recover the original router path
        oTransCtx.CopyProp(
            propRouterPath, 2, pContext );
        // recover the original iid
        oDesc.SetIntProp(
            propIid, iid( IStreamMH ) );

        // we need to do the following things
        // before reponse to the remote client
        //
        // 1. create and start the tcpbdgestm proxy
        InterfPtr pUxIf = pProxy;
        ret = CreateUxStream( pDataDesc, iFd,
            clsid( CRpcTcpBridgeProxyStream ),
            false, pUxIf );

        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oUxIf(
            ( CObjBase*) pUxIf );

        oUxIf.SetIntProp( propStreamId,
            ( guint32& )iStmId );

        // to set CIfUxListeningRelayTask to
        // receive both incoming stream as well as
        // control events.
        oUxIf.SetBoolProp( propListenOnly, true );

        CParamList oParams;
        oParams[ propIfPtr ] = ObjPtr( this );
        oParams.Push( ObjPtr( pUxIf ) );
        oParams.Push( ObjPtr( pDataDesc ) );
        oParams.Push( iStmId );

        oParams[ propEventSink ] =
            ObjPtr( pCallback );

        TaskletPtr pStartTask;
        ret = pStartTask.NewObj(
            clsid( CIfStartUxSockStmRelayTaskMH ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = this->AddSeqTask( pStartTask );

    }while( 0 );
    
    if( ERROR( ret ) )
    {
        CParamList oResp;
        oResp[ propReturnValue ] = ret;
        OnServiceComplete(
            oResp.GetCfg(), pCallback );

        if( iFd > 0 )
        {
            // close the fd to notify the failure
            close( iFd );
            iFd = -1;
        }
        if( iStmId > 0 )
        {
            CRpcTcpBridge* pBridge = ObjPtr( this );
            if( likely( pBridge != nullptr ) )
            {
                pBridge->CloseLocalStream(
                    nullptr, iStmId );
            }
        }
    }

    if( pContext )
    {
        CParamList oContext( pContext );
        oContext.ClearParams();
    }

    return ret;
}

// tokClose is received or error in connection
gint32 CStreamServerRelayMH::OnClose(
    gint32 iStmId, IEventSink* pCallback )
{
    gint32 ret = 0;
    if( iStmId < 0 )
        return -EINVAL;

    if( iStmId == TCP_CONN_DEFAULT_CMD ||
        iStmId == TCP_CONN_DEFAULT_STM )
        return -EINVAL;

    do{
        InterfPtr pIf;
        ret = GetUxStream( iStmId, pIf );
        if( ERROR( ret ) )
            break;

        CRpcTcpBridgeProxyStream* pProxy = pIf;
        gint32 iBdgePrxyId =
            pProxy->GetStreamId( true );

        // write a close token to the bridge
        // proxy's peer directly and bypass the
        // m_pWritingTask
        TaskletPtr pSendClose;
        BufPtr pBuf( true );
        *pBuf = ( guint8 )tokClose;
        ret = DEFER_IFCALL_NOSCHED(
            pSendClose, ObjPtr( this ),
            &CRpcTcpBridgeShared::WriteStream,
            iBdgePrxyId, *pBuf, 1,
            ( IEventSink* )nullptr );

        if( ERROR( ret ) )
            break;

        // Let the WriteStream to notify this task
        // when completed
        CIfDeferCallTask* pTemp = pSendClose;
        BufPtr pwscb( true );
        *pwscb = ObjPtr( pSendClose );
        pTemp->UpdateParamAt( 3, pwscb );

        // add the sequence queue to make sure the
        // tokClose is sent before the hCannel is
        // closed.
        ret = this->AddSeqTask( pSendClose );
        if( ERROR( ret ) )
            ( *pSendClose )( eventCancelTask );

        ret = super::OnClose( iStmId, pCallback );

    }while( 0 );

    return ret;
}

gint32 CIfStartUxSockStmRelayTaskMH::OnTaskComplete(
    gint32 iRet )
{
    gint32 ret = 0;

    CParamList oResp;
    CParamList oParams(
        ( IConfigDb* )GetConfig() );

    CRpcServices* pUxSvc = nullptr;
    gint32 iStmId = -1;

    InterfPtr pParentIf;

    do{
        if( ERROR( iRet ) )
        {
            ret = iRet;
            break;
        }

        ObjPtr pIf;
        ret = oParams.GetObjPtr( propIfPtr, pIf );
        if( ERROR( ret ) )
            break;
        
        pParentIf = pIf;
        CRpcServices* pParent = pIf;

        IStream* pStream = nullptr;

        CStreamServerRelayMH* pSvr =
            ObjPtr( pParent );
        pStream = pSvr;

        if( pStream == nullptr ||
            pParent == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        ret = oParams.GetPointer( 0, pUxSvc );
        if( ERROR( ret ) )
            break;

        if( pUxSvc == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        HANDLE hChannel = ( HANDLE )pUxSvc;
        InterfPtr pChanlIf( pUxSvc );
        ret = pStream->AddUxStream(
            hChannel, pChanlIf );

        if( ERROR( ret ) )
            break;

        oParams.GetIntProp(
            2, ( guint32& )iStmId ) ;

        CStreamServerRelayMH* pStmRly = pParentIf;
        ret = pStmRly->BindUxTcpStream( 
            hChannel, iStmId );

        if( ERROR( ret ) )
        {
            break;
        }

        // set the response for FETCH_DATA request
        oResp[ propReturnValue ] = STATUS_SUCCESS;
        // push the pDataDesc
        oResp.Push( ( ObjPtr& ) oParams[ 1 ] );

        // push the tcp stream id
        oResp.Push( ( guint32 )iStmId );
        oResp.SetBoolProp( propNonFd, true );

        // placeholders
        oResp.Push( 0 );
        oResp.Push( 0x20 );

        // set the response for OpenChannel
        TaskletPtr pConnTask;
        ret = DEFER_IFCALL_NOSCHED( pConnTask,
            ObjPtr( pParent ),
            &IStream::OnConnected,
            hChannel );

        if( ERROR( ret ) )
            break;

        ret = pParent->RunManagedTask( pConnTask );
        if( ERROR( ret ) )
            ( *pConnTask )( eventCancelTask );

        if( ret == STATUS_PENDING )
            ret = 0;

    }while( 0 );

    while( ERROR( ret ) )
    {
        if( pUxSvc == nullptr || 
            pParentIf.IsEmpty() )
            break;

        if( iStmId < 0 )
            break;

        CRpcServices* pParent = pParentIf;
        CStreamServerRelayMH* pStream =
            ObjPtr( pParent );

        if( unlikely( pStream == nullptr ) )
            break;

        pStream->OnClose( iStmId );

        break;
    }

    if( ERROR( ret ) && SUCCEEDED( iRet ) )
    {
        oResp[ propReturnValue ] = ret;
        iRet = ret;
    }

    EventPtr pEvt;
    ret = GetInterceptTask( pEvt );
    if( SUCCEEDED( ret ) )
    {
        CCfgOpenerObj oCfg( ( IEventSink* )pEvt );
        oCfg.SetPointer( propRespPtr,
            ( CObjBase* )oResp.GetCfg() );

        pEvt->OnEvent( eventTaskComp, iRet, 0, 0 );
    }

    oParams.ClearParams();
    ClearClientNotify();

    return iRet;
}