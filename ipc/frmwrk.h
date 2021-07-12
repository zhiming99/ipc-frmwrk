/*
 * =====================================================================================
 *
 *       Filename:  frmwrk.h
 *
 *    Description:  declaration of the pnp manager, driver manager, and io manager
 *
 *        Version:  1.0
 *        Created:  01/19/2016 08:30:43 PM
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
#pragma once

#include <json/json.h>
#include <vector>
#include <unistd.h>
#include <sys/syscall.h>
#include "namespc.h"
#include "defines.h"
#include "registry.h"
#include "tasklets.h"
#include "utils.h"
#include "port.h"
#include "portdrv.h"
#include "mainloop.h"

namespace rpcf
{

extern gint32 ReadJsonCfg(
    const std::string& strFile,
    Json::Value& valConfig );

class CDriverManager : public IService
{
    protected:


	// all the drivers created
	// std::map<std::string, PortDrvPtr> m_mapPortDrivers;

    // all the bus drivers
	// std::map<std::string, BusDrvPtr> m_mapBusDrivers;

    CIoManager*     m_pIoMgr;
    std::string     m_strCfgPath;
    Json::Value     m_oConfig;

    gint32 RegDrvInternal(
        const std::string& strDrvPath,
        CPortDriver* pdrv ) const;

    gint32 UnRegDrvInternal(
        const std::string& strRegPath,
        CPortDriver* pDrv ) const;

	public:

    std::string GetBusDrvRegRoot() const;
    // std::string GetAllDrvRegRoot() const;

    std::string GetPreloadRegRoot(
        const std::string& strClass) const;

    CDriverManager(
        const IConfigDb* pCfg );

    ~CDriverManager();

    gint32 ReadConfig(
        Json::Value& valConfig ) const;

    // load all the bus drivers of this module
    gint32 LoadStaticDrivers();

    // driver loading 
    gint32 LoadDriver(
        const IConfigDb* pCfg );

    gint32 LoadDriver(
        const std::string& strDriverName );

    // Create all the preloadable objects of this
    // module
    gint32 CreatePreloadable();

    // Create an object of the specified of this
    // module
    gint32 CreateObject(
        const std::string& strObjName,
        const std::string& strClass );

    gint32 LoadClassFactories();

    gint32 UnloadDriver(
        const std::string& strDriverName );

    gint32 FindDriver(
        const std::string& strDriverName,
        IPortDriver*& pRet ) const;

    Json::Value& GetJsonCfg()
    { return m_oConfig; }

    // load the driver and create the upper-stream port
    gint32 BuildPortStack(
        IPort* pPdoPort );

    gint32 RegBusDrv( IBusDriver* pDrv ) const;
    gint32 RegPortDrv( CPortDriver* pDrv ) const;

    gint32 UnRegBusDrv( IBusDriver* pDrv ) const;
    gint32 UnRegPortDrv( CPortDriver* pDrv ) const;

    gint32 GetDrvRegPath(
        CPortDriver* pDrv,
        std::string& strPath,
        bool bBusDrv = false ) const;

    std::string GetPortDrvRegRoot() const;

    gint32 Start();
    gint32 Stop();

    gint32 StopDrivers();
    gint32 StopPreloadable();

    // IEventSink method
    gint32 OnEvent( EnumEventId iEvent,
            LONGWORD dwParam1 = 0,
            LONGWORD dwParam2 = 0,
            LONGWORD* pData = NULL  )
    { return -ENOTSUP; }

    inline CIoManager* GetIoMgr() const
    {  return m_pIoMgr; }
};

typedef CAutoPtr< Clsid_CDriverManager, CDriverManager > DrvMgrPtr;


class CPnpManager : public IService
{
    // this class listen on all the ports
    // for pnp event
 
    protected:

    CIoManager      *m_pIoMgr;

    // gint32 StartStopPorts(
      //   IPort* pPort, bool bStop, IRP* pMasterIrp );

    gint32 StartPortsInternal(
        IPort* pPort, IRP* pMasterIrp = nullptr );

    gint32 StopPortsInternal(
        IPort* pPort, IRP* pMasterIrp = nullptr );

    // the notification method for 
    // subscribers of user interfaces
    void OnPortStartStop(
        IPort* pPort, bool bStop = false );

    gint32 QueryStop(
        IPort* pPort, IRP* pIrp, IRP* pMasterIrp );

    gint32 DoStop(
        IPort* pPort, IRP* pIrp, IRP* pMasterIrp );

    void HandleCPEvent(
        EnumEventId iEvent,
        LONGWORD dwParam1,
        LONGWORD dwParam2,
        LONGWORD* pData );

    public:

    struct PnpEvent
    {
        // structure to send to the workitem queue
        guint32 dwEvent;
        CfgPtr pCfg;
        ServicePtr pService;
        PnpEvent();
    };

    CPnpManager( const IConfigDb* pCfg );

    // TODO: where to issue an irp to the bus
    // port to listen to the pnp event
    gint32 Start();

    // TODO: the method will cancel the irp to the bus
    // port listening to the pnp event
    gint32 Stop();

    /**
    * @name StartPortStack 
    * @{ */
    /** 
     * Start the ports on a port stack
     * from bottom up. It will send down an irp
     * to inform the port to start
     * 
     *  @} */
    gint32 StartPortStack(
        ObjPtr pPort,
        ObjPtr pMasterIrp );

    /**
    * @name StopPortStack
    * @{ */
    /** 
     * Stop the ports on a port stack from
     * top down. it will call each port's stop
     * method to complete the stop
     * Please make sure after the port is stopped
     * the reference count drops to the same value
     * as before the previous call of StartPortStack
     *
     * pPort is ptr to any ports on the stack
     * @} */
    gint32 StopPortStack(
        IPort* pPort,
        IRP* pMaster = nullptr,
        bool bSendEvent = true );

    /**
    * @name Destroy a port stack, on which the pPort
    * lives
    * @{ */
    /**
     * you need to make sure the pPort is not in its starting
     * state, otherwise it could fail  @} */
    
    gint32 DestroyPortStack( IPort* pPort,
        IEventSink* pCallback = nullptr );

    void OnPortStarted( IPort* pTopPort );
    void OnPortStopped( IPort* pTopPort );

    // called via the event eventPortAttached
    // stack building
    void OnPortAttached( IPort* pPdoPort );

    // called by the bus port or the pdo port
    void OnPortRemoved( IPort* pPdoPort );

    // the method will notify the bus driver
    // to update its internal objects
    void OnPortRemoved( IConfigDb* pConfig );

    // used by services to register event sink for bus event
    gint32 SubscribeEvent(
        EnumPropId iPropId, IEventSink* pEventSink );

    // IEventSink method
    gint32 OnEvent(
        EnumEventId iEvent,
        LONGWORD dwParam1 = 0,
        LONGWORD dwParam2 = 0,
        LONGWORD* pData = NULL  );

    inline CIoManager* GetIoMgr() const
    { return m_pIoMgr; }


};

typedef CAutoPtr< clsid( CPnpManager ), CPnpManager > PnpMgrPtr;

class CPortInterfaceMap : public CObjBase
{
    std::multimap< PortPtr, EventPtr > m_mapPort2If;
    std::multimap< EventPtr, PortPtr > m_mapIf2Port;
    std::set< HANDLE > m_setHandles;
    stdrmutex m_oLock;

    typedef std::pair< std::multimap<PortPtr, EventPtr>::iterator,
        std::multimap<PortPtr, EventPtr>::iterator> PEPAIR;

    typedef std::pair< std::multimap<PortPtr, EventPtr>::const_iterator,
        std::multimap<PortPtr, EventPtr>::const_iterator> CPEPAIR;

    typedef std::pair< std::multimap<EventPtr, PortPtr >::iterator,
        std::multimap<EventPtr, PortPtr >::iterator> EPPAIR;

    typedef std::pair< std::multimap<EventPtr, PortPtr >::const_iterator,
        std::multimap<EventPtr, PortPtr >::const_iterator> CEPPAIR;

    public:
    CPortInterfaceMap(); 
    ~CPortInterfaceMap();

    std::recursive_mutex& GetLock() const { return (std::recursive_mutex&) m_oLock; }
    gint32 AddToHandleMap( IPort* pPort, IEventSink* pEvent );
    gint32 RemoveFromHandleMap( IPort* pPort, IEventSink* pEvent );
    gint32 RemoveFromHandleMap( IEventSink* pEvent, IPort* pPort );
    gint32 PortExist( IPort* pPort ) const;
    gint32 PortExist( HANDLE hPort ) const;
    gint32 GetPortPtr( HANDLE hHandle, PortPtr& pPort ) const;
    gint32 InterfaceExist( IEventSink* pEvent ) const;

    gint32 PortExist(
        IPort* pPort, std::vector< EventPtr >* pvecVals = nullptr ) const;

    gint32 InterfaceExist(
        IEventSink* pEvent, std::vector< PortPtr >* pvecVals = nullptr ) const;

    gint32 IsPortNoRef( IPort* pPort, bool& bNoRef ) const;
};

#define CONFIG_FILE "./driver.json"

class CIoManager : public IService
{
    DrvMgrPtr                   m_pDrvMgr;
    MloopPtr                    m_pLoop;
    mutable stdrmutex           m_oGrandLock;
    RegPtr                      m_pReg;
    UtilsPtr                    m_pUtils;
    PnpMgrPtr                   m_pPnpMgr;
    sem_t                       m_semSync;

    // currently we allow only one instance of
    // CIoManager in a process. we use
    // `m_bInit' to control this.
    bool                        m_bInit = false;
    std::atomic< bool >         m_bStop = { false };

    CPortInterfaceMap           m_oPortIfMap;

    // a map for port existance check from services
    std::map<HANDLE, gint32>    m_mapHandle2Port;

    // the thread pool for irp completion
    ThrdPoolPtr                 m_pIrpThrdPool;
    ThrdPoolPtr                 m_pTaskThrdPool;


    // module name
    std::string                 m_strModName;
    guint32                     m_dwNumCores;
    // house clean timer
    gint32                      m_iHcTimer;

    std::vector< ThreadPtr >    m_vecStandAloneThread;

    gint32                      m_iMaxIrpThrd = 2;
    gint32                      m_iMaxTaskThrd = 2;

    ObjPtr                      m_pSyncIf;

    protected:

    std::recursive_mutex& GetLock() const;

    // on-demand port creation, from an interface
    // object on a non-mainloop thread, synchronous call
    //
    // a driver stack building process is started
    // open or create a port if not existing
    gint32 OpenPortByCfg(
        const IConfigDb* pCfg,
        PortPtr& pPort );

    gint32 CancelAssocIrp(
        IRP* pirp );

    gint32 PostCompleteIrp(
        IRP* pIrp, bool bCancel = false );

    gint32 StartStandAloneThread(
        ThreadPtr& pThread, EnumClsid iTaskClsid );

    gint32 ClearStandAloneThreads();

    gint32 CloseChildPort(
        IPort* pBusPort,
        IPort* pPort,
        IEventSink* pCallback );

    public:

    CIoManager(
        const std::string& strModName );

    CIoManager(
        const IConfigDb* pCfg );

    ~CIoManager();

    gint32 OpenPortByRef(
        HANDLE hPort,
        IEventSink* pEventSink );

    // to open an existing port with the well-known path
    // in the registry
    gint32 OpenPortByPath(
        const std::string& strPath,
        HANDLE& hPort );

    // open an existing port or create a port if not existing
    // this method is usually called by the interfaces to
    // bind to a port
    // in the pCfg
    //
    // propEventSink: the interface for registration and
    //      callback
    //
    // propPortClass: mandatory
    // propBusName: mandatory
    //
    // propPortName: optional
    //
    // propPortId : mandatory for local dbus
    // propIpAddr: mandatory for proxy
    //
    gint32 OpenPort(
        const IConfigDb* pCfg, // pCfg: port config infor
        HANDLE& hPort );       // hPort: handle to the port to return

    // on-demand port Removal, from an interface on 
    // a non-mainloop thread, synchronous
    gint32 ClosePort(
        HANDLE hPort,
        IEventSink* pEvent,
        IEventSink* pCallback = nullptr );

    // irp helper
    gint32 SubmitIrp(
        HANDLE hPort,
        IRP* pIrp,
        bool bCheckExist = true );

    gint32 SubmitIrpInternal(
        IPort* pPort,
        IRP* pIrp,
        bool bTopmost = true );

    gint32 CancelSlaves(
        IRP* pIrp, bool bForce, gint32 iRet );

    gint32 CancelIrp(
        IRP* pirp, bool bForce = true, gint32 iRet = 0 );

    gint32 CompleteIrp( IRP* pirp );

    gint32 CompleteAssocIrp( IRP* pirp );

    // helpers
    CUtilities& GetUtils() const;
    CMainIoLoop* GetMainIoLoop() const;
    CPnpManager& GetPnpMgr() const;
    CDriverManager& GetDrvMgr() const;
    CRegistry& GetRegistry() const;
    CThreadPool& GetIrpThreadPool() const;
    CThreadPool& GetTaskThreadPool() const;
    const std::string& GetModName() const;
    sem_t* GetSyncSem() const;

    ObjPtr& GetSyncIf() const;

    guint32 GetNumCores() const
    { return m_dwNumCores; }

    // registry helpers
    gint32 RegisterObject(
        const std::string& strPath,
        const ObjPtr& pObj );

    gint32 UnregisterObject(
        const std::string& strPath );

    gint32 SetObject(
        const std::string& strPath,
        ObjPtr& pObj );

    gint32 GetObject(
        const std::string& strPath,
        ObjPtr& pObj );

    gint32 RemoveRegistry(
        const std::string& strPath,
        const ObjPtr& pObj );

    gint32 MakeBusRegPath(
        std::string& strPath,
        const std::string& strBusName );

    gint32 AddToHandleMap(
        IPort* pPort,
        IEventSink* pEvent = nullptr );

    gint32 RemoveFromHandleMap(
        IPort* pPort,
        IEventSink* pEvent = nullptr );

    gint32 PortExist(
        IPort* pPort,
        std::vector< EventPtr >* pvecEvent = nullptr );

    gint32 GetPortPtr(
        HANDLE hHandle, PortPtr& pPort ) const;

    gint32 IsPortNoRef(
        IPort* pPort, bool& bNoRef ) const;

    gint32 EnumDBusNames(
        std::vector<std::string>& vecNames );

    bool RunningOnMainThread() const;

    gint32 ScheduleWorkitem(
        guint32 dwFlags,
        IEventSink* pCallback,
        LONGWORD dwContext,
        bool bLongWait = false );

    gint32 ScheduleTask(
        EnumClsid iClsid,
        CfgPtr& pCfg,
        bool bOneshot = false );

    gint32 RescheduleTask(
        TaskletPtr& pTask,
        bool bOneshot = false );

    void WaitThreadsQuit();

    gint32 ScheduleTaskMainLoop(
        EnumClsid iClsid, CfgPtr& pCfg );

    gint32 RescheduleTaskMainLoop(
        TaskletPtr& pTask );

    gint32 Start();
    gint32 Stop();

    gint32 OnEvent( EnumEventId iEvent,
        LONGWORD dwParam1,
        LONGWORD dwParam2,
        LONGWORD* pData);

    gint32 GetPortProp(
        HANDLE hPort,
        gint32 iProp,
        BufPtr& pBuf );

    gint32 RemoveTask(
        TaskletPtr& pTask );

    template< class T >
    gint32 GetCmdLineOpt( EnumPropId iPropId, T& oVal )
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

            oVal = ( T& )*pBuf;
        }while( 0 );

        return ret;
    }

    template< class T >
    gint32 SetCmdLineOpt( EnumPropId iPropId, const T& oVal )
    {
        gint32 ret = 0;
        do{
            BufPtr pBuf( true );
            *pBuf = oVal;
            CStdRMutex oRegLock( m_pReg->GetLock() );
            ret = m_pReg->ChangeDir( "/cmdline" );
            if( ERROR( ret ) )
                break;

            ret = m_pReg->SetProperty( iPropId, *pBuf );
            if( ERROR( ret ) )
                break;

        }while( 0 );

        return ret;
    }

    gint32 GetRouterName(
        std::string& strRtName );

    inline gint32 SetRouterName(
        const std::string& strRtName )
    {
        return SetCmdLineOpt(
            propRouterName, strRtName );
    }

    gint32 TryLoadClassFactory(
        const std::string& strFile );

    gint32 TryFindDescFile(
        const std::string& strFile,
        std::string& strPath );

    gint32 AddAndRun(
        TaskletPtr& pTask, bool bImmediate );

    gint32 AppendAndRun(
        TaskletPtr& pTask );

    gint32 AddSeqTask(
        TaskletPtr& pTask,
        bool bLong = false );

    inline bool IsStopping()
    { return m_bStop; }

    inline CIoManager* GetIoMgr()
    { return this; }
};

template<>
gint32 CIoManager::GetCmdLineOpt< std::string >(
    EnumPropId iPropId, std::string& oVal );

typedef CAutoPtr< Clsid_CIoManager, CIoManager > IoMgrPtr;

}
