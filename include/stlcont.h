/*
 * =====================================================================================
 *
 *       Filename:  stlcont.h
 *
 *    Description:  declaration of wrappers for stl container objects
 *
 *        Version:  1.0
 *        Created:  07/27/2016 08:59:08 PM
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

#include <map>
#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <algorithm>
#include <set>
#include "clsids.h"
#include "propids.h"
#include "autoptr.h"
#include "defines.h"
#include "dlfcn.h"
#include "buffer.h"

using stdstr = std::string;
using cchar = const char;

#define MAX_ELEM_CONTAINER  10000

namespace rpcf
{

template< typename T >
class CStlQueue : public CObjBase
{
    protected:

    std::deque< T > m_vecElems;
    // std::recursive_mutex   m_oLock;

    public:

    typedef CObjBase super;
    typedef typename std::deque< T >::iterator MyItr;
    typedef typename std::deque< T > MyType;

    CStlQueue()
    {;}

    gint32 Serialize(
        CBuffer& oBuf ) const
    { return -ENOTSUP; }

    gint32 Deserialize(
        const CBuffer& obuf )
    { return -ENOTSUP; }
    
    const std::deque< T >& operator()() const
    { return m_vecElems; }

    std::deque< T >& operator()()
    { return m_vecElems; }

    // std::recursive_mutex& GetLock() const
    // {  return ( std::recursive_mutex& )m_oLock; }
};

class CStlIntQueue : public CStlQueue< guint32 >
{

    public:
    typedef CStlQueue< guint32 > super;
    CStlIntQueue()
        :CStlQueue<guint32>()
    {
        SetClassId( clsid( CStlIntQueue ) );
    }
};

typedef CAutoPtr< clsid( CStlIntQueue ), CStlIntQueue > IntQuePtr;

template< typename T >
class CStlVector : public CObjBase
{
    protected:

    std::vector< T > m_vecElems;
    std::recursive_mutex   m_oLock;

    public:

    typedef CObjBase super;
    typedef typename std::vector< T >::iterator MyItr;
    typedef typename std::vector< T > MyType;
    typedef T  ElemType;

    struct SERI_HEADER : public SERI_HEADER_BASE
    {
        typedef SERI_HEADER_BASE super;
        guint32 dwCount;
        ElemType arrElems[ 0 ];

        SERI_HEADER()
        {
            dwCount = 0; 
        }

        SERI_HEADER( const SERI_HEADER& oSrc )
            : super( oSrc )
        {
            dwCount = oSrc.dwCount;
        }

        SERI_HEADER& operator=(
            const SERI_HEADER& oSrc )
        {
            super::operator=( oSrc );
            dwCount = oSrc.dwCount;
            return *this;
        }

        void ntoh()
        {
            super::ntoh();
            dwCount = ntohl( dwCount );
        }

        void hton()
        {
            super::hton();
            dwCount = htonl( dwCount );
        }
    };

    CStlVector()
    {;}

    gint32 Serialize(
        CBuffer& oBuf ) const
    { return -ENOTSUP; }

    gint32 Deserialize(
        const CBuffer& obuf )
    { return -ENOTSUP; }
    
    const std::vector< T >& operator()() const
    { return m_vecElems; }

    std::vector< T >& operator()()
    { return m_vecElems; }

    std::recursive_mutex& GetLock() const
    {  return ( std::recursive_mutex& )m_oLock; }
};

class CStlIntVector : public CStlVector< guint32 >
{
    public:

    typedef CStlVector< guint32 > super;
    CStlIntVector() :super()
    {
        SetClassId( clsid( CStlIntVector ) );
    }

    gint32 Serialize(
        CBuffer& oBuf ) const;

    gint32 Deserialize(
        const CBuffer& obuf );
};

typedef CAutoPtr< clsid( CStlIntVector ), CStlIntVector > IntVecPtr;
class CStlQwordVector : public CStlVector< guint64 >
{
    public:

    typedef CStlVector< guint64 > super;
    CStlQwordVector() :super()
    {
        SetClassId( clsid( CStlQwordVector ) );
    }

    gint32 Serialize(
        CBuffer& oBuf ) const;

    gint32 Deserialize(
        const CBuffer& obuf );
};

typedef CAutoPtr< clsid( CStlQwordVector ), CStlQwordVector > QwVecPtr;

/** @name CStlLongWordVector, the class used to
 * pass the pointers within the threads. The size
 * of LONGWORD could change between different
 * arch. So don't use it across the boxes.  @{ */
/**  @} */

class CStlLongWordVector : public CStlVector< LONGWORD >
{
    public:

    typedef CStlVector< LONGWORD > super;
    CStlLongWordVector() :super()
    {
        SetClassId( clsid( CStlLongWordVector ) );
    }

    gint32 Serialize(
        CBuffer& oBuf ) const;

    gint32 Deserialize(
        const CBuffer& obuf );
};

typedef CAutoPtr< clsid( CStlLongWordVector ), CStlLongWordVector  > LwVecPtr;

class CStlBufVector : public CStlVector< BufPtr >
{

    public:
    typedef CStlVector<BufPtr> super;
    CStlBufVector()
        :CStlVector<BufPtr>()
    {
        SetClassId( clsid( CStlBufVector  ) );
    }
};

typedef CAutoPtr< clsid( CStlBufVector ), CStlBufVector > BufVecPtr;

class CStlObjVector : public CStlVector< ObjPtr >
{

    public:
    typedef CStlVector<ObjPtr> super;
    CStlObjVector()
        :CStlVector<ObjPtr>()
    {
        SetClassId( clsid( CStlObjVector  ) );
    }

    struct SERI_HEADER : public SERI_HEADER_BASE
    {
        typedef SERI_HEADER_BASE super;
        guint32 dwCount;
        // dwCount is sizeof( arrOffsets ) - 1, the
        // last offset is the size of the payload
        // section.
        //
        // arroffsets is followed by the payload
        // section for each serialized objects
        guint32 arrOffsets[ 0 ];

        SERI_HEADER() : super()
        {
            dwClsid = clsid( CStlObjVector );
            dwCount = 0; 
        }

        SERI_HEADER( const SERI_HEADER& oSrc )
            : super( oSrc )
        {
            *this = oSrc;
        }

        SERI_HEADER& operator=(
            const SERI_HEADER& oSrc )
        {
            super::operator=( oSrc );

            dwCount = oSrc.dwCount;
            if( oSrc.dwCount > MAX_ELEM_CONTAINER )
                dwCount = 1;

            memcpy( arrOffsets,
                oSrc.arrOffsets,
                dwCount * sizeof( guint32 ) );

            return *this;
        }

        void ntoh()
        {
            super::ntoh();
            dwCount = ntohl( dwCount );

            if( dwCount > MAX_ELEM_CONTAINER )
                return;

            guint32* pOffset = arrOffsets;
            guint32 dwVal;
            for( guint32 i = 0; i < dwCount; i++ )
            {
                dwVal = ntohl( pOffset[ i ] );
                pOffset[ i ] = dwVal;
            }
        }

        void hton()
        {
            if( dwCount > MAX_ELEM_CONTAINER )
                return;

            guint32* pOffset = arrOffsets;
            guint32 dwVal;
            for( guint32 i = 0; i < dwCount; i++ )
            {
                dwVal = htonl( pOffset[ i ] );
                pOffset[ i ] = dwVal;
            }

            dwCount = htonl( dwCount );
            super::hton();
        }
    };

    gint32 Serialize(
        CBuffer& oBuf ) const;

    gint32 Deserialize(
        const CBuffer& obuf );
    
    gint32 Deserialize(
        const char* pBuf, guint32 dwBufSize );
};

typedef CAutoPtr< clsid( CStlObjVector ), CStlObjVector > ObjVecPtr;

template< typename Key_, typename Val_ >
class CStlMap : public CObjBase
{
    protected:

    std::map< Key_, Val_ > m_mapElems;
    std::recursive_mutex   m_oLock;

    public:

    typedef CObjBase super;

    typedef typename std::map< Key_, Val_ >::iterator MyItr;
    typedef typename std::pair< Key_, Val_ > MyPair;
    typedef typename std::map< Key_, Val_ > MyMap;
    typedef MyPair ElemType;
    typedef Key_ KeyType;
    typedef Val_ ValueType;

    CStlMap()
    {;}

    // a functor to return the container
    const std::map< Key_, Val_ >& operator()() const
    {
        return ( std::map< Key_, Val_ >& )m_mapElems;
    }

    std::map< Key_, Val_ >& operator()()
    {
        return ( std::map< Key_, Val_ >& )m_mapElems;
    }

    struct SERI_HEADER : public SERI_HEADER_BASE
    {
        typedef SERI_HEADER_BASE super;
        guint32 dwCount;
        ElemType arrElems[ 0 ];

        SERI_HEADER()
        {
            dwCount = 0; 
        }

        SERI_HEADER( const SERI_HEADER& oSrc )
            : super( oSrc )
        {
            dwCount = oSrc.dwCount;
        }

        SERI_HEADER& operator=(
            const SERI_HEADER& oSrc )
        {
            super::operator=( oSrc );
            dwCount = oSrc.dwCount;
            return *this;
        }

        void ntoh()
        {
            super::ntoh();
            dwCount = ntohl( dwCount );
        }

        void hton()
        {
            super::hton();
            dwCount = htonl( dwCount );
        }
    };

    gint32 Serialize(
        CBuffer& oBuf ) const
    { return -ENOTSUP; }

    gint32 Deserialize(
        const CBuffer& obuf )
    { return -ENOTSUP; }

    std::recursive_mutex& GetLock()
    {  return m_oLock; }
};

class CStlIntMap
    : public CStlMap< guint32, guint32 > 
{
 
    public:
    typedef CStlMap< guint32, guint32 > super;

    CStlIntMap() : super()
    {
        SetClassId( clsid( CStlIntMap ) );
    }
    gint32 Serialize(
        CBuffer& oBuf ) const;

    gint32 Deserialize(
        const CBuffer& obuf );

};

typedef CAutoPtr< clsid( CStlIntMap ), CStlIntMap > IntMapPtr;

class CStlObjMap
    : public CStlMap< ObjPtr, QwVecPtr > 
{
 
    public:
    typedef CStlMap< ObjPtr, QwVecPtr > super;

    CStlObjMap() : super()
    {
        SetClassId( clsid( CStlObjMap ) );
    }
};

typedef CAutoPtr< clsid( CStlObjMap ), CStlObjMap > ObjMapPtr;

class CStlEventMap:
    public CStlMap< EventPtr, gint32 > 
{
    public:

    typedef CStlMap< EventPtr, gint32 > super;

    CStlEventMap() : super()
    {
        SetClassId( clsid( CStlEventMap ) );
    }

    gint32 SubscribeEvent(
        EventPtr& pEventSink ) 
    {

        CStdRMutex oLock( GetLock() );
        MyMap& oMap = m_mapElems;
        if( oMap.find( pEventSink ) == oMap.end() )
        {
            oMap[ pEventSink ] = 1;
        }
        else
        {
            ++oMap[ pEventSink ];
        }
        return 0;
    }

    gint32 UnsubscribeEvent(
        EventPtr& pEventSink )
    {

        gint32 ret = -ENOENT;

        CStdRMutex oLock( GetLock() );

        MyMap& oMap = m_mapElems;
        if( oMap.find( pEventSink ) != oMap.end() )
        {
            ret = --oMap[ pEventSink ];

            if( ret <= 0 )
                oMap.erase( pEventSink );

            ret = 0;
        }

        return ret;
    }

    gint32 BroadcastEvent(
        EnumEventId iEvent,
        LONGWORD dwParam1,
        LONGWORD dwParam2,
        LONGWORD* pData )
    {
        MyMap& oMap = m_mapElems;
        std::vector<EventPtr> vecEvents;

        if( iEvent != eventInvalid )
        {
            CStdRMutex oLock( GetLock() );
            MyItr itr = oMap.begin();
            while( itr != oMap.end() )
            {
                vecEvents.push_back( itr->first );
                ++itr;
            }
        }

        for( guint32 i = 0; i < vecEvents.size(); i++ )
        {
            EventPtr& pEvt = vecEvents[ i ];
            pEvt->OnEvent(
                iEvent, dwParam1, dwParam2, pData );
        }

        return 0;
    }
};

typedef CAutoPtr< clsid( CStlEventMap ), CStlEventMap > EvtMapPtr;

template< typename Val_ >
class CStlSet : public CObjBase
{
    protected:

    std::set< Val_ > m_setElems;
    stdrmutex m_oLock;

    public:
    typedef CObjBase super;
    typedef typename std::set< Val_ >::iterator MyItr;
    typedef typename std::set< Val_ > MySet;


    CStlSet()
    {;}

    ~CStlSet()
    { m_setElems.clear(); }

    // a functor to return the container
    const std::set< Val_ >& operator()() const
    { return ( std::set< Val_ >& )m_setElems; }

    std::set< Val_ >& operator()()
    { return ( std::set< Val_ >& )m_setElems; }

    gint32 Serialize(
        CBuffer& oBuf ) const
    { return -ENOTSUP; }

    gint32 Deserialize(
        const CBuffer& obuf )
    { return -ENOTSUP; }

    std::recursive_mutex& GetLock()
    {  return m_oLock; }
};

class CStlObjSet : public CStlSet< ObjPtr >
{
public:
    typedef ObjPtr ElemType;

    CStlObjSet() :CStlSet< ObjPtr >()
    { SetClassId( clsid( CStlObjSet ) ); }

    ~CStlObjSet()
    {;}
};

typedef CAutoPtr< clsid( CStlObjSet ), CStlObjSet > ObjSetPtr;

class CStlStringSet : public CStlSet< std::string >
{
public:
    typedef std::string ElemType;

    CStlStringSet() :CStlSet< std::string >()
    { SetClassId( clsid( CStlStringSet ) ); }

    ~CStlStringSet()
    {;}
};

typedef CAutoPtr< clsid( CStlStringSet ), CStlStringSet > StrSetPtr;

}
