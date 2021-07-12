/*
 * ============================================================================
 *
 *       Filename:  namespc.h
 *
 *    Description:  name definitions for interface, object, module and
 *                  registries
 *
 *        Version:  1.0
 *        Created:  11/20/2016 09:05:17 AM
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
 * ============================================================================
 */

#pragma once

#include <dbus/dbus.h>
#include <objfctry.h>


namespace rpcf
{

#define REG_DRV_ROOT                     "/drivers"
#define REG_PRELOADABLE_ROOT             "/preloadable"

#define DBUS_SYS_INTERFACE              DBUS_INTERFACE_DBUS
#define DBUS_SYS_OBJPATH                DBUS_PATH_DBUS

#define PORT_CLASS_DBUS_PROXY_PDO       "DBusProxyPdo"
#define PORT_CLASS_DBUS_PROXY_FDO       "DBusProxyFdo"
#define PORT_CLASS_RPC_TCP_FIDO         "RpcTcpFido"
#define PORT_CLASS_TCP_STREAM_PDO       "TcpStreamPdo"
#define PORT_CLASS_TCP_STREAM_PDO2      "TcpStreamPdo2"
#define PORT_CLASS_RPC_NATPROTO_FDO     "RpcNativeProtoFdo"
#define PORT_CLASS_LOCALDBUS_PDO        "DBusLocalPdo"
#define PORT_CLASS_LOCALDBUS            "DBusBusPort"
#define PORT_CLASS_RPC_TCPBUS           "RpcTcpBusPort"
#define PORT_CLASS_LOOPBACK_PDO         "DBusLoopbackPdo"
#define PORT_CLASS_UXSOCK_BUS           "UnixSockBusPort"
#define PORT_CLASS_UXSOCK_STM_PDO       "UnixSockStmPdo"
#define PORT_CLASS_DBUS_PROXY_PDO_LPBK  "DBusProxyPdoLpbk"

// registry path for event subscription from the
// IoManager
#define REG_IO_EVENTMAP_DIR_FMT \
     "/bulletin_board/%s/connpoint_container"

#define BUILD_STRING2( fmt, strVal, strVal2, strRet ) \
do{ \
    char szBuf[ REG_MAX_NAME << 1 ]; \
    szBuf[ sizeof( szBuf ) - 1 ] = 0; \
    snprintf( szBuf, sizeof( szBuf ) - 1,\
        fmt, strVal.c_str(), strVal2.c_str() ); \
    strRet = szBuf; \
}while( 0 )

#define BUILD_STRING1( fmt, strVal, strRet ) \
do{ \
    char szBuf[ REG_MAX_NAME << 1 ]; \
    szBuf[ sizeof( szBuf ) - 1 ] = 0; \
    snprintf( szBuf, sizeof( szBuf ) - 1,\
        fmt, strVal.c_str() ); \
    strRet = szBuf; \
}while( 0 )

inline std::string REG_IO_EVENTMAP_DIR(
    const std::string& strModName )
{
    std::string strRet;

    BUILD_STRING1( REG_IO_EVENTMAP_DIR_FMT,
        strModName, strRet );

    return strRet;
}

#define DBUS_NAME_PREFIX                "org.rpcf."
#define DBUS_IF_NAME_FMT                DBUS_NAME_PREFIX"%s.%s"
#define DBUS_DEST_PREFIX_FMT            DBUS_NAME_PREFIX"%s"

inline std::string DBUS_IF_NAME(
    const std::string& strIfName )
{
    std::string strRet;
    std::string strIfSpc = std::string( "Interf" );

    BUILD_STRING2( DBUS_IF_NAME_FMT,
        strIfSpc , strIfName, strRet );

    return strRet;
}

inline std::string IF_NAME_FROM_DBUS(
    const std::string& strIfName )
{
    // reverse of DBUS_IF_NAME
    std::string strRet;
    std::string strIfSpc = DBUS_NAME_PREFIX;
    strIfSpc += std::string( "Interf." );

    size_t pos = strIfName.find( strIfSpc, 0 );
    if( pos == std::string::npos )
        return strIfName;

    if( pos + strIfSpc.size() >= strIfName.size() )
        return strIfName;

    return strIfName.substr( pos + strIfSpc.size(),
        255 - strIfSpc.size() );
}

inline std::string DBUS_DESTINATION(
    const std::string& strModName )
{
    std::string strRet;
    BUILD_STRING1( DBUS_DEST_PREFIX_FMT,
        strModName, strRet );

    return strRet;
}

#define LOOPBACK_DESTINATION   "org.rpcf.loopback"

/**
* @name Modue name module name is used to register
* on the dbus as a well-known address, to receive
* messages from other modules.
*
* @{ It usually consists 3 part, one is the prefix
* `Mod_', the second is the base name, which is
* better derived from the class name with capatal
* 'C' and can be found by CoGetClassId, and the
* third is the instance number, combined with
* underline.
*
* The module name usually comes from the config
* file, except some system modules*/
/**  @} */

// module name for rpc router which will serve as
// the dbus destination where the requst to rpc
// router will send
#define MODNAME_RPCROUTER               "rpcrouter"
#define MODNAME_INVALID                 "Mod_Invalid"

// the interface that provide req forwarder
// service
#define IFNAME_REQFORWARDER             "CRpcReqForwarder"
#define IFNAME_TCP_BRIDGE               "CRpcTcpBridge"

#define IFNAME_REQFORWARDERAUTH         "CRpcReqForwarderAuth"

#define IFNAME_INVALID                  "If_Invalid" 
// the obj name which will provide two interfaces
// CRpcReqForwarder and CRpcReqForwarderProxy 

#define OBJNAME_REQFWDR                 "RpcReqForwarderImpl"
#define OBJNAME_TCP_BRIDGE              "RpcTcpBridgeImpl"
#define OBJNAME_ROUTER                  "RpcRouterManagerImpl"
#define OBJNAME_ROUTER_BRIDGE           "RpcRouterBridgeImpl"
#define OBJNAME_ROUTER_REQFWDR          "RpcRouterReqFwdrImpl"

#define OBJNAME_REQFWDR_AUTH            "RpcReqForwarderAuthImpl"
#define OBJNAME_TCP_BRIDGE_AUTH         "RpcTcpBridgeAuthImpl"

#define OBJNAME_ROUTER_BRIDGE_AUTH      "RpcRouterBridgeAuthImpl"
#define OBJNAME_ROUTER_REQFWDR_AUTH     "RpcRouterReqFwdrAuthImpl"

#define OBJNAME_IOMANAGER               "Obj_IoManager"
#define OBJNAME_INVALID                 "Obj_Invalid"

#define DBUS_DEF_OBJ                    OBJNAME_IOMANAGER

#define DBUS_DEFAULT_OBJ_PATH_FMT       "/org/rpcf/%s/objs/%s"

#define DBUS_RESP_KEEP_ALIVE            "KeepAlive"

#define DBUS_RMT_CANCEL_REQ             "NotifyCancelReq"

// obj path consists of three part, one is the
// fixed part, and the modname, and then the
// objname. The prefix must be consistant with
// the DBUS_DESTINATION, except that all '.' are
// replaced with '/' 
inline std::string DBUS_OBJ_PATH(
    const std::string& strModName,
    std::string strObjName = "" )
{
    std::string strRet;

    if( strModName.find_first_of( "./" ) !=
        std::string::npos )
        return std::string( "" );

    if( strObjName.empty() )
        strObjName = DBUS_DEF_OBJ;

    BUILD_STRING2( DBUS_DEFAULT_OBJ_PATH_FMT,
        strModName, strObjName, strRet );

    return strRet;
}

inline std::string DBUS_DESTINATION2(
    const std::string& strModName,
    const std::string& strObjName )
{
    if( strModName.find_first_of( "./" ) !=
        std::string::npos )
        return std::string( "" );

    std::string strRet =
        DBUS_OBJ_PATH( strModName, strObjName );

    std::replace( strRet.begin(),
        strRet.end(), '/', '.' );

    while( strRet.size() > 0 &&
        strRet[ 0 ] == '.' )
        strRet.erase( strRet.begin() );

    return strRet;
}

#define SYS_METHOD_SENDDATA             "RpcCall_SendData"
#define SYS_METHOD_FETCHDATA            "RpcCall_FetchData"
#define SYS_METHOD_FORWARDREQ           "RpcCall_ForwardRequest"
#define SYS_METHOD_OPENRMTPORT          "RpcCall_OpenRemotePort"
#define SYS_METHOD_CLOSERMTPORT         "RpcCall_CloseRemotePort"
#define SYS_METHOD_ENABLERMTEVT         "RpcCall_EnableRemoteEvent"
#define SYS_METHOD_LOCALLOGIN           "RpcCall_LocalLogin"
#define SYS_METHOD_DISABLERMTEVT        "RpcCall_DisableRemoteEvent"
#define SYS_METHOD_CLEARRMTEVTS         "RpcCall_ClearRemoteEvents"
#define SYS_METHOD_CHECK_ROUTERPATH     "RpcCall_CheckRouterPath"
#define SYS_METHOD_HANDSAKE             "RpcCall_Handshake"

#define SYS_METHOD_USERCANCELREQ        "RpcCall_UserCancelRequest"
#define SYS_METHOD_FORCECANCELREQS      "RpcCall_ForceCancelRequests"
#define SYS_METHOD_PAUSE                "RpcCall_Pause"
#define SYS_METHOD_RESUME               "RpcCall_Resume"
#define SYS_METHOD_KEEPALIVEREQ         "RpcCall_KeepAliveRequest"

#define SYS_EVENT_KEEPALIVE             "RpcEvt_KeepAlive"
#define SYS_EVENT_FORWARDEVT            "RpcEvt_ForwardEvent"
#define SYS_EVENT_RMTSVREVENT           "RpcEvt_RmtSvrEvent"
#define SYS_EVENT_REFRESHREQLIMIT       "RpcEvt_RefreshReqLimit"

#define IF_METHOD_ENABLEEVT             "IfReq_EnableEvt"
#define IF_METHOD_DISABLEEVT            "IfReq_DisableEvt"
#define IF_METHOD_LISTENING             "IfReq_Listening"
#define IF_EVENT_PROGRESS               "IfEvt_Progress"

// Bridge-specific command between proxy and
// server
#define BRIDGE_METHOD_OPENSTM           "Brdg_Req_OpenStream"
#define BRIDGE_METHOD_CLOSESTM          "Brdg_Req_CloseStream"
#define BRIDGE_EVENT_INVALIDSTM         "Brdg_Evt_InvalidStream"
#define BRIDGE_EVENT_SVROFF             "Brdg_Evt_ServerOffline"
#define BRIDGE_EVENT_MODOFF             "Brdg_Evt_ModOffline"

#define USER_METHOD_PREFIX              "UserMethod_"
#define SYS_METHOD_PREFIX              	"RpcCall_"
#define USER_EVENT_PREFIX               "UserEvent_"
#define SYS_EVENT_PREFIX                "RpcEvt_"

inline std::string SYS_EVENT(
    const std::string& strMethod )
{
    std::string strName = SYS_EVENT_PREFIX;
    strName += strMethod; 
    return strName;
}

inline std::string USER_EVENT(
    const std::string& strMethod )
{
    std::string strName = USER_EVENT_PREFIX;
    strName += strMethod; 
    return strName;
}

inline std::string USER_METHOD(
    const std::string& strMethod )
{
    std::string strName = USER_METHOD_PREFIX;
    strName += strMethod; 
    return strName;
}

inline std::string SYS_METHOD(
    const std::string& strMethod )
{
    std::string strName = SYS_METHOD_PREFIX;
    strName += strMethod; 
    return strName;
}

}
