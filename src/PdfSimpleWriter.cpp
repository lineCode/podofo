/***************************************************************************
 *   Copyright (C) 2005 by Dominik Seichter                                *
 *   domseichter@web.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "PdfSimpleWriter.h"

#include "PdfDate.h"
#include "PdfFont.h"
#include "PdfFontMetrics.h"
#include "PdfObject.h"
#include "PdfPage.h"
#include "PdfImage.h"

#ifndef _WIN32
#include <fontconfig.h>
#endif

#include <algorithm>
#include <sstream>

namespace PoDoFo {

using namespace std;

class FontComperator { 
public:
    FontComperator( const string & sPath )
        {
            m_sPath = sPath;
        }
    
    bool operator()(const PdfFont* pFont) 
        { 
            return (m_sPath == pFont->FontMetrics()->Filename());
        }
private:
    string m_sPath;
};


PdfSimpleWriter::PdfSimpleWriter()
{
    m_pPageTree     = NULL;
    m_nPageTreeSize = 0;
    m_bInitDone     = false;

#ifndef _WIN32
    m_pFcConfig     = (void*)FcInitLoadConfigAndFonts();
#endif
}    

PdfSimpleWriter::~PdfSimpleWriter()
{
#ifndef _WIN32
    FcConfigDestroy( (FcConfig*)m_pFcConfig );
#endif

    if( m_bInitDone )    
        FT_Done_FreeType( m_ftLibrary );
}

PdfError PdfSimpleWriter::Init()
{
    PdfError  eCode;
    PdfDate   cDate;
    PdfString sDate;

    m_bInitDone = true;
    if( FT_Init_FreeType( &m_ftLibrary ) )
    {
        RAISE_ERROR( ePdfError_FreeType );
    }

    SAFE_OP( PdfWriter::Init() );

    m_pPageTree = m_vecObjects.CreateObject( "Pages" );
    m_pPageTree->AddKey( "Kids", "[ ]" );

    this->GetCatalog()->AddKey( "Pages", m_pPageTree->Reference().ToString().c_str() );

    cDate.ToString( sDate );
    this->GetInfo()->AddKey( "Producer", PdfString("PoDoFo") );
    this->GetInfo()->AddKey( "CreationDate", sDate );

    return eCode;
}

PdfPage* PdfSimpleWriter::CreatePage( const TSize & tSize )
{
    ostringstream    oStream;
    PdfVariant       var;
    TVariantList     array;

    TCIReferenceList it;
    PdfPage*         pPage    = m_vecObjects.CreateObject<PdfPage>();


    m_vecPageReferences.push_back( pPage->Reference() );

    oStream << ++m_nPageTreeSize;

    var.SetDataType( ePdfDataType_Reference );

    it = m_vecPageReferences.begin();
    while( it != m_vecPageReferences.end()  )
    {
        var.SetReference( *it );
        array.push_back( var );
        ++it;
    }

    var.SetDataType( ePdfDataType_Array );
    var.SetArray( array );

    m_pPageTree->AddKey( "Count", oStream.str().c_str() );
    m_pPageTree->AddKey( "Kids",  var );

    pPage->AddKey( "Parent", m_pPageTree->Reference() );
    if( pPage->Init( tSize, &m_vecObjects ).IsError() )
    {
        delete pPage;
        return NULL;
    }

    return pPage;
}

PdfFont* PdfSimpleWriter::CreateFont( const char* pszFontName, bool bEmbedd )
{
    PdfError          eCode;
#ifdef _WIN32
    std::string       sPath = PdfFontMetrics::GetFilenameForFont( pszFontName );
#else
    std::string       sPath = PdfFontMetrics::GetFilenameForFont( (FcConfig*)m_pFcConfig, pszFontName );
#endif
    PdfFont*          pFont;
    PdfFontMetrics*   pMetrics;
    TCISortedFontList it;

    if( sPath.empty() )
    {
        PdfError::LogMessage( eLogSeverity_Critical, "No path was found for the specified fontname: %s\n", pszFontName );
        return NULL;
    }

    it = std::find_if( m_vecFonts.begin(), m_vecFonts.end(), FontComperator( sPath ) );

    if( it == m_vecFonts.end() )
    {
        pMetrics = new PdfFontMetrics( &m_ftLibrary, sPath.c_str() );
        pFont    = m_vecObjects.CreateObject<PdfFont>();

        m_vecFonts  .push_back( pFont );

        // Now sort the font list
        std::sort( m_vecFonts.begin(), m_vecFonts.end() );

        eCode = pFont->Init( pMetrics, &m_vecObjects, bEmbedd );
        if( eCode.IsError() )
        {
            eCode.PrintErrorMsg();
            PdfError::LogMessage( eLogSeverity_Error, "Cannot initialize font: %s\n", pszFontName );
            return NULL;
        }
    }
    else
        pFont = *it;

    return pFont;
}

PdfImage* PdfSimpleWriter::CreateImage()
{
    return m_vecObjects.CreateObject<PdfImage>();
}

void PdfSimpleWriter::SetDocumentAuthor( const PdfString & sAuthor )
{
    this->GetInfo()->AddKey( "Author", sAuthor );
}

void PdfSimpleWriter::SetDocumentCreator( const PdfString & sCreator )
{
    this->GetInfo()->AddKey( "Creator", sCreator );
}

void PdfSimpleWriter::SetDocumentKeywords( const PdfString & sKeywords )
{
    this->GetInfo()->AddKey( "Keywords", sKeywords );
}

void PdfSimpleWriter::SetDocumentSubject( const PdfString & sSubject )
{
    this->GetInfo()->AddKey( "Subject", sSubject );
}

void PdfSimpleWriter::SetDocumentTitle( const PdfString & sTitle )
{
    this->GetInfo()->AddKey( "Title", sTitle );
}

}; 