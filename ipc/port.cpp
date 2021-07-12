/*
 * =====================================================================================
 *
 *       Filename:  port.cpp
 *
 *    Description:  implementation of CPort class and the Driver classes
 *
 *        Version:  1.0
 *        Created:  04/12/2016 09:10:13 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi(woodhead99@gmail.com )
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

#include <vector>
#include <string>
//NOTE: c++11 required
#include <stdexcept>
#include "defines.h"
#include "port.h"
#include "frmwrk.h"
#include "stlcont.h"
// #include "emaphelp.h"
#include "jsondef.h"
#include "ifhelper.h"
#include "portex.h"

namespace rpcf
{

using namespace std;

guint32 GetPnpState( IRP* pIrp );
void SetPnpState( IRP* pIrp, guint32 state );

CPort::CPort( const IConfigDb* pCfg )
{
    SetClassId( clsid( CPort ) );

	m_dwFlags = PORTFLG_TYPE_FDO;

    if( true )
    {
        CCfgOpener  oPortCfg;
        ObjPtr pObj( this );
        oPortCfg.SetObjPtr( propPortPtr, pObj );

        m_pPortState.NewObj(
            clsid( CPortState ),
            oPortCfg.GetCfg() );
    }

    // by default, the port are attached at birth
    SetPortState( PORT_STATE_ATTACHED );

    // the upper filter and lower filter port
    m_pUpperPort = nullptr;
	m_pLowerPort = nullptr;
    m_pBusPort = nullptr;


	// the limit of parallel requests in process
	m_dwMaxCurReq = 0;

    // the port identification infomation

	m_pDriver = nullptr;

    m_pCfgDb.NewObj();
    *m_pCfgDb = *pCfg;

    CCfgOpener a( ( IConfigDb* )m_pCfgDb );

    gint32 ret = a.GetPointer(
        propIoMgr, m_pIoMgr );

    if( ERROR( ret ) )
        throw std::invalid_argument(
            "no io manager, cannot run properly" );

    ret = m_pCfgDb->RemoveProperty( propIoMgr );
    if( m_pCfgDb->exist( propDrvPtr ) )
    {
        // optional, not exist if the port is a pdo
        ObjPtr pObj;
        ret = a.GetObjPtr( propDrvPtr, pObj );
        if( SUCCEEDED( ret ) )
        {
            m_pDriver = pObj;
            m_pCfgDb->RemoveProperty(
                propDrvPtr );
        }
    }

    return;
}

CPort::~CPort()
{
}

#define RetrieveBusValue( iSelf_ ) \
do{\
    if( IsBusPort( m_dwFlags ) == PORTFLG_TYPE_BUS )\
    {\
        oBuf = ( *m_pCfgDb )[ iSelf_ ];\
        break;\
    }\
    else\
    {\
        ret = -ENOENT;\
        if( m_pBusPort != nullptr )\
        {\
            ret = m_pBusPort->GetProperty( iProp, oBuf );\
            break;\
        }\
        PortPtr pPort = GetLowerPort(); \
        if( !pPort.IsEmpty() )\
        {\
            oPortLock.Unlock(); \
            ret = pPort->GetProperty( iProp, oBuf );\
            break;\
        }\
    }\
    break;\
}while( 0 );

#define RetrieveValue( Type_, iSelf_ ) \
do{\
    if( PortType( m_dwFlags ) == Type_ )\
    {\
        ret = this->GetProperty( iSelf_, oBuf );\
        break;\
    }\
    else\
    {\
        ret = -ENOENT;\
        PortPtr pPort = GetLowerPort(); \
        if( !pPort.IsEmpty() )\
        {\
            oPortLock.Unlock(); \
            ret = pPort->GetProperty( iProp, oBuf );\
            break;\
        }\
    }\
    break;\
}while( 0 );

gint32 CPort::EnumProperties(
    std::vector< gint32 >& vecProps ) const
{
    vecProps = {
        propPortState,
        // propPortId,
        // propPortName,
        // propPortClass,
        propLowerPortPtr,
        propUpperPortPtr,
        propPortType,
        propIsBusPort,
        propDrvPtr,
        propBusPortPtr,
        // propFdoId,
        // propFdoName,
        // propFdoClass,
        // propPdoId,
        // propPdoName,
        // propPdoClass,
        // propBusId,
        // propBusName,
        // propBusClass
        };

    gint32 ret = super::EnumProperties( vecProps );
    if( ERROR( ret ) )
    {
        vecProps.clear();
        return ret;
    }

    CStdRMutex oPortLock( GetLock() );
    if( !m_pCfgDb.IsEmpty() )
    {
        m_pCfgDb->EnumProperties( vecProps );
    }
    else
    {
        std::sort( vecProps.begin(), vecProps.end() );
    }
    return 0;
}

gint32 CPort::GetProperty( gint32 iProp, CBuffer& oBuf ) const
{
    gint32 ret = 0;

    CStdRMutex oPortLock( GetLock() );
    switch( PropIdFromInt( iProp ) )
    {
    case propPortState:
        {
            // this is a read-only property
            oBuf = GetPortState();
            break;
        }
    case propSelfPtr:
        {
            // this is a read-only property
            oBuf = ObjPtr( ( CObjBase* )this );
            break;
        }
    case propPortId:
    case propPortName:
    case propPortClass:
        {
            ret = m_pCfgDb->GetProperty(
                iProp, oBuf );
            break;
        }
    case propLowerPortPtr:
        {
            ObjPtr a( m_pLowerPort );
            oBuf = a;
            break;
        }
    case propUpperPortPtr:
        {
            ObjPtr a( m_pUpperPort );
            oBuf = a;
            break;
        }
    case propPortType:
        {
            guint32 dwType = PortType( m_dwFlags );
            oBuf = dwType;
            break;
        }
    case propIsBusPort:
        {
            bool bBusPort =
                ( ( m_dwFlags & PORTFLG_TYPE_BUS ) != 0 );
                
            oBuf = bBusPort;
            break;
        }
    case propDrvPtr:
        {
            if( PortType( m_dwFlags ) != PORTFLG_TYPE_PDO )
             {
                 oBuf = ObjPtr( m_pDriver );
             }
             else
             {
                 ret = -EINVAL;
             }
            break;
        }
    case propBusPortPtr:
        {
            if( IsBusPort( m_dwFlags ) == PORTFLG_TYPE_BUS )
            {
                oBuf = ObjPtr( ( CPort* )this );
            }
            else if( PortType( m_dwFlags ) == PORTFLG_TYPE_PDO )
            {
                if( m_pBusPort != nullptr )
                    oBuf = ObjPtr( m_pBusPort );
                else
                    ret = -ENOENT;
            }
            else
            {
                PortPtr pPort( m_pLowerPort );
                oPortLock.Unlock();
                if( !pPort.IsEmpty() )
                {
                    ret = pPort->GetProperty( iProp, oBuf );
                }
                else
                {
                    ret = ERROR_FAIL;
                }
            }
            break;
        }
    case propFdoId:
        {
            RetrieveValue( PORTFLG_TYPE_FDO, propPortId );
            break;
        }
    case propFdoName:
        {
            RetrieveValue( PORTFLG_TYPE_FDO, propPortName );
            break;
        }
    case propFdoClass:
        {
            RetrieveValue( PORTFLG_TYPE_FDO, propPortClass );
            break;
        }
    case propFdoPtr:
        {
            RetrieveValue( PORTFLG_TYPE_FDO, propSelfPtr );
            break;
        }
    case propPdoId:
        {
            RetrieveValue( PORTFLG_TYPE_PDO, propPortId );
            break;
        }
    case propPdoName:
        {
            RetrieveValue( PORTFLG_TYPE_PDO, propPortName );
            break;
        }
    case propPdoClass:
        {
            RetrieveValue( PORTFLG_TYPE_PDO, propPortClass );
            break;
        }
    case propPdoPtr:
        {
            RetrieveValue( PORTFLG_TYPE_PDO, propSelfPtr );
            break;
        }
    case propBusId:
        {
            RetrieveBusValue( propPortId );
            break;
        }
    case propBusName:
        {
            RetrieveBusValue( propPortName );
            break;
        }
    case propBusClass:
        {
            RetrieveBusValue( propPortClass );
            break;
        }
    case propSrcDBusName:
    case propSrcUniqName:
        {
            RetrieveBusValue( iProp );
            break;
        }
    default:
        {
            ret = m_pCfgDb->GetProperty(
                iProp, oBuf );
            break;
        }
    }
    return ret;
}

gint32 CPort::SetProperty( gint32 iProp, const CBuffer& oBuf )
{
    gint32 ret = 0;

    CStdRMutex oPortLock( GetLock() );

    switch( PropIdFromInt( iProp ) )
    {
    case propPortState:
        {
            // please don't do it unless
            // you know what you are doing
            guint32 dwPortState = ( guint32& )oBuf;
            ret = SetPortState( dwPortState );
            break;
        }
    case propPortId:
    case propPortName:
    case propPortClass:
        ( *m_pCfgDb )[ iProp ] = oBuf;
        break;

    case propUpperPortPtr:
        {
            if( m_pUpperPort != nullptr )
            {
                ret = -EEXIST;
                break;
            }

            ObjPtr a = oBuf;
            if( a.IsEmpty() )
            {
                m_pUpperPort = nullptr;
            }
            else
            {
                m_pUpperPort = a;
            }
            break;
        }
    case propLowerPortPtr:
        {
            if( m_pLowerPort != nullptr )
            {
                ret = -EEXIST;
                break;
            }
            ObjPtr a = oBuf;
            if( a.IsEmpty() )
                m_pLowerPort = nullptr;
            else
            {
                m_pLowerPort = a;
            }

            break;
        }
    default:
        {
            ret = m_pCfgDb->SetProperty(
                iProp, oBuf );
            break;
        }
    }
    return ret;
}

gint32 CPort::RemoveProperty( gint32 iProp )
{
    gint32 ret = 0;
    CStdRMutex oPortLock( GetLock() );
    switch( iProp )
    {
    case propLowerPortPtr:
        {
            m_pLowerPort = nullptr;
            break;
        }
    case propUpperPortPtr:
        {
            m_pUpperPort = nullptr;
            break;
        }
    case propDrvPtr:
        {
            m_pDriver = nullptr;
            break;
        }
    case propBusPortPtr:
        {
            if( PortType( m_dwFlags ) == PORTFLG_TYPE_BUS )
            {
                ret = -ENOTSUP;
            }
            else if( PortType( m_dwFlags ) == PORTFLG_TYPE_PDO )
            {
                m_pBusPort = nullptr;
            }
            break;
        }
    default:
        ret = m_pCfgDb->RemoveProperty( iProp );
    }

    return ret;
}

gint32 CPort::StartEx( IRP* pIrp )
{
    // the state transition for port starting
    // is :
    // PNP_STATE_START_LOWER --->
    // PNP_STATE_START_PRE_SELF --->
    // PNP_STATE_START_SELF --->
    // PNP_STATE_START_POST_SELF --->
    // PNP_STATE_START_PORT_RDY
    gint32 ret = 0;
    do{
        if( GetPnpState( pIrp ) == PNP_STATE_START_LOWER )
        {
            SetPnpState( pIrp, PNP_STATE_START_PRE_SELF );
            ret = PreStart( pIrp );

            if( ERROR( ret ) )
                break;

            if( ret == STATUS_PENDING )
                break;
        }

        // no else in case prestart can return successfully
        // immediately
        if( GetPnpState( pIrp ) == PNP_STATE_START_PRE_SELF )
        {
            SetPnpState( pIrp, PNP_STATE_START_SELF );
            ret = Start( pIrp );
            if( ERROR( ret ) )
                break;

            if( ret == STATUS_PENDING )
                break;
        }

        if( GetPnpState( pIrp ) == PNP_STATE_START_SELF )
        {
            SetPnpState( pIrp, PNP_STATE_START_POST_SELF );
            ret = PostStart( pIrp );
        }

    }while( 0 );

    return ret;
}

gint32 CPort::StopEx( IRP* pIrp )
{
    gint32 ret = 0;

    do{
        if( GetPnpState( pIrp ) == PNP_STATE_STOP_BEGIN )
        {
            // we put PreStop ahead of
            // StopLowerPort in case the port
            // needs to do some IO before the
            // lower port truly stopped.
            //
            // this is the last chance the port
            // will do IO via the lower port if
            // the underlying connection is still
            // alive
            ret = PreStop( pIrp );

            if( ERROR( ret ) )
                break;

            if( ret != STATUS_MORE_PROCESS_NEEDED )
            {
                SetPnpState( pIrp, PNP_STATE_STOP_PRE );
            }
            else
            {
                ret = STATUS_PENDING;
            }

            if( !SUCCEEDED( ret ) )
                break;
        }

        if( GetPnpState( pIrp ) == PNP_STATE_STOP_PRE )
        {
            SetPnpState( pIrp, PNP_STATE_STOP_LOWER );
            ret = StopLowerPort( pIrp );
            if( !SUCCEEDED( ret ) )
                break;
        }

        if( GetPnpState( pIrp ) == PNP_STATE_STOP_LOWER )
        {
            SetPnpState( pIrp, PNP_STATE_STOP_SELF );
            ret = Stop( pIrp );
            if( !SUCCEEDED( ret ) )
                break;
        }

        if( GetPnpState( pIrp ) == PNP_STATE_STOP_SELF )
        {
            SetPnpState( pIrp, PNP_STATE_STOP_POST );
            ret = PostStop( pIrp );
            if( !SUCCEEDED( ret ) )
                break;
        }

        if( GetPnpState( pIrp ) == PNP_STATE_STOP_POST )
        {
            break;
        }
        else
        {
            ret = ERROR_STATE;
        }

    }while( 0 );

    return ret;
}

gint32 CPort::PreStart( IRP* pIrp )
{
    return 0;
}


gint32 CPortStartStopNotifTask::operator()(
    guint32 dwContext )
{
    gint32 ret = 0;
    CParamList oTaskCfg( ( IConfigDb*)GetConfig() );

    do{
        ObjPtr pObj;

        CStlEventMap *pEventMap = nullptr;
        ret = oTaskCfg.GetPointer( 2, pEventMap );
        if( ERROR( ret ) )
            break;

        IPort* pBottomPort = nullptr;
        ret = oTaskCfg.GetPointer( 1, pBottomPort );
        if( ERROR( ret ) )
            break;

        guint32 dwEventId;
        ret = oTaskCfg.GetIntProp( 0, dwEventId );
        if( ERROR( ret ) )
            break;

        if( pEventMap != nullptr &&
            pBottomPort != nullptr )
        {
            pEventMap->BroadcastEvent(
                eventPnp,
                dwEventId,
                PortToHandle( pBottomPort ),
                nullptr );
        }

    }while( 0 );

    oTaskCfg.ClearParams();

    return ret;
}

gint32 CPort::ScheduleStartStopNotifTask(
    guint32 dwEventId, bool bImmediately )
{
    gint32 ret = 0;
    do{
        // trigger a port started event
        if( GetUpperPort().IsEmpty() )
        {
            CParamList a;

            ObjPtr pObj;

            // param 1, event id
            a.Push( dwEventId );

            pObj = GetBottomPort();

            // param 2 this port
            a.Push( pObj );

            CEventMapHelper< CPort >
                b( ( CPort* )pObj );

            ret = b.GetEventMap( pObj ); 

            if( ERROR( ret ) )
                break;

            // param 3 the event map
            a.Push( pObj );

            if( !bImmediately )
            {
                ret = GetIoMgr()->ScheduleTask(
                    clsid( CPortStartStopNotifTask ), a.GetCfg() );
                if( SUCCEEDED( ret ) )
                    ret = STATUS_PENDING;
            }
            else
            {
                TaskletPtr pTask;
                pTask.NewObj(
                    clsid( CPortStartStopNotifTask ), a.GetCfg() );
                ret = ( *pTask )( dwEventId );
            }
        }
    }while( 0 );
    return ret;
}

gint32 CPort::PostStop( IRP* pIrp )
{
    return 0;
}

gint32 CPort::PostStart( IRP* pIrp )
{
    return 0;
}

gint32 CPort::OnPortReady( IRP* pIrp )
{
    // the port won't send out event if the
    // event is triggered by that IRP.
    return 0;
}

void CPort::OnPortStartFailed(
    IRP* pIrp, gint32 ret )
{ return; }

void CPort::OnPortStopped()
{
    // FIXME: not symetric with OnPortStarted
    // for stopping, there is not a neat solution
    // to put the OnPortStopped in the irp
    // callback
    //
    ScheduleStartStopNotifTask(
        eventPortStopped,
        !GetIoMgr()->RunningOnMainThread() );

    DebugPrint( 0, "%s is stopped...",
        CoGetClassName( GetClsid() ) );
}

gint32 CCancelIrpsTask::operator()(
    guint32 dwContext ) 
{
    gint32 ret = 0;

    CParamList oParams(
        ( IConfigDb* )m_pCtx );
    do{
        ObjPtr pObj;
        ret = oParams.GetObjPtr( 1, pObj ); 
        if( ERROR( ret ) )
            break;

        IrpVecPtr pIrpVec = pObj;
        if( pIrpVec.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        gint32 iError = 0;
        ret = oParams.GetIntProp(
            0, ( guint32& )iError );
        if( ERROR( ret ) )
        {
            iError = ERROR_CANCEL;
        }

        bool bComplete = false;
        ret = oParams.GetBoolProp( 2, bComplete );
        if( ERROR( ret ) )
            bComplete = false;

        CIoManager* pMgr = nullptr;
        ret = oParams.GetPointer(
            propIoMgr, pMgr );

        if( ERROR( ret ) )
            break;

        IRP* pIrp = nullptr;
        guint32 dwSize = ( *pIrpVec )().size();
        for( guint32 i = 0; i < dwSize; ++i ) 
        {
            pIrp = ( *pIrpVec )()[ i ];
            if( pIrp == nullptr ||
                pIrp->GetStackSize() == 0 )
                continue;

            if( !bComplete )
            {
                pMgr->CancelIrp(
                    pIrp, true, iError );
            }
            else
            {
                pMgr->CompleteIrp( pIrp );
            }
        }

        ( *pIrpVec )().clear();

        // complete the parent irp if any
        ret = oParams.GetObjPtr(
            propIrpPtr, pObj );

        if( ERROR( ret ) )
        {
            ret = 0;
            break;
        }

        IRP* pMasterIrp = pObj;
        if( pMasterIrp == nullptr )
            break;

        pMgr->CompleteIrp( pMasterIrp );

    }while( 0 );

    oParams.Clear();

    return SetError( ret );
}

gint32 CPort::PreStop( IRP* pIrp )
{
    // if you want to do something in the PreStop
    // override this method and execute your logics
    // and call this method at the end
    //
    // one scenario is the port spawned IRP, which
    // won't be monitored by the GateKeeper, so it
    // must follow the rules to be completed or 
    // canceled after PreStop is called
    gint32 ret = 0;
    do{
        ret = GetIoMgr()->RemoveFromHandleMap( this );

        // cancel all the irps
        // if( GetUpperPort() == nullptr )
        if( true )
        {
            CIrpGateKeeper oGateKeeper( this );
            map< IrpPtr, gint32 > mapIrps;
            oGateKeeper.GetAllIrps( mapIrps );

            if( mapIrps.size() == 0 )
            {
                ret = 0;
                break;
            }

            map< IrpPtr, gint32 >::iterator itr
                = mapIrps.begin();

            IrpVecPtr pIrpVec( true );

            while( itr != mapIrps.end())
            {
                ( *pIrpVec )().push_back( itr->first );
                ++itr;
            }

            CParamList oParams;
            gint32 iError = ERROR_PORT_STOPPED;
            ret = oParams.Push( iError );
            if( ERROR( ret ) )
                break;

            ObjPtr pObj;
            pObj = pIrpVec;
            ret = oParams.Push( pObj );
            if( ERROR( ret ) )
                break;

            oParams.SetPointer(
                propIoMgr, GetIoMgr() );

            pObj = IrpPtr( pIrp );
            ret = oParams.SetObjPtr(
                propIrpPtr, pObj );

            if( ERROR( ret ) )
                break;

            TaskletPtr pTask;
            ret = pTask.NewObj(
                clsid( CCancelIrpsTask ),
                oParams.GetCfg() );

            if( ERROR( ret ) )
                break;

            // a synchronous cancel
            //
            // here we implicitly assume the STOP
            // IRP cannot be cancelled at any
            // time.
            ( *pTask )( eventZero );

        }
    }while( 0 );

    return ret;
}

void SetPnpState(
    IRP* pIrp, guint32 state )
{
    BufPtr pBuf;
    pIrp->GetCurCtx()->GetExtBuf( pBuf );

    if( 0 == pBuf->size() )
        return;

    PORT_START_STOP_EXT* psse =
        ( PORT_START_STOP_EXT* )( *pBuf );

    psse->m_dwState = state;
}

guint32 GetPnpState(
    IRP* pIrp )
{
    BufPtr pBuf;
    pIrp->GetCurCtx()->GetExtBuf( pBuf );

    if( 0 == pBuf->size() )
        return 0;

    PORT_START_STOP_EXT* psse =
        ( PORT_START_STOP_EXT* )( *pBuf );

    return psse->m_dwState;
}

gint32 CPort::AllocIrpCtxExt(
    IrpCtxPtr& pIrpCtx,
    void* pContext ) const
{
    gint32 ret = 0;
    switch( pIrpCtx->GetMajorCmd() )
    {
    case IRP_MJ_PNP:
        {
            switch( pIrpCtx->GetMinorCmd() )
            {
            case IRP_MN_PNP_START:
            case IRP_MN_PNP_STOP:
            case IRP_MN_PNP_QUERY_STOP:
                {
                    PORT_START_STOP_EXT oExt; 
                    oExt.m_dwState = 0;

                    BufPtr pBuf( true );
                    if( ERROR( ret ) )
                        break;

                    *pBuf = oExt;
                    pIrpCtx->SetExtBuf( pBuf );
                    break;
                }
            default:
                break;
            }
            break;
        }
    case IRP_MJ_ASSOC:
        {
            switch( pIrpCtx->GetMinorCmd() )
            {
            case IRP_MN_ASSOC_RUN:
                {
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

gint32 CPort::SubmitStartIrp( IRP* pIrp )
{

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    gint32 ret = 0;
    guint32 dwOldState = PORT_STATE_INVALID;

    do{

        // in this way, we do not need to lock the
        // port from top down to the bottom

        IPort* pLowerPort = GetLowerPort();
        SetPnpState( pIrp, PNP_STATE_START_LOWER );

        if( pLowerPort != nullptr )
        {
            // alloc the irp stack
            ret = pIrp->AllocNextStack(
                pLowerPort, IOSTACK_ALLOC_COPY );

            if( SUCCEEDED( ret ) )
            {
                pLowerPort->AllocIrpCtxExt(
                    pIrp->GetTopStack() );
            }

            // FIXME: what to do on failure
            if( ERROR( ret ) )
                break;

            // start the lower port before self-start
            ret = pLowerPort->SubmitIrp( pIrp );

            if( ret == STATUS_PENDING )
                break;
        
            // pop the irp stack if allocated
            pIrp->PopCtxStack();
            IrpCtxPtr& pIrpCtx = pIrp->GetCurCtx();

            pIrpCtx->SetStatus( ret );

            CCfgOpenerObj oPortCfg(
                ( CObjBase* )pLowerPort );

            guint32 dwLowPortState = 0;

            ret = oPortCfg.GetIntProp(
                propPortState, dwLowPortState );

            if( ERROR( ret ) )
                break;

            if( dwLowPortState != PORT_STATE_READY )
            {
                // failed to start lower port
                ret = ERROR_FAIL;
                break;
            }

            // well, the lower port is already in
            // good shape, let's move on
            ret = 0;

            // succeeded immediately,
            // let's fall through
        }

        // if succeeded, let's start this port
        if( SUCCEEDED( ret ) )
        {
            CStdRMutex oPortLock( GetLock() );

            ret = CanContinue(
                pIrp, PORT_STATE_STARTING, &dwOldState );

            if( ERROR( ret ) )
                return ret;

            // NOTE: we don't use CanContinue here
            // since this is the only thread this port
            // is referenced for now.
            if( GetPortState() != PORT_STATE_STARTING )
            {
                // wrong state, don't continue
                ret = ERROR_STATE;
                break;
            }

            oPortLock.Unlock();
            ret = StartEx( pIrp );
            oPortLock.Lock();

            if( ret == STATUS_PENDING )
                break;

            if( GetPortState() != PORT_STATE_STARTING )
            {
                // something unexpected
                ret = ERROR_STATE;
                break;
            }
            if( ERROR( ret ) )
            {
                break;
            }

            // Now the port ready for PNP_STOP
            // request but not quite ok for other requests
            // if there is still something to do in
            // OnPortReady. The func command should work
            // when the eventPortStarted is received
            SetPortState( PORT_STATE_READY );
            SetPnpState( pIrp, PNP_STATE_START_PORT_RDY );

            oPortLock.Unlock();
            ret = OnPortReady( pIrp );
        }

    }while( 0 );

    if( ERROR( ret ) )
    {
        IrpCtxPtr& pIrpCtx = pIrp->GetCurCtx();
        pIrpCtx->SetStatus( ret );
        SetPortState( PORT_STATE_STOPPED );
        OnPortStartFailed( pIrp, ret );
    }

    return ret;
}

gint32 CPort::StopLowerPort( IRP* pIrp )
{
    gint32 ret = 0;

    // let's first stop the lower port before
    // stopping this port
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        // stop the lower irp
        if( !GetLowerPort().IsEmpty() )
        {
            PortPtr pLowerPort( GetLowerPort() );
            SetPnpState( pIrp, PNP_STATE_STOP_LOWER );

            ret = pIrp->AllocNextStack(
                pLowerPort, IOSTACK_ALLOC_COPY );

            if( ERROR( ret ) )
                break;

            ret = pLowerPort->AllocIrpCtxExt(
                pIrp->GetTopStack() );

            ret = pLowerPort->SubmitIrp( pIrp );

            if( ret == STATUS_PENDING )
                break;
        
            // pop the irp stack if allocated
            pIrp->PopCtxStack();

            IrpCtxPtr& pIrpCtx =
                pIrp->GetCurCtx();

            pIrpCtx->SetStatus( ret );

            if( ERROR( ret ) )
                break;
        }
    }while( 0 );

    return ret;
}

gint32 CPort::SubmitStopIrpEx( IRP* pIrp )
{
    gint32 ret = 0;
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    DebugPrint( 0, "Stopping %s @0x%x...",
        CoGetClassName( GetClsid() ), this );
    // PNP_STATE_STOP_PRELOCK is a state before
    // the port switch to PORT_STATE_STOPPING,
    // that is the I/O is still available at this
    // stage.
    SetPnpState( pIrp, PNP_STATE_STOP_PRELOCK );
    ret = OnPrepareStop( pIrp );
    if( ret == STATUS_PENDING )
        return ret;

    return SubmitStopIrp( pIrp );
}

gint32 CPort::SubmitStopIrp( IRP* pIrp )
{

    gint32 ret = 0;
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;

    // let's first stop this port before send
    // it down to the lower port
    do{

        // steps to stop this port after stop
        // the lower port
        // 1. stop further irp submitting to this port
        //
        // 2. cancel all the IRPs waiting in the queue
        //
        // 3. release the resources held by this port
        //
        // 4. put the port in stop state
        //
        // we cannot preempt to cancel on-going requests
        // because the control will turn out to be very
        // complicated.
        // Therefore, the general rule is that all the
        // requests should aware of this and keep the
        // period at BUSY state as short as possible
        //
        // PORT_STATE_STOPPING serve as a roadblock to
        // block further irp submission
        if( true )
        {
            CStdRMutex oStatLock( m_pPortState->GetLock() );
            ret = CanContinue( pIrp, PORT_STATE_STOPPING );

            if( ERROR( ret ) )
                break;

            if( ret == STATUS_PENDING )
            {
                // It is possible CanContinue could be
                // blocked in an heavy traffic situation,
                // it will be resumed later
                //
                // to support resume, you should not rely
                // too much on whether the code is running
                // in the submit-irp context
                TaskletPtr pTask;
                ret = GetStopResumeTask( pIrp, pTask );
                if( ERROR( ret ) )
                    break;

                m_pPortState->SetResumePoint( pTask );
                ret = STATUS_PENDING;
                break;
            }
        }

        SetPnpState( pIrp, PNP_STATE_STOP_BEGIN );

        ret = StopEx( pIrp );

        if( ret == STATUS_PENDING )
        {
            break;
        }
        else
        {
            CStdRMutex a( GetLock() );
            guint32 dwPortState = GetPortState();
            if( dwPortState != PORT_STATE_STOPPING )
            {
                // FIXME: we should not land here
                ret = ERROR_STATE;
                throw std::runtime_error(
                    "Port state machine has gone wrong" );
                break;
            }

            //FIXME: whether or not succeeded,
            //this port will be marked stopped
            bool bSet = SetPortStateWake(
                PORT_STATE_STOPPING,
                PORT_STATE_STOPPED );
            if( !bSet )
                ret = ERROR_FAIL;

            // if( SUCCEEDED( ret ) )
            {
                a.Unlock();
                OnPortStopped();
                a.Lock();

            }
        }


    }while( 0 );

    if( ERROR( ret ) )
    {
#ifdef DEBUG
        string strObjDump;
        this->Dump( strObjDump );
        DebugPrint( ret, "SubmitStopIrp failed "
            "portstate=%d, pnpstate=%d,\n%s",
            GetPortState(),
            GetPnpState( pIrp ),
            strObjDump.c_str() );
#endif
    }

    return ret;
}

gint32 CPort::SubmitQueryStopIrp( IRP* pIrp )
{

    gint32 ret = 0;
    guint32 dwOldState = 0;

    ret = CanContinue(
        pIrp, PORT_STATE_BUSY_SHARED, &dwOldState );

    if( ERROR( ret ) )
        return ret;

    do{
        if( pIrp == nullptr
            || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }


        IrpCtxPtr pCurCtx = pIrp->GetCurCtx();
        IPort* pLowerPort = GetLowerPort();

        if( pLowerPort )
        {
            pIrp->AllocNextStack(
                pLowerPort, IOSTACK_ALLOC_COPY );

            pLowerPort->AllocIrpCtxExt(
                pIrp->GetTopStack() );

            ret = pLowerPort->SubmitIrp( pIrp );

            if( ret == STATUS_PENDING )
                break;

            if( ERROR( ret ) )
            {
                // ignore the status on the
                // context stack
                pCurCtx->SetStatus( ret );
                pIrp->PopCtxStack();
                break;
            }
        }

        SetPnpState( pIrp, 1 );
        ret = OnQueryStop( pIrp );
        if( ret == STATUS_PENDING )
            break;

    }while( 0 );

    // either query completed or not
    // the port return to the READY state
    // SetPortStateWake(
    //   PORT_STATE_BUSY, dwOldState );
    PopState( dwOldState );

    return ret;
}

gint32 CPort::SubmitPortStackIrp( IRP* pIrp )
{
    // This IRP must not return with
    // STATUS_PENDING, and must reutrn
    // immediately
    gint32 ret = 0;
    guint32 dwOldState;

    if( pIrp == nullptr )
        return -EINVAL;

    ret = CanContinue(
        pIrp, PORT_STATE_BUSY, &dwOldState );

    if( ERROR( ret ) )
        return ret;

    do{
        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack();
        guint32 dwMinorCmd = pIrpCtx->GetMinorCmd();

        IPort* pLowerPort = GetLowerPort();
        if( pLowerPort )
        {
            guint32 dwPos = pIrp->GetCurPos();
            pIrp->AllocNextStack(
                pLowerPort, IOSTACK_ALLOC_COPY );

            pLowerPort->AllocIrpCtxExt(
                pIrp->GetTopStack() );

            ret = pLowerPort->SubmitIrp( pIrp );
            if( ret == STATUS_PENDING )
            {
                throw std::runtime_error(
                    "STATUS_PENDING is not allowed in STACK_DESTROY irp" );
                break;
            }

            pIrp->SetCurPos( dwPos );
            pIrp->PopCtxStack();
            pIrp->GetCurCtx()->SetStatus( ret );

            // we ignore the return value and
            // let's continue with what should do
            // next
        }

        do{
            if( dwMinorCmd == IRP_MN_PNP_STACK_BUILT )
            {
                if( pLowerPort == nullptr )
                {
                    //
                    // register this port and make it accessable
                    // to the system
                    //
                    // we now make registry for the pdo port only 
                    //
                    string strPath;
                    CCfgOpenerObj a( this );
                    ret = a.GetStrProp( propRegPath, strPath );
                    if( ERROR( ret ) )
                    {
                        // no reg path, not a big deal
                        break;
                    }

                    CRegistry& oReg = GetIoMgr()->GetRegistry();
                    CStdRMutex oLock( oReg.GetLock() );

                    ret = oReg.ExistingDir( strPath );
                    if( ERROR( ret ) )
                    {
                        ret = oReg.MakeDir( strPath );
                        if( ERROR( ret ) )
                            break;
                    }
                    else
                    {
                        ret = oReg.ChangeDir( strPath );
                        if( ERROR( ret ) )
                            break;
                    }

                    ObjPtr pThisPort( this );

                    // set the registry entry for this port
                    ret = oReg.SetObject( propObjPtr, pThisPort );
                    if( ERROR( ret ) )
                        break;

                    if( GetLowerPort().IsEmpty() )
                    {
                        // for pdo port or the bottom port, we have a
                        // notification point for subscribers to register
                        // the callback for pnp event
                        ObjPtr pMap;
                        pMap.NewObj( clsid( CStlEventMap ) );
                        ret = oReg.SetObject( propEventMapPtr, pMap ); 
                        if( ERROR( ret ) )
                            break;
                    }
                }

                PopState( dwOldState );
                ret = OnPortStackBuilt( pIrp );
            }
            else if( dwMinorCmd == IRP_MN_PNP_STACK_DESTROY )
            {
                // IRP_MN_PNP_STACK_DESTROY
                ret = OnPortStackDestroy( pIrp );

                if( pLowerPort == nullptr )
                {
                    string strPath;
                    CCfgOpenerObj a( this );
                    ret = a.GetStrProp( propRegPath, strPath );
                    if( ERROR( ret ) )
                    {
                        // not a critical issue, ignore it
                        ret = 0;
                        break;
                    }

                    // remove the registry entry for this port
                    CRegistry& oReg = GetIoMgr()->GetRegistry();
                    CStdRMutex( oReg.GetLock() );
                    ret = oReg.ExistingDir( strPath );
                    if( SUCCEEDED( ret ) )
                    {
                        ret = oReg.RemoveDir( strPath );
                    }
                }

                PopState( dwOldState );
                SetPortStateWake( PORT_STATE_STOPPED,
                    PORT_STATE_REMOVED );
            }
            else if( dwMinorCmd == IRP_MN_PNP_STACK_READY )
            {
                // IRP_MN_PNP_STACK_DESTROY
                PopState( dwOldState );
                ret = OnPortStackReady( pIrp );
            }
            else
            {
                ret = -ENOTSUP;
            }

        }while( 0 );

    }while( 0 );

    return ret;
}

gint32 CPort::SubmitFuncIrp( IRP* pIrp )
{
    gint32 ret = 0;

    guint32 dwOldState;

    ret = CanContinue( pIrp,
        PORT_STATE_BUSY_SHARED, &dwOldState );

    if( ERROR( ret ) )
    {
        DebugPrint( ret,
            "CPort::SubmitFuncIrp error, portstate = %d",
            GetPortState() );

        IrpCtxPtr& pCtx = pIrp->GetTopStack();
        pCtx->SetStatus( ret );

        return ret;
    }

    do{
	    ret = OnSubmitIrp( pIrp );

    }while( 0 );

    PopState( dwOldState );

    return ret;
}
/**
* @name SubmitSetPropIrp
* @brief to set a property on the specified port
* object.
*
* @parameters the m_pReqData hold a pointer to a
* IConfigDb object, which contains the following
* parameters
*   [ 0 ] : Pointer to the port whose property 
*           will be set.
*
*   [ 1 ] : PropertyId, the property id 
*
*   [ 2 ] : a BufPtr containing the value of the
*           property
* @{ */
/**  @} */

gint32 CPort::SubmitSetPropIrp( IRP* pIrp )
{
    gint32 ret = 0;

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    IrpCtxPtr pCtx = pIrp->GetCurCtx();

    do{
        CStdRMutex oPortLock( GetLock() );
        guint32 dwPortState = GetPortState();

        if( dwPortState == PORT_STATE_REMOVED
            || dwPortState == PORT_STATE_INVALID )
        {
            ret = ERROR_STATE;
            break;
        }

        CfgPtr pParams;
        ret = pCtx->GetReqAsCfg( pParams );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }
        CParamList oParams( pParams );
        ObjPtr pTarget;
        ret = oParams.GetObjPtr( 0, pTarget );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        if( pTarget->GetObjId() != GetObjId() )
        {
            IPort* pLowerPort = GetLowerPort();
            if( pLowerPort == nullptr )
            {
                ret = -ENOENT;
                break;
            }
            ret = pIrp->AllocNextStack(
                pLowerPort, IOSTACK_ALLOC_COPY );

            if( ERROR( ret ) )
                break;

            pLowerPort->AllocIrpCtxExt(
                pIrp->GetTopStack() );

            oPortLock.Unlock();

            ret = pLowerPort->SubmitIrp( pIrp );

            oPortLock.Lock();

            dwPortState = GetPortState();
            if( dwPortState == PORT_STATE_REMOVED
                || dwPortState == PORT_STATE_INVALID )
            {
                ret = ERROR_STATE;
                pIrp->PopCtxStack();
                break;
            }

            pIrp->PopCtxStack();
            break;
        }

        BufPtr pVal;
        ret = oParams.GetProperty( 2, pVal );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwPropId = 0;
        ret = oParams.GetIntProp( 1, dwPropId );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }

        ret = this->SetProperty(
            dwPropId, *pVal );

        break;

    }while( 0 );

    pCtx->SetStatus( ret );

    return ret; 
}

/**
* @name SubmitGetPropIrp
* @brief to set a property on the specified port
* object.
*
* @parameters the m_pReqData hold a pointer to a
* IConfigDb object, which contains the following
* parameters
*   [ 0 ] : Pointer to the port whose property 
*           will be set.
*
*   [ 1 ] : PropertyId, the property id 
*
* @return the m_pRespData contains the property
* value with a CBuffer object.
* @{ */
/**  @} */
gint32 CPort::SubmitGetPropIrp( IRP* pIrp )
{
    gint32 ret = 0;

    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    IrpCtxPtr pCtx = pIrp->GetCurCtx();

    do{
        CStdRMutex oPortLock( GetLock() );
        guint32 dwPortState = GetPortState();

        if( dwPortState == PORT_STATE_REMOVED
            || dwPortState == PORT_STATE_INVALID )
        {
            ret = ERROR_STATE;
            break;
        }

        CfgPtr pParams;
        ret = pCtx->GetReqAsCfg( pParams );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }
        CParamList oParams( pParams );
        ObjPtr pTarget;
        ret = oParams.GetObjPtr( 0, pTarget );
        if( ERROR( ret ) )
        {
            ret = -EINVAL;
            break;
        }
        if( pTarget->GetObjId() != GetObjId() )
        {
            IPort* pLowerPort = GetLowerPort();
            if( pLowerPort == nullptr )
            {
                ret = -ENOENT;
                pCtx->SetStatus( ret );
                break;
            }
            ret = pIrp->AllocNextStack(
                pLowerPort, IOSTACK_ALLOC_COPY );

            if( ERROR( ret ) )
                break;

            pLowerPort->AllocIrpCtxExt(
                pIrp->GetTopStack() );

            oPortLock.Unlock();

            ret = pLowerPort->SubmitIrp( pIrp );

            oPortLock.Lock();

            dwPortState = GetPortState();
            if( dwPortState == PORT_STATE_REMOVED
                || dwPortState == PORT_STATE_INVALID )
            {
                ret = ERROR_STATE;
                pCtx->SetStatus( ret );
                pIrp->PopCtxStack();
                break;
            }

            pCtx->SetStatus( ret );
            if( SUCCEEDED( ret ) )
            {
                BufPtr& pResp =
                    pIrp->GetTopStack()->m_pRespData;

                if( !pResp.IsEmpty() )
                {
                    pCtx->m_pRespData = pResp;
                }
                else
                {
                    // weired
                    ret = ERROR_FAIL;
                    pCtx->SetStatus( ret );
                }
            }
            pIrp->PopCtxStack();
            break;
        }

        guint32 dwPropId = 0;
        ret = oParams.GetIntProp( 1, dwPropId );
        if( ERROR( ret ) )
        {
            pCtx->SetStatus( ret );
            break;
        }

        BufPtr pResp( true );
        ret = this->GetProperty( dwPropId, *pResp );

        pCtx->SetStatus( ret );
        if( SUCCEEDED( ret ) )
        {
            pCtx->SetRespData( pResp );
        }
        break;

    }while( 0 );

    return ret; 
}

gint32 CPort::SubmitPnpIrp( IRP* pIrp )
{
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pIrpCtx =
            pIrp->GetTopStack();

        guint32 dwCmd = pIrpCtx->GetMinorCmd();

        switch( dwCmd )
        {
        case IRP_MN_PNP_START:
            {
                // let's first start the lower port
                ret = SubmitStartIrp( pIrp );
                break;
            }
        case IRP_MN_PNP_STOP:
            {
                ret = SubmitStopIrpEx( pIrp );
                break;
            }

        case IRP_MN_PNP_QUERY_STOP:
            {
                // a command goes right before
                // the true stop
                ret = SubmitQueryStopIrp( pIrp );
                break;
            }
        case IRP_MN_PNP_STACK_BUILT:
        case IRP_MN_PNP_STACK_DESTROY:
        case IRP_MN_PNP_STACK_READY:
            {
                // NOTE: this is an immediate request
                ret = SubmitPortStackIrp( pIrp );
                break;
            }
        default:
            {
                ret = -ENOTSUP;
                break;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CPort::PassUnknownIrp( IRP* pIrp )
{
    // handle the submission of those
    // unknown irp
    gint32 ret = 0;
    do{
        if( pIrp == nullptr
            || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        IPort* pLowerPort = GetLowerPort();

        if( PortType( m_dwFlags ) == PORTFLG_TYPE_FIDO
            && pLowerPort != nullptr )
        {
            // NOTE: the default behavor for
            // filter driver is to
            // pass unknown irps on to the
            // lower driver
            ret = pIrp->AllocNextStack(
                pLowerPort, IOSTACK_ALLOC_COPY );

            if( ERROR( ret ) )
                break;

            ret = pLowerPort->AllocIrpCtxExt(
                pIrp->GetTopStack() );

            // save my ctx pos before let
            // the irp go
            guint32 dwPos = pIrp->GetCurPos();

            ret = pLowerPort->SubmitIrp( pIrp );
            if( ret == STATUS_PENDING )
                break;

            // clean up the allocated context
            ret = pIrp->GetTopStack()->GetStatus();
            pIrp->PopCtxStack();
            pIrp->SetCurPos( dwPos );
            pIrp->GetCurCtx()->SetStatus( ret );

            break;
        }
        else
        {
            ret = -ENOTSUP;
        }

    }while( 0 );

    return ret;
}

gint32 CPort::SubmitIrp( IRP* pIrp )
{
    gint32 ret = 0;

    CIrpGateKeeper oGateKeeper( this );
    oGateKeeper.IrpIn( pIrp );

    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }


        // always assume the top context on stack
        // is for current port

        guint32 dwCurPos = pIrp->GetCurPos(); 
        pIrp->SetCurPos( pIrp->GetStackSize() - 1 );

        IrpCtxPtr& pIrpCtx =
            pIrp->GetCurCtx();


        switch( pIrpCtx->GetMajorCmd() )
        {
        case IRP_MJ_PNP:
            {
                ret = SubmitPnpIrp( pIrp );
                break;
            }
        case IRP_MJ_FUNC:
            {
                ret = SubmitFuncIrp( pIrp );
                break;
            }
        case IRP_MJ_GETPROP:
            {
                ret = SubmitGetPropIrp( pIrp );
                break;
            }
        case IRP_MJ_SETPROP:
            {
                ret = SubmitSetPropIrp( pIrp );
                break;
            }
        default:
            {
                ret = PassUnknownIrp( pIrp );
                break;
            }
        }

        if( ret != STATUS_PENDING )
        {
            // touch only non-pendig irp
            pIrp->SetCurPos( dwCurPos );
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        oGateKeeper.IrpOut( pIrp );
    }

    return ret;
}

gint32 CPort::CompleteStartIrp( IRP* pIrp )
{
    gint32 ret = 0;

    // we want to support asynchronous and
    // long start/stop, so the code looks
    // a little bit complicated
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr pCtxPort =
            pIrp->GetCurCtx();

        if( !pIrp->IsIrpHolder() )
        {
            IrpCtxPtr& pCtxLower = pIrp->GetTopStack();

            ret = pCtxLower->GetStatus();
            pCtxPort->SetStatus( ret );
            pIrp->PopCtxStack();
        }
        else
        {
            ret = pCtxPort->GetStatus();
        }

        if( ERROR( ret ) )
        {
            guint32 dwPortState = GetPortState();
            if( dwPortState == PORT_STATE_ATTACHED )
            {
                // switch to starting state so
                // that the port can swith to
                // stopped state later
                SetPortState( PORT_STATE_STARTING );
            }
            break;
        }

        guint32 dwState = 0;
        dwState = GetPnpState( pIrp );

        CStdRMutex a( m_oLock );
        guint32 dwPortState = GetPortState();

        // NOTE: during port start, the port could
        // be physically detached and the stop
        // process can occur simutaneously.
        //
        if( dwPortState != PORT_STATE_ATTACHED &&
            dwPortState != PORT_STATE_STARTING &&
            dwPortState != PORT_STATE_READY )
        {
            // well, something bad happening,
            // maybe the port has got lost
            ret = ERROR_STATE;
            break;
        }

        if( dwPortState == PORT_STATE_ATTACHED
            && dwState == PNP_STATE_START_LOWER )
        {
            SetPortState( PORT_STATE_STARTING );

            // resume the start process
            a.Unlock();
            ret = StartEx( pIrp );
            a.Lock();

            // more processing needed
            if( ret == STATUS_PENDING )
                break;

            dwPortState = GetPortState();
            if( dwPortState != PORT_STATE_STARTING )
            {
                throw std::runtime_error(
                    "Port state machine has gone wrong" );
            }

            // we are still in control
            if( ERROR( ret ) )
            {
                pCtxPort->SetStatus( ret );
                break;
            }
        }

        if( dwPortState == PORT_STATE_STARTING
            && ( dwState == PNP_STATE_START_PRE_SELF
                || dwState == PNP_STATE_START_SELF ) )
        {
            // resume the start process
            a.Unlock();
            ret = StartEx( pIrp );
            a.Lock();

            // more processing needed
            if( ret == STATUS_PENDING )
                break;

            dwPortState = GetPortState();
            if( dwPortState != PORT_STATE_STARTING )
            {
                throw std::runtime_error(
                    "Port state machine has gone wrong while start itself" );
            }

            if( ERROR( ret ) )
            {
                pCtxPort->SetStatus( ret );
                break;
            }
        }

        dwState = GetPnpState( pIrp );
        if( dwPortState == PORT_STATE_STARTING 
            && dwState == PNP_STATE_START_POST_SELF )
        {
            SetPortState( PORT_STATE_READY );

            a.Unlock();
            // we may want to do something after port
            // switched to ready. Override OnPortReady
            // for this purpose
            SetPnpState( pIrp, PNP_STATE_START_PORT_RDY );
            ret = OnPortReady( pIrp );
            a.Lock();

            if( ERROR( ret ) )
                pCtxPort->SetStatus( ret );

            break;
        }

        // FIXME: there is opportunity the port
        // state could be changed away from the
        // PORT_STATE_READY, indicating some other
        // thing is going on.
        dwPortState = GetPortState();
        dwState = GetPnpState( pIrp );
        if( dwPortState == PORT_STATE_READY 
            && dwState == PNP_STATE_START_PORT_RDY )
        {
            pCtxPort->SetStatus( ret );
            break;
        }

        // wrong state encountered
        ret = ERROR_STATE;

    }while( 0 );

    if( ERROR( ret ) )
    {
        guint32 dwPortState = GetPortState();
        if( SetPortStateWake( dwPortState,
            PORT_STATE_STOPPED ) )
        {

            // There could be the situation that
            // the lower port has entered the
            // started state when this port failed
            // to start. And DestroyPortStack will
            // handle this situation.
            //
            // Don't call OnPortStopped here.
            // eventPortStartFailed will be sent
            // in the irp's callback
        }
        else if( dwPortState == PORT_STATE_READY )
        {
            // Possibly failed in OnPortReady().
            // For a bus port, it may be the child
            // port fails to start. Just return
            // the error, let the pnpmgr to stop
            // this port
        }

        OnPortStartFailed( pIrp, ret );
    }
    return ret;
}

#define STOP_PORT_STEP( cur_stat, next_stat ) \
{ \
    dwPnpState = GetPnpState( pIrp ); \
    if( dwPnpState == cur_stat ) \
    { \
        SetPnpState( pIrp, next_stat ); \
        a.Unlock(); \
        ret = StopEx( pIrp ); \
        a.Lock(); \
        dwPortState = GetPortState(); \
        if( dwPortState != PORT_STATE_STOPPING ) \
        { \
            throw std::runtime_error( \
                "State Machine gone wrong" ); \
        } \
        if( ERROR( ret ) ) \
        {\
            pCtxPort->SetStatus( ret );\
            break; \
        }\
        if( ret == STATUS_PENDING ) \
            break; \
    } \
} 

gint32 CPort::CompleteStopIrpEx( IRP* pIrp )
{
    gint32 ret = 0;

    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr pCtxPort =
            pIrp->GetCurCtx();;

        if( !pIrp->IsIrpHolder() )
        {
            IrpCtxPtr pCtxLower = pIrp->GetTopStack();
            ret = pCtxLower->GetStatus();
            pCtxPort->SetStatus( ret );
            pIrp->PopCtxStack();
        }
        else
        {
            ret = pCtxPort->GetStatus();
        }
        
        guint32 dwState = GetPnpState( pIrp );
        if( dwState == PNP_STATE_STOP_PRELOCK )
        {
            pCtxPort->SetStatus( 0 );
            ret = SubmitStopIrp( pIrp );
        }
        else
        {
            ret = CompleteStopIrp( pIrp );
        }

    }while( 0 );

    return ret;
}

gint32 CPort::CompleteStopIrp( IRP* pIrp )
{
    gint32 ret = 0;

    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr pCtxPort =
            pIrp->GetCurCtx();

        if( !pIrp->IsIrpHolder() )
        {
            IrpCtxPtr pCtxLower = pIrp->GetTopStack();
            ret = pCtxLower->GetStatus();
            pCtxPort->SetStatus( ret );
            pIrp->PopCtxStack();
        }
        else
        {
            ret = pCtxPort->GetStatus();
        }

        if( ERROR( ret ) )
        {
            break;
        }
        else
        {
            guint32 dwPnpState;
            CStdRMutex a( GetLock() );
            guint32 dwPortState = GetPortState();
            if( dwPortState != PORT_STATE_STOPPING )
            {
                // don't continue
                ret = ERROR_STATE;
                pCtxPort->SetStatus( ret );
                break;
            }

            // NOTE: the state begin-2-begin
            // indicates a loop for PreStop, which
            // should internally switch the
            // pnpstate to PNP_STATE_STOP_PRE once
            // PreStop is done
            STOP_PORT_STEP( PNP_STATE_STOP_BEGIN,
                PNP_STATE_STOP_BEGIN );

            STOP_PORT_STEP( PNP_STATE_STOP_PRE,
                PNP_STATE_STOP_PRE );

            STOP_PORT_STEP( PNP_STATE_STOP_LOWER,
                PNP_STATE_STOP_LOWER );

            STOP_PORT_STEP( PNP_STATE_STOP_SELF,
                PNP_STATE_STOP_SELF );

            dwPortState = GetPortState();
            if( dwPortState == PORT_STATE_STOPPING
                && dwPnpState == PNP_STATE_STOP_POST )
            {
                // do nothing
            }
            else
            {
                ret = ERROR_STATE;
            }

            pCtxPort->SetStatus( ret );
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        guint32 dwPortState = GetPortState();
        if( !SetPortStateWake( dwPortState,
            PORT_STATE_STOPPED ) )
        {
            DebugMsg( ret, "State Machine gone wrong" );
        }
        else
        {
            OnPortStopped();
        }
    }

    return ret;
}

gint32 CPort::CompleteQueryStopIrp( IRP* pIrp )
{
    gint32 ret = 0;

    if( pIrp == nullptr )
        return -EINVAL;

    IrpCtxPtr& pCtxPort =
        pIrp->GetCurCtx();
    
    guint32 dwQueryState = GetPnpState( pIrp );

    if( dwQueryState == 0 )
    {
        if( !pIrp->IsIrpHolder() )
        {
            ret = pIrp->GetTopStack()->GetStatus();
            pCtxPort->SetStatus( ret );
            pIrp->PopCtxStack();
        }
        else
        {
            // the status shout be set outside the
            // completion routine
        }
        SetPnpState( pIrp, 1 );
        ret = OnQueryStop( pIrp );
    }
    else if( dwQueryState == 1 )
    {
        // the status has already been set
    }
    else
    {
        ret = ERROR_STATE;
    }
    return ret;
}

gint32 CPort::CompletePnpIrp( IRP* pIrp )
{
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtxPort =
            pIrp->GetCurCtx();

        guint32 dwCmd = pCtxPort->GetMinorCmd();
        switch( dwCmd )
        {
        case IRP_MN_PNP_START:
            {
                ret = CompleteStartIrp( pIrp );
                break;
            }
        case IRP_MN_PNP_STOP:
            {
                ret = CompleteStopIrpEx( pIrp );
                break;
            }

        case IRP_MN_PNP_QUERY_STOP:
            {
                ret = CompleteQueryStopIrp( pIrp );
                break;
            }
            
        default:
            {
                ret = -ENOTSUP;
                pCtxPort->SetStatus( ret );

                if( !pIrp->IsIrpHolder() )
                    pIrp->PopCtxStack();

                break;
            }
        }

    }while( 0 );

    return ret;
}

// assoc irp methods
gint32 CPort::MakeAssocIrp(
    IRP* pMaster,
    IRP* pSlave )
{

    gint32 ret = 0;
    do{
        if( pMaster == nullptr || pSlave == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr pCtx = pMaster->GetTopStack();
        if( pCtx->GetMajorCmd() != IRP_MJ_ASSOC ||
            pCtx->GetMinorCmd() != IRP_MN_ASSOC_RUN )
        {
            // let's make a context stack for association
            IPort* pPort = pMaster->GetTopPort();
            if( pPort != this )
            {
                ret = -EINVAL;
                break;
            }

            // NOTE: still that port
            pMaster->AllocNextStack( this );
            pCtx = pMaster->GetTopStack();
            pCtx->SetMajorCmd( IRP_MJ_ASSOC );
            pCtx->SetMinorCmd( IRP_MN_ASSOC_RUN );

            this->AllocIrpCtxExt(
                pMaster->GetTopStack(),
                pMaster );

            pCtx->SetIoDirection( IRP_DIR_IN );
        }

        pSlave->SetMasterIrp( pMaster );
        pMaster->AddSlaveIrp( pSlave );

    }while( 0 );

    return ret;
}

gint32 CPort::CompleteAssocIrp(
    IRP* pMaster, IRP* pSlave )
{
    if( pMaster == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    bool bRollback = true;
    CStdRMutex a( pMaster->GetLock() );
    do{
        ret = pMaster->CanContinue(
            IRP_STATE_COMPLETING_ASSOC );

        if( ERROR( ret ) )
        {
            // probably the pMaster is
            // also being cancelled
            bRollback = false;
            break;
        }

        if( pSlave->GetMasterIrp() != pMaster )
        {
            ret = -EINVAL;
            break;
        }

        pMaster->RemoveSlaveIrp( pSlave );

        if( pMaster->GetTopStack()->GetMinorCmd()
            != IRP_MN_ASSOC_RUN )
        {
            // FIXME: to leave the irp dangling?
            ret = -ENOTSUP;
            break;
        }

        // give the subclass an opportunity 
        // to do some port specific things.
        gint32 iRet = 0;
        if( !pMaster->IsCbOnly() )
            iRet = OnCompleteSlaveIrp(
                pMaster, pSlave );

        if( pMaster->GetSlaveCount() > 0 )
        {
            ret = STATUS_PENDING;
            break;
        }

        // don't touch other guy's irp ctx in
        // this port
        IPort* pTopPort = pMaster->GetTopPort();
        if( pTopPort == ( IPort* )this )
            pMaster->PopCtxStack();

        // set the return code
        pMaster->GetTopStack()->SetStatus( iRet );

        // the irp state should go back to ready
        // before completion
        pMaster->SetState(
            IRP_STATE_COMPLETING_ASSOC,
            IRP_STATE_READY );

        // let's complete the master irp
        a.Unlock();

        // FIXME: is it possible to complete
        // the pMaster here
        //
        ret = GetIoMgr()->CompleteIrp( pMaster );
        bRollback = false;

    }while( 0 );

    if( ret == STATUS_PENDING || ERROR( ret ) )
    {
        if( bRollback )
        {
            pMaster->SetState(
                IRP_STATE_COMPLETING_ASSOC,
                IRP_STATE_READY );
        }
    }
    else if( SUCCEEDED( ret ) )
    {
        // master irp is completed
        // don't touch it any more
    }
    return ret;
}

gint32 CPort::OnCompleteSlaveIrp(
    IRP* pMaster, IRP* pSlave )
{
    return 0;
}

gint32 CPort::CompleteIoctlIrp( IRP* pIrp )
{
    return 0;
}

gint32 CPort::CompleteFuncIrp( IRP* pIrp )
{
    gint32 ret = 0;

    do{
        if( pIrp == nullptr 
           || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }

        if( pIrp->MajorCmd() != IRP_MJ_FUNC )
        {
            ret = -EINVAL;
            break;
        }
        
        switch( pIrp->MinorCmd() )
        {
        case IRP_MN_IOCTL:
            {
                ret = CompleteIoctlIrp( pIrp );
                break;
            }
        default:
            {
                ret = -ENOTSUP;
                break;
            }
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        IrpCtxPtr& pCtx =
            pIrp->GetCurCtx();

        if( !pIrp->IsIrpHolder() )
        {
            IrpCtxPtr& pCtxLower = pIrp->GetTopStack();

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

gint32 CPort::CompleteIrp( IRP* pIrp )
{
    // we need to move the data and the
    // status to the upper layer of
    // IRP_CONTEXT on the stack
    //
	// provide a chance for the caller to
	// complete a request
    //
    // better call this clean up function after
    // the subclass CancelIrp has done with the
    // top stack
    gint32 ret = 0;
    CIrpGateKeeper oGateKeeper( this );

    do{
        if( pIrp == nullptr
            || pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }
        else
        {

            // TODO: release all the resource allocated for the
            // IRP_CONTEXT
            //
            // and copy the return result to the upper level
            //
            // IRP_CONTEXT before pop the stack
            IrpCtxPtr& pIrpCtx =
                pIrp->GetCurCtx();

            switch( pIrpCtx->GetMajorCmd() )
            {
            case IRP_MJ_PNP:
                {
                    ret = CompletePnpIrp( pIrp );
                    break;
                }
            case IRP_MJ_FUNC:
                {
                    ret = CompleteFuncIrp( pIrp );
                    break;
                }
            case IRP_MJ_ASSOC:
                {
                    if( pIrpCtx->GetMinorCmd()
                        == IRP_MN_ASSOC_RUN )
                    {
                        // the next port will clear
                        // this ctx
                        // we should not come here
                        // normally, and the
                        // CompleteAssocIrp will pop
                        // the stack
                        ret = 0;     
                        break;
                    }
                }
            default:
                {
                    ret = -ENOTSUP;
                    pIrpCtx->SetStatus( ret );

                    if( !pIrp->IsIrpHolder() )
                    {
                        pIrp->PopCtxStack();
                    }
                    break;
                }
            }
        }

        // TODO: remove the irp from the internal waiting
        // queue if any
 
    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        oGateKeeper.IrpOut( pIrp );
    }

    return ret;
}

gint32 CPort::CancelStartStopIrp(
    IRP* pIrp, bool bForce, bool bStart )
{

    gint32 ret = 0;
    if( pIrp == nullptr )
        return -EINVAL;

    // we want to support asynchronous and
    // long start/stop, so the code looks
    // a little bit complicated
    do{

        IrpCtxPtr& pCtxPort =
            pIrp->GetCurCtx();

        if( !pIrp->IsIrpHolder() )
        {
            IrpCtxPtr& pCtxLower = pIrp->GetTopStack();
            ret = pCtxLower->GetStatus();
            // we don't pop the stack here
            // it will be poped in CancelIrp

            // we cannot directly set the status
            // because it is not always -ECANCELED, and
            // sometimes it could be -ETIMEDOUT set
            // by timer callback
            pCtxPort->SetStatus( ret );
        }
        else
        {
            ret = pCtxPort->GetStatus();
        }

        // set the port to state 'attached'
        if( bStart )
        {
            ret = SetPortStateWake(
                PORT_STATE_STARTING,
                PORT_STATE_ATTACHED,
                true );
        }
        else
        {
            // FIXME: it is problemic to set the port
            // state to indicate the port has completed
            // the stop command?
            ret = SetPortStateWake(
                PORT_STATE_STOPPING,
                PORT_STATE_STOPPED );
            if( SUCCEEDED( ret ) )
            {
                OnPortStopped();
            }
        }

    }while( 0 );

    return ret;
}

gint32 CPort::CancelStartIrp(
    IRP* pIrp, bool bForce )
{
    if( pIrp == nullptr )
        return -EINVAL;

    return CancelStartStopIrp( pIrp, true, true );
}

gint32 CPort::CancelStopIrp(
    IRP* pIrp, bool bForce )
{
    if( pIrp == nullptr )
        return -EINVAL;

    return CancelStartStopIrp( pIrp, true, false );
}

gint32 CPort::CancelPnpIrp(
    IRP* pIrp, bool bForce )
{
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtxPort =
            pIrp->GetCurCtx();

        guint32 dwCmd =
            pCtxPort->GetMinorCmd();

        switch( dwCmd )
        {
        case IRP_MN_PNP_START:
            {
                ret = CancelStartIrp( pIrp );
                break;
            }
        case IRP_MN_PNP_STOP:
            {
                ret = CancelStopIrp( pIrp );
                break;
            }
        case IRP_MN_PNP_QUERY_STOP:
            {
                break;
            }
        default:
            {
                ret = -ENOTSUP;
                pCtxPort->SetStatus( ret );
                break;
            }
        }
    }while( 0 );

    return ret;
}

gint32 CPort::CancelFuncIrp(
    IRP* pIrp, bool bForce )
{

    gint32 ret = 0;
    if( pIrp == nullptr
        || pIrp->GetStackSize() == 0 )
        return -EINVAL;

    do{
        if( !pIrp->IsIrpHolder() )
        {
            // replicate the status
            pIrp->GetCurCtx()->SetStatus(
                pIrp->GetTopStack()->GetStatus() );

            pIrp->PopCtxStack();
        }

    }while( 0 );

    return ret;
}

gint32 CPort::CancelIrp(
    IRP* pIrp, bool bForce )
{
    //
	// provide a chance for the caller to
	// cancel a request
    //
    // NOTE: There should not be long wait in this
    // routine. and it should be completed in
    // immediately and synchronously only.
    //
    // this is the main entrance for CancelIrp from
    // a port
    //
    gint32 ret = 0;
    CIrpGateKeeper oGateKeeper( this );

    do{
        if( bForce == false && !CanCancelIrp( pIrp ) )
        {
            ret = ERROR_CANNOT_CANCEL;
            break;
        }

        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        if( pIrp->GetStackSize() == 0 )
        {
            ret = -EINVAL;
            break;
        }
        // make sure we are on the top
        // remove the context stack prepared for
        // the next lower port
        //
        // usually we should inspect the content
        // on the stack and move something from
        // stack[ iPos ] to stack[ iPos - 1 ]
        //

        IrpCtxPtr& pCtx =
            pIrp->GetCurCtx();

        switch( pCtx->GetMajorCmd() )
        {
        case IRP_MJ_PNP:
            {
                // FIXME: remove footprint of this
                // irp from the port
                CancelPnpIrp( pIrp );
                break;
            }
        case IRP_MJ_FUNC:
            {
                CancelFuncIrp( pIrp );
                break;
            }
        case IRP_MJ_ASSOC:
            {
                // we should not come here
                // fall through
            }
        default:
            {
                pCtx->SetStatus( -ECANCELED );
                break;
            }
        }

        if( !pIrp->IsIrpHolder() )
        {
            // pop the topmost ctx on the stack
            pIrp->PopCtxStack();
        }

        // TODO:
        // remove the irp from the internal waiting queue if any
 
    }while( 0 );

    if( ret != STATUS_PENDING )
    {
        oGateKeeper.IrpOut( pIrp );
    }

    return ret;
}

gint32 CPort::GetStackPos() const
{
    gint32 iPos = 0;
    IPort* pPort = GetUpperPort();
    while( pPort != nullptr )
    {
        iPos++;
        pPort = pPort->GetUpperPort();
    }
    return iPos;
}

PortPtr CPort::GetUpperPort() const
{
    CCfgOpenerObj a( this );

    ObjPtr pObj;
    gint32 ret = a.GetObjPtr(
        propUpperPortPtr, pObj );

    if( ERROR( ret ) )
        return PortPtr();

    return PortPtr( pObj );
}

PortPtr CPort::GetLowerPort() const
{
    CCfgOpenerObj a( this );

    ObjPtr pObj;
    gint32 ret = a.GetObjPtr(
        propLowerPortPtr, pObj );

    if( ERROR( ret ) )
        return PortPtr();

    return PortPtr( pObj );
}

gint32 CPort::FindPortByType(
    guint32 dwPortType,
    PortPtr& pPortRet ) const
{
    gint32 ret = -ENOENT;
    CPort* pPort = nullptr;
    do{
        if( dwPortType == PORTFLG_TYPE_PDO ||
            dwPortType == PORTFLG_TYPE_FDO )
        {
            pPort = static_cast< CPort* >
                ( GetBottomPort() );

            while( pPort != nullptr )
            {
                guint32 dwType =
                    pPort->GetPortType();    
                if( dwType == dwPortType )
                {
                    pPortRet = pPort;
                    ret = 0;
                    break;
                }
                pPort = static_cast< CPort* >
                    ( pPort->GetUpperPort() );
            }
        }
        else if( dwPortType == PORTFLG_TYPE_BUS )
        {
            pPort = static_cast< CPort* >
                ( GetBottomPort() );

            CCfgOpenerObj oPortCfg( pPort );
            ObjPtr pBus;

            ret = oPortCfg.GetObjPtr(
                propBusPortPtr, pBus );
            if( ERROR( ret ) )
                break;

            pPortRet = pBus;
            if( !pPortRet.IsEmpty() )
                ret = 0;
        }
        else if( dwPortType == PORTFLG_TYPE_FIDO )
        {
            ret = -ENOTSUP;
        }
        else if( dwPortType == PORTFLG_TYPE_INVALID )
        {
            ret = -EINVAL;
        }

    }while( 0 );

    return ret;
}

gint32 CPort::CanContinue(
	IRP* pIrp,
    guint32 dwNewState,
    guint32* pdwOldState )
{
    // gate for SubmitFuncIrp
    // and SubmitStopIrp
    return m_pPortState->CanContinue(
        pIrp, dwNewState, pdwOldState );
}

bool CPort::SetPortStateWake(
	guint32 dwCurState,
    guint32 dwNewState,
    bool bWakeAll )
{
    return m_pPortState->SetPortStateWake(
        dwCurState, dwNewState, bWakeAll );
}

gint32 CPort::OnEvent(
    EnumEventId iEvent,
    LONGWORD dwParam1,
    LONGWORD dwParam2,
    LONGWORD* pData )
{ return 0;}

gint32 CPort::AttachToPort( IPort* pLowerPort )
{
    gint32 ret = 0;
    if( pLowerPort == nullptr )
        return -EINVAL;

    CCfgOpenerObj a( pLowerPort );
    ret = a.SetObjPtr(
        propUpperPortPtr, ObjPtr( this ) );

    CCfgOpenerObj b( this );
    ret = b.SetObjPtr(
        propLowerPortPtr, ObjPtr( pLowerPort ) );

    return ret;
}

gint32 CPort::DetachFromPort( IPort* pLowerPort )
{
    gint32 ret = 0;
    if( pLowerPort == nullptr )
        return -EINVAL;

    CCfgOpenerObj a( pLowerPort );
    ret = a.RemoveProperty( propUpperPortPtr );

    CCfgOpenerObj b( this );
    ret = b.RemoveProperty( propLowerPortPtr );

    return ret;
}

PortPtr CPort::GetTopmostPort() const
{
    PortPtr pCur( ( IPort* ) this );
    PortPtr pNext = pCur;
    while( !pNext.IsEmpty() )
    {
        pCur = pNext;
        pNext = pCur->GetUpperPort();
    }
    return pCur;
}

PortPtr CPort::GetBottomPort() const
{
    PortPtr pCur( ( IPort* ) this );
    PortPtr pNext = pCur;
    while( !pNext.IsEmpty() )
    {
        pCur = pNext;
        pNext = pCur->GetLowerPort();
    }
    return pCur;
}

gint32 CPort::OnPortStackDestroy( IRP* pIrp )
{
    gint32 ret = 0;
    if( GetLowerPort().IsEmpty() )
    {
        ret = GetIoMgr()->RemoveFromHandleMap( this );    
    }
    return ret;
}

gint32 CPort::OnPortStackBuilt( IRP* pIrp )
{
    gint32 ret = 0;
    if( GetLowerPort().IsEmpty() )
    {
        ret = GetIoMgr()->AddToHandleMap( this );    
    }
    return ret;
}

std::string CPort::ExClassToInClass(
    const std::string& strPortName )
{
    string strClass = strPortName;
    strClass.insert( 0, 1, 'C' );
    return strClass;
}

std::string CPort::InClassToExClass(
    const std::string& strPortName )
{
    return strPortName.substr( 1 );
}

/**
* @name CPort::CanAcceptMsg @{ test if the
* port state is valid for receiving incoming
* messages */

/** Parameter:
 *
 *  dwPortState: the port state to test. which can
 *  be obtained by GetPortState()
 * @} */

bool CPort::CanAcceptMsg(
    guint32 dwPortState )
{
    return ( dwPortState == PORT_STATE_READY
        || dwPortState == PORT_STATE_BUSY
        || dwPortState == PORT_STATE_BUSY_SHARED
        || dwPortState == PORT_STATE_BUSY_RESUME );
}

// this route is called before the port goes to
// stopping state
gint32 CGenericBusPort::OnPrepareStop( IRP* pIrp )
{
    if( pIrp == nullptr ||
        pIrp->GetStackSize() == 0 )
        return -EINVAL;
    return StopChild( pIrp );
}

CGenericBusPort::CGenericBusPort(
    const IConfigDb* pConfig )
    :super( pConfig ),
     m_atmPdoId( 1 )
{
    gint32 ret = 0;

    do{
        if( pConfig == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CCfgOpener a( pConfig );

        // init some data members
        m_dwFlags =
            ( PORTFLG_TYPE_FDO | PORTFLG_TYPE_BUS );

        string strBusPath;
        string strBusPortName;

        ret = a.GetStrProp(
            propPortName, strBusPortName );

        if( ERROR( ret ) )
            break;

        GetIoMgr()->MakeBusRegPath(
            strBusPath, strBusPortName );

        // add the reg path for this bus port to
        // the config db
        CCfgOpener b( ( IConfigDb* )m_pCfgDb );
        b.SetStrProp( propRegPath, strBusPath );

    }while( 0 );

    if( ERROR( ret ) )
    {
        string strMsg = DebugMsg( ret,
            "Error occurs in CGenericBusPort constructor" );  
        throw runtime_error( strMsg );
    }
}

void CGenericBusPort::AddPdoPort(
    guint32 iPortId, PortPtr& portPtr )
{
    if( m_mapId2Pdo.find( iPortId ) !=
        m_mapId2Pdo.end() )
        return;

    m_mapId2Pdo[ iPortId ] = portPtr;
    
    TaskletPtr pTask;
    m_mapPort2TaskGrp[ portPtr ] = pTask;
}

void CGenericBusPort::RemovePdoPort(
    guint32 iPortId )
{
    if(m_mapId2Pdo.find( iPortId )
        == m_mapId2Pdo.end() )
        return;

    PortPtr pPort = m_mapId2Pdo[ iPortId ];
    m_mapId2Pdo.erase( iPortId );

    std::map< PortPtr, TaskletPtr >::iterator
        itr = m_mapPort2TaskGrp.find( pPort );
    if( itr == m_mapPort2TaskGrp.end() )
        return;

    // NOTE: don't cancel the taskgroup, and let
    // it complete
    m_mapPort2TaskGrp.erase( itr );
}

gint32 CGenericBusPort::GetPdoPort(
    guint32 iPortId, PortPtr& portPtr ) const
{
    map<guint32, PortPtr>::const_iterator itr
        = m_mapId2Pdo.find( iPortId );
        if( itr == m_mapId2Pdo.cend() )
            return -ENOENT;

    portPtr = itr->second;
    return 0;
}

gint32 CGenericBusPort::GetChildPorts(
        vector< PortPtr >& vecChildPorts )
{
    map<guint32, PortPtr>::iterator itr
        = m_mapId2Pdo.begin();
    while( itr != m_mapId2Pdo.cend() )
    {
        vecChildPorts.push_back( itr->second );
        ++itr;
    }

    return 0;
}

bool CGenericBusPort::PortExist(
    guint32 iPortId ) const
{
    return m_mapId2Pdo.find( iPortId )
        != m_mapId2Pdo.end();
}

// handler on port destroy
gint32 CGenericBusPort::OnPortStackDestroy(
    IRP* pIrp )
{
    return super::OnPortStackDestroy( pIrp );
}

gint32 CPortAttachedNotifTask::operator()(
    guint32 dwContext )
{
    gint32 ret = 0;

    do{
        ObjPtr pObj;
        CParamList a( m_pCtx );

        ret = a.Pop( pObj );
        if( ERROR( ret ) )
            break;

        EvtMapPtr pEventMap = ( CStlEventMap* )( pObj );

        ret = a.Pop( pObj );
        if( ERROR( ret ) )
            break;

        PortPtr pNewPort = ( IPort* )pObj;
        if( pNewPort.IsEmpty() )
        {
            ret = -EINVAL;
            break;
        }

        guint32 dwEventId;
        ret = a.Pop( dwEventId );
        if( ERROR( ret ) )
            break;

        if( a.exist( propMasterIrp) )
        {
            CCfgOpenerObj oPortCfg(
                ( CObjBase* )pNewPort );

            oPortCfg.CopyProp(
                propMasterIrp, m_pCtx );

            a.RemoveProperty( propMasterIrp );
        }

        pEventMap->BroadcastEvent(
            eventPnp,
            dwEventId,
            PortToHandle( ( IPort* )pNewPort ),
            nullptr);

    }while( 0 );

    return ret;
}

gint32 CGenericBusPort::SchedulePortAttachNotifTask(
    IPort* pNewPort,
    guint32 dwEventId,
    IRP* pMasterIrp,
    bool bImmediately )
{
    gint32 ret = 0;
    do{
        CParamList a;

        // NOTE: at this moment, the pNewPort is
        // not registered in the registry so no
        // event map for it yet.  we will
        // broadcast the event via the event map
        // from the bus port
        ObjPtr pObj( pNewPort );
        a.Push( dwEventId );
        a.Push( pObj );

        CEventMapHelper< CGenericBusPort > b( this );
        ret = b.GetEventMap( pObj );

        if( ERROR( ret ) )
            break;

        a.Push( pObj );

        if( pMasterIrp != nullptr )
        {
            a.SetPointer( propMasterIrp,
                pMasterIrp );
        }

        if( !bImmediately )
        {
            ret = GetIoMgr()->ScheduleTask(
                clsid( CPortAttachedNotifTask ),
                a.GetCfg() );

            if( SUCCEEDED( ret ) )
                ret = STATUS_PENDING;
        }
        else
        {
            TaskletPtr pTask;

            pTask.NewObj(
                clsid( CPortAttachedNotifTask ), a.GetCfg() );

            ret = ( *pTask )( dwEventId );
        }

    }while( 0 );

    return ret;
}

gint32 CGenericBusPort::EnumPdoPorts(
    vector<PortPtr>& vecPorts )
{

    CStdRMutex oPortLock( GetLock() );
	map<guint32, PortPtr>::iterator
        itr = m_mapId2Pdo.begin();

    while( itr != m_mapId2Pdo.end() )
    {
        vecPorts.push_back( itr->second );
        ++itr;
    }
    return 0;
}

gint32 CGenericBusPort::EnumPdoClasses(
    StrSetPtr& psetStrings ) const
{
    gint32 ret = 0;
    if( psetStrings.IsEmpty() )
    {
        ret = psetStrings.NewObj();
        if( ERROR( ret ) )
            return ret;
    }
    do{
        std::string strBusClass;
        CCfgOpenerObj oPortCfg( this );
        ret = oPortCfg.GetStrProp(
            propPortClass, strBusClass );
        if( ERROR( ret ) )
            break;

        CIoManager* pMgr = GetIoMgr();
        CDriverManager& oDrvMgr = pMgr->GetDrvMgr();
        Json::Value& ojc = oDrvMgr.GetJsonCfg();
        Json::Value& oPorts = ojc[ JSON_ATTR_PORTS ];

        if( oPorts == Json::Value::null )
        {
            ret = -ENOENT;
            break;
        }

        if( !oPorts.isArray() || oPorts.size() == 0 )
        {
            ret = -ENOENT;
            break;
        }

        for( guint32 i = 0; i < oPorts.size(); i++ )
        {
            Json::Value& elem = oPorts[ i ];
            if( elem == Json::Value::null )
                continue;

            if( !elem.isMember( JSON_ATTR_BUSPORTCLASS ) ||
                !elem[ JSON_ATTR_BUSPORTCLASS ].isString() )
                continue;

            std::string strBusClass2 =
                elem[ JSON_ATTR_BUSPORTCLASS ].asString();
            if( strBusClass2.empty() )
                continue;

            if( strBusClass2 != strBusClass )
                continue;

            if( !elem.isMember( JSON_ATTR_PORTTYPE ) ||
                !elem[ JSON_ATTR_PORTTYPE ].isString() )
            {
                ret = -EINVAL;
                break;
            }

            std::string strPortType =
                elem[ JSON_ATTR_PORTTYPE ].asString();

            if( strPortType.empty() )
            {
                ret = -EINVAL;
                break;
            }
            if( strPortType != "Pdo" )
            {
                ret = -EINVAL;
                break;
            }

            if( !elem.isMember( JSON_ATTR_PORTCLASS ) ||
                !elem[ JSON_ATTR_PORTCLASS ].isString() )
            {
                ret = -EINVAL;
                break;
            }

            std::string strPortClass =
                elem[ JSON_ATTR_PORTCLASS ].asString();
            if( strPortClass.empty() )
            {
                ret = -EINVAL;
                break;
            }

            ( *psetStrings )().insert( strPortClass );
        }

        if( ( *psetStrings )().empty() )
            ret = -ENOENT;

    }while( 0 );

    return ret;
}

gint32 CGenericBusPort::PreStop( IRP* pIrp )
{
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtx = pIrp->GetCurCtx();

        BufPtr pBuf;
        pCtx->GetExtBuf( pBuf );
        CGenBusPnpExt* pExt =
            ( CGenBusPnpExt* )( *pBuf );

        if( pExt->m_dwExtState == 0 )
        {
            pExt->m_dwExtState = 1;
            if( ret == STATUS_PENDING )
            {
                break;
            }
            // fall through
        }

        if( pExt->m_dwExtState == 1 )
        {
            ret = super::PreStop( pIrp );

            if( ERROR( ret ) )
                break;

            pExt->m_dwExtState = 2;

            if( ret ==
                STATUS_MORE_PROCESS_NEEDED )
                break;
            // fall through
        }

        if( pExt->m_dwExtState == 2 )
        {
            SetPnpState( pIrp,
                PNP_STATE_STOP_PRE );
            ret = 0;
            break;
        }

        // we don't expect the PreStop
        // will be entered again
        ret = ERROR_STATE;
        break;

    }while( 0 );

    if( ret == STATUS_PENDING )
        ret = STATUS_MORE_PROCESS_NEEDED;

    return ret;
}

gint32 CGenBusPortStopChildTask::RunTask()
{
    // this is a retriable task
    gint32 ret = 0;
    // bool bRetry = false;
    // bool bRevisit = false;
    do{
        CIoManager* pMgr = nullptr;
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        CGenericBusPortEx* pBusPort = nullptr;
        ret = oCfg.GetPointer(
            propPortPtr, pBusPort );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = oCfg.GetObjPtr( propPortPtrs, pObj );
        if( ERROR( ret ) )
            break;

        // we don't check the port state because we
        // are sure in the stopping port state, an
        // exclusive state
        PortVecPtr pPorts = pObj;
        vector< PortPtr >& vecChildPorts = ( *pPorts )();
        gint32 iPortCount = vecChildPorts.size();

        vector< PortPtr > vecPortRevisit;

        CParamList oParams;
        oParams.SetPointer( propIoMgr, pMgr );
        TaskGrpPtr pTaskGrp;
        ret = pTaskGrp.NewObj( 
            clsid( CIfParallelTaskGrp ),
            oParams.GetCfg() );

        for( gint32 i = iPortCount - 1; i >= 0; --i )
        {
            TaskletPtr pStopTask;
            ret = DEFER_OBJCALL_NOSCHED2(
                1, pStopTask, pBusPort,
                &CGenericBusPortEx::DestroyPortSynced,
                vecChildPorts.back(), nullptr );
            if( ERROR( ret ) )
                continue;

            pTaskGrp->AppendTask( pStopTask );
            vecChildPorts.pop_back();
        }

        if( pTaskGrp->GetTaskCount() > 0 )
        {
            CIfRetryTask* pRetryTask = pTaskGrp;
            pRetryTask->SetClientNotify( this );
            TaskletPtr pTask( pRetryTask );
            ret = pMgr->RescheduleTask( pTask );
            if( ERROR( ret ) )
            {
                ( *pTask )( eventCancelTask );
                break;
            }
            ret = STATUS_PENDING;
        }

    }while( 0 );

    if( ret != STATUS_PENDING )
        OnTaskComplete( ret );

    return ret;
}

gint32 CGenBusPortStopChildTask::OnTaskComplete(
    gint32 iRet )
{
    gint32 ret = 0;
    do{
        CIoManager* pMgr = nullptr;
        CCfgOpener oCfg(
            ( IConfigDb* )GetConfig() );

        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        IRP* pIrp;

        ret =oCfg.GetPointer( propIrpPtr, pIrp );
        if( ERROR( ret ) )
            break;

        CStdRMutex oIrpLock( pIrp->GetLock() );
        ret = pIrp->CanContinue(IRP_STATE_READY );
        if( ERROR( ret ) )
            break;

        IrpCtxPtr pTopCtx = pIrp->GetTopStack();
        pTopCtx->SetStatus( iRet );

        oIrpLock.Unlock();

        pMgr->CompleteIrp( pIrp ); 

    }while( 0 );

    return ret;
}

gint32 CGenericBusPort::StopChild( IRP* pIrp )
{
    // stop all the child PDOs the bus port has
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        // we cannot run on the main thread

        PortVecPtr pPorts( true );
        vector<PortPtr>& vecChildPorts = ( *pPorts )();

        CCfgOpener oCfg;
        ret = oCfg.SetPointer(
            propIoMgr, GetIoMgr() );

        if( ERROR( ret ) )
            break;


        // let's stop all the pdo before we stop this port

        if( true )
        {
            CStdRMutex oPortLock( GetLock() );
            ret = GetChildPorts( vecChildPorts );
        }

        if( vecChildPorts.empty() )
            break;

        oCfg.SetPointer( propPortPtr, this );
        oCfg.SetPointer( propIrpPtr, pIrp );
        oCfg.SetObjPtr( propPortPtrs, pPorts );

        oCfg.SetIntProp( propIntervalSec,
            BUSPORT_STOP_RETRY_SEC ); 

        oCfg.SetIntProp( propRetries,
            BUSPORT_STOP_RETRY_TIMES ); 

        // we have to schedule a task to release
        // the current lock on the irp otherwise,
        // the locked irp cannot be released to
        // allow MakeAssociation
        //
        // actually it is not an issue because we
        // are using recursive lock
        ret = GetIoMgr()->ScheduleTask(
            clsid( CGenBusPortStopChildTask ),
            oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        ret = STATUS_PENDING;

    }while( 0 );

    return ret;
}


gint32 CGenericBusPort::AllocIrpCtxExt(
    IrpCtxPtr& pIrpCtx,
    void* pContext ) const 
{
    gint32 ret = -ENOTSUP;
    switch( pIrpCtx->GetMajorCmd() )
    {
    case IRP_MJ_PNP:
        {
            switch( pIrpCtx->GetMinorCmd() )
            {
            case IRP_MN_PNP_START:
            case IRP_MN_PNP_STOP:
            case IRP_MN_PNP_QUERY_STOP:
                {
                    CGenBusPnpExt oExt;

                    oExt.m_dwState = 0;
                    oExt.m_dwExtState = 0;

                    BufPtr pBuf( true );
                    *pBuf = oExt;
                    ret = pIrpCtx->SetExtBuf( pBuf );
                    break;
                }
            default:
                break;
            }

        }

    default:
        break;
    }

    if( ERROR( ret ) )
    {
        // let's try it on CPort
        ret = super::AllocIrpCtxExt(
            pIrpCtx, pContext );
    }

    return ret;

}

gint32 CGenericBusPort::OpenPdoPort(
    const IConfigDb* pConstCfg,
    PortPtr& pNewPort )
{
    // this method could be called in two ways
    // 1. called from the user request to open
    // a port
    //
    // 2. called from within the dbus bus port
    // when itself is fully started, and the
    // must-have localdbuspdo and loopbackpdo
    // will created via this call
    if( pConstCfg == nullptr )
        return -EINVAL;

    gint32 ret = 0;

    do{
        try{
            CfgPtr pCfg( true );
            string strBusName;
            string strPath;
            string strPortName;

            CStdRMutex oPortLock( GetLock() );
            guint32 dwState = GetPortState();
            if( dwState == PORT_STATE_BUSY ||
                dwState == PORT_STATE_BUSY_SHARED )
            {
                // notify to retry later
                ret = -EAGAIN;
                break;
            }
            else if( dwState != PORT_STATE_READY &&
                dwState != PORT_STATE_STARTING )
            {
                ret = ERROR_STATE;
                break;
            }

            HANDLE hPort = INVALID_HANDLE;

            CCfgOpener oExtCfg;
            *oExtCfg.GetCfg() = *pConstCfg;

            // first let's try if we can open the
            // port with the port name
            ret = BuildPdoPortName(
                oExtCfg.GetCfg(), strPortName );

            if( ERROR( ret ) )
                break;

            ret = oExtCfg.GetStrProp(
                propBusName, strBusName );

            if( ERROR( ret ) )
                break;

            ret = GetIoMgr()->MakeBusRegPath(
                strPath, strBusName );

            if( ERROR( ret ) )
                break;

            strPath += string( "/" ) + strPortName;

            ret = GetIoMgr()->OpenPortByPath(
                strPath, hPort );

            if( SUCCEEDED( ret ) )
            {
                pNewPort = HandleToPort( hPort );
                // special return code, indicating
                // port is opened already
                ret = EEXIST;
                break;
            }
            else if( ret != -ENOENT )
            {
                // something is wrong
                break;
            }
            else
            {
                // start to creat a pdo
                ret = 0;
            }

            // prevent the interface ptr to go into
            // port's config.
            oExtCfg.RemoveProperty( propIfPtr );

            // let's retrieve some must-have properties
            // from the config
            ret = oExtCfg.SetStrProp(
                propPortName, strPortName );

            // port path
            ret = oExtCfg.SetStrProp(
                propRegPath, strPath );

            // the bus name
            ret = oExtCfg.SetStrProp(
                propBusName, strBusName );

            // pass a pointer to CIoManager
            oExtCfg.SetPointer( propIoMgr, GetIoMgr() );

            // param parent port
            ret = oExtCfg.SetObjPtr(
                propBusPortPtr, ObjPtr( this ) );

            // well, not exist, let's do some hard
            // work to create the pdo

            ret = CreatePdoPort(
                oExtCfg.GetCfg(), pNewPort );

            if( SUCCEEDED( ret ) )
            {

                CCfgOpenerObj oPortCfg( ( CObjBase* )pNewPort );
                if( oPortCfg.exist( propPortId ) )
                {
                    guint32 dwPortId = -1;
                    oPortCfg.GetIntProp( propPortId,
                        dwPortId );
                    
                    // attach the port to the bus port
                    PortPtr portPtr( pNewPort );
                    this->AddPdoPort( dwPortId, portPtr );
                }

                if( dwState == PORT_STATE_READY )
                {
                    oPortLock.Unlock();

                    // at this moment, we are OK to
                    // release the lock so that if
                    // `pnp stop' comes, it can find
                    // this port and stop the children
                    // port. trigger an event to pnp
                    // manager
                    //
                    // and possibly we are on a
                    // calling task thread 
                    // 
                    // Removed immediate call of the
                    // task because which makes the
                    // situation complicated for the
                    // interface to handle when the
                    // port is started. one delimma is
                    // that the in the immediate case,
                    // the eventPortStarted is sent
                    // before success is returned,
                    // which makes the interface
                    // weired.
                    //
                    // so for now if the port is not
                    // created, the OpenPortByCfg will
                    // always return STATUS_PENDING
                    // and waiting for the portStarted
                    // Event
                    ret = SchedulePortAttachNotifTask(
                        pNewPort, eventPortAttached );
                }

                break;
            }
            else if( ret == -EEXIST )
            {
                // inform the user to try again or
                // waiting for the PortStarted
                // event
                ret = -EAGAIN;
            }
        }
        catch( std::invalid_argument& e )
        {
            ret = -EINVAL;
        }
    }
    while( 0 );

    return ret;
}

gint32 CBusPortStopSingleChildTask::OnIrpComplete(
    PIRP pIrp )
{
    gint32 ret = 0;
    CParamList oCfg(
        ( IConfigDb* )GetConfig() );
    do{
        CIoManager* pMgr = nullptr;
        ret = oCfg.GetPointer( propIoMgr, pMgr );
        if( ERROR( ret ) )
            break;

        IPort* pPort = nullptr;
        ret = oCfg.GetPointer( 0, pPort );
        if( ERROR( ret ) )
            break;

        CPnpManager& oPnpMgr = pMgr->GetPnpMgr();
        oPnpMgr.DestroyPortStack( pPort );

        IEventSink* pCallback = nullptr;
        ret = oCfg.GetPointer( 2, pCallback );
        if( ERROR( ret ) )
            break;

        LONGWORD dwParam2 = 0;
        ret = oCfg.GetIntPtr( 3,
            ( guint32*& )dwParam2 );

        if( ERROR( ret ) )
            break;

        LONGWORD dwParam1 =
            ( LONGWORD )( ( IRP* )pIrp );

        // run the original callback
        pCallback->OnEvent( eventIrpComp,
            dwParam1, dwParam2, nullptr );

    }while( 0 );

    oCfg.ClearParams();
    return ret;
}

gint32 CBusPortStopSingleChildTask::RunTask()
{
    // this is a retriable task
    gint32 ret = 0;
    CParamList oCfg(
        ( IConfigDb* )GetConfig() );

    do{
        CIoManager* pMgr = nullptr;

        ret = oCfg.GetPointer(
            propIoMgr, pMgr );

        if( ERROR( ret ) )
            break;

        IPort* pPort = nullptr;
        ret = oCfg.GetPointer( 0, pPort );
        if( ERROR( ret ) )
            break;

        PIRP pIrp = nullptr; 
        ret = oCfg.GetPointer( 1, pIrp );
        if( ERROR( ret ) )
            break;

        CStdRMutex oIrpLock( pIrp->GetLock() );
        ret = pIrp->CanContinue( IRP_STATE_READY );
        if( ERROR( ret ) )
            break;

        EventPtr pIrpCb = pIrp->m_pCallback;
        LONGWORD dwContext = pIrp->m_dwContext;

        if( !pIrpCb.IsEmpty() )
        {
            oCfg.Push( ObjPtr( pIrpCb ) );
            oCfg.Push( dwContext );
        }

        // intercept the callback
        pIrp->SetCallback( this, 0 );

        CPnpManager& oPnpMgr = pMgr->GetPnpMgr();
        ret = oPnpMgr.StopPortStack(pPort, pIrp );

        // to avoid other contender to complete or
        // cancel the irp before we are done
        oIrpLock.Unlock();

        // the irp completion will be called
        if( ret == STATUS_PENDING )
            break;

        if( ERROR( ret ) )
        {
            DebugPrint( ret,
            "Failed to stop the port gracefully "
            "proceed to complete the master irp "
            "anyway" );
        }
        else
        {
            oPnpMgr.DestroyPortStack( pPort );
        }

        oIrpLock.Lock();

        gint32 iRet =
            pIrp->CanContinue( IRP_STATE_READY );

        if( ERROR( iRet ) )
            break;

        // recover the original callback
        pIrp->SetCallback( pIrpCb, dwContext );
        IrpCtxPtr& pIrpCtx = pIrp->GetTopStack();

        pIrpCtx->SetStatus( ret );
        oIrpLock.Unlock();

        pMgr->CompleteIrp( pIrp );
        break;

    }while( 0 );

    return ret;
}

gint32 CGenericBusPort::ClosePdoPort(
    PIRP pIrp, IPort* pPort )
{
    if( pPort == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    guint32 dwOldState = 0;

    ret = CanContinue( pIrp,
        PORT_STATE_BUSY_SHARED, &dwOldState );

    if( ERROR( ret ) )
        return ret;

    do{
        CPort* pPdoPort =
            ( CPort* )pPort->GetBottomPort();

        bool bPdo = false;
        while( !bPdo && pPdoPort != nullptr )
        {
            CCfgOpenerObj oPortCfg( pPdoPort ); 

            guint32 dwPortType =
                pPdoPort->GetPortType();
            if( dwPortType == PORTFLG_TYPE_PDO )
            {
                bPdo = true;
                break;
            }
            if( dwPortType == PORTFLG_TYPE_INVALID )
            {
                ret = -EINVAL;
                break;
            }
            pPdoPort = ( CPort* )
                pPdoPort->GetUpperPort();
        }

        if( ERROR( ret ) )
            break;

        if( !bPdo )
        {
            ret = -ENOENT;
            break;
        }

        CCfgOpenerObj oPortCfg( pPdoPort );
        guint32 dwPortId = 0;
        ret = oPortCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            break;

        if( true )
        {
            CStdRMutex oPortLock( GetLock() );
            if( !PortExist( dwPortId ) )
            {
                ret = -ENOENT;
                break;
            }
        }

        CIoManager* pMgr = GetIoMgr();
        CParamList oParams;
        oParams.SetPointer( propIoMgr, pMgr );
        oParams.Push( ObjPtr( pPort ) );
        oParams.Push( ObjPtr( pIrp ) );

        TaskletPtr pTask;
        ret = pTask.NewObj(
            clsid( CBusPortStopSingleChildTask ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

        ret = AddStartStopPortTask( pPort, pTask );
        if( ERROR( ret ) )
            ( *pTask )( eventCancelTask );

    }while( 0 );

    PopState( dwOldState );

    return ret;
}

gint32 CGenericBusPort::SubmitPnpIrp(
    IRP* pIrp )
{
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pIrpCtx =
            pIrp->GetTopStack();

        guint32 dwCmd = pIrpCtx->GetMinorCmd();

        switch( dwCmd )
        {
        case IRP_MN_PNP_STOP_CHILD:
            {
                // let's first start the lower port
                if( pIrpCtx->m_pReqData.IsEmpty() )
                {
                    ret = -EINVAL;
                    break;
                }
                ObjPtr pObj = *pIrpCtx->m_pReqData;
                IPort* pPort = pObj;
                if( pPort == nullptr )
                {
                    ret = -EINVAL;
                    break;
                }
                ret = ClosePdoPort( pIrp, pPort );
                break;
            }
        default:
            {
                ret = super::SubmitPnpIrp( pIrp );
                break;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CGenericBusPort::CompletePnpIrp(
    IRP* pIrp )
{
    gint32 ret = 0;
    do{
        if( pIrp == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        IrpCtxPtr& pCtxPort =
            pIrp->GetCurCtx();

        guint32 dwCmd = pCtxPort->GetMinorCmd();
        switch( dwCmd )
        {
        case IRP_MN_PNP_STOP_CHILD:
            {
                ret = pCtxPort->GetStatus();
                break;
            }
        default:
            {
                ret = super::CompletePnpIrp( pIrp );
                break;
            }
        }

    }while( 0 );

    return ret;
}

}
