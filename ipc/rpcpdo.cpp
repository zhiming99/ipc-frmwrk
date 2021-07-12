/*
 * =====================================================================================
 *
 *       Filename:  rpcpdo.cpp
 *
 *    Description:  implementation of CRpcPdoPort class
 *
 *        Version:  1.0
 *        Created:  11/27/2016 09:46:52 PM
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
#include "dbusport.h"
#include "emaphelp.h"
#include "reqopen.h"

namespace rpcf
{

using namespace std;

CRpcBasePort::CRpcBasePort( const IConfigDb* pCfg )
    : CPort( pCfg )
{
}


gint32 CRpcBasePort::GetMatchMap(
    IRP* pIrp, MatchMap*& pMap )
{
    pMap = &m_mapEvtTable;
    return 0;
}
/**
* @name DispatchSignalMsg
* @{ Dispatch the signal messages to the pending irps */
/** pMessage : the dbus message to dispatch 
 * The method has two strategies to dispatch the event
 *
 * 1. iterate through the eventTable to find all the irps
 * and complete the irps with the message, which is suitable
 * for CDBusLocalPdo
 *
 * 2. find the first irp which waiting for this event and
 * complete the irp with the message, which is for
 * CDBusProxyPdo
 *
 * @} */

gint32 CRpcBasePort::DispatchSignalMsg(
    DBusMessage* pMessage )
{
    if( pMessage == nullptr )
        return -EINVAL;

    gint32 ret = -ENOENT;
    DMsgPtr pMsg;

    CStdRMutex oPortLock( GetLock() );
    guint32 dwPortState = GetPortState();

    if( !CanAcceptMsg( dwPortState ) )
    {
        // prevent messages piling in the
        // match entry
        return ERROR_STATE;
    }

    map< MatchPtr, MATCH_ENTRY >::iterator itr =
        m_mapEvtTable.begin();

    vector< IrpPtr > vecIrpsToComplete;
    vector< IrpPtr > vecIrpsError;

    while( itr != m_mapEvtTable.end() )
    {
        pMsg = pMessage;
        gint32 ret1 =
            itr->first->IsMyMsgIncoming( pMsg );

        if( ERROR( ret1 ) )
        {
            ++itr;
            continue;
        }

        MATCH_ENTRY& oMe = itr->second;
        if( !oMe.IsConnected() )
        {
            ++itr;
            continue;
        }

        oMe.m_quePendingMsgs.push_back( pMsg );
        if( oMe.m_queWaitingIrps.empty() )
        {
            if( oMe.m_quePendingMsgs.size()
                > MAX_DBUS_REQS )
            {
                // discard the oldest msg if too many
                oMe.m_quePendingMsgs.pop_front();
            }
            ++itr;
            continue;
        }

        while( oMe.m_queWaitingIrps.size() > 0 &&
            oMe.m_quePendingMsgs.size() > 0 )
        {
            // first come, first service
            IrpPtr pIrp; 
            pIrp = oMe.m_queWaitingIrps.front();
            oMe.m_queWaitingIrps.pop_front();

            pMsg = oMe.m_quePendingMsgs.front();
            oMe.m_quePendingMsgs.pop_front();

            oPortLock.Unlock();
            // BUGBUG: the oMe could turn invalid
            // at this point under extreme
            // condition.
            CStdRMutex oIrpLock( pIrp->GetLock() );
            oPortLock.Lock();
            ret = pIrp->CanContinue( IRP_STATE_READY );
            if( ERROR( ret ) )
            {
                // discard the irp and the
                // messages
                continue;
            }

            if( pIrp->GetStackSize() == 0 )
            {
                // bad irp
                vecIrpsError.push_back( pIrp );
                continue;
            }

            IrpCtxPtr& pCtx = pIrp->GetTopStack();
            if( pIrp->IoDirection() == IRP_DIR_IN
                && pIrp->MinorCmd() == IRP_MN_IOCTL
                && pIrp->MajorCmd() == IRP_MJ_FUNC )
            {
                BufPtr bufPtr( true );
                *bufPtr = pMsg;

                pCtx->SetRespData( bufPtr );
                pCtx->SetStatus( 0 );
                vecIrpsToComplete.push_back( pIrp );
                continue;
            }

            //wierd, what is wrong with this irp
            pCtx->SetStatus( -EINVAL );
            vecIrpsError.push_back( pIrp );
        }
        ++itr;
    }

    oPortLock.Unlock();

    guint32 dwIrpCount = vecIrpsToComplete.size();
    for( guint32 i = 0; i < dwIrpCount; i++ )
    {
        GetIoMgr()->CompleteIrp(
            vecIrpsToComplete[ i ] );
    }

    dwIrpCount = vecIrpsError.size();
    for( guint32 i = 0; i < dwIrpCount; i++ )
    {
        GetIoMgr()->CancelIrp(
            vecIrpsError[ i ], true, -EINVAL );
    }

    oPortLock.Lock();

    if( dwIrpCount > 0 )
        ret = 0;

    return ret;
}

gint32 CRpcBasePort::FindIrpForResp(
    DBusMessage* pMessage, IrpPtr& pReqIrp )
{
    gint32 ret = 0;

    if( pMessage == nullptr )
        return -EINVAL;

    DMsgPtr pMsg( pMessage );

    do{
        IrpPtr pIrp;

        if( true )
        {
            guint32 dwSerial = 0;

            CStdRMutex oPortLock( GetLock() );
            guint32 dwPortState = GetPortState();
            if( !CanAcceptMsg( dwPortState ) )
                return ERROR_STATE;

            ret = pMsg.GetReplySerial( dwSerial );
            if( ERROR( ret ) )
                break;

            map< guint32, IrpPtr >::iterator itr =
                m_mapSerial2Resp.find( dwSerial );

            if( itr == m_mapSerial2Resp.end() )
            {
                ret = OnRespMsgNoIrp( pMessage );
                if( ret == -ENOTSUP )
                    ret = -ENOENT;
                break;
            }

            pIrp = itr->second;
            m_mapSerial2Resp.erase( itr );
        }

        if( !pIrp.IsEmpty() )
        {
            CStdRMutex oIrpLock( pIrp->GetLock() ) ;
            ret = pIrp->CanContinue( IRP_STATE_READY );
            if( ERROR( ret ) )
                break;

            IrpCtxPtr& pIrpCtx = pIrp->GetCurCtx();

            if( pMsg.GetType()
                == DBUS_MESSAGE_TYPE_METHOD_RETURN )
            {
                BufPtr pBuf( true );
                *pBuf = pMsg;
                pIrpCtx->SetRespData( pBuf  );
                pIrpCtx->SetStatus( 0 );
            }
            else if( pMsg.GetType()
                == DBUS_MESSAGE_TYPE_ERROR )
            {
                gint32 iRet = pMsg.GetErrno();
                if( iRet == 0 )
                    iRet = ERROR_FAIL;

                pIrpCtx->SetStatus( iRet );
            }

            pReqIrp = pIrp;
        }
        else
        {
            ret = -EFAULT;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::DispatchRespMsg(
    DBusMessage* pMsg )
{
    IrpPtr pIrp;
    if( pMsg == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        ret = FindIrpForResp( pMsg, pIrp );
        if( ret == -ENOENT )
        {
            /*DMsgPtr pDumpMsg( pMsg );
            DebugPrint( GetPortState(),
                "Error cannot find irp for response messages\n%s\n, port=%s, 0x%x",
                pDumpMsg.DumpMsg().c_str(),
                CoGetClassName( GetClsid() ), 
                ( LONGWORD )this );*/
        }

        if( ERROR( ret ) )
            return ret;

    }while( 0 );

    ret = GetIoMgr()->CompleteIrp( pIrp );

    return ret;
}

gint32 CRpcBasePort::OnSubmitIrp( IRP* pIrp )
{
    gint32 ret = 0;

    do{
        if( pIrp == nullptr
            || pIrp->GetStackSize() == 0 )
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
        case IRP_MN_IOCTL:
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

    }while( 0 );

    return ret;
}

DBusHandlerResult CRpcBasePort::DispatchMsg(
    gint32 iType, DBusMessage* pMsg )
{
    DBusHandlerResult ret =
        DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    switch( iType )
    {
    case DBUS_MESSAGE_TYPE_SIGNAL:
        {
            if( DispatchSignalMsg( pMsg ) != -ENOENT )
                ret = DBUS_HANDLER_RESULT_HANDLED;
            break;
        }
    case DBUS_MESSAGE_TYPE_ERROR:
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        {
            if( DispatchRespMsg( pMsg ) != -ENOENT )
                ret = DBUS_HANDLER_RESULT_HANDLED;
            break;
        }
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
    default:
        break;
    }

    return ret;
}

gint32 CRpcBasePort::DispatchKeepAliveMsg(
    DBusMessage* pMessage )
{

    if( pMessage == nullptr )
        return -EINVAL;

    DMsgPtr pMsg( pMessage );
    IrpPtr pIrp;
    bool bFound = false;

    
    if( CanAcceptMsg( GetPortState() ) )
    {
        CStdRMutex oPortLock( GetLock() );
        guint32 dwSerial = 0;
        pMsg.GetSerial( dwSerial );

        if( m_mapSerial2Resp.find( dwSerial )
            != m_mapSerial2Resp.end() )
        {
            pIrp = m_mapSerial2Resp[ dwSerial ];
            bFound = true;
        }
    }

    if( bFound )
    {
        // extend the timer for the connection request
        gint32 ret = 0;

        CStdRMutex oLock( pIrp->GetLock() );
        ret = pIrp->CanContinue( IRP_STATE_READY );
        if( ERROR( ret ) )
            return ret;

        pIrp->ResetTimer();
    }
    return 0;
}

DBusHandlerResult CRpcBasePort::PreDispatchMsg(
    gint32 iMsgType, DBusMessage* pMessage )
{
    DBusHandlerResult ret =
        DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if( iMsgType !=
        DBUS_MESSAGE_TYPE_METHOD_RETURN )
        return ret;

    if( pMessage == nullptr )
        return ret;

    DMsgPtr pMsg( pMessage );
    do{
        // handle the keep-alive message here
        string strMember = pMsg.GetMember();

        if( strMember.empty() )
            break;

        if( strMember == DBUS_RESP_KEEP_ALIVE ) 
        {
            // we have received a KEEP_ALIVE message
            DispatchKeepAliveMsg( pMsg );
            ret = DBUS_HANDLER_RESULT_HANDLED;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::DispatchData(
    CBuffer* pData )
{
    gint32 ret = 0;

    if( pData == nullptr )
        return -EINVAL;


    if( m_pTestIrp.IsEmpty() )
    {
        m_pTestIrp.NewObj();
        m_pTestIrp->AllocNextStack( nullptr );
        IrpCtxPtr& pIrpCtx = m_pTestIrp->GetTopStack(); 
        pIrpCtx->SetMajorCmd( IRP_MJ_FUNC );
        pIrpCtx->SetMinorCmd( IRP_MN_IOCTL );
        pIrpCtx->SetCtrlCode( CTRLCODE_LISTENING );
    }

    DMsgPtr& pMsg = *pData;
    gint32 iType = pMsg.GetType();
    DBusHandlerResult ret2; 
    guint32 dwPortState = 0;
    if( true )
    {
        CStdRMutex oPortLock( GetLock() );
        dwPortState = GetPortState();

        if( !( dwPortState == PORT_STATE_READY
             || dwPortState == PORT_STATE_BUSY_SHARED
             || dwPortState == PORT_STATE_BUSY
             || dwPortState == PORT_STATE_BUSY_RESUME ) )
        {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }

    guint32 dwOldState = 0;
    do{
        ret = CanContinue( m_pTestIrp,
            PORT_STATE_BUSY_SHARED, &dwOldState );

        if( ret == -EAGAIN )
        {
            usleep(100);
            continue;
        }
        break;

    }while( 1 );

    if( ERROR( ret ) )
    {
#ifdef DEBUG
        string strDump = pMsg.DumpMsg();

        DebugPrint( ret, 
            "Error, dmsg cannot be processed, %s, curstate = %d, requested state =%d",
            strDump.c_str(),
            dwOldState,
            PORT_STATE_BUSY_SHARED );
#endif

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    do{

        ret2 = PreDispatchMsg( iType, pMsg );
        if( ret2 != DBUS_HANDLER_RESULT_NOT_YET_HANDLED )
            break;
     
        ret2 = DispatchMsg( iType, pMsg );
        if( ret2 != DBUS_HANDLER_RESULT_NOT_YET_HANDLED )
            break;

        PostDispatchMsg( iType, pMsg );

    }while( 0 );

    PopState( dwOldState );

    if( iType == DBUS_MESSAGE_TYPE_SIGNAL
        && ret2 == DBUS_HANDLER_RESULT_HALT )
    {
        // a channel to stop processing the signal
        // further
        ret = ERROR_PREMATURE;
    }
    else if( ret2 == DBUS_HANDLER_RESULT_NOT_YET_HANDLED )
    {
        ret = -ENOENT;
    }
    
    return ret;
}

gint32 CRpcBasePort::HandleRegMatch(
    IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    do{
        MatchMap *pMap = nullptr;

        // let's process the func irps
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        IMessageMatch* pMatch =
            *( ObjPtr* )( *pCtx->m_pReqData );

        if( pMatch == nullptr )
        {
            ret = -EINVAL;
            break;
        }


        MatchPtr matchPtr( pMatch );
        ret = GetMatchMap( pIrp, pMap );

        if( ERROR( ret ) )
            break;

        bool bNew = false;
        CStdRMutex oPortLock( GetLock() );
        MatchMap::iterator itr = 
            pMap->find( matchPtr );
        MATCH_ENTRY* pme = nullptr;
        if( itr == pMap->end() )
        {
            // new match rule
            // let's register a match 
            bNew = true;
        }
        else
        {
            itr->second.AddRef();
            if( !itr->second.IsConnected() )
                ret = -ENOTCONN;
        }
        oPortLock.Unlock();
        if( bNew )
        {
            // add a dbus match rule, 
            // Note: the operation contains dbus
            // connection lock, and cannot put
            // within the port lock, otherwise,
            // deadlock can happen.
            MATCH_ENTRY oMe;
            ret = SetupDBusSetting( pMatch );
            if( ERROR( ret ) )
                break;

            if( ret == ENOTCONN )
            {
                // note that the dbus match
                // is not added if not connected
                oMe.SetConnected( false );
                ret = -ENOTCONN;
            }
            else
            {
                oMe.SetConnected( true );
            }
            oMe.AddRef();
            oPortLock.Lock();
            ( *pMap )[ matchPtr ] = oMe;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::HandleUnregMatch( IRP* pIrp )
{

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    do{
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        IMessageMatch* pMatch =
            *( ObjPtr* )( *pCtx->m_pReqData );

        if( pMatch == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CStdRMutex oPortLock( GetLock() );

        MatchMap *pMap = nullptr;

        MatchPtr matchPtr( pMatch );

        ret = GetMatchMap( pIrp, pMap );

        if( ERROR( ret ) )
            break;

        if( pMap->find( matchPtr ) != pMap->end() )
        {
            MATCH_ENTRY& oEntry = ( *pMap )[ matchPtr ] ;

            gint32 iRefCount = oEntry.Release();
            if( iRefCount == 0 )
            {
                if( oEntry.m_quePendingMsgs.size() )
                    oEntry.m_quePendingMsgs.clear();

                MATCH_ENTRY tempEnt = oEntry;
                pMap->erase( matchPtr );
                oPortLock.Unlock();

                // nobody referencing this match rule
                // remove it from the req/evt table
                ret = ClearDBusSetting( pMatch );
                if( ERROR( ret ) )
                    break;

                // completing the irps with error 
                // and pending on the match
                while( tempEnt.m_queWaitingIrps.size() )
                {
                    IrpPtr irpPtr =
                        tempEnt.m_queWaitingIrps.front();

                    tempEnt.m_queWaitingIrps.pop_front();
                    IrpCtxPtr& pCtx = irpPtr->GetCurCtx();
                    pCtx->SetStatus( -ECANCELED );

                    GetIoMgr()->CancelIrp(
                        irpPtr, true, -ECANCELED );
                }
            }
        }
        else
        {
            ret = -ENOENT;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::SchedCompleteIrps(
    std::deque< IrpPtr >& queIrps,
    std::deque< DMsgPtr>& queMsgs )
{
    gint32 ret = 0;
    do{
        IrpVecPtr pIrpVec( true );
        while( queIrps.size() && queMsgs.size() )
        {
            IrpPtr& pIrp = queIrps.front();
            queIrps.pop_front();

            if( pIrp.IsEmpty() ||
                pIrp->GetStackSize() == 0 )
                continue;

            CStdRMutex oIrpLock(
                pIrp->GetLock() );

            ret = pIrp->CanContinue(
                IRP_STATE_READY );

            if( ERROR( ret ) )
                continue;

            IrpCtxPtr& pCtx =
                pIrp->GetTopStack();

            BufPtr pBuf( true );
            *pBuf = queMsgs.front();
            queMsgs.pop_front();
            pCtx->SetRespData( pBuf );
            pCtx->SetStatus( 0 );

            ( *pIrpVec )().push_back( pIrp );
        }

        if( ( *pIrpVec )().empty() )
        {
            ret = -ENOENT;
            break;
        }

        CParamList oParams;
        ret = oParams.Push( STATUS_SUCCESS );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        pObj = pIrpVec;
        ret = oParams.Push( pObj );
        if( ERROR( ret ) )
            break;

        oParams.Push( ( bool )true );

        oParams.SetPointer(
            propIoMgr, GetIoMgr() );

        ret = GetIoMgr()->ScheduleTask(
            clsid( CCancelIrpsTask ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::HandleListening( IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    IrpPtr pIrpLocked( pIrp );

    do{
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        IMessageMatch* pMatch =
            *( ObjPtr* )( *pCtx->m_pReqData );

        if( pMatch == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CStdRMutex oPortLock( GetLock() );

        MatchMap *pMap = nullptr;

        MatchPtr matchPtr( pMatch );

        ret = GetMatchMap( pIrp, pMap );
        if( ERROR( ret ) )
            break;

        MatchMap::iterator itr =
            pMap->find( matchPtr );

        if( itr != pMap->end() )
        {
            // first, check if there are pending
            // messages if there are, this irp can
            // return immediately with the first
            // pending message.
            MATCH_ENTRY& oMe = itr->second;

            if( !oMe.IsConnected() )
            {
                ret = -ENOTCONN;
                break;
            }
            if( oMe.m_quePendingMsgs.size() &&
                oMe.m_queWaitingIrps.empty() )
            {
                BufPtr pBuf( true );
                *pBuf = oMe.m_quePendingMsgs.front();
                pCtx->SetRespData( pBuf );

                oMe.m_quePendingMsgs.pop_front();
                // immediate return
            }
            else if( oMe.m_quePendingMsgs.size() &&
                !oMe.m_queWaitingIrps.empty() )
            {
                bool bFound = false;
                oMe.m_queWaitingIrps.push_back( pIrp );
                std::deque< IrpPtr > queIrps;
                std::deque< DMsgPtr > queMsgs;
                while( oMe.m_queWaitingIrps.size() &&
                    oMe.m_quePendingMsgs.size() )
                {
                    queIrps.push_back(
                        oMe.m_queWaitingIrps.front() );
                    queMsgs.push_back(
                        oMe.m_quePendingMsgs.front() );
                    oMe.m_queWaitingIrps.pop_front();
                    oMe.m_quePendingMsgs.pop_front();
                    if( queIrps.front() == pIrpLocked )
                        bFound = true;
                }
                oPortLock.Unlock();
                ret = SchedCompleteIrps(
                    queIrps, queMsgs );
                // BUGBUG:Discard all the remaining
                // messages.
                if( SUCCEEDED( ret ) )
                    ret = STATUS_PENDING;
                else if( ERROR( ret ) && !bFound )
                {
                    // try best to keep in shape
                    // on error
                    ret = STATUS_PENDING;
                }
            }
            else
            {
                // queue the irp
                oMe.m_queWaitingIrps.push_back(
                    IrpPtr( pIrp ) );
                ret = STATUS_PENDING;
            }
        }
        else
        {
            // must be registered before listening
            ret = -ENOENT;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::SubmitIoctlCmd( IRP* pIrp )
{
    gint32 ret = 0;

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
    {
        return -EINVAL;
    }

    // let's process the func irps
    IrpCtxPtr pCtx = pIrp->GetCurCtx();

    do{

        if( pIrp->MajorCmd() != IRP_MJ_FUNC
            || pIrp->MinorCmd() != IRP_MN_IOCTL )
        {
            ret = -EINVAL;
            break;
        }

        switch( pIrp->CtrlCode() )
        {
        case CTRLCODE_REG_MATCH:
            {
                ret = HandleRegMatch( pIrp );
                break; 
            }

        case CTRLCODE_UNREG_MATCH:
            {
                ret = HandleUnregMatch( pIrp );
                break; 
            }
        case CTRLCODE_LISTENING:
            {
                ret = HandleListening( pIrp );
                break;
            }
        case CTRLCODE_FETCH_DATA:
        case CTRLCODE_SEND_DATA:
            {
                ret = HandleSendData( pIrp );
                break;
            }
        case CTRLCODE_SEND_REQ:
            {
                ret = HandleSendReq( pIrp );
                break;
            }
        case CTRLCODE_SEND_EVENT:
            {
                ret = HandleSendEvent( pIrp );
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
        pCtx->SetStatus( ret );

    return ret;
}

gint32 CRpcBasePort::ClearMatchMapInternal(
    MatchMap& oMap,
    vector< IrpPtr >& vecPendingIrps,
    vector< MatchPtr >& vecMatches )
{
    gint32 ret = 0;
    // note: this method must be called with
    // the port locked
    MatchMap::iterator itrMe = oMap.begin();

    while( itrMe != oMap.end() )
    {
        // only one of the two queues in
        // the MATCH_ENTRY is not empty
        // NOTE: normally, at this point the map
        // should be empty because the user have
        // unregistered the match earilier.
        // the RefCount is ignored here
        MATCH_ENTRY& oMe = itrMe->second;
        if( !oMe.m_queWaitingIrps.empty() )
        {
            deque<IrpPtr>::iterator itrIrp =
                oMe.m_queWaitingIrps.begin(); 

            while( !oMe.m_queWaitingIrps.empty() )
            {
                vecPendingIrps.push_back( *itrIrp );
                oMe.m_queWaitingIrps.pop_front();
                itrIrp = oMe.m_queWaitingIrps.begin(); 
            }
        }
        else
        {
            oMe.m_quePendingMsgs.clear();
        }
        vecMatches.push_back( itrMe->first );
        oMap.erase( itrMe );
        itrMe = oMap.begin();
    }
    return ret;
}

gint32 CRpcBasePort::ClearMatchMap(
    vector< IrpPtr >& vecPendingIrps,
    vector< MatchPtr >& vecMatches )
{
    return ClearMatchMapInternal(
        m_mapEvtTable,
        vecPendingIrps,
        vecMatches );
}

gint32 CRpcBasePort::RemoveIrpFromMapInternal(
    IRP* pIrp, MatchMap& oMap )
{
    gint32 ret = -ENOENT;
    bool bFound = false;

    // brutal search
    MatchMap::iterator itrMe =
        oMap.begin();

    while( itrMe != oMap.end() )
    {
        MATCH_ENTRY& oMe = itrMe->second;

        deque< IrpPtr >& irpQue =
            oMe.m_queWaitingIrps;

        if( irpQue.empty() )
        {
            break;
        }

        deque< IrpPtr >::iterator itrIrp =
            irpQue.begin(); 

        while( itrIrp != irpQue.end() )
        {
            if( pIrp->GetObjId()
                == ( *itrIrp )->GetObjId() )
            {
                irpQue.erase( itrIrp );
                bFound = true;
                break;
            }
            ++itrIrp;
        }
        ++itrMe;
    }
    if( bFound )
        ret = 0;

    return ret;
}

gint32 CRpcBasePort::RemoveIrpFromMap(
    IRP* pIrp )
{
    if( pIrp == nullptr )
        return -EINVAL;

    bool bFound = false;
    gint32 ret = -ENOENT;

    do{
        ret = RemoveIrpFromMapInternal(
            pIrp, m_mapEvtTable );

        if( ret == 0 )
        {
            bFound = true;
            break;
        }

        CStdRMutex oPortLock( GetLock() );
        // let's find where the irp is
        map< guint32, IrpPtr >::iterator itr =
            m_mapSerial2Resp.begin();

        while( itr != m_mapSerial2Resp.end() )
        {
            if( pIrp->GetObjId()
                == itr->second->GetObjId() )
            {
                bFound = true;
                break;
            }
            ++itr;
        }
        if( bFound )
        {
            m_mapSerial2Resp.erase( itr );
            break;
        }

    }while( 0 );

    if( bFound )
        ret = 0;

    return ret;
}

gint32 CRpcBasePort::CancelAllIrps( gint32 iErrno )
{

    gint32 ret = 0;

    do{
        CStdRMutex oPortLock( GetLock() );

        guint32 dwPortState = GetPortState();
        if( dwPortState != PORT_STATE_STOPPING )
        {
            ret = ERROR_STATE;
            break;
        }

        vector< IrpPtr > vecIrpRemaining;

        vector< MatchPtr > vecMatches;
        ret = ClearMatchMap(
            vecIrpRemaining, vecMatches );
        if( ERROR( ret ) )
            break;

        map< guint32, IrpPtr >::iterator itrResp =
            m_mapSerial2Resp.begin();

        while( itrResp != m_mapSerial2Resp.end() )
        {
            vecIrpRemaining.push_back( itrResp->second );
            ++itrResp;
        }

        m_mapSerial2Resp.clear();
        oPortLock.Unlock();

        for( auto elem : vecMatches )
        {
            ClearDBusSetting( elem );
        }

        vector< IrpPtr >::iterator itrIrp =
            vecIrpRemaining.begin();

        while( itrIrp != vecIrpRemaining.end() )
        {
            GetIoMgr()->CancelIrp(
                *itrIrp, true, iErrno );
            ++itrIrp;
        }

    }while( 0 );

    return ret;
}
gint32 CRpcBasePort::PreStop( IRP *pIrp )
{
    // clear all the match things
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    ret = CancelAllIrps( ERROR_PORT_STOPPED );

    // NOTE: we put the PreStop here to avoid
    // when PreStop may someday in the future
    // return pending status. 
    ret = super::PreStop( pIrp );

    return ret;
}

gint32 CRpcBasePort::CancelFuncIrp(
    IRP* pIrp, bool bForce )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    do{
        CStdRMutex oPortLock( GetLock() );
        ret = RemoveIrpFromMap( pIrp );
        guint32 dwPortState = GetPortState();

        if( dwPortState == PORT_STATE_STOPPING &&
            ERROR( ret ) )
        {
            // already removed by ClearMatchMap
            ret = 0;
            break;
        }

    }while( 0 );

    if( ERROR( ret ) )
        return ret;

    return super::CancelFuncIrp( pIrp, bForce );
}

gint32 CRpcBasePort::AddMatch(
    MatchMap& mapMatch, MatchPtr& pMatch )
{
    gint32 ret = 0;
    do{
        CStdRMutex oPortLock( GetLock() );

        MATCH_ENTRY oMe;
        oMe.AddRef();
        m_mapEvtTable[ pMatch ] = oMe;

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::RemoveMatch(
    MatchMap& mapMatch, MatchPtr& pMatch )
{
    gint32 ret = 0;
    do{
        CStdRMutex oPortLock( GetLock() );
        mapMatch.erase( pMatch );

    }while( 0 );
 
    return ret;
}

gint32 CRpcBasePort::IsIfSvrOnline(
    const DMsgPtr& pMsg )
{
    if( pMsg.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    DMsgPtr pTemp;

    if( pMsg.GetType() ==
        DBUS_MESSAGE_TYPE_METHOD_RETURN )
        return 0;

    if( pMsg.GetDestination().empty() )
    {
        // no destination, should be a signal
        // message, and no further check
        return ret;
    }

    do{
        CStdRMutex oPortLock( GetLock() );
        map< MatchPtr, MATCH_ENTRY>::iterator itr =
            m_mapEvtTable.begin();

        // if the match is not registered
        // we assume it is connected, and let
        // timeout to clean up if server is
        // not online
        ret = 0;

        while( itr != m_mapEvtTable.end() )
        {
            ret = itr->first->
                IsMyMsgOutgoing( pMsg );

            if( SUCCEEDED( ret ) )
            {
                if( itr->second.IsConnected() )
                    ret = 0;
                else
                    ret = ENOTCONN;
                break;
            }
            ++itr;
        }

    }while( 0 );

    return ret;
}

DBusHandlerResult CRpcBasePort::DispatchDBusSysMsg(
    DBusMessage* pMessage )
{
    DBusHandlerResult ret =
        DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if( pMessage == nullptr )
        return ret;

    DMsgPtr pMsg( pMessage );

    string strMember = pMsg.GetMember();

    if( strMember.empty() )
        return ret;

    if( strMember == "NameOwnerChanged" )
    {
        if( SUCCEEDED( OnModOnOffline( pMsg ) ) )
        {
            ret = DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    return ret;
}

gint32 CRpcBasePortModOnOfflineTask::operator()(
    guint32 dwContext )
{

    gint32 ret = 0;

    CParamList a( ( IConfigDb* )m_pCtx );
    CIoManager* pMgr = nullptr;
    string strModName;

    do{
        ret = a.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        guint32 dwFlags = 0;
        ret = a.Pop( dwFlags );
        if( ERROR( ret ) )
            break;

        bool bOnline =
            ( ( dwFlags & 0x01 ) > 0 );

        ret = a.GetStrProp(
            propModName, strModName );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = a.GetObjPtr(
            propPortPtr, pObj );
        if( ERROR( ret ) )
            break;

        CPort* pPort = pObj;
        if( pPort == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        // Broadcast this event. The pnp manager
        // will stop the port stack connecting to
        // this module later
        CEventMapHelper< CPort > oEvtHelper( pPort );

        guint32 eventId =
            bOnline ? eventModOnline : eventModOffline;

        ret = oEvtHelper.BroadcastEvent(
            eventConnPoint,
            eventId, dwFlags,
            ( LONGWORD* )strModName.c_str() );

    }while( 0 );

    return ret;
}

gint32 CRpcPdoPort::ScheduleModOnOfflineTask(
    const string strModName, guint32 dwFlags )
{
    gint32 ret = 0;

    // Nasty logic: 
    //
    // 1. Using subclass
    //
    // 2. Depends on the order of the PDOs in the
    // message process array
    if( clsid( CDBusLocalPdo ) != GetClsid() &&
        clsid( CDBusLoopbackPdo ) != GetClsid() )
    {
        // make sure to send just once
        return 0;
    }

    do{
        CParamList oParams;
        ret = oParams.SetPointer(
            propIoMgr, GetIoMgr() );
        if( ERROR( ret ) )
        {
            break;
        }
        ret = oParams.Push( dwFlags );
        if( ERROR( ret ) )
        {
            break;
        }

        ret = oParams.SetStrProp(
            propModName, strModName );
        if( ERROR( ret ) )
        {
            break;
        }

        ret = oParams.SetObjPtr(
            propPortPtr, ObjPtr( this ) );

        if( ERROR( ret ) )
        {
            break;
        }

        ret = GetIoMgr()->ScheduleTask(
            clsid( CRpcBasePortModOnOfflineTask ),
            oParams.GetCfg() );

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::ClearReqIrpsOnDest(
    const string& strDestination,
    vector< IrpPtr >& vecIrpsToCancel )
{
    gint32 ret = 0;
    do{
        // clear all the pending request irps
        map< guint32, IrpPtr >::iterator itr2 =
            m_mapSerial2Resp.begin();

        vector< gint32 > vecIrpToRemove;

        while( itr2 != m_mapSerial2Resp.end() )
        {
            IrpCtxPtr& pCtx =
                itr2->second->GetTopStack();

            DMsgPtr pMsg = *pCtx->m_pReqData;
            if( pMsg.IsEmpty() )
            {
                ++itr2;
                continue;
            }
            if( pMsg.GetDestination() == strDestination )
            {
                vecIrpsToCancel.push_back( itr2->second );
                vecIrpToRemove.push_back( itr2->first );
            }
            ++itr2;
        }

        for( guint32 i = 0; i < vecIrpToRemove.size(); ++i )
        {
            m_mapSerial2Resp.erase( vecIrpToRemove[ i ] );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::OnModOnOffline(
    DMsgPtr& pMsg, bool bOnline,
    const std::string& strModName )
{
    gint32 ret = 0;
    vector< IrpPtr > vecIrpsToCancel;
    do{
        bool bInterested = false;
        CStdRMutex oPortLock( GetLock() );

        guint32 dwPortState = GetPortState();
        if( !CanAcceptMsg( dwPortState ) )
        {
            ret = ERROR_STATE;
            break;
        }

        map< MatchPtr, MATCH_ENTRY>::iterator itr =
            m_mapEvtTable.begin();

        while( itr != m_mapEvtTable.end() )
        {
            if( itr->first->IsMyDest( strModName ) ||
                itr->first->IsMyObjPath( strModName ) )
            {
                bInterested = true;
                MATCH_ENTRY& oMe = itr->second;
                oMe.SetConnected( bOnline );
                if( !bOnline &&
                    oMe.m_queWaitingIrps.size() )
                {
                    // cancel those irps
                    vecIrpsToCancel.insert( 
                        vecIrpsToCancel.begin(),
                        oMe.m_queWaitingIrps.begin(),   
                        oMe.m_queWaitingIrps.end() );

                    oMe.m_queWaitingIrps.clear();
                    oMe.m_quePendingMsgs.clear();
                }
            }
            ++itr;
        }

        if( !bOnline )
        {
            // clear all the pending request irps
            // waiting for responses from that
            // module
            ClearReqIrpsOnDest(
                strModName, vecIrpsToCancel );
        }

        if( true )
        {
            // finally, schedule a task to send out
            // the module online/offline message
            guint32 dwFlags = ( guint32 )bOnline;

            if( bInterested )
                dwFlags |= MOD_ONOFFLINE_IRRELEVANT;

            ret = ScheduleModOnOfflineTask(
                strModName, dwFlags );
        }

    }while( 0 );

    for( guint32 i = 0; i < vecIrpsToCancel.size(); ++i )
    {
        GetIoMgr()->CancelIrp(
            vecIrpsToCancel[ i ], true, -ENOTCONN );
    }

    return ret;
}
/**
* @name OnModOnOffline
* handler of a dbus message indicating the remote
* or local module server is becoming online or
* offline
* @{ */
/**  @} */

gint32 CRpcBasePort::OnModOnOffline(
    DBusMessage* pDBusMsg )
{
    // mark those matches with the specified
    // interface online or offline, which will
    // block the further IO requests.

    // Performance Alert: a namechange will cause
    // all the stacks to process this message
    // once. Performance could be an issue.
    gint32 ret = 0;

    if( pDBusMsg == nullptr )
        return -EINVAL;

    vector< IrpPtr > vecIrpsToCancel;

    do{
        bool bOnline = false;
        char* pszModName = nullptr;
        CDBusError oError;
        DMsgPtr pMsg( pDBusMsg );

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

        ret = OnModOnOffline( pMsg,
            bOnline, std::string( pszModName ) );

    }while( 0 );

    return ret;
}

gint32 CRpcBasePortEx::GetMatchMap(
    IRP* pIrp, MatchMap*& pMap )
{
    gint32 ret = 0;

    if( pIrp == nullptr 
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    IrpCtxPtr& pCtx = pIrp->GetCurCtx();

    IMessageMatch* pMatch =
        ( ObjPtr& )( *pCtx->m_pReqData );

    ret = GetMatchMap( pMatch, pMap );

    return ret;
}
gint32 CRpcBasePortEx::RemoveIrpFromMap(
    IRP* pIrp )
{

    if( pIrp == nullptr )
        return -EINVAL;

    gint32 ret =
        super::RemoveIrpFromMap( pIrp );

    if( ERROR( ret ) )
    {
        ret = RemoveIrpFromMapInternal(
            pIrp, m_mapReqTable );
    }
    return ret;
}

gint32 CRpcBasePortEx::ClearMatchMap(
    vector< IrpPtr >& vecPendingIrps,
    vector< MatchPtr >& vecMatches )
{
    gint32 ret = 0;

    ret = super::ClearMatchMap(
        vecPendingIrps, vecMatches );

    if( ERROR( ret ) )
        return ret;

    ret = ClearMatchMapInternal(
        m_mapReqTable,
        vecPendingIrps,
        vecMatches );

    return ret;
}

gint32 CRpcBasePortEx::DispatchReqMsg(
    DBusMessage* pMessage )
{
    if( pMessage == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    DMsgPtr pMsg( pMessage );

    CStdRMutex oPortLock( GetLock() );

    guint32 dwPortState = GetPortState();

    if( !CanAcceptMsg( dwPortState ) )
    {
        // prevent messages piling in the match
        // entry
        return ERROR_STATE;
    }

    map< MatchPtr, MATCH_ENTRY >::iterator itr =
        m_mapReqTable.begin();

    IrpPtr pIrpToComplete;
    vector< IrpPtr > vecIrpToComplete;
    vector< IrpPtr > vecIrpsError;
    bool bQueued = false;

    while( itr != m_mapReqTable.end() )
    {
        bool bDone = false;

        ret = itr->first->IsMyMsgIncoming( pMsg );
        if( SUCCEEDED( ret ) )
        {
            bDone = true;
            do{
                // test if the match is allowed to
                // receive requests
                CCfgOpenerObj oMatch(
                    ( CObjBase* )( itr->first ) );
                guint32 dwQueSize = 0;

                ret = oMatch.GetIntProp(
                    propQueSize, dwQueSize );

                if( ERROR( ret ) )
                {
                    dwQueSize = MAX_DBUS_REQS;
                    ret = 0;
                }

                if( dwQueSize == 0 )
                {
                    ret = ERROR_QUEUE_FULL;
                    break;
                }

                MATCH_ENTRY& ome = itr->second;
                if( !ome.IsConnected() )
                {
                    // weird, not connected, how
                    // did this message arrived
                    // here
                    //
                    // waiting till the server is
                    // online
                    //
                    ret = -ENOTCONN;
                    break;
                }

                if( ome.m_queWaitingIrps.empty() )
                {
                    ome.m_quePendingMsgs.push_back( pMsg );
                    if( ome.m_quePendingMsgs.size()
                        > dwQueSize )
                    {
                        // discard the oldest msg
                        ome.m_quePendingMsgs.pop_front();
                    }
                    bQueued = true;
                    break;
                }

                ome.m_quePendingMsgs.push_back( pMsg );
                while( ome.m_quePendingMsgs.size() > 0 &&
                    ome.m_queWaitingIrps.size() > 0 ) 
                {
                    pMsg = ome.m_quePendingMsgs.front();
                    ome.m_quePendingMsgs.pop_front();

                    IrpPtr pIrp =
                        ome.m_queWaitingIrps.front();
                    ome.m_queWaitingIrps.pop_front();
                    oPortLock.Unlock();

                    CStdRMutex oIrpLock( pIrp->GetLock() );
                    oPortLock.Lock();
                    ret = pIrp->CanContinue( IRP_STATE_READY );
                    if( ERROR( ret ) )
                        continue;

                    IrpCtxPtr& pCtx = pIrp->GetCurCtx();
                    if( pIrp->IoDirection() == IRP_DIR_IN
                        && pIrp->MinorCmd() == IRP_MN_IOCTL
                        && pIrp->MajorCmd() == IRP_MJ_FUNC )
                    {
                        BufPtr bufPtr( true );
                        *bufPtr = pMsg;
                        ret = 0;
                        pCtx->SetRespData( bufPtr );
                        pCtx->SetStatus( ret );
                        vecIrpToComplete.push_back( pIrp );
                    }
                    else
                    {
                        //wierd, what is wrong
                        //with this irp
                        pCtx->SetStatus( -EINVAL );
                        vecIrpsError.push_back( pIrp );
                    }
                }

            }while( 0 );
        }

        if( bDone )
            break;

        ++itr;
    }
    oPortLock.Unlock();

    CIoManager* pMgr = GetIoMgr();
    if( vecIrpToComplete.size() > 0 )
    {
        for( auto& elem : vecIrpToComplete )
        {
            ret = pMgr->CompleteIrp( elem );
        }
    }
    else if( !bQueued && SUCCEEDED( ret ) )
    {
        ret = -ENOENT;
    }

    for( auto& pIrpErr : vecIrpsError )
    {
        pMgr->CancelIrp(
            pIrpErr, true, -EINVAL );
    }

    return ret;
}

CRpcPdoPort::CRpcPdoPort( const IConfigDb* pCfg )
    : super( pCfg )
{
    do{
        gint32 ret = 0;

        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        ObjPtr pObj;
        CCfgOpener oCfg( pCfg );

        ret = oCfg.GetObjPtr(
            propBusPortPtr, pObj );

        if( ERROR( ret ) )
            break;

        // NOTE: no reference count on bus
        // port, because this port's life
        // cycle is contained within the bus
        // port's
        m_pBusPort = pObj;

        // we don't want the bus port
        // staying in the config db any
        // more
        ret = m_pCfgDb->RemoveProperty(
            propBusPortPtr );

    }while( 0 );
}

gint32 CRpcBasePort::ScheduleModOnOfflineTask(
    const std::string strModName, guint32 dwFlags )
{ return -ENOTSUP; }

gint32 CRpcBasePort::BuildSendDataReq(
    IConfigDb* pCfg, DMsgPtr& pMsg )
{
    gint32 ret = 0;

    if( pCfg == nullptr )
        return -EINVAL;

    bool bNonFd = false;
    do{
        // new req message
        ret = pMsg.NewObj();
        if( ERROR( ret ) )
            break;

        CReqOpener oParams( pCfg );

        guint32 dwSize;
        ret = oParams.Pop( dwSize );
        if( ERROR( ret ) )
            break;
        
        guint32 dwOffset;
        ret = oParams.Pop( dwOffset );
        if( ERROR( ret ) )
            break;

        gint32 iFd;
        ret = oParams.Pop( iFd );
        if( ERROR( ret ) )
            break;

        ObjPtr pDataDesc; 
        ret = oParams.Pop( pDataDesc );
        if( ERROR( ret ) )
            break;

        guint64 qwTaskId = 0;
        ret = oParams.GetQwordProp(
            propTaskId, qwTaskId );
        if( ERROR( ret ) )
            break;

        IConfigDb* pDataCfg = pDataDesc;
        if( pDataCfg == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        if( oParams.exist( propNonFd ) )
            bNonFd = true;

        string strPath;
        ret = oParams.GetObjPath( strPath );
        if( SUCCEEDED( ret ) )
            pMsg.SetPath( strPath );

        string strIfName;
        ret = oParams.GetIfName( strIfName );

        if( ERROR( ret ) )
            break;
        pMsg.SetInterface( strIfName );

        string strDest;
        ret = oParams.GetDestination( strDest );
        if( ERROR( ret ) )
            break;
        pMsg.SetDestination( strDest );

        string strSender;
        ret = oParams.GetSender( strSender );
        if( ERROR( ret ) )
        {
            string strModName =
                GetIoMgr()->GetModName();

            strSender =
                DBUS_DESTINATION( strModName );
        }

        pMsg.SetSender( strSender );

        string strMethod;
        ret = oParams.GetMethodName( strMethod );
        if( ERROR( ret ) )
            break;
        pMsg.SetMember( strMethod );

        BufPtr pCfgBuf( true );
        CfgPtr pCfg1 = pDataDesc;
        ret = pCfg1->Serialize( *pCfgBuf );

        if( ERROR( ret ) )
            break;

        gint32 iFdType = DBUS_TYPE_UNIX_FD;
        if( iFd < 0 || bNonFd )
            iFdType = DBUS_TYPE_UINT32;

        const char* pData = pCfgBuf->ptr();
        if( !dbus_message_append_args( pMsg,
            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
            &pData, pCfgBuf->size(),
            iFdType, &iFd,
            DBUS_TYPE_UINT32, &dwOffset,
            DBUS_TYPE_UINT32, &dwSize,
            DBUS_TYPE_UINT64, &qwTaskId,
            DBUS_TYPE_INVALID ) )
        {
            ret = ERROR_FAIL;
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcBasePort::BuildSendDataMsg(
    IRP* pIrp, DMsgPtr& pMsg )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    if( !pMsg.IsEmpty() )
        pMsg.Clear();

    gint32 ret = 0;
    IrpCtxPtr& pCtx = pIrp->GetCurCtx();

    do{
        // there is a CfgPtr in the buffer
        ObjPtr& pObj = *pCtx->m_pReqData; 
        CfgPtr pCfg = pObj;
        if( pCfg.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        CReqOpener oParams( ( IConfigDb* )pCfg );
        guint32 dwCallFlags = 0;

        ret = oParams.GetCallFlags( dwCallFlags );
        if( ERROR( ret ) )
            break;

        guint32 dwMsgType =
            dwCallFlags & CF_MESSAGE_TYPE_MASK;

        if( dwMsgType
            != DBUS_MESSAGE_TYPE_METHOD_CALL )
        {
            ret = -ENOTSUP;
            break;
        }

        ret = BuildSendDataReq( pCfg, pMsg );

    }while( 0 );

    return ret;
}

gint32 CRpcPdoPort::HandleSendData(
    IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;

    do{
         
        DMsgPtr pMsg;

        ret = BuildSendDataMsg( pIrp, pMsg );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        BufPtr pBuf( true );
        *pBuf = pMsg;

        // NOTE: We have overwritten the incoming
        // parameter with a new buffer ptr
        pCtx->SetReqData( pBuf );

        ret = HandleSendReq( pIrp );
        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) )
            break;

        if( SUCCEEDED( ret ) )
        {
            // cannot succeed at this moment
            ret = ERROR_FAIL;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcPdoPort::SendDBusMsg(
        DBusMessage* pMsg, guint32* pdwSerial )
{
    gint32 ret = 0;

    if( pMsg == nullptr )
        return -EINVAL;

    if( m_pBusPort->GetClsid()
        == clsid( CDBusBusPort ) )
    {
        CDBusBusPort* pBus = static_cast
        < CDBusBusPort* >( m_pBusPort );

        ret = pBus->SendDBusMsg( pMsg, pdwSerial );
    }
    else
    {
        ret = -EINVAL;
    }
    return ret;
}

gint32 CRpcPdoPort::HandleSendReq( IRP* pIrp )
{

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        // client side I/O
        guint32 dwSerial = 0;
        guint32 dwIoDir = pCtx->GetIoDirection();

        ret = -EINVAL;

        if( dwIoDir == IRP_DIR_INOUT ||
            dwIoDir == IRP_DIR_OUT )
        {
            DMsgPtr pMsg = *pCtx->m_pReqData;

            ret = IsIfSvrOnline( pMsg );
            if( ret == ENOTCONN )
            {
                // not connected yet, no need to
                // send the request
                ret = -ret;
                break;
            }

            if( ERROR( ret ) )
                break;

            CDBusBusPort *pBusPort = static_cast
                < CDBusBusPort* >( m_pBusPort );

            dwSerial =
                pBusPort->LabelMessage( pMsg );

            if( dwIoDir == IRP_DIR_INOUT )
            {
                // NOTE: there are slim chances,
                // that the response message
                // arrives before the irp enters
                // m_mapSerial2Resp after sending,
                // so better add the irp to
                // m_mapSerial2Resp ahead of
                // sending.
                CStdRMutex oPortLock( GetLock() );
                m_mapSerial2Resp[ dwSerial ] =
                    IrpPtr( pIrp );
            }
            else
            {
                pMsg.SetNoReply( true );
            }

            ret = SendDBusMsg( pMsg, nullptr );
            if( SUCCEEDED( ret ) &&
                dwIoDir == IRP_DIR_INOUT )
                ret = STATUS_PENDING;
        }

        if( ERROR( ret ) &&
            dwIoDir == IRP_DIR_INOUT &&
            dwSerial != 0 )
        {
            CStdRMutex oPortLock( GetLock() );
            m_mapSerial2Resp.erase( dwSerial );
        }

    }while( 0 );

    return ret;
}

gint32 CRpcPdoPort::HandleSendEvent( IRP* pIrp )
{
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        // client side I/O
        guint32 dwSerial = 0;
        guint32 dwIoDir = pCtx->GetIoDirection();

        ret = -EINVAL;

        if( dwIoDir == IRP_DIR_OUT )
        {
            DMsgPtr pMsg = *pCtx->m_pReqData;
            if( pMsg.GetType() !=
                DBUS_MESSAGE_TYPE_SIGNAL )
            {
                ret = -EINVAL;
                break;
            }

            ret = IsIfSvrOnline( pMsg );
            if( ret == ENOTCONN )
            {
                // not connected yet, no need to
                // send the request
                ret = -ret;
                break;
            }

            if( ERROR( ret ) )
                break;

            CDBusBusPort *pBusPort = static_cast
                < CDBusBusPort* >( m_pBusPort );

            dwSerial =
                pBusPort->LabelMessage( pMsg );

            pMsg.SetNoReply( true );

            ret = SendDBusMsg( pMsg, nullptr );
        }
        else
        {
            ret = -EINVAL;
        }

    }while( 0 );

    return ret;
}

gint32 CRpcPdoPort::SetupDBusSetting(
    IMessageMatch* pMsgMatch )
{
    if( pMsgMatch == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    CDBusError dbusError;

    do{

        EnumMatchType iMatchType =
            pMsgMatch->GetType();

        // test if the local server is ready
        if( iMatchType == matchClient )
        {

            string strDest =
                pMsgMatch->GetDest();

            if( strDest.empty() )
            {
                ret = -EINVAL;
                break;
            }

            CDBusBusPort *pBusPort = static_cast
                < CDBusBusPort* >( m_pBusPort );

            ret = pBusPort->IsDBusSvrOnline( strDest );
            if( ERROR( ret ) )
            {
                // server is not online
                break;
            }
            // add the match rule for the signal
            // messages
            string strRules = pMsgMatch->ToDBusRules(
                DBUS_MESSAGE_TYPE_SIGNAL );

            if( SUCCEEDED( ret ) || ret == ENOTCONN )
            {
                gint32 iRet = 
                    pBusPort->AddRules( strRules );

                if( ERROR( iRet ) )
                    ret = iRet;
            }
        }

    }while( 0 );

    return ret;
}

/**
* @name CRpcPdoPort::ClearDBusSetting
* @{ */
/** Clear the footprint on dbus. 
 * this method only clear signal setting
 * @} */
gint32 CRpcPdoPort::ClearDBusSetting(
    IMessageMatch* pMsgMatch )
{
    gint32 ret = 0;

    if( pMsgMatch == nullptr )
        return -EINVAL;

    do{
        string strRules = pMsgMatch->
            ToDBusRules( DBUS_MESSAGE_TYPE_SIGNAL );

        CDBusBusPort *pBusPort = static_cast
            < CDBusBusPort* >( m_pBusPort );

        ret = pBusPort->RemoveRules( strRules );

    }while( 0 );

    return ret;
}

// IEventSink method
gint32 CRpcPdoPort::OnEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData  )
{
    gint32 ret = 0;

    switch( iEvent )
    {
    case cmdDispatchData:
        {
            if( pData == nullptr )
                break;

            CBuffer* pBuf = reinterpret_cast
                < CBuffer* >( pData );

            ret = this->DispatchData( pBuf );
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

gint32 CRpcPdoPort::SubmitIoctlCmd(
    IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
    {
        return -EINVAL;
    }

    gint32 ret = 0;

    // let's process the func irps
    IrpCtxPtr pCtx = pIrp->GetCurCtx();

    do{

        if( pIrp->MajorCmd() != IRP_MJ_FUNC
            || pIrp->MinorCmd() != IRP_MN_IOCTL )
        {
            ret = -EINVAL;
            break;
        }

        switch( pIrp->CtrlCode() )
        {
        case CTRLCODE_SET_REQUE_SIZE:
            {
                // server side I/O
                ret = HandleSetReqQueSize( pIrp );
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

gint32 CRpcPdoPort::HandleSetReqQueSize( IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    do{
        IrpCtxPtr& pCtx = pIrp->GetCurCtx();
        CStdRMutex oPortLock( GetLock() );
        IConfigDb* pCfg =
            ( ObjPtr& )( *pCtx->m_pReqData );

        if( pCfg == nullptr ) 
        {
            ret = -EFAULT;
            break;
        }
        CParamList oCfg( pCfg );
        guint32 dwQueSize = 0;;
        ret = oCfg.GetIntProp(
            propQueSize, dwQueSize );

        if( ERROR( ret ) )
            break;

        IMessageMatch* pMatch = nullptr;
        ret = oCfg.GetPointer(
            propMatchPtr, pMatch );

        if( ERROR( ret ) )
            break;

        MatchMap* pMatchMap = nullptr;
        // check if the match exist in the match
        // map
        ret = GetMatchMap( pMatch, pMatchMap );
        if( ERROR( ret ) )
            break;

        CCfgOpenerObj oMatch( pMatch );
        oMatch.SetIntProp(
            propQueSize, dwQueSize );

    }while( 0 );

    return ret;
}

}
