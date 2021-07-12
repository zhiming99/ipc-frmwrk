/*
 * =====================================================================================
 *
 *       Filename:  iomgr.cpp
 *
 *    Description:  implementation of IO manager
 *
 *        Version:  1.0
 *        Created:  06/27/2016 08:55:02 PM
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

#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <string>
#include "defines.h"
#include "registry.h"
#include "frmwrk.h"
#include "tasklets.h"
#include "jsondef.h"
#include <algorithm>
#include "portex.h"

#include "ifhelper.h"

namespace rpcf
{

using namespace std;

gint32 CIoManager::SubmitIrpInternal(
    IPort* pPort,
    IRP* pIrp1,
    bool bTopmost )
{
    gint32 ret = 0;

    if( pIrp1 == nullptr || pPort == nullptr )
        return -EINVAL;

    IrpPtr pIrp( pIrp1 );

    do{
        pIrp->m_dwTid = GetTid();
        pIrp->SetSubmited();

        if( bTopmost )
            pPort = pPort->GetTopmostPort();

        pair< PortPtr, IrpCtxPtr >& oStackEntry =
            pIrp->m_vecCtxStack.back();

        // set the port ptr if not already set
        // on the port object stack
        oStackEntry.first = pPort;

        IrpCtxPtr& pCtx = oStackEntry.second;
        BufPtr pExtBuf;
        pCtx->GetExtBuf( pExtBuf );
        if( pExtBuf.IsEmpty() || pExtBuf->empty() )
            pPort->AllocIrpCtxExt( pCtx, pIrp1 );

        CStdRMutex a( pIrp->GetLock() );
        ret = pPort->SubmitIrp( pIrp );
        if( ret != STATUS_PENDING )
        {
            // make this irp unaccessable
            pIrp->SetState(
                IRP_STATE_READY, IRP_STATE_COMPLETED );
            pIrp->SetStatus( ret );
        }
        else
        {
            // we mark the pending only
            // when the irp is about to
            // leave the stack
            pIrp->MarkPending();
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::SubmitIrp(
    HANDLE hPort, IRP* pIrp, bool bCheckExist )
{
    gint32 ret = 0;

    if( pIrp == nullptr ||
        hPort == INVALID_HANDLE )
        return -EINVAL;

    do{
        PortPtr pPort;
        if( bCheckExist )
        {
            ret = GetPortPtr( hPort, pPort );
            if( ERROR( ret ) )
                break;
        }
        else
        {
            pPort = HandleToPort( hPort );
        }

        ret = SubmitIrpInternal( pPort, pIrp );

    }while( 0 );

    return ret;
}

gint32 CIoManager::CompleteAssocIrp(
    IRP* pSlave )
{
    gint32 ret = 0;

    if( pSlave == nullptr )
        return -EINVAL;

    // let's check if it has accociated irp we don't
    // check the slaves, because the slaves must
    // complete before the master completes
    do
    {
        IrpPtr pMaster = pSlave->GetMasterIrp();
        if( pMaster.IsEmpty()  )
        {
            ret = -EFAULT;
            break;
        }
        // well, we need to inform the marster irp
        // we are done

        IPort* pPort = pMaster->GetTopPort();
        if( pPort != nullptr )
        {
            ret = pPort->CompleteAssocIrp( pMaster, pSlave );
        }

    }while( 0 );

    // we are done with the master irp
    pSlave->SetMasterIrp( nullptr );

    return ret;
}

gint32 CIoManager::CancelAssocIrp( IRP* pSlave )
{
    gint32 ret = 0;

    if( pSlave == nullptr )
        return -EINVAL;

    // first let's check to cancel all the slave
    // irps

    // let's check if it has accociated irp
    // we don't check the slaves, because the
    // slaves must complete before the master
    // completes
    do
    {
        IRP* pMaster = pSlave->GetMasterIrp();
        if( pMaster == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        // well, we need to inform the marster
        // irp we are done
        IPort* pPort = pMaster->GetTopPort();
        if( pPort == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        // FIXME: is it right to complete the 
        // master here?
        ret = pPort->CompleteAssocIrp( pMaster, pSlave );

        if( ERROR( ret ) || ret == STATUS_PENDING ) 
        {
            // we don't care the return value
        }

        // we are done with the master irp
        pSlave->SetMasterIrp( nullptr );

        // recover the original value
        ret = pSlave->GetStatus();

    }while( 0 );

    return ret;
}

gint32 CIoManager::PostCompleteIrp( IRP* pIrp, bool bCancel )
{

    gint32 ret = 0;

    // wake up the user land
    if( pIrp->IsSyncCall() && pIrp->IsPending() )
    {
        // wake up the caller and we are done for
        // synchronous call, it implies that the
        // caller must hold a reference count to
        // the irp till the call finished
        Sem_Post( &pIrp->m_semWait );
    }
    else if( pIrp->IsSyncCall() && !pIrp->IsPending() )
    {
        // this is an immediate return how did we
        // land here
        ret = ERROR_STATE;
    }
    else if( !pIrp->IsSyncCall() && pIrp->IsPending() )
    {
        // asnynchronous call.
        // we don't know what could happen in user
        // callback, and let's schedule an
        // workitem to notify the caller
        if( unlikely( pIrp->CompleteInPlace() && 
            !pIrp->m_pCallback.IsEmpty() ) )
        {
            // complete the irp on the same thread
            // CompleteIrp is called. a
            // performance boost
            ObjPtr irpPtr( pIrp );
            CCfgOpener oCfg;
            ret = oCfg.SetObjPtr( propIrpPtr, irpPtr );
            if( SUCCEEDED( ret ) )
            {
                TaskletPtr pTask;
                ret = pTask.NewObj(
                    clsid( CIoMgrIrpCompleteTask ),
                    oCfg.GetCfg() );
                if( SUCCEEDED( ret ) )
                {
                    ( *pTask )( eventZero );
                }
            }
        }
        else
        {
            // check to see if we have dedicated irp
            // completion thread
            ThreadPtr thrdPtr;
            IThread* pThrd = nullptr;
            ret = pIrp->GetIrpThread( thrdPtr );

            if( SUCCEEDED( ret ) )
                pThrd = thrdPtr;

            if( nullptr == pThrd )
            {
                // in case some interface use workitem
                // for irp completion
                if( !pIrp->m_pCallback.IsEmpty() )
                {
                    ObjPtr irpPtr( pIrp );
                    CCfgOpener oCfg;
                    ret = oCfg.SetObjPtr( propIrpPtr, irpPtr );
                    if( SUCCEEDED( ret ) )
                    {
                        ret = ScheduleTask(
                            clsid( CIoMgrIrpCompleteTask ),
                            oCfg.GetCfg() );
                    }
                }
            }
            else
            {
                // this is usually for irps from a
                // server interface
                if( pThrd && !pIrp->m_pCallback.IsEmpty() )
                {
                    CIrpCompThread* pIrpThrd = thrdPtr;
                    if( pIrpThrd )
                    {
                        pIrpThrd->AddIrp( pIrp );
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                }
            }
        }
    }
    else // ( !pIrp->IsSyncCall() && !pIrp->IsPending() )
    {
        // what's going on
    }

    // remove the mutal reference
    pIrp->ClearIrpThread();
    return ret;
}

gint32 CIoManager::CompleteIrp( IRP* pIrpComp )
{

    if( pIrpComp == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        IrpPtr pIrp( pIrpComp );

        CStdRMutex a( pIrp->GetLock() );

        if( !pIrp->CanComplete() )
        {
            // no need to go further because the SubmitIrp
            // is not returned yet
            ret = ERROR_CANNOT_COMP;
            break;
        }

        // lock this irp
        if( pIrp->GetState() != IRP_STATE_READY )
        {
            ret = ERROR_STATE;
            break;
        }
        ret = pIrp->GetTopStack()->GetStatus();
        if( ERROR( ret ) )
        {
            // possibly the irp timeout
            if( pIrp->GetSlaveCount() )
            {
                // cancel the slave IRPs if any
                a.Unlock();
                CancelSlaves( pIrp, true, ret );
                a.Lock();
            }
        }

        ret = pIrp->CanContinue( IRP_STATE_COMPLETING );
        if( ERROR( ret ) )
            break;

        // walk through the irp context stack
        if( !pIrp->IsCbOnly() )
        {
            gint32 iSize = ( gint32 )pIrp->GetStackSize();
            if( iSize > 0 )
            {
                vector< pair< PortPtr, IrpCtxPtr > >&
                    vecStack = pIrp->m_vecCtxStack;

               for( gint32 i = iSize - 1; i >= 0; --i )
               {
                    pair< PortPtr, IrpCtxPtr >& oElem = vecStack[ i ];
                    if( !oElem.first.IsEmpty() )
                    {
                        pIrp->SetCurPos( i );
                        ret = oElem.first->CompleteIrp( pIrp );
                        if( ret == STATUS_PENDING )
                        {
                            // FIXME: Is it the responsibility of the
                            // guy who returns STATUS_PENDING to set the
                            // irp to the proper state
                            // 
                            pIrp->SetState(
                                IRP_STATE_COMPLETING, IRP_STATE_READY );

                            // NOTE: the user will have to reset
                            // the irp state to a proper value
                            //
                            // we need to stop the completion and
                            // return immediately
                            return ret;
                        }
                    }
                }

                // we should not pop out the first io stack
                // because it is owned by the user land
                IrpCtxPtr& pCtxPtr = vecStack.begin()->second;
                pIrp->SetStatus( pCtxPtr->GetStatus() );
            }
            else
            {
                pIrp->SetStatus( -EINVAL );
            }
        }
        else
        {
            IrpCtxPtr& pCtxPtr =
                pIrp->m_vecCtxStack.begin()->second;
            pIrp->SetStatus( pCtxPtr->GetStatus() );
        }

        // remove the timer if any
        if( pIrp->HasTimer() )
        {
            GetUtils().GetTimerSvc().RemoveTimer( pIrp->m_iTimerId );
        }

        pIrp->SetState(
            IRP_STATE_COMPLETING, IRP_STATE_COMPLETED );

        a.Unlock();
        if( pIrp->GetMasterIrp() != nullptr )
        {
            this->CompleteAssocIrp( pIrp );
            // we don't care the return value from
            // CompleteAssocIrp
        }

        ret = PostCompleteIrp( pIrp );
        // till now, the irp will go out of
        // the port stack, and no access any more
    }while( 0 );

    return ret;
}

gint32 CIoMgrIrpCompleteTask::operator()(
    guint32 dwContext )
{

    gint32 ret = 0;

    do{
        CCfgOpener a( ( IConfigDb* )GetConfig() );
        ObjPtr objPtr;

        // get the pointer to the bus port
        ret = a.GetObjPtr( propIrpPtr, objPtr );
        if( ERROR( ret ) )
            break;

        a.RemoveProperty( propIrpPtr );

        IrpPtr pIrp = objPtr;
        if( pIrp.IsEmpty() )
        {
            ret = -EINVAL;
            break;
        }

        if( !pIrp->m_pCallback.IsEmpty() )
        {
            LONGWORD dwParam1 =
                ( LONGWORD )( ( IRP* )pIrp );

            LONGWORD dwParam2 =
                pIrp->m_dwContext;

            pIrp->m_pCallback->OnEvent(
                eventIrpComp,
                dwParam1,
                dwParam2,
                nullptr );
        }

    }while( 0 );
    return ret;
}

gint32 CIoManager::CancelSlaves(
    IRP* pIrp, bool bForce, gint32 iRet )
{
    gint32 ret = 0;
    do{
        // lock this irp
        CStdRMutex a( pIrp->GetLock() );
        ret = pIrp->CanContinue( IRP_STATE_CANCEL_SLAVE );
        if( ERROR( ret ) )
        {
            // already cancelled or completed
            break;
        }

        if( pIrp->GetSlaveCount() == 0 )
        {
            pIrp->SetState(
                IRP_STATE_CANCEL_SLAVE, IRP_STATE_READY );

            break;
        }

        if( pIrp->m_mapSlaveIrps.size() == 0 )
        {
            // no active slave irps in the map
            // lift the bar to complete the master irp
            pIrp->SetMinSlaves( 0 );
            pIrp->SetState(
                IRP_STATE_CANCEL_SLAVE, IRP_STATE_READY );
            break;
        }

        IRP* pSlave = pIrp->m_mapSlaveIrps.begin()->first;

        // remove the slave here. if we do it after the
        // CancelIrp, there could be chance the master
        // be canceled by other guys.
        pIrp->RemoveSlaveIrp( pSlave );

        // cancel all the slaves first
        if( pIrp->GetSlaveCount() > 0 )
        {
            pIrp->SetState(
                IRP_STATE_CANCEL_SLAVE, IRP_STATE_READY );

            // let's cancel slave irps
            if( pSlave != nullptr )
            {
                // to avoid nested locking, we release the
                // marster irp's lock
                a.Unlock();
                CancelIrp( pSlave, bForce, iRet );
            }
            continue;
        }
        else if( pIrp->GetSlaveCount() == 0 )
        {
            // add a dummy irp to prevent the
            // completion of master irp in
            // CancelAssocIrp, because we are going to
            // cancel it. CancelAssocIrp happens when
            // the slave irp is being canceled from
            // somewhere else
            if( pSlave != nullptr )
            {
                // add a dummy slave to prevent the
                // CompleteIrp from being called in
                // CPort::CompleteAssocIrp
                pIrp->AddSlaveIrp( ( IRP* )nullptr );

                // reset the state to allow others to touch this
                // irp, especially when CompleteAssocIrp from
                // slave irp's CancelIrp
                pIrp->SetState(
                    IRP_STATE_CANCEL_SLAVE, IRP_STATE_READY );

                // let's cancel slave irps
                a.Unlock();
                CancelIrp( pSlave, bForce, iRet );
                continue;
            }
            else
            {
                // should be what we have added on purpose
                pIrp->RemoveSlaveIrp( pSlave );
            }
        }

        pIrp->SetState(
            IRP_STATE_CANCEL_SLAVE, IRP_STATE_READY );

        break;

    }while( 1 );

    return 0;
}

gint32 CIoManager::CancelIrp(
    IRP* pIrp, bool bForce, gint32 iRet )
{
    if( pIrp == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    // here we take the ownership of pIrp
    // it will be released on return
    //
    // We don't need to increment the ref count
    // here because the reference count was
    // incremented by an earilier call to 
    // CIoManager::SubmitIrp,
 
    IrpPtr irpPtr( pIrp );

    do{
        // nasty logics here
        CancelSlaves( pIrp, bForce, iRet );
        
        CStdRMutex a( pIrp->GetLock() );
        if( !pIrp->CanComplete() )
            break;

        ret = pIrp->CanContinue( IRP_STATE_CANCELLING );
        if( ERROR( ret ) )
        {
            // already cancelled or completed
           break; 
        }

        // walk through the irp context stack
        if( !pIrp->IsCbOnly() )
        {
            if( pIrp->GetStackSize() > 0 )
            {
                gint32 iSize = ( gint32 )pIrp->GetStackSize();
                vector< pair< PortPtr, IrpCtxPtr > >&
                    vecStack = pIrp->m_vecCtxStack;

                bool bFirst = true;
                for( gint32 i = iSize - 1; i >= 0; --i )
                {
                    pair< PortPtr, IrpCtxPtr >& oElem = vecStack[ i ];
                    if( !oElem.first.IsEmpty() )
                    {
                        // set the status for the top stack
                        if( bFirst )
                        {
                            oElem.second->SetStatus(
                                iRet == 0 ? -ECANCELED : iRet );
                            bFirst = false;
                        }

                        pIrp->SetCurPos( i );
                        oElem.first->CancelIrp( pIrp, bForce );
                    }

                    // the context stack will be popped by the
                    // Port's CancelIrp
                    //
                    // Note that the top most IRP_CONTEXT will
                    // not be popped by the port at the bottom of
                    // the port stack
                    //
                    // the rest non-bottom ports will
                    // face the IRP_CONTEXT it prepared for
                    // the its lower port

                }
            }
        }
        pIrp->SetStatus( iRet );

        // we leave the last IRP_CONTEXT for the io manager
        // or the attached interface, at this point
        // the no port reference is held by this IRP
        //
        // till now, the irp will go out of
        // the port stack, and no access any more

        if( pIrp->HasTimer() )
        {
            GetUtils().GetTimerSvc().RemoveTimer( pIrp->m_iTimerId );
        }

        pIrp->SetState(
            IRP_STATE_CANCELLING, IRP_STATE_CANCELLED );

        a.Unlock();
        if( pIrp->GetMasterIrp() != nullptr )
        {
            ret = CancelAssocIrp( pIrp );
        }

        ret = PostCompleteIrp( pIrp, true );

    }while( 0 );

    return ret;
}


CUtilities&
CIoManager::GetUtils() const
{
    return *m_pUtils;
}

CPnpManager&
CIoManager::GetPnpMgr() const
{
    return *m_pPnpMgr;
}

CDriverManager&
CIoManager::GetDrvMgr() const
{
    return *m_pDrvMgr;
}

CRegistry&
CIoManager::GetRegistry() const
{
    return *m_pReg;
}

CThreadPool&
CIoManager::GetIrpThreadPool() const
{
    return *m_pIrpThrdPool;
}

CThreadPool&
CIoManager::GetTaskThreadPool() const
{
    return *m_pTaskThrdPool;
}

ObjPtr&
CIoManager::GetSyncIf() const
{
    return const_cast< ObjPtr& >( m_pSyncIf );
}

// registry helpers
gint32 CIoManager::RegisterObject(
    const std::string& strPath, const ObjPtr& pObj )
{
    gint32 ret = 0;
    do{
        CRegistry& oReg = GetRegistry();
        CStdRMutex a( oReg.GetLock() );
        ret = oReg.ChangeDir( strPath );
        if( ret == -ENOENT )
        {
            ret = oReg.MakeDir( strPath );
            if( SUCCEEDED( ret ) )
                ret = oReg.ChangeDir( strPath );
        }

        if( ERROR( ret ) )
            break;

        ret = oReg.SetObject( propObjPtr, pObj );

    }while( 0 );
    return ret;
}

gint32 CIoManager::UnregisterObject(
    const std::string& strPath )
{
    gint32 ret = 0;
    do{
        CRegistry& oReg = GetRegistry();
        CStdRMutex a( oReg.GetLock() );

        ret = oReg.ChangeDir( strPath );
        if( ERROR( ret ) )
            break;

        ret = oReg.RemoveProperty( propObjPtr );
    }while( 0 );

    return ret;
}

gint32 CIoManager::SetObject(
    const std::string& strPath, ObjPtr& pObj )
{
    gint32 ret = 0;
    do{
        CRegistry& oReg = GetRegistry();
        CStdRMutex a( oReg.GetLock() );
        ret = oReg.ChangeDir( strPath );
        if( ret == -ENOENT )
        {
            ret = oReg.MakeDir( strPath );
            if( SUCCEEDED( ret ) )
                ret = oReg.ChangeDir( strPath );
        }

        if( ERROR( ret ) )
            break;

        ret = oReg.SetObject( propObjPtr, pObj );

    }while( 0 );

    return ret;
}

gint32 CIoManager::GetObject(
    const std::string& strPath, ObjPtr& pObj )
{
    gint32 ret = 0;
    do{
        CRegistry& oReg = GetRegistry();
        CStdRMutex a( oReg.GetLock() );
        ret = oReg.ChangeDir( strPath );
        if( ERROR( ret ) )
            break;

        ret = oReg.GetObject( propObjPtr, pObj );

    }while( 0 );

    return ret;
}

// with the name strPort or config parameters from pcfg
gint32 CIoManager::OpenPortByPath(
    const std::string& strPath,
    HANDLE& hPort )
{
    gint32 ret = 0;

    ObjPtr objPtr;
    do{
        ret = this->GetObject( strPath, objPtr );
        if( ERROR( ret ) )
            break;

        CObjBase* pObj = objPtr;

        IPort* pPort = ( IPort* ) pObj;
        if( pPort == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        // further check to see if the port
        // registered with the handle map
        ret = PortExist( pPort );
        if( ERROR( ret ) )
        {
            // well, the port might not be
            // ready for user access yet
            break;
        }

        hPort = PortToHandle( pPort );

    }while( 0 );

    return ret;
}

gint32 CIoManager::CloseChildPort(
    IPort* pBusPort,
    IPort* pPort,
    IEventSink* pCallback )
{
    if( pPort == nullptr ||
        pBusPort == nullptr ||
        pCallback == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        IrpPtr pIrp( true );
        pIrp->AllocNextStack( nullptr );
        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack(); 

        // stop the child port
        pIrpCtx->SetMajorCmd( IRP_MJ_PNP );
        pIrpCtx->SetMinorCmd( IRP_MN_PNP_STOP_CHILD );
        pIrpCtx->SetIoDirection( IRP_DIR_OUT );
        pBusPort->AllocIrpCtxExt( pIrpCtx );

        BufPtr pBuf( true );
        *pBuf = ObjPtr( pPort );
        pIrpCtx->SetReqData( pBuf );

        pIrp->SetCallback( pCallback, 0 );
        pIrp->SetTimer(
            PORT_START_TIMEOUT_SEC, this );

        HANDLE hBusPort =
            PortToHandle( pBusPort );

        pIrp->SetIrpThread( this );

        ret = SubmitIrp( hBusPort, pIrp, false );
        if( ret != STATUS_PENDING )
        {
            pIrp->RemoveTimer();
            pIrp->RemoveCallback();
            pIrp->ClearIrpThread();
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::ClosePort(
    HANDLE hPort,
    IEventSink* pIf, // the registered interface
    IEventSink* pCallback ) // the callback for complete stop
{
    // Note: we need to make sure all the irps are
    // completed or canceled and the interface
    // unregistered before calling this method
    // otherwise unexpected things would happen
    gint32 ret = 0;
    do{
        if( hPort == INVALID_HANDLE )
        {
            ret = -EINVAL;
            break;
        }
        bool bNoRef = false;
        PortPtr ptrPort;
        ret = GetPortPtr( hPort, ptrPort );
        if( ERROR( ret ) )
            break;

        CPort* pPort = static_cast< CPort* >(
            HandleToPort( hPort ) );

        if( pIf != nullptr )
            RemoveFromHandleMap( pPort, pIf );
        ret = IsPortNoRef( pPort, bNoRef );
        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        if( !bNoRef )
            break;

        if( pPort->Unloadable() && !IsStopping() )
        {
            TaskletPtr pDummy;
            if( pCallback == nullptr )
            {
                ret = pDummy.NewObj(
                    clsid( CIfDummyTask ) );
                if( SUCCEEDED( ret ) )
                    pCallback = pDummy;
            }

            IPort* pBusPort = nullptr;

            CCfgOpenerObj oPortCfg( pPort );
            ret = oPortCfg.GetPointer(
                propBusPortPtr, pBusPort );

            if( ERROR( ret ) )
            {
                ObjPtr pDriver;
                ret = oPortCfg.GetObjPtr(
                    propDrvPtr, pDriver );
                if( ERROR( ret ) )
                    break;

                CGenBusDriverEx* pBusDrv = pDriver;
                if( pBusDrv == nullptr )
                {
                    ret = -EFAULT;
                    break;
                }
                ret = pBusDrv->DestroyPortSynced(
                    pPort, pCallback );
            }
            else
            {
                ret = CloseChildPort( pBusPort,
                    pPort, pCallback );
            }
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::OpenPortByRef(
    HANDLE hPort,
    IEventSink* pEventSink )
{
    if( hPort == INVALID_HANDLE ||
        pEventSink == nullptr )
        return -EINVAL;

    PortPtr pPort;
    gint32 ret = GetPortPtr( hPort, pPort );
    if( ERROR( ret ) )
        return ret;

    return AddToHandleMap( pPort, pEventSink );
}

gint32 CIoManager::GetPortProp(
    HANDLE hPort,
    gint32 iProp,
    BufPtr& pBuf )
{
    if( hPort == INVALID_HANDLE )
        return -EINVAL;

    PortPtr pPort;
    gint32 ret = GetPortPtr( hPort, pPort );
    if( ERROR( ret ) )
        return ret;

    ret = pPort->GetProperty( iProp, *pBuf );

    return ret;
}

gint32 CIoManager::MakeBusRegPath(
    std::string& strPath,
    const std::string& strBusName )
{

    size_t pos;
    pos = strBusName.rfind( '.' );
    string strBusPort;
    if( pos == string::npos )
    {
        strBusPort = strBusName;
    }
    else if( pos + 1 < strBusName.size() )
    {
        strBusPort =
            strBusName.substr( pos + 1 );
    }
    else
    {
        return -EINVAL;
    }

    strPath = string( "/port/" )
        + GetModName()
        + "/bus/"
        + strBusPort;

    return 0;
}

// create a port in the hard way
gint32 CIoManager::OpenPortByCfg(
    const IConfigDb* pCfg, PortPtr& pPort )
{
    gint32 ret = 0;
    // we are sure the bus port will be loaded
    // earlier than the first call of OpenPortByCfg
    // our first try is to use bus name to create
    // the specified port
    do{
        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        try{
            string strBusName;
            CCfgOpener a( pCfg );
            ret =  a.GetStrProp( propBusName, strBusName );
            if( ERROR( ret ) )
                break;

            string strPath;
            ret = MakeBusRegPath( strPath, strBusName );
            if( ERROR( ret ) )
                break;

            // get the pointer to the bus port
            ObjPtr objPtr;
            ret = this->GetObject( strPath, objPtr );
            if( ret == -ENOENT )
            {
                // possibly the driver and port are not
                // loaded yet
                ret = -EAGAIN;
                break;
            }
            if( ERROR( ret ) )
                break;

            CGenericBusPort* pBus = objPtr;

            if( pBus == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            else
            {
                CStdRMutex oPortLock( pBus->GetLock() );

                guint32 dwState = pBus->GetPortState();
                if( dwState == PORT_STATE_STARTING )
                {
                    // try it later
                    ret = -EAGAIN;
                    break;
                }
                else if( dwState != PORT_STATE_READY )
                {
                    ret = ERROR_STATE;
                    break;
                }
            }
            // create the pdo port or open the
            // existing pdo port
            ret = pBus->OpenPdoPort( pCfg, pPort );
            if( ERROR( ret ) )
                break;

        }
        catch ( std::invalid_argument& e )
        {
            ret = -EINVAL;
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::OpenPort(
    const IConfigDb* pCfg,
    HANDLE& hPort )
{
    PortPtr pPort;
    gint32 ret = 0;

    do{
        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        /*if( RunningOnMainThread() )
        {
            // we cannot open port on the
            // main IO thread
            ret = ERROR_WRONG_THREAD;
            break;
        }*/

        // FIXME: what is the ref count of pPort
        ret = this->OpenPortByCfg( pCfg, pPort );

        if( SUCCEEDED( ret ) || ret == EEXIST )
        {
            if( ret == EEXIST && !pPort.IsEmpty() )
            {
                // make sure the OpenPortByCfg
                // won't return a port stopped or
                // stopping
                CPort* pNewPort =
                    static_cast< CPort* >( pPort );

                CStdRMutex oPortLock(
                    pNewPort->GetLock() );

                guint32 dwState =
                    pNewPort->GetPortState();

                if( dwState == PORT_STATE_STOPPING 
                    || dwState == PORT_STATE_STOPPED
                    || dwState == PORT_STATE_ATTACHED
                    || dwState == PORT_STATE_STARTING )
                {
                    ret = ERROR_STATE;
                    break;
                }
            }

            // the new port is created 
            ObjPtr pObj;
            CCfgOpener a( pCfg );
            ret = a.GetObjPtr( propIfPtr, pObj );
            if( ERROR( ret ) )
            {
                // it is required to bind event
                // sink to the handle
                break;
            }

            IEventSink* pEvent = pObj;
            if( pEvent == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            ret = AddToHandleMap( pPort, pEvent );
            hPort = PortToHandle( pPort );
            break;
        }
        else if( ret == STATUS_PENDING )
        {
            // the response will be returned via the
            // pCallback. The hPort is valid though,
            // but you cannot use it right now, because
            // it is not added to the handle map yet.
            // It can be used as a hint in the
            // callback
            //
            // for the port to start, OpenPort will
            // propbably, end up at this point
            break;
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::OnEvent( EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData)
{
    gint32 ret = 0;
    switch( iEvent )
    {
    case eventWorkitem:
        {
            switch( ( EnumEventId )dwParam1 )
            {
            case eventStart:
                {
                    CParamList oParams;

                    oParams.SetPointer(
                        propIoMgr, this );

                    oParams.SetBoolProp(
                        propNoPort, true );

                    ret = GetSyncIf().NewObj(
                        clsid( CSimpleSyncIf ),
                        oParams.GetCfg() );
                    if( ERROR( ret ) )
                        break;

                    CRpcServices* pIf = GetSyncIf();
                    pIf->Start();

                    // notified we can start.  the
                    // sem will be posted after
                    // the mainloop is started,
                    // which is important for
                    // receiving tcp socket events 
                    ret = GetPnpMgr().Start();
                    if( ERROR( ret ) )
                        break;

                    ret = GetDrvMgr().Start();

                    if( ERROR( ret ) )
                        GetPnpMgr().Stop();

                    m_iHcTimer = GetUtils().GetTimerSvc().
                        AddTimer( 30, this, eventHouseClean );

                    break;
                }
            default:
                break;
            }
        }
    case eventWorkitemCancel:
        {
            switch( ( EnumEventId )dwParam1 )
            {
            default:
                break;
            }
        }
    case eventTimeout:
        {
            switch( ( EnumEventId )dwParam1 )
            {
            case eventHouseClean:
                {
                    ClearStandAloneThreads();
                    m_iHcTimer = GetUtils().GetTimerSvc().
                        AddTimer( 30, this, eventHouseClean );
                    break;
                }
            default:
                break;
            }
        }
    default:
        break;
    }
    return ret;
}

CMainIoLoop* CIoManager::GetMainIoLoop() const
{
    return m_pLoop;
}

const string& CIoManager::GetModName() const
{
    return m_strModName;
}

gint32 CIoManager::AddToHandleMap(
    IPort* pPort, IEventSink* pEvent )
{
    if( pPort == nullptr )
        return -EINVAL;

    return m_oPortIfMap.AddToHandleMap(
        pPort, pEvent );
}

gint32 CIoManager::RemoveFromHandleMap(
    IPort* pPort, IEventSink* pEvent )
{
    return m_oPortIfMap.RemoveFromHandleMap(
        pPort, pEvent );
}

gint32 CIoManager::PortExist(
    IPort* pPort, vector< EventPtr >* pvecEvents )
{
    return m_oPortIfMap.PortExist( pPort, pvecEvents );
}

gint32 CIoManager::GetPortPtr(
    HANDLE hHandle, PortPtr& pPort ) const
{
    return m_oPortIfMap.GetPortPtr( hHandle, pPort );
}

gint32 CIoManager::IsPortNoRef(
    IPort* pPort, bool& bNoRef ) const
{
    return m_oPortIfMap.IsPortNoRef( pPort, bNoRef );
}

bool CIoManager::RunningOnMainThread() const
{

    CMainIoLoop* pMainLoop = GetMainIoLoop();
    if( pMainLoop == nullptr )
    {
        throw std::runtime_error(
            "Mainloop bad address" );
    }

    guint32 dwMainTid =
        pMainLoop->GetThreadId();

    return GetTid() == dwMainTid;
}

gint32 CIoManager::EnumDBusNames(
    vector<string>& vecNames )
{
    gint32 ret = 0;
    do{
        string strModeName = GetModName();

        // NOTE: c++11 needed for to_string

        string strPath = string( "/port/" )
            + strModeName
            + "/bus";

        CRegistry& oReg = GetRegistry();
        CStdRMutex oLock( oReg.GetLock() );
        ret = oReg.ChangeDir( strPath );
        if( ERROR( ret ) )
            break;

        vector< string > vecBusNames;
        ret = oReg.ListDir( vecBusNames );
        if( ERROR( ret ) )
            break;

        for( guint32 i = 0; i < vecBusNames.size(); i++ )
        {
            gint32 ret1 = strncmp(
                vecBusNames[ i ].c_str(),
                PORT_CLASS_LOCALDBUS,
                sizeof( PORT_CLASS_LOCALDBUS ) - 1 );

            if( ret1 == 0 )
            {
                vecNames.push_back( vecBusNames[ i ] );
            }
        }

        if( vecNames.empty() )
            ret = -ENOENT;

    }while( 0 );
    return ret;
}

gint32 CIoManager::Start()
{
    gint32 ret = 0;
    do{
        if( m_bInit )
        {
            // well, it is already initialized,
            // let's fail
            ret = ERROR_STATE;
            break;
        }

        DebugPrint( 0, "IoMgr is starting..." );

        m_bInit = true;
        m_bStop = false;

        GetTaskThreadPool().Start();
        GetIrpThreadPool().Start();

        // this task will be executed in the
        // mainloop
        CCfgOpener a;
        a.SetPointer( propIoMgr, this );

        ScheduleTaskMainLoop(
            clsid( CIoMgrPostStartTask ),
            a.GetCfg() );

        // let's enter the main loop. The whole
        // system begin to run.
        GetMainIoLoop()->Start();

        sem_wait( GetSyncSem() );

    }while( 0 );

    return ret;
}


gint32 CIoManager::ScheduleTaskMainLoop(
    EnumClsid iClsid, CfgPtr& pCfg )
{

    gint32 ret = 0;
    do{
        TaskletPtr pTask;
        ret = pTask.NewObj( iClsid, pCfg );
        if( ERROR( ret ) )
            break;
        RescheduleTaskMainLoop( pTask );

    }while( 0 );

    return ret;
}

gint32 CIoManager::RescheduleTaskMainLoop(
    TaskletPtr& pTask )
{

    gint32 ret = 0;
    do{
        if( pTask.IsEmpty() )
        {
            ret = -EINVAL;
            break;
        }
        pTask->MarkPending();
        GetMainIoLoop()->AddTask( pTask );

        if( !pTask->IsAsync() )
        {
            CTaskletSync* pSyncTask =
                static_cast< CTaskletSync* >( ( CTasklet* )pTask );

            if( pSyncTask != nullptr )
            {
                pSyncTask->WaitForComplete();
                ret = pSyncTask->GetError();
            }
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::ClearStandAloneThreads()
{
    CStdRMutex oLock( GetLock() );
    vector< ThreadPtr >::iterator itr =
        m_vecStandAloneThread.begin();

    while( itr != m_vecStandAloneThread.end() ) 
    {
        if( !( *itr )->IsRunning() )
        {
            ( *itr )->Stop();
            itr = m_vecStandAloneThread.erase( itr );
        }
        else
            ++itr;
    }

    if( !m_vecStandAloneThread.empty() )
        return -EEXIST;

    return 0;
}

void CIoManager::WaitThreadsQuit()
{
    while( ClearStandAloneThreads() == -EEXIST )
    {
        CStdRMutex oLock( GetLock() );
        if( m_vecStandAloneThread.empty() )
            break;
        ThreadPtr pThread =
            m_vecStandAloneThread.front();

        oLock.Unlock();
        if( !pThread.IsEmpty() )
            pThread->Join();
    }
}

gint32 CIoManager::StartStandAloneThread(
    ThreadPtr& pThread, EnumClsid iTaskClsid )
{
    gint32 ret = 0;
    
    ret = pThread.NewObj(
        clsid( COneshotTaskThread ) );

    if( ERROR( ret ) )
        return ret;

    CCfgOpenerObj oThrdCfg(
        ( CObjBase*) pThread );

    ret = oThrdCfg.SetIntProp(
        propClsid, iTaskClsid );

    if( ERROR( ret ) )
        return ret;

    pThread->Start(); 

    CStdRMutex oLock( GetLock() );
    m_vecStandAloneThread.push_back( pThread );
    ClearStandAloneThreads();

    return ret;
}

/**
* @name ScheduleTask
* @{ Schedule a deferred task, which will be run
* out of caller running context. if the task is 
* a synchronous task, this method will wait till
* the task is completed
**/
/**Parameters:
 *  iClsid [ in ]: the class id of the task to run
 *  pCfg [ in ]: the config db as the contructor
 *      parameter.
 *
 *  bOneShot [ in ]: the flag to indicate the task
 *      will run on the newly created thread other than
 *      the thread from a thread pool, and the newly
 *      created thread will quit after this task is 
 *      completed
 *@} */

gint32 CIoManager::ScheduleTask(
    EnumClsid iClsid,
    CfgPtr& pCfg,
    bool bOneshot )
{
    gint32 ret = 0;

    do{
        TaskletPtr pTask;
        ret = pTask.NewObj( iClsid, pCfg );
        if( ERROR( ret ) )
            break;

        ret = RescheduleTask(
            pTask, bOneshot );

    }while( 0 );

    return ret;
}

gint32 CIoManager::RescheduleTask(
    TaskletPtr& pTask, bool bOneshot )
{
    gint32 ret = 0;

    do{
        if( pTask.IsEmpty() )
        {
            ret = -EINVAL;
            break;
        }

        ThreadPtr pThread;

        if( unlikely( bOneshot ) )
        {
            ret = StartStandAloneThread(
                pThread, pTask->GetClsid() );

            if( ERROR( ret ) )
                break;

            pTask->MarkPending();

            CTaskThread* pTaskThread = pThread;
            pTaskThread->AddTask( pTask );
        }
        else
        {
            ret = GetTaskThreadPool().GetThread( pThread, true );
            if( ERROR( ret ) )
                break;

            CTaskThread* pgth =
                static_cast< CTaskThread* >( pThread );

            if( pgth )
            {
                pTask->MarkPending();
                pgth->AddTask( pTask );
            }
        }

        if( !pTask->IsAsync() )
        {
            CTaskletSync* pSyncTask =
                static_cast< CTaskletSync* >( ( CTasklet* )pTask );

            if( pSyncTask != nullptr )
            {
                pSyncTask->WaitForComplete();
                ret = pSyncTask->GetError();
            }
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::Stop()
{
    gint32 ret = 0;
    CCfgOpener a;
    a.SetPointer( propIoMgr, this );
    if( true )
    {
        if( IsStopping() )
            return ERROR_STATE;
        m_bStop = true;
    }

    DebugPrint( 0, "IoMgr is stopping..." );

    if( m_iHcTimer != 0 )
    {
        GetUtils().GetTimerSvc().
            RemoveTimer( m_iHcTimer );
        m_iHcTimer = 0;
    }

    ret = ScheduleTask(
        clsid( CIoMgrStopTask ),
        a.GetCfg(),
        true );
    WaitThreadsQuit();
    return ret;
}

CIoManager::CIoManager( const std::string& strModName ) :
      m_strModName( strModName )
{
    gint32 ret = 0;
    SetClassId( clsid( CIoManager ) );

    do{
        CParamList a;
        a.SetPointer( propIoMgr, this );
        a.SetStrProp( propConfigPath,
            string( CONFIG_FILE ) );

        CfgPtr pCfg = a.GetCfg();

        ret = m_pReg.NewObj();
        if( ERROR( ret ) )
        {
            throw std::runtime_error(
                "CRegistry failed to initialize" );
        }
        m_pReg->MakeDir( "/cmdline" );

        StrSetPtr psetPaths( true );
        std::string strPath;
        // get the libcombase.so's path
        ret = GetLibPath( strPath );
        if( SUCCEEDED( ret ) )
        {
            ( *psetPaths )().insert( strPath );
            stdstr strTestLibs = strPath + "/rpcf";
            ( *psetPaths )().insert( strTestLibs );
        }

        // get the executable's path
        strPath.clear();
        ret = GetModulePath( strPath ); 
        if( SUCCEEDED( ret ) )
        {
            ( *psetPaths )().insert( strPath );
            stdstr strTestBins = strPath + "/rpcf";
            ( *psetPaths )().insert( strTestBins );
        }

        if( ( *psetPaths)().empty() )
        {
            throw std::runtime_error(
                "Error get search paths for"
                " config files" );
        }

        GetEnvLibPath( ( *psetPaths )() );
        ObjPtr pObj = psetPaths;
        this->SetCmdLineOpt(
            propSearchPaths, pObj );

        ret = m_pUtils.NewObj(
            clsid( CUtilities ), pCfg );
        if( ERROR( ret ) )
        {
            throw std::runtime_error(
                "CUtilities failed to initialize" );
        }

        ret = m_pPnpMgr.NewObj(
            clsid( CPnpManager ), pCfg );

        if( ERROR( ret ) )
        {
            throw std::runtime_error(
                "CPnpManager failed to initialize" );
        }

        ret = m_pLoop.NewObj(
            clsid( CMainIoLoop ), pCfg );

        if( ERROR( ret ) )
        {
            throw std::runtime_error(
                "CMainIoLoop failed to initialize" );
        }

        ret = m_pDrvMgr.NewObj(
            clsid( CDriverManager ), pCfg );

        if( ERROR( ret ) )
        {
            throw std::runtime_error(
                "CDriverManager failed to initialize" );
        }

        // load count
        a.Push( 1 );
        // max thread
        a.Push( 2 );
        a.Push( ( guint32 )clsid( CIrpCompThread ) );

        ret = m_pIrpThrdPool.NewObj(
            clsid( CIrpThreadPool ), pCfg );
        if( ERROR( ret ) )
        {
            throw std::runtime_error(
                "CIrpThreadPool failed to initialize" );
        }

        a.ClearParams();
        // load count
        a.Push( 1 );
        // max thread
        a.Push( 2 );
        a.Push( ( guint32 )clsid( CTaskThread ) );

        ret = m_pTaskThrdPool.NewObj(
            clsid( CTaskThreadPool ), pCfg );
        if( ERROR( ret ) )
        {
            throw std::runtime_error(
                "CTaskThreadPool failed to initialize" );
        }

        Sem_Init( &m_semSync, 0, 0 );

        Json::Value& oCfg = m_pDrvMgr->GetJsonCfg();
        if( oCfg == Json::Value::null )
        {
            throw std::invalid_argument(
                "No config" );
        }

        m_dwNumCores = std::max( 1U,
            std::thread::hardware_concurrency() );

        Json::Value& oArch = oCfg[ JSON_ATTR_ARCH ];
        if( oArch != Json::Value::null && 
            oArch.isMember( JSON_ATTR_NUM_CORE ) &&
            oArch[ JSON_ATTR_NUM_CORE ] != Json::Value::null )
        {
            Json::Value& oCores = oArch[ JSON_ATTR_NUM_CORE ];
            string strVal = oCores.asString();
            guint32 dwNumCores = std::strtol(
                strVal.c_str(), nullptr, 10 );
            if( dwNumCores != 0 )
            {
                // the user wants to limit the 
                // physical number
                m_dwNumCores = std::min(
                    m_dwNumCores, dwNumCores );
            }
        }

        m_iHcTimer = 0;

    }while( 0 );

    return;
}

static inline string ExtractModNameIoMgr(
    const IConfigDb* pCfg )
{

    if( pCfg == nullptr )
    {
        string strMsg = DebugMsg( -EINVAL,
            "Invalid argument");
        throw std::invalid_argument( strMsg );
    }

    CCfgOpener oCfg( pCfg );
    string strModName;
    gint32 ret = oCfg.GetStrProp(
        0, strModName );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( -EINVAL,
            "Error extract modname");
        throw std::invalid_argument( strMsg );
    }

    return strModName;
}

CIoManager::CIoManager(
    const IConfigDb* pCfg )
   :CIoManager( ExtractModNameIoMgr( pCfg ) )
{
    gint32 ret = 0;
    do{
        CCfgOpener oCfg( pCfg );
        guint32 dwMaxThrds = 0;
        ret = oCfg.GetIntProp(
            propMaxIrpThrd, dwMaxThrds );
        if( SUCCEEDED( ret ) )
        {
            dwMaxThrds = std::min(
                dwMaxThrds,GetNumCores() );
            m_pIrpThrdPool->SetMaxThreads(
                dwMaxThrds );
        }

        ret = oCfg.GetIntProp(
            propMaxTaskThrd, dwMaxThrds );
        if( SUCCEEDED( ret ) )
        {
            dwMaxThrds = std::min(
                dwMaxThrds,GetNumCores() );

            // task thread count cannot be zero
            if( dwMaxThrds == 0 )
                dwMaxThrds = 1;

            m_pTaskThrdPool->SetMaxThreads(
                dwMaxThrds );
        }

    }while( 0 );
}

CIoManager::~CIoManager()
{
    sem_destroy( &m_semSync );
}

std::recursive_mutex& CIoManager::GetLock() const
{
    return m_oGrandLock;
}
gint32 CIoManager::ScheduleWorkitem(
    guint32 dwFlags,
    IEventSink* pCallback,
    LONGWORD dwContext,
    bool bLongWait )
{
    CWorkitemManager& oWiMgr =
        GetUtils().GetWorkitemManager();

    return oWiMgr.ScheduleWorkitem( dwFlags,
        pCallback, dwContext, bLongWait ); 
}

sem_t* CIoManager::GetSyncSem() const
{
    return ( sem_t* )&m_semSync;
}

gint32 CIoManager::RemoveTask(
    TaskletPtr& pTask )
{
    if( pTask.IsEmpty() )
        return -EINVAL;

    ThrdPoolPtr pThrdPool(
         &GetTaskThreadPool() );

    CTaskThreadPool* pTaskPool = pThrdPool;
    if( pTaskPool == nullptr )
        return -EFAULT;

    return pTaskPool->RemoveTask( pTask );
}

gint32 CIoManager::TryLoadClassFactory(
    const std::string& strPath )
{
    gint32 ret = 0;
    do{
        ret = access( strPath.c_str(), X_OK );
        if( SUCCEEDED( ret ) )
        {
            ret = CoLoadClassFactory(
                strPath.c_str() );
            break;
        }
        if( strPath[ 0 ] == '/' )
        {
            ret = -ENOENT;
            DebugPrint( ret,
                "Failed to load class factory %s",
                strPath.c_str() );
            break;
        }

        // relative path
        std::string strFile =
            basename( strPath.c_str() );

        ObjPtr pObj;
        ret = this->GetCmdLineOpt(
            propSearchPaths, pObj );
        if( ERROR( ret ) )
            break;

        StrSetPtr psetPaths( pObj );
        if( psetPaths.IsEmpty() )
        {
            ret = -ENOENT;
            break;
        }

        bool bLoaded = false;
        for( auto& elem : ( *psetPaths )() )
        {
            std::string strFullPath =
                elem + "/" + strFile;

            ret = access(
                strFullPath.c_str(), X_OK );

            if( ret == -1 )
            {
                ret = -errno;
                continue;
            }

            ret = CoLoadClassFactory(
                strFullPath.c_str() );
            if( ERROR( ret ) )
                continue;

            bLoaded = true;
            break;
        }

        if( bLoaded )
            ret = 0;
        else
        {
            DebugPrint( ret,
                "Failed to load class factory %s",
                strPath.c_str() );
            ret = -ENOENT;
        }

    }while( 0 );

    return ret;
}

gint32 CIoManager::TryFindDescFile(
    const std::string& strFileName,
    std::string& strPath )
{
    gint32 ret = 0;
    do{
        ObjPtr pObj;

        // relative path
        std::string strFile = "./";
        strFile +=
            basename( strFileName.c_str() );

        ret = access( strFile.c_str(), R_OK );
        if( ret == 0 )
        {
            strPath = strFile;
            ret = STATUS_SUCCESS;
            break;
        }

        ret = GetCmdLineOpt(
            propSearchPaths, pObj );
        if( ERROR( ret ) )
            break;

        StrSetPtr psetPaths( pObj );
        if( psetPaths.IsEmpty() )
        {
            ret = -ENOENT;
            break;
        }
        bool bFound = false;
        std::string strFullPath;
        for( auto& elem : ( *psetPaths )() )
        {
            strFullPath = elem + "/" + strFile;

            ret = access(
                strFullPath.c_str(), R_OK );

            if( ret == -1 )
            {
                ret = -errno;
                continue;
            }

            bFound = true;
            break;
        }

        if( bFound )
        {
            strPath = strFullPath;
            ret = 0;
            break;
        }

        ret = FindInstCfg(
            strFile, strPath );

    }while( 0 );

    return ret;
}

template<>
gint32 CIoManager::GetCmdLineOpt(
    EnumPropId iPropId, std::string& oVal )
{
    gint32 ret = 0;
    do{
        BufPtr pBuf( true );
        CStdRMutex oRegLock( m_pReg->GetLock() );
        ret = m_pReg->ChangeDir( "/cmdline" );
        if( ERROR( ret ) )
            break;

        ret = m_pReg->GetProperty( iPropId, *pBuf );
        if( ERROR( ret ) )
            break;

        oVal = *pBuf;
    }while( 0 );

    return ret;
}

gint32 CIoManager::GetRouterName(
    std::string& strRtName )
{
    gint32 ret = GetCmdLineOpt(
        propRouterName, strRtName );
    if( ERROR( ret ) )
         strRtName = MODNAME_RPCROUTER;
    return 0;
}

gint32 CIoManager::AddSeqTask(
    TaskletPtr& pTask, bool bLong )
{
    CRpcServices* pIf = GetSyncIf();
    if( pIf == nullptr )
        return -EFAULT;

    return pIf->AddSeqTask( pTask, bLong );
}
gint32 CIoManager::AddAndRun(
    TaskletPtr& pTask, bool bImmediate )
{
    CRpcServices* pIf = GetSyncIf();
    if( pIf == nullptr )
        return -EFAULT;

    return pIf->AddAndRun( pTask, bImmediate );
}

gint32 CIoManager::AppendAndRun(
    TaskletPtr& pTask )
{
    CRpcServices* pIf = GetSyncIf();
    if( pIf == nullptr )
        return -EFAULT;

    return pIf->AppendAndRun( pTask );
}

gint32 CIoMgrStopTask::operator()(
    guint32 dwContext )
{
    gint32 ret = 0;
    CIoManager* pMgr;
    CCfgOpenerObj oCfg( this );
    ret = oCfg.GetPointer( propIoMgr, pMgr );
    if( ERROR( ret ) )
        return ret;

    if( pMgr )
    {
        // NOTE: we cannot stop task
        // thread pool here
        ret = pMgr->GetDrvMgr().Stop();
        ret = pMgr->GetPnpMgr().Stop();
        ObjPtr pObj = pMgr->GetSyncIf();
        if( !pObj.IsEmpty() )
        {
            CRpcServices* pIf = pObj;
            pIf->Stop();
        }
        ret = pMgr->GetTaskThreadPool().Stop();
        ret = pMgr->GetIrpThreadPool().Stop();
        ret = pMgr->GetMainIoLoop()->Stop();
        ret = pMgr->GetUtils().Stop();
    }

    if( dwContext == eventOneShotTaskThrdCtx )
        Sem_Post( &m_semSync );

    return SetError( ret );
}

gint32 CIoMgrPostStartTask::operator()(
    guint32 dwContext )
{
    gint32 ret = 0;
    if( GetConfig().IsEmpty() )
        return SetError( -EFAULT );

    ObjPtr pObj;
    CCfgOpenerObj oCfg( this );
    CIoManager* pMgr = nullptr;
    do{
        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        ret = pMgr->GetUtils().Start();
        if( ERROR( ret ) )
            break;

        pMgr->OnEvent( eventWorkitem,
            eventStart, 0, nullptr );

    }while( 0 );

    if( pMgr )
        Sem_Post( pMgr->GetSyncSem() );

    return SetError( ret );
}

}
