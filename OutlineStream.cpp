/*
* Copyright 2008-2017 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the CrossLine outliner Oln2 library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "OutlineStream.h"
#include "OutlineItem.h"
#include <cassert>
#include <QtDebug>
using namespace Oln;
using namespace Stream;

const char* OutlineStream::s_olnFrameTag = "oln";

void OutlineStream::writeTo( DataWriter & out, const Udb::Obj &oln )
{
	out.startFrame( NameTag( s_olnFrameTag ) );
	DataCell v;
	v = oln.getValue( OutlineItem::AttrIsTitle );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "titl" ) );
	v = oln.getValue( OutlineItem::AttrIsReadOnly );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "ro" ) );
	v = oln.getValue( OutlineItem::AttrIsExpanded );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "exp" ) );
	v = oln.getValue( OutlineItem::AttrIdent );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "id" ) );
	v = oln.getValue( OutlineItem::AttrAltIdent );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "aid" ) );
	v = oln.getValue( OutlineItem::AttrAlias );
	bool writeText = true;
	// Wenn das Item ein Alias ist, schreiben wir vorsorglich den referenzierten Text auch raus.
	if( v.hasValue() )
	{
		Udb::Obj o = oln.getObject( v.getOid() );
		if( !o.isNull() )
		{
			out.writeSlot( DataCell().setUuid( o.getUuid() ), NameTag( "ali" ) ); 
			v = o.getValue( OutlineItem::AttrText );
			if( v.hasValue() )
			{
				out.writeSlot( v, NameTag( "text" ), true ); 
				writeText = false;
			}
		}
	}
	if( writeText )
	{
		v = oln.getValue( OutlineItem::AttrText );
		if( v.hasValue() )
			out.writeSlot( v, NameTag( "text" ), true ); 
	}
	Udb::Obj sub = oln.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == OutlineItem::TID )
			writeTo( out, sub );
	}while( sub.next() );
	out.endFrame();
}

static void _setHome( Udb::Obj& o, const Udb::Obj& home )
{
	Udb::Obj sub = o.getFirstObj();
	if( !sub.isNull() ) do
	{
		sub.setValue( OutlineItem::AttrHome, home );
		_setHome( sub, home );
	}while( sub.next() );
}

Udb::Obj OutlineStream::readFrom( DataReader &in, Udb::Transaction *txn, DataCell::OID home, bool nextToken )
{
	assert( txn != 0 );
    d_error.clear();
    DataReader::Token t = in.getCurrentToken();
    if( nextToken )
            t = in.nextToken();
	if( t != DataReader::BeginFrame || !in.getName().getTag().equals( s_olnFrameTag ) )
	{
		d_error = QLatin1String( "expecting oln frame" );
		return Udb::Obj();
	}
	Udb::Obj oln = readObj( in, txn, home );
	if( home == 0 ) // Wenn kein äusseres Home, dann ist oln ein eigenständiges Objekt
	{
		_setHome( oln, oln );
		oln.setType( 0 );
		Outline::markHasItems(oln);
	}else
	{
		Udb::Obj h = oln.getObject(home);
		Outline::markHasItems(h);
	}
    return oln;
}

Udb::Obj OutlineStream::readObj( DataReader & in, Udb::Transaction *txn, DataCell::OID home )
{
	DataReader::Token t = in.nextToken();
	Udb::Obj oln = txn->createObject( OutlineItem::TID );
	oln.setValue( OutlineItem::AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
	if( home )
		oln.setValue( OutlineItem::AttrHome, DataCell().setOid( home ) );
	while( DataReader::isUseful( t ) )
	{
		switch( t )
		{
		case DataReader::Slot:
			{
				const NameTag name = in.getName().getTag();
				if( name.equals( "text" ) )
				{
					// Wenn das Alias bekannt ist, brauchen wir keinen lokalen Text.
					DataCell v = oln.getValue( OutlineItem::AttrAlias );
					if( !v.hasValue() || v.getOid() == 0 )
						oln.setValue( OutlineItem::AttrText, in.readValue() );
				}else if( name.equals( "titl" ) )
					oln.setValue( OutlineItem::AttrIsTitle, in.readValue() );
				else if( name.equals( "ro" ) )
					oln.setValue( OutlineItem::AttrIsReadOnly, in.readValue() );
				else if( name.equals( "id" ) )
					oln.setValue( OutlineItem::AttrIdent, in.readValue() );
				else if( name.equals( "aid" ) )
					oln.setValue( OutlineItem::AttrAltIdent, in.readValue() );
				else if( name.equals( "exp" ) )
					oln.setValue( OutlineItem::AttrIsExpanded, in.readValue() );
				else if( name.equals( "ali" ) )
				{
					// Falls Objekt nicht bekannt, wird gleichwohl ein Null-Alias eingefügt.
					Udb::Obj o = txn->getObject( in.readValue() );
					oln.setValue( OutlineItem::AttrAlias, o );
					// Wenn das Alias bekannt ist, brauchen wir keinen lokalen Text.
					if( !o.isNull() )
						oln.setValue( OutlineItem::AttrText, DataCell().setNull() );
				}
				else
				{
					//d_error = QLatin1String( "unexpected slot" );
					//return Udb::Obj();
					qWarning() << "OutlineStream::readObj unexpected slot " << name.toString();
				}
			}
			break;
		case DataReader::BeginFrame:
			if( !in.getName().getTag().equals( s_olnFrameTag ) )
			{
				d_error = QLatin1String( "unexpected child frame" );
				return Udb::Obj();
			}else
			{
				Udb::Obj sub = readObj( in, txn, home );
				if( sub.isNull() )
					return Udb::Obj();
				else
					sub.aggregateTo( oln );
			}
			break;
		case DataReader::EndFrame:
			return oln; // Ok, gutes Ende
        default:
            Q_ASSERT(false);
            break;
		}
		t =in.nextToken();
	}
	d_error = QLatin1String( "unexpected end of stream" );
	return Udb::Obj();
}
