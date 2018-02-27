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

#include "OutlineTreeModel.h"
#include <Gui/VariantValue.h>
#include <Text2/AtomManager.h>
#include <Text2/ParagraphMdl.h>
#include <QColor>
#include <QMimeData>
#include <QtDebug>
#include <QBuffer>
using namespace Oln;
using namespace Gui;


OutlineTreeModel::OutlineTreeModel( QObject* o ):
	QAbstractItemModel( o )
{
	clear();
}

OutlineTreeModel::~OutlineTreeModel()
{
}

void OutlineTreeModel::clear()
{
	d_mdl.clear();
	reset();
}

QVariant OutlineTreeModel::data(const QModelIndex &index, int role) const
{
	if( d_mdl.getRoot() == 0 || !index.isValid() )
		return QVariant();
    OutlineItem* e = static_cast<OutlineItem*>(index.internalPointer());
	if( e == 0 )
		return QVariant();

	switch( role )
	{
	case ExpandedRole:
		return e->isExpanded();
	case LevelRole:
		return (int)e->getLevel();
	case TitleRole:
		return !e->isBody();
	case AtomRole:
		return QVariant::fromValue( VariantValue::ValueBox( Root::Value(), d_mdl.getAtoms(), false ) );
	case Qt::DisplayRole:
	case Qt::EditRole:
			{
				bool copy = false;
				Root::Value v;
				if( e->isBody() )
				{
					e->getVal( v );
					if( !v.isValid() )
					{
						Root::ValueWriter vw;
						Text::ParagraphMdl::writeEmpty( vw, d_mdl.getAtoms() );
						vw.releaseTo( v );
						copy = true;
					}
				}else
				{
					e->getVal( v );
					if( !v.isValid() )
						v.setAnsi( "" );
				}
				return VariantValue::toVariant( v, d_mdl.getAtoms(), copy );
			}
	case Qt::DecorationRole:
			{
				if( !e->getNr().empty() )
					return e->getNr().c_str();
			}
	}
	return QVariant();
}

bool OutlineTreeModel::setData ( const QModelIndex & index, const QVariant & value, int role )  
{
    OutlineItem* e = static_cast<OutlineItem*>(index.internalPointer());
	if( e == 0 )
		return false;

	if( index.column() == 0 )
	{
		Root::Value val;
		if( !VariantValue::toValue( value, val ) )
			return false;
		if( role == Qt::EditRole )
		{
			e->setVal( val );
			emit dataChanged( index, index );
			emit onDirty();
		}else if( role == ExpandedRole )
		{
			e->setExpanded( value.toBool() );
			emit onDirty();
		}else if( role == Qt::SizeHintRole )
		{
			// Trick um Height-Cache zu invalidieren
			emit dataChanged( index, index );
		}else if( role == TitleRole )
		{
			e->setIsBody( !value.toBool() );
			emit dataChanged( index, index );
			emit onDirty();
		}
		return true;
	}	
	// TODO: was ist mit anderen Spalten und was ist mit BodyOnly?
	return false;
}

QVariant OutlineTreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
	if( role != Qt::DisplayRole )
		return QVariant();

	switch( section )
	{
	case 0:
		return "Title";
	case 1:
		return "ID";
	}

    return QVariant();
}

QModelIndex OutlineTreeModel::index(int row, int column, const QModelIndex &parent) const
{
	if( d_mdl.getRoot() == 0 )
		return QModelIndex();

    OutlineItem* par = d_mdl.getRoot();
	if (parent.isValid())
		par = static_cast<OutlineItem*>(parent.internalPointer());

	OutlineItem* e = par->getAt( row );
	if( e )
		return createIndex( row, column, e );
	else
		return QModelIndex();
}

QModelIndex OutlineTreeModel::parent(const QModelIndex &index) const
{
	// index invalid bezieht sich auf den unsichtbare Root des Tree
	if( d_mdl.getRoot() == 0 || !index.isValid() )
		return QModelIndex();

    OutlineItem* e = static_cast<OutlineItem*>(index.internalPointer());
	OutlineItem* par = e->getOwner();
	if( par == 0 ) // Sollte eigentlich nicht vorkommen, aber Qt schafft das manchmal doch.
		return QModelIndex();

	OutlineItem* parpar = par->getOwner();
	if( parpar == 0 )
		return QModelIndex();
	else
		return createIndex( parpar->indexOf( par ), 0, par );
}

int OutlineTreeModel::rowCount(const QModelIndex &parent) const
{
	if( d_mdl.getRoot() == 0 )
		return 0;

    OutlineItem* par = d_mdl.getRoot();
	if (parent.isValid())
		par = static_cast<OutlineItem*>(parent.internalPointer());

	return par->getCount();
}

Qt::ItemFlags OutlineTreeModel::flags(const QModelIndex &index) const
{

    if ( !index.isValid())
        return Qt::ItemIsEnabled;
	Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	OutlineItem* e = static_cast<OutlineItem*>(index.internalPointer());
	if( e == 0 )
		return f;

    if( index.isValid() )
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    else
        f |= Qt::ItemIsDropEnabled;

	switch( index.column() )
	{
	case 0:
		if( !e->isFixText() ) 
			f |= Qt::ItemIsEditable;
		return f;
	default:
		return 0;
	}
}

OutlineItem* OutlineTreeModel::getItem( const QModelIndex & i ) const
{
	if( !i.isValid() )
		return d_mdl.getRoot();
	else
		return static_cast<OutlineItem*>( i.internalPointer() );
}

QModelIndex OutlineTreeModel::createChild( QModelIndex parent, int row, bool isBody )
{
	if( d_mdl.getRoot() == 0 )
		return QModelIndex();
    OutlineItem* paritem = static_cast<OutlineItem*>( parent.internalPointer() );
	if( paritem == 0 )
		paritem = d_mdl.getRoot();
	if( !paritem->canAddChild() )
		return QModelIndex();
	paritem->setExpanded( true );
	const int count = paritem->getCount();
	if( row < 0 || row > count )
		row = count;
	beginInsertRows( parent, row, row );
	OutlineItem* item = paritem->createChild( row, isBody );
	assert( item );
	endInsertRows();
	return index( row, 0, parent );
}

bool OutlineTreeModel::removeChild( QModelIndex parent, int row )
{
	if( d_mdl.getRoot() == 0 )
		return false;
    OutlineItem* paritem = d_mdl.getRoot();
	if (parent.isValid())
		paritem = static_cast<OutlineItem*>(parent.internalPointer());

	const int count = paritem->getCount();
	if( row < 0 || row > count )
		row = count;
	if( !paritem->canRemoveChild( row ) )
		return false;
	beginRemoveRows( parent, row, row );
	paritem->removeChild( row );
	endRemoveRows();
	return true;
}

bool OutlineTreeModel::move( QModelIndex source, QModelIndex toParent, int row )
{
	if( d_mdl.getRoot() == 0 || !source.isValid() )
		return false;
	Root::Ref<OutlineItem> from = static_cast<OutlineItem*>( source.internalPointer() );
	assert( from && from->getOwner() );
    OutlineItem* paritem = d_mdl.getRoot();
	if( toParent.isValid() )
		paritem = static_cast<OutlineItem*>( toParent.internalPointer() );
	if( !paritem->canAddChild() )
		return false;
	const int count = paritem->getCount();
	if( row < 0 || row > count )
		row = count;
	QModelIndex pidx = parent( source );
	if( !from->getOwner()->canRemoveChild( source.row() ) )
		return false;
	beginRemoveRows( pidx, source.row(), source.row() ); // RISK: ist das zulässig?
	beginInsertRows( toParent, row, row );
	from->moveTo( paritem, row );
	endInsertRows();
	endRemoveRows(); // Diese Reihenfolge ist relevant, ansonsten Crash bei setExpanded
	return true;
}

Qt::DropActions OutlineTreeModel::supportedDropActions() const
{
	// Qt::CopyAction | 
	return Qt::MoveAction;
}

QStringList OutlineTreeModel::mimeTypes() const
{
    QStringList types;
    types << "application/outlineitems";
    types << "application/outlineitemrefs";
    return types;
}

QMimeData* OutlineTreeModel::createMimeData(const QModelIndexList &indexes, bool pointer ) const
{
	if( indexes.isEmpty() )
		return 0;
	if( pointer )
	{
		QByteArray buf;
		QTextStream out( &buf, QIODevice::WriteOnly );
		out << this << endl;
		for( int i = 0; i < indexes.size(); i++ )
		{
			out << indexes[i].row() << endl;
			out << indexes[i].column() << endl;
			out << indexes[i].internalPointer() << endl;
		}
		out.flush();
		QMimeData *mimeData = new QMimeData();
		mimeData->setData("application/outlineitemrefs", buf );
		return mimeData;
	}else
	{
		// by value
		Root::ValueWriter tmp;
		QModelIndexList::const_iterator i;
		for( i = indexes.begin(); i != indexes.end(); ++i )
		{
			OutlineItem* item = getItem( (*i) );
			d_mdl.saveOutline( item, tmp, d_mdl.getAtoms(), true );
		}
		Root::ValueWriter vw;
		if( !Root::StreamEnvelope::pack( vw, tmp, d_mdl.getAtoms() ) )
			return 0; // RISK
		QMimeData *mimeData = new QMimeData();
		QByteArray ba( (const char*)vw.getData(), vw.getDataLen() );
		mimeData->setData("application/outlineitems", ba );
		return mimeData;
	}
}

QMimeData *OutlineTreeModel::mimeData(const QModelIndexList &indexes) const
{
	return createMimeData( indexes, true );
}

bool OutlineTreeModel::dropOutlineItem(const QMimeData *data,
    Qt::DropAction action, int row, int column, QModelIndex parent)
{
	if( action != Qt::CopyAction )
		return false;
	QByteArray ba = data->data( "application/outlineitems" );
	Root::ValueReader vr( (Root::UInt8*)ba.data(), ba.size() );

	// RISK: das ist gefährlich. Wir schreiben direkt in QByteArray rein!
	if( !Root::StreamEnvelope::unpack( vr, d_mdl.getAtoms() ) )
		return false;

	OutlineItem* paritem = getItem( parent );
	assert( paritem );

	if( !paritem->canAddChild() )
		return false;
	paritem->setExpanded( true );
	if( row == -1 )
	{
		if( parent.isValid() )
		{
			// Item soll sibling von parent werden
			row = parent.row() + 1;
			parent = parent.parent();
			paritem = paritem->getOwner();
		}else
			// Item soll am Schluss eingefügt werden
			row = paritem->getCount();
	}//else
		// Parent valid und gültige row fügt item in Parent ein

	Root::Ref<OutlineItem> item;
	Root::Ref<OutlineItem> first;
	while( vr.hasMore() )
	{
		// Gehe durch den Stream und lese alle Outlines.
		// Schliesslich enthält first das erste gelesene Item und
		// alle weiteren aller Outlines sind daran als next angehängt.
		if( !d_mdl.loadOutline( item, vr, d_mdl.getAtoms() ) )
			return false;
		if( first.isNull() )
			first = item;
		item = item->getLast();
	}
	int count2 = 0;
	item = first;
	for( ; item; count2++ )
		item = item->getNext();

	beginInsertRows( parent, row, row + count2 );
	d_mdl.insert( paritem, row, first );
	endInsertRows();
	return true; 
}

bool OutlineTreeModel::dropOutlinePtr(const QMimeData *data,
    Qt::DropAction action, int row, int column, QModelIndex parent)
{
	if( action != Qt::MoveAction )
		return false;
	QByteArray ba = data->data( "application/outlineitemrefs" );

	QTextStream in( &ba, QIODevice::ReadOnly );
	quint32 ptr;
	in >> ptr;

	if( reinterpret_cast<void*>( ptr ) != this )
		return false;

	QModelIndexList list;
	while( !in.atEnd() )
	{
		int row, col;
		in >> row;
		in >> col;
		in >> ptr;
		if( ptr )
			list.append( createIndex( row, col, reinterpret_cast<void*>( ptr ) ) );
	}
	if( list.isEmpty() )
		return false;
	return move( list[0], parent, row );
}

static int unfold( QString& s )
{
	int i = 0;
	while( i < s.size() && s[i] == '\t' )
		i++;
	s = s.trimmed();
	return i;
}

bool OutlineTreeModel::dropText(const QMimeData *data,
    Qt::DropAction action, int row, int column, const QModelIndex &p)
{
	const int s_limit = 50;

	QStringList sl = data->text().split( '\n' );
	QModelIndex pidx = p;
	if( row == -1 )
		pidx = parent( p );
	OutlineItem* pitem = getItem( pidx );
	if( pitem == 0 )
		pitem = getRoot();
	if( !pitem->canAddChild() )
		return false;
	int level = -1;
	QModelIndex curidx;
	Root::Value v;
	if( true ) // sl.size() > s_limit )
		blockSignals( true );

	for( int i = 0; i < sl.size(); i++ )
	{
		QString s = sl[i];
		const int tabs = unfold( s );
		if( level == -1 )
		{
			level = tabs;
		}else if( tabs > level )
		{
			pidx = curidx;
			level++; // Ignoriere ungültige Indents
		}else if( tabs < level )
		{
			while( level > tabs && pidx.isValid() )
			{
				pidx = parent( pidx );
				level--;
			}
		}
		curidx = createChild( pidx, -1, true );
		Root::ValueWriter vw;
		Text::ParagraphMdl mdl;
		if( !mdl.TextSource::insertText( 0, s.toLatin1() ) )
			return false;
		bool res = mdl.TextSource::write( vw, d_mdl.getAtoms() );
		vw.releaseTo( v );
		OutlineItem* item = getItem( curidx );
		res = item->setVal( v );
		// setData( curidx, s );
	}
	if( true ) // sl.size() > s_limit )
	{
		blockSignals( false );
		emit layoutChanged();
		// reset();
	}
	return true;
}

bool OutlineTreeModel::dropMimeData(const QMimeData *data,
    Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if( data->hasFormat("application/outlineitems") )
        return dropOutlineItem( data, action, row, column, parent );
	else if( data->hasFormat("application/outlineitemrefs" ) )
        return dropOutlinePtr( data, action, row, column, parent );
	else if( data->hasText() )
        return dropText( data, action, row, column, parent );

	return false;
}

bool OutlineTreeModel::loadFrom( Root::ValueReader& vr )
{
	/*
	if( !d_mdl.getAtoms()->read( vr ) )
		return false;
	if( !d_mdl.loadFrom( vr ) )
		return false;
		*/
	if( !Root::StreamEnvelope::unpack( vr, d_mdl.getAtoms() ) )
		return false;
	if( !d_mdl.loadFrom( vr ) )
		return false;
	reset();
	return true;
}

bool OutlineTreeModel::saveTo( Root::ValueWriter& vw ) const
{
	Root::ValueWriter tmp;
	if( !d_mdl.saveTo( tmp ) )
		return false;
	return Root::StreamEnvelope::pack( vw, tmp, d_mdl.getAtoms() );
}

bool OutlineTreeModel::saveTo( Stream::DataWriter& vw ) const
{
	return d_mdl.saveTo( vw );
}
