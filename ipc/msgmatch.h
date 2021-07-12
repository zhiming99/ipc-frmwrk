/*
 * =====================================================================================
 *
 *       Filename:  msgmatch.h
 *
 *    Description:  declaration of message match classes
 *
 *        Version:  1.0
 *        Created:  10/16/2016 10:06:25 AM
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

#include <sys/time.h>
#include "defines.h"
#include "utils.h"
#include "configdb.h"
#include "namespc.h"

namespace rpcf
{

typedef enum {

     matchServer,
     matchClient,
     matchInvalid

}EnumMatchType;

class IMessageMatch : public CObjBase
{
    public:

    // if a event message this interface has subcribed
    virtual gint32 IsMyMsgIncoming(
        DBusMessage* pMsg ) const = 0;

    virtual gint32 IsMyMsgOutgoing(
        DBusMessage* pMsg ) const = 0;

    // a equalness comparison
    virtual bool operator==(
        const IMessageMatch& rhs ) const = 0;

    virtual const IMessageMatch& operator=(
        const IMessageMatch& rhs ) = 0;

    virtual std::string ToDBusRules(
        gint32 iMsgType ) const = 0;

    virtual std::string ToString() const = 0;

    virtual EnumMatchType GetType() const = 0;

    virtual bool IsMyDest(
        const std::string& strDest ) const = 0;

    virtual bool IsMyObjPath(
        const std::string& strObjPath ) const = 0;

    virtual std::string GetDest() const = 0;

    virtual const CfgPtr& GetCfg() const = 0;
};

typedef CAutoPtr< Clsid_Invalid, IMessageMatch > MatchPtr;

}

namespace std
{
    using namespace rpcf;
    template<>
    struct less<MatchPtr>
    {
        bool operator()(const MatchPtr& k1, const MatchPtr& k2) const
            {
                std::string strVal1 = k1->ToString();
                std::string strVal2 = k2->ToString();

                if( strVal1.empty() || strVal2.empty() )
                    return false;

                if( strVal1 < strVal2 )
                    return true;

                return false;
            }
    };
}

namespace rpcf
{

class CMessageMatch : public IMessageMatch
{
    protected:

    std::string     m_strObjPath;
    std::string     m_strIfName;
    EnumMatchType   m_iMatchType;
    CfgPtr          m_pCfg;

    public:

    typedef IMessageMatch super;

    struct SERI_HEADER : public SERI_HEADER_BASE
    {
        typedef SERI_HEADER_BASE super;

        gint32 iMatchType;
        SERI_HEADER() : SERI_HEADER_BASE()
        {
            iMatchType = 0;
        }

        void ntoh()
        {
            super::ntoh();
            iMatchType = ntohl( iMatchType );
        }

        void hton()
        {
            super::hton();
            iMatchType = htonl( iMatchType );
        }
    };

    CMessageMatch() : m_pCfg( true )
    {
        SetClassId( clsid( CMessageMatch ) );
    }

    CMessageMatch( const IConfigDb* pCfg ) :
        CMessageMatch()
    {
        gint32 ret = 0;

        do{
            if( pCfg == nullptr )
            {
                // maybe user want to setup the
                // properties later
                break;
            }

            CCfgOpener a( pCfg );

            ret = a.GetStrProp(
                propIfName, m_strIfName );

            if( ERROR( ret ) )
                break;

            guint32* pMatchType =
                ( guint32* )&m_iMatchType;

            ret = a.GetIntProp( propMatchType,
                *pMatchType );

            if( ERROR( ret ) )
                break;

            ret = a.GetStrProp(
                propObjPath, m_strObjPath );

        }while( 0 );

        if( ERROR( ret ) )
        {
            std::string strMsg =
                DebugMsg( ret, "error in CMessageMatch's ctor" );

            throw std::runtime_error( strMsg );
        }
    }

    protected:
    // if a request message this interface can handle
    gint32 IsMyMsgBasic( DBusMessage* pMessage ) const
    {
        gint32 ret = 0;

        do{
            if( pMessage == nullptr )
            {
                ret = -EINVAL;
                break;
            }

            DMsgPtr pMsg( pMessage );

            if( m_strIfName.empty()
                || m_strObjPath.empty() )
            {
                ret = -EINVAL;
                break;
            }

            // test if the interface is the same
            if( m_strIfName != pMsg.GetInterface() )
            {
                ret = ERROR_FALSE;
                break;
            }

            // test if the object is the same
            if( m_strObjPath != pMsg.GetPath() )
            {
                ret = ERROR_FALSE;
            }

        }while( 0 );

        return ret;
    }

    public:

    gint32 IsMyMsgIncoming( DBusMessage* pMessage ) const
    {
        gint32 ret = 0;

        if( pMessage == nullptr )
            return -EINVAL;

        do{
            DMsgPtr pMsg( pMessage );
            bool bClient = ( GetType() == matchClient );

            gint32 iType = pMsg.GetType();

            if( bClient )
            {
                switch( iType )
                {
                case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                case DBUS_MESSAGE_TYPE_ERROR:
                case DBUS_MESSAGE_TYPE_SIGNAL:
                    break;

                case DBUS_MESSAGE_TYPE_METHOD_CALL:
                default:
                    {
                        ret = -ENOTSUP;
                        break;
                    }
                }
            }
            else
            {
                // a message from the server
                switch( iType )
                {
                case DBUS_MESSAGE_TYPE_METHOD_CALL:
                    break;

                case DBUS_MESSAGE_TYPE_SIGNAL:
                case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                case DBUS_MESSAGE_TYPE_ERROR:
                default:
                    {
                        ret = -ENOTSUP;
                        break;
                    }
                }
            }

        }while( 0 );

        if( ERROR( ret ) )
            return ret;

        return IsMyMsgBasic( pMessage );
    }

    // if a event message this interface has subcribed
    gint32 IsMyMsgOutgoing( DBusMessage* pMessage ) const
    {
        gint32 ret = 0;

        if( pMessage == nullptr )
            return -EINVAL;

        do{
            DMsgPtr pMsg( pMessage );
            bool bClient = ( GetType() == matchClient );

            gint32 iType = pMsg.GetType();

            if( bClient )
            {
                switch( iType )
                {
                case DBUS_MESSAGE_TYPE_METHOD_CALL:
                    break;

                case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                case DBUS_MESSAGE_TYPE_ERROR:
                case DBUS_MESSAGE_TYPE_SIGNAL:
                default:
                    {
                        ret = -ENOTSUP;
                        break;
                    }
                }
            }
            else
            {
                switch( iType )
                {
                case DBUS_MESSAGE_TYPE_SIGNAL:
                case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                case DBUS_MESSAGE_TYPE_ERROR:
                        break;

                case DBUS_MESSAGE_TYPE_METHOD_CALL:
                default:
                    ret = -ENOTSUP;
                    break;
                }
            }

        }while( 0 );

        if( ERROR( ret ) )
            return ret;

        return IsMyMsgBasic( pMessage );
    }

    virtual std::string GetDest() const
    {
        CCfgOpenerObj oCfg( this );
        std::string strMyDest;

        gint32 ret = oCfg.GetStrProp(
            propDestDBusName, strMyDest );

        if( ERROR( ret ) )
            return std::string( "" );

        return strMyDest;
    }
    
    virtual bool IsMyObjPath(
        const std::string& strObjPath ) const
    {

        // the dest is a prefix of obj path
        std::string strMyObjPath = m_strObjPath;

        std::replace( strMyObjPath.begin(),
            strMyObjPath.end(), '/', '.' );
            
        if( strObjPath == strMyObjPath )
            return true;

        return false;
    }

    bool IsMyDest(
        const std::string& strDest ) const
    {
        gint32 ret = 0;
        std::string strMyDest = GetDest();
        if( !strMyDest.empty() )
        {
            ret = strncmp(
                strMyDest.c_str(),
                strDest.c_str(),
                strDest.size() );
            if( ret != 0 )
                return false;
        }
        else
        {
            // the dest is a prefix of obj path
            strMyDest = m_strObjPath;
            std::replace( strMyDest.begin(),
                strMyDest.end(), '/', '.' );
            gint32 iIdx = 0;

            if( strMyDest[ 0 ] == '.' )
                iIdx = 1;

            if( strDest != strMyDest.substr( iIdx, strDest.size() ) )
                return false;

            // FIXME: it is not a right solution.
            // we should rely on the
            // propDestDBusName for accurate match
            ret = 0;
        }

        if( ERROR( ret ) )
            return false;

        return true;
    }

    // a equalness comparison
    bool operator==( const IMessageMatch& rhs ) const
    {
        if( m_strObjPath.empty()
            || m_strIfName.empty() )
            return false;

        if( GetType() != rhs.GetType() )
            return false;

        if( ToString() != rhs.ToString() )
            return false;

        return true;
    }

    const IMessageMatch& operator=(
        const IMessageMatch& rhs )
    {
        const CMessageMatch& oMatch =
            static_cast< const CMessageMatch& >( rhs );

        return ( *this = oMatch );
    }

    EnumMatchType GetType() const
    {
        return m_iMatchType;
    }

    void SetType( EnumMatchType iType )
    {
        m_iMatchType = iType;
    }

    gint32 GetIfName(
        std::string& strIfName ) const
    {
        strIfName = m_strIfName;
        return 0;
    }

    gint32 GetObjPath(
        std::string& strObjPath ) const
    {
        strObjPath = m_strObjPath;
        return 0;
    }

    std::string ToDBusRules(
        gint32 iMsgType ) const
    {
        gint32 ret = 0;

        std::string strMatch;
        std::string strRule = "type=";

        switch( iMsgType )
        {
        case DBUS_MESSAGE_TYPE_SIGNAL:
            {
                strRule += "'signal'";
                break;
            }
        case DBUS_MESSAGE_TYPE_METHOD_CALL:
            {
                strRule += "'method_call'";
                break;
            }
        case DBUS_MESSAGE_TYPE_METHOD_RETURN:
            {
                strRule += "'method_return'";
                break;
            }
        case DBUS_MESSAGE_TYPE_ERROR:
            {
                strRule += "'error'";
                break;
            }
        case DBUS_MESSAGE_TYPE_INVALID:
        default:
            {
                strRule = "";
                break;
            }
        }

        if( !strRule.empty() )
            strMatch = strRule;

        GetIfName( strRule );
        if( !strRule.empty() )
        {
            strMatch += std::string( ",interface='" )
                + strRule + std::string( "'" );
        }

        GetObjPath( strRule );
        if( !strRule.empty() )
        {
            strMatch += std::string( ",path='" )
                + strRule + std::string( "'" );
        }

        CCfgOpener oCfg( ( IConfigDb* )GetCfg() );
        if( oCfg.exist( propMethodName ) )
        {
            ret = oCfg.GetStrProp(
                propMethodName, strRule );

            if( SUCCEEDED( ret ) && !strRule.empty() )
            {
                strMatch += std::string( ",member='" )
                    + strRule + std::string( "'" );
            }
        }

        return strMatch;
    }

    std::string ToString() const
    {
        std::string strAll =
            ToDBusRules( DBUS_MESSAGE_TYPE_INVALID );

        strAll += ",match=";
        if( GetType() == matchClient )
        {
            strAll += "matchClient";
        }
        else if( GetType() == matchServer )
        {
            strAll += "matchServer";
        }

        return strAll;
    }

    virtual gint32 EnumProperties(
        std::vector< gint32 >& vecProps ) const 
    {
        gint32 ret =
            super::EnumProperties( vecProps );

        if( ERROR( ret ) )
            return ret;

        vecProps.push_back( propObjPath );
        vecProps.push_back( propIfName );
        vecProps.push_back( propMatchType );

        ret = m_pCfg->EnumProperties( vecProps );

        if( ERROR( ret ) )
            return ret;

        return 0;
    }

    gint32 GetPropertyType(
        gint32 iProp, gint32& iType ) const
    {

        gint32 ret = 0;
        switch( iProp )
        {
        case propObjPath:
        case propIfName:
            {
                iType = typeString;
                break;
            }
        case propMatchType:
            {
                iType = typeUInt32;
                break;
            }
        default:
            {
                ret = m_pCfg->GetPropertyType(
                    iProp, iType );
                break;
            }
        }
        return ret;
    }

    bool exist( gint32 iProp ) const
    {
        gint32 ret = 0;
        switch( iProp )
        {
        case propObjPath:
        case propIfName:
        case propMatchType:
            {
                ret = true;
                break;
            }
        default:
            {
                ret = m_pCfg->exist( iProp );
                break;
            }
        }
        return ret;
    }

    gint32 GetProperty(
        gint32 iProp, CBuffer& oBuf ) const
    {
        gint32 ret = 0;
        switch( iProp )
        {
        case propObjPath:
            {
                oBuf = m_strObjPath;
                break;
            }
        case propIfName:
            {
                oBuf = m_strIfName;
                break;
            }
        case propMatchType:
            {
                oBuf = GetType();
                break;
            }
        default:
            {
                ret = m_pCfg->GetProperty(
                    iProp, oBuf );
                break;
            }
        }
        return ret;
    }

    gint32 SetProperty(
        gint32 iProp, const CBuffer& oBuf )
    {
        gint32 ret = 0;
        switch( iProp )
        {
        case propObjPath:
            {
                m_strObjPath = oBuf;
                break;
            }
        case propIfName:
            {
                m_strIfName = oBuf;
                break;
            }
        case propMatchType:
            {
                guint32 dwType = oBuf;
                SetType( ( EnumMatchType )dwType );
                break;
            }
        default:
            {
                ret = m_pCfg->SetProperty(
                    iProp, oBuf );
                break;
            }
        }
        return ret;
    }

    gint32 RemoveProperty( gint32 iProp )
    {
        gint32 ret = 0;
        switch( iProp )
        {
        case propObjPath:
            {
                m_strObjPath.clear();
                break;
            }
        case propIfName:
            {
                m_strIfName.clear();
                break;
            }
        case propMatchType:
            {
                SetType( matchInvalid );
                break;
            }
        default:
            {
                ret = m_pCfg->
                    RemoveProperty( iProp );
                break;
            }
        }
        return ret;
    }

    const CMessageMatch& operator=(
        const CMessageMatch& rhs )
    {
        m_strObjPath = rhs.m_strObjPath;
        m_strIfName = rhs.m_strIfName;
        SetType( rhs.GetType() );
        if( !rhs.m_pCfg.IsEmpty() )
        {
            *m_pCfg = *rhs.m_pCfg;
        }
        else
        {
            m_pCfg.Clear();
        }

        return *this;
    }

    virtual gint32 Serialize(
        CBuffer& oBuf ) const
    {
        gint32 ret = 0;
        CfgPtr pCfg( true );
        *pCfg = *m_pCfg;
        ( *pCfg )[ propIfName ] = m_strIfName;
        ( *pCfg )[ propObjPath ] = m_strObjPath;

        SERI_HEADER oHeader;

        oHeader.iMatchType = GetType();

        BufPtr pCfgBuf( true );
        ret = pCfg->Serialize( *pCfgBuf );

        if( ERROR( ret ) )
            return ret;

        // NOTE: we DON'T use the 
        // clsid( CMessageMatch )
        oHeader.dwClsid = GetClsid();

        oHeader.dwSize =
            pCfgBuf->size();

        oHeader.hton();
        ret = oBuf.Append(
            ( guint8* )&oHeader,
            sizeof( oHeader ) );

        if( ERROR( ret ) )
            return ret;

        ret = oBuf.Append(
            ( guint8* )pCfgBuf->ptr(),
            pCfgBuf->size() );

        if( ERROR( ret ) )
            return ret;

        return 0;
    }

    virtual gint32 Deserialize(
        const CBuffer& oBuf )
    {
        if( oBuf.empty() )
            return -EINVAL;

        return Deserialize(
            oBuf.ptr(), oBuf.size() );
    }

    virtual gint32 Deserialize(
        const char* pBuf, guint32 dwSize )
    {
        gint32 ret = 0;

        do{
            if( pBuf == nullptr )
            {
                ret = -EINVAL;
                break;
            }

            SERI_HEADER* pHeader =
                ( SERI_HEADER* )pBuf;
            
            SERI_HEADER oHeader;
            // memcpy( &oHeader, pHeader, sizeof( oHeader ) );
            oHeader = *pHeader;
            oHeader.ntoh();

            // NOTE: we DON'T hardcode the clsid(
            // CMessageMatch )
            if( oHeader.dwClsid != GetClsid() )
            {
                ret = -EINVAL;
                break;
            }

            gint32 ret = m_pCfg.NewObj();
            if( ERROR( ret ) )
                break;

            ret = m_pCfg->Deserialize(
                pBuf + sizeof( oHeader ),
                oHeader.dwSize );

            if( ERROR( ret ) )
                return ret;

            if( !m_pCfg->exist( propIfName ) )
            {
                ret = -EFAULT;
                break;
            }
            if( !m_pCfg->exist( propObjPath ) )
            {
                ret = -EFAULT;
                break;
            }

            m_strIfName = ( *m_pCfg )[ propIfName ];
            m_strObjPath = ( *m_pCfg )[ propObjPath ];
            SetType( ( EnumMatchType )oHeader.iMatchType );

        }while( 0 );

        return ret;
    }

    const CfgPtr& GetCfg() const
    { return m_pCfg; }

    CfgPtr& GetCfg()
    { return m_pCfg; }
};

class CProxyMsgMatch : public CMessageMatch
{

    public:

    typedef CMessageMatch super;

    CProxyMsgMatch( const IConfigDb* pCfg ) :
        CMessageMatch( pCfg )
    {
        SetClassId( clsid( CProxyMsgMatch ) );

        if( pCfg == nullptr )
            return;

       CCfgOpenerObj oThisCfg( this );
       oThisCfg.CopyProp( propIpAddr, pCfg );
    }
    
    // test if a event message this match is
    // listening to

    gint32 IsMyMsgIncoming(
        DBusMessage* pDBusMsg ) const 
    {
        gint32 ret =
            super::IsMyMsgIncoming( pDBusMsg );

        if( ERROR( ret ) )
            return ret;

        DMsgPtr pMsg( pDBusMsg );
        ObjPtr pTransCtx;
        ret = pMsg.GetObjArgAt( 0, pTransCtx );
        if( ERROR( ret ) )
            return ret;

        CCfgOpenerObj oCfg( this );
        ret = oCfg.IsEqualProp(
            propConnHandle, pTransCtx );

        if( ERROR( ret ) )
            return ret;

        std::string strEvtPath;
        CCfgOpener oTransCtx(
            ( IConfigDb* )pTransCtx );

        ret = oTransCtx.GetStrProp(
            propRouterPath, strEvtPath );

        if( ERROR( ret ) )
            return ret;

        std::string strExpPath;
        ret = oCfg.GetStrProp(
            propRouterPath, strExpPath );

        if( ERROR( ret ) )
            return ret;
        
        if( strEvtPath == strExpPath )
            return STATUS_SUCCESS;

        ret = ERROR_FALSE;
        if( strEvtPath.size() > strExpPath.size() )
            return ret;

        guint32 dwLen = strEvtPath.size();
        if( dwLen < strExpPath.size() )
        {
            std::string strVal =
                strExpPath.substr( 0, dwLen );

            if( pMsg.GetMember() != 
                SYS_EVENT_RMTSVREVENT )
                return ret;

            if( strVal != strEvtPath )
                return ret;

            if( strVal[ dwLen - 1 ] == '/' )
                return STATUS_SUCCESS;

            if( strExpPath[ dwLen ] == '/' )
                return STATUS_SUCCESS;
        }
        return ERROR_FALSE;
    }

    std::string ToString() const
    {
        std::string strAll = super::ToString();

        CCfgOpenerObj oCfg( this );

        guint32 dwConnHandle = 0;
        gint32 ret = oCfg.GetIntProp(
            propConnHandle, dwConnHandle );

        if( ERROR( ret ) )
            return strAll;

        strAll += ",ConnHandle=";
        strAll += std::to_string( dwConnHandle );

        std::string strValue;
        ret = oCfg.GetStrProp(
            propRouterPath, strValue );
        if( ERROR( ret ) )
            return strAll;

        strAll += ",RouterPath=";
        strAll += strValue; 

        return strAll;
    }
};

class CDBusDisconnMatch : public CMessageMatch
{

    public:

    typedef CMessageMatch super;

    CDBusDisconnMatch( const IConfigDb* pCfg ) :
        CMessageMatch( pCfg )
    {
        SetClassId( clsid( CDBusDisconnMatch ) );

        gint32 ret = 0;

        if( pCfg != nullptr )
        {
            m_pCfg = pCfg;
        }

        do{
            CCfgOpenerObj oCfg( this );

            ret = oCfg.SetStrProp(
                propObjPath, DBUS_PATH_LOCAL );

            if( ERROR( ret ) )
                break;

            ret = oCfg.SetStrProp(
                propIfName, DBUS_INTERFACE_LOCAL );

            if( ERROR( ret ) )
                break;

            ret = oCfg.SetIntProp(
                propMatchType, matchClient );

            if( ERROR( ret ) )
                break;

            ret = oCfg.SetStrProp(
                propMethodName, "Disconnected" );

        }while( 0 );

        if( ERROR( ret ) )
        {
            std::string strInfo = DebugMsg(
                ret, "Error in CDBusDisconnMatch ctor" );

            throw std::runtime_error( strInfo );
        }
        return;
    }

    virtual gint32 IsMyMsgIncoming(
        DBusMessage* pMessage ) const
    {
        DMsgPtr pMsg( pMessage );

        gint32 ret = super::IsMyMsgIncoming( pMsg );

        if( ERROR( ret ) )
            return ret;

        std::string strMsgMember;

        strMsgMember = pMsg.GetMember();

        if( strMsgMember.empty() )
            return ERROR_FALSE;

        CCfgOpener oCfg( ( IConfigDb* )m_pCfg );
        std::string strMember;

        ret = oCfg.GetStrProp(
            propMethodName, strMember );

        if( ERROR( ret ) )
            return ret;

        if( strMember != strMsgMember )
            return ERROR_FALSE;

        return 0;
    }

    std::string ToString() const
    {
        std::string strAll = super::ToString();
        strAll += ",Member=";
        strAll += "Disconnected"; 
        return strAll;
    }
};

class CDBusSysMatch : public CMessageMatch
{
    public:
    CDBusSysMatch( const IConfigDb* pCfg = nullptr ) :
        CMessageMatch( nullptr )
    {
        SetClassId( clsid( CDBusSysMatch ) );

        gint32 ret = 0;
        do{
            CCfgOpenerObj matchCfg( this );

            ret = matchCfg.SetStrProp(
                propObjPath, DBUS_SYS_OBJPATH );

            if( ERROR( ret ) )
                break;

            ret = matchCfg.SetStrProp(
                propIfName, DBUS_SYS_INTERFACE );

            if( ERROR( ret ) )
                break;

            ret = matchCfg.SetIntProp(
                propMatchType, matchClient );

            if( ERROR( ret ) )
                break;

            ret = matchCfg.SetStrProp(
                propDestDBusName, 
                DBUS_SERVICE_DBUS );

            if( ERROR( ret ) )
                break;

        }while( 0 );

        if( ERROR( ret ) )
        {
            std::string strMsg = DebugMsg( ret,
                "error in CDBusSysMatch's ctor" );

            throw std::runtime_error( strMsg );
        }
    }
};

class CDBusLoopbackMatch : public CMessageMatch
{
    gint32 FilterMsgS2P(
        DBusMessage* pDBusMsg, bool bIncoming ) const;

    gint32 FilterMsgP2S(
        DBusMessage* pDBusMsg, bool bIncoming ) const;

    gint32 AddRemoveBusName(
        const std::string& strName, bool bRemove );

    public:
    typedef CMessageMatch super;
    CDBusLoopbackMatch ( const IConfigDb* pCfg );

    virtual gint32 IsMyMsgIncoming(
        DBusMessage* pDBusMsg ) const;

    virtual gint32 IsMyMsgOutgoing(
        DBusMessage* pDBusMsg ) const;

    gint32 AddBusName(
        const std::string& strName );

    gint32 RemoveBusName(
        const std::string& strName );

    gint32 IsRegBusName(
        const std::string& strBusName ) const;

};


class CRouterRemoteMatch : public CMessageMatch
{
    public:
    typedef CMessageMatch super;
    CRouterRemoteMatch( const IConfigDb* pCfg = nullptr ) :
        CMessageMatch( pCfg )
    {
        SetClassId( clsid( CRouterRemoteMatch ) );
        SetType( matchClient );
    }

    virtual gint32 CopyMatch( IMessageMatch* pMatch )
    {
        CCfgOpenerObj oCfg( this );
        gint32 ret = 0;

        do{
            ret = oCfg.CopyProp( propObjPath, pMatch );
            if( ERROR( ret ) )
                break;

            ret = oCfg.CopyProp( propIfName, pMatch );
            if( ERROR( ret ) )
                break;

            CCfgOpener oCfg2( ( IConfigDb* )GetCfg() );
            IConfigDb* prhs = pMatch->GetCfg();

            ret = oCfg2.CopyProp(
                propDestDBusName, prhs );

            if( ERROR( ret ) )
                break;

            ret = oCfg2.CopyProp( propRouterPath, prhs );
            if( ERROR( ret ) )
                break;

            ret = oCfg2.CopyProp( propPortId, prhs );
            if( oCfg2.IsEqual(
                propRouterPath, std::string( "/" ) ) )
                break;

            // optional
            oCfg.CopyProp( propPrxyPortId, pMatch );

        }while( 0 );

        return ret;
    }

    virtual std::string ToString() const
    {
        std::string strAll = super::ToString();

        CCfgOpenerObj oCfg( this );

        guint32 dwPortId = 0;
        gint32 ret = oCfg.GetIntProp(
            propPortId, dwPortId );

        if( ERROR( ret ) )
            return strAll;

        strAll += ",PortId=";
        strAll += std::to_string( dwPortId );

        std::string strValue;
        oCfg.GetStrProp(
            propDestDBusName, strValue );

        strAll += ",Destination=";
        strAll += strValue;

        ret = oCfg.GetStrProp(
            propRouterPath, strValue );
        if( ERROR( ret ) )
            return strAll;

        strAll += ",RouterPath=";
        strAll += strValue; 

        return strAll;
    }

    gint32 IsMyReqToRelay(
        IConfigDb* pReqCtx,
        DBusMessage* pReqMsg ) const
    {
        gint32 ret =
            super::IsMyMsgOutgoing( pReqMsg );

        if( ERROR( ret ) )
            return ret;
        do{
            DMsgPtr pMsg( pReqMsg );
            CCfgOpener oCfg(
                ( IConfigDb* )GetCfg() );

            std::string strDest =
                pMsg.GetDestination();

            ret = oCfg.IsEqual(
                propDestDBusName, strDest );

            if( ERROR( ret ) )
                break;

            ret = oCfg.IsEqualProp(
                propRouterPath, pReqCtx );

            if( ERROR( ret ) )
                break;

            guint32 dwPortId = 0;
            CCfgOpener oReqCtx( pReqCtx );
            ret = oReqCtx.GetIntProp(
                propConnHandle, dwPortId );
            if( ERROR( ret ) )
                break;

            ret = oCfg.IsEqual(
                propPortId, dwPortId );

            break;

        }while( 0 );

        return ret;
    }

    gint32 IsMyModEvent(
        DBusMessage* pEvtMsg ) const
    {
        DMsgPtr pMsg( pEvtMsg );
        if( pMsg.GetMember() !=
            "NameOwnerChanged" )
            return ERROR_FALSE;

        std::string strModName;
        gint32 ret = pMsg.GetStrArgAt(
            0, strModName );

        if( ERROR( ret ) )
            return ret;

        CCfgOpener oCfg(
            ( IConfigDb* )GetCfg() );

        return oCfg.IsEqual(
            propDestDBusName, strModName );
    }

    gint32 IsMyEvtToForward(
        IConfigDb* pEvtCtx,
        DBusMessage* pEvtMsg ) const
    {
        CCfgOpener oCfg(
            ( IConfigDb* )GetCfg() );

        gint32 ret = oCfg.IsEqualProp(
            propRouterPath, pEvtCtx );

        if( ERROR( ret ) )
            return ERROR_FALSE;
        
        ret = super::IsMyMsgIncoming( pEvtMsg );

        if( ERROR( ret ) )
        {
            ret = IsMyModEvent( pEvtMsg );
            if( ERROR( ret ) )
                return ERROR_FALSE;
        }

        return ret;
    }

    virtual gint32 IsMyModule(
        const std::string strModName )
    {
        if( strModName.empty() )
            return -EINVAL;

        std::string strModPath =
            DBUS_DESTINATION( strModName );

        if( strModPath.size() >=
            m_strObjPath.size() )
            return ERROR_FALSE;

        std::replace( strModPath.begin(),
            strModPath.end(), '.', '/');

        std::string strPrefix;
        strPrefix = m_strObjPath.substr(
            0, strModPath.size() );
        
        if( strPrefix != strModPath )
            return ERROR_FALSE;

        std::string strObjName;
        strObjName = m_strObjPath.substr(
            strModPath.size(),
            m_strObjPath.size() );

        if( strObjName.empty() )
            return ERROR_FALSE;

        if( strObjName[ 0 ] != '/' )
            return ERROR_FALSE;

        strObjName[ 0 ] = 'O';

        if( strObjName.find( '/' ) !=
            std::string::npos )
            return ERROR_FALSE;

        return 0;
    }

    guint32 GetPortId()
    {
        CCfgOpenerObj oCfg( this );
        guint32 dwPortId = 0;
        oCfg.GetIntProp( propPortId, dwPortId );
        return dwPortId;
    }
};

class CRouterLocalMatch : public CRouterRemoteMatch
{
    public:
    typedef CRouterRemoteMatch super;
    CRouterLocalMatch( const IConfigDb* pCfg = nullptr ) :
        CRouterRemoteMatch( pCfg )
    {
        SetClassId( clsid( CRouterLocalMatch ) );
        SetType( matchClient );
    }

    gint32 CopyMatch( IMessageMatch* pMatch )
    {
        CCfgOpenerObj oCfg( this );
        gint32 ret = 0;

        do{
            ret = oCfg.CopyProp(
                propObjPath, pMatch );
            if( ERROR( ret ) )
                break;

            ret = oCfg.CopyProp(
                propIfName, pMatch );
            if( ERROR( ret ) )
                break;

            CCfgOpener oCfg2( ( IConfigDb* )GetCfg() );
            IConfigDb* prhs = pMatch->GetCfg();

            ret = oCfg2.CopyProp(
                propDestDBusName, prhs );

            if( ERROR( ret ) )
                break;

            ret = oCfg2.CopyProp(
                propRouterPath, prhs );
            if( ERROR( ret ) )
                break;

            ret = oCfg2.CopyProp(
                propSrcDBusName, prhs );
            if( ERROR( ret ) )
                break;

            ret = oCfg2.CopyProp(
                propSrcUniqName, prhs );
            if( ERROR( ret ) )
                break;

            oCfg2.CopyProp(
                propPrxyPortId, prhs );

        }while( 0 );

        return ret;
    }

    std::string ToString() const
    {
        std::string strAll =
            super::super::ToString();

        std::string strValue;
        CCfgOpenerObj oCfg( this );

        oCfg.GetStrProp(
            propSrcUniqName, strValue );

        strAll += ",UniqName=";
        strAll += strValue;

        guint32 dwPortId = 0;
        oCfg.GetIntProp(
            propPrxyPortId, dwPortId );

        strAll += ",PrxyPortId=";
        strAll += std::to_string( dwPortId );

        oCfg.GetStrProp(
            propRouterPath, strValue );

        strAll += ",RouterPath=";
        strAll += strValue;

        return strAll;
    }

    gint32 IsMyReqToForward(
        IConfigDb* pReqCtx,
        DBusMessage* pReqMsg ) const
    {
        gint32 ret =
            super::IsMyMsgOutgoing( pReqMsg );

        if( ERROR( ret ) )
            return ret;

        CCfgOpener oReqCtx( pReqCtx );
        CCfgOpener oCfg(
            ( IConfigDb* )GetCfg() );

        ret = oCfg.IsEqualProp(
            propRouterPath, pReqCtx );
        if( ERROR( ret ) )
            return ret;
        
        DMsgPtr pMsg( pReqMsg );
        stdstr strVal = pMsg.GetSender();

        ret = oCfg.IsEqual( propSrcDBusName, 
            pMsg.GetSender() );

        if( ERROR( ret ) )
        {
            ret = oCfg.IsEqual(
                propSrcUniqName, 
                pMsg.GetSender() );
        }

        if( ERROR( ret ) )
            return ret;

        return 0;
    }

    gint32 IsMyEvtToRelay(
        IConfigDb* pEvtCtx,
        DBusMessage* pEvtMsg ) const
    {
        gint32 ret = 0;

        do{
            CCfgOpener oCfg(
                ( IConfigDb* )GetCfg() );

            ret = oCfg.IsEqualProp(
                propRouterPath, pEvtCtx );

            if( ERROR( ret ) )
                break;

            ret = super::IsMyMsgIncoming(
                pEvtMsg );

            if( ERROR( ret ) )
            {
                ret = IsMyModEvent( pEvtMsg );
                if( ERROR( ret ) )
                    break;
            }

            CCfgOpener oEvtCtx( pEvtCtx );

            guint32 dwPortId = 0;
            ret = oEvtCtx.GetIntProp(
                propConnHandle, dwPortId );
            if( ERROR( ret ) )
                break;

            ret = oCfg.IsEqual(
                propPrxyPortId, dwPortId );

        }while( 0 );

        return ret;
    }

    virtual gint32 IsMyModule(
        const std::string strModName )
    {
        if( strModName.empty() )
            return -EINVAL;

        gint32 ret =
            super::IsMyModule( strModName );

        if( SUCCEEDED( ret ) )
            return ret;

        std::string strObjPath = strModName;
        std::replace( strObjPath.begin(),
            strObjPath.end(), '.', '/');

        CCfgOpenerObj oCfg( this );
        ret = oCfg.IsEqual( propSrcDBusName,
            strModName );

        if( SUCCEEDED( ret ) )
             return ret;

        ret = oCfg.IsEqual( propDestDBusName,
            strModName );
 
        if( SUCCEEDED( ret ) )
             return ret;

        return ERROR_FALSE;
    }
};

}
