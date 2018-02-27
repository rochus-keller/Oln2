/*
* Copyright 2008-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the CrossLine outliner Oln2 library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.info.
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

#include "OutlineItem.h"
#include "LinkSupport.h"
#include <Udb/Database.h>
#include <Udb/Transaction.h>
#include <Udb/Extent.h>
#include <Txt/TextHtmlParser.h>
#include <Txt/Styles.h>
#include <Txt/TextOutStream.h>
#include <QtDebug>
using namespace Oln;
using namespace Stream;

quint32 OutlineItem::TID = 10;
quint32 OutlineItem::AttrIsExpanded = 11;
quint32 OutlineItem::AttrIsTitle = 12;
quint32 OutlineItem::AttrIsReadOnly = 13;
quint32 OutlineItem::AttrHome = 14;
quint32 OutlineItem::AttrAlias = 15;
quint32 Outline::TID = 11;
quint32 Outline::AttrHasItems = 16;

const char* OutlineItem::AliasIndex = "OutlineItem::AliasIndex";

static bool s_doBackRef = false;

OutlineItem OutlineItem::createItem(Udb::Obj &parent, const Udb::Obj &before)
{
	Q_ASSERT(!parent.isNull());
	OutlineItem item = parent.createAggregate( TID, before );
	item.setCreatedOn();
	Obj p = parent.getValueAsObj( AttrHome );
	if( p.isNull() )
		p = parent;
	item.setValueAsObj( AttrHome, p );
	return item;
}

OutlineItem OutlineItem::createSubItem(const Udb::Obj &before)
{
	return createItem( *this, before );
}

void OutlineItem::setExpanded(bool on)
{
    setBool( AttrIsExpanded, on );
}

bool OutlineItem::isExpanded() const
{
    return getValue( AttrIsExpanded ).getBool();
}

void OutlineItem::setTitle(bool on)
{
    setBool( AttrIsTitle, on );
}

bool OutlineItem::isTitle() const
{
	return getValue(AttrIsTitle).getBool();
}

QString OutlineItem::toTextHtml() const
{
	Stream::DataCell v = getValue( AttrText );
	if( v.isHtml() )
		return v.getStr();
	else if( v.isBml() )
	{
		Stream::DataReader in(v);
		QString str;
		QTextStream out(&str,QIODevice::WriteOnly);
		Txt::TextOutStream::toHtml(in,out,false);
		return str;
	}else
		return v.toString();
}

QString OutlineItem::getAltOrIdent() const
{

	QString id = ContentObject::getAltOrIdent();
	if( id.isEmpty() )
	{
		ContentObject o = getAlias();
		if( !o.isNull() )
			id = o.getAltOrIdent();
	}
	return id;
}

void OutlineItem::setReadOnly(bool on)
{
    setBool( AttrIsReadOnly, on );
}

bool OutlineItem::isReadOnly() const
{
    return getValue(AttrIsReadOnly).getBool();
}

void OutlineItem::setHome(const Udb::Obj & o)
{
    setValueAsObj( AttrHome, o );
}

Udb::Obj OutlineItem::getHome() const
{
    return getValueAsObj(AttrHome);
}

void OutlineItem::setAlias(const Udb::Obj & o)
{
    setValueAsObj( AttrAlias, o );
}

Udb::Obj OutlineItem::getAlias() const
{
	return getValueAsObj(AttrAlias);
}

Udb::Obj OutlineItem::getPrevItem() const
{
	Udb::Obj o = *this;
	while( o.prev() )
	{
		if( o.getType() == TID )
		{
			return o;
		}
	}
	return Udb::Obj();
}

Udb::Obj OutlineItem::getNextItem() const
{
	Udb::Obj o = *this;
	while( o.next() )
	{
		if( o.getType() == TID )
		{
			return o;
		}
	}
	return Udb::Obj();
}

static int _itemNr( const Udb::Obj & item )
{
	Q_ASSERT( !item.getParent().isNull() );
	int nr = 0;
	Udb::Obj sub = item.getParent().getFirstObj();
	do
	{
		if( sub.equals( item ) )
			return nr;
		else if( sub.getType() == OutlineItem::TID )
			nr++;
	}while( sub.next() );
	Q_ASSERT( false );
	return 0;
}

QString OutlineItem::getParagraphNumber(const Udb::Obj & item)
{
	if( item.getType() == TID )
	{
		int nr = _itemNr( item );
		QString lhs = getParagraphNumber( item.getParent() );
		if( !lhs.isEmpty() )
			return lhs + QLatin1Char( '.' ) + QString::number( nr + 1 );
		else
			return QString::number( nr + 1 );
	}
	return QString();
}

static const QUuid s_backRefIdx = "{1c3f2ad8-99c1-11e5-8994-feff819cdc9f}";

void OutlineItem::updateBackRefs(const OutlineItem & item, const DataCell & newText)
{
	if( !s_doBackRef )
		return;
	if( item.isNull() )
		return;
	const NameTag lt( "link" );
	Link l;
	const QUuid dbid = item.getDb()->getDbUuid();
	const int slen = ::strlen( Txt::Styles::s_linkSchema );
	QMap<Udb::OID,int> refs;
	const DataCell oldText = item.getValue( AttrText );
	if( oldText.isBml() )
	{
		DataReader in( oldText );
		while( DataReader::isUseful( in.nextToken() ) )
		{
			if( in.getName().getTag() == lt && l.readFrom( in.getValue().getArr() ) && l.d_db == dbid )
				refs[l.d_oid]--;
		}
	}else if( oldText.isHtml() )
	{
		Txt::TextHtmlParser p;
		p.parse( oldText.getStr(), 0 );
		for( int i = 0; i < p.count(); i++ )
		{
			if( p.at(i).id == Txt::Html_a && p.at(i).charFormat.anchorHref().startsWith(QLatin1String(
				Txt::Styles::s_linkSchema ) ) &&
				l.readFrom( QByteArray::fromBase64( p.at(i).charFormat.anchorHref().mid( slen ).toAscii() ) ) &&
							l.d_db == dbid )
				refs[l.d_oid]--;
		}
	}
	if( newText.isBml() )
	{
		DataReader in( newText );
		while( DataReader::isUseful( in.nextToken() ) )
		{
			if( in.getName().getTag() == lt && l.readFrom( in.getValue().getArr() ) && l.d_db == dbid )
				refs[l.d_oid]++;
		}
	}else if( newText.isHtml() )
	{
		Txt::TextHtmlParser p;
		p.parse( newText.getStr(), 0 );
		for( int i = 0; i < p.count(); i++ )
		{
			if( p.at(i).id == Txt::Html_a && p.at(i).charFormat.anchorHref().startsWith(QLatin1String(
				Txt::Styles::s_linkSchema ) ) &&
				l.readFrom( QByteArray::fromBase64( p.at(i).charFormat.anchorHref().mid( slen ).toAscii() ) ) &&
							l.d_db == dbid )
				refs[l.d_oid]++;
		}
	}
	if( refs.isEmpty() )
		return;
	Udb::Obj idx = item.getTxn()->getOrCreateObject(s_backRefIdx);
	Udb::Obj::KeyList k(2);
	k[1] = item;
	QMap<Udb::OID,int>::const_iterator i;
	for( i = refs.begin(); i != refs.end(); ++i )
	{
		if( i.value() != 0 )
		{
			k[0].setOid( i.key() );
			const int n = idx.getCell(k).getUInt16() + i.value();
			if( n <= 0 )
				idx.setCell( k, DataCell().setNull() );
			else
				idx.setCell(k, DataCell().setUInt16(n) );
		}
	}
}

void OutlineItem::updateBackRefs(const OutlineItem &item)
{
	if( !s_doBackRef )
		return;
	// dasselbe wie updateBackRefs(item,v), aber ohne neuen Wert, bzw. der bestehende wird eingetragen
	if( item.isNull() )
		return;
	const NameTag lt( "link" );
	Link l;
	const QUuid dbid = item.getDb()->getDbUuid();
	const int slen = ::strlen( Txt::Styles::s_linkSchema );
	QMap<Udb::OID,int> refs;
	const DataCell text = item.getValue( AttrText );
	if( text.isBml() )
	{
		DataReader in( text );
		while( DataReader::isUseful( in.nextToken() ) )
		{
			if( in.getName().getTag() == lt && l.readFrom( in.getValue().getArr() ) && l.d_db == dbid )
				refs[l.d_oid]++;
		}
	}else if( text.isHtml() )
	{
		Txt::TextHtmlParser p;
		p.parse( text.getStr(), 0 );
		for( int i = 0; i < p.count(); i++ )
		{
			if( p.at(i).id == Txt::Html_a && p.at(i).charFormat.anchorHref().startsWith(QLatin1String(
				Txt::Styles::s_linkSchema ) ) &&
				l.readFrom( QByteArray::fromBase64( p.at(i).charFormat.anchorHref().mid( slen ).toAscii() ) ) &&
							l.d_db == dbid )
				refs[l.d_oid]++;
		}
	}
	if( refs.isEmpty() )
		return;
	Udb::Obj idx = item.getTxn()->getOrCreateObject(s_backRefIdx);
	Udb::Obj::KeyList k(2);
	k[1] = item;
	QMap<Udb::OID,int>::const_iterator i;
	for( i = refs.begin(); i != refs.end(); ++i )
	{
		if( i.value() != 0 )
		{
			k[0].setOid( i.key() );
			idx.setCell(k, DataCell().setUInt16(i.value()) );
			//qDebug() << "Indexed" << k[0].getOid() << k[1].getOid() << i.value(); // TEST
		}
	}
}

void OutlineItem::updateAllRefs(Udb::Transaction* txn)
{
	if( !s_doBackRef )
		return;
	Udb::Obj idx = txn->getOrCreateObject(s_backRefIdx);
	idx.erase();
	txn->commit();
	Udb::Extent e( txn );
	if( e.first() ) do
	{
		OutlineItem obj = e.getObj();
		if( obj.getType() == TID )
		{
			updateBackRefs( obj );
		}
	}while( e.next() );
	txn->commit();
}

QList<OutlineItem> OutlineItem::getReferences(const Udb::Obj &obj)
{
	QList<OutlineItem> res;
	if( obj.isNull() )
		return res;
	Udb::Obj idx = obj.getTxn()->getOrCreateObject(s_backRefIdx);
	Udb::Obj::KeyList k(1);
	k[0] = obj;
	Udb::Mit mit = idx.findCells(k);
	if( !mit.isNull() ) do
	{
		k = mit.getKey();
		Q_ASSERT( k.size() == 2 );
		OutlineItem item = obj.getObject( k[1].getOid() );
		if( item.getType() == TID )
			res.append( item );
		else
			qDebug() << "Invalid Reference in" << obj.getOid() << obj.getString( AttrText );
		//qDebug() << "Found" << k[0].getOid() << k[1].getOid(); // TEST

	}while( mit.nextKey() );
	return res;
}

void OutlineItem::erase(Udb::Obj item)
{
	// NOTE: das nützt nichts, da ja irgendwer ganze Objektbäume löschen kann, wovon wir hier nichts erfahren.
	// Besser mit itemErasedCallback den Index aktualisieren
	item.erase();
}

void OutlineItem::itemErasedCallback(Udb::Transaction * txn, const Udb::UpdateInfo & info)
{
	if( info.d_kind != Udb::UpdateInfo::PreCommit )
		return;
	QList<Udb::UpdateInfo> updates = txn->getPendingNotifications();
	for( int i = 0; i < updates.size(); i++ )
	{
		if( updates[i].d_kind == Udb::UpdateInfo::ObjectErased && updates[i].d_name == TID )
		{
			OutlineItem item = txn->getObject( updates[i].d_id );
			updateBackRefs( item, DataCell() );
		}
	}
}

void OutlineItem::doBackRef(bool on)
{
	s_doBackRef = on;
}

void Outline::markHasItems(Udb::Obj & oln)
{
	if( oln.isNull() )
		return;
	bool hasItems = false;
	Udb::Obj sub = oln.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == OutlineItem::TID )
		{
			hasItems = true;
			break;
		}
	}while( sub.next() );
	if( hasItems )
		oln.setValue( AttrHasItems, DataCell().setBool(true) );
	else
		oln.clearValue( AttrHasItems );
}

bool Outline::hasItems(const Udb::Obj & o)
{
	if( o.isNull() )
		return false;
	return o.getValue( AttrHasItems ).getBool();
}


