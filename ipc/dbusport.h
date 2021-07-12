/*
 * =====================================================================================
 *
 *       Filename:  dbusport.h
 *
 *    Description:  dbus bus port and bus driver implementation
 *
 *        Version:  1.0
 *        Created:  03/18/2016 11:38:24 PM
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

#include <dbus/dbus.h>
#include "defines.h"
#include "port.h"
#include "autoptr.h"
#include "msgmatch.h"
#include "portex.h"

namespace rpcf
{

#define DBUS_HANDLER_RESULT_HALT    ( ( DBusHandlerResult )100 )

gint32 NormalizeIpAddr(
    gint32 dwProto,
    const std::string strIn,
    std::string& strOut );

gint32 IpAddrToByteStr(
    const std::string& strIpAddr,
    std::string& strRet );

gint32 IpAddrToBytes(
    const std::string& strIpAddr,
    guint8* pBytes,
    guint32& dwSize );

gint32 BytesToString(
    const guint8* bytes,
    guint32 dwSize,
    std::string& strRet );

class CRpcBasePort : public CPort
{
    protected:

    struct MATCH_ENTRY
    {
        // irps waiting for the response or event
        std::deque<IrpPtr>      m_queWaitingIrps;

        // in case the incoming message is not being
        // handled in time
        std::deque<DMsgPtr>     m_quePendingMsgs;

        // we use a refcount to keep track of count
        // of interfaces referencing the same match
        gint32                  m_iRefCount;
        bool                    m_bConnected;

        MATCH_ENTRY()
            : m_iRefCount( 0 ),
            m_bConnected( false )
        {;}

        MATCH_ENTRY( const MATCH_ENTRY& rhs )
        {
            m_iRefCount = rhs.m_iRefCount;
            m_bConnected = rhs.m_bConnected;
            m_queWaitingIrps = rhs.m_queWaitingIrps;
            m_quePendingMsgs = rhs.m_quePendingMsgs;
        }

        ~MATCH_ENTRY()
        {
            m_queWaitingIrps.clear();
            m_quePendingMsgs.clear();
        }

        int AddRef()
        {
            ++m_iRefCount;
            return m_iRefCount;
        }
        int Release()
        {
            --m_iRefCount;
            return m_iRefCount;
        }
        int RefCount() const
        { return m_iRefCount; }

        bool IsConnected() const
        {
            return m_bConnected;
        }

        void SetConnected( bool bConnected )
        {
            m_bConnected = bConnected;
        }
    };

    IrpPtr m_pTestIrp;
    //
	// event/request listening irps

    // routing table for incoming event/signal
    // messages
    typedef std::map< MatchPtr, MATCH_ENTRY > MatchMap;

    MatchMap m_mapEvtTable;


	// the map for responst of the outgoing request IRPs 
	std::map< guint32, IrpPtr >     m_mapSerial2Resp;

    gint32 FindIrpForResp(
        DBusMessage* pMsg, IrpPtr& pIrp );

    gint32 CancelAllIrps( gint32 iErrno );
    
    protected:

    virtual gint32 DispatchSignalMsg(
        DBusMessage* pMsg );

    virtual gint32 OnRespMsgNoIrp(
        DBusMessage* pDBusMsg )
    { return -ENOTSUP; }

    virtual gint32 DispatchRespMsg(
        DBusMessage* pMsg );

    gint32 DispatchKeepAliveMsg(
        DBusMessage* pMsg );

    gint32 ClearMatchMapInternal(
        MatchMap& oMap,
        std::vector< IrpPtr >& vecPendingIrps,
        std::vector< MatchPtr>& vecMatches );

    virtual gint32 ClearDBusSetting(
        IMessageMatch* pMatch ) = 0;

    virtual gint32 SetupDBusSetting(
        IMessageMatch* pMatch ) = 0;

    virtual gint32 ClearMatchMap(
        std::vector< IrpPtr >& vecPendingIrps,
        std::vector< MatchPtr>& vecMatches );

    gint32 RemoveIrpFromMapInternal(
        IRP* pIrp, MatchMap& oMap );

    virtual gint32 RemoveIrpFromMap( IRP* pIrp );

    virtual gint32 HandleRegMatch( IRP* pIrp );

    virtual gint32 HandleUnregMatch( IRP* pIrp );

    virtual gint32 HandleListening( IRP* pIrp );

    virtual gint32 HandleSendReq( IRP* pIrp ) = 0;

    virtual gint32 HandleSendEvent( IRP* pIrp ) = 0;

    virtual gint32 HandleSendData( IRP* pIrp ) = 0;

    // virtual gint32 HandleFetchData( IRP* pIrp );

    gint32 AddMatch( MatchMap& oMap, MatchPtr& pMatch );

    gint32 RemoveMatch( MatchMap& oMap, MatchPtr& pMatch );

    virtual gint32 IsIfSvrOnline( const DMsgPtr& pMsg );

    gint32 ClearReqIrpsOnDest(
        const std::string& strDestination,
        std::vector< IrpPtr >& vecIrpsToCancel );

    gint32 OnModOnOffline( DMsgPtr& pMsg, bool bOnline,
        const std::string& strModName );

    virtual gint32 OnModOnOffline( DBusMessage* pMsg );

    virtual DBusHandlerResult  DispatchDBusSysMsg(
        DBusMessage* pMsg );

    virtual gint32 ScheduleModOnOfflineTask(
        const std::string strModName, guint32 dwFlags );

    gint32 BuildSendDataReq(
        IConfigDb* pParams, DMsgPtr& pMsg );

    gint32 SchedCompleteIrps(
        std::deque< IrpPtr >& queIrps,
        std::deque< DMsgPtr>& queMsgs );

    public:

    typedef CPort super;

    CRpcBasePort( const IConfigDb* pCfg = nullptr );

    virtual gint32 OnSubmitIrp( IRP* pIrp );

    virtual gint32 PreStop( IRP *pIrp );

    virtual gint32 SubmitIoctlCmd( IRP* pIrp );

    virtual gint32 GetMatchMap(
        IRP* pIrp, MatchMap*& pMap );

	virtual gint32 CancelFuncIrp(
        IRP* pIrp, bool bForce = true );

    virtual gint32 DispatchData( CBuffer* pData );

    virtual DBusHandlerResult PreDispatchMsg(
        gint32 iMsgType, DBusMessage* pMsg );

    virtual DBusHandlerResult DispatchMsg(
        gint32 iMsgType, DBusMessage* pMsg );

    virtual void PostDispatchMsg(
        gint32 iMsgType, DBusMessage* pMsg )
    {;}

    virtual gint32 Start()
    { return 0; }

    virtual gint32 BuildSendDataMsg(
        IRP* pIrp, DMsgPtr& pMsg );

};
/**
* @name CRpcBasePortEx, an extension of
* CRpcBasePort with the requst map for incoming
* requests 
* @{ */
/**  @} */

class CRpcBasePortEx : public CRpcBasePort
{
    protected:
    // routing table for incoming request messages
    MatchMap    m_mapReqTable;

    virtual gint32 ClearMatchMap(
        std::vector< IrpPtr >& vecPendingIrps,
        std::vector< MatchPtr>& vecMatches );

    virtual gint32 RemoveIrpFromMap( IRP* pIrp );

    gint32 GetMatchMap( IMessageMatch* pMatch,
        MatchMap*& pMap );

    public:

    typedef  CRpcBasePort super;

    CRpcBasePortEx( const IConfigDb* pCfg )
        :CRpcBasePort( pCfg )
    {;}

    gint32 GetMatchMap( IRP* pIrp,
        MatchMap*& pMap );

    gint32 DispatchReqMsg( DBusMessage* pMsg );

    // dispatch data
    virtual DBusHandlerResult DispatchMsg(
        gint32 iMsgType, DBusMessage* pMsg );

};

class IRpcPdoPort : public CRpcBasePortEx
{
    // a tag class to mark this object as a pdo port
    public:
    typedef  CRpcBasePortEx super;

    IRpcPdoPort( const IConfigDb* pCfg )
        :CRpcBasePortEx( pCfg )
    { m_dwFlags = PORTFLG_TYPE_PDO; }
};

class CRpcPdoPort : public IRpcPdoPort
{
    protected:

    DBusConnection* m_pDBusConn;

    virtual gint32 ClearDBusSetting(
        IMessageMatch* pMatch );

    virtual gint32 SetupDBusSetting(
        IMessageMatch* pMatch );

    virtual gint32 HandleSendReq( IRP* pIrp );

    virtual gint32 HandleSendEvent( IRP* pIrp );

    virtual gint32 HandleSendData( IRP* pIrp );

    gint32 ScheduleModOnOfflineTask(
        const std::string strModName, guint32 dwFlags );

    public:

    typedef IRpcPdoPort super;

    CRpcPdoPort( const IConfigDb* pCfg = nullptr );

    virtual gint32 SendDBusMsg(
        DBusMessage* pMsg, guint32* pdwSerial );

    // IEventSink method
    virtual gint32 OnEvent(
        EnumEventId iEvent,
        LONGWORD dwParam1 = 0,
        LONGWORD dwParam2 = 0,
        LONGWORD* pData = NULL  );

    
    gint32 SubmitIoctlCmd( IRP* pIrp );
    gint32 HandleSetReqQueSize( IRP* pIrp );
};

class CDBusLocalPdo : public CRpcPdoPort
{
    // pdo for local rpc via dbus
    //
    // the following two queues are for
    // server interfaces
    //

    // match table for dbus sys message
    MatchPtr                m_matchDBus;

    // registered objects on the dbus
    std::map< std::string, gint32 > m_mapRegObjs;

    protected:

    virtual gint32 ClearDBusSetting(
        IMessageMatch* pMatch );

    virtual gint32 SetupDBusSetting(
        IMessageMatch* pMatch );


    gint32 HandleSendResp( IRP* pIrp );

    public:

    typedef CRpcPdoPort super;

    CDBusLocalPdo(
        const IConfigDb* pCfg = NULL );

    ~CDBusLocalPdo();

    virtual bool Unloadable()
    { return false; }

    virtual gint32 PostStart( IRP* pIrp );


    virtual gint32 Stop( IRP *pIrp );

    gint32 SubmitIoctlCmd( IRP* pIrp );

    virtual DBusHandlerResult PreDispatchMsg(
        gint32 iMsgType, DBusMessage* pMsg );

    virtual DBusHandlerResult DispatchDBusSysMsg(
        DBusMessage* pMsg );

    virtual gint32 OnModOnOffline(
        DBusMessage* pMsg );
};

typedef CAutoPtr< clsid( CDBusLocalPdo ), CDBusLocalPdo > LocPdoPtr;

class CDBusLoopbackPdo : public CRpcPdoPort
{
    // this pdo will enable the in-process call to
    // a server from a proxy interface on the other
    // thread
    protected:

    virtual gint32 ClearDBusSetting(
        IMessageMatch* pMatch )
    { return 0; }

    virtual gint32 SetupDBusSetting(
        IMessageMatch* pMatch );

    // match table for dbus sys message
    MatchPtr                m_matchDBus;

    public:

    typedef CRpcPdoPort super;

    CDBusLoopbackPdo (
        const IConfigDb* pCfg );

    ~CDBusLoopbackPdo();

    virtual gint32 SendDBusMsg(
        DBusMessage* pMsg, guint32* pdwSerial );

    virtual DBusHandlerResult PreDispatchMsg(
        gint32 iMsgType, DBusMessage* pMsg );

    virtual bool Unloadable()
    { return false; }
};

struct CStartStopPdoCtx
{
    std::map< gint32, gint32 > m_mapIdToRes;
};

struct IConnParams
{
    CfgPtr m_pParams;

    IConnParams( const IConfigDb* pCfg )
    { m_pParams = const_cast< IConfigDb* >( pCfg ); }

    IConnParams( const CfgPtr& pCfg )
    { m_pParams = const_cast< CfgPtr& >( pCfg ); }

    IConnParams()
    {};

    IConnParams( const IConnParams& rhs )
    { m_pParams = const_cast< CfgPtr& >( rhs.m_pParams ); }

    void SetCfg( IConfigDb* pCfg )
    { m_pParams = pCfg; }

    IConfigDb* GetCfg() const
    { return m_pParams; }

    inline bool IsEmpty() const
    { return m_pParams.IsEmpty(); }
};

class CConnParams : public IConnParams
{
    public:
    typedef IConnParams super;
    CConnParams()
    {}

    CConnParams( const IConfigDb* pCfg ) :
        super( pCfg )
    {}

    CConnParams( const CfgPtr& pCfg ) :
        super( pCfg )
    {}
    CConnParams( const CConnParams& rhs ) :
        super( rhs )
    {}

    bool less(const CConnParams& rhs ) const
    {
        bool ret = false;
        do{
            bool bServer = false;
            guint32 bVal1 = 0, bVal2 = 0;
            bVal1 = bServer = IsServer();
            bVal2 = rhs.IsServer();
            if( bVal1 < bVal2 )
            {
                ret = true;
                break;
            }

            if( bVal2 < bVal1 )
            {
                ret = false;
                break;
            }

            std::string strVal1, strVal2;

            strVal1 = GetSrcIpAddr();
            strVal2 = rhs.GetSrcIpAddr();

            if( strVal1 < strVal2 )
            {
                ret = true;
                break;
            }

            if( strVal2 < strVal1 )
            {
                ret = false;
                break;
            }

            bVal1 = GetSrcPortNum();
            bVal2 = rhs.GetSrcPortNum();

            if( bVal1 < bVal2 )
            {
                ret = true;
                break;
            }

            if( bVal2 < bVal1 )
            {
                ret = false;
                break;
            }

            strVal1 = GetDestIpAddr();
            strVal2 = rhs.GetDestIpAddr();

            if( strVal1 < strVal2 )
            {
                ret = true;
                break;
            }

            if( strVal2 < strVal1 )
            {
                ret = false;
                break;
            }

            bVal1 = GetDestPortNum();
            bVal2 = rhs.GetDestPortNum();

            if( bVal1 < bVal2 )
            {
                ret = true;
                break;
            }

            if( bVal2 < bVal1 )
            {
                ret = false;
                break;
            }

            if( IsWebSocket() && !bServer )
            {
                std::string strUrl1, strUrl2;
                strUrl1 = GetUrl();
                strUrl2 = rhs.GetUrl();
                if( strUrl1.empty() &&
                    strUrl2.empty() )
                {
                    ret = true;
                }
                else if( strUrl1.empty() )
                {
                    ret = true;
                }
                else if( strUrl2.empty() )
                {
                    ret = false;
                }
                else if( strUrl1 < strUrl2 )
                {
                    ret = true;
                }
                break;
            }

            ret = false;

        }while( 0 );

        return ret;
    }

    bool operator==( const CConnParams& rhs ) const
    {
        bool ret = false;
        do{
            bool bServer = false;
            guint32 bVal1 = 0, bVal2 = 0;
            bVal1 = bServer = IsServer();
            bVal2 = rhs.IsServer();
            if( bVal1 != bVal2 )
            {
                ret = false;
                break;
            }

            std::string strVal1, strVal2;
            if( bServer )
            {
                strVal1 = GetSrcIpAddr();
                strVal2 = rhs.GetSrcIpAddr();

                if( strVal1 != strVal2 )
                {
                    ret = false;
                    break;
                }

                bVal1 = GetSrcPortNum();
                bVal2 = rhs.GetSrcPortNum();

                if( bVal1 != bVal2 )
                {
                    ret = false;
                    break;
                }
            }

            strVal1 = GetDestIpAddr();
            strVal2 = rhs.GetDestIpAddr();

            if( strVal1 != strVal2 )
            {
                ret = false;
                break;
            }

            bVal1 = GetDestPortNum();
            bVal2 = rhs.GetDestPortNum();

            if( bVal1 != bVal2 )
            {
                ret = false;
                break;
            }

            bVal1 = IsWebSocket();
            if( bVal1 > 0 )
            {
                // reverse proxy
                std::string strUrl1, strUrl2;
                strUrl1 = GetUrl();
                strUrl2 = rhs.GetUrl();
                if( strUrl1 != strUrl2 )
                {
                    ret = false;
                    break;
                }
            }

            ret = true;

        }while( 0 );

        return ret;
    }

    bool operator!=( const CConnParams& rhs )
    { return !operator==(rhs); }

    inline guint32 GetSrcPortNum() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        guint32 dwPortNum = 0;
        lhs.GetIntProp(
            propSrcTcpPort, dwPortNum );
        return dwPortNum;
    }

    inline void SetSrcPortNum( guint32 dwPortNum )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetIntProp(
            propSrcTcpPort, dwPortNum );
    }

    inline guint32 GetDestPortNum() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        guint32 dwPortNum = 0;
        lhs.GetIntProp(
            propDestTcpPort, dwPortNum );
        return dwPortNum;
    }

    inline void SetDestPortNum( guint32 dwPortNum )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetIntProp(
            propDestTcpPort, dwPortNum );
    }

    inline std::string GetSrcIpAddr() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        std::string strIpAddr;
        lhs.GetStrProp(
            propSrcIpAddr, strIpAddr );
        return strIpAddr;
    }

    inline void SetSrcIpAddr(
        const std::string& strIpAddr )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetStrProp(
            propSrcIpAddr, strIpAddr );
    }

    inline std::string GetDestIpAddr() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        std::string strIpAddr;
        lhs.GetStrProp(
            propDestIpAddr, strIpAddr );
        return strIpAddr;
    }

    inline void SetDestIpAddr(
        const std::string& strIpAddr )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetStrProp(
            propDestIpAddr, strIpAddr );
    }

    inline std::string GetRouterPath() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        std::string strVal;
        lhs.GetStrProp(
            propRouterPath, strVal );
        return strVal;
    }

    inline void SetRouterPath( const std::string& strVal)
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetStrProp(
            propRouterPath, strVal );
    }

    inline bool IsSSL() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        bool bVal = false;
        lhs.GetBoolProp( propEnableSSL, bVal );
        return bVal;
    }

    inline void SetSSL( bool bEnable )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetBoolProp(
            propEnableSSL, bEnable );
    }

    inline bool IsWebSocket() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        bool bVal = false;
        lhs.GetBoolProp(
            propEnableWebSock, bVal );
        return bVal;
    }

    inline void SetWebSocket( bool bEnable )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetBoolProp(
            propEnableWebSock, bEnable );
    }

    inline bool IsCompression() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        bool bVal = false;
        lhs.GetBoolProp( propCompress, bVal );
        return bVal;
    }

    inline void SetCompression( bool bEnable )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetBoolProp(
            propCompress, bEnable );
    }


    inline bool IsConnRecover() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        bool bVal = false;
        lhs.GetBoolProp(
            propConnRecover, bVal );
        return bVal;
    }

    inline void SetConnRecover( bool bEnable )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetBoolProp(
            propConnRecover, bEnable );
    }

    inline bool IsServer() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        bool bVal = false;
        lhs.GetBoolProp( propIsServer, bVal );
        return bVal;
    }

    inline void SetServer( bool bServer )
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetBoolProp(
            propIsServer, bServer );
    }

    inline std::string GetUrl() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );
        std::string strVal;
        lhs.GetStrProp( propDestUrl, strVal );
        return strVal;
    }

    inline void SetUrl( const std::string& strVal)
    {
        CCfgOpener lhs(
            ( IConfigDb* )m_pParams );
        lhs.SetStrProp(
            propDestUrl, strVal );
    }

    inline bool HasAuth() const
    {
        CCfgOpener lhs(
            ( const IConfigDb* )m_pParams );

        bool bAuth;
        ObjPtr pObj;
        gint32 ret = lhs.GetObjPtr(
            propAuthInfo, pObj );
        if( SUCCEEDED( ret ) )
            return true;

        ret = lhs.GetBoolProp(
            propHasAuth, bAuth );
        if( SUCCEEDED( ret ) )
            return bAuth;

        return false;
    }
};

class CConnParamsProxy : public CConnParams
{
    public:
    typedef CConnParams super;

    CConnParamsProxy( const IConfigDb* pCfg ) :
        super( pCfg )
    {}

    CConnParamsProxy( const CfgPtr& pCfg ) :
        super( pCfg )
    {}

    CConnParamsProxy( const CConnParamsProxy& rhs ) :
        super( rhs )
    {}

    bool LessAuth( const CConnParamsProxy& rhs ) const
    {
        bool ret = 0;
        do{
            IConfigDb* pAuth1 = nullptr;
            IConfigDb* pAuth2 = nullptr;

            CCfgOpener oConn1( GetCfg() );
            CCfgOpener oConn2( rhs.GetCfg() );

            oConn1.GetPointer(
                propAuthInfo, pAuth1 );

            oConn2.GetPointer(
                propAuthInfo, pAuth2 );

            if( pAuth1 == pAuth2 )
            {
                ret = false;
                break;
            }

            if( pAuth1 == nullptr &&
                pAuth2 != nullptr )
            {
                ret = true;
                break;
            }

            if( pAuth1 != nullptr &&
                pAuth2 == nullptr )
            {
                ret = false;
                break;
            }

            std::string strVal1, strVal2;

            CCfgOpener oAuth1( pAuth1 );
            CCfgOpener oAuth2( pAuth2 );

            oAuth1.GetStrProp(
                propAuthMech, strVal1 );

            oAuth2.GetStrProp(
                propAuthMech, strVal2 );

            if( strVal1 < strVal2 )
            {
                ret = true;
                break;
            }

            if( strVal1 > strVal2 )
            {
                ret = false;
                break;
            }

            if( strVal1 == "krb5" )
            {
                oAuth1.GetStrProp(
                    propUserName, strVal1 );

                oAuth2.GetStrProp(
                    propUserName, strVal2 );

                if( strVal1 < strVal2 )
                {
                    ret = true;
                    break;
                }

                if( strVal1 > strVal2 )
                {
                    ret = false;
                    break;
                }

                oAuth1.GetStrProp(
                    propRealm, strVal1 );

                oAuth2.GetStrProp(
                    propRealm, strVal2 );

                std::transform( strVal1.begin(),
                    strVal1.end(), strVal1.begin(),
                    []( unsigned char c )
                    { return std::tolower( c ); } );

                std::transform( strVal2.begin(),
                    strVal2.end(), strVal2.begin(),
                    []( unsigned char c )
                    { return std::tolower( c ); } );

                if( strVal1 < strVal2 )
                {
                    ret = true;
                    break;
                }

                if( strVal1 > strVal2 )
                {
                    ret = false;
                    break;
                }

                bool bSignMsg1 = false;
                bool bSignMsg2 = false;

                oAuth1.GetBoolProp(
                    propSignMsg, bSignMsg1 );
                
                oAuth2.GetBoolProp(
                    propSignMsg, bSignMsg2 );

                if( !bSignMsg1 && bSignMsg2 )
                {
                    ret = true;
                    break;
                }
            }

            ret = false;

        }while( 0 );

        return ret;   
    }

    bool less(const CConnParamsProxy& rhs ) const
    {
        bool ret = false;
        do{
            bool bServer = false;
            guint32 bVal1 = 0, bVal2 = 0;
            bVal1 = bServer = IsServer();
            bVal2 = rhs.IsServer();
            if( bVal1 < bVal2 )
            {
                ret = true;
                break;
            }

            if( bVal2 < bVal1 )
            {
                ret = false;
                break;
            }

            std::string strVal1 =
                GetDestIpAddr();

            std::string strVal2 =
                rhs.GetDestIpAddr();

            if( strVal1 < strVal2 )
            {
                ret = true;
                break;
            }

            if( strVal2 < strVal1 )
            {
                ret = false;
                break;
            }

            bVal1 = GetDestPortNum();
            bVal2 = rhs.GetDestPortNum();

            if( bVal1 < bVal2 )
            {
                ret = true;
                break;
            }

            if( bVal2 < bVal1 )
            {
                ret = false;
                break;
            }

            strVal1 = GetDestIpAddr();
            strVal2 = rhs.GetDestIpAddr();

            if( strVal1 < strVal2 )
            {
                ret = true;
                break;
            }

            if( strVal2 < strVal1 )
            {
                ret = false;
                break;
            }

            strVal1 = GetRouterPath();
            strVal2 = rhs.GetRouterPath();

            if( strVal1 < strVal2 )
            {
                ret = true;
                break;
            }

            if( strVal2 < strVal1 )
            {
                ret = false;
                break;
            }

            bVal1 = IsWebSocket();
            bVal2 = rhs.IsWebSocket();
            if( bVal1 < bVal2 )
            {
                ret = true;
                break;
            }
            else if( bVal1 > bVal2 )
            {
                ret = false;
                break;
            }
            if( bVal1 > 0 )
            {
                // reverse proxy
                std::string strUrl1, strUrl2;
                strUrl1 = GetUrl();
                strUrl2 = rhs.GetUrl();
                if( strUrl1 < strUrl2 )
                {
                    ret = true;
                    break;
                }
                if( strUrl2 < strUrl1 )
                {
                    ret = false;
                    break;
                }
            }

            bVal1 = IsSSL();
            bVal2 = rhs.IsSSL();
            if( bVal1 < bVal2 )
            {
                ret = true;
                break;
            }
            else if( bVal1 > bVal2 )
            {
                ret = false;
                break;
            }

            bool bAuth1 = HasAuth();
            bool bAuth2 = rhs.HasAuth();
            if( !bAuth1 && bAuth2 )
            {
                ret = true;
                break;
            }

            if( ( bAuth1 && !bAuth2 ) ||
                ( !bAuth1 && !bAuth2 ) )
            {
                ret = false;
                break;
            }

            ret = LessAuth( rhs );

        }while( 0 );

        return ret;
    }
};

class CConnParamsProxySepConns :
    public CConnParamsProxy
{
    public:
    typedef CConnParamsProxy super;
    CConnParamsProxySepConns( const IConfigDb* pCfg ) :
        super( pCfg )
    {}

    CConnParamsProxySepConns( const CfgPtr& pCfg ) :
        super( pCfg )
    {}

    CConnParamsProxySepConns(
        const CConnParamsProxySepConns& rhs ) :
        super( rhs )
    {}

    bool less(const CConnParamsProxySepConns& rhs ) const
    {
        CCfgOpener oCfg( ( IConfigDb* )GetCfg() );
        IConfigDb* pRhs = rhs.GetCfg();
        CCfgOpener oCfg2( pRhs );

        stdstr strVal1;
        oCfg.GetStrProp(
            propSrcUniqName, strVal1 );

        stdstr strVal2;
        oCfg2.GetStrProp(
            propSrcUniqName, strVal2 );
        if( strVal1 < strVal2 )
            return true;

        if( strVal1 > strVal2 )
            return false;

        return super::less( rhs );
    }
};

}

namespace std
{
    using namespace rpcf;
    template<>
    struct less<CConnParamsProxySepConns>
    {
        bool operator()(
            const CConnParamsProxySepConns& k1,
            const CConnParamsProxySepConns& k2) const
        {
            return k1.less( k2 );
        }
    };
    template<>
    struct less<CConnParamsProxy>
    {
        bool operator()(
            const CConnParamsProxy& k1,
            const CConnParamsProxy& k2) const
        {
            return k1.less( k2 );
        }
    };

    template<>
    struct less<CConnParams>
    {
        bool operator()(
            const CConnParams& k1,
            const CConnParams& k2) const
        {
            return k1.less( k2 );
        }
    };
}

namespace rpcf
{

using ADDRID_MAP =
    std::map< CConnParamsProxy, guint32 >;

using ADDRID_MAP2 =
    std::map< CConnParamsProxySepConns, guint32 >;

struct AddrIdMap
{
    bool m_bSepConns = false;
    ADDRID_MAP* m_pMap = nullptr;
    ADDRID_MAP2* m_pMap2 = nullptr;

    ~AddrIdMap()
    {
        if( m_bSepConns )
            delete m_pMap2;
        else
            delete m_pMap;
        m_pMap2 = nullptr;
        m_pMap = nullptr;
    }

    inline void Initialize( bool bSepConns )
    {
        m_bSepConns = bSepConns;
        if( bSepConns )
           m_pMap2 = new ADDRID_MAP2;
        else
           m_pMap = new ADDRID_MAP;
    }

    gint32 Find(
        CConnParamsProxy& ocp,
        guint32& dwPortId );

    gint32 Add(
        CConnParamsProxy& ocp,
        guint32 dwPortId );

    gint32 Erase(
        CConnParamsProxy& ocp );
};

class CDBusBusPort : public CGenericBusPortEx
{
    // the pending calls waiting for dispatching
    // only calls to registered interface will
    // enter this queue. others are discarded
    std::deque<DMsgPtr> m_quePendingMsgs;

    // the dbus connection
    DBusConnection      *m_pDBusConn;
    TaskletPtr          m_pFlushTask;

    // local port attached to this bus
    gint32              m_iLocalPortId;
    gint32              m_iLpbkPortId;

    // proxy port attached to this bus
    std::vector<gint32> m_vecProxyPortIds;

    MatchPtr            m_pMatchDisconn;

    MatchPtr            m_pMatchLpbkProxy;
    MatchPtr            m_pMatchLpbkServer;

    std::map< std::string, gint32 > m_mapRules;

    AddrIdMap m_mapAddrToId;

    gint32 CreateLocalDBusPdo(
        const IConfigDb* pConfig,
        PortPtr& pNewPort );

    gint32 CreateRpcProxyPdoShared(
        const IConfigDb* pConfig,
        PortPtr& pNewPort,
        EnumClsid iClsid );

    inline gint32 CreateRpcProxyPdoLpbk(
        const IConfigDb* pConfig,
        PortPtr& pNewPort )
    {
        return CreateRpcProxyPdoShared(
            pConfig, pNewPort,
            clsid( CDBusProxyPdoLpbk ) );
    }

    inline gint32 CreateRpcProxyPdo(
        const IConfigDb* pConfig,
        PortPtr& pNewPort )
    {
        return CreateRpcProxyPdoShared(
            pConfig, pNewPort,
            clsid( CDBusProxyPdo ) );
    }

    gint32 CreateLoopbackPdo(
        const IConfigDb* pConfig,
        PortPtr& pNewPort );

    gint32 ReplyWithError(
        DBusMessage* pMsg );

    virtual gint32 GetChildPorts(
        std::vector< PortPtr >& vecChildPdo );

    void ReleaseDBus();

    gint32 SchedulePortsAttachNotifTask(
        std::vector< PortPtr >& vecChildPdo,
        guint32 dwEventId,
        IRP* pMasterIrp = nullptr );

    gint32 AddRemoveBusNameLpbk(
        const std::string& strName,
        bool bRemove );

    gint32 AddBusNameLpbk(
        const std::string& strName );

    gint32 RemoveBusNameLpbk(
        const std::string& strName );

    gint32 BuildModOnlineMsgLpbk(
        const std::string& strModName,
        bool bOnline, DMsgPtr& pMsg );

    gint32 AsyncStopDBusConn(
        IRP* pIrp, gint32 iPos );

    public:

    typedef CGenericBusPortEx super;

    CDBusBusPort(
        const IConfigDb* pConfig );

    ~CDBusBusPort();

    // this is our dbus message dispatcher
    // pData points to this object
    static DBusHandlerResult DBusMessageCallback(
         DBusConnection *pConn,
         DBusMessage *pMsg,
         void *pData);

    DBusHandlerResult OnMessageArrival(
        DBusMessage *pMsg );

    gint32 SendLpbkMsg(
        DBusMessage *pMsg, guint32* pdwSerial );

    gint32 SendDBusMsg(
        DBusMessage *pMsg, guint32* pdwSerial );

    virtual gint32 BuildPdoPortName(
        IConfigDb* pCfg,
        std::string& strPortName );

    virtual gint32 CreatePdoPort(
        IConfigDb* pConfig,
        PortPtr& pNewPort );

    virtual void RemovePdoPort(
            guint32 iPortId );

    // the only exit of dbus message out of
    // this application
    gint32 SendData( const BufPtr& pMsg );

    // IPort methods
    gint32 Start( IRP* pIrp );
    gint32 Stop( IRP* pIrp );

    // IEventSink method
    gint32 OnEvent( EnumEventId iEvent,
            LONGWORD dwParam1 = 0,
            LONGWORD dwParam2 = 0,
            LONGWORD* pData = NULL  );

    // handler of the func irps
    gint32 OnSubmitIrp( IRP* pIrp )
    { return -ENOTSUP; }

    gint32 PostStart( IRP* pIrp );
    gint32 OnPortReady( IRP* pIrp );
    void   OnPortStartFailed(
        IRP* pIrp, gint32 ret );

    gint32 HandleDBusDisconn();

    // loopback related methods
    gint32 InitLpbkMatch();
    gint32 FilterLpbkMsg(
        DBusMessage* pMsg, guint32* pdwSerial );

    gint32 ScheduleLpbkTask(
        MatchPtr& pFilter,
        DBusMessage *pDBusMsg,
        guint32* pdwSerial );

    DBusHandlerResult OnLpbkMsgArrival(
        DBusMessage* pMsg );

    // methods from CObjBase
    gint32 GetProperty(
            gint32 iProp,
            CBuffer& oBuf ) const;

    gint32 SetProperty(
            gint32 iProp,
            const CBuffer& oBuf );

    gint32 EnumProperties(
        std::vector< gint32 >& vecProps ) const;

    virtual gint32 AllocIrpCtxExt(
        IrpCtxPtr& pIrpCtx,
        void* pContext = nullptr ) const;

    virtual gint32 ReleaseIrpCtxExt(
        IrpCtxPtr& pIrpCtx,
        void* pContext = nullptr );

    virtual gint32 OnCompleteSlaveIrp(
        IRP* pMaster, IRP* pSlave );

    gint32 RegBusName(
        const std::string& strName,
        guint32 dwFlags );

    gint32 ReleaseBusName(
        const std::string& strName );

    gint32 AddRules(
        const std::string& strRules );

    gint32 RemoveRules(
        const std::string& strRules );

    gint32 IsDBusSvrOnline(
        const std::string& strDest );

    gint32 IsRegBusName(
        const std::string& strDest );

	virtual gint32 PreStop( IRP* pIrp );
    static guint32 LabelMessage( DMsgPtr& pMsg );
};

class CDBusConnFlushTask 
    : public CTasklet
{
    CIoManager* m_pMgr;
    CDBusBusPort* m_pBusPort;
    DBusConnection* m_pDBusConn;
    guint32     m_dwIntervalMs;
    HANDLE      m_hTimer;

    public:
    typedef CTasklet super;
    CDBusConnFlushTask( const IConfigDb* pCfg );
    ~CDBusConnFlushTask();
    gint32 operator()( guint32 dwContext );
    gint32 Stop();
};


class CDBusBusDriver : public CGenBusDriverEx
{
    public:
    typedef CGenBusDriverEx super;
    CDBusBusDriver( const IConfigDb* pCfg );
	gint32 Probe( IPort* pLowerPort,
        PortPtr& pNewPort,
        const IConfigDb* pConfig = NULL );
};

typedef CAutoPtr< clsid( CDBusBusPort ), CDBusBusPort > DBPortPtr;

}
