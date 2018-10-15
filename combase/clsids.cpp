/*
 * =====================================================================================
 *
 *       Filename:  clsids.cpp
 *
 *    Description:  a map of class id to class name
 *
 *        Version:  1.0
 *        Created:  08/07/2016 01:18:27 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi( woodhead99@gmail.com )
 *   Organization:
 *
 * =====================================================================================
 */
#include <map>
#include <vector>
#include "defines.h"
#include "buffer.h"
#include "configdb.h"
#include "registry.h"
#include "stlcont.h"
#include <dlfcn.h>
#include "objfctry.h"

using namespace std;

extern FctryVecPtr g_pFactories;

// c++11 required

static FactoryPtr InitClassFactory()
{

    BEGIN_FACTORY_MAPS;

    INIT_MAP_ENTRY( CBuffer );
    INIT_MAP_ENTRY( CRegistry );
    INIT_MAP_ENTRY( CStlIntVector );
    INIT_MAP_ENTRY( CStlIntQueue );
    INIT_MAP_ENTRY( CStlBufVector );
    INIT_MAP_ENTRY( CStlEventMap );
    INIT_MAP_ENTRY( CStlIntMap );
    INIT_MAP_ENTRY( CStlObjSet );
    INIT_MAP_ENTRY( CStlObjVector );
    INIT_MAP_ENTRY( CStlStringSet );

    INIT_MAP_ENTRYCFG( CConfigDb );

    END_FACTORY_MAPS;
};

const char* CoGetClassName( EnumClsid iClsid )
{
    map< EnumClsid, const char* >::iterator itr;
    if( iClsid == clsid( Invalid ) )
        return "Invalid";

    // the search is said to be optimized
    // Let's assume it as a binary search
    return g_pFactories->GetClassName( iClsid );
}

EnumClsid CoGetClassId( const char* szClassName )
{
    if( szClassName == nullptr )
        return Clsid_Invalid;

    return g_pFactories->GetClassId( szClassName );

}

typedef gint32 (*PLOADCLASSFACTORY)( FactoryPtr& pFactory ) ;

gint32 CoLoadClassFactory( const char* pszPath  )
{
    if( pszPath == nullptr )
        return -EINVAL;

    FactoryPtr pFactory;
    void* hDll = nullptr;
    gint32 ret = 0;

    do{
        hDll = dlopen( pszPath, RTLD_LAZY );
        if( hDll == nullptr )
        {
            ret = -EBADF;
            break;
        }

        dlerror();

        PLOADCLASSFACTORY pFunc =
            ( PLOADCLASSFACTORY )dlsym( hDll, "DllLoadFactory" );

        if( pFunc == nullptr )
        {
            ret = -ENOENT;
            break;
        }

        ret = ( *pFunc )( pFactory );

        if( ERROR( ret ) )
        {
            pFactory.Clear();
            break;
        }

        ret = g_pFactories->AddFactory(
            pFactory, hDll );

    }while( 0 );

    if( ERROR( ret ) && hDll != nullptr )
        dlclose( hDll );

    return ret;
}

gint32 CoAddClassFactory(
    const FactoryPtr& pFactory )
{

    if( pFactory.IsEmpty() )
        return -EINVAL;

    return g_pFactories->AddFactory(
        pFactory, nullptr );
}

/**
* @name Load all the class factory from the
* directory pszDir, and put them to vector
* g_pFactories.
* @{ */
/**  @} */

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

gint32 CoLoadClassFactories( const char* dir )
{
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;

    char* pszAbPath = realpath( dir, NULL );

    if( pszAbPath == nullptr )
        return -errno;

    string strPrefix( pszAbPath );
    strPrefix += "/";

    free( pszAbPath );
    pszAbPath = nullptr;

    if((dp = opendir(dir)) == NULL)
    {
        fprintf(stderr,"cannot open directory: %s\n", dir);
        return -errno;
    }

    string strOldDir = getcwd( NULL, 0 );

    chdir(dir);

    while((entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name,&statbuf);
        if(S_ISDIR(statbuf.st_mode))
        {
            // we don't handle subdirectory yet
            /*  Found a directory, but ignore . and .. */
            if( strcmp(".",entry->d_name) == 0 ||
                strcmp("..",entry->d_name) == 0 )
            continue;

            // printf("%*s%s/\n",depth,"",entry->d_name);
            /*  Recurse at a new indent level */
            // WalkDir(entry->d_name,depth+4);
        }
        else
        {
            if( strcmp("libcombase.so",entry->d_name) == 0 )
                continue;

            string strPath = strPrefix + entry->d_name;
            CoLoadClassFactory( strPath.c_str() );
        }
    }

    chdir( strOldDir.c_str() );
    closedir(dp);

    return 0;
}

#ifdef DEBUG
std::unordered_map<EnumClsid, std::string> g_mapId2Name;

static gint32 CoInitClsidNames()
{
    vector<EnumClsid> vecIds;
    g_pFactories->EnumClassIds( vecIds );
    for( auto iClsid : vecIds )
    {
        const char* pszName = CoGetClassName( iClsid );
        if( pszName != nullptr )
        {
            g_mapId2Name[ iClsid ] = string( pszName );
        }
    }
    return 0;
}

#endif
// call this method before anything else is
// called
gint32 CoInitialize( guint32 dwContext )
{
    if( g_pFactories.IsEmpty() )
    {
        g_pFactories = new CClassFactories(); 
        if( g_pFactories.IsEmpty() )
            return -EFAULT;

        g_pFactories->DecRef();
    }
    // load the class factory of combase ahead of 
    // any of the rest libraires
    FactoryPtr pBaseFactory = InitClassFactory();
    if( pBaseFactory.IsEmpty() )
        return -EFAULT;

    gint32 ret = g_pFactories->AddFactory(
        pBaseFactory, nullptr );

    if( ERROR( ret ) )
        return ret;

    ret = CoLoadClassFactories( "." );
#ifdef DEBUG
    if( SUCCEEDED( ret ) )
        CoInitClsidNames();        
#endif
    return ret;
}

gint32 CoUninitialize()
{
    g_pFactories->Clear();    
    g_pFactories.Clear();
    return 0;
}

extern "C"
gint32 DllLoadFactory( FactoryPtr& pFactory )
{
    pFactory = InitClassFactory();
    if( pFactory.IsEmpty() )
        return -EFAULT;

    return 0;
}

