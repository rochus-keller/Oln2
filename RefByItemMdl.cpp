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

#include "RefByItemMdl.h"
#include <QTreeView>
#include <Udb/Database.h>
#include <Oln2/OutlineUdbMdl.h>
#include <Oln2/OutlineItem.h>
#include <Udb/Idx.h>
#include <Udb/Extent.h>
#include <QtDebug>
using namespace Oln;

static QString _formatTitle(const Udb::Obj & o )
{
	if( o.isNull() )
		return QString();
	OutlineItem co = o;
	const QString id = co.getAltOrIdent();
	if( !id.isEmpty() )
		return QString("%1 %2").arg(id).arg(co.getText());
	// else
	return co.getText();
}

QString (*RefByItemMdl::formatTitle)(const Udb::Obj & o ) = _formatTitle;

RefByItemMdl::RefByItemMdl( QTreeView* p ):QAbstractItemModel( p )
{
	connect( p, SIGNAL( doubleClicked ( const QModelIndex & ) ), this, SLOT( onRefDblClicked( const QModelIndex & ) ) );
}

QTreeView* RefByItemMdl::getTree() const
{
	return static_cast<QTreeView*>( QObject::parent() );
}

void RefByItemMdl::setObj( const Udb::Obj& o, bool focus )
{
	if( !d_root.d_obj.equals( o ) )
	{
		d_root.d_obj = o;
		refill();
	}
	if( focus && !o.isNull() )
		focusOn( o.getValueAsObj( OutlineItem::AttrHome ) );

	getTree()->expandAll();
}

void RefByItemMdl::refill()
{
	foreach( Slot* s, d_root.d_children )
		delete s;
	d_root.d_children.clear();
	reset();
	if( d_root.d_obj.isNull() )
		return;
	QMap<Udb::OID,QMap<QString,Udb::OID> > items;
	Udb::Idx idx( d_root.d_obj.getTxn(), OutlineItem::AliasIndex );
	if( !idx.isNull() && idx.seek( d_root.d_obj ) ) do
	{
		Udb::Obj item = d_root.d_obj.getObject( idx.getOid() );
		Q_ASSERT( !item.isNull(true,true) && item.getType() == OutlineItem::TID );
		Udb::Obj context = item.getValueAsObj( OutlineItem::AttrHome );
		Q_ASSERT( !context.isNull(true,true) );
		items[context.getOid()][OutlineItem::getParagraphNumber(item)] = item.getOid();
	}while( idx.nextKey() );
	QList<OutlineItem> refs = OutlineItem::getReferences( d_root.d_obj );
	for( int k = 0; k < refs.size(); k++ )
	{
		//qDebug() << refs[k].isNull() << refs[k].getOid() << refs[k].getType() << refs[k].getText();
		Q_ASSERT( !refs[k].isNull(true,true) && refs[k].getType() == OutlineItem::TID  );
		Udb::Obj context = refs[k].getValueAsObj( OutlineItem::AttrHome );
		Q_ASSERT( !context.isNull(true,true) );
		items[context.getOid()][OutlineItem::getParagraphNumber(refs[k])] = refs[k].getOid();
	}
	QMap<Udb::OID,QMap<QString,Udb::OID> >::const_iterator i;
	for( i = items.begin(); i != items.end(); ++i )
	{
		Slot* s1 = new Slot();
		s1->d_parent = &d_root;
		d_root.d_children.prepend( s1 ); // sortiere absteigend nach OID
		s1->d_obj = d_root.d_obj.getObject(i.key());
		d_cache[ i.key() ] = s1;
		QMap<QString,Udb::OID>::const_iterator j;
		for( j = i.value().begin(); j != i.value().end(); ++j )
		{
			Slot* s2 = new Slot();
			s2->d_parent = s1;
			s1->d_children.append( s2 );
			s2->d_obj = d_root.d_obj.getObject( j.value() );
			s2->d_nr = j.key();
			d_cache[ j.value() ] = s2;
		}
	}
	reset();
}

QModelIndex RefByItemMdl::parent ( const QModelIndex & index ) const
{
	if( index.isValid() )
	{
		Slot* s = static_cast<Slot*>( index.internalPointer() );
		Q_ASSERT( s != 0 );
		if( s->d_parent == &d_root )
			return QModelIndex();
		// else
		Q_ASSERT( s->d_parent != 0 );
		Q_ASSERT( s->d_parent->d_parent != 0 );
		return createIndex( s->d_parent->d_parent->d_children.indexOf( s->d_parent ), 0, s->d_parent );
	}else
		return QModelIndex();}

int RefByItemMdl::rowCount ( const QModelIndex & parent ) const
{
	if( parent.isValid() )
	{
		Slot* s = static_cast<Slot*>( parent.internalPointer() );
		Q_ASSERT( s != 0 );
		return s->d_children.size();
	}else
		return d_root.d_children.size();}

QModelIndex RefByItemMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	const Slot* s = &d_root;
	if( parent.isValid() )
	{
		s = static_cast<Slot*>( parent.internalPointer() );
		Q_ASSERT( s != 0 );
	}
	if( row < s->d_children.size() )
		return createIndex( row, column, s->d_children[row] );
	else
		return QModelIndex();
}

QVariant RefByItemMdl::data ( const QModelIndex & index, int role ) const
{
	if( !index.isValid() || d_root.d_obj.isNull() )
		return QVariant();
	Slot* s = static_cast<Slot*>( index.internalPointer() );
	Q_ASSERT( s != 0 );
	OutlineItem item = s->d_obj;
	OutlineItem alias = s->d_obj.getValueAsObj(OutlineItem::AttrAlias);
	if( alias.isNull() )
		alias = item;
	switch( index.column() )
	{
	case 0:
		switch( role )
		{
		case Qt::DisplayRole:
			if( item.getType() == OutlineItem::TID )
			{
				const QString id = item.getAltOrIdent();
				if( id.isEmpty())
					return QString( "%1: %2").arg( s->d_nr ).arg( alias.getText() );
				else
					return QString( "%1: %2 %3").arg( s->d_nr ).arg( id ).arg( alias.getText() );
			}else
				return formatTitle( alias );
		case Qt::ToolTipRole:
			if( alias.getType() == OutlineItem::TID )
				return alias.toTextHtml();
			else
				return formatTitle( alias );
		case Qt::DecorationRole:
			return Oln::OutlineUdbMdl::getPixmap( item.getType() );
		}
		break;
	}
	return QVariant();
}

Udb::Obj RefByItemMdl::getObject( const QModelIndex& i ) const
{
	if( i.isValid() )
	{
		Slot* s = static_cast<Slot*>( i.internalPointer() );
		Q_ASSERT( s != 0 );
		return s->d_obj;
	}else
		return d_root.d_obj;
}

void RefByItemMdl::focusOn( const Udb::Obj& o )
{
	return; // Verursacht Crash
	getTree()->setCurrentIndex( getIndex( o ) );
	getTree()->scrollTo( getTree()->currentIndex() );
}

QModelIndex RefByItemMdl::getIndex( Slot* s ) const
{
	if( s == 0 || s->d_parent == 0 )
		return QModelIndex();
	else
		return createIndex( s->d_parent->d_children.indexOf( s ), 0, s );
}

QModelIndex RefByItemMdl::getIndex( const Udb::Obj& o ) const
{
	if( o.isNull() )
		return QModelIndex();
	Slot* s = d_cache.value( o.getOid() );
	if( s )
		return getIndex( s );
	else
		return QModelIndex();
}

void RefByItemMdl::onRefDblClicked(const QModelIndex & i)
{
	Udb::Obj o = getObject( i );
	emit sigFollowObject(o);
}
