/*
 * =====================================================================================
 *
 *       Filename:  prxyport.h
 *
 *    Description:  declarations of proxy pdo and proxy fdo ports
 *
 *        Version:  1.0
 *        Created:  01/14/2018 11:33:22 AM
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
#include "dbusport.h"

namespace rpcf
{

class CDBusProxyPdo : public CRpcPdoPort
{
    // this pdo will implement the ForwardRequest 
    // proxy method
    //
    // there will be an `ip addr' property in the
    // config db
    // or a `unit number' in the future

    protected:
    bool                m_bConnected;
    bool                m_bAuth = false;

    // this match will work at two places
    //
    // 1. in PreDispatchMsg to detect remote if
    // online/offline event.
    //
    // 2. when processing NameOwnerChanged, this
    // match will test if the RPCROUTER is gone
    // and block further request.
    MatchPtr            m_pMatchFwder;

    // match table for dbus sys message
    MatchPtr            m_pMatchDBus;

    bool                m_bStopReady = false;
    TaskletPtr          m_pConnTask;
    std::atomic< bool > m_atmInitDone;
    std::atomic< guint32 > m_atmReqSent;

    gint32 CheckConnCmdResp(
        DBusMessage* pMsg, gint32& iMethodReturn );

    gint32 HandleConnRequest(
        IRP* pIrp, bool bConnect );

    gint32 HandleRmtRegMatch(
        IRP* pIrp, bool bReg = true );

    gint32 BuildMsgHeader( DMsgPtr& pMsg ) const;

    virtual gint32 PackupReqMsg(
        DMsgPtr& pReqMsg,
        DMsgPtr& pOutMsg,
        bool bNoReply = false ) const;

    gint32 UnpackFwrdRespMsg( IRP* pIrp );
    gint32 UnpackFwrdEventMsg( IRP* pIrp );

    gint32 CompleteConnReq( IRP* pIrp );
    gint32 CompleteListening( IRP* pIrp );
    gint32 CompleteFwrdReq( IRP* pIrp );
    gint32 CompleteRmtRegMatch( IRP* pIrp );

    virtual gint32 HandleListening( IRP* pIrp );
    gint32 Reconnect();

    public:

    typedef CRpcPdoPort super;

    // dispatch data
    CDBusProxyPdo( const IConfigDb* pCfg = NULL );

    ~CDBusProxyPdo();

    // virtual gint32 DispatchData( CBuffer* pData );

    virtual gint32 SubmitIoctlCmd( IRP* pIrp );

    // during start, we will send out a connect
    // request to the router to make a connection
    // to the target machine
    virtual gint32 OnPortReady( IRP* pIrp );
    virtual gint32 CompleteIoctlIrp( IRP* pIrp );

    inline bool IsInitDone()
    { return m_atmInitDone; }

    inline void SetInitDone()
    { m_atmInitDone = true; }

    inline bool IsConnected()
    { return m_bConnected; }

    void SetConnected( bool bConnected );

    virtual void OnPortStopped();

    gint32 HandleFwrdReq( IRP* pIrp );

    virtual DBusHandlerResult PreDispatchMsg(
        gint32 iType, DBusMessage* pMsg );

    virtual gint32 OnRmtSvrOnOffline(
        DBusMessage* pMsg );

    virtual gint32 PostStart( IRP* pIrp );
    virtual gint32 OnQueryStop( IRP* pIrp );

    gint32 OnModOnOffline(
        DBusMessage* pDBusMsg );

    gint32 NotifyRouterOffline();

    inline void ClearConnTask()
    {
        CStdRMutex oPortLock( GetLock() );
        m_pConnTask.Clear();
    }

    std::string GetReqFwdrName() const;
};

typedef CAutoPtr< clsid( CDBusProxyPdo ), CDBusProxyPdo > ProxyPdoPtr;


class IRpcFdoPort : public CRpcBasePort
{
    // a tag class to mark this object as a pdo port
    public:

    typedef  CRpcBasePort super;

    IRpcFdoPort( const IConfigDb* pCfg )
        : CRpcBasePort( pCfg )
    { m_dwFlags = PORTFLG_TYPE_FDO; }
};

class CDBusProxyFdo : public IRpcFdoPort
{
    // to filter for remote interface
    // online/offline event
    MatchPtr            m_matchModOnOff;

    // to filter for remote dbus online/offline
    // event
    MatchPtr            m_matchDBusOff;
    MatchPtr            m_matchRmtSvrEvt;

    TaskletPtr          m_pListenTask;

    bool                m_bConnected = false;
    bool                m_bAuth = false;

    gint32 HandleListeningFdo( IRP* pIrp );

    protected:


    gint32 MatchExist( IRP* pIrp );

    gint32 HandleRegMatchInternal(
        IRP* pIrp, bool bReg );

    virtual gint32 HandleRegMatch( IRP* pIrp );

    virtual gint32 HandleUnregMatch( IRP* pIrp );

    gint32 ScheduleModOnOfflineTask(
        const std::string strModName, guint32 dwFlags );

    virtual gint32 HandleSendData( IRP* pIrp );

    virtual gint32 HandleSendReq( IRP* pIrp );

    virtual gint32 HandleSendEvent( IRP* pIrp )
    { return -ENOTSUP; }

    gint32 BuildSendDataMsg(
        IRP* pIrp, DMsgPtr& pMsg );

    gint32 HandleRegMatchLocal( IRP* pIrp );

    public:

    typedef IRpcFdoPort super;

    CDBusProxyFdo( const IConfigDb* pCfg );

    virtual gint32 ClearDBusSetting(
        IMessageMatch* pMatch );

    virtual gint32 SetupDBusSetting(
        IMessageMatch* pMatch );

    virtual gint32 CompleteIoctlIrp( IRP* pIrp );

    virtual gint32 OnPortReady( IRP* pIrp );

    inline void SetConnected( bool bConnected )
    { m_bConnected = bConnected; }

    inline bool IsConnected()
    { return m_bConnected; }

    // virtual gint32 DispatchRespMsg(
    //     DBusMessage* pMsg );

    gint32 CompleteConnReq( IRP* pIrp );

    virtual gint32 SubmitIoctlCmd( IRP* pIrp );

    virtual DBusHandlerResult PreDispatchMsg(
        gint32 iMsgType, DBusMessage* pMsg );

    virtual gint32 PostStart( IRP* pIrp );

    virtual gint32 PreStop( IRP* pIrp );

};

class CProxyPdoConnectTask
    : public CThreadSafeTask
{
    public:
    typedef CThreadSafeTask super;
    CProxyPdoConnectTask( const IConfigDb* pCfg = nullptr )
        : CThreadSafeTask( pCfg )
    {
        SetClassId( clsid( CProxyPdoConnectTask ) );
    }
    gint32 Process( guint32 dwContext );
    gint32 ExtendIrpTimer( IRP* pIrp );
    gint32 CompleteMasterIrp(
        IRP* pIrp, gint32 ret2 );
    gint32 CompleteReconnect( gint32 iRet );
};

class CDBusProxyPdoLpbk : public CDBusProxyPdo
{
    public:
    typedef CDBusProxyPdo super;
    CDBusProxyPdoLpbk( const IConfigDb* pCfg );

    gint32 SendDBusMsg( DBusMessage* pMsg,
        guint32* pdwSerial );

    gint32 GetSender( std::string& strSender ) const;

    virtual gint32 PackupReqMsg(
        DMsgPtr& pReqMsg,
        DMsgPtr& pOutMsg,
        bool bNoReply = false ) const;

    gint32 GetProperty(
        gint32 iProp, CBuffer& oBuf ) const;
};

}
