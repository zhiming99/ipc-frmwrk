/****BACKUP YOUR CODE BEFORE RUNNING RIDLC***/
// Implement the following methods
// to get the RPC proxy/server work
#include "rpc.h"
using namespace rpcf;
#include "StressTest.h"
#include "StressSvccli.h"

// IEchoThings Proxy
/* Async Req */
std::atomic< guint32 > g_dwCounter( 0 );
extern std::atomic< bool > g_bQueueFull;
extern std::atomic< guint32 > g_dwReqs;

gint32 CStressSvc_CliImpl::EchoCallback(
    IConfigDb* context, gint32 iRet,
    const std::string& strResp /*[ In ]*/ )
{
    gint32 ret = 0;
    do{
        g_dwReqs--;
        if( context == nullptr )
        {
            ret = -EINVAL;
            break;
        }
        CCfgOpener oCfg( context );
        guint32 dwIdx;
        oCfg.GetIntProp( 0, dwIdx );
        if( iRet == ERROR_QUEUE_FULL )
        {
            g_bQueueFull = true;
            printf( "Queue is full, %d "
                "req cannot be sent\n", dwIdx );
            return iRet;
        }
        else if( iRet == -ETIMEDOUT )
        {
            DebugPrint( iRet, "req %d, timeout, "
                "pending %d", dwIdx,
                ( guint32 )g_dwReqs );
            return iRet;
        }

        DebugPrint( iRet, "req %d, completed, "
            "resp is %s, pending %d", dwIdx,
            strResp.c_str(),
            ( guint32 )g_dwReqs );

        g_bQueueFull = false;
        dwIdx = ++g_dwCounter;
        oCfg.SetIntProp( 0, dwIdx );
        stdstr strMsg = "Hello, Server ";
        strMsg += std::to_string( dwIdx );
        g_dwReqs++;

        TaskletPtr pTask;
        ret = this->Echo( context, strMsg );
        if( ret == STATUS_PENDING )
            ret = 0;

    }while( 0 );

    return ret;
}

/* Event */
gint32 CStressSvc_CliImpl::OnHelloWorld(
    const std::string& strMsg /*[ In ]*/ )
{
    printf( "event from svr: %s\n",
        strMsg.c_str() );
    return 0;
}

