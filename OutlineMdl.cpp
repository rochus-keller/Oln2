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

#include "OutlineMdl.h"
#include "HtmlToOutline.h"
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <QtDebug>
#include <QStringList>
#include <QMimeData>
#include <cassert>
#include "OutlineStream.h"
#include "TextToOutline.h"
using namespace Oln;
using namespace Udb;

OutlineMdl::OutlineMdl(QObject *parent)
	: QAbstractItemModel(parent), d_root(0)
{
}

OutlineMdl::~OutlineMdl()
{
	if( d_root )
		delete d_root;
}

OutlineMdl::Slot::~Slot()
{
	for( int i = 0; i < d_subs.size(); i++ )
	{
		delete d_subs[i];
	}
}

int OutlineMdl::Slot::getLevel() const
{
	if( d_super )
		return d_super->getLevel() + 1;
	else
		return 0;
}

OutlineMdl::Slot* OutlineMdl::getSlot( const QModelIndex& index ) const
{
	Slot* s = 0;
	if( index.isValid() )
	{
		s = static_cast<Slot*>( index.internalPointer() );
		Q_ASSERT( s != 0 );
		// check ob s noch gültig ist. QTreeView reexpand bringt ab und zu ungültigen index
	}else
		s = d_root;
	assert( s != 0 );
	return s;
}

QModelIndex OutlineMdl::index( int row, int column, const QModelIndex & parent ) const
{
    // Ungültiger QModelIndex repräsentiert d_root, enthält den aber nicht als Pointer
    if( parent.isValid() )
	{
		Slot* s = static_cast<Slot*>( parent.internalPointer() );
		Q_ASSERT( s != 0 );
        Q_ASSERT( row < s->d_subs.size() );
		return createIndex( row, column, s->d_subs[row] );
    }else if( d_root )
	{
		Q_ASSERT( row < d_root->d_subs.size() );
		return createIndex( row, column, d_root->d_subs[row] );
	}else
        return QModelIndex();
}

int OutlineMdl::rowCount ( const QModelIndex & parent ) const
{
    if( parent.isValid() )
    {
        Slot* s = static_cast<Slot*>( parent.internalPointer() );
        Q_ASSERT( s != 0 );
        return s->d_subs.size();
    }else if( d_root )
        return d_root->d_subs.size();
    else
        return 0;
}

QModelIndex OutlineMdl::parent(const QModelIndex & index) const
{
    if( index.isValid() )
	{
		Slot* s = static_cast<Slot*>( index.internalPointer() );
		Q_ASSERT( s != 0 );
		if( s->d_super == d_root )
			return QModelIndex();
		// else
		Q_ASSERT( s->d_super != 0 );
		Q_ASSERT( s->d_super->d_super != 0 );
		return createIndex( s->d_super->d_super->d_subs.indexOf( s->d_super ), 0, s->d_super );
	}else
		return QModelIndex();
}

static QString _number( const QModelIndex & index )
{
	if( !index.isValid() )
		return QString();
	QString lhs = _number( index.parent() );
	if( !lhs.isEmpty() )
		return lhs + QLatin1Char( '.' ) + QString::number( index.row() + 1 );
	else
		return QString::number( index.row() + 1 );
}

QVariant OutlineMdl::data ( const QModelIndex & index, int role ) const
{
	if( d_root == 0 )
		return QVariant();
    if( !index.isValid() )
    {
        if( role == ReadOnlyRole )
            return isReadOnly();
        else
            return QVariant();
    }// else

	// index.column don't care
    Slot* s = static_cast<Slot*>( index.internalPointer() );
    Q_ASSERT( s != 0 );
	switch( role )
	{
		// Spaltenunabhängige Information
	case OidRole:
		return s->getId();
	case LevelRole:
		return s->getLevel();
	case TitleRole:
		return s->isTitle(this);
	case NumberRole:
		return _number( index );
	case ExpandedRole:
		return s->isExpanded(this);
	case AliasRole:
		return s->isAlias(this);
	case ReadOnlyRole:
		return isReadOnly() || s->isReadOnly(this) ;
	case Qt::DisplayRole:
	case Qt::EditRole:
	case Qt::DecorationRole:
	case Qt::ToolTipRole:
	case Qt::StatusTipRole:
	case Qt::WhatsThisRole:
	case IdentRole:
		return s->getData( this, role );
	}
	return QVariant();
}

QVariant OutlineMdl::headerData ( int section, Qt::Orientation orientation, int role ) const
{
    Q_UNUSED(role);
    Q_UNUSED(orientation);
    Q_UNUSED(section);
	return QVariant();
}

Qt::DropActions OutlineMdl::supportedDropActions () const
{
	return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::ItemFlags OutlineMdl::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
	Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	if( !isReadOnly() )
		f |= Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled;
	// Wir dürfen Editable nicht hier regeln, sondern müssen im Delegate selber fragen. Ansonsten wird der Editor
	// gar nicht mehr geöffnet, nicht einmal readonly.
	return f;
}

void OutlineMdl::clear()
{
    beginResetModel();
	if( d_root )
	{
		if( !d_root->d_subs.isEmpty() )
		{
			beginRemoveRows( QModelIndex(), 0, d_root->d_subs.size() - 1 );
			d_cache.clear();
			delete d_root;
			d_root = 0;
			endRemoveRows();
		}else
		{
			delete d_root;
			d_root = 0;
		}
	}
    endResetModel();
}

void OutlineMdl::recursiveRemove( Slot* s )
{
	if( s == 0 )
		return;
	d_cache.remove( s->getId() );
	for( int i = 0; i < s->d_subs.size(); i++ )
		recursiveRemove( s->d_subs[i] );
}

void OutlineMdl::clearCache( const QModelIndex & parent )
{
	Slot* s = getSlot( parent );
    if( s->d_subs.isEmpty() )
        return;
	beginRemoveRows( parent, 0, s->d_subs.size() - 1 );
	// Lasse keine Listen mit gelöschten Items rumliegen
	QList<Slot*> tmp = s->d_subs;
	s->d_subs.clear();
	for( int i = 0; i < tmp.size(); i++ )
	{
		recursiveRemove( tmp[i] );
		delete tmp[i];
	}
	endRemoveRows();
}

QModelIndex OutlineMdl::getIndex( quint64 oid, bool fetch ) const
{
    Q_UNUSED(fetch);
    if( oid == 0 )
		return QModelIndex();
	Slot* s = d_cache.value( oid ); 
	if( s != 0 )
	{
		// Das Objekt wurde bereits geladen.
		const int row = s->d_super->d_subs.indexOf( s );
		assert( row >= 0 );
		return createIndex( row, 0, s );
	}
	return QModelIndex();
}

QModelIndex OutlineMdl::getFirstIndex() const
{
	if( d_root->d_subs.isEmpty() )
		return QModelIndex();
	else
		return createIndex( 0, 0, d_root->d_subs.first() );
}

void OutlineMdl::add( Slot* s, Slot* to, int before )
{
	assert( s != 0 );
	assert( s->d_super == 0 );
	if( to == 0 )
	{
		if( d_root )
			clear();
		d_root = s;
	}else
	{
		assert( s->getId() != 0 );
		d_cache[ s->getId() ] = s;
		s->d_super = to;
		if( before < 0 )
			to->d_subs.append( s );
		else
			to->d_subs.insert( before, s );
	}
}

void OutlineMdl::remove( Slot* s )
{
	assert( s != 0 );
	if( s == d_root )
		clear();
	else
	{
		recursiveRemove( s );
		s->d_super->d_subs.removeOne( s );
		delete s;
	}
}

OutlineMdl::Slot* OutlineMdl::findSlot( quint64 id ) const
{
	return d_cache.value( id );
}

quint64 OutlineMdl::getId( const QModelIndex & i ) const
{
	Slot* s = getSlot( i );
	if( s )
		return s->getId();
	else
		return 0;
}
