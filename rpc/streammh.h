/*
 * =====================================================================================
 *
 *       Filename:  streammh.h
 *
 *    Description:  stream support for multi-hop 
 *
 *        Version:  1.0
 *        Created:  03/10/2020 09:22:44 AM
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

#pragma once

#include "rpcroute.h"
#include "stmrelay.h"

namespace rpcf
{

class CStreamServerRelayMH :
    public CStreamRelayBase< CStreamServer >
{
    gint32 OnClose2(
        HANDLE hChannel,
        gint32 iStmId,
        IEventSink* pCallback );

    gint32 OnCloseInternal(
        HANDLE hChannel,
        IEventSink* pCallback );

    gint32 SendCloseToAll( IEventSink* pCallback );
    gint32 ResumePreStop( IEventSink* pCallback )
    {   return super::OnPreStop( pCallback ); }

    protected:
    gint32 OnPreStop( IEventSink* pCallback );

    public:
    typedef CStreamRelayBase< CStreamServer > super;

    CStreamServerRelayMH( const IConfigDb* pCfg )
        : _MyVirtBase( pCfg ), super( pCfg )
    {;}

    CRpcRouter* GetParent() const
    {
        CRpcInterfaceServer* pMainIf =
            ObjPtr( ( CObjBase* )this );

        if( pMainIf == nullptr )
            return nullptr;

        return pMainIf->GetParent();
    }

    const EnumClsid GetIid() const
    { return iid( IStreamMH ); }

    gint32 GetStreamsByBridgeProxy(
        CRpcServices* pProxy,
        std::vector< HANDLE >& vecHandles );

    gint32 FetchData_Server(
        IConfigDb* pDataDesc,           // [in]
        gint32& fd,                     // [out]
        guint32& dwOffset,              // [in, out]
        guint32& dwSize,                // [in, out]
        IEventSink* pCallback );

    // request to the remote server completed
    gint32 OnFetchDataComplete(
        IEventSink* pCallback,
        IEventSink* pIoReqTask,
        IConfigDb* pContext );

    gint32 OnOpenStreamComplete(
        IEventSink* pCallback,
        IEventSink* pIoReqTask,
        IConfigDb* pContext );

    bool SupportIid( EnumClsid iIfId ) const
    {
        if( iIfId == iid( IStreamMH ) )
            return true;
        return false;
    }

    using IStream::CreateUxStream;

    gint32 CreateUxStream(
        IConfigDb* pDataDesc,
        gint32 iFd, EnumClsid iClsid,
        bool bServer,
        InterfPtr& pIf );

    inline gint32 GetUxStream(
        gint32 iStmId, InterfPtr& pIf ) const
    {
        CStdRMutex oIfLock( GetLock() );
        std::map< gint32, HANDLE >::const_iterator
            itr = m_mapStmIdToHandle.find( iStmId );

        if( itr == m_mapStmIdToHandle.end() )
            return -EINVAL;

        return GetUxStream( itr->second, pIf );
    }

    // the up-stream request to close or error
    // occurs
    virtual gint32 OnClose( gint32 iStmId,
        IEventSink* pCallback = nullptr );

    // the down-stream node request to close or
    // error occues
    virtual gint32 OnClose( HANDLE hChannel,
        IEventSink* pCallback = nullptr );
};

/**
* @name CRpcTcpBridgeProxyStream
* @{ */
/**
 * To forward incoming stream packets to/from the
 * bridge from/to the bridge proxy within the
 * bridge router, with the flow control.
 * @} */

class CRpcTcpBridgeProxyStream :
    public CUnixSockStmProxyRelay
{
    gint32  m_iBdgePrxyStmId = 0;
    gint32  m_iBdgeStmId = 0;
    InterfPtr m_pBridgeProxy;
    CFlowControl m_oFlowCtrlRmt;

    public:
    typedef CUnixSockStmProxyRelay super;

    static CfgPtr InitCfg( const IConfigDb* pCfg );
    CRpcTcpBridgeProxyStream(
        const IConfigDb* pCfg );

    virtual gint32 StartTicking(
        IEventSink* pContext );

    virtual gint32 OnPostStart(
        IEventSink* pContext );

    virtual gint32 OnFlowControl();

    inline gint32 IncTxBytes( guint32 dwSize )
    { return m_oFlowCtrl.IncTxBytes( dwSize ); }

    inline gint32 IncTxBytesRemote( guint32 dwSize )
    { return m_oFlowCtrlRmt.IncTxBytes( dwSize ); }

    using CUnixSockStream::GetParent;
    inline InterfPtr GetBridge() const
    {
        InterfPtr pIf;
        this->GetParent( pIf );
        return pIf;
    }

    inline InterfPtr GetBridgeProxy() const
    { return m_pBridgeProxy; }

    inline gint32 GetStreamId( bool bProxy ) const
    {
        if( bProxy )
            return m_iBdgePrxyStmId;
        return m_iBdgeStmId;
    }

    virtual gint32 OnFCLifted();

    virtual gint32 GetReqToLocal(
        guint8& byToken, BufPtr& pBuf );

    virtual gint32 GetReqToRemote(
        guint8& byToken, BufPtr& pBuf );

    virtual gint32 OnFlowControlRemote();

    virtual gint32 OnFCLiftedRemote();

    virtual gint32 ForwardToRemote(
        guint8 byToken, BufPtr& pBuf );

    virtual gint32 OnPostStop(
        IEventSink* pCallback );

    virtual gint32 OnDataReceived(
        CBuffer* pBuf );

    virtual gint32 OnDataReceivedRemote(
        CBuffer* pBuf );

    virtual gint32 OnProgress(
        CBuffer* pBuf )
    { return 0; }

    virtual gint32 OnProgressRemote(
        CBuffer* pBuf )
    { return 0; }

    virtual IStream* GetParent()
    {
        CStreamServerRelayMH* pIf =
            ObjPtr( m_pParent );
        return ( IStream* )pIf;
    }

    virtual gint32 OnPreStop(
        IEventSink* pCallback );

    virtual gint32 SendBdgeStmEvent(
        guint8 byToken, BufPtr& pBuf );
};

struct CIfUxRelayTaskHelperMH : 
    public CIfUxRelayTaskHelper
{
    public:

    typedef CIfUxRelayTaskHelper super;
    CIfUxRelayTaskHelperMH(
        IEventSink* pTask ) : super( pTask )
    {;}

    gint32 GetBridgeIf(
        InterfPtr& pIf, bool bProxy );

    virtual gint32 WriteTcpStream(
        gint32 iStreamId,
        CBuffer* pSrcBuf,
        guint32 dwSizeToWrite,
        IEventSink* pCallback );

    gint32 ReadTcpStream(
        gint32 iStreamId,
        BufPtr& pSrcBuf,
        guint32 dwSizeToWrite,
        IEventSink* pCallback );
};


class CIfUxSockTransRelayTaskMH :
    public CIfUxSockTransRelayTask
{
    public:
    typedef CIfUxSockTransRelayTask super;

    CIfUxSockTransRelayTaskMH(
        const IConfigDb* pCfg );
    CRpcTcpBridgeProxyStream* GetOwnerIf();
    gint32 RunTask();
};

class CIfTcpStmTransTaskMH :
    public CIfTcpStmTransTask
{
    public:

    typedef CIfTcpStmTransTask super;

    CIfTcpStmTransTaskMH(
        const IConfigDb* pCfg );

    CRpcTcpBridgeProxyStream* GetOwnerIf();
    gint32 RunTask();
    gint32 PostEvent( BufPtr& pBuf );
};

class CIfUxListeningRelayTaskMH :
    public CIfUxListeningRelayTask
{
    public:
    typedef CIfUxListeningRelayTask super;
    CIfUxListeningRelayTaskMH(
        const IConfigDb* pCfg );
    gint32 RunTask();
    gint32 PostEvent( BufPtr& pBuf );
    gint32 Pause();
    gint32 Resume();
};

class CIfStartUxSockStmRelayTaskMH :
    public CIfStartUxSockStmRelayTask
{
    public:
    typedef CIfStartUxSockStmRelayTask super;

    CIfStartUxSockStmRelayTaskMH( const IConfigDb* pCfg ) :
        super( pCfg )
    { SetClassId( clsid( CIfStartUxSockStmRelayTaskMH ) ); }
    gint32 OnTaskComplete( gint32 iRet );
};

}
