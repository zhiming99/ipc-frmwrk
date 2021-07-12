/*
 * =====================================================================================
 *
 *       Filename:  gencpp.cpp
 *
 *    Description:  implementations of proxy/server generator for C++
 *
 *        Version:  1.0
 *        Created:  02/25/2021 01:34:32 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ming Zhi( woodhead99@gmail.com )
 *   Organization:
 *
 *      Copyright:  2021 Ming Zhi( woodhead99@gmail.com )
 *
 *        License:  Licensed under GPL-3.0. You may not use this file except in
 *                  compliance with the License. You may find a copy of the
 *                  License at 'http://www.gnu.org/licenses/gpl-3.0.html'
 *
 * =====================================================================================
 */
#include <sys/stat.h>
#include "rpc.h"
using namespace rpcf;
#include "astnode.h"
#include "gencpp.h"
#include "sha1.h"

extern std::string g_strAppName;

std::map< gint32, char > g_mapTypeSig =
{
    { TOK_UINT64, 'Q' },
    { TOK_INT64, 'q' },
    { TOK_UINT32, 'D' },
    { TOK_INT32, 'd' },
    { TOK_UINT16, 'W' },
    { TOK_INT16, 'w' },
    { TOK_FLOAT, 'f' },
    { TOK_DOUBLE, 'F' },
    { TOK_BOOL, 'b' },
    { TOK_BYTE, 'B' },
    { TOK_HSTREAM, 'h' },
    { TOK_STRING, 's' },
    { TOK_BYTEARR, 'a' },
    { TOK_OBJPTR, 'o' },
    { TOK_STRUCT, 'O' }
};

gint32 CArgListUtils::GetArgCount(
    ObjPtr& pArgs ) const
{
    if( pArgs.IsEmpty() )
        return 0;

    CArgList* pal = pArgs;
    if( pal == nullptr )
        return 0;

    return pal->GetCount();
}

gint32 CArgListUtils::ToStringInArgs(
    ObjPtr& pArgs,
    std::vector< std::string >& vecArgs ) const
{
    CArgList* pal = pArgs;
    std::string strVal;
    if( pal == nullptr )
        return STATUS_SUCCESS;

    do{
        guint32 dwCount = pal->GetCount();
        for( guint32 i = 0; i < dwCount; i++ )
        {
            ObjPtr pObj = pal->GetChild( i );
            CFormalArg* pfa = pObj;
            if( pfa == nullptr )
                continue;
            pObj = pfa->GetType();
            guint32 dwClsid = pObj->GetClsid();
            std::string strType;
            std::string strVarName = pfa->GetName();
            if( dwClsid != clsid( CPrimeType ) )
            {
                CAstNodeBase* pType = pObj;
                strType = std::string( "const " ) +
                    pType->ToStringCpp() + "&";
            }
            else 
            {
                CPrimeType* pType = pObj;
                guint32 dwName = pType->GetName();
                if( dwName == TOK_STRING )
                {
                    strType = std::string( "const " ) +
                        pType->ToStringCpp() + "&";
                }
                else if( dwName == TOK_OBJPTR ||
                    dwName == TOK_BYTEARR )
                {
                    strType = pType->ToStringCpp();
                    strType += "&";
                }
                else if( dwName == TOK_HSTREAM )
                {
                    strType = "HANDLE";
                    strVarName += "_h";
                }
                else
                {
                    strType = pType->ToStringCpp();
                }
            }

            vecArgs.push_back(
                strType + " " + strVarName );
        }

    }while( 0 );

    return STATUS_SUCCESS;
}

gint32 CArgListUtils::ToStringOutArgs(
    ObjPtr& pArgs,
    std::vector< std::string >& vecArgs ) const
{
    CArgList* pal = pArgs;
    std::string strVal;
    if( pal == nullptr )
        return STATUS_SUCCESS;

    do{
        guint32 dwCount = pal->GetCount();
        for( guint32 i = 0; i < dwCount; i++ )
        {
            ObjPtr pObj = pal->GetChild( i );
            CFormalArg* pfa = pObj;
            if( pfa == nullptr )
                continue;
            pObj = pfa->GetType();
            guint32 dwClsid = pObj->GetClsid();
            std::string strType;
            std::string strVarName = pfa->GetName();
            if( dwClsid != clsid( CPrimeType ) )
            {
                CAstNodeBase* pType = pObj;
                strType = pType->ToStringCpp() + "&";
            }
            else 
            {
                CPrimeType* pType = pObj;
                guint32 dwName = pType->GetName();
                if( dwName != TOK_HSTREAM )
                {
                    strType = pType->ToStringCpp() + "&";
                }
                else
                {
                    strType = "HANDLE&";
                    strVarName += "_h";
                }
            }

            vecArgs.push_back(
                strType + " " + strVarName );
        }

    }while( 0 );

    return STATUS_SUCCESS;
}
std::string CArgListUtils::DeclPtrLocal(
    const std::string& strType,
    const std::string strVar ) const
{
    std::string strRet = "DECLPTRO( ";
    strRet += strType + ", " +
        strVar + " )";
    return strRet;
}

gint32 CArgListUtils::GenLocals(
    ObjPtr& pArgList,
    std::vector< std::string >& vecLocals,
    bool bObjPtr ) const
{
    CArgList* pinal = pArgList;
    if( pinal == nullptr )
        return -EINVAL;
   
    gint32 ret = 0; 

    do{
        guint32 i = 0;
        for( ; i < pinal->GetCount(); i++ )
        {
            ObjPtr pObj = pinal->GetChild( i );
            CFormalArg* pfa = pObj;
            if( pfa == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            pObj = pfa->GetType();
            CAstNodeBase* pNode = pObj;
            std::string strType =
                pNode->ToStringCpp();

            std::string strLocal =
                pfa->GetName();

            std::string strSig =
                pNode->GetSignature();

            bool bNonStruct =
                !( ( strSig.size() == 1 ) &&
                ( strSig[ 0 ] == 'O' ) );

            if( bNonStruct || !bObjPtr )
            {
                strType += " ";
                vecLocals.push_back(
                    strType + strLocal );
            }
            else
            {
                vecLocals.push_back(
                    DeclPtrLocal(
                    strType, strLocal ) );
            }
        }

    }while( 0 );

    return ret;
}

gint32 CArgListUtils::GetHstream(
    ObjPtr& pArgList,
    std::vector< ObjPtr >& vecHstms ) const
{
    CArgList* pinal = pArgList;
    if( pinal == nullptr )
        return -EINVAL;
   
    gint32 ret = 0; 

    do{
        guint32 i = 0;
        for( ; i < pinal->GetCount(); i++ )
        {
            ObjPtr pObj = pinal->GetChild( i );
            CFormalArg* pfa = pObj;
            if( pfa == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            pObj = pfa->GetType();
            CAstNodeBase* pNode = pObj;
            std::string strSig =
                pNode->GetSignature();

            if( strSig[ 0 ] == 'h' )
            {
                vecHstms.push_back(
                    ObjPtr( pfa ) );
            }
        }

    }while( 0 );

    return ret;
}

gint32 CArgListUtils::GetArgsForCall(
    ObjPtr& pArgList,
    std::vector< std::string >& vecArgs )  const
{
    CArgList* pinal = pArgList;
    if( pinal == nullptr )
        return 0;
   
    gint32 ret = 0; 

    do{
        guint32 i = 0;
        for( ; i < pinal->GetCount(); i++ )
        {
            ObjPtr pObj = pinal->GetChild( i );
            CFormalArg* pfa = pObj;
            if( pfa == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            std::string strVarName =
                pfa->GetName();

            pObj = pfa->GetType();
            CAstNodeBase* pNode = pObj;
            std::string strSig =
                pNode->GetSignature();

            if( strSig[ 0 ] == 'h' )
                strVarName += ".m_hStream";

            vecArgs.push_back( strVarName );
        }

    }while( 0 );

    return ret;
}

gint32 CArgListUtils::GetArgTypes(
    ObjPtr& pArgList,
    std::set< ObjPtr >& setTypes ) const
{
    CArgList* pinal = pArgList;

    if( pinal == nullptr )
        return -EINVAL;
   
    gint32 ret = 0; 
    do{
        guint32 i = 0;
        for( ; i < pinal->GetCount(); i++ )
        {
            ObjPtr pObj = pinal->GetChild( i );
            CFormalArg* pfa = pObj;
            if( pfa == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            pObj = pfa->GetType();
            setTypes.insert( pObj );
        }

    }while( 0 );
    return ret;
}

gint32 CArgListUtils::GetArgTypes(
    ObjPtr& pArgList,
    std::vector< std::string >& vecTypes ) const
{
    CArgList* pinal = pArgList;
    if( pinal == nullptr )
        return -EINVAL;
   
    gint32 ret = 0; 
    do{
        guint32 i = 0;
        for( ; i < pinal->GetCount(); i++ )
        {
            ObjPtr pObj = pinal->GetChild( i );
            CFormalArg* pfa = pObj;
            if( pfa == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            pObj = pfa->GetType();
            CAstNodeBase* pNode = pObj;
            std::string strType =
                pNode->ToStringCpp();

            vecTypes.push_back( strType );
        }

    }while( 0 );
    return ret;
}

gint32 CMethodWriter::GenActParams(
    ObjPtr& pArgList )
{
    if( GetArgCount( pArgList ) == 0 )
        return STATUS_SUCCESS;

    gint32 ret = 0;
    do{
        std::vector< std::string > vecArgs;
        ret = GetArgsForCall(
            pArgList, vecArgs );
        if( ERROR( ret ) )
            break;

        guint32 i = 0;
        for( ;i < vecArgs.size(); i++ )
        {
            CCOUT << vecArgs[ i ];
            if( i + 1 < vecArgs.size() )
            {
                CCOUT << ",";
                NEW_LINE;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CMethodWriter::GenActParams(
    ObjPtr& pArgList, ObjPtr& pArgList2 )
{
    guint32 dwCount =
        GetArgCount( pArgList );

    guint32 dwCount2 =
        GetArgCount( pArgList2 );

    if( dwCount == 0 && dwCount2 == 0 )
        return STATUS_SUCCESS;

    if( dwCount == 0 )
        return GenActParams( pArgList2 );

    if( dwCount2 == 0 )
        return GenActParams( pArgList );

    gint32 ret = 0;
    do{
        std::vector< std::string > vecArgs;
        ret = GetArgsForCall(
            pArgList, vecArgs );
        if( ERROR( ret ) )
            break;

        ret = GetArgsForCall(
            pArgList2, vecArgs );
        if( ERROR( ret ) )
            break;

        guint32 i = 0;
        for( ;i < vecArgs.size(); i++ )
        {
            CCOUT << vecArgs[ i ];
            if( i + 1 < vecArgs.size() )
            {
                CCOUT << ",";
                NEW_LINE;
            }
        }

    }while( 0 );

    return ret;
}

gint32 CMethodWriter::GenSerialArgs(
    ObjPtr& pArgList,
    const std::string& strBuf,
    bool bDeclare, bool bAssign, bool bNoRet )
{
    gint32 ret = 0;
    if( GetArgCount( pArgList ) == 0 )
        return 0;

    do{
        NEW_LINE;
        Wa( "CSerialBase oSerial_;" );
        CEmitSerialCode oesc(
            m_pWriter, pArgList );

        std::vector< ObjPtr > vecHstms;
        ret = GetHstream(
            pArgList, vecHstms );
        if( ERROR( ret ) )
            break;

        for( auto& elem : vecHstms )
        {
            CFormalArg* pfa = elem;
            std::string strLocal =
                pfa->GetName();

            if( bDeclare )
            {
                CCOUT << "HSTREAM "
                    << strLocal << ";";
                NEW_LINE;
            }

            CCOUT << strLocal <<
                ".m_pIf = this;";
            NEW_LINE;

            if( bAssign )
            {
                CCOUT << strLocal
                    << ".m_hStream = "
                    << strLocal << "_h;";
                NEW_LINE;
            }
        }

        CCOUT << "do";
        BLOCK_OPEN;
        ret = oesc.OutputSerial(
            "oSerial_", strBuf );
        if( ERROR( ret ) )
            break;
        NEW_LINE;
        CCOUT << strBuf << "->SetOffset( 0 );";
        BLOCK_CLOSE;
        CCOUT << "while( 0 );";
        NEW_LINES( 2 );
        CCOUT<< "if( ERROR( ret ) )";
        INDENT_UPL;
        if( bNoRet )
            CCOUT << "break;";
        else
            CCOUT << "return ret;";
        INDENT_DOWNL;

    }while( 0 );

    return ret;
}

gint32 CMethodWriter::GenDeserialArgs(
    ObjPtr& pArgList,
    const std::string& strBuf,
    bool bDeclare, bool bAssign, bool bNoRet )
{
    gint32 ret = 0;
    if( GetArgCount( pArgList ) == 0 )
        return -ENOENT;

    do{
        NEW_LINE;
        Wa( "CSerialBase oDeserial_;" );
        CEmitSerialCode oedsc(
            m_pWriter, pArgList );

        std::vector< ObjPtr > vecHstms;
        ret = GetHstream(
            pArgList, vecHstms );
        if( ERROR( ret ) )
            break;

        for( auto& elem : vecHstms )
        {
            CFormalArg* pfa = elem;
            std::string strLocal =
                pfa->GetName();

            if( bDeclare )
            {
                CCOUT << "HSTREAM "
                    << strLocal << ";";
                NEW_LINE;
            }
            CCOUT << strLocal <<
                ".m_pIf = this;";

            NEW_LINE;
        }

        CCOUT << "do";
        BLOCK_OPEN;
        CCOUT << "guint32 dwOrigOff = "
            << strBuf << "->offset();";
        NEW_LINE;
        ret = oedsc.OutputDeserial(
            "oDeserial_", strBuf );
        if( ERROR( ret ) )
            break;
        NEW_LINE;
        CCOUT << strBuf
            << "->SetOffset( dwOrigOff );";
        BLOCK_CLOSE;
        CCOUT << "while( 0 );";
        NEW_LINES( 2 );
        CCOUT<< "if( ERROR( ret ) )";
        INDENT_UPL;
        if( bNoRet )
        {
            Wa( "break;" );
        }
        else
        {
            Wa( "return ret;" );
        }
        INDENT_DOWNL;

        if( bAssign == false )
            break;

        if( vecHstms.empty() )
            break;

        for( auto& elem : vecHstms )
        {
            CFormalArg* pfa = elem;
            std::string strLocal =
                pfa->GetName();

            // assign the stream handle to
            // the out parameter
            CCOUT << strLocal << "_h = "
                << strLocal << ".m_hStream;";

            NEW_LINE;
        }

    }while( 0 );

    return ret;
}

gint32 CMethodWriter::DeclLocals(
    ObjPtr& pArgList,
    bool bObjPtr )
{
    std::vector< std::string > vecLocals;
    gint32 ret = GenLocals(
        pArgList, vecLocals, bObjPtr );
    if( ERROR( ret ) )
        return ret;

    for( auto& elem : vecLocals )
    {
        CCOUT << elem << ";";
        NEW_LINE;
    }
    return STATUS_SUCCESS;
}

gint32 CMethodWriter::GenFormArgs(
    ObjPtr& pArg, bool bIn, bool bShowDir )
{
    std::vector< std::string > vecArgs; 
    if( bIn )
        ToStringInArgs( pArg, vecArgs );
    else
        ToStringOutArgs( pArg, vecArgs );

    std::string strVal;
    if( vecArgs.empty() )
        return STATUS_SUCCESS;

    guint32 i = 0;
    for( ; i < vecArgs.size(); i++ )
    {
        CCOUT << vecArgs[ i ]; 

        if( bShowDir )
        {
            if( bIn )
                CCOUT << " /*[ In ]*/";
            else
                CCOUT << " /*[ Out ]*/";
        }

        if( i + 1 < vecArgs.size() )
        {
            CCOUT << ",";
            NEW_LINE;
        }
    }

    return STATUS_SUCCESS;
}

guint32 GenClsid( const std::string& strName )
{
    SHA1 sha;
    std::string strHash = g_strAppName + strName;
    sha.Input( strHash.c_str(), strHash.size() );

    unsigned char digest[20];
    sha.Result( (unsigned*)digest );
    unsigned char bottom = ( ( ( guint32 )
        clsid( UserClsidStart ) ) >> 24 ) + 1;
    unsigned char ceiling = 0xFF;
    guint32 i = 0;
    for( ; i < sizeof( digest ); ++i )
        if( digest[ i ] >= bottom &&
            digest[ i ] < ceiling )
            break;

    if( sizeof( digest ) - i < sizeof( guint32 ) )
    {
        std::string strMsg = DebugMsg( -1,
            "internal error the SHA1 hash is"
            " conflicting" );
        throw std::runtime_error( strMsg );
    }
    guint32 iClsid = 0;

    memcpy( &iClsid,
        digest + i, sizeof( iClsid ) );

    return iClsid;
}

#define GEN_FILEPATH( strRet, strPath, strName ) \
do{ \
    strRet = ( strPath ) + "/" + ( strName ); \
    gint32 ret = access( \
        strRet.c_str(), F_OK ); \
    if( ret == 0 ) \
        strRet += ".new"; \
}while( 0 )

CFileSet::CFileSet(
    const std::string& strOutPath,
    const std::string& strAppName )
{
    if( strAppName.empty() )
    {
        std::string strMsg =
            "appname is empty";
        throw std::invalid_argument( strMsg );
    }

    m_strAppHeader =
        strOutPath + "/" + strAppName + ".h";

    m_strAppCpp =
        strOutPath + "/" + strAppName + ".cpp";

    GEN_FILEPATH( m_strObjDesc,
        strOutPath, strAppName + "desc.json" );

    GEN_FILEPATH( m_strDriver,
        strOutPath, "driver.json" );

    GEN_FILEPATH( m_strMakefile,
        strOutPath, "Makefile" );

    GEN_FILEPATH( m_strMainCli,
        strOutPath, "maincli.cpp" );

    GEN_FILEPATH( m_strMainSvr, 
        strOutPath, "mainsvr.cpp" );

    m_strPath = strOutPath;

    gint32 ret = OpenFiles();
    if( ERROR( ret ) )
    {
        std::string strMsg = DebugMsg( ret,
            "internal error open files" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CFileSet::OpenFiles()
{
    STMPTR pstm( new std::ofstream(
        m_strAppHeader,
        std::ofstream::out |
        std::ofstream::trunc ) );

    m_vecFiles.push_back( std::move( pstm ) );

    pstm = STMPTR( new std::ofstream(
        m_strAppCpp,
        std::ofstream::out |
        std::ofstream::trunc) );

    m_vecFiles.push_back( std::move( pstm ) );

    pstm = STMPTR( new std::ofstream(
        m_strObjDesc,
        std::ofstream::out |
        std::ofstream::trunc) );

    m_vecFiles.push_back( std::move( pstm ) );

    pstm = STMPTR( new std::ofstream(
        m_strDriver,
        std::ofstream::out |
        std::ofstream::trunc) );

    m_vecFiles.push_back( std::move( pstm ) );

    pstm = STMPTR( new std::ofstream(
        m_strMakefile,
        std::ofstream::out |
        std::ofstream::trunc) );

    m_vecFiles.push_back( std::move( pstm ) );

    pstm = STMPTR( new std::ofstream(
        m_strMainCli,
        std::ofstream::out |
        std::ofstream::trunc) );

    m_vecFiles.push_back( std::move( pstm ) );

    pstm = STMPTR( new std::ofstream(
        m_strMainSvr,
        std::ofstream::out |
        std::ofstream::trunc) );

    m_vecFiles.push_back( std::move( pstm ) );

    return STATUS_SUCCESS;
}

gint32 CFileSet::AddSvcImpl(
    const std::string& strSvcName )
{
    if( strSvcName.empty() )
        return -EINVAL;
    gint32 ret = 0;
    do{
        gint32 idx = m_vecFiles.size();
        std::string strExt = ".cpp";
        std::string strExtHdr = ".h";
        std::string strCpp = m_strPath +
            "/" + strSvcName + "svr.cpp";
        std::string strCliCpp = m_strPath +
            "/" + strSvcName + "cli.cpp";

        ret = access( strCpp.c_str(), F_OK );
        if( ret == 0 )
        {
            strExt = "cpp.new";
            strCpp = m_strPath + "/" +
                strSvcName + "svr.cpp.new";
        }

        ret = access( strCliCpp.c_str(), F_OK );
        if( ret == 0 )
        {
            strExt = ".cpp.new";
            strCliCpp = m_strPath + "/" +
                strSvcName + "cli.cpp.new";
        }

        std::string strHeader = m_strPath +
            "/" + strSvcName + "svr.h";

        ret = access( strHeader.c_str(), F_OK );
        if( ret == 0 )
        {
            strExtHdr = ".h.new";
            strHeader = m_strPath + "/" +
                strSvcName + "svr.h.new";
        }

        std::string strCliHeader = m_strPath +
            "/" + strSvcName + "cli.h";

        ret = access( strCliHeader.c_str(), F_OK );
        if( ret == 0 )
        {
            strExtHdr = ".h.new";
            strCliHeader = m_strPath + "/" +
                strSvcName + "cli.h.new";
        }

        STMPTR pstm( new std::ofstream(
            strHeader,
            std::ofstream::out |
            std::ofstream::trunc ) );

        m_vecFiles.push_back( std::move( pstm ) );
        m_mapSvcImp[ strSvcName + "svr" + strExtHdr ] = idx;

        pstm = STMPTR( new std::ofstream(
            strCliHeader,
            std::ofstream::out |
            std::ofstream::trunc ) );

        idx += 1;
        m_vecFiles.push_back( std::move( pstm ) );
        m_mapSvcImp[ strSvcName + "cli" + strExtHdr ] = idx;

        pstm = STMPTR( new std::ofstream(
            strCpp,
            std::ofstream::out |
            std::ofstream::trunc) );

        idx += 1;
        m_vecFiles.push_back( std::move( pstm ) );
        m_mapSvcImp[ strSvcName + "svr" + strExt ] = idx;

        pstm = STMPTR( new std::ofstream(
            strCliCpp,
            std::ofstream::out |
            std::ofstream::trunc) );

        idx += 1;
        m_vecFiles.push_back( std::move( pstm ) );
        m_mapSvcImp[ strSvcName + "cli" + strExt ] = idx;

    }while( 0 );

    return ret;
}

CFileSet::~CFileSet()
{
    for( auto& elem : m_vecFiles )
    {
        if( elem != nullptr )
            elem->close();
    }
    m_vecFiles.clear();
}

gint32 SetStructRefs( ObjPtr& pRoot )
{
    if( pRoot.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        CStatements* pStmts = pRoot;
        if( pStmts == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        for( guint32 i = 0;
            i < pStmts->GetCount(); i++ )
        {
            ObjPtr pObj = pStmts->GetChild( i );
            guint32 dwClsid =
                ( guint32 )pObj->GetClsid();
            switch( dwClsid )
            {
            case clsid( CServiceDecl ):
                {
                    CSetStructRefs ossr( pObj );
                    ret = ossr.SetStructRefs();
                    if( ERROR( ret ) )
                        break;
                    break;
                }
            case clsid( CTypedefDecl ):
            case clsid( CStructDecl ) :
            case clsid( CInterfaceDecl ):
            case clsid( CAppName ) :
            case clsid( CConstDecl ) :
                {
                    break;
                }
            default:
                {
                    ret = -ENOTSUP;
                    break;
                }
            }
            if( ERROR( ret ) )
                break;
        }

    }while( 0 );

    return ret;
}

gint32 CHeaderPrologue::Output()
{
    Wa( "#pragma once" );
    Wa( "#include <string>" );
    Wa( "#include \"rpc.h\"" );
    Wa( "#include \"ifhelper.h\"" );
    if( m_pStmts->IsStreamNeeded() )
        Wa( "#include \"streamex.h\"" );
    else
        Wa( "#include \"seribase.h\"" );
    NEW_LINE;
    CCOUT << "#define DECLPTRO( _type, _name ) \\";
    INDENT_UPL;
    CCOUT << "ObjPtr p##_name;\\";
    NEW_LINE;
    CCOUT << "p##_name.NewObj( clsid( _type ) );\\";
    NEW_LINE;
    CCOUT << "_type& _name=*(_type*)p##_name;";
    INDENT_DOWNL;

    return STATUS_SUCCESS;
}

gint32 GenHeaderFile(
    CCppWriter* pWriter, ObjPtr& pRoot )
{
    if( pWriter == nullptr ||
        pRoot.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        pWriter->SelectHeaderFile();

        CHeaderPrologue oHeader(
            pWriter, pRoot );

        ret = oHeader.Output();
        if( ERROR( ret ) )
            break;

        CDeclareClassIds odci(
            pWriter, pRoot );

        ret = odci.Output();
        if( ERROR( ret ) )
            break;

        CStatements* pStmts = pRoot;
        if( pStmts == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        for( guint32 i = 0;
            i < pStmts->GetCount(); i++ )
        {
            ObjPtr pObj = pStmts->GetChild( i );
            guint32 dwClsid =
                ( guint32 )pObj->GetClsid();
            switch( dwClsid )
            {
            case clsid( CTypedefDecl ):
                {
                    CDeclareTypedef odt(
                        pWriter, pObj );
                    ret = odt.Output();
                    break;
                }
            case clsid( CStructDecl ) :
                {
                    CStructDecl* psd = pObj;
                    if( psd->RefCount() == 0 )
                        break;
                    CDeclareStruct ods(
                        pWriter, pObj );
                    ret = ods.Output();
                    break;
                }
            case clsid( CInterfaceDecl ):
                {
                    CInterfaceDecl* pNode = pObj;
                    if( pNode == nullptr )
                    {
                        ret = -EFAULT;
                        break;
                    }

                    if( pNode->RefCount() == 0 )
                        break;

                    CDeclInterfProxy odip(
                        pWriter, pObj );
                    ret = odip.Output();
                    if( ERROR( ret ) )
                        break;

                    CDeclInterfSvr odis(
                        pWriter, pObj );

                    ret = odis.Output();
                    break;
                }
            case clsid( CServiceDecl ):
                {
                    CDeclService ods(
                        pWriter, pObj );
                    ret = ods.Output();
                    if( ERROR( ret ) )
                        break;
                    break;
                }
            case clsid( CAppName ) :
            case clsid( CConstDecl ) :
                {
                    break;
                }
            default:
                {
                    ret = -ENOTSUP;
                    break;
                }
            }
            if( ERROR( ret ) )
                break;
        }

        std::vector< ObjPtr > vecSvcs;
        ret = pStmts->GetSvcDecls( vecSvcs );
        if( ERROR( ret ) )
            break;

        std::vector< std::pair< std::string, ObjPtr > > vecSvcNames;
        for( auto& elem : vecSvcs )
        {
            CServiceDecl* psd = elem;
            if( psd == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            vecSvcNames.push_back(
                { psd->GetName(), elem } );
            pWriter->AddSvcImpl( psd->GetName() );
        }

        for( auto& elem : vecSvcNames )
        {
            // server declarations
            ret = pWriter->SelectImplFile(
                elem.first + "svr.h" ); 

            if( ERROR( ret ) )
            {
                ret = pWriter->SelectImplFile(
                    elem.first + "svr.h.new" );
            }

            CDeclServiceImpl osi(
                pWriter, elem.second, true );

            ret = osi.Output();
            if( ERROR( ret ) )
                break;

            // client declarations
            ret = pWriter->SelectImplFile(
                elem.first + "cli.h" ); 

            if( ERROR( ret ) )
            {
                ret = pWriter->SelectImplFile(
                    elem.first + "cli.h.new" );
            }

            CDeclServiceImpl osi2(
                pWriter, elem.second, false );

            ret = osi2.Output();
            if( ERROR( ret ) )
                break;

            /* svr implementions*/
            ret = pWriter->SelectImplFile(
                elem.first + "svr.cpp" ); 

            if( ERROR( ret ) )
            {
                ret = pWriter->SelectImplFile(
                    elem.first + "svr.cpp.new" );
            }

            if( ERROR( ret ) )
                break;

            CImplServiceImpl oisi(
                pWriter, elem.second, true );

            ret = oisi.Output();
            if( ERROR( ret ) )
                break;

            /* client implementions*/
            ret = pWriter->SelectImplFile(
                elem.first + "cli.cpp" ); 

            if( ERROR( ret ) )
            {
                ret = pWriter->SelectImplFile(
                    elem.first + "cli.cpp.new" );
            }

            if( ERROR( ret ) )
                break;

            CImplServiceImpl oisi2(
                pWriter, elem.second, false );

            ret = oisi2.Output();
            if( ERROR( ret ) )
                break;
        }

        if( !vecSvcNames.empty() )
        {
            CImplMainFunc omf( pWriter, pRoot );
            ret = omf.Output();
            if( ERROR( ret ) )
                break;
        }

    }while( 0 );

    return ret;
}

gint32 GenCppFile(
    CCppWriter* m_pWriter, ObjPtr& pRoot )
{
    if( m_pWriter == nullptr ||
        pRoot.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;
    do{
        m_pWriter->SelectCppFile();

        CStatements* pStmts = pRoot;
        if( pStmts == nullptr )
        {
            ret = -EFAULT;
            break;
        }

        std::string strName = pStmts->GetName();

        Wa( "// Generated by ridlc" );
        Wa( "// This file content could be rewritten by ridlc" );
        Wa( "#include \"rpc.h\"" );
        Wa( "#include \"iftasks.h\"" );

        Wa( "using namespace rpcf;" );
        CCOUT << "#include \"" << strName << ".h\"";
        NEW_LINE;
        std::vector< ObjPtr > vecSvcs;
        pStmts->GetSvcDecls( vecSvcs );
        /*for( auto& elem : vecSvcs )
        {
            CServiceDecl* psvd = elem;
            std::string strName = psvd->GetName();
            CCOUT<< "#include \"" <<
                strName << ".h\"";
            NEW_LINE;
        }*/
        NEW_LINE;

        for( guint32 i = 0;
            i < pStmts->GetCount(); i++ )
        {
            ObjPtr pObj = pStmts->GetChild( i );
            guint32 dwClsid =
                ( guint32 )pObj->GetClsid();
            switch( dwClsid )
            {
            case clsid( CStructDecl ) :
                {
                    CStructDecl* psd = pObj;
                    if( psd->RefCount() == 0 )
                        break;
                    CImplSerialStruct oiss(
                        m_pWriter, pObj );
                    ret = oiss.Output();
                    break;
                }
            case clsid( CInterfaceDecl ):
                {
                    CAstNodeBase* pNode = pObj;
                    if( pNode == nullptr )
                    {
                        ret = -EFAULT;
                        break;
                    }

                    CInterfaceDecl* pifd = pObj;

                    if( pifd->RefCount() == 0 )
                        break;

                    CImplIufProxy oiufp(
                        m_pWriter, pObj );
                    oiufp.Output();

                    ObjPtr pmdlobj =
                        pifd->GetMethodList();

                    if( pmdlobj.IsEmpty() )
                    {
                        ret = -ENOENT;
                        break;
                    }

                    CMethodDecls* pmdl = pmdlobj;
                    for( guint32 i = 0;
                        i < pmdl->GetCount(); i++ )
                    {
                        ObjPtr pmd = pmdl->GetChild( i );
                        CImplIfMethodProxy oiimp(
                            m_pWriter, pmd );
                        ret = oiimp.Output();
                        if( ERROR( ret ) )
                            break;
                    }

                    CImplIufSvr oiufs(
                        m_pWriter, pObj );
                    oiufs.Output();

                    for( guint32 i = 0;
                        i < pmdl->GetCount(); i++ )
                    {
                        ObjPtr pmd = pmdl->GetChild( i );
                        CImplIfMethodSvr oiims(
                            m_pWriter, pmd );
                        ret = oiims.Output();
                        if( ERROR( ret ) )
                            break;
                    }

                    break;
                }
            case clsid( CServiceDecl ):
            case clsid( CTypedefDecl ):
            case clsid( CAppName ) :
            case clsid( CConstDecl ) :
                {
                    break;
                }
            default:
                {
                    ret = -ENOTSUP;
                    break;
                }
            }
            if( ERROR( ret ) )
                break;
        }

        if( ERROR( ret ) )
            break;

    }while( 0 );

    return ret;
}

gint32 GenCppProj(
    const std::string& strOutPath,
    const std::string& strAppName,
    ObjPtr pRoot )
{
    if( strOutPath.empty() ||
        strAppName.empty() ||
        pRoot.IsEmpty() )
        return -EINVAL;

    gint32 ret = 0;

    do{
        struct stat sb;
        if( lstat( strOutPath.c_str(), &sb ) == -1 )
        {
            std::string strCmd = "mkdir -p ";
            strCmd += strOutPath;
            ret = system( strCmd.c_str() );
            ret = 0;
        }
        else
        {
            mode_t iFlags =
                ( sb.st_mode & S_IFMT );
            if( iFlags != S_IFDIR )
            {
                std::string strMsg =
                    "error '";
                strMsg += strOutPath;
                strMsg +=
                   "' is not a valid directory";
                printf( "%s\n",
                    strMsg.c_str() );
                break;
            }
        }

        CCppWriter oWriter(
            strOutPath, strAppName, pRoot );

        SetStructRefs( pRoot );

        ret = GenHeaderFile( &oWriter, pRoot );
        if( ERROR( ret ) )
            break;

        ret = GenCppFile( &oWriter, pRoot );
        if( ERROR( ret ) )
            break;

        oWriter.SelectMakefile();
        CExportMakefile oemf( &oWriter, pRoot );
        ret = oemf.Output();
        if( ERROR( ret ) )
            break;

        oWriter.SelectDrvFile();
        CExportDrivers oedrv( &oWriter, pRoot );
        ret = oedrv.Output();

        oWriter.SelectDescFile();
        CExportObjDesc oedesc( &oWriter, pRoot );
        ret = oedesc.Output();

    }while( 0 );

    return ret;
}

gint32 CDeclareClassIds::Output()
{
    bool bFirst = true;
    gint32 ret = STATUS_SUCCESS;

    NEW_LINE;
    Wa( "enum EnumMyClsid" );
    BLOCK_OPEN;
    std::vector< ObjPtr > vecSvcs;
    ret = m_pStmts->GetSvcDecls( vecSvcs );
    if( ERROR( ret ) )
    {
        printf(
        "error no service declared\n" );
        return ret;
    }
    std::vector< std::string > vecNames;
    for( auto& elem : vecSvcs )
    {
        CServiceDecl* pSvc = elem;
        vecNames.push_back(
            pSvc->GetName() );
    }
        
    if( vecNames.size() > 1 )
    {
        std::sort( vecNames.begin(),
            vecNames.end() );
    }

    std::string strAppName =
        m_pStmts->GetName();

    std::string strVal = strAppName;
    for( auto& elem : vecNames )
        strVal += elem;

    guint32 dwClsid = GenClsid( strVal );
    for( auto& elem : vecSvcs )
    {
        CServiceDecl* pSvc = elem;
        if( pSvc == nullptr )
            continue;

        std::string strSvcName =
            pSvc->GetName();
        
        if( bFirst )
        {
            CCOUT << "DECL_CLSID( "
                << "C" << strSvcName
                << "_CliSkel"
                << " ) = "
                << dwClsid
                << ",";

            bFirst = false;
        }
        else
        {
            CCOUT << "DECL_CLSID( "
                << "C" << strSvcName
                << "_CliSkel" << " ),";
        }

        NEW_LINE;

        CCOUT << "DECL_CLSID( "
            << "C" << strSvcName
            << "_SvrSkel" << " ),";

        NEW_LINE;

        CCOUT << "DECL_CLSID( "
            << "C" << strSvcName
            << "_CliImpl" << " ),";

        NEW_LINE;

        CCOUT << "DECL_CLSID( "
            << "C" << strSvcName
            << "_SvrImpl" << " ),";

        NEW_LINE;
    }

    NEW_LINE;
    std::vector< ObjPtr > vecIfs;
    m_pStmts->GetIfDecls( vecIfs );
    for( auto& elem : vecIfs )
    {
        CInterfaceDecl* pifd = elem;
        if( pifd == nullptr )
            continue;

        if( pifd->RefCount() == 0 )
            continue;

        std::string strIfName =
            pifd->GetName();
        
        CCOUT << "DECL_IID( "
            << strIfName
            << " ),";
        NEW_LINE;
    }

    NEW_LINE;
    std::vector < ObjPtr > vecStructs;
    m_pStmts->GetStructDecls( vecStructs );
    for( auto& elem : vecStructs )
    {
        CStructDecl* psd = elem;
        if( psd->RefCount() == 0 )
            continue;
        std::string strName = psd->GetName();
        std::string strMsgId =
            strAppName + "::" + strName;
        guint32 dwMsgId = GenClsid( strMsgId );

        CCOUT << "DECL_CLSID( "
            << strName
            << " ) = "
            << dwMsgId << ",";
    }

    BLOCK_CLOSE;
    CCOUT << ";";
    NEW_LINES( 2 );

    return ret;
}

CDeclareTypedef::CDeclareTypedef(
    CCppWriter* pWriter, ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'typedef' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CDeclareTypedef::Output()
{
    ObjPtr pType = m_pNode->GetType();
    std::string strSig = GetTypeSig( pType );
    if( strSig.find( 'O' ) != std::string::npos )
    {
        gint32 ret = 0;
        std::set< ObjPtr > setStructs;
        if( strSig.size() == 1 &&
            strSig[ 0 ] == 'O' )
        {
            CStructRef* pRef = pType;
            if( pRef == nullptr )
                return -EFAULT;

            ret = pRef->GetStructType( pType );
            if( ERROR( ret ) )
                return ret;

            CStructDecl* psd = pType;
            if( psd == nullptr )
                return ret;

            if( psd->RefCount() == 0 )
                return ret;
        }
        else if( strSig[ 0 ] == '(' )
        {
            CSetStructRefs ossr( pType );
            ret = ossr.ExtractStructArr(
                pType, setStructs );
            if( ERROR( ret ) )
                return ret;

            guint32 dwRef = 0;
            for( auto& elem : setStructs )
            {
                CStructDecl* psd = elem;
                if( psd == nullptr )
                    continue;
                dwRef += psd->RefCount();
            }
            if( dwRef == 0 )
                return 0;
        }
        else if( strSig[ 0 ] == '[' )
        {
            std::set< ObjPtr > setStructs;
            CSetStructRefs ossr( pType );
            ret = ossr.ExtractStructMap(
                pType, setStructs );
            if( ERROR( ret ) )
                return ret;

            guint32 dwRef = 0;
            for( auto& elem : setStructs )
            {
                CStructDecl* psd = elem;
                if( psd == nullptr )
                    continue;
                dwRef += psd->RefCount();
            }
            if( dwRef == 0 )
                return 0;
        }
    }

    std::string strVal =
        m_pNode->ToStringCpp();
    if( strVal.empty() )
        return ERROR_FAIL;
    Wa( strVal );
    NEW_LINE;
    return STATUS_SUCCESS;
}

CDeclareStruct::CDeclareStruct(
    CCppWriter* pWriter,
    ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'struct' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CDeclareStruct::Output()
{
    gint32 ret = 0;
    do{
        std::string strName =
            m_pNode->GetName();

        AP( "struct " ); 
        CCOUT << strName;
        INDENT_UPL;
        CCOUT <<  ": public CStructBase";
        INDENT_DOWNL;
        BLOCK_OPEN;
    
        Wa( "typedef CStructBase super;" );
        NEW_LINE;

        ObjPtr pFields =
            m_pNode->GetFieldList();

        CFieldList* pfl = pFields;
        guint32 dwCount = pfl->GetCount();
        if( dwCount == 0 )
        {
            ret = -ENOENT;
            break;
        }

        ObjPtr pParent = m_pNode->GetParent();
        CStatements* pstmts = pParent;

        std::string strAppName =
            pstmts->GetName();

        std::string strMsgId =
            strAppName + "::" + strName;

        guint32 dwMsgId = GenClsid( strMsgId );

        Wa( "//Message identity" );
        CCOUT << "guint32 m_dwMsgId = clsid( "
            << strName << " );";
        NEW_LINE;

        CCOUT << "std::string m_strMsgId = "
            << "\"" << strMsgId << "\";";
        NEW_LINE;

        BufPtr pMsgId( true );
        *pMsgId = dwMsgId;
        m_pNode->SetProperty(
            PROP_MESSAGE_ID, pMsgId );

        Wa( "// data members" );
        guint32 i = 0;
        for( ; i < dwCount; i++ )
        {
            ObjPtr pObj = pfl->GetChild( i );
            CFieldDecl* pfd = pObj;
            if( pfd == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            std::string strLine =
                pfd->ToStringCpp();
            CCOUT<< strLine << ";"; 
            NEW_LINE;
        }

        NEW_LINE;
        Wa( "// Constructor" );
        CCOUT << strName << "() : super()";
        NEW_LINE;
        CCOUT << "{ SetClassId( ( EnumClsid )m_dwMsgId ); }";
        NEW_LINES( 2 );
        Wa( "// methods" );
        // declare two methods to implement
        CCOUT<< "gint32 Serialize(";
        INDENT_UPL;
        CCOUT << " BufPtr& pBuf_ ) const override;";
        INDENT_DOWNL;
        NEW_LINE;
        CCOUT << "gint32 Deserialize(";
        INDENT_UPL;
        CCOUT <<" BufPtr& pBuf_ ) override;"; 
        INDENT_DOWNL;
        NEW_LINE;
        Wa( "guint32 GetMsgId() const override" );
        Wa( "{ return m_dwMsgId; }" );
        NEW_LINE;
        CCOUT << "const std::string&";
        INDENT_UPL;
        CCOUT << "GetMsgName() const override";
        INDENT_DOWNL;
        Wa( "{ return m_strMsgId; }" );

        NEW_LINE;
        CCOUT << strName << "& " << "operator=(";
        INDENT_UPL;
        CCOUT << "const " << strName << "& rhs );";
        INDENT_DOWN;
        BLOCK_CLOSE;
        CCOUT << ";";
        NEW_LINES( 2 );

    }while( 0 );

    return ret;
}

CDeclInterfProxy::CDeclInterfProxy(
    CCppWriter* pWriter, ObjPtr& pNode )
    : super( pWriter )
{
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "interface declaration" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CDeclInterfProxy::Output()
{
    gint32 ret = 0;
    do{
        std::string strName =
            m_pNode->GetName();

        AP( "class " ); 
        std::string strClass = "I";
        strClass += strName + "_PImpl";
        CCOUT << strClass;
        std::string strBase;
        bool bStream = m_pNode->IsStream();
        if( bStream )
        {
            strBase =
            "CStreamProxyWrapper"; 
        }
        else
        {
            strBase =
            "CAggInterfaceProxy"; 
        }

        INDENT_UP;
        NEW_LINE;
        if( bStream )
            CCOUT<< ": public " << strBase;
        else
            CCOUT<< ": public virtual " << strBase;

        INDENT_DOWN;
        NEW_LINE;
        BLOCK_OPEN;

        Wa( "public:" );    
        CCOUT << "typedef "
            << strBase << " super;";
        NEW_LINE;

        CCOUT << strClass
            << "( const IConfigDb* pCfg ) :";

        INDENT_UP;
        NEW_LINE;
        if( m_pNode->IsStream() )
        {
            CCOUT << "super::super( pCfg ), "
                << "super( pCfg )";
            NEW_LINE;
            CCOUT << "{}";
        }
        else
        {
            CCOUT << "super( pCfg )";
            NEW_LINE;
            CCOUT << "{}";
        }
        INDENT_DOWN;
        NEW_LINE;
        Wa( "gint32 InitUserFuncs();" );
        NEW_LINE;
        Wa( "const EnumClsid GetIid() const override" );
        CCOUT << "{ return iid( "
            << strName << " ); }";
        NEW_LINES( 2 );

        ObjPtr pMethods =
            m_pNode->GetMethodList();

        CMethodDecls* pmds = pMethods;
        guint32 i = 0;
        pmds->GetCount();
        guint32 dwCount = pmds->GetCount();
        for( ; i < dwCount; i++ )
        {
            ObjPtr pObj = pmds->GetChild( i );
            CMethodDecl* pmd = pObj;
            if( pmd == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            if( pmd->IsEvent() )
            {
                ret = OutputEvent( pmd );
                if( ERROR( ret ) )
                    break;
            }
            else if( !pmd->IsAsyncp() )
            {
                ret = OutputSync( pmd );
                if( ERROR( ret ) )
                    break;
            }
            else 
            {
                ret = OutputAsync( pmd );
            }
            if( i + 1 < dwCount )
                NEW_LINE;
        }
        BLOCK_CLOSE;
        CCOUT << ";";
        NEW_LINES( 2 );

    }while( 0 );

    return ret;
}

gint32 CDeclInterfProxy::OutputEvent(
    CMethodDecl* pmd )
{
    if( pmd == nullptr )
        return -EINVAL;

    do{
        std::string strName, strArgs;
        strName = pmd->GetName();
        ObjPtr pArgs = pmd->GetInArgs();
        CCOUT<< "//RPC event handler '" << strName <<"'";
        NEW_LINE;
        Wa( "//TODO: implement me" );
        std::string strDecl =
            std::string( "virtual gint32 " ) +
            strName + "(";
        CCOUT << strDecl;
        guint32 dwCount = GetArgCount( pArgs );

        INDENT_UPL;
        if( dwCount > 0 )
        {
            GenFormInArgs( pArgs );
            CCOUT << " ) = 0;";
        }
        else
        {
            CCOUT << ") = 0;";
        }
        INDENT_DOWNL;

        pmd->SetAbstDecl( strDecl );
        pmd->SetAbstFlag( MF_IN_ARG | MF_IN_AS_IN );

        if( pmd->IsSerialize() && dwCount > 0 )
        { 
            NEW_LINE;
            // extra step the deserialize before
            // calling event handler.
            Wa( "//RPC event handler wrapper" );
            CCOUT << "gint32 " << strName <<"Wrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback, BufPtr& pBuf_ );";
            INDENT_DOWN;
        }
        else if( dwCount == 0 )
        {
            NEW_LINE;
            // extra step the deserialize before
            // calling event handler.
            Wa( "//RPC event handler wrapper" );
            CCOUT << "gint32 " << strName
                <<"Wrapper( IEventSink* pCallback );";
        }
        else if( !pmd->IsSerialize() )
        {
            NEW_LINE;
            // extra step the deserialize before
            // calling event handler.
            Wa( "//RPC event handler wrapper" );
            CCOUT << "gint32 " << strName
                <<"Wrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback, ";
            NEW_LINE;
            GenFormInArgs( pArgs );
            CCOUT << " );";
            INDENT_DOWN;
        }
        NEW_LINE;

    }while( 0 );

    return STATUS_SUCCESS;
}

gint32 CDeclInterfProxy::OutputAsync(
    CMethodDecl* pmd )
{
    if( pmd == nullptr )
        return -EINVAL;
    do{
        std::string strName, strArgs;
        strName = pmd->GetName();
        ObjPtr pInArgs = pmd->GetInArgs();
        ObjPtr pOutArgs = pmd->GetOutArgs();

        guint32 dwInCount = GetArgCount( pInArgs );
        guint32 dwOutCount = GetArgCount( pOutArgs );

        Wa( "//RPC Async Req Sender" );
        CCOUT << "gint32 " << strName << "( ";
        INDENT_UP;
        NEW_LINE;

        CCOUT << "IConfigDb* context";
        if( dwInCount > 0 )
        {
            CCOUT << ", ";
            NEW_LINE;
            GenFormInArgs( pInArgs );
            CCOUT << " ";
        }

        CCOUT << ");";
        INDENT_DOWN;

        if( pmd->IsSerialize() && dwInCount > 0 )
        {
            NEW_LINES( 2 );
            CCOUT << "gint32 " << strName
                << "Dummy( BufPtr& pBuf_ )";
            NEW_LINE;
            CCOUT << "{ return STATUS_SUCCESS; }";
        }

        NEW_LINE;

        NEW_LINE;
        Wa( "//Async callback wrapper" );
        CCOUT << "gint32 " << strName
            << "CbWrapper" << "( ";
        INDENT_UPL;

        Wa( "IEventSink* pCallback, " );
        Wa( "IEventSink* pIoReq," );
        CCOUT <<  "IConfigDb* pReqCtx );";

        INDENT_DOWNL;
        NEW_LINE;

        Wa( "//RPC Async Req Callback" );
        Wa( "//TODO: implement me by adding" );
        Wa( "//response processing code" );

        std::string strDecl;
        strDecl += "virtual gint32 " + strName
            + "Callback" + "(";
        CCOUT << strDecl;
        strDecl += "IConfigDb* context, gint32 iRet";

        if( dwOutCount > 0 )
        {
            INDENT_UPL;
            CCOUT << "IConfigDb* context, ";
            NEW_LINE;
            CCOUT << "gint32 iRet,";
            strDecl += ",";
            GenFormInArgs( pOutArgs );
            CCOUT << " ) = 0;";
            INDENT_DOWN;
        }
        else
        {
            INDENT_UPL;
            CCOUT << "IConfigDb* context, ";
            NEW_LINE;
            CCOUT << "gint32 iRet ) = 0;";
            INDENT_DOWN;
        }

        pmd->SetAbstDecl( strDecl );
        pmd->SetAbstFlag(
            MF_OUT_ARG | MF_OUT_AS_IN );

        NEW_LINE;

    }while( 0 );

    return STATUS_SUCCESS;
}

gint32 CDeclInterfProxy::OutputSync(
    CMethodDecl* pmd )
{
    if( pmd == nullptr )
        return -EINVAL;
    do{
        std::string strName, strArgs;
        strName = pmd->GetName();
        ObjPtr pInArgs = pmd->GetInArgs();
        ObjPtr pOutArgs = pmd->GetOutArgs();
        guint32 dwInCount =
            GetArgCount( pInArgs );

        guint32 dwOutCount =
            GetArgCount( pOutArgs );

        guint32 dwCount =
            dwInCount + dwOutCount;

        Wa( "//RPC Sync Req Sender" );
        CCOUT << "gint32 " << strName << "(";

        bool bComma = false;
        if( dwCount == 0 )
        {
            CCOUT << ");";
            NEW_LINE;
        }
        else
        {
            INDENT_UPL;
            if( dwInCount > 0 )
            {
                GenFormInArgs( pInArgs );
                bComma = true;
            }
            if( dwOutCount > 0 )
            {
                if( bComma )
                {
                    CCOUT << ",";
                    NEW_LINE;
                }

                GenFormOutArgs( pOutArgs );
            }

            CCOUT << " );";
            INDENT_DOWNL;
        }

        if( pmd->IsSerialize() && dwInCount > 0 )
        {
            NEW_LINE;
            CCOUT << "gint32 " << strName
                << "Dummy( BufPtr& pBuf_ )";
            NEW_LINE;
            Wa( "{ return STATUS_SUCCESS; }" );
        }

    }while( 0 );

    return STATUS_SUCCESS;
}

CDeclInterfSvr::CDeclInterfSvr(
    CCppWriter* pWriter, ObjPtr& pNode )
    : super( pWriter )
{
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "interface declaration" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CDeclInterfSvr::Output()
{
    gint32 ret = 0;
    do{
        std::string strName =
            m_pNode->GetName();

        AP( "class " ); 
        std::string strClass = "I";
        strClass += strName + "_SImpl";
        CCOUT << strClass;
        std::string strBase;
        bool bStream = m_pNode->IsStream();
        if( bStream )
        {
            strBase = "CStreamServerWrapper"; 
        }
        else
        {
            strBase = "CAggInterfaceServer"; 
        }

        INDENT_UP;
        NEW_LINE;
        if( bStream )
            CCOUT<< ": public " << strBase;
        else
            CCOUT<< ": public virtual " << strBase;

        INDENT_DOWN;
        NEW_LINE;
        BLOCK_OPEN;

        Wa( "public:" );    
        CCOUT << "typedef "
            << strBase << " super;";
        NEW_LINE;
        CCOUT << strClass
            << "( const IConfigDb* pCfg ) :";
        INDENT_UP;
        NEW_LINE;
        if( m_pNode->IsStream() )
        {
            CCOUT << "super::super( pCfg ), "
                << "super( pCfg )";
            NEW_LINE;
            CCOUT << "{}";
        }
        else
        {
            CCOUT << "super( pCfg )";
            NEW_LINE;
            CCOUT << "{}";
        }
        INDENT_DOWN;
        NEW_LINE;
        Wa( "gint32 InitUserFuncs();" );
        NEW_LINE;
        Wa( "const EnumClsid GetIid() const override" );
        CCOUT<< "{ return iid( "
            << strName <<" ); }";
        NEW_LINES( 2 );

        ObjPtr pMethods =
            m_pNode->GetMethodList();

        CMethodDecls* pmds = pMethods;
        guint32 i = 0;
        pmds->GetCount();
        guint32 dwCount = pmds->GetCount();
        for( ; i < dwCount; i++ )
        {
            ObjPtr pObj = pmds->GetChild( i );
            CMethodDecl* pmd = pObj;
            if( pmd == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            if( pmd->IsEvent() )
            {
                ret = OutputEvent( pmd );
                if( ERROR( ret ) )
                    break;
            }
            else if( !pmd->IsAsyncs() )
            {
                ret = OutputSync( pmd );
                if( ERROR( ret ) )
                    break;
            }
            else 
            {
                ret = OutputAsync( pmd );
            }
            if( i + 1 < dwCount )
                NEW_LINE;
        }
        BLOCK_CLOSE;
        CCOUT << ";";
        NEW_LINES( 2 );

    }while( 0 );

    return ret;
}

gint32 CDeclInterfSvr::OutputEvent(
    CMethodDecl* pmd )
{
    if( pmd == nullptr )
        return -EINVAL;

    do{
        std::string strName, strArgs;
        strName = pmd->GetName();
        ObjPtr pArgs = pmd->GetInArgs();
        Wa( "//RPC event sender" );
        CCOUT << "gint32 " << strName << "(";
        guint32 dwInCount = GetArgCount( pArgs );
        if( dwInCount > 0 )
        {
            INDENT_UPL;
            GenFormInArgs( pArgs );
            CCOUT << " );";
            INDENT_DOWN;
        }
        else
        {
            CCOUT << " );";
        }

        NEW_LINE;

    }while( 0 );

    return STATUS_SUCCESS;
}

gint32 CDeclInterfSvr::OutputSync(
    CMethodDecl* pmd )
{
    if( pmd == nullptr )
        return -EINVAL;
    do{
        std::string strName, strArgs;
        strName = pmd->GetName();

        ObjPtr pInArgs = pmd->GetInArgs();
        ObjPtr pOutArgs = pmd->GetOutArgs();

        guint32 dwInCount =
            GetArgCount( pInArgs );

        guint32 dwOutCount =
            GetArgCount( pOutArgs );

        guint32 dwCount = dwInCount + dwOutCount;

        Wa( "//RPC Sync Req Handler Wrapper" );
        CCOUT << "gint32 " << strName << "Wrapper(";
        INDENT_UPL;
        CCOUT << "IEventSink* pCallback";

        if( pmd->IsSerialize() &&
            dwInCount > 0 )
        {
            CCOUT << ", BufPtr& pBuf_ );";
        }
        else if( dwInCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pInArgs );
            CCOUT << " );"; 
        }
        else
        {
            CCOUT << " );"; 
        }
        INDENT_DOWNL;
        NEW_LINE;

        Wa( "//RPC Sync Req Handler" );
        Wa( "//TODO: implement me" );
        std::string strDecl =
            std::string( "virtual gint32 " )
            + strName + "(";
        CCOUT << strDecl;
        if( dwCount == 0 )
        {
            CCOUT << ") = 0;";
        }
        else
        {
            INDENT_UPL;
            bool bComma = false;
            if( dwInCount > 0 )
            {
                bComma = true;
                GenFormInArgs( pInArgs );
            }
            if( dwOutCount > 0 )
            {
                if( bComma )
                {
                    CCOUT << ",";
                    NEW_LINE;
                }
                GenFormOutArgs( pOutArgs );
            }
            CCOUT << " ) = 0;";
            INDENT_DOWN;
        }
        NEW_LINE;

        pmd->SetAbstDecl( strDecl, false );
        guint32 dwFlags = 0;
        if( dwInCount > 0 )
            dwFlags |= MF_IN_ARG | MF_IN_AS_IN;
        if( dwOutCount > 0 )
            dwFlags |= MF_OUT_ARG | MF_OUT_AS_OUT;
        pmd->SetAbstFlag( dwFlags, false );

    }while( 0 );

    return STATUS_SUCCESS;
}

gint32 CDeclInterfSvr::OutputAsync(
    CMethodDecl* pmd )
{
    if( pmd == nullptr )
        return -EINVAL;
    do{
        std::string strName, strArgs;
        strName = pmd->GetName();
        ObjPtr pInArgs = pmd->GetInArgs();
        ObjPtr pOutArgs = pmd->GetOutArgs();

        guint32 dwInCount =
            GetArgCount( pInArgs );

        guint32 dwOutCount =
            GetArgCount( pOutArgs );

        guint32 dwCount = dwInCount + dwOutCount;
        if( pmd->IsSerialize() )
        {
            Wa( "//RPC Async Req Handler wrapper" );
            CCOUT << "gint32 "
                << strName << "Wrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback"
                << ", BufPtr& pBuf_ );";
            INDENT_DOWNL;
            NEW_LINE;

            CCOUT << "gint32 "
                << strName << "CancelWrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback,";
            NEW_LINE;
            CCOUT << "gint32 iRet, "
                << "BufPtr& pBuf_ );";
            INDENT_DOWNL;
            NEW_LINE;
        }
        else if( dwInCount == 0 )
        {
            Wa( "//RPC Async Req Handler wrapper" );
            CCOUT << "gint32 "
                << strName << "Wrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback );";
            INDENT_DOWNL;
            NEW_LINE;

            CCOUT << "gint32 "
                << strName << "CancelWrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback,";
            NEW_LINE;
            CCOUT << "gint32 iRet );";
            INDENT_DOWNL;
            NEW_LINE;
        }
        else
        {
            Wa( "//RPC Async Req Handler wrapper" );
            CCOUT << "gint32 "
                << strName << "Wrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback,";
            NEW_LINE;
            GenFormInArgs( pInArgs );
            CCOUT << " );";
            INDENT_DOWNL;
            NEW_LINE;

            CCOUT << "gint32 "
                << strName << "CancelWrapper(";
            INDENT_UPL;
            CCOUT << "IEventSink* pCallback,";
            NEW_LINE;
            CCOUT << "gint32 iRet,";
            NEW_LINE;
            GenFormInArgs( pInArgs );
            CCOUT << " );";
            INDENT_DOWNL;
            NEW_LINE;
        }

        Wa( "//RPC Async Req Cancel Handler" );
        CCOUT << "virtual gint32 "
            << "On" << strName << "Canceled(";
        INDENT_UPL;
        CCOUT << "gint32 iRet";
        if( dwInCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pInArgs );
        }
        CCOUT << " ) = 0;";
        INDENT_DOWNL;
        NEW_LINE;

        Wa( "//RPC Async Req Callback" );
        Wa( "//Call this method when you have" );
        Wa( "//finished the async operation" );
        Wa( "//with all the return value set" );
        Wa( "//or an error code" );

        CCOUT << "virtual gint32 " << strName
            << "Complete" << "( ";

        INDENT_UPL;
        CCOUT << "IEventSink* pCallback, "
            << "gint32 iRet";

        if( dwOutCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pOutArgs );
        }
        CCOUT << " );";
        INDENT_DOWNL;
        NEW_LINE;

        Wa( "//RPC Async Req Handler" );
        Wa( "//TODO: adding code to emit your async" );
        Wa( "//operation, keep a copy of pCallback and" );
        Wa( "//return STATUS_PENDING" );
        std::string strDecl =
            std::string( "virtual gint32 " ) +
            strName + "(";

        CCOUT << strDecl;
        strDecl += "IEventSink* pCallback";

        INDENT_UPL;
        CCOUT << "IEventSink* pCallback";
        if( dwCount > 0 )
        {
            strDecl += ",";
            CCOUT << ",";
        }

        if( dwInCount > 0 )
        {
            NEW_LINE;
            GenFormInArgs( pInArgs );
        }
        if( dwOutCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormOutArgs( pOutArgs );
        }
        CCOUT << " ) = 0;";
        INDENT_DOWNL;

        pmd->SetAbstDecl( strDecl, false );

        guint32 dwFlags = 0;
        if( dwInCount > 0 )
            dwFlags = MF_IN_AS_IN | MF_IN_ARG;
        if( dwOutCount > 0 )
            dwFlags |= MF_OUT_ARG | MF_OUT_AS_OUT;
        pmd->SetAbstFlag( dwFlags, false );

    }while( 0 );

    return STATUS_SUCCESS;
}

CDeclService::CDeclService(
    CCppWriter* pWriter,
    ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'service' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CDeclService::Output()
{
    guint32 ret = STATUS_SUCCESS;
    do{
        std::string strSvcName =
            m_pNode->GetName();
        CCOUT << "DECLARE_AGGREGATED_PROXY(";
        INDENT_UP;
        NEW_LINE;

        std::vector< ObjPtr > vecIfs;
        m_pNode->GetIfRefs( vecIfs );
        if( vecIfs.empty() )
        {
            ret = -ENOENT;
            break;
        }

        CCOUT << "C" << strSvcName << "_CliSkel,";
        NEW_LINE;
        if( m_pNode->IsStream() )
            Wa( "CStreamProxyAsync," );
        for( guint32 i = 0;
            i < vecIfs.size(); i++ )
        {
            CInterfRef* pifr = vecIfs[ i ];
            std::string strName = pifr->GetName();
            CCOUT << "I" << strName << "_PImpl";
            if( i + 1 < vecIfs.size() )
            {
                CCOUT << ",";
                NEW_LINE;
            }
        }
        CCOUT << " );";
        INDENT_DOWN;
        NEW_LINES( 2 );

        CCOUT << "DECLARE_AGGREGATED_SERVER(";
        INDENT_UP;
        NEW_LINE;
        CCOUT << "C" << strSvcName << "_SvrSkel,";
        NEW_LINE;
        if( m_pNode->IsStream() )
            Wa( "CStreamServerAsync," );
        for( guint32 i = 0;
            i < vecIfs.size(); i++ )
        {
            CInterfRef* pifr = vecIfs[ i ];
            std::string strName = pifr->GetName();
            CCOUT << "I" << strName << "_SImpl";
            if( i + 1 < vecIfs.size() )
            {
                CCOUT << ",";
                NEW_LINE;
            }
        }
        CCOUT << " );";
        INDENT_DOWN;
        NEW_LINES( 2 );

    }while( 0 );

    return ret;
}

gint32 CSetStructRefs::ExtractStructArr(
    ObjPtr& pArray,
    std::set< ObjPtr >& setStructs )
{
    if( pArray.IsEmpty() )
        return 0;

    gint32 ret = 0;
    do{
        CStructRef* psr = pArray;
        if( psr != nullptr )
        {
            // this is an alias
            std::string strName = psr->GetName();
            ret = g_mapAliases.GetAliasType(
                strName, pArray );
            if( ERROR( ret ) )
                break;
        }
        CArrayType* pat = pArray;
        if( pat == nullptr )
            break;

        ObjPtr pType = pat->GetElemType();
        CArrayType* pet;
        pet = pType;

        if( pet != nullptr )
        {
            ret = ExtractStructArr(
                pType, setStructs );
            break;
        }
        CMapType* pmt = pType;
        if( pmt != nullptr )
        {
            ret = ExtractStructMap(
                pType, setStructs );
            break;
        }

        CStructRef* pRef = pType;
        if( pRef == nullptr )
            break;

        std::string strName = pRef->GetName();

        ObjPtr pStruct;
        ret = g_mapDecls.GetDeclNode(
            strName, pStruct );
        if( ERROR( ret ) )
        {
            DebugPrintEx( logErr, ret,
                "error, %s not defined\n",
                strName.c_str() );
            break;
        }
        setStructs.insert( pStruct );

    }while( 0 );

    return ret;
}

gint32 CSetStructRefs::ExtractStructMap(
    ObjPtr& pMap,
    std::set< ObjPtr >& setStructs )
{
    if( pMap.IsEmpty() )
        return 0;

    gint32 ret = 0;
    do{
        CStructRef* psr = pMap;
        if( psr != nullptr )
        {
            // this is an alias
            std::string strName = psr->GetName();
            ret = g_mapAliases.GetAliasType(
                strName, pMap );
            if( ERROR( ret ) )
                break;
        }
        CMapType* pmt = pMap;
        if( pmt == nullptr )
            break;

        ObjPtr pType = pmt->GetElemType();
        guint32 iCount = 0;
        do{
            CMapType* pet = pType;
            CArrayType* pat = pType;
            if( pet != nullptr )
            {
                ret = ExtractStructMap(
                    pType, setStructs );
                if( ERROR( ret ) )
                    break;
            }
            else if( pat != nullptr )
            {
                ret = ExtractStructArr(
                    pType, setStructs );
                if( ERROR( ret ) )
                    break;
            }
            else
            {
                CStructRef* pRef = pType;
                if( pRef != nullptr ){
                std::string strName =
                    pRef->GetName();

                ObjPtr pStruct;
                ret = g_mapDecls.GetDeclNode(
                    strName, pStruct );
                if( ERROR( ret ) )
                {
                    DebugPrintEx( logErr, ret,
                        "error, %s not defined\n",
                        strName.c_str() );
                    break;
                }
                CStructDecl* psd = pStruct;
                if( psd != nullptr )
                    setStructs.insert( pStruct );
                }
            }

            pType = pmt->GetKeyType();
            ++iCount;

        }while( iCount < 2 );

    }while( 0 );

    return ret;
}

gint32 CSetStructRefs::SetStructRefs()
{
    guint32 ret = STATUS_SUCCESS;
    do{
        std::vector< ObjPtr > vecIfs;
        m_pNode->GetIfRefs( vecIfs );
        if( vecIfs.empty() )
            break;

        
        std::vector< ObjPtr > vecIfds;
        for( auto& elem : vecIfs )
        {
            ObjPtr pObj;
            CInterfRef* pifr = elem;
            std::string strName = pifr->GetName();
            ret = pifr->GetIfDecl( pObj );
            if( ERROR( ret ) )
            {
                ret = 0;
                continue;
            }
            vecIfds.push_back( pObj );
        }

        std::vector< ObjPtr > vecMethods;
        for( auto& elem : vecIfds )
        {
            CInterfaceDecl* pifd = elem;
            ObjPtr pObj = pifd->GetMethodList();
            CMethodDecls* pmds = pObj;
            guint32 i = 0;
            for( ; i < pmds->GetCount(); i++ )
            {
                ObjPtr pObj = pmds->GetChild( i );
                vecMethods.push_back( pObj );
            }
        }

        std::set< ObjPtr > setTypes;
        for( auto& elem : vecMethods )
        {
            CMethodDecl* pmd = elem;
            ObjPtr pInArgs = pmd->GetInArgs();
            ObjPtr pOutArgs = pmd->GetOutArgs();

            GetArgTypes( pInArgs, setTypes );
            GetArgTypes( pOutArgs, setTypes );
        }

        for( auto elem : setTypes )
        {
            std::string strSig =
                GetTypeSig( elem );

            if( strSig.find( 'O' ) ==
                std::string::npos )
                continue;

            std::set< ObjPtr > setStructs;
            if( strSig.size() == 1 )
            {
                CStructRef* pRef = elem;
                if( pRef == nullptr )
                    continue;

                std::string strName =
                    pRef->GetName();

                ObjPtr pStruct;
                ret = g_mapDecls.GetDeclNode(
                    strName, pStruct );
                if( ERROR( ret ) )
                {
                    DebugPrintEx( logErr, ret,
                        "error, %s not defined\n",
                        strName.c_str() );
                    continue;
                }
                CStructDecl* psd = pStruct;
                if( psd == nullptr )
                    continue;

                psd->AddRef();
                continue;
            }
            else if( strSig[ 0 ] == '(' )
            {
                ret = ExtractStructArr(
                    elem, setStructs );                
            }
            else if( strSig[ 0 ] == '[' )
            {
                ret = ExtractStructMap(
                    elem, setStructs );                
            }

            for( auto& elem : setStructs )
            {
                CStructDecl* psd = elem;
                if( psd == nullptr )
                    continue;
                psd->AddRef();
            }
        }

    }while( 0 );

    return ret;
}

CDeclServiceImpl::CDeclServiceImpl(
    CCppWriter* pWriter,
    ObjPtr& pNode, bool bServer )
    : super( pWriter )
{
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'service' node" );
        throw std::runtime_error( strMsg );
    }
    m_bServer = bServer;
}

gint32 CDeclServiceImpl::FindAbstMethod(
    std::vector< ABSTE >& vecMethods,
    bool bProxy )
{
    gint32 ret = 0;
    do{
        std::vector< ObjPtr > vecIfs;
        m_pNode->GetIfRefs( vecIfs );
        if( vecIfs.empty() )
        {
            ret = -ENOENT;
            break;
        }

        for( auto& elem : vecIfs )
        {
            CInterfRef* pifr = elem;
            ObjPtr pifdo;
            ret = pifr->GetIfDecl( pifdo );
            if( ERROR( ret ) )
                break;

            CInterfaceDecl* pifd = pifdo;
            ObjPtr pObj = pifd->GetMethodList();
            CMethodDecls* pmds = pObj;
            guint32 i = 0;

            std::string strComment = "// ";
            strComment += pifd->GetName();
            vecMethods.push_back(
                {strComment, pifdo } );

            for( ; i < pmds->GetCount(); i++ )
            {
                ObjPtr p = pmds->GetChild( i );
                CMethodDecl* pmd = p;
                if( pmd == nullptr )
                {
                    ret = -EFAULT;
                    break;
                }
                std::string strDecl =
                    pmd->GetAbstDecl( bProxy );
                if( strDecl.empty() )
                    continue;

                vecMethods.push_back(
                    { strDecl, p } );
            }
            if( ERROR( ret ) )
                break;
        }

    }while( 0 );

    return ret;
}

gint32 CDeclServiceImpl::DeclAbstMethod(
    ABSTE oMethod, bool bProxy, bool bComma )
{
    CMethodDecl* pmd = oMethod.second;    
    if( pmd == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        ObjPtr pInArgs = pmd->GetInArgs();
        ObjPtr pOutArgs = pmd->GetOutArgs();

        guint32 dwInCount =
            GetArgCount( pInArgs );

        guint32 dwOutCount =
            GetArgCount( pOutArgs );

        guint32 dwCount = 0;

        guint32 dwFlags = 0;
        pmd->GetAbstFlag( dwFlags, bProxy );

        bool bInArg = false, bOutArg = false;

        if( dwFlags & MF_IN_ARG )
            bInArg = true;

        if( dwFlags & MF_OUT_ARG )
            bOutArg = true;

        bool bInAsIn = false, bOutAsIn = false;
        if( dwFlags & MF_IN_AS_IN )
            bInAsIn = true;

        if( dwFlags & MF_OUT_AS_IN )
            bOutAsIn = true;

        if( bInArg )
            dwCount = dwInCount;

        if( bOutArg )
            dwCount += dwOutCount;

        std::string strRet = oMethod.first;
        if( !bComma )
        {
            char szHeader[] =
                "virtual gint32 ";
            strRet = strRet.substr(
                sizeof( szHeader ) - 1 );
        }

        if( dwCount == 0 )
        {
            if( strRet.back() == '(' )
            {
                CCOUT << strRet;
                CCOUT << ")";
                if( bComma )
                    CCOUT << ";";
                NEW_LINE;
                break;
            }
            // there are some fixed arguments
            // to indent
        }

        size_t pos = strRet.find_first_of( '(' );
        bool bAppend = false;
        if( pos != std::string::npos )
        {
            pos += 1;
            INDENT_UP;
            std::string strNewLine =
                m_pWriter->NewLineStr();
            INDENT_DOWN;
            if( pos >= strRet.size() )
            {
                strRet.append( strNewLine );
                bAppend = true;
            }
            else
                strRet.insert( pos, strNewLine );
        }
        else
        {
            printf( "error '%s' missing left paren\n",
                oMethod.first.c_str() );
            ret = -EINVAL;
            break;
        }

        CCOUT << strRet;
        if( dwCount == 0 )
        {
            CCOUT << " )";
            if( bComma )
                CCOUT << ";";
            NEW_LINE;
            break;
        }

        if( !bAppend )
        {
            INDENT_UPL;
        }
        else
        {
            INDENT_UP;
        }
        if( bInArg && bInAsIn ) 
        {
            GenFormInArgs( pInArgs, true );
        }
        else if( bInArg )
        {
            GenFormOutArgs( pInArgs, true );
        }

        if( dwInCount > 0 && dwOutCount > 0 &&
            bInArg && bOutArg )
        {
            CCOUT << ",";
            NEW_LINE;
        }

        if( bOutArg && bOutAsIn )
        {
            GenFormInArgs( pOutArgs, true );
        }
        else if( bOutArg )
        {
            GenFormOutArgs( pOutArgs, true );
        }
        CCOUT << " )";
        if( bComma )
            CCOUT << ";";
        INDENT_DOWNL;

    }while( 0 );

    return ret;
}

gint32 CDeclServiceImpl::Output()
{
    guint32 ret = STATUS_SUCCESS;
    do{
        std::string strSvcName =
            m_pNode->GetName();

        std::vector< ABSTE > vecPMethods;
        ret = FindAbstMethod( vecPMethods, true );
        if( ERROR( ret ) )
            break;

        std::vector< ABSTE > vecSMethods;
        ret = FindAbstMethod( vecSMethods, false );
        if( ERROR( ret ) )
            break;
        
        if( vecSMethods.empty() &&
            vecPMethods.empty() )
            break;

        Wa( "//Generated by ridlc" );
        Wa( "//Your task is to implement the following classes" );
        Wa( "//to get your rpc server work" );
        Wa( "#pragma once" );
        CAstNodeBase* pParent =
            m_pNode->GetParent();
        if( pParent == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        CStatements* pStmts = ObjPtr( pParent );
        if( pStmts == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        std::string strAppName = pStmts->GetName();
        CCOUT << "#include \"" << strAppName << ".h\"" ;
        NEW_LINES( 2 );

        std::string strClass, strBase;
        if( !vecPMethods.empty() && !IsServer() )
        {
            strClass = "C";
            strClass += strSvcName + "_CliImpl";
            strBase = "C";
            strBase += strSvcName + "_CliSkel";
            CCOUT << "class " << strClass;

            INDENT_UP;
            NEW_LINE;

            CCOUT << ": public " << strBase;

            INDENT_DOWN;
            NEW_LINE;
            BLOCK_OPEN;

            Wa( "public:" );    
            CCOUT << "typedef "
                << strBase << " super;";
            NEW_LINE;

            CCOUT << strClass
                << "( const IConfigDb* pCfg ) :";
            INDENT_UP;
            NEW_LINE;
            CCOUT << "super::virtbase( pCfg ), "
                << "super( pCfg )";
            INDENT_DOWN;
            NEW_LINE;
            CCOUT << "{}";
            NEW_LINES( 2 );

            if( m_pNode->IsStream() )
            {
                Wa( "/* The following two methods are important for */" );
                Wa( "/* streaming transfer. rewrite them if necessary */" );
                Wa( "gint32 OnStreamReady( HANDLE hChannel ) override" );
                Wa( "{ return super::OnStreamReady( hChannel ); } " );
                NEW_LINE;
                Wa( "gint32 OnStmClosing( HANDLE hChannel ) override" );
                Wa( "{ return super::OnStmClosing( hChannel ); }" );
            }

            for( guint32 i = 0;
                i < vecPMethods.size(); i++ )
            {
                ABSTE& elem = vecPMethods[ i ];
                if( elem.first[ 0 ] == '/' &&
                    elem.first[ 1 ] == '/' )
                {
                    CCOUT << elem.first;
                    NEW_LINE;
                    continue;
                }
                DeclAbstMethod( elem, true );
                if( i + 1 < vecPMethods.size() )
                    NEW_LINE;
            }

            BLOCK_CLOSE;
            CCOUT << ";";
            NEW_LINES( 2 );
        }

        if( vecSMethods.empty() || !IsServer() )
            break;

        strClass = "C";
        strClass += strSvcName + "_SvrImpl";
        strBase = "C";
        strBase += strSvcName + "_SvrSkel";

        CCOUT << "class " << strClass;

        INDENT_UP;
        NEW_LINE;

        CCOUT << ": public " << strBase;

        INDENT_DOWN;
        NEW_LINE;
        BLOCK_OPEN;

        Wa( "public:" );    
        CCOUT << "typedef "
            << strBase << " super;";
        NEW_LINE;

        CCOUT << strClass
            << "( const IConfigDb* pCfg ) :";
        INDENT_UP;
        NEW_LINE;
        CCOUT << "super::virtbase( pCfg ), "
            << "super( pCfg )";
        INDENT_DOWN;
        NEW_LINE;
        CCOUT << "{}";
        NEW_LINES( 2 );

        if( m_pNode->IsStream() )
        {
            Wa( "/* The following two methods are important for */" );
            Wa( "/* streaming transfer. rewrite them if necessary */" );
            Wa( "gint32 OnStreamReady( HANDLE hChannel ) override" );
            Wa( "{ return super::OnStreamReady( hChannel ); } " );
            NEW_LINE;
            Wa( "gint32 OnStmClosing( HANDLE hChannel ) override" );
            Wa( "{ return super::OnStmClosing( hChannel ); }" );
            NEW_LINE;
        }

        for( guint32 i = 0;
            i < vecSMethods.size(); i++ )
        {
            ABSTE& elem = vecSMethods[ i ];
            if( elem.first[ 0 ] == '/' &&
                elem.first[ 1 ] == '/' )
            {
                CCOUT << elem.first;
                NEW_LINE;
                continue;
            }
            DeclAbstMethod( elem, false );

            CMethodDecl* pmd = elem.second;
            if( !pmd->IsAsyncs() )
            {
                if( i + 1 < vecSMethods.size() )
                    NEW_LINE;
                continue;
            }
            NEW_LINE;
            std::string strName = pmd->GetName();
            ObjPtr pInArgs = pmd->GetInArgs();
            guint32 dwInCount =
                GetArgCount( pInArgs );
            Wa( "// RPC Async Req Cancel Handler" );
            Wa( "// Rewrite this method to release the resources" );
            CCOUT << "gint32 "
                << "On" << strName << "Canceled(";
            INDENT_UPL;
            CCOUT << "gint32 iRet";
            if( dwInCount > 0 )
            {
                CCOUT << ",";
                NEW_LINE;
                GenFormInArgs( pInArgs, true );
            }
            CCOUT << " )";
            INDENT_DOWNL;
            BLOCK_OPEN;
            CCOUT << "DebugPrintEx( logErr, iRet,";
            INDENT_UPL;
            CCOUT << "\"request '" << strName
                << "' is canceled.\" );";
            INDENT_DOWNL;
            CCOUT << "return 0;";
            BLOCK_CLOSE;

            if( i + 1 < vecSMethods.size() )
                NEW_LINE;
        }
        BLOCK_CLOSE;
        CCOUT << ";";
        NEW_LINES( 2 );

    }while( 0 );

    return ret;
}

gint32 CImplServiceImpl::Output()
{
    guint32 ret = STATUS_SUCCESS;
    do{
        std::string strSvcName =
            m_pNode->GetName();

        std::vector< ABSTE > vecPMethods;
        ret = FindAbstMethod( vecPMethods, true );
        if( ERROR( ret ) )
            break;

        std::vector< ABSTE > vecSMethods;
        ret = FindAbstMethod( vecSMethods, false );
        if( ERROR( ret ) )
            break;
        
        if( vecSMethods.empty() &&
            vecPMethods.empty() )
            break;

        Wa( "/****BACKUP YOUR CODE BEFORE RUNNING RIDLC***/" );
        Wa( "// Implement the following methods" );
        Wa( "// to get the RPC proxy/server work" );
        CAstNodeBase* pParent =
            m_pNode->GetParent();
        if( pParent == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        CStatements* pStmts = ObjPtr( pParent );
        if( pStmts == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        std::string strAppName = pStmts->GetName();
        Wa( "#include \"rpc.h\"" );
        Wa( "using namespace rpcf;" );
        CCOUT << "#include \""
            << strAppName << ".h\"" ;
        NEW_LINE;
        if( IsServer() )
            CCOUT << "#include \""
                << strSvcName << "svr.h\"";
        else
            CCOUT << "#include \""
                << strSvcName << "cli.h\"";

        NEW_LINES( 2 );

        std::string strClass, strBase;
        if( !vecPMethods.empty() && !IsServer() )
        {
            strClass = "C";
            strClass += strSvcName + "_CliImpl";

            for( guint32 i = 0;
                i < vecPMethods.size(); i++ )
            {
                ABSTE& elem = vecPMethods[ i ];

                if( elem.first[ 0 ] == '/' &&
                    elem.first[ 1 ] == '/' )
                {
                    if( vecPMethods.size() == 1 )
                        continue;
                    CCOUT << elem.first
                        << " Proxy";
                    NEW_LINE;
                    continue;
                }

                CMethodDecl* pmd = elem.second;
                bool bAsync = pmd->IsAsyncp();
                bool bEvent = pmd->IsEvent();

                if( bAsync )
                    Wa( "/* Async Req */" );
                else if( bEvent )
                    Wa( "/* Event */" );
                else
                    Wa( "/* Sync Req */" );

                CCOUT << "gint32 " << strClass << "::";
                DeclAbstMethod(
                    elem, true, false );
                BLOCK_OPEN;
                if( bEvent )
                {
                    Wa( "// TODO: Processing the event here" );
                    Wa( "// return code ignored");
                    CCOUT << "return 0;";
                }
                else if( bAsync )
                {
                    if( pmd->IsNoReply() )
                    {
                        Wa( "// TODO: the request is sent or failed" );
                        Wa( "// return code ignored");
                    }
                    else
                    {
                        Wa( "// TODO: Process the server response here" );
                        Wa( "// return code ignored");
                    }
                    CCOUT<< "return 0;";
                }
                BLOCK_CLOSE;
                NEW_LINES( 2 );
            }

            NEW_LINE;
        }

        if( vecSMethods.empty() || !IsServer() )
            break;

        strClass = "C";
        strClass += strSvcName + "_SvrImpl";

        for( guint32 i = 0;
            i < vecSMethods.size(); i++ )
        {
            ABSTE& elem = vecSMethods[ i ];
            if( elem.first[ 0 ] == '/' &&
                elem.first[ 1 ] == '/' )
            {
                if( vecSMethods.size() == 1 )
                    continue;
                CCOUT << elem.first
                    << " Server";
                NEW_LINE;
                continue;
            }

            CMethodDecl* pmd = elem.second;
            bool bAsync = pmd->IsAsyncs();
            bool bEvent = pmd->IsEvent();

            if( bAsync )
                Wa( "/* Async Req */" );
            else if( bEvent )
                Wa( "/* Event */" );
            else
                Wa( "/* Sync Req */" );

            CCOUT << "gint32 " << strClass << "::";
            DeclAbstMethod( elem, false, false );
            BLOCK_OPEN;
            if( bAsync )
            {
                Wa( "// TODO: Emitting an async operation here." );
                std::string strName =
                    pmd->GetName();
                CCOUT << "// And make sure to call "
                    << strName << "Complete";
                NEW_LINE;
                CCOUT << "// when the service is done";
                NEW_LINE;    
                CCOUT << "return STATUS_PENDING;";
            }
            else if( !bEvent )
            {
                // sync method
                Wa( "// TODO: Process the sync request here " );
                Wa( "// return code can be an Error or" );
                Wa( "// STATUS_SUCCESS" );
                CCOUT << "return ERROR_NOT_IMPL;";
            }
            BLOCK_CLOSE;
            NEW_LINES( 2 );
        }

        NEW_LINE;

    }while( 0 );

    return ret;
}

CImplSerialStruct::CImplSerialStruct(
    CCppWriter* pWriter, ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'struct' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CImplSerialStruct::OutputSerial()
{
    gint32 ret = 0;
    do{
        CCOUT << "gint32 " << m_pNode->GetName()
            << "::" << "Serialize( BufPtr& pBuf_ ) const";
        NEW_LINE;
        BLOCK_OPEN;
        CCOUT << "if( pBuf_.IsEmpty() )";
        INDENT_UPL;
        CCOUT << "return -EINVAL;";
        INDENT_DOWNL;
        CCOUT << "gint32 ret = 0;";
        NEW_LINE;
        CCOUT << "do";
        BLOCK_OPEN;

        CCOUT << "ret = CSerialBase::Serialize(";
        INDENT_UPL;
        CCOUT << "pBuf_, m_dwMsgId );";
        INDENT_DOWNL;
        Wa( "if( ERROR( ret ) ) break;" );
        NEW_LINE;

        ObjPtr pFields =
            m_pNode->GetFieldList();

        CEmitSerialCode oesc(
            m_pWriter, pFields );

        oesc.OutputSerial( "", "pBuf_" );

        BLOCK_CLOSE;
        CCOUT << "while( 0 );";
        NEW_LINES( 2 );

        Wa( "return ret;" );

        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );

    return ret;
}

gint32 CImplSerialStruct::OutputDeserial()
{
    gint32 ret = 0;
    do{
        CCOUT << "gint32 " << m_pNode->GetName()
            << "::" << "Deserialize( ";
        CCOUT << "BufPtr& pBuf_ )";
        NEW_LINE;
        BLOCK_OPEN;
        CCOUT << "if( pBuf_.IsEmpty() )";
        INDENT_UPL;
        CCOUT << "return -EINVAL;";
        INDENT_DOWNL;
        CCOUT << "gint32 ret = 0;";
        NEW_LINE;
        CCOUT << "do";
        BLOCK_OPEN;

        ObjPtr pFields =
            m_pNode->GetFieldList();

        CFieldList* pfl = pFields;
        guint32 dwCount = pfl->GetCount();
        if( dwCount == 0 )
        {
            ret = -ENOENT;
            break;
        }

        Wa( "guint32 dwMsgId = 0;" );
        CCOUT << "ret = CSerialBase::Deserialize(";
        INDENT_UPL;
        CCOUT << "pBuf_, dwMsgId );";
        INDENT_DOWNL;
        NEW_LINE;
        Wa( "if( ERROR( ret ) ) return ret;" );
        Wa( "if( m_dwMsgId != dwMsgId ) return -EINVAL;" );
        NEW_LINE;

        CEmitSerialCode odesc( m_pWriter, pFields );
        odesc.OutputDeserial( "", "pBuf_" );

        BLOCK_CLOSE;
        CCOUT << "while( 0 );";
        NEW_LINES( 2 );

        Wa( "return ret;" );

        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );

    return ret;
}

gint32 CImplSerialStruct::OutputAssign()
{
    gint32 ret = 0;
    do{
        std::string strName = m_pNode->GetName();
        CCOUT << strName << "& " << strName
            << "::" << "operator=(";
        INDENT_UPL;
        CCOUT<< "const " << strName << "& rhs )";
        INDENT_DOWNL;
        BLOCK_OPEN;
        CCOUT << "do";
        BLOCK_OPEN;

        ObjPtr pFields =
            m_pNode->GetFieldList();
        CFieldList* pfl = pFields;
        if( pfl == nullptr )
        {
            ret = -EFAULT;
            break;
        }
        guint32 dwCount = pfl->GetCount();
        if( dwCount == 0 )
        {
            CCOUT << "break;";
        }
        else
        {
            Wa( "// data members" );
            CCOUT << "if( GetObjId() == rhs.GetObjId() )";
            INDENT_UPL;
            CCOUT << "break;";
            INDENT_DOWNL;
            NEW_LINE;
            guint32 i = 0;
            for( ; i < dwCount; i++ )
            {
                ObjPtr pObj = pfl->GetChild( i );
                CFieldDecl* pfd = pObj;
                if( pfd == nullptr )
                {
                    ret = -EFAULT;
                    break;
                }
                std::string strField =
                    pfd->GetName();

                CCOUT<< strField << " = rhs."
                    << strField << ";"; 
                if( i + 1 < dwCount )
                    NEW_LINE;
            }
            NEW_LINE;
        }
        BLOCK_CLOSE;
        CCOUT << "while( 0 );";
        NEW_LINES( 2 );
        CCOUT << "return *this;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );

    return ret;
}

gint32 CImplSerialStruct::Output()
{
    gint32 ret = OutputSerial();
    if( ERROR( ret ) )
        return ret;
    NEW_LINE;
    ret = OutputDeserial();
    if( ERROR( ret ) )
        return ret;
    NEW_LINE;
    ret = OutputAssign();
    NEW_LINE;
    return ret;
}

CImplIufProxy::CImplIufProxy(
    CCppWriter* pWriter, ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CImplIufProxy::Output()
{
    gint32 ret = 0;
    do{
        std::string strClass = "I";
        strClass += m_pNode->GetName() + "_PImpl";
        std::string strIfName = m_pNode->GetName();
        CCOUT << "gint32 " << strClass
            << "::" << "InitUserFuncs()";
        NEW_LINE;
        BLOCK_OPEN;

        ObjPtr pMethods =
            m_pNode->GetMethodList();

        CMethodDecls* pmds = pMethods;
        guint32 i = 0;
        pmds->GetCount();
        guint32 dwCount = pmds->GetCount();

        std::vector< ObjPtr > vecEvent;
        std::vector< ObjPtr > vecReq;

        for( ; i < dwCount; i++ )
        {
            ObjPtr pObj = pmds->GetChild( i );
            CMethodDecl* pmd = pObj;
            if( pmd == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            if( pmd->IsEvent() )
            {
                vecEvent.push_back( pObj );
            }
            else 
            {
                vecReq.push_back( pObj );
            }
        }

        if( !vecReq.empty() )
        {
            CCOUT << "BEGIN_IFPROXY_MAP( "
                << strIfName << ", false );";
            NEW_LINES( 2 );
            for( auto& elem : vecReq )
            {
                CMethodDecl* pmd = elem;
                std::string strMethod = pmd->GetName();

                ObjPtr pObj = pmd->GetInArgs();
                CArgList* pal = pObj;
                guint32 dwCount = 0;
                if( pal == nullptr )
                    dwCount = 0;
                else
                    dwCount = pal->GetCount();
                
                if( dwCount == 0 )
                {
                    CCOUT << "ADD_USER_PROXY_METHOD_EX( 0,";
                    INDENT_UP;
                    NEW_LINE;
                    CCOUT << strClass << "::"
                        << strMethod << ",";
                }
                else if( pmd->IsSerialize() )
                {
                    CCOUT << "ADD_USER_PROXY_METHOD_EX( 1,";
                    INDENT_UP;
                    NEW_LINE;
                    CCOUT << strClass << "::"
                        << strMethod << "Dummy,";
                }
                else
                {
                    CCOUT << "ADD_USER_PROXY_METHOD_EX( "
                        << dwCount << ",";
                    INDENT_UP;
                    NEW_LINE;
                    CCOUT << strClass << "::"
                        << strMethod<< ",";
                }
                NEW_LINE;
                CCOUT << "\"" << strMethod << "\" );";
                INDENT_DOWN;
                NEW_LINES( 2 );
            }

            Wa( "END_IFPROXY_MAP;" );
            NEW_LINE;
        }

        if( !vecEvent.empty() )
        {
            CCOUT << "BEGIN_IFHANDLER_MAP( "
                << strIfName << " );";
            NEW_LINES( 2 );
            for( auto& elem : vecEvent )
            {
                CMethodDecl* pmd = elem;
                std::string strMethod = pmd->GetName();
                
                CCOUT << "ADD_USER_EVENT_HANDLER(";
                INDENT_UPL;

                // either serializable or not, we
                // will use a wrapper to remove
                // `IEventSink' before calling the
                // true event handler
                CCOUT << strClass << "::"
                    << strMethod << "Wrapper,";
                NEW_LINE;
                CCOUT << "\"" << strMethod << "\" );";
                INDENT_DOWNL;
                NEW_LINE;
            }

            Wa( "END_IFHANDLER_MAP;" );
            NEW_LINE;
        }

        CCOUT << "return STATUS_SUCCESS;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );

    return ret;
}

CImplIufSvr::CImplIufSvr(
    CCppWriter* pWriter, ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CImplIufSvr::Output()
{
    gint32 ret = 0;
    do{
        std::string strClass = "I";
        strClass += m_pNode->GetName() + "_SImpl";
        std::string strIfName = m_pNode->GetName();
        CCOUT << "gint32 " << strClass
            << "::" << "InitUserFuncs()";
        NEW_LINE;
        BLOCK_OPEN;

        ObjPtr pMethods =
            m_pNode->GetMethodList();

        CMethodDecls* pmds = pMethods;
        guint32 i = 0;
        pmds->GetCount();
        guint32 dwCount = pmds->GetCount();

        std::vector< ObjPtr > vecHandlers;

        for( ; i < dwCount; i++ )
        {
            ObjPtr pObj = pmds->GetChild( i );
            vecHandlers.push_back( pObj );
        }

        if( !vecHandlers.empty() )
        {
            CCOUT << "BEGIN_IFHANDLER_MAP( "
                << strIfName << " );";
            NEW_LINES( 2 );
            for( auto& elem : vecHandlers )
            {
                CMethodDecl* pmd = elem;
                std::string strMethod = pmd->GetName();

                if( pmd->IsEvent() )
                    continue;

                CCOUT << "ADD_USER_SERVICE_HANDLER(";
                INDENT_UPL;
                CCOUT << strClass << "::"
                    << strMethod << "Wrapper,";
                NEW_LINE;
                CCOUT << "\"" << strMethod << "\" );";
                INDENT_DOWNL;
                NEW_LINE;
            }

            Wa( "END_IFHANDLER_MAP;" );
            NEW_LINE;
        }

        CCOUT << "return STATUS_SUCCESS;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );

    return ret;
}

CEmitSerialCode::CEmitSerialCode(
    CCppWriter* pWriter, ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pArgs = pNode;
    if( m_pArgs == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CEmitSerialCode::OutputSerial(
    const std::string& strObjc,
    const std::string strBuf )
{
    gint32 ret = 0;
    do{

        guint32 i = 0;
        guint32 dwCount = m_pArgs->GetCount();
        for( ; i < dwCount; i++ )
        {
            ObjPtr pObj = m_pArgs->GetChild( i );
            CFieldDecl* pfa = pObj;
            if( pfa == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            pObj = pfa->GetType();
            std::string strName = pfa->GetName();
            CAstNodeBase* pTypeNode = pObj;

            std::string strSig =
                pTypeNode->GetSignature();

            if( strSig.empty() )
            {
                ret = -EINVAL;
                break;
            }

            std::string strObj;
            if( !strObjc.empty() )
                strObj = strObjc + '.';

            switch( strSig[ 0 ] )
            {
            case '(' :
                {
                    CCOUT << "ret = " << strObj
                        << "SerializeArray(";
                    INDENT_UPL;
                    CCOUT << strBuf << ", " << strName << ", \""
                        << strSig << "\" );";
                    INDENT_DOWNL;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            case '[' :
                {
                    CCOUT << "ret = "<< strObj
                        << "SerializeMap(";
                    INDENT_UPL;
                    CCOUT << strBuf << ", " << strName << ", \""
                        << strSig << "\" );";
                    INDENT_DOWNL;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }

            case 'O' :
                {
                    CCOUT << "ret = " << strObj
                        << "SerialStruct( ";
                    INDENT_UPL;
                    CCOUT << strBuf << ", " << strName << " );";
                    INDENT_DOWNL;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            case 'h':
                {
                    CCOUT << "ret = " << strName
                        <<".Serialize( " << strBuf << " );";

                    NEW_LINE;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            case 'Q':
            case 'q':
            case 'D':
            case 'd':
            case 'W':
            case 'w':
            case 'f':
            case 'F':
            case 'b':
            case 'B':
            case 's':
            case 'a':
            case 'o':
                {
                    CCOUT << "ret = " << strObj
                        << "CSerialBase::Serialize(";
                    INDENT_UPL;
                    CCOUT << strBuf << ", "
                        << strName << " );";
                    INDENT_DOWNL;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            default:
                {
                    ret = -EINVAL;
                    break;
                }
            }
            if( i + 1 < dwCount )
                NEW_LINE;
        }

    }while( 0 );

    return ret;
}

gint32 CEmitSerialCode::OutputDeserial(
    const std::string& strObjc,
    const std::string strBuf )
{
    gint32 ret = 0;
    do{
        guint32 i = 0;
        guint32 dwCount = m_pArgs->GetCount();
        for( ; i < dwCount; i++ )
        {
            ObjPtr pObj = m_pArgs->GetChild( i );
            CFieldDecl* pfa = pObj;
            if( pfa == nullptr )
            {
                ret = -EFAULT;
                break;
            }

            pObj = pfa->GetType();
            std::string strName = pfa->GetName();
            CAstNodeBase* pTypeNode = pObj;

            std::string strSig =
                pTypeNode->GetSignature();

            if( strSig.empty() )
            {
                ret = -EINVAL;
                break;
            }

            std::string strObj;
            if( !strObjc.empty() )
                strObj = strObjc + '.';

            switch( strSig[ 0 ] )
            {
            case '(' :
                {
                    CCOUT << "ret = " << strObj << "DeserialArray(";
                    INDENT_UPL;
                    CCOUT << strBuf << ", " << strName << ", \""
                        << strSig << "\" );";
                    INDENT_DOWNL;
                    NEW_LINE;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            case '[' :
                {
                    CCOUT << "ret = " << strObj << "DeserialMap(";
                    INDENT_UPL;
                    CCOUT << strBuf << ", " << strName << ", \""
                        << strSig << "\" );";
                    INDENT_DOWNL;
                    NEW_LINE;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }

            case 'O' :
                {
                    CCOUT << "ret = " << strObj << "DeserialStruct(";
                    INDENT_UPL;
                    CCOUT << strBuf << ", " << strName << " );";
                    INDENT_DOWNL;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            case 'h':
                {
                    CCOUT << "ret = " << strName
                        <<".Deserialize( " << strBuf <<" );";
                    NEW_LINE;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            case 'Q':
            case 'q':
            case 'D':
            case 'd':
            case 'W':
            case 'w':
            case 'f':
            case 'F':
            case 'b':
            case 'B':
            case 's':
            case 'a':
            case 'o':
                {
                    CCOUT << "ret = " << strObj
                        << "CSerialBase::Deserialize(";
                    INDENT_UPL;
                    CCOUT << strBuf << ", " << strName << " );";
                    INDENT_DOWNL;
                    NEW_LINE;
                    Wa( "if( ERROR( ret ) ) break;" );
                    break;
                }
            default:
                {
                    ret = -EINVAL;
                    break;
                }
            }
            if( i + 1 < dwCount )
                NEW_LINE;
        }
    }while( 0 );

    return ret;
}

CMethodWriter::CMethodWriter(
    CCppWriter* pWriter )
{ m_pWriter = pWriter; }

CImplIfMethodProxy::CImplIfMethodProxy(
    CCppWriter* pWriter, ObjPtr& pNode )
    : super( pWriter )
{
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }

    CAstNodeBase* pParent = m_pNode->GetParent();
    if( pParent == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'method_decls' node" );
        throw std::runtime_error( strMsg );
    }
    pParent = pParent->GetParent();
    if( pParent == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }

    m_pIf = ObjPtr( pParent );
    if( m_pIf == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CImplIfMethodProxy::Output()
{
    gint32 ret = 0;

    do{
        NEW_LINE;

        if( m_pNode->IsEvent() )
        {
            ret = OutputEvent();
            break;
        }
        else if( m_pNode->IsAsyncp() )
        {
            ret = OutputAsync();
            if( ERROR( ret ) )
                break;

            ret = OutputAsyncCbWrapper();
        }
        else
        {
            ret = OutputSync();
        }

    }while( 0 );

    return ret;
}

gint32 CImplIfMethodProxy::OutputEvent()
{
    gint32 ret = 0;
    std::string strClass = "I";
    strClass += m_pIf->GetName() + "_PImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    guint32 dwInCount = GetArgCount( pInArgs );
    guint32 dwCount = dwInCount;

    bool bSerial = m_pNode->IsSerialize();

    do{
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "Wrapper( ";
        INDENT_UP;
        NEW_LINE;
        CCOUT << "IEventSink* pCallback";
        if( dwCount == 0 )
        {
        }
        else if( bSerial )
        {
            CCOUT << ", BufPtr& pBuf_";
        }
        else
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pInArgs );
        }
        CCOUT << " )";
        INDENT_DOWN;
        NEW_LINE;

        BLOCK_OPEN;

        if( dwCount == 0 )
        {
            CCOUT << "return this->"
                << strMethod << "();";
        }
        else if( !bSerial )
        {
            CCOUT << "return this->"
                << strMethod << "(";
            INDENT_UPL;
            GenActParams( pInArgs );
            CCOUT << " );";
            INDENT_DOWN;
        }
        else
        {
            Wa( "gint32 ret = 0;" );
            DeclLocals( pInArgs );

            ret = GenDeserialArgs(
                pInArgs, "pBuf_", false, false );
            if( ERROR( ret ) )
                break;

            // call the user's handler
            CCOUT << strMethod << "(";
            GenActParams( pInArgs );

            if( dwCount == 0 )
                CCOUT << ");";
            else
                CCOUT << " );";

            NEW_LINES( 2 );
            CCOUT << "return ret;";
        }

        BLOCK_CLOSE;
        NEW_LINES( 1 );

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodProxy::OutputSync()
{
    gint32 ret = 0;
    std::string strClass = "I";
    std::string strIfName = m_pIf->GetName();
    strClass += strIfName + "_PImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    ObjPtr pOutArgs = m_pNode->GetOutArgs();

    guint32 dwInCount = GetArgCount( pInArgs );
    guint32 dwOutCount = GetArgCount( pOutArgs );
    guint32 dwCount = dwInCount + dwOutCount;


    bool bSerial = m_pNode->IsSerialize();
    bool bNoReply = m_pNode->IsNoReply();

    do{
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "(";

        // gen the param list
        if( dwCount == 0 )
        {
            CCOUT << ")";
        }
        else
        {
            INDENT_UPL;
            bool bComma = false;
            if( dwInCount > 0 )
            {
                bComma = true;
                GenFormInArgs( pInArgs );
            }

            if( dwOutCount > 0 )
            {
                if( bComma )
                {
                    CCOUT << ",";
                    NEW_LINE;
                }
                GenFormOutArgs( pOutArgs );
            }
    
            CCOUT << " )";
            INDENT_DOWNL;
        }

        BLOCK_OPEN;
        Wa( "gint32 ret = 0;" );
        Wa( "CParamList oOptions_;" );
        Wa( "CfgPtr pResp_;" );
        CCOUT << "oOptions_[ propIfName ] =";
        INDENT_UP;
        NEW_LINE;
        CCOUT << "DBUS_IF_NAME( \""
            << strIfName << "\" );";
        INDENT_DOWN;
        if( bNoReply )
        {
            NEW_LINE;
            CCOUT << "oOptions_[ propNoReply ] = true;";
        }
        NEW_LINE;
        CCOUT << "oOptions_[ propSeriProto ] = ";
        INDENT_UPL;
        if( bSerial )
            CCOUT << "( guint32 )seriRidl;";
        else
            CCOUT << "( guint32 )seriNone;";
        INDENT_DOWNL;
        guint32 dwTimeoutSec =
            m_pNode->GetTimeoutSec();
        if( dwTimeoutSec > 0 )
        {
            NEW_LINE;
            CCOUT << "oOptions_[ propTimeoutSec ] = "
                << dwTimeoutSec << ";"; 
        }
        NEW_LINE;

        if( 0 == dwInCount + dwOutCount )
        {
            CCOUT << "ret = this->SyncCallEx("
                << "oOptions_.GetCfg(), pResp_, "
                << "\"" << strMethod << "\" );";
            NEW_LINE;
            if( !bNoReply )
            {
                Wa( "if( ERROR( ret ) ) return ret;" );
                NEW_LINE;
                Wa( "guint32 dwRet_ = 0;" );
                Wa( "CCfgOpener oResp_( ( IConfigDb* )pResp_ );" );
                Wa( "ret = oResp_.GetIntProp( propReturnValue, dwRet_ );" );
                Wa( "if( ERROR( ret ) ) return ret;" );
                Wa( "ret = ( gint32 )dwRet_;" );
            }
            Wa( "return ret;" );
        }
        else if( !bSerial )
        {
            CCOUT << "ret = this->SyncCallEx(";
            INDENT_UPL;
            CCOUT << "oOptions_.GetCfg(), pResp_,";
            NEW_LINE;
            CCOUT << "\"" << strMethod << "\"";

            if( dwInCount > 0 )
            {
                CCOUT << ",";
                NEW_LINE;
                GenActParams( pInArgs );
                INDENT_DOWN;
            }

            CCOUT << " );";
            NEW_LINE;
            if( !bNoReply )
            {
                Wa( "if( ERROR( ret ) ) return ret;" );
                NEW_LINE;
                Wa( "guint32 dwRet_ = 0;" );
                Wa( "CCfgOpener oResp_( ( IConfigDb* )pResp_ );" );
                CCOUT << "ret = oResp_.GetIntProp(";
                INDENT_UPL;
                CCOUT << "propReturnValue, dwRet_ );";
                INDENT_DOWNL;
                Wa( "if( ERROR( ret ) ) return ret;" );
                NEW_LINE;
                Wa( "ret = ( gint32 )dwRet_;" );

                if( dwOutCount > 0 )
                {
                    Wa( "if( ERROR( ret ) ) return ret;" );
                    NEW_LINE;
                    CCOUT << "ret = this->FillArgs(";
                    INDENT_UPL;
                    CCOUT << "pResp_, ret,";
                    NEW_LINE;
                    GenActParams( pOutArgs );
                    CCOUT << " );";
                    INDENT_DOWNL;
                }
            }
            Wa( "return ret;" );
        }
        else /* need serialize */
        {
            NEW_LINE;
            Wa( "//Serialize the input parameters" );
            Wa( "BufPtr pBuf_( true );" );
            ret = GenSerialArgs(
                pInArgs, "pBuf_", true, true );

            if( ERROR( ret ) )
                break;

            NEW_LINE;
            CCOUT << "ret = this->SyncCallEx(";
            INDENT_UPL;
            CCOUT << "oOptions_.GetCfg(), pResp_, ";
            NEW_LINE;
            CCOUT << "\"" << strMethod << "\", "
                << "pBuf_ );";
            INDENT_DOWNL;
            NEW_LINE;
            if( !bNoReply )
            {
                Wa( "if( ERROR( ret ) ) return ret;" );
                NEW_LINE;
                Wa( "guint32 dwRet_ = 0;" );
                Wa( "CCfgOpener oResp_( ( IConfigDb* )pResp_ );" );
                Wa( "ret = oResp_.GetIntProp( propReturnValue, dwRet_ );" );
                Wa( "if( ERROR( ret ) ) return ret;" );
                Wa( "ret = ( gint32 )dwRet_;" );
                if( dwOutCount > 0 )
                {
                    Wa( "if( ERROR( ret ) ) return ret;" );
                    Wa( "BufPtr pBuf2;" );
                    Wa( "ret = oResp_.GetProperty( 0, pBuf2 );" );
                    Wa( "if( ERROR( ret ) ) return ret;" );

                    if( ERROR( ret ) )
                        break;

                    ret = GenDeserialArgs(
                        pOutArgs, "pBuf2", true, true );
                }
                NEW_LINE;
            }
            CCOUT << "return ret;";
        }
        BLOCK_CLOSE;
        NEW_LINES( 1 );

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodProxy::OutputAsync()
{
    gint32 ret = 0;
    std::string strClass = "I";
    std::string strIfName = m_pIf->GetName();
    strClass += strIfName + "_PImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    ObjPtr pOutArgs = m_pNode->GetOutArgs();

    guint32 dwInCount = GetArgCount( pInArgs );

    bool bSerial = m_pNode->IsSerialize();
    bool bNoReply = m_pNode->IsNoReply();

    do{
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "( ";
        INDENT_UPL;
        CCOUT << "IConfigDb* context";
        // gen the param list
        if( dwInCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pInArgs );
        }
        CCOUT << " )";
        INDENT_DOWNL;

        BLOCK_OPEN;

        Wa( "CParamList oOptions_;" );
        Wa( "CfgPtr pResp_;" );
        CCOUT << "oOptions_[ propIfName ] =";
        INDENT_UP;
        NEW_LINE;
        CCOUT << "DBUS_IF_NAME( \""
            << strIfName << "\" );";
        INDENT_DOWN;
        if( bNoReply )
        {
            NEW_LINE;
            CCOUT << "oOptions_[ propNoReply ] = true;";
        }
        NEW_LINE;
        CCOUT << "oOptions_[ propSeriProto ] = ";
        INDENT_UPL;
        if( bSerial )
            CCOUT << "( guint32 )seriRidl;";
        else
            CCOUT << "( guint32 )seriNone;";
        INDENT_DOWNL;
        guint32 dwTimeoutSec =
            m_pNode->GetTimeoutSec();
        if( dwTimeoutSec > 0 )
        {
            CCOUT << "oOptions_[ propTimeoutSec ] = "
                << dwTimeoutSec << ";"; 
            NEW_LINE;
        }
        NEW_LINE;

        Wa( "CParamList oReqCtx_;" );
        Wa( "ObjPtr pTemp( context );" );
        Wa( "oReqCtx_.Push( pTemp );" );
        Wa( "TaskletPtr pRespCb_;" );
        NEW_LINE;
        CCOUT << "gint32 ret = NEW_PROXY_RESP_HANDLER2(";
        INDENT_UPL;
        CCOUT <<  "pRespCb_, ObjPtr( this ), ";
        NEW_LINE;
        CCOUT << "&" << strClass << "::"
            << strMethod << "CbWrapper, ";
        NEW_LINE;
        CCOUT << "nullptr, oReqCtx_.GetCfg() );";
        INDENT_DOWN;
        NEW_LINES( 2 );
        Wa( "if( ERROR( ret ) ) return ret;" );
        NEW_LINE;

        if( dwInCount == 0 )
        {
            CCOUT << "ret = this->AsyncCall(";
            INDENT_UPL;
            Wa( "( IEventSink* )pRespCb_, " );
            Wa( "oOptions_.GetCfg(), pResp_," );
            CCOUT << "\"" << strMethod << "\" );";
            INDENT_DOWNL;
        }
        else if( !bSerial )
        {
            CCOUT << "ret = this->AsyncCall(";
            INDENT_UPL;
            Wa( "( IEventSink* )pRespCb_, " );
            Wa( "oOptions_.GetCfg(), pResp_," );
            CCOUT << "\"" << strMethod << "\"";

            if( dwInCount > 0 )
            {
                CCOUT << ", ";
                INDENT_UP;
                GenActParams( pInArgs );
                INDENT_DOWN;
            }

            CCOUT << " );";
            INDENT_DOWNL;

        }
        else /* need serialize */
        {
            Wa( "//Serialize the input parameters" );
            Wa( "BufPtr pBuf_( true );" );
            ret = GenSerialArgs(
                pInArgs, "pBuf_", true, true );

            if( ERROR( ret ) )
                break;

            NEW_LINE;
            CCOUT << "ret = this->AsyncCall(";
            INDENT_UPL;
            Wa( "( IEventSink* )pRespCb_, " );
            Wa( "oOptions_.GetCfg(), pResp_," );
            CCOUT << "\"" << strMethod << "\",";
            NEW_LINE;
            CCOUT << "pBuf_ );";
            INDENT_DOWNL;
        }
        if( !bNoReply )
        {
            NEW_LINE;
            Wa( "if( ret == STATUS_PENDING )" );
            BLOCK_OPEN;
            CCOUT << "if( context == nullptr )";
            INDENT_UPL;
            CCOUT <<"return ret;";
            INDENT_DOWNL;
            CCOUT << "CCfgOpener oContext( context );";
            NEW_LINE;
            CCOUT <<"oContext.CopyProp(";
            INDENT_UPL;
            CCOUT << "propTaskId, pResp_ );";
            INDENT_DOWNL;
            CCOUT << "return ret;";
            BLOCK_CLOSE;
            NEW_LINES( 2 );
            Wa( "( *pRespCb_ )( eventCancelTask );" );
        }
        CCOUT << "return ret;";
        BLOCK_CLOSE;
        NEW_LINES( 1 );

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodProxy::OutputAsyncCbWrapper()
{
    gint32 ret = 0;
    std::string strClass = "I";
    std::string strIfName = m_pIf->GetName();
    strClass += strIfName + "_PImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    ObjPtr pOutArgs = m_pNode->GetOutArgs();

    guint32 dwOutCount = GetArgCount( pOutArgs );

    bool bSerial = m_pNode->IsSerialize();
    bool bNoReply = m_pNode->IsNoReply();

    do{
        Wa( "//Async callback wrapper" );
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "CbWrapper" << "( ";
        INDENT_UP;
        NEW_LINE;
        Wa( "IEventSink* pCallback, " );
        Wa( "IEventSink* pIoReq," );
        CCOUT << "IConfigDb* pReqCtx )";
        INDENT_DOWNL;
        BLOCK_OPEN;

        Wa( "gint32 ret = 0;" );
        CCOUT << "do";
        BLOCK_OPEN;
        NEW_LINE; 
        Wa( "IConfigDb* pResp_ = nullptr;" );
        Wa( "CCfgOpenerObj oReq_( pIoReq );" );
        CCOUT << "ret = oReq_.GetPointer(";
        INDENT_UPL;
        CCOUT << "propRespPtr, pResp_ );";
        INDENT_DOWNL;
        CCOUT << "if( ERROR( ret ) )";
        INDENT_UPL;
        CCOUT << "break;";
        INDENT_DOWNL;
        NEW_LINE;

        Wa( "CCfgOpener oResp_( pResp_ );" );
        Wa( "gint32 iRet = 0;" );

        CCOUT << "ret = oResp_.GetIntProp( ";
        INDENT_UPL;
        CCOUT << "propReturnValue, ( guint32& ) iRet );";
        INDENT_DOWNL;

        CCOUT << "if( ERROR( ret ) ) break;";
        NEW_LINES( 2 );

        Wa( "IConfigDb* context = nullptr;" );
        Wa( "CCfgOpener oReqCtx_( pReqCtx );" );
        Wa( "ret = oReqCtx_.GetPointer( 0, context );" );
        Wa( "if( ERROR( ret ) ) context = nullptr;" );
        NEW_LINE;

        // gen the param list
        if( dwOutCount == 0 || bNoReply )
        {
             CCOUT << strMethod
                 <<"Callback( context, iRet );";
        }
        else if( !bSerial )
        {
            DeclLocals( pOutArgs );

            Wa( "if( ERROR( iRet ) )" );
            BLOCK_OPEN;

            CCOUT << strMethod << "Callback(";
            INDENT_UPL;
            CCOUT << "context, iRet,";
            NEW_LINE;

            GenActParams( pOutArgs );

            CCOUT << " )";
            NEW_LINE;
            INDENT_DOWN;

            Wa( "break;" );
            BLOCK_CLOSE;

            Wa( "guint32 i = 0;" );
            Wa( "std::vector< BufPtr > vecParms" );
            CCOUT << "for( ; i <" << dwOutCount<< "; i++ )";
            NEW_LINE;
            BLOCK_OPEN;
            Wa( "BufPtr pVal;" );
            Wa( "ret = oResp_.GetProperty( i, pVal );" );
            Wa( "if( ERROR( ret ) break;" );
            Wa( "vecParams.push_back( pVal );" ); 
            BLOCK_CLOSE;
            Wa( "if( ERROR( ret ) break;" );

            CCOUT << strMethod << "Callback( ";
            INDENT_UPL;
            CCOUT << "context, iRet";
            NEW_LINE;

            std::vector< std::string > vecTypes;
            ret = GetArgTypes( pOutArgs, vecTypes );
            if( ERROR( ret ) ||
                vecTypes.size() != dwOutCount )
            {
                ret = -ERANGE;
                break;
            }
            for( guint32 i = 0; i < dwOutCount; i++ )
            {
                CCOUT << ",";
                NEW_LINE;
                CCOUT << "CastTo< " <<  vecTypes[ i ] << ">(";
                CCOUT << "vecParams[" << i << "] )";
            }

            CCOUT << " )";
            INDENT_DOWN;
            NEW_LINE;
        }
        else /* need serialization */
        {
            DeclLocals( pOutArgs );
            NEW_LINE;
            Wa( "if( SUCCEEDED( iRet ) )" );
            BLOCK_OPEN;
            Wa( "BufPtr pBuf_;" );
            Wa( "ret = oResp_.GetProperty( 0, pBuf_ );" );
            Wa( "if( ERROR( ret ) ) break;" );

            ret = GenDeserialArgs( pOutArgs,
                "pBuf_", false, false, true );
            if( ERROR( ret ) )
                break;
            BLOCK_CLOSE;

            NEW_LINE;
            CCOUT << "this->" << strMethod <<"Callback(";
            INDENT_UPL;
            CCOUT << "context, iRet,";
            NEW_LINE;
            GenActParams( pOutArgs );
            CCOUT << " );";
            INDENT_DOWN;
        }

        NEW_LINE;
        BLOCK_CLOSE;
        Wa( "while( 0 );" );
        NEW_LINE;
        Wa( "return 0;" );

        BLOCK_CLOSE;
        NEW_LINES( 1 );

    }while( 0 );
    
    return ret;
}

CImplIfMethodSvr::CImplIfMethodSvr(
    CCppWriter* pWriter, ObjPtr& pNode )
    : super( pWriter )
{
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }

    CAstNodeBase* pParent = m_pNode->GetParent();
    if( pParent == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'method_decls' node" );
        throw std::runtime_error( strMsg );
    }
    pParent = pParent->GetParent();
    if( pParent == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }

    m_pIf = ObjPtr( pParent );
    if( m_pIf == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'interf_decl' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CImplIfMethodSvr::Output()
{
    gint32 ret = 0;

    do{
        NEW_LINE;

        if( m_pNode->IsEvent() )
        {
            ret = OutputEvent();
            break;
        }
        else if( m_pNode->IsAsyncs() )
        {
            ret = OutputAsync();
            if( ERROR( ret ) )
                break;

            ret = OutputAsyncCallback();
        }
        else
        {
            ret = OutputSync();
        }

    }while( 0 );

    return ret;
}

gint32 CImplIfMethodSvr::OutputEvent()
{
    gint32 ret = 0;
    std::string strClass = "I";
    std::string strIfName = m_pIf->GetName();
    strClass += strIfName + "_SImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    guint32 dwInCount = GetArgCount( pInArgs );

    bool bSerial = m_pNode->IsSerialize();

    do{
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "(";
        INDENT_UPL;

        // gen the param list
        if( dwInCount == 0 )
        {
            CCOUT << ")";
        }
        else
        {
            GenFormInArgs( pInArgs );
            CCOUT << " )";
        }

        INDENT_DOWNL;
        BLOCK_OPEN;
        Wa( "gint32 ret = 0;" );
        Wa( "CParamList oOptions_;" );
        CCOUT << "oOptions_[ propSeriProto ] = ";
        INDENT_UPL;
        if( bSerial )
            CCOUT << "( guint32 )seriRidl;";
        else
            CCOUT << "( guint32 )seriNone;";
        INDENT_DOWNL;
        guint32 dwTimeoutSec =
            m_pNode->GetTimeoutSec();
        if( dwTimeoutSec > 0 )
        {
            NEW_LINE;
            CCOUT << "oOptions_[ propTimeoutSec ] = "
                << dwTimeoutSec << ";"; 
        }
        NEW_LINE;

        if( 0 == dwInCount )
        {
            CCOUT << "ret = this->SendEventEx(";
            INDENT_UPL;
            Wa( "nullptr, oOptions_.GetCfg()," );
            CCOUT << "iid( "<< strIfName << " ), ";
            NEW_LINE;
            CCOUT << "USER_EVENT( \"" << strMethod << "\" ), \"\" ) ";
            INDENT_DOWN;
        }
        else if( !bSerial )
        {
            CCOUT << "ret = this->SendEventEx(";
            INDENT_UPL;
            Wa( "nullptr, oOptions_.GetCfg()," );
            CCOUT << "iid( "<< strIfName << " ), ";
            NEW_LINE;
            CCOUT << "USER_EVENT( \"" << strMethod << "\" ), \"\",";
            NEW_LINE;
            ret = GenActParams( pInArgs );
            if( ERROR( ret ) )
                break;

            CCOUT << " );";
            INDENT_DOWN;
        }
        else /* need serialize */
        {
            Wa( "//Serialize the input parameters" );
            Wa( "BufPtr pBuf_( true );" );
            ret = GenSerialArgs(
                pInArgs, "pBuf_", true, true );
            if( ERROR( ret ) )
                break;

            CCOUT << "ret = this->SendEventEx(";
            INDENT_UPL;
            Wa( "nullptr, oOptions_.GetCfg()," );
            CCOUT << "iid( "<< strIfName << " ), ";
            NEW_LINE;
            CCOUT << "USER_EVENT( \"" << strMethod << "\" ), \"\",";
            NEW_LINE;
            CCOUT << "pBuf_ );";
            INDENT_DOWN;
        }
        NEW_LINES( 2 );
        CCOUT << "return ret;";

        BLOCK_CLOSE;
        NEW_LINES( 1 );

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodSvr::OutputSync()
{
    gint32 ret = 0;
    std::string strClass = "I";
    strClass += m_pIf->GetName() + "_SImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    ObjPtr pOutArgs = m_pNode->GetOutArgs();

    guint32 dwInCount = GetArgCount( pInArgs );
    guint32 dwOutCount = GetArgCount( pOutArgs );
    guint32 dwCount = dwInCount + dwOutCount;

    bool bSerial = m_pNode->IsSerialize();
    bool bNoReply = m_pNode->IsNoReply();

    do{
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "Wrapper( ";
        INDENT_UPL;
        CCOUT << "IEventSink* pCallback";
        if( dwCount == 0 )
        {
        }
        else if( bSerial )
        {
            CCOUT << ", BufPtr& pBuf_";
        }
        else
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pInArgs );
        }

        CCOUT << " )";
        INDENT_DOWNL;

        BLOCK_OPEN;

        if( dwCount == 0 )
        {
            CCOUT << "gint32 ret = this->"
                << strMethod << "();";
            NEW_LINE;
            if( !bNoReply )
            {
                Wa( "CParamList oResp_;" );
                Wa( "oResp_[ propReturnValue ] = ret;" );
            }
        }
        else if( !bSerial )
        {
            DeclLocals( pOutArgs );
            NEW_LINE;
            CCOUT << "gint32 ret = this->"
                << strMethod << "(";
            INDENT_UPL;

            GenActParams( pInArgs, pOutArgs );

            if( dwInCount > 0 )
                CCOUT <<  " );";
            else
                CCOUT << ");";

            INDENT_DOWNL;

            Wa( "if( ret == STATUS_PENDING )" );
            BLOCK_OPEN;
            Wa( "ret = ERROR_STATE;" );
            CCOUT << "DebugPrintEx( logErr, ret, ";
            INDENT_UPL;
            CCOUT << "\"Cannot return pending with\"";
            NEW_LINE;
            CCOUT << "\"Sync Request Handler\" );";
            INDENT_DOWN;
            BLOCK_CLOSE;
            NEW_LINE;

            if( !bNoReply )
            {
                Wa( "CParamList oResp_;" );
                Wa( "oResp_[ propReturnValue ] = ret;" );
                if( dwOutCount > 0 )
                {
                    Wa( "if( SUCCEEDED( ret ) )" );
                    BLOCK_OPEN;

                    std::vector< std::string > vecArgs;
                    ret = GetArgsForCall( pOutArgs, vecArgs );
                    for( guint32 i = 0; i < vecArgs.size(); i++ )
                    {
                        CCOUT << "oResp_.Push( "
                            << vecArgs[ i ] << " );";
                        if( i + 1 < vecArgs.size() )
                            NEW_LINE;
                    }
                    BLOCK_CLOSE;
                    NEW_LINE;
                }
            }
        }
        else
        {
            Wa( "gint32 ret = 0;" );

            DeclLocals( pInArgs );
            DeclLocals( pOutArgs );

            if( dwInCount > 0 )
            {
                ret = GenDeserialArgs(
                    pInArgs, "pBuf_", false, false );
            }

            // call the user's handler
            CCOUT << "ret = " << strMethod << "(";
            INDENT_UP;
            NEW_LINE;

            GenActParams( pInArgs, pOutArgs );

            if( dwInCount + dwOutCount == 0 )
                CCOUT << ");";
            else
                CCOUT << " );";
            INDENT_DOWNL;
            Wa( "if( ret == STATUS_PENDING )" );
            BLOCK_OPEN;
            Wa( "ret = ERROR_STATE;" );
            CCOUT << "DebugPrintEx( logErr, ret, ";
            INDENT_UPL;
            CCOUT << "\"Cannot return pending with\"";
            NEW_LINE;
            CCOUT << "\"Sync Request Handler\" );";
            INDENT_DOWN;
            BLOCK_CLOSE;

            if( !bNoReply )
            {
                NEW_LINES( 2 );
                Wa( "CParamList oResp_;" );
                Wa( "oResp_[ propReturnValue ] = ret;" );
                if( dwOutCount > 0 )
                {
                    Wa( "if( SUCCEEDED( ret ) )" );
                    BLOCK_OPEN;

                    Wa( "BufPtr pBuf2( true );" );
                    ret = GenSerialArgs(
                        pOutArgs, "pBuf2", false, false );
                    if( ERROR( ret ) )
                        break;

                    CCOUT << "oResp_.Push( pBuf2 );";
                    BLOCK_CLOSE;
                    NEW_LINE;
                }
            }
        }

        if( !bNoReply )
        {
            NEW_LINE;
            CCOUT << "this->SetResponse( ";
            INDENT_UPL;
            CCOUT << "pCallback, oResp_.GetCfg() );";
            INDENT_DOWNL;
        }

        NEW_LINE;
        CCOUT << "return ret;";
        BLOCK_CLOSE;
        NEW_LINES( 1 );

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodSvr::OutputAsyncNonSerial()
{
    gint32 ret = 0;
    std::string strClass = "I";
    strClass += m_pIf->GetName() + "_SImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    ObjPtr pOutArgs = m_pNode->GetOutArgs();

    guint32 dwInCount = GetArgCount( pInArgs );
    guint32 dwOutCount = GetArgCount( pOutArgs );

    bool bSerial = m_pNode->IsSerialize();
    if( bSerial )
        return -EINVAL;
    bool bNoReply = m_pNode->IsNoReply();
    do{
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "Wrapper( ";
        INDENT_UPL;
        if( dwInCount == 0 )
        {
            CCOUT << "IEventSink* pCallback )";
        }
        else
        {
            CCOUT << "IEventSink* pCallback,";
            NEW_LINE;
            GenFormInArgs( pInArgs );
            CCOUT << " )";
        }

        INDENT_DOWNL;
        BLOCK_OPEN;

        Wa( "gint32 ret = 0;" );
        Wa( "TaskletPtr pNewCb;" );
        CCOUT << "do";
        BLOCK_OPEN;

        guint32 dwTimeoutSec =
            m_pNode->GetTimeoutSec();
        if( dwTimeoutSec == 0 )
        {
            Wa( "guint32 dwTimeoutSec = 0;" );
            Wa( "CCfgOpenerObj oCfg( this);" );
            CCOUT << "ret = oCfg.GetIntProp(";
            INDENT_UPL;
            CCOUT << "propTimeoutSec, dwTimeoutSec );";
            INDENT_DOWNL;
            Wa( "if( ERROR( ret ) ) break;" );
        }
        else
        {
            CCOUT << "guint32 dwTimeoutSec = "
                << dwTimeoutSec << ";";
            NEW_LINE;
        }

        NEW_LINE;
        CCOUT << "CIfInvokeMethodTask* pInv =";
        INDENT_UPL;
        CCOUT << "ObjPtr( pCallback );";
        INDENT_DOWNL;
        CCOUT << "CfgPtr pCallOpt;";
        NEW_LINE;
        CCOUT << "ret = pInv->GetCallOptions( pCallOpt );";
        NEW_LINE;
        Wa( "if( ERROR( ret ) ) break;" );
        NEW_LINE;
        CCOUT << "CCfgOpener oOptions(";
        INDENT_UPL;
        CCOUT << "( IConfigDb* )pCallOpt );";
        INDENT_DOWNL;
        Wa( "oOptions[ propTimeoutSec ] = dwTimeoutSec;" );
        Wa( "oOptions.RemoveProperty( propKeepAliveSec );" );
        CCOUT << "if( dwTimeoutSec > 2 )";
        INDENT_UPL; 
        CCOUT << "oOptions[ propKeepAliveSec ] =";
        INDENT_UPL; 
        CCOUT << "dwTimeoutSec / 2;";
        INDENT_DOWNL;
        INDENT_DOWNL;

        if( dwOutCount > 0 )
        {
            DeclLocals( pOutArgs );
            NEW_LINE;
        }

        CCOUT << "ret = DEFER_CANCEL_HANDLER2(";
        INDENT_UPL;
        CCOUT << "-1, pNewCb, this,";
        NEW_LINE;
        CCOUT << "&" << strClass << "::"
            << strMethod << "CancelWrapper,";
        NEW_LINE;
        CCOUT << "pCallback, 0";
        if( dwInCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            ret = GenActParams( pInArgs );
            if( ERROR( ret ) )
                break;
        }
        CCOUT << " );";
        INDENT_DOWNL;
        NEW_LINE;
        Wa( "if( ERROR( ret ) ) break;" );
        NEW_LINE;

        // call the user's handler
        CCOUT << "ret = "
            << strMethod << "( pNewCb";

        if( dwInCount + dwOutCount > 0 )
        {
            CCOUT << ",";
            INDENT_UPL;
            GenActParams( pInArgs, pOutArgs );
            INDENT_DOWN;
        }
        CCOUT << " );";
        NEW_LINES( 2 );
        if( !bNoReply )
        {
            Wa( "if( ret == STATUS_PENDING ) break;" );
            NEW_LINE;
            Wa( "CParamList oResp_;" );
            Wa( "oResp_[ propReturnValue ] = ret;" );
            if( dwOutCount > 0 )
            {
                Wa( "if( SUCCEEDED( ret ) )" );
                BLOCK_OPEN;
                std::vector< std::string > vecArgs;
                ret = GetArgsForCall( pOutArgs, vecArgs );
                if( ERROR( ret ) )
                    break;
                for( guint32 i = 0; i < vecArgs.size(); i++ )
                {
                    CCOUT << "oResp_.Push( "
                        << vecArgs[ i ] << " );";
                    if( i + 1 < vecArgs.size() )
                        NEW_LINE;
                }
                BLOCK_CLOSE;
                NEW_LINE;
            }
            NEW_LINE;
            CCOUT << "this->SetResponse( pCallback,";
            INDENT_UPL;
            CCOUT << "oResp_.GetCfg() );";
            INDENT_DOWNL;
        }
        BLOCK_CLOSE; // do
        Wa( "while( 0 );" );
        NEW_LINE;
        CCOUT << "if( ret != STATUS_PENDING &&";
        INDENT_UPL;
        CCOUT << "!pNewCb.IsEmpty() )";
        INDENT_DOWNL;
        BLOCK_OPEN;
        CCOUT << "CIfRetryTask* pTask = pNewCb;";
        NEW_LINE;
        CCOUT << "pTask->ClearClientNotify();";
        NEW_LINE;
        CCOUT << "( *pNewCb )( eventCancelTask );";
        BLOCK_CLOSE;
        NEW_LINE;
        CCOUT << "return ret;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodSvr::OutputAsyncSerial()
{
    gint32 ret = 0;
    std::string strClass = "I";
    strClass += m_pIf->GetName() + "_SImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    ObjPtr pOutArgs = m_pNode->GetOutArgs();

    guint32 dwInCount = GetArgCount( pInArgs );
    guint32 dwOutCount = GetArgCount( pOutArgs );

    bool bSerial = m_pNode->IsSerialize();
    if( !bSerial )
        return -EINVAL;
    bool bNoReply = m_pNode->IsNoReply();
    do{
        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "Wrapper( ";
        INDENT_UPL;
        if( dwInCount > 0 )
            CCOUT << "IEventSink* pCallback"
                << ", BufPtr& pBuf_ )";
        else
            CCOUT << "IEventSink* pCallback )";

        INDENT_DOWNL;
        BLOCK_OPEN;

        Wa( "gint32 ret = 0;" );
        Wa( "TaskletPtr pNewCb;" );
        CCOUT << "do";
        BLOCK_OPEN;

        guint32 dwTimeoutSec =
            m_pNode->GetTimeoutSec();
        if( dwTimeoutSec == 0 )
        {
            Wa( "guint32 dwTimeoutSec = 0;" );
            Wa( "CCfgOpenerObj oCfg( this);" );
            CCOUT << "ret = oCfg.GetIntProp(";
            INDENT_UPL;
            CCOUT << "propTimeoutSec, dwTimeoutSec );";
            INDENT_DOWNL;
            Wa( "if( ERROR( ret ) ) break;" );
        }
        else
        {
            CCOUT << "guint32 dwTimeoutSec = "
                << dwTimeoutSec << ";";
            NEW_LINE;
        }

        NEW_LINE;
        CCOUT << "CIfInvokeMethodTask* pInv =";
        INDENT_UPL;
        CCOUT << "ObjPtr( pCallback );";
        INDENT_DOWNL;
        CCOUT << "CfgPtr pCallOpt;";
        NEW_LINE;
        CCOUT << "ret = pInv->GetCallOptions( pCallOpt );";
        NEW_LINE;
        Wa( "if( ERROR( ret ) ) break;" );
        NEW_LINE;
        CCOUT << "CCfgOpener oOptions(";
        INDENT_UPL;
        CCOUT << "( IConfigDb* )pCallOpt );";
        INDENT_DOWNL;
        Wa( "oOptions[ propTimeoutSec ] = dwTimeoutSec;" );
        Wa( "oOptions.RemoveProperty( propKeepAliveSec );" );
        CCOUT << "if( dwTimeoutSec > 2 )";
        INDENT_UPL; 
        CCOUT << "oOptions[ propKeepAliveSec ] =";
        INDENT_UPL; 
        CCOUT << "dwTimeoutSec / 2;";
        INDENT_DOWNL;
        INDENT_DOWNL;

        CCOUT << "ret = DEFER_CANCEL_HANDLER2(";
        INDENT_UPL;
        CCOUT << "-1, pNewCb, this,";
        NEW_LINE;
        CCOUT << "&" << strClass << "::"
            << strMethod << "CancelWrapper,";
        NEW_LINE;
        CCOUT << "pCallback, 0, pBuf_ );";
        INDENT_DOWNL;
        NEW_LINE;
        Wa( "if( ERROR( ret ) ) break;" );
        NEW_LINE;

        if( dwInCount > 0 )
        {
            DeclLocals( pInArgs, true );
            ret = GenDeserialArgs(
                pInArgs, "pBuf_",
                false, false, true );
            if( ERROR( ret ) )
                break;
        }

        if( dwOutCount > 0 )
        {
            DeclLocals( pOutArgs );
            NEW_LINE;
        }

        // call the user's handler
        CCOUT << "ret = "
            << strMethod << "(";

        INDENT_UPL;
        CCOUT << "pNewCb,";
        NEW_LINE;
        GenActParams( pInArgs, pOutArgs );

        CCOUT << " );";
        INDENT_DOWNL;
        if( !bNoReply )
        {
            NEW_LINE;
            Wa( "if( ret == STATUS_PENDING ) break;" );
            NEW_LINE;
            Wa( "CParamList oResp_;" );
            Wa( "oResp_[ propReturnValue ] = ret;" );
            if( dwOutCount > 0 )
            {
                Wa( "if( SUCCEEDED( ret ) )" );
                BLOCK_OPEN;

                Wa( "BufPtr pBuf2( true );" );
                ret = GenSerialArgs(
                    pOutArgs, "pBuf2", false,
                    false, true );
                CCOUT << "oResp_.Push( pBuf2 );";
                BLOCK_CLOSE;
                NEW_LINE;
            }
            NEW_LINE;
            CCOUT << "this->SetResponse( pCallback,";
            INDENT_UPL;
            CCOUT << "oResp_.GetCfg() );";
            INDENT_DOWNL;
        }
        BLOCK_CLOSE; // do
        Wa( "while( 0 );" );
        NEW_LINE;
        CCOUT << "if( ret != STATUS_PENDING &&";
        INDENT_UPL;
        CCOUT << "!pNewCb.IsEmpty() )";
        INDENT_DOWNL;
        BLOCK_OPEN;
        CCOUT << "CIfRetryTask* pTask = pNewCb;";
        NEW_LINE;
        CCOUT << "pTask->ClearClientNotify();";
        NEW_LINE;
        CCOUT << "( *pNewCb )( eventCancelTask );";
        BLOCK_CLOSE;
        NEW_LINE;
        CCOUT << "return ret;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodSvr::OutputAsyncCancelWrapper()
{
    gint32 ret = 0;
    std::string strClass = "I";
    strClass += m_pIf->GetName() + "_SImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    guint32 dwInCount = GetArgCount( pInArgs );

    bool bSerial = m_pNode->IsSerialize();

    do{
        Wa( "// this method is called when" );
        Wa( "// timeout happens or user cancels" );
        Wa( "// this pending request" );

        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "CancelWrapper(";
        INDENT_UPL;
        CCOUT << "IEventSink* pCallback,";
        NEW_LINE;
        CCOUT << "gint32 iRet";
        if( bSerial )
        {
            CCOUT << ", BufPtr& pBuf_";
        }
        else if( dwInCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pInArgs );
        }
        CCOUT << " )";
        INDENT_DOWNL;

        BLOCK_OPEN;
        Wa( "gint32 ret = 0;" );

        CCOUT << "do";
        BLOCK_OPEN;
        if( bSerial )
        {
            DeclLocals( pInArgs );
            ret = GenDeserialArgs( pInArgs,
                "pBuf_", false, false, true );
            if( ERROR( ret ) )
                break;
        }

        // call the user's handler
        CCOUT << "On" << strMethod
            << "Canceled(";

        if( dwInCount == 0 )
        {
            CCOUT << " iRet );";
        }
        else
        {
            INDENT_UPL;
            Wa( "iRet," );
            GenActParams( pInArgs );
            CCOUT << " );";
            INDENT_DOWN;
        }

        BLOCK_CLOSE;
        CCOUT << "while( 0 );";
        NEW_LINES( 2 );
        CCOUT << "return ret;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodSvr::OutputAsyncCallback()
{
    gint32 ret = 0;
    std::string strClass = "I";
    strClass += m_pIf->GetName() + "_SImpl";
    std::string strMethod = m_pNode->GetName();

    ObjPtr pInArgs = m_pNode->GetInArgs();
    ObjPtr pOutArgs = m_pNode->GetOutArgs();
    guint32 dwOutCount = GetArgCount( pOutArgs );

    bool bSerial = m_pNode->IsSerialize();
    bool bNoReply = m_pNode->IsNoReply();

    do{
        Wa( "// call me when you have completed" );
        Wa( "// the asynchronous operation" );

        CCOUT << "gint32 " << strClass << "::"
            << strMethod << "Complete( ";
        INDENT_UP;
        NEW_LINE;
        CCOUT << "IEventSink* pCallback"
            << ", gint32 iRet";

        if( dwOutCount > 0 )
        {
            CCOUT << ",";
            NEW_LINE;
            GenFormInArgs( pOutArgs );
        }

        CCOUT << " )";

        INDENT_DOWN;
        NEW_LINE;

        if( bNoReply )
        {
            BLOCK_OPEN;
            Wa( "if( pCallback == nullptr ) return -EINVAL;" );
            Wa( "pCallback->OnEvent( eventTaskComp, iRet, 0, 0 );" );
            CCOUT << "if( SUCCEEDED( iRet ) ) return iRet;";
            NEW_LINE;
            CCOUT << "DebugPrint( iRet,";
            INDENT_UPL;
            CCOUT << "\"'"<< strMethod
                << "' completed with error\" );";
            INDENT_DOWNL;
            CCOUT << "return iRet;";
            BLOCK_CLOSE;
            NEW_LINE;
            break;
        }
        BLOCK_OPEN;
        Wa( "gint32 ret = 0;" );

        CCOUT << "if( iRet == STATUS_PENDING )";
        INDENT_UPL;
        CCOUT << "ret = ERROR_STATE;";
        INDENT_DOWNL;
        CCOUT << "else";
        INDENT_UPL;
        CCOUT << "ret = iRet;";
        INDENT_DOWNL;
        NEW_LINE;
        Wa( "CParamList oResp_;" );
        Wa( "oResp_[ propReturnValue ] = ret;" );

        if( dwOutCount == 0 )
        {
            NEW_LINE;
        }
        else if( bSerial )
        {
            Wa( "if( SUCCEEDED( ret ) )" );
            BLOCK_OPEN;
            Wa( "BufPtr pBuf_( true );" );
            ret = GenSerialArgs(
                pOutArgs, "pBuf_", true, true );
            if( ERROR( ret ) )
                break;
            Wa( "oResp_.Push( pBuf_ );" );
            BLOCK_CLOSE;
            NEW_LINE;
        }
        else
        {
            Wa( "if( SUCCEEDED( ret ) )" );
            BLOCK_OPEN;
            std::vector< std::string > vecArgs;
            ret = GetArgsForCall( pOutArgs, vecArgs );
            if( ERROR( ret ) )
                break;

            for( guint32 i = 0; i < vecArgs.size(); i++ )
            {
                CCOUT << "oResp_.Push( "
                    << vecArgs[ i ] << " );";
                if( i + 1 < vecArgs.size() )
                    NEW_LINE;
            }
            BLOCK_CLOSE;
            NEW_LINE;
        }

        CCOUT << "this->OnServiceComplete( ";
        INDENT_UPL;
        CCOUT << "( IConfigDb* )oResp_.GetCfg(), pCallback );";
        INDENT_DOWNL;
        CCOUT << "return ret;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );
    
    return ret;
}

gint32 CImplIfMethodSvr::OutputAsync()
{
    gint32 ret = 0;

    bool bSerial = m_pNode->IsSerialize();
    if( bSerial )
        ret = OutputAsyncSerial();
    else
        ret = OutputAsyncNonSerial();

    if( ERROR( ret ) )
        return ret;

    return OutputAsyncCancelWrapper();
}

CImplClassFactory::CImplClassFactory(
    CCppWriter* pWriter,
    ObjPtr& pNode,
    bool bServer )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'statements' node" );
        throw std::runtime_error( strMsg );
    }
    m_bServer = bServer;
}

gint32 CImplClassFactory::Output()
{
    if( m_pNode == nullptr )
        return -EFAULT;

    gint32 ret = 0;
    do{
        CStatements* pStmts = m_pNode;

        std::vector< ObjPtr > vecSvcs;
        pStmts->GetSvcDecls( vecSvcs );

        std::vector< ObjPtr > vecStructs;
        pStmts->GetStructDecls( vecStructs );
        std::vector< ObjPtr > vecActStructs;
        for( auto& elem : vecStructs )
        {
            CStructDecl* psd = elem;
            if( psd->RefCount() == 0 )
                continue;
            vecActStructs.push_back( elem );
        }

        NEW_LINE;

        Wa( "FactoryPtr InitClassFactory()" ); 
        BLOCK_OPEN;
        CCOUT << "BEGIN_FACTORY_MAPS;";
        NEW_LINES( 2 );

        for( auto& elem : vecSvcs )
        {
            CServiceDecl* pSvc = elem;
            if( IsServer() )
            {
                CCOUT << "INIT_MAP_ENTRYCFG( ";
                CCOUT << "C" << pSvc->GetName()
                    << "_SvrImpl );";
                NEW_LINE;
            }
            else
            {
                CCOUT << "INIT_MAP_ENTRYCFG( ";
                CCOUT << "C" << pSvc->GetName()
                    << "_CliImpl );";
                NEW_LINE;
            }
        }

        NEW_LINE;
        for( auto& elem : vecActStructs )
        {
            CStructDecl* psd = elem;
            CCOUT << "INIT_MAP_ENTRY( ";
            CCOUT << psd->GetName() << " );";
            NEW_LINE;
        }

        NEW_LINE;
        CCOUT << "END_FACTORY_MAPS;";
        BLOCK_CLOSE;
        NEW_LINES( 2 );

        Wa( "extern \"C\"" );
        Wa( "gint32 DllLoadFactory( FactoryPtr& pFactory )" );
        BLOCK_OPEN;
        Wa( "pFactory = InitClassFactory();" );
        CCOUT << "if( pFactory.IsEmpty() )";
        INDENT_UPL;
        CCOUT << "return -EFAULT;";
        INDENT_DOWNL;
        CCOUT << "return STATUS_SUCCESS;";
        BLOCK_CLOSE;
        NEW_LINE;

    }while( 0 );

    return ret;
}

extern std::string g_strTarget;

CImplMainFunc::CImplMainFunc(
    CCppWriter* pWriter,
    ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
}

gint32 CImplMainFunc::Output()
{
    gint32 ret = 0;

    do{
        CStatements* pStmts = m_pNode;
        std::vector< ObjPtr > vecSvcs;
        pStmts->GetSvcDecls( vecSvcs );
        CServiceDecl* pSvc = vecSvcs[ 0 ];
        std::string strSvcName = pSvc->GetName();
        std::string strModName = g_strTarget;
        std::vector< std::string > vecSuffix =
            { "cli", "svr" };
        for( auto& elem : vecSuffix )
        {
            bool bProxy = ( elem == "cli" );
            std::string strClass =
                std::string( "C" ) + strSvcName;
            if( bProxy )
            {
                strClass += "_CliImpl";
                m_pWriter->SelectMainCli();
            }
            else
            {
                strClass += "_SvrImpl";
                m_pWriter->SelectMainSvr();
            }

            Wa( "// Generated by ridlc" );
            Wa( "#include \"rpc.h\"" );
            Wa( "#include \"proxy.h\"" );
            Wa( "using namespace rpcf;" );
            for( auto elem : vecSvcs )
            {
                CServiceDecl* pSvc = elem;
                if( bProxy )
                {
                    CCOUT << "#include \""
                        << pSvc->GetName() << "cli.h\"";
                }
                else
                {
                    CCOUT << "#include \""
                        << pSvc->GetName() << "svr.h\"";
                }
                NEW_LINE;
            }
            NEW_LINES( 1 );
            Wa( "ObjPtr g_pIoMgr;" );
            NEW_LINE;

            // InitContext
            Wa( "extern FactoryPtr InitClassFactory();" );
            Wa( "gint32 InitContext()" );
            BLOCK_OPEN;
            Wa( "gint32 ret = CoInitialize( 0 );" );
            CCOUT << "if( ERROR( ret ) )";
            INDENT_UPL;
            CCOUT << "return ret;";
            INDENT_DOWNL;

            CCOUT << "do";
            BLOCK_OPEN;
            CCOUT << "// load class factory for '"
                << strModName << "'";
            NEW_LINE;
            CCOUT << "FactoryPtr p = "
                << "InitClassFactory();";
            NEW_LINE;
            CCOUT << "ret = CoAddClassFactory( p );";
            NEW_LINE;
            CCOUT << "if( ERROR( ret ) )";
            INDENT_UPL;
            CCOUT << "break;";
            INDENT_DOWNL;
            NEW_LINE;
            Wa( "CParamList oParams;" );
            CCOUT << "oParams.Push( \""
                << strModName + elem << "\" );";
            NEW_LINES( 2 );

            Wa( "// adjust the thread number if necessary" );
            if( bProxy )
            {
                Wa( "oParams[ propMaxIrpThrd ] = 0;" );
                Wa( "oParams[ propMaxTaskThrd ] = 1;" );
            }
            else
            {
                Wa( "oParams[ propMaxIrpThrd ] = 2;" );
                Wa( "oParams[ propMaxTaskThrd ] = 2;" );
            }
            NEW_LINE;

            CCOUT << "ret = g_pIoMgr.NewObj(";
            INDENT_UPL;
            CCOUT << "clsid( CIoManager ), ";
            NEW_LINE;
            CCOUT << "oParams.GetCfg() );";
            INDENT_DOWNL;
            CCOUT << "if( ERROR( ret ) )";
            INDENT_UPL;
            CCOUT << "break;";
            INDENT_DOWNL;
            NEW_LINE;
            CCOUT << "IService* pSvc = g_pIoMgr;";
            NEW_LINE;
            CCOUT << "ret = pSvc->Start();";
            NEW_LINE;
            BLOCK_CLOSE;
            CCOUT << "while( 0 );";
            NEW_LINES( 2 );
            Wa( "if( ERROR( ret ) )" );
            BLOCK_OPEN;
            CCOUT << "g_pIoMgr.Clear();";
            NEW_LINE;
            CCOUT << "CoUninitialize();";
            BLOCK_CLOSE;
            NEW_LINE;
            CCOUT << "return ret;";
            BLOCK_CLOSE;
            NEW_LINES( 2 );

            // DestroyContext
            Wa( "gint32 DestroyContext()" );
            BLOCK_OPEN;
            Wa( "IService* pSvc = g_pIoMgr;" );
            Wa( "if( pSvc != nullptr )" );
            BLOCK_OPEN;
            Wa( "pSvc->Stop();" );
            CCOUT << "g_pIoMgr.Clear();";
            BLOCK_CLOSE;
            NEW_LINES( 2 );
            Wa( "CoUninitialize();" );

            CCOUT << "DebugPrintEx( logErr, 0,";
            INDENT_UPL;
            CCOUT << "\"#Leaked objects is %d\",";
            NEW_LINE;
            CCOUT << "CObjBase::GetActCount() );";
            INDENT_DOWNL;
            CCOUT << "return STATUS_SUCCESS;";
            BLOCK_CLOSE;
            NEW_LINES( 2 );

            ObjPtr pRoot( m_pNode );
            CImplClassFactory oicf(
                m_pWriter, pRoot, !bProxy );

            ret = oicf.Output();
            if( ERROR( ret ) )
                break;

            NEW_LINE;
            //  business logic
            if( bProxy )
            {
                CCOUT << "gint32 maincli(";
                INDENT_UPL;
                CCOUT << strClass << "* pIf,";
                NEW_LINE;
                CCOUT << "int argc, char** argv );";
                INDENT_DOWNL;
            }
            else
            {
                CCOUT << "gint32 mainsvr( ";
                INDENT_UPL;
                CCOUT << strClass << "* pIf,";
                NEW_LINE;
                CCOUT << "int argc, char** argv );";
                INDENT_DOWNL;
            }
            NEW_LINES( 1 );

            // main function
            Wa( "int main( int argc, char** argv)" );
            BLOCK_OPEN;
            Wa( "gint32 ret = 0;" );
            CCOUT << "do";
            BLOCK_OPEN;
            Wa( "ret = InitContext();" );
            CCOUT << "if( ERROR( ret ) )";
            INDENT_UPL;
            CCOUT << "break;";
            INDENT_DOWNL;
            NEW_LINE;
            Wa( "InterfPtr pIf;" );
            Wa( "CParamList oParams;" );
            Wa( "oParams[ propIoMgr ] = g_pIoMgr;" );
            CCOUT << "ret = CRpcServices::LoadObjDesc(";
            INDENT_UPL;
            CCOUT << "\"./"
                << g_strAppName << "desc.json\",";
            NEW_LINE;
            CCOUT << "\"" << strSvcName << "\",";
            NEW_LINE;
            if( bProxy )
                CCOUT << "false, oParams.GetCfg() );";
            else
                CCOUT << "true, oParams.GetCfg() );";

            INDENT_DOWNL;

            CCOUT << "if( ERROR( ret ) )";
            INDENT_UPL;
            CCOUT << "break;";
            INDENT_DOWNL;
            NEW_LINE;

            CCOUT << "ret = pIf.NewObj(";
            INDENT_UPL;
            CCOUT << "clsid( " << strClass << " ),";
            NEW_LINE;
            CCOUT << "oParams.GetCfg() );";
            INDENT_DOWNL;

            CCOUT << "if( ERROR( ret ) )";
            INDENT_UPL;
            CCOUT << "break;";
            INDENT_DOWNL;
            NEW_LINE;
            CCOUT << strClass << "* pSvc = pIf;";
            NEW_LINE;
            CCOUT << "ret = pSvc->Start();";
            NEW_LINE;

            CCOUT << "if( ERROR( ret ) )";
            INDENT_UPL;
            CCOUT << "break;";
            INDENT_DOWNL;

            if( !bProxy )
            {
                NEW_LINE;
                Wa( "mainsvr( pSvc, argc, argv );" );
            }
            else
            {
                NEW_LINE;
                CCOUT << "while( pSvc->GetState()"
                    << "== stateRecovery )";
                INDENT_UPL;
                CCOUT << "sleep( 1 );";
                INDENT_DOWNL;
                NEW_LINE;
                CCOUT << "if( pSvc->GetState() != "
                    << "stateConnected )";
                NEW_LINE;
                BLOCK_OPEN;
                CCOUT << "ret = ERROR_STATE;";
                NEW_LINE;
                CCOUT << "break;";
                BLOCK_CLOSE;
                NEW_LINES( 2 );
                Wa( "maincli( pSvc, argc, argv );" );
            }

            NEW_LINE;
            Wa( "// Stopping the object" );
            CCOUT << "ret = pSvc->Stop();";
            NEW_LINE;
            BLOCK_CLOSE;
            CCOUT<< "while( 0 );";
            NEW_LINES( 2 );
            Wa( "DestroyContext();" );
            CCOUT << "return ret;";
            BLOCK_CLOSE;
            NEW_LINES(2);

            Wa( "//-----Your code begins here---" );
            //  business logic
            if( bProxy )
            {
                CCOUT << "gint32 maincli(";
                INDENT_UPL;
                CCOUT << strClass << "* pIf,";
                NEW_LINE;
                CCOUT << "int argc, char** argv )";
                INDENT_DOWNL;
            }
            else
            {
                CCOUT << "gint32 mainsvr( ";
                INDENT_UPL;
                CCOUT << strClass << "* pIf,";
                NEW_LINE;
                CCOUT << "int argc, char** argv )";
                INDENT_DOWNL;
            }
            BLOCK_OPEN;
            if( bProxy )
            {
                Wa( "//-----Adding your code here---" );
            }
            else
            {
                CCOUT << "// replace 'sleep' with your code for";
                NEW_LINE;
                CCOUT << "// advanced control";
                NEW_LINE;
                CCOUT << "while( pIf->IsConnected() )";
                INDENT_UPL;
                CCOUT << "sleep( 1 );";
                INDENT_DOWNL;
            }
            CCOUT << "return STATUS_SUCCESS;";
            BLOCK_CLOSE;
            NEW_LINES( 2 );
        }

    }while( 0 );

    return ret;
}

CExportBase::CExportBase(
    CCppWriter* pWriter,
    ObjPtr& pNode )
{
    m_pWriter = pWriter;
    m_pNode = pNode;
    if( m_pNode == nullptr )
    {
        std::string strMsg = DebugMsg(
            -EFAULT, "internal error empty "
            "'statements' node" );
        throw std::runtime_error( strMsg );
    }
}

gint32 CExportBase::Output()
{
    gint32 ret = 0;
    do{
        STMIPTR pstm( new std::ifstream(
            m_strFile, std::ifstream::in ) );

        if( !pstm->good() )
        {
            std::string strPath;
            ret = FindInstCfg(
                m_strFile, strPath );

            if( ERROR( ret ) )
                break;

            pstm->open(
                strPath, std::ifstream::in );

            if( !pstm->good() )
            {
                ret = -errno;
                break;
            }
        }

        BufPtr pBuf( true );
        pBuf->Resize( PAGE_SIZE );
        pstm->seekg( 0, pstm->end );
        guint32 iLen = pstm->tellg();
        pstm->seekg( 0, pstm->beg );
        while( iLen > 0 )
        {
            guint32 dwCount =
                std::min( iLen, pBuf->size() );
            pstm->read( pBuf->ptr(), dwCount );
            m_pWriter->m_curFp->write(
                pBuf->ptr(), dwCount );
            iLen -= dwCount;
            if( !m_pWriter->m_curFp->good() )
            {
                ret = -errno;
                break;
            }
        }

    }while( 0 );

    if( ERROR( ret ) )
    {
        std::string strMsg = strerror( -ret );
        DebugPrintEx( logErr, ret,
            "error open file `%s', %s\n", 
            m_strFile, strMsg.c_str() );
    }

    return ret;
}

CExportMakefile::CExportMakefile(
    CCppWriter* pWriter,
    ObjPtr& pNode )
    : super( pWriter, pNode )
{ m_strFile = "./mktpl"; }

gint32 CExportMakefile::Output()
{
    gint32 ret = 0;
    do{
        ret = super::Output();
        if( ERROR( ret ) )
            break;

        m_pWriter->m_curFp->flush();
        m_pWriter->m_curFp->close();

        std::vector< ObjPtr > vecSvcs;
        ret = m_pNode->GetSvcDecls( vecSvcs );
        if( ERROR( ret ) )
            break;

        std::string strAppName =
            m_pNode->GetName();

        std::string strCpps =
            strAppName + ".cpp ";

        std::string strClient =
            strAppName + "cli";

        std::string strObjClient, strObjServer;
        for( auto& elem : vecSvcs )
        {
            CServiceDecl* psd = elem;
            if( psd == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            strObjClient +=
                std::string( "$(OBJ_DIR)/" ) +
                psd->GetName() + "cli.o ";

            strObjServer +=
                std::string( "$(OBJ_DIR)/" ) +
                psd->GetName() + "svr.o ";
        }

        std::string strServer =
            strAppName + "svr";

        std::string strCmdLine =
            "sed -i 's:XXXSRCS:";

        strCmdLine += strCpps + ":;" +
            "s:XXXCLI:" + strClient + ":;" +
            "s:XXXSVR:" + strServer + ":; " +
            "s:XXXOBJSSVR:" + strObjServer + ":; " +
            "s:XXXOBJSCLI:" + strObjClient + ":' " +
            m_pWriter->m_pFiles->m_strMakefile;

        printf( "%s\n", strCmdLine.c_str() );
        system( strCmdLine.c_str() );

    }while( 0 );

    if( !m_pWriter->m_curFp->is_open() )
    {
        // keep current file open
        m_pWriter->m_curFp->open(
            m_pWriter->m_pFiles->m_strMakefile,
            std::ofstream::out |
            std::ofstream::app);
    }

    return ret;
}

CExportDrivers::CExportDrivers(
    CCppWriter* pWriter,
    ObjPtr& pNode )
    : super( pWriter, pNode )
{ m_strFile = "./drvtpl.json"; }

#include "jsondef.h"
#include "frmwrk.h"

gint32 CExportDrivers::Output()
{
    gint32 ret = 0;
    do{
        ret = super::Output();
        if( ERROR( ret ) )
            break;

        m_pWriter->m_curFp->flush();
        m_pWriter->m_curFp->close();

        Json::Value oVal;
        ret = ReadJsonCfg(
            m_pWriter->m_strCurFile,
            oVal );
        if( ERROR( ret ) )
            break;

        m_pWriter->m_curFp->open(
            m_pWriter->m_strCurFile,
            std::ofstream::out |
            std::ofstream::trunc);
        if( !m_pWriter->m_curFp->good() )
        {
            ret = -errno;
            break;
        }

        std::string strAppName =
            m_pNode->GetName();

        std::vector< ObjPtr > vecSvcs;
        ret = m_pNode->GetSvcDecls( vecSvcs );
        if( ERROR( ret ) )
            break;

        bool bStream = false;
        for( auto& elem : vecSvcs )
        {
            CServiceDecl* psd = elem;
            if( psd == nullptr )
                continue;
            if( psd->IsStream() )
            {
                bStream = true;
                break;
            }
        }

        
        std::string strAppCli =
            strAppName + "cli";

        std::string strAppSvr =
            strAppName + "svr";

        Json::Value& oModuleArray =
            oVal[ JSON_ATTR_MODULES ];

        if( oModuleArray == Json::Value::null )
        {
            ret = -EINVAL;
        }

        if( !oModuleArray.isArray() )
        {
            ret = -EINVAL;
            break;
        }

        Json::Value oCli;
        oCli[ JSON_ATTR_MODNAME ] = strAppCli;
        Json::Value oDrvToLoad;
        oDrvToLoad.append( "DBusBusDriver" );
        if( bStream )
            oDrvToLoad.append( "UnixSockBusDriver" );
        oCli[ JSON_ATTR_DRVTOLOAD ] = oDrvToLoad;

        Json::Value oSvr = oCli;
        oSvr[ JSON_ATTR_MODNAME ] = strAppSvr;
        oModuleArray.append( oSvr );
        oModuleArray.append( oCli );

        Json::StreamWriterBuilder oBuilder;
        oBuilder["commentStyle"] = "None";
        oBuilder["indentation"] = "    ";
        std::unique_ptr<Json::StreamWriter> pWriter(
            oBuilder.newStreamWriter() );
        pWriter->write( oVal, m_pWriter->m_curFp );
        m_pWriter->m_curFp->flush();

    }while( 0 );

    return ret;
}

CExportObjDesc::CExportObjDesc(
    CCppWriter* pWriter,
    ObjPtr& pNode )
    : super( pWriter, pNode )
{ m_strFile = "./odesctpl.json"; }

gint32 CExportObjDesc::Output()
{
    gint32 ret = 0;
    do{
        ret = super::Output();
        if( ERROR( ret ) )
            break;

        m_pWriter->m_curFp->flush();
        m_pWriter->m_curFp->close();

        Json::Value oVal;
        ret = ReadJsonCfg(
            m_pWriter->m_strCurFile,
            oVal );
        if( ERROR( ret ) )
            break;

        m_pWriter->m_curFp->open(
            m_pWriter->m_strCurFile,
            std::ofstream::out |
            std::ofstream::trunc);
        if( !m_pWriter->m_curFp->good() )
        {
            ret = -errno;
            break;
        }

        std::string strAppName =
            m_pNode->GetName();

        oVal[ JSON_ATTR_SVRNAME ] = strAppName;

        Json::Value oElemTmpl;

        // get object array
        Json::Value& oObjArray =
            oVal[ JSON_ATTR_OBJARR ];

        if( oObjArray == Json::Value::null ||
            !oObjArray.isArray() ||
            oObjArray.empty() )
        {
            ret = -ENOENT;
            break;
        }

        oElemTmpl = oObjArray[ 0 ];
        oObjArray.clear();

        std::vector< ObjPtr > vecSvcs;
        ret = m_pNode->GetSvcDecls( vecSvcs );
        if( ERROR( ret ) )
            break;

        for( auto& elem : vecSvcs )
        {
            CServiceDecl* psd = elem;
            if( psd == nullptr )
                continue;

            Json::Value oElem = oElemTmpl;
            ret = BuildObjDesc( psd, oElem );
            if( ERROR( ret ) )
                break;
            oObjArray.append( oElem );
        }

        if( ERROR( ret ) )
            break;

        Json::StreamWriterBuilder oBuilder;
        oBuilder["commentStyle"] = "None";
        oBuilder["indentation"] = "    ";
        std::unique_ptr<Json::StreamWriter> pWriter(
            oBuilder.newStreamWriter() );
        pWriter->write( oVal, m_pWriter->m_curFp );
        m_pWriter->m_curFp->flush();

    }while( 0 );

    return ret;
}

gint32 CExportObjDesc::BuildObjDesc(
    CServiceDecl* psd, Json::Value& oElem )
{
    if( psd == nullptr )
        return -EINVAL;

    gint32 ret = 0;
    do{
        std::string strSvcName =
            psd->GetName();

        oElem[ JSON_ATTR_OBJNAME ] = strSvcName;

        guint32 dwVal = 0;
        ret = psd->GetTimeoutSec( dwVal );
        if( SUCCEEDED( ret ) )
        {
            oElem[ JSON_ATTR_REQ_TIMEOUT ] =
                std::to_string( dwVal );
            oElem[ JSON_ATTR_KA_TIMEOUT ] =
                std::to_string( dwVal / 2 );
        }

        std::string strVal;
        ret = psd->GetIpAddr( strVal );
        if( SUCCEEDED( ret ) )
            oElem[ JSON_ATTR_IPADDR ] = strVal;

        ret = psd->GetPortNum( dwVal );
        if( SUCCEEDED( ret ) )
            oElem[ JSON_ATTR_TCPPORT ] =
                std::to_string( dwVal / 2 );

        ret = psd->GetRouterPath( strVal );
        if( SUCCEEDED( ret ) )
            oElem[ JSON_ATTR_ROUTER_PATH ] =
                strVal;

        BufPtr pAuth;
        ret = psd->GetAuthVal( pAuth );
        if( SUCCEEDED( ret ) )
        {
            Json::CharReaderBuilder oBuilder;
            std::unique_ptr< Json::CharReader >
                pReader( oBuilder.newCharReader() );

            if( pReader == nullptr )
            {
                ret = -EFAULT;
                break;
            }
            Json::Value oAuth;
            std::string strMsg;
            if( !pReader->parse( pAuth->ptr(),
                pAuth->ptr() + pAuth->size(),
                &oAuth, &strMsg ) )
            {
                DebugPrintEx( logErr, ret,
                    "error, %s\n", strMsg.c_str() );
                ret = -EBADMSG;
                break;
            }
            oElem[ JSON_ATTR_AUTHINFO ] = oAuth;
        }

        if( psd->IsWebSocket() )
            oElem[ JSON_ATTR_ENABLE_WEBSOCKET ] =
                "true";
        else
            oElem[ JSON_ATTR_ENABLE_WEBSOCKET ] =
                "false";

        if( psd->IsCompress() )
            oElem[ JSON_ATTR_ENABLE_COMPRESS ] =
                "true";
        else
            oElem[ JSON_ATTR_ENABLE_COMPRESS ] =
                "false";

        if( psd->IsSSL() )
            oElem[ JSON_ATTR_ENABLE_SSL ] =
                "true";
        else
            oElem[ JSON_ATTR_ENABLE_SSL ] =
                "false";

        ret = 0;

        // get interface array
        Json::Value& oIfArray =
            oElem[ JSON_ATTR_IFARR ];

        if( oIfArray == Json::Value::null ||
            !oIfArray.isArray() ||
            oIfArray.empty() )
        {
            ret = -ENOENT;
            break;
        }

        guint32 i;
        for( i = 0; i < oIfArray.size(); i++ )
        {
            if( !oIfArray[ i ].isMember(
                JSON_ATTR_IFNAME ) )
                continue;
            Json::Value& oIfElem = oIfArray[ i ];
            if( oIfElem[ JSON_ATTR_IFNAME ]
                == "XXXX" )
            {
                std::string strMainIf = "C";
                strMainIf += strSvcName + "_SvrImpl";
                oIfElem[ JSON_ATTR_IFNAME ] =
                    strMainIf;
                oIfElem[ JSON_ATTR_BIND_CLSID ] =
                    "true";
                oIfElem[ JSON_ATTR_DUMMYIF ] =
                    "true";
                break;
            }
        }

        if( psd->IsStream() )
        {
            Json::Value oJif;
            oJif[ JSON_ATTR_IFNAME ] =
                "IStream";
            oIfArray.append( oJif );
        }

        std::vector< ObjPtr > vecIfs;
        ret = psd->GetIfRefs( vecIfs );
        if( ERROR( ret ) )
            break;

        for( auto& pIf : vecIfs )
        {
            CInterfRef* pifr = pIf;
            if( pifr == nullptr )
                continue;

            Json::Value oJif;
            oJif[ JSON_ATTR_IFNAME ] =
                pifr->GetName();
            oIfArray.append( oJif );
        }

    }while( 0 );

    return ret;
}
