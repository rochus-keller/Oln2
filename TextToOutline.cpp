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

#include "TextToOutline.h"
#include <QTextStream>
#include <QStack>
#include <Txt/TextOutHtml.h>
#include "OutlineItem.h"
#include <cassert>
using namespace Oln;


static Udb::Obj createItem(Udb::Transaction* txn, Stream::DataCell::OID home)
{
	Udb::Obj o = txn->createObject( OutlineItem::TID );
	o.setValue( OutlineItem::AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
	if( home )
		o.setValue( OutlineItem::AttrHome, Stream::DataCell().setOid( home ) );
	o.setValue( OutlineItem::AttrIsExpanded, Stream::DataCell().setBool( true ) );
	return o;
}

QList<Udb::Obj> TextToOutline::parse( QString text, Udb::Transaction * txn, Stream::DataCell::OID home )
{
	QList<Udb::Obj> res;
	QTextStream in( &text, QIODevice::ReadOnly );
	int i;
	int level;
	QStack<Udb::Obj> obs;
	while( !in.atEnd() )
	{
		QString line = in.readLine();
		if( line.startsWith( "FINITO" ) )
			return res;
		i = 0;
		level = 0;
		while( i < line.size() && line[i] == QChar('\t') )
		{
			i++;
			level++;
		}
		if( obs.size() > level )
			obs.resize( level );
		if( level == 0 )
		{
			obs.push( createItem( txn, home ) );
			QString str = line.mid( i );
			obs.top().setValue( OutlineItem::AttrText, Stream::DataCell().setString( str ) );
			res.append( obs.top() );
		}else
		{
			assert( obs.size() > 0 );
			Udb::Obj o = createItem( txn, home );
			o.aggregateTo( obs.top() );
			obs.push( o );
			obs.top().setValue( OutlineItem::AttrText, Stream::DataCell().setString( line.mid( i ) ) );
		}

	}
	// TODO: Outline::markHasItems
	return res;
}

static QString _toText( const Udb::Obj& item, int level )
{
	QString res;
	for( int i = 0; i < level; i++ )
		res += QChar('\t');
	Stream::DataCell v;
	Stream::DataCell::OID oid = item.getValue( OutlineItem::AttrAlias ).getOid();
	if( oid )
	{
		Udb::Obj o = item.getObject( oid );
		if( !o.isNull() )
			o.getValue( OutlineItem::AttrText, v );
		else
			item.getValue( OutlineItem::AttrText, v );
	}else
		item.getValue( OutlineItem::AttrText, v );
	if( v.isHtml() )
		res += Txt::TextOutHtml::htmlToPlainText( v.getStr() );
	else
		res += v.toString().simplified();
	res += QChar('\n');
	Udb::Obj sub = item.getFirstObj();
	if( !sub.isNull() ) do
	{
		res += _toText( sub, level + 1 );
	}while( sub.next() );
	return res;
}

QString TextToOutline::toText( const Udb::Obj &item )
{
	return _toText( item, 0 );
}
