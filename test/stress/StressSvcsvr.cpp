/****BACKUP YOUR CODE BEFORE RUNNING RIDLC***/
// Implement the following methods
// to get the RPC proxy/server work
#include "rpc.h"
using namespace rpcf;
#include "StressTest.h"
#include "StressSvcsvr.h"

std::atomic< unsigned int > g_dwCounter(0);
// IEchoThings Server
/* Sync Req */
gint32 CStressSvc_SvrImpl::Echo(
    const std::string& strText /*[ In ]*/,
    std::string& strResp /*[ Out ]*/ )
{
    strResp = strText + " echo " +
        std::to_string( ( guint32 )g_dwCounter++ );
    return STATUS_SUCCESS;
}

/* Sync Req */
gint32 CStressSvc_SvrImpl::EchoUnknown(
    BufPtr& pBuf /*[ In ]*/,
    BufPtr& pResp /*[ Out ]*/ )
{
    pResp = pBuf;
    return STATUS_SUCCESS;
}

/* Sync Req */
gint32 CStressSvc_SvrImpl::Ping(
    const std::string& strCount /*[ In ]*/ )
{

    DebugPrint( 0, "client counter %s, svr counter %d",
        strCount.c_str(),
        ( guint32 )g_dwCounter++ );
    return STATUS_SUCCESS;
}
