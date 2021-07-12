/*
 * =====================================================================================
 *
 *       Filename:  dbushook.cpp
 *
 *    Description:  implementation of CDBusLoopHooks for evloop
 *
 *        Version:  1.0
 *        Created:  09/26/2018 01:04:16 PM
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

#include <stdio.h>
#include <vector>
#include <string>
#include <unistd.h>
#include <functional>
#include  <sys/syscall.h>
#include "configdb.h"
#include "frmwrk.h"
#include "mainloop.h"
#include "ifhelper.h"

#ifdef _USE_LIBEV
#include "evloop.h"

namespace rpcf
{

CDBusLoopHookCb::CDBusLoopHookCb(
    const IConfigDb* pCfg )
    : super( pCfg ),
    m_hWatch( INVALID_HANDLE )
{
    gint32 ret = 0;
    do{
        if( pCfg == nullptr )
        {
            ret = -EINVAL;
            break;
        }

        CParamList oParams(
            ( IConfigDb* )GetConfig() );

        guint32* pdwval = ( guint32* )&m_iType;
        ret = oParams.Pop( *pdwval );
        if( ERROR( ret ) )
            break;

        ObjPtr pObj;
        ret = oParams.Pop( pObj );
        if( ERROR( ret ) )
            break;

        m_pParent = pObj;
        if( m_pParent == nullptr )
        {
            ret = -EFAULT;
            break;
        }

    }while( 0 );

    if( ERROR( ret ) )
    {
        throw std::invalid_argument(
            DebugMsg( ret,
            "Error occurs in CDBusLoopHookCb's ctor" ) );
    }
}

CDBusLoopHookCb::~CDBusLoopHookCb()
{
    if( m_hWatch == INVALID_HANDLE )
        return;
    if( m_pParent == nullptr )
        return;
    CEvLoop* pLoop = m_pParent->GetLoop();
    if( pLoop == nullptr )
        return;
    pLoop->RemoveSource( m_hWatch, m_iType );
}

gint32 CDBusLoopHookCb::Stop()
{
    CEvLoop* pLoop = m_pParent->GetLoop();
    pLoop->StopSource( m_hWatch, m_iType );
    return 0;
}

gint32 CDBusTimerCallback::DoAddWatch(
    bool bEnabled )
{
    gint32 ret = 0;
    do{
        HANDLE hWatch = INVALID_HANDLE;
        CEvLoop* pLoop = m_pParent->GetLoop();
        TaskletPtr pTask( this );
        ret = pLoop->AddTimerWatch(
            pTask, hWatch );
        if( ERROR( ret ) )
            break;
        m_hWatch = hWatch;

        if( bEnabled )
        {
            ret = pLoop->StartSource(
                m_hWatch, srcTimer );
            if( ERROR( ret ) )
                break;
            m_pParent->AddWatch(
                pTask, srcTimer );
        }

    }while( 0 );

    return ret;
}
dbus_bool_t CDBusTimerCallback::AddWatch(
    DBusTimeout* ptw, void* data )
{
    if( data == nullptr )
        return false;

    CDBusLoopHooks* pHook =
        ( CDBusLoopHooks*) data;

    gint32 ret = 0;
    do{
        CParamList oParams;
        oParams.Push( ObjPtr( pHook ) );
        oParams.Push( srcTimer );

        TaskletPtr pTask;

        pTask.NewObj(
            clsid( CDBusTimerCallback ),
            oParams.GetCfg() );
        
        guint32 dwIntervalMs =
            dbus_timeout_get_interval( ptw );

        CDBusTimerCallback* pThis = pTask;
        if( pThis == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CParamList oTaskCfg( ( IConfigDb* )
            pThis->GetConfig() );

        // prepare parameters for AddTimerWatch
        oTaskCfg.ClearParams();
        oTaskCfg.Push( dwIntervalMs );
        oTaskCfg.Push( true );

        // don't start immediately
        oTaskCfg.Push( false ); 
        pThis->m_pDT = ptw;

        dbus_timeout_set_data(ptw, pThis, NULL);
        bool bEnabled = 
            dbus_timeout_get_enabled( ptw );

        ret = pThis->DoAddWatch( bEnabled );

    }while( 0 );

    if( ERROR( ret ) )
        return false;

    return true;
}

gint32 CDBusTimerCallback::DoRemoveWatch()
{
    gint32 ret = 0;
    do{
        CEvLoop* pLoop = m_pParent->GetLoop();
        pLoop->StopSource( m_hWatch, srcTimer );
        TaskletPtr pTask( this );
        m_pParent->RemoveWatch( pTask, srcTimer );
        ret = pLoop->RemoveSource(
            m_hWatch, srcTimer );
        m_hWatch = INVALID_HANDLE;
        m_pDT = nullptr;

    }while( 0 );

    return ret;
}

void CDBusTimerCallback::RemoveWatch(
    DBusTimeout* ptw, void* data )
{
    if( ptw == nullptr || data == nullptr )
        return;

    gint32 ret = 0;
    do{
        void* pData =
            dbus_timeout_get_data( ptw );

        if( pData == nullptr )
            break;

        CDBusTimerCallback* pThis =
            ( CDBusTimerCallback* )pData;

        if( pThis == nullptr )
            break;

        ret = pThis->DoRemoveWatch();
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return;
}

gint32 CDBusTimerCallback::DoToggleWatch(
    bool bEnabled )
{
    gint32 ret = 0;
    do{
        CEvLoop* pLoop = m_pParent->GetLoop();
        if( bEnabled )
        {
            ret = pLoop->StopSource(
                m_hWatch, srcTimer );
            if( ERROR( ret ) )
                break;

            ret = pLoop->StartSource(
                m_hWatch, srcTimer );
            if( ERROR( ret ) )
                break;
        }
        else
        {
            pLoop->StopSource(
                m_hWatch, srcTimer );
            if( SUCCEEDED( ret ) )
            {
                TaskletPtr pTask( this );
                m_pParent->RemoveWatch(
                    pTask, srcTimer );

                ret = pLoop->RemoveTimerWatch(
                    m_hWatch );

                m_hWatch = INVALID_HANDLE;
                m_pDT = nullptr;
                // possibly this object is gone,
                // don't touch it anymore
            }
        }

    }while( 0 );

    return ret;
}

void CDBusTimerCallback::ToggleWatch(
    DBusTimeout* ptw, void* data )

{
    if( ptw == nullptr || data == nullptr )
        return;

    gint32 ret = 0;

    do{
        void* pData =
            dbus_timeout_get_data( ptw );

        if( pData == nullptr )
            break;;

        bool bEnabled =
            dbus_timeout_get_enabled( ptw );

        CDBusTimerCallback* pThis =
            ( CDBusTimerCallback* )pData;

        if( pThis == nullptr )
            break;

        ret = pThis->DoToggleWatch( bEnabled );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return;
}

// callbacks from libev
gint32 CDBusTimerCallback::operator()(
    guint32 dwContext )
{
    gint32 ret = G_SOURCE_REMOVE;
    if( m_pDT != nullptr )
    {
        dbus_timeout_handle( m_pDT );
        ret = G_SOURCE_CONTINUE;
    }
    return SetError( ret );
}

gint32 CDBusIoCallback::DoAddWatch(
    bool bEnabled )
{
    gint32 ret = 0;
    do{
        HANDLE hWatch = INVALID_HANDLE;
        CEvLoop* pLoop = m_pParent->GetLoop();
        TaskletPtr pTask( this );
        ret = pLoop->AddIoWatch( pTask, hWatch );
        if( ERROR( ret ) )
            break;
        m_hWatch = hWatch;
        if( bEnabled )
        {
            pLoop->StartSource( hWatch, srcIo );
            m_pParent->AddWatch( pTask, srcIo );
        }

    }while( 0 );

    return ret;
}

dbus_bool_t CDBusIoCallback::AddWatch(
    DBusWatch* ptw, void* data )
{
    if( data == nullptr )
        return false;

    CDBusLoopHooks* pHook =
        ( CDBusLoopHooks*) data;

    gint32 ret = 0;
    do{
        CParamList oParams;
        oParams.Push( ObjPtr( pHook ) );
        oParams.Push( srcIo );

        TaskletPtr pTask;
        pTask.NewObj(
            clsid( CDBusIoCallback ),
            oParams.GetCfg() );

        CDBusIoCallback* pThis = pTask;
        if( pThis == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        CParamList oTaskCfg( ( IConfigDb* )
            pThis->GetConfig() );
        gint32 iFd = dbus_watch_get_unix_fd( ptw );
        guint32 dwFlags = dbus_watch_get_flags( ptw );
        guint32 dwCondition = 0;

        if (dwFlags & DBUS_WATCH_READABLE)
            dwCondition |= POLLIN;

        if (dwFlags & DBUS_WATCH_WRITABLE)
            dwCondition |= POLLOUT;

        bool bEnabled =
            dbus_watch_get_enabled( ptw );

        // prepare parameters for AddIoWatch
        oTaskCfg.ClearParams();
        oTaskCfg.Push( iFd );
        oTaskCfg.Push( dwCondition );

        // don't start immediately
        oTaskCfg.Push( false ); 
        pThis->m_pDT = ptw;
        dbus_watch_set_data(ptw, pThis, NULL);
        ret = pThis->DoAddWatch( bEnabled );

        // AddRef for dbus_watch_set_data
        pThis->AddRef();

    }while( 0 );

    if( ERROR( ret ) )
        return false;

    return true;
}

gint32 CDBusIoCallback::DoRemoveWatch()
{
    gint32 ret = 0;
    do{
        CEvLoop* pLoop = m_pParent->GetLoop();

        HANDLE hWatch = m_hWatch;
        m_hWatch = INVALID_HANDLE;
        m_pDT = nullptr;

        pLoop->StopSource( hWatch, srcIo );

        TaskletPtr pTask( this );
        m_pParent->RemoveWatch( pTask, srcIo );

        ret = pLoop->RemoveSource(
            hWatch, srcIo );

    }while( 0 );

    return ret;
}

void CDBusIoCallback::RemoveWatch(
    DBusWatch* ptw, void* data )
{
    if( ptw == nullptr || data == nullptr )
        return;

    gint32 ret = 0;
    do{
        void* pData =
            dbus_watch_get_data( ptw );

        if( pData == nullptr )
            break;

        CDBusIoCallback* pThis =
            ( CDBusIoCallback* )pData;

        if( pThis == nullptr )
            break;

        ObjPtr pObj( pThis );
        pThis->Release();
        ret = pThis->DoRemoveWatch();
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return;
}

gint32 CDBusIoCallback::DoToggleWatch(
    bool bEnabled )
{
    gint32 ret = 0;
    do{
        CEvLoop* pLoop = m_pParent->GetLoop();
        if( bEnabled )
        {
            // pLoop->StopSource( m_hWatch, srcIo );
            pLoop->StartSource( m_hWatch, srcIo );
        }
        else
        {
            DoRemoveWatch();
        }

    }while( 0 );

    return ret;
}

void CDBusIoCallback::ToggleWatch(
    DBusWatch* ptw, void* data )

{
    if( ptw == nullptr || data == nullptr )
        return;

    gint32 ret = 0;

    do{
        void* pData =
            dbus_watch_get_data( ptw );

        bool bEnabled = 
            dbus_watch_get_enabled( ptw );

        if( pData == nullptr )
            break;;

        CDBusIoCallback* pThis =
            ( CDBusIoCallback* )pData;

        if( pThis == nullptr )
            break;

        ret = pThis->DoToggleWatch( bEnabled );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return;
}

// callbacks from libev
gint32 CDBusIoCallback::operator()(
    guint32 dwContext )
{
    guint32 dwFlags = 0;
    DBusDispatchStatus ret =
        DBUS_DISPATCH_COMPLETE;

    if( m_hWatch == INVALID_HANDLE )
        return SetError( G_SOURCE_REMOVE );

    if( !m_pParent->IsSetupDone() )
        return SetError( G_SOURCE_CONTINUE );

    ret = dbus_connection_get_dispatch_status(
            m_pParent->GetDBusConn() );

    if( ret == DBUS_DISPATCH_DATA_REMAINS )
        m_pParent->WakeupDispatch( ret );

    std::vector< LONGWORD > vecParams;
    gint32 ret2 = GetParamList( vecParams );
    if( ERROR( ret2 ) )
        return SetError( G_SOURCE_CONTINUE );
    guint32 revents = vecParams[ 1 ];

    if( revents & POLLIN )
        dwFlags |= DBUS_WATCH_READABLE;
    if( revents & POLLOUT )
        dwFlags |= DBUS_WATCH_WRITABLE;

    ret2 = G_SOURCE_CONTINUE;
    if( revents & POLLERR )
    {
        dwFlags |= DBUS_WATCH_ERROR;
        ret2 = G_SOURCE_REMOVE;
    }
    if( revents & POLLHUP )
    {
        dwFlags |= DBUS_WATCH_HANGUP;
        ret2 = G_SOURCE_REMOVE;
    }

    DBusWatch* pWat = m_pDT;
    if( pWat != nullptr )
        dbus_watch_handle( pWat, dwFlags );

    return SetError( ret2 );
}

CDBusDispatchCallback::CDBusDispatchCallback(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CDBusDispatchCallback ) );
    gint32 ret = Initialize();
    if( ERROR( ret ) )
    {
        throw std::runtime_error(
            DebugMsg( ret,
            "Error in CDBusDispatchCallback's ctor" ) );
    }
}

gint32 CDBusDispatchCallback::Initialize()
{
    gint32 ret = 0;
    do{
        CEvLoop* pLoop = m_pParent->GetLoop();
        HANDLE hWatch = INVALID_HANDLE;

        CParamList oParams(
            ( IConfigDb* )GetConfig() );

        DBusConnection* pConn =
            m_pParent->GetDBusConn();

        dbus_connection_set_dispatch_status_function(
            pConn,
            &DispatchChange,
            this,
            nullptr );

        // start immediately
        oParams.Push( true );
        TaskletPtr pTask( this );
        ret = pLoop->AddAsyncWatch(
            pTask, hWatch );
        m_hWatch = hWatch;

    }while( 0 );

    return ret;
}

gint32 CDBusDispatchCallback::DoDispatch()
{
    DBusConnection* pConn =
        m_pParent->GetDBusConn();
    do{
        DBusDispatchStatus iRet =
            dbus_connection_dispatch( pConn );

        if( iRet == DBUS_DISPATCH_DATA_REMAINS )
            continue;
        break;

    }while( 1 );

    return 0;
}

// callbacks from mainloop
gint32 CDBusDispatchCallback::operator()(
    guint32 dwContext )
{
    if( !m_pParent->IsSetupDone() )
        return SetError( G_SOURCE_CONTINUE );

    if( m_pParent->IsDBusWatchRemoved() )
    {
        // the last message is eventDBusOffline,
        // ignore it, because the shutdown is
        // going on already
        DebugPrint( 0, "The last message arrives" );
        return SetError( G_SOURCE_REMOVE );
    }

    gint32 ret = DoDispatch();
    if( ret == ERROR_STATE )
        return SetError( G_SOURCE_REMOVE );

    return SetError( G_SOURCE_CONTINUE );
}

void CDBusDispatchCallback::DispatchChange(
    DBusConnection *pConn,
    DBusDispatchStatus iStat,
    void *data)
{
    if( data == nullptr )
        return;

    CDBusDispatchCallback* pThis =
        ( CDBusDispatchCallback* )data;

    if( iStat == DBUS_DISPATCH_DATA_REMAINS )
    {
        CEvLoop* pLoop =
            pThis->m_pParent->GetLoop();
        pLoop->AsyncSend( pThis->m_hWatch );
    }

    return;
}

CDBusWakeupCallback::CDBusWakeupCallback(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CDBusWakeupCallback ) );
    Initialize();
}

gint32 CDBusWakeupCallback::Initialize()
{
    gint32 ret = 0;
    do{
        CEvLoop* pLoop = m_pParent->GetLoop();
        HANDLE hWatch = INVALID_HANDLE;

        CParamList oParams(
            ( IConfigDb* )GetConfig() );

        DBusConnection* pConn =
            m_pParent->GetDBusConn();

        dbus_connection_set_wakeup_main_function(
            pConn,
            &WakeupLoop,
            this,
            &RemoveWatch );

        // start immediately
        oParams.Push( true );
        TaskletPtr pTask( this );
        ret = pLoop->AddAsyncWatch(
            pTask, hWatch );

        if( ERROR( ret ) )
            break;

        m_hWatch = hWatch;

    }while( 0 );

    return ret;
}

// callbacks from libev
gint32 CDBusWakeupCallback::operator()(
    guint32 dwContext )
{
    return SetError( G_SOURCE_CONTINUE );
}

// callbacks from dbus
void CDBusWakeupCallback::WakeupLoop(
    void *data)
{
    if( data == nullptr )
        return;
    do{
        CDBusWakeupCallback* pThis =
            ( CDBusWakeupCallback* )data;

        CEvLoop* pLoop =
            pThis->m_pParent->GetLoop();
        pLoop->AsyncSend( pThis->m_hWatch );

    }while( 0 );
    return;
}

gint32 CDBusWakeupCallback::DoRemoveWatch()
{
    gint32 ret = 0;
    do{
        CEvLoop* pLoop = m_pParent->GetLoop();
        pLoop->StopSource( m_hWatch, srcAsync );
        ret = pLoop->RemoveSource(
            m_hWatch, srcAsync );

    }while( 0 );

    return ret;
}

void CDBusWakeupCallback::RemoveWatch(
    void *data )
{
    if( data == nullptr )
        return;

    gint32 ret = 0;
    do{
        CDBusWakeupCallback* pThis =
            ( CDBusWakeupCallback*) data;

        ret = pThis->DoRemoveWatch();
        if( ERROR( ret ) )
            break;

    }while( 0 );

    return;
}

CDBusLoopHooks::CDBusLoopHooks(
    const IConfigDb* pCfg )
    : m_pConn( nullptr ),
    m_bSetupDone( false )
{
    SetClassId( clsid( CDBusLoopHooks ) );
    gint32 ret = 0;
    if( pCfg == nullptr )
        return;

    CCfgOpener oCfg( pCfg );
    ret = oCfg.GetPointer( 0, m_pLoop );
    if( ERROR( ret ) )
    {
        throw std::invalid_argument(
            DebugMsg( ret,
            "Error occurs in CDBusLoopHooks's ctor" ) );
    }
}

gint32 CDBusLoopHooks::Start(
    DBusConnection* pConn )
{
    if( pConn == nullptr )
        return -EINVAL;

    m_pConn = pConn;
    dbus_connection_ref( m_pConn );

    gint32 ret = 0;
    do{
        if( dbus_connection_set_watch_functions(
            pConn, &CDBusIoCallback::AddWatch,
            &CDBusIoCallback::RemoveWatch,
            &CDBusIoCallback::ToggleWatch,
            this, NULL) == false )
        {
            ret = -ENOMEM;
            break;
        }

        if( dbus_connection_set_timeout_functions(
            pConn, &CDBusTimerCallback::AddWatch,
            &CDBusTimerCallback::RemoveWatch,
            &CDBusTimerCallback::ToggleWatch,
            this, NULL) == false )
        {
            ret = -ENOMEM;
            break;
        }

        CParamList oParams;
        oParams.Push( ObjPtr( this ) );
        oParams.Push( srcAsync );

        ret = m_pWakeupCb.NewObj(
            clsid( CDBusWakeupCallback ),
            oParams.GetCfg() );

        if( ERROR( ret ) )
            break;

        // To setup dispatch callback at last to
        // avoid deadlock due to twisted
        // synchronous SOURCE_HEADER::start and
        // dbus connection lock
        oParams[ 1 ] = srcAsync;
        ret = m_pDispatchCb.NewObj(
            clsid( CDBusDispatchCallback ),
            oParams.GetCfg() );
        if( ERROR( ret ) )
            break;

    }while( 0 );

    if( SUCCEEDED( ret ) )
        SetSetupDone( true );

    return ret;
}

gint32 CDBusLoopHooks::AddWatch(
    TaskletPtr& pCallback, EnumSrcType iType )
{
    if( pCallback.IsEmpty() )
        return -EINVAL;
    if( iType != srcIo && iType != srcTimer )
        return -EINVAL;

    std::set< TaskletPtr >* pWatches = &m_setIoCbs;
    if( iType == srcTimer )
        pWatches = &m_setTimerCbs;

    CStdRMutex oLock( GetLock() );
    pWatches->insert( pCallback );

    return 0;
}

gint32 CDBusLoopHooks::RemoveWatch(
    TaskletPtr& pCallback, EnumSrcType iType )
{
    if( pCallback.IsEmpty() )
        return -EINVAL;
    if( iType != srcIo && iType != srcTimer )
        return -EINVAL;

    std::set< TaskletPtr >* pWatches = &m_setIoCbs;
    if( iType == srcTimer )
        pWatches = &m_setTimerCbs;

    CStdRMutex oLock( GetLock() );
    pWatches->erase( pCallback );

    return 0;
}

gint32 CDBusLoopHooks::Stop()
{
    CDBusLoopHookCb* pCb = m_pDispatchCb;
    SetSetupDone( false );
    if( !m_pDispatchCb.IsEmpty() )
    {
        if( pCb != nullptr )
            pCb->Stop();
    }

    if( !m_pWakeupCb.IsEmpty() )
    {
        pCb = m_pWakeupCb;
        if( pCb != nullptr )
            pCb->Stop();
    }

    if( m_pConn != nullptr )
    {
        dbus_connection_unref( m_pConn );
        m_pConn = nullptr;
    }

    CStdRMutex oLock( GetLock() );
    for( auto pCallback : m_setIoCbs )
    {
        pCb = pCallback;
        if( pCb != nullptr )
            pCb->Stop();
    }

    for( auto pCallback : m_setTimerCbs )
    {
        pCb = pCallback;
        if( pCb != nullptr )
            pCb->Stop();
    }

    return 0;
}

void CDBusLoopHooks::ClearAll()
{
    m_pDispatchCb.Clear();
    m_pWakeupCb.Clear();

    CStdRMutex oLock( GetLock() );
    m_setIoCbs.clear();
    m_setTimerCbs.clear();
}

void CDBusLoopHooks::WakeupDispatch(
    DBusDispatchStatus iStat )
{
    if( m_pDispatchCb.IsEmpty() )
        return;
    CDBusDispatchCallback* pCb =
        m_pDispatchCb;
    if( pCb == nullptr )
        return;
    pCb->DispatchChange(
        GetDBusConn(), iStat, pCb );
    return;
}

}
#endif
