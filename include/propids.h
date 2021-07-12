/*
 * =====================================================================================
 *
 *       Filename:  propids.h
 *
 *    Description:  definitions of property ids
 *
 *        Version:  1.0
 *        Created:  06/18/2016 09:32:01 PM
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
#include <nsdef.h>

namespace rpcf
{

enum EnumPropId : gint32
{
    // the lower 0x100 for
    // positional parameters
    //
    // Interface related property
    propSerialNumber = 0x1000,
    propSrcDBusName,
    propDestDBusName,
    propSrcUniqName,
    propDestUniqName,
    propIfName,
    propObjPath,
    propMethodName,
    propMatchType,      // type: gint32
    propSrcIpAddr,
    propDestIpAddr,
    propSrcTcpPort,
    propDestTcpPort,

    // Port related property
    propDrvName = 0x2000,
    propBusName,        // type: string
    propBusClass,       // type: string
    propBusId,          // type: guint32 
    propProtocol,       // type: string as the protocol between the router, include `native', `ws', `http2' ...
    propPortName,
    propPortClass,
    propPortId,         // type: guint32
    propPortState,      // type: guint32
    propFdoName,
    propFdoClass,
    propFdoId,          // type: guint32
    propPdoName,
    propPdoClass,       // type: a string as the pdo port class
    propPdoId,          // type: guint32
    propIpAddr,         // type: string
    propPortType,       // type: guint32
    propIsBusPort,      // type: bool
    propModName,
    propRegPath,        // type: string
    propPath1,          // type: string
    propPath2,
    propPath3,
    propPortPtr,        // type: ObjPtr to the port interested
    propUpperPortPtr,   // type: ObjPtr
    propLowerPortPtr,   // type: ObjPtr
    propBusPortPtr,     // type: ObjPtr to the bus port
    propEventMapPtr,    // type: ObjPtr to CStlEventMap
    propMsgPtr,         // type: DMsgPtr to the DBusMessage or CfgPtr,
                        //      depending on the type flag of the container CBuffer

    // connection points
    propStartStopEvent, // type: a connection point for port start/stop event 
    propDBusSysEvent,   // type: a connection point for local dbus daemon online/offline
    propDBusModEvent,   // type: a connection point for local dbus module online/offline
    propRmtSvrEvent,    // type: a connection point for online/offline of the remote server
    propRmtModEvent,    // type: a connection point for online/offline of the remote module
    propAdminEvent,     // type: a connection point for online/offline of the remote module

    propSelfPtr,        // type: ObjPtr as the current port itslef
    propPdoPtr,         // type: ObjPtr to th pdo port
    propFdoPtr,         // type: ObjPtr to the fdo port

    // misc
    propRpcSvrPortNum = 0x3000,  // type: integer
    propObjPtr,         // type: ObjPtr of IPort*
    propMatchMap,       // type: ObjPtr of CStdMap<ObjPtr, MatchPtr> and
                        //      ObjPtr is pointer to the interface, which will
                        //      be used as handle for removal

    propIoMgr,          // type: ObjPtr of pointer to CIoManager
    propEventSink,      // type: ObjPtr of pointer to IEventSink
    propConfigPath,     // type: string, for "driver.json" file path
    propDrvPtr,         // type: ObjPtr of IPortDriver*
    propDBusConn,       // type: intptr to the DBusConnection
    propParamList,      // type: ObjPtr to CStlIntVector, used to pass parameters in CTasklet::OnEvent
    propParamCount,     // type: guint32 sizeof the propParamList

    propTimerParamList, // type: ObjPtr to CStlIntVector, used to pass parameters in CTasklet::OnEvent for eventTimeout
    propNotifyParamList, // type: ObjPtr to CStlIntVector, used to pass parameters in CTasklet::OnEvent for eventRpcNotify
    propIrpPtr,         // type: ObjPtr to Irp
    propMasterIrp,      // type: ObjPtr to the Master Irp
    propPortPtrs,       // type: ObjPtr to CStlPortVector
    propEventId,        // type: guint32
    propMaxTaskThrd,    // type: guint32 as the max count of task thread
    propIntervalSec,    // type: guint32 for time interval in second
    propRetries,        // type: guint32 for times to retry
    propRetriesBackup,  // type: guint32 as backup of propRetries
    propTimerId,        // type: guint32 as a timer id
    propParentPtr,      // type: ObjPtr to a generic obj pointer which contains the parent object
    propSingleIrp,      // type: bool to tell CRpcPdoPort to complete a event irp in the match map
    propObjId,          // type: guint64 for a unique object id
    propClsid,          // type: guint32 for the class id
    propRefCount,       // type: guint32 for the reference count of the object
    propFuncAddr,       // type: ObjPtr to a task as a resume point
    propMaxIrpThrd,     // type: guint32 as the max count of irp complete thread
    propIfPtr,          // type: ObjPtr of pointer to a IInterfaceState
    propTaskPtr,        // type: ObjPtr of pointer to a tasklet
    propParentTask,     // type: ObjPtr of pointer to a parent tasklet
    propQueuedReq,      // type: bool value to indicate InvokeMethod to service the req one by one or concurrently
    propMatchPtr,       // type: ObjPtr of pointer to a message match
    propRouterPtr,      // type: ObjPtr of pointer to a CRpcRouter
    propSubmitPdo,      // type: boolean, to indicate whether the request will go direct to pdo instead of the whole port stack

    // if task control bits
    propNotifyClient,   // type: bool value to indicate whether to notify the client
    propContext,        // type: guint32 for a context param
    propRunning,        // type: bool value to indicate a tasklet is running, that is whether the runtask is called
    propNoResched,      // type: bool value to indicate no Reschedule after the sub-task completed
    propCanceling,      // type: bool value to indicate a taskis being canceled
    propCancelTid,      // type: bool value to indicate which thread the cancel method is running on

    propReturnValue,    // type: gint32 value as the return value of a request
    propCallFlags,      // type: guint32 contains flags for the call
    propCallOptions,    // type: guint32 contains flags for the call
    propKeepAliveSec,   // type: guint32 contains the seconds to wait before heartbeat are sent
    propTimeoutSec,     // type: guint32 contains the seconds to wait before the request is timeout
    propTaskId,         // type: guint64 contains the taskid in the call request
    propRmtTaskId,      // type: guint64 contains the taskid for canceling purpose
    propRespPtr,        // type: ObjPtr contains the response CfgPtr 
    propReqPtr,         // type: ObjPtr contains the request CfgPtr 
    propFilePath,       // type: string for file path
    propFilePath1,      // type: string for second file path 
    propFd,             // type: gint32 for file descriptor
    propStreamId,       // type: gint32 for the stream id
    propByteCount,      // type: guint32 for the size to transfer
    propPeerStmId,      // type: gint32 for the stream id from the peer
    propSeqNo,          // type: guint32 for a sequence number
    propSeqNo2,         // type: guint32 for a sequence number
    propCmdId,          // type: guint32 for a command id
    propProtoId,        // type: guint32 for a protocol id
    propReqType,        // type: guint32 for a request type, same as DMsgPtr::GetType()
    propSockPtr,        // type: ObjPtr, contains a pointer to the socket
    propUserName,       // type: ObjPtr, contains a string of user name
    propNonFd,          // type: bool for non file descriptor in SendData/FetchData transfer
    propPasswd,         // type: string, contains a string of password hash
    propThreadId,       // type: gint32, contains a thread id
    propIfStateClass,   // type: class id of the interface-state
    propObjList,        // type: ObjPtr to a CStlObjVector object
    propValList,        // type: ObjPtr to whatever list
    propIid,            // type: an interface id of EnumClsid type, used in CMessageMatch
    propTaskState,      // type: an integer of EnumTaskState type, in CIfParallelTask
    propPausable,       // type: a bool value to indicate the match is for an interface which can be paused
    propSysMethod,      // type: a bool value to indicate if the call is from a built-in interface
    propPauseOnStart,   // type: a bool value to indicate the server to pause on start
    propQueSize,        // type: guint32 for the pending message size for incoming requests
    propStreaming,      // type: a bool value to indicate if current FetchData call is for streaming
    propMsgCount,       // type: a invoke message counter for used by CMessageCounter
    propMsgOutCount,    // type: a send message counter for used by CMessageCounter
    propMsgRespCount,   // type: a resp message counter for used by CMessageCounter
    propDummyMatch,     // type: the match is for state transition only, not for matching purpose
    propObjInstName,    // type: the instance name of an object, used to construct the objpath
    propSvrInstName,    // type: the instance name of a server, used to construct the objpath
    propRouterRole,     // type: gint32, the role of the router instance: 01 as a forwarder, 02 as a server,
                        //       03 or not exist as both forwarder and server, 
    propListenSock,     // type: bool to indicate whether the tcpbusport need to create the listening socket for incoming requests
    propTransGrpPtr,    // type: ObjPtr to the transaction group object
    propOnline,         // type: bool to tell if the remote module for the CRouterLocalMatch is online on the forwarder side
    propKAParamList,    // type: ObjPtr to CStlIntVector, used to pass parameters in CTasklet::OnEvent for eventKeepAlive
    propIsServer,       // type: bool to indicate if the stream pdo is on server side or proxy side
    propRxBytes,        // type: guint64 of bytes received
    propTxBytes,        // type: guint64 of bytes sent
    propRxPkts,         // type: guint32 of packets received
    propTxPkts,         // type: guint32 of packets sent
    propExtInfo,        // type: ObjPtr to a configdb with extended information
    propListenOnly,     // type: bool to tell if the CUnixSockStmPdo to return both event and data via CTRLCODE_LISTENING req
    propDataDesc,       // type: Objptr to a configdb for Send/Fetch's data description
    propObjDescPath,    // type: a string as the path of the object description file
    propRouterName,     // type: a string as the router name
    propAddrFormat,     // type: a string as the address format, including `ipv4', `ipv6', `dn'( domain name ), `url'
    propChildPdoClass,  // type: a string as the child pdo port class to create
    propSubmitTo,       // type: an handle to the port the irp to submit. only valid when propSubmitPdo is true.
    propCompress,       // type: a boolean value to indicate whether to compress the data packet over the tcp connection.
    propEnableSSL,      // type: a boolean value to indicate whether to enable SSL ssession
    propEnableWebSock,  // type: a boolean value to indicate whether to enable Websocket
    propDestUrl,        // type: a string value as the location of the server.
    propConnRecover,    // type: a boolean value to indicate whether to recover the lost connection for websocket or http2
    propConnParams,     // type: an objptr to a configdb object containing the tcp connection settings
    propConnHandle,     // type: an guint32 to a connnection handle to the CRpcTcpBridgeProxy on the client side
    propTransCtx,       // type: a pointer to the configdb as are the context
                        // information for transferring requests or events,
                        // which could include propConnHandle, propRouterPath,

    propObjDescFile,    // type: a string as the file path for the object description file
    propRouterPath,     // type: an string as the routing path to the destination server
    propPrxyPortId,     // type: a guint32 as the portid for a proxy to forward
                        // the request, 0xfffffff stands for a CRpcReqForwarderProxy, and other values
                        // for a CRpcTcpBridgeProxy
    propNodeName,       // type: a string as the node name for a bridge proxy

    // security properties
    propSecCtx,         // type: a pointer to a IConfigDb object holding security context
    propSessHash,       // type: a byte array as the session hash
    propSignature,      // type: a byte array as the message's signature/digest
    propOpenPort,       // type: a bool to indiate the CDBusProxyPdo if a openport/locallogin is to send
    propHasAuth,        // type: an bool flag to indicate authentication is enabled on the router
    propAuthInfo,       // type: an objptr to a configdb containting authentication information
    propAuthMech,       // type: a string as the authentication mechanism, it can be `krb5', `ntlmv2' or `password'
    propTimestamp,      // type: a guint64 as a timestamp usec
    propServiceName,    // type: a string as the service name
    propRealm,          // type: a string as a kerberos realm
    propNoEnc,          // type: a bool to tell encryption is not needed
    propContinue,       // type: a bool to tell if the login process is still going on or not
    propSearchPaths,    // type: an objptr to a set of paths where to find the libraries and config files

    propCustomEvent,    // type: a connection point for port specific events
    propSignMsg,        // type: a bool to indicate whether to sign the message or encrypt the message.

    propNoPort,         // type: a bool to indicate the interface object have the underlying port
    propSalt,           // type: a guint64 as a salt for sess hash

    propPyObj,          // type: a pointer to a PyObject which is an interface object
    propJavaObj,        // type: a pointer to a java object which is an interface object
    propPeerObjId,      // type: a gint64 for the uxstream object id from the peer

    propSeriProto,      // type: a gint32 to indicate the serialize protocol of the current parament pack
    propNoReply,        // type: a bool value to indicate if the request has no reply.

    propRttMs,          // type: a guint32 as the round trip time of a tcp connection in ms.

    propMaxConns,       // type: a guint32 to specify the max connections the router can accepts
    propMaxReqs,        // type: a guint32 to specify the max requests in process concurrently this router allows
    propMaxPendings,    // type: a guint32 to specify the max requests queued or in process by this router
    propEnableRfc,      // type: a bool to enable/disable request-based flow control
    propSepConns,       // type: a bool with `true' to have the reqfwdr to establish connections to the remote bridge.

    propReservedEnd = 0x10000000,
    propInvalid = -1, 
};

}
#define PropIdFromInt( intid ) ( static_cast< EnumPropId >( intid ) )
#define PropId( propid ) ( static_cast< gint32 >( propid ) )

