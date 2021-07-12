/*
 * =====================================================================================
 *
 *       Filename:  routmain.cpp
 *
 *    Description:  the main entry of the stand-alone rpc router.
 *
 *        Version:  1.0
 *        Created:  12/21/2018 03:42:25 PM
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

#include <string>
#include <iostream>
#include <unistd.h>

#include "routmain.h"
#include <ifhelper.h>
#include <frmwrk.h>
#include <rpcroute.h>

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_REGISTRATION( CIfRouterTest );

// router role: 1. reqfwdr, 2. bridge, 3. both
static guint32 g_dwRole = 1;
static bool g_bAuth = false;
static bool g_bRfc = false;
static bool g_bSepConn = false;
static std::string g_strService;

void CIfRouterTest::setUp()
{
    gint32 ret = 0;
    do{
        DebugPrint( 0, "Starting..." );
        ret = CoInitialize( 0 );
        CPPUNIT_ASSERT( SUCCEEDED( ret ) );

        CParamList oParams;

        ret = oParams.Push(
            std::string( MODULE_NAME ) );

        CPPUNIT_ASSERT( SUCCEEDED( ret ) );

        ret = m_pMgr.NewObj(
            clsid( CIoManager ),
            oParams.GetCfg() );

        CPPUNIT_ASSERT( SUCCEEDED( ret ) );

        CIoManager* pSvc = m_pMgr;

        if( pSvc != nullptr )
        {
            pSvc->SetCmdLineOpt(
                propRouterRole, g_dwRole );

            if( g_bAuth )
            {
                pSvc->SetCmdLineOpt(
                    propHasAuth, g_bAuth );
            }

            if( g_bRfc )
                pSvc->SetCmdLineOpt(
                    propEnableRfc, g_bRfc );

            if( g_strService.size() > 0 )
            {
                pSvc->SetCmdLineOpt(
                    propServiceName,
                    g_strService );
            }

            if( g_dwRole & 0x01 )
            {
                pSvc->SetCmdLineOpt(
                    propSepConns, g_bSepConn );
            }

            pSvc->SetRouterName( MODULE_NAME );
            ret = pSvc->Start();
        }
        else
            ret = -EFAULT;

        CPPUNIT_ASSERT( SUCCEEDED( ret ) );

    }while( 0 );
}

void CIfRouterTest::tearDown()
{
    gint32 ret = 0;
    do{
        IService* pSvc = m_pMgr;
        CPPUNIT_ASSERT( pSvc != nullptr );

        ret = pSvc->Stop();
        CPPUNIT_ASSERT( SUCCEEDED( ret ) );

        m_pMgr.Clear();

        ret = CoUninitialize();
        CPPUNIT_ASSERT( SUCCEEDED( ret ) );

        CPPUNIT_ASSERT( CObjBase::GetActCount() == 0 );

    }while( 0 );
}

CfgPtr CIfRouterTest::InitRouterCfg(
    guint32 dwRole )
{
    CParamList oCfg;
    gint32 ret = 0;
    std::string strDescPath;

    do{
        strDescPath = ROUTER_OBJ_DESC;
        if( g_bAuth )
            strDescPath = ROUTER_OBJ_DESC_AUTH;

        if( g_dwRole & 0x2 )
            oCfg[ propMaxTaskThrd ] = 4;
        oCfg[ propSvrInstName ] = MODULE_NAME;
        oCfg[ propIoMgr ] = m_pMgr;
        ret = CRpcServices::LoadObjDesc(
            strDescPath, OBJNAME_ROUTER, true,
            oCfg.GetCfg() );

        if( ERROR( ret ) )
            break;

        oCfg[ propIoMgr ] = m_pMgr;
        oCfg[ propIfStateClass ] =
            clsid( CIfRouterMgrState );

        oCfg[ propRouterRole ] = dwRole;

    }while( 0 );

    if( ERROR( ret ) )
    {
        std::string strMsg = DebugMsg(
            ret, "Error loading desc file %s",
            strDescPath.c_str() );

        throw std::runtime_error( strMsg );
    }

    return oCfg.GetCfg();
}

void CIfRouterTest::testSvrStartStop()
{
    CPPUNIT_ASSERT( !m_pMgr.IsEmpty() );
    CfgPtr pCfg = InitRouterCfg( g_dwRole );

    InterfPtr pIf;
    gint32 ret = 0;
    do{
        if( pCfg.IsEmpty() )
        {
            ret = -EFAULT;
            break;
        }

        ret =  pIf.NewObj(
            clsid( CRpcRouterManagerImpl ),
            pCfg );

        if( ERROR( ret ) )
            break;
            
        ret = pIf->Start();
        if( ERROR( ret ) )
            break;

        CInterfaceServer* pSvr = pIf;
        while( pSvr->IsConnected() )
            sleep( 1 );

    }while( 0 );

    if( !pIf.IsEmpty() )
        ret = pIf->Stop();

    CPPUNIT_ASSERT( SUCCEEDED( ret ) );

    return;
}

int main( int argc, char** argv )
{
    CppUnit::TextUi::TestRunner runner;
    CppUnit::TestFactoryRegistry& registry =
        CppUnit::TestFactoryRegistry::getRegistry();

    int opt = 0;
    int ret = 0;
    while( ( opt = getopt( argc, argv, "r:acfs:" ) ) != -1 )
    {
        switch (opt)
        {
        case 'r':
            {
                g_dwRole = ( guint32 )atoi( optarg );
                if( g_dwRole == 0 || g_dwRole > 3 )
                    ret = -EINVAL;
                break;
            }
        case 'a':
            {
                g_bAuth = true;
                break;
            }
        case 'c':
            {
                g_bSepConn = true;
                break;
            }
        case 's':
            {
                g_strService = optarg;
                break;
            }
        case 'f':
            {
                g_bRfc = true;
                break;
            }
        default: /*  '?' */
            ret = -EINVAL;
            break;
        }
        if( !g_bAuth )
        {
            if( g_strService.size() > 0 )
                ret = -EINVAL;
        }
        else
        {
            if( g_dwRole == 1 && 
                g_strService.size() > 0 )
                g_strService.clear();
        }

        if( ( g_dwRole & 1 ) == 0 )
            g_bSepConn = false;

        if( ERROR( ret ) )
            break;
    }

    if( ERROR( ret ) )
    {
        fprintf( stderr,
            "Usage: %s [-r <role number, 1: reqfwrd, 2: bridge, 3: both>, mandatory ]\n"
            "\t [-a to enable authentication ]\n"
            "\t [-c to establish a seperate connection to the same bridge per client, only for role 1]\n"
            "\t [-f to enable request-based flow control on the gateway bridge, ignore it if no massive connections ]\n"
            "\t [-s < Service Name for authentication, valid for role 2 or 3, and ignored for role 1 >]\n",
            argv[ 0 ] );
        exit( -ret );
    }

    runner.addTest( registry.makeTest() );
    bool wasSuccessful = runner.run( "", false );

    return !wasSuccessful;
}
