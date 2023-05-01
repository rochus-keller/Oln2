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

#include "OutlineUdbStream.h"
#include "OutlineItem.h"
#include "LinkSupport.h"
#include <Udb/Database.h>
#include <cassert>
#include <QtDebug>
using namespace Oln;
using namespace Stream;

const quint32 OutlineUdbStream::s_magic = 1489782925; // Zufallszahl zur Unterstützung der Formatwiedererkennung

void OutlineUdbStream::writeOutline(DataWriter & out, const Udb::Obj &outline, bool wh)
{
	if( wh )
		writeHeader( out, outline );
	bool writeDbId = true;
	Udb::Obj item = outline.getFirstObj();
	if( !item.isNull() ) do
	{
		if( item.getType() == OutlineItem::TID )
		{
			writeItem( out, item, writeDbId, true );
			writeDbId = false;
		}
	}while( item.next() );
}

void OutlineUdbStream::writeHeader(DataWriter & out, const Udb::Obj& outline)
{
	out.writeSlot( DataCell().setTag( NameTag("olns") ) );
	out.writeSlot( DataCell().setUInt32( s_magic ) );
	out.writeSlot( DataCell().setUInt16(2) );
	out.writeSlot( DataCell().setDateTime( QDateTime::currentDateTime() ) );
	out.writeSlot( outline.getValue( Udb::ContentObject::AttrText ) );
	out.writeSlot( outline.getValue( Udb::ContentObject::AttrIdent ) );
	out.writeSlot( outline.getValue( Udb::ContentObject::AttrAltIdent ) );
	out.writeSlot( outline.getValue( Udb::ContentObject::AttrCreatedOn ) );
	out.writeSlot( outline.getValue( Udb::ContentObject::AttrModifiedOn ) );
}

void OutlineUdbStream::writeItem( DataWriter & out, Udb::Obj item, bool writeDbId, bool recursive )
{
	if( item.isNull() )
		return;

	out.startFrame( NameTag( "oln" ) );

	if( writeDbId )
		out.writeSlot( DataCell().setUuid( item.getDb()->getDbUuid() ), NameTag("dbid") );

	out.writeSlot( item, NameTag("oid") );

	DataCell v;
	v = item.getValue( OutlineItem::AttrIsTitle );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "titl" ) );
	v = item.getValue( OutlineItem::AttrIsReadOnly );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "ro" ) );
	v = item.getValue( OutlineItem::AttrIsExpanded );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "exp" ) );
	v = item.getValue( OutlineItem::AttrIdent );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "id" ) );
	v = item.getValue( OutlineItem::AttrAltIdent );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "aid" ) );

	Udb::Obj alias = item.getValueAsObj( OutlineItem::AttrAlias );
	if( !alias.isNull() )
		out.writeSlot( alias, NameTag( "ali" ) ); // verwendet id, aid und text von alias, falls vorhanden
	else
		alias = item;

	v = alias.getValue( OutlineItem::AttrText );
	if( v.hasValue() )
		out.writeSlot( v, NameTag( "text" ), true );

	if( recursive )
	{
		Udb::Obj sub = item.getFirstObj();
		if( !sub.isNull() ) do
		{
			if( sub.getType() == OutlineItem::TID )
				writeItem( out, sub, false, recursive );
		}while( sub.next() );
	}
	out.endFrame();
}

QByteArray OutlineUdbStream::readOutline( DataReader &in, Udb::Obj outline, bool readHeader ) throw()
{
	Q_ASSERT( !outline.isNull() );

	if( readHeader )
	{
		if( in.nextToken() != DataReader::Slot || in.getValue().getTag() != NameTag("olns") )
			return "Not a Outline stream";
		if( in.nextToken() != DataReader::Slot || in.getValue().getUInt32() != s_magic )
			return "Not a Outline stream";
		if( in.nextToken() != DataReader::Slot || in.getValue().getUInt16() != 2 )
			return "Wrong version";
		if( in.nextToken() != DataReader::Slot || !in.getValue().isDateTime() )
			return "Invalid format";
		if( in.nextToken() != DataReader::Slot )
			return "Invalid format";
		outline.setValue( Udb::ContentObject::AttrText, in.getValue() );
		if( in.nextToken() != DataReader::Slot )
			return "Invalid format";
		outline.setValue( Udb::ContentObject::AttrIdent, in.getValue() );
		if( in.nextToken() != DataReader::Slot )
			return "Invalid format";
		outline.setValue( Udb::ContentObject::AttrAltIdent, in.getValue() );
		if( in.nextToken() != DataReader::Slot || !in.getValue().isDateTime() )
			return "Invalid format";
		const QDateTime createdOn = in.getValue().getDateTime();
		if( in.nextToken() != DataReader::Slot )
			return "Invalid format";
		DataCell dt;
		if( in.getValue().isDateTime() )
			dt = in.getValue();
		else
			dt.setDateTime(createdOn);
		outline.setValue( Udb::ContentObject::AttrModifiedOn, dt );
	}
	return readItems( in, outline, Udb::Obj() );
}

QByteArray OutlineUdbStream::readItems(DataReader & in, Udb::Obj parent, const Udb::Obj &before ) throw()
{
	Udb::Obj outline = parent;
	if( parent.getType() == OutlineItem::TID )
		outline = parent.getValueAsObj( OutlineItem::AttrHome );
	Q_ASSERT( !outline.isNull() );

	OidMap oidMap;
	QUuid dbid;
	try
	{
		while( in.nextToken() == DataReader::BeginFrame )
		{
			if( !in.getName().getTag().equals( "oln" ) )
				return "Invalid format";
			const QUuid uuid = readOlnFrame( in, parent, outline, before, oidMap );
			if( !uuid.isNull() )
				dbid = uuid;
		}
	}catch( const Exception& e )
	{
		return e.d_msg;
	}
	if( DataReader::isUseful( in.nextToken() ) )
		return "Invalid format";
	else if( !remapRefs( outline, dbid, oidMap ) )
		return "Invalid references or format";
	else
		return QByteArray(); // no errors
	Outline::markHasItems( outline );
}

bool OutlineUdbStream::remapRefs(const Udb::Obj &home, const QUuid& dbid, const OidMap & oidMap )
{
	const QUuid thisDb = home.getDb()->getDbUuid();

	OidMap::const_iterator i, j;
	for( i = oidMap.begin(); i != oidMap.end(); ++i )
	{
		OutlineItem item = home.getObject( i.value() );
		const Udb::OID foreignOid = item.getValue( OutlineItem::AttrAlias ).getUInt64();
		if( foreignOid != 0 )
		{
			Udb::OID newOid = 0;
			j = oidMap.find( foreignOid );
			if( j != oidMap.end() )
				// Das Alias zeigt auf ein Objekt im selben Dokument; wir mappen darauf
				newOid = j.value();
			else if( thisDb == dbid )
			{
				// Wir sind in derselben DB; die OID sollte also bekannt sein
				Udb::Obj o = home.getObject(foreignOid);
				if( !o.isNull(true,true) )
					newOid = foreignOid;
			}
			if( newOid )
			{
				item.setValue( OutlineItem::AttrAlias, DataCell().setOid( newOid ) );
				item.clearValue( OutlineItem::AttrText );
			}else
				item.clearValue( OutlineItem::AttrAlias );
		}// ansonsten wurde AttrAlias bereits richtig gesetzt oder ist leer
		DataCell v = item.getValue( OutlineItem::AttrText );
		if( v.isBml() )
		{
			DataReader in( v );
			DataWriter out;
			while( DataReader::isUseful( in.nextToken() ) )
			{
				const DataCell& name = in.getName();
				switch( in.getCurrentToken() )
				{
				case DataReader::BeginFrame:
					if( !name.isTag() )
						return false; // in rtxt gibt es nur Frames mit Tag
					out.startFrame(name.getTag());
					break;
				case DataReader::EndFrame:
					out.endFrame();
					break;
				case DataReader::Slot:
					if( name.isNull() )
						out.writeSlot( in.getValue() );
					else if( name.isTag() )
					{
						DataCell v = in.getValue();
						if( name.getTag().equals("link") )
						{
							Link l;
							if( !l.readFrom( in.getValue().getArr() ) )
								return false; // ungültiges Link-Format
							if( l.d_db.isNull() || l.d_db == dbid )
							{
								if( l.d_db.isNull() )
									qDebug() << "isNull";
								j = oidMap.find( l.d_oid );
								if( j != oidMap.end() )
								{
									// Die Ref zeigt auf ein Objekt im selben Dokument; wir mappen darauf
									l.d_oid = j.value();
									l.d_db = thisDb;
									v.setLob( l.writeTo() );
								}
							}
						}
						out.writeSlot( v, name.getTag() );
					}else
						return false; // In rtxt gibt es nur Slots mit Tag oder unbenannt
					break;
				}
			}
			OutlineItem::updateBackRefs(item, out.getBml() );
			item.setValue( OutlineItem::AttrText, out.getBml() );
		}else if( v.isHtml() )
		{
			// TODO: was machen wir damit?
			// hier müssten alle <a> mit Styles::s_linkSchema auf intern umgebogen werden, wenn in gleicher DB.
			// analog zu oben
		}
	}
	return true;
}

QUuid OutlineUdbStream::readOlnFrame( DataReader & in, Udb::Obj parent, const Udb::Obj &home,
									  const Udb::Obj &before, OidMap & oidMap )
{
	Q_ASSERT( !parent.isNull() );
	Q_ASSERT( !home.isNull() );
	Q_ASSERT( in.getCurrentToken() == DataReader::BeginFrame );
	Q_ASSERT( in.getName().getTag().equals( "oln" ) );

	OutlineItem item = parent.createAggregate( OutlineItem::TID, before );
	item.setCreatedOn();
	item.setHome( home );

	QUuid dbid;

	DataReader::Token t = in.nextToken();
	while( DataReader::isUseful( t ) )
	{
		switch( t )
		{
		case DataReader::Slot:
			{
				const NameTag name = in.getName().getTag();
				if( name.equals( "dbid" ) )
					dbid = in.getValue().getUuid();
				else if( name.equals( "oid" ) )
				{
					const Udb::OID oid = in.getValue().getOid();
					if( oid )
					{
						if( oidMap.contains(oid) )
							throw Exception( "object ID is not unique" );
						oidMap[oid] = item.getOid();
					}
				}else if( name.equals( "text" ) )
				{
					item.setValue( OutlineItem::AttrText, in.getValue() );
				}else if( name.equals( "titl" ) )
					item.setValue( OutlineItem::AttrIsTitle, in.getValue() );
				else if( name.equals( "ro" ) )
					item.setValue( OutlineItem::AttrIsReadOnly, in.getValue() );
				else if( name.equals( "id" ) )
					item.setValue( OutlineItem::AttrIdent, in.getValue() );
				else if( name.equals( "aid" ) )
					item.setValue( OutlineItem::AttrAltIdent, in.getValue() );
				else if( name.equals( "exp" ) )
					item.setValue( OutlineItem::AttrIsExpanded, in.getValue() );
				else if( name.equals( "ali" ) )
					item.setValue( OutlineItem::AttrAlias, in.getValue() );
					// Hier wird der Wert einfach mal übernommen; die Aufloesung erfolgt in weiterem Schritt
				else
					qWarning() << "OutlineUdbStream::readObj unexpected slot " << name.toString();
			}
			break;
		case DataReader::BeginFrame:
			if( !in.getName().getTag().equals( "oln" ) )
                throw Exception( "unexpected child frame " + in.getName().toString().toUtf8() );
			else
				readOlnFrame( in, item, home, Udb::Obj(), oidMap );
			break;
		case DataReader::EndFrame:
			{
				DataCell v = item.getValue( OutlineItem::AttrAlias );
				if( v.isUuid() )
				{
					Udb::Obj other = item.getTxn()->getObject( v );
					if( !other.isNull() )
					{
						item.clearValue( OutlineItem::AttrText );
						item.setValueAsObj( OutlineItem::AttrAlias, other );
					}else
						item.clearValue( OutlineItem::AttrAlias );
				}else if( v.isOid() )
					item.setValue( OutlineItem::AttrAlias, DataCell().setUInt64( v.getOid() ) );
					// Für spätere Unterscheidbarkeit von vorigem Fall, wo Uuid aufgelöst werden konnte, und damit
					// die Zahl bei Nicht-Auflösung unschädlich ist
				else
					item.clearValue( OutlineItem::AttrAlias );
				OutlineItem::updateBackRefs(item);
			}
			return dbid; // Ok, gutes Ende
        default:
            Q_ASSERT(false);
            break;
		}
		t = in.nextToken();
	}
	throw Exception( "unexpected end of stream" );
	return dbid;
}
