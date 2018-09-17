/*
 * =====================================================================================
 *
 *       Filename:  ifclient.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/15/2018 01:15:23 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi( woodhead99@gmail.com ), 
 *   Organization:  
 *
 * =====================================================================================
 */


#include <rpc.h>
#include <string>
#include <unistd.h>
#include <cppunit/TestFixture.h>
#include "evtsvr.h"
#include "ifhelper.h"

using namespace std;

CEventClient::CEventClient(
    const IConfigDb* pCfg )
    : super( pCfg )
{
    SetClassId( clsid( CEventClient ) );
}

gint32 CEventClient::InitUserFuncs()
{
    super::InitUserFuncs();

    BEGIN_HANDLER_MAP;

    ADD_USER_EVENT_HANDLER(
        CEventClient::OnHelloWorld,
        EVENT_HelloWorld );

    END_HANDLER_MAP;

    return 0;
}


gint32 CEventClient::OnHelloWorld(
    IEventSink* pContext,
    const std::string& strText )
{
    gint32 ret = 0;

    do{
        printf( "Received Event: %s\n",
            strText.c_str() );
    }while( 0 );

    return ret;
}

