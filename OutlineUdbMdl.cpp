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

#include "OutlineUdbMdl.h"
#include "HtmlToOutline.h"
#include "OutlineItem.h"
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <QtDebug>
#include <QStringList>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QUrlQuery>
#include "OutlineUdbStream.h"
#include "TextToOutline.h"
using namespace Oln;
using namespace Udb;

const char* OutlineUdbMdl::s_mimeOutline = "application/crossline/items";


static const int s_batch = 50; // Wenn es weniger sind als eine Seitenlänge gibt es Probleme

static QMap<quint32, QPair<QString,QString> > s_pix; // typeId -> [path,typeCode]
static QMap<QString,quint32> s_typeCodes; // typeCode -> typeId

quint64 OutlineUdbMdl::UdbSlot::getId() const
{
	return d_item.getOid();
}

bool OutlineUdbMdl::UdbSlot::isTitle(const OutlineMdl* mdl ) const
{
	return d_item.getValue( OutlineItem::AttrIsTitle ).getBool();
}

bool OutlineUdbMdl::UdbSlot::isExpanded(const OutlineMdl* mdl ) const
{
	return d_item.getValue( OutlineItem::AttrIsExpanded ).getBool();
}

bool OutlineUdbMdl::UdbSlot::isReadOnly(const OutlineMdl* mdl ) const
{
	return d_item.getValue( OutlineItem::AttrIsReadOnly ).getBool();
}

bool OutlineUdbMdl::UdbSlot::isAlias(const OutlineMdl* mdl ) const
{
	return d_item.getValue( OutlineItem::AttrAlias ).isOid();
}

QVariant OutlineUdbMdl::UdbSlot::getData(const OutlineMdl* mdl,int role) const
{
	//const OutlineUdbMdl* umdl = static_cast<const OutlineUdbMdl*>(mdl);
	if( role == Qt::DisplayRole || role == Qt::EditRole )
	{
		Stream::DataCell v;
		Stream::DataCell::OID oid = d_item.getValue( OutlineItem::AttrAlias ).getOid();
		if( oid )
		{
            // Falls das Item ein Alias ist
			Udb::Obj o = d_item.getObject( oid );
			if( !o.isNull() )
                // Das Alias zeigt auf ein gültiges Objekt
				o.getValue( OutlineItem::AttrText, v );
			else
                // Das Objekt, auf welches das Alias zeigt, ist unbekannt
				d_item.getValue( OutlineItem::AttrText, v ); // RISK: ist das wirklich sinnvoll?
		}else
            // Falls das Item kein Alias ist
			d_item.getValue( OutlineItem::AttrText, v );
		switch( v.getType() )
		{
		case Stream::DataCell::TypeAscii:
		case Stream::DataCell::TypeLatin1:
		case Stream::DataCell::TypeString:
			return v.toString();
		case Stream::DataCell::TypeBml:
			return QVariant::fromValue( OutlineUdbMdl::Bml( v.getBml() ) );
		case Stream::DataCell::TypeHtml:
			return QVariant::fromValue( OutlineUdbMdl::Html( v.getStr() ) );
        default:
            break;
		}
	}else if( role == Qt::DecorationRole )
	{
		int type = d_item.getType();
		Stream::DataCell::OID oid = d_item.getValue( OutlineItem::AttrAlias ).getOid();
		if( oid )
		{
			Udb::Obj o = d_item.getObject( oid );
			if( !o.isNull() )
				type = o.getType();
		}
		QPixmap pix = getPixmap( type );
		if( !pix.isNull() )
			return pix;
	}else if( role == IdentRole )
	{
		// Wir verwenden hier tatsächlich zuerst die ID des Alias und dann des Originals
		QString id = d_item.getString( OutlineItem::AttrAltIdent, true );
		if( id.isEmpty() )
			id = d_item.getString( OutlineItem::AttrIdent, true );
		if( id.isEmpty() )
		{
			Udb::Obj alias = d_item.getValueAsObj( OutlineItem::AttrAlias );
			if( !alias.isNull() )
			{
				id = alias.getString( OutlineItem::AttrAltIdent, true );
				if( id.isEmpty() )
					id = alias.getString( OutlineItem::AttrIdent, true );
			}
		}
		return id;
	}
	return QVariant();
}

OutlineUdbMdl::OutlineUdbMdl( QObject* p ):OutlineMdl(p),d_blocked(false)
{
}

OutlineUdbMdl::UdbSlot* OutlineUdbMdl::getSlot( const QModelIndex& index ) const
{
	return static_cast<UdbSlot*>( OutlineMdl::getSlot( index ) );
}

OutlineUdbMdl::UdbSlot* OutlineUdbMdl::findSlot( quint64 id ) const
{
    return static_cast<UdbSlot*>( OutlineMdl::findSlot( id ) );
}

void OutlineUdbMdl::collapse(UdbSlot * s)
{
    if( !s->getSubs().isEmpty() )
    {
        s->d_item.setValue( OutlineItem::AttrIsExpanded, Stream::DataCell().setBool( false ) );
        const int count = s->getSubs().count();
        for( int i = 0; i < count; i++ )
            collapse( static_cast<UdbSlot*>( s->getSubs()[i] ) );
    }
}

void OutlineUdbMdl::setOutline( const Udb::Obj& doc )
{
	d_outline = doc;
	UdbSlot* s = new UdbSlot();
	if( !d_outline.isNull() )
		s->d_item = d_outline; // wozu eigentlich?
	add( s, 0 );
}

bool OutlineUdbMdl::isReadOnly() const
{
	if( d_outline.isNull() )
		return true;
	else
		return d_outline.getValue( OutlineItem::AttrIsReadOnly ).getBool() || d_outline.getDb()->isReadOnly();
}

bool OutlineUdbMdl::hasChildren( const QModelIndex & parent ) const
{
	if( d_outline.isNull() )
		return false;
	UdbSlot* p = getSlot( parent );
	Q_ASSERT( p != 0 );
	if( !p->getSubs().isEmpty() )
		return true;
	else
		return !p->d_item.getFirstObj().isNull();
}

bool OutlineUdbMdl::canFetchMore ( const QModelIndex & parent ) const
{
	if( d_outline.isNull() )
		return false;
	UdbSlot* p = getSlot( parent );
	const int n = fetch( p, 1 );
	return n > 0;
}

int OutlineUdbMdl::fetch( UdbSlot* p, int max, ObjList* l ) const
{
	Q_ASSERT( p != 0 );
	Obj o;
	if( p->getSubs().isEmpty() )
	{
		o = p->d_item.getFirstObj();
	}else
	{
		o = static_cast<UdbSlot*>( p->getSubs().last() )->d_item;
		if( !o.isNull() )
		{
			if( !o.next() )
				return 0;
		}
	}

	int n = 0;
	if( !o.isNull() ) do
	{
		if( o.getType() == OutlineItem::TID )
		{
            if( findSlot( o.getOid() ) )
            {
                // TEST
				//qDebug() << "*** already present" << o.getValue( OutlineItem::AttrText ).toPrettyString();
				//qDebug() << "with parent" << d_outline.getObject( s->getSuper()->getId() ).getValue( OutlineItem::AttrText ).toPrettyString();
                // Breche vorzeitig ab, da fetch Objekte laden will, die bereits da sind; das kann bei
                // Indent/Unindent/Drag/Drop passieren
                return n;
            }
            if( l )
				l->append( o );
			n++;
		}
	}while( o.next() && ( max == 0 || n < max ) );
	return n;
}

void OutlineUdbMdl::create( UdbSlot* p, const ObjList& l )
{
	for( int i = 0; i < l.size(); i++ )
	{
        // TEST
		//qDebug() << "fetching" << l[i].getValue( OutlineItem::AttrText ).toPrettyString();
		//qDebug() << "to parent" << d_outline.getObject( p->getId() ).getValue( OutlineItem::AttrText ).toPrettyString();
		UdbSlot* s = new UdbSlot();
		s->d_item = l[i];
		Q_ASSERT( p != 0 );
		add( s, p );
	}
}

void OutlineUdbMdl::fetchMore ( const QModelIndex & parent )
{
	// Wird von QTreeView eigenartigerweise bei onUpdate::Deaggregate::beginRemoveRow aufgerufen.
	if( d_blocked )
	{
        // TEST
        //qDebug() << "calling fetchMore during Deaggregate";
		return;
	}
	fetchLevel( parent, parent.isValid() ); 
	// Da QTreeView nur bei expand fetchMore aufruft, laden wir gleich alles
}

void OutlineUdbMdl::fetchLevel( const QModelIndex & parent, bool all )
{
	if( d_outline.isNull() )
		return;

	UdbSlot* p = getSlot( parent );
	ObjList l;
    fetch( p, (all)?0:s_batch, &l );
	if( l.isEmpty() )
		return;

	beginInsertRows( parent, p->getSubs().size(), p->getSubs().size() + l.size() - 1 );
	create( p, l );
	endInsertRows();
}

void _fetchAll( OutlineUdbMdl* mdl, const QModelIndex & parent )
{
	mdl->fetchLevel( parent, true );
	const int count = mdl->rowCount( parent );
	for( int row = 0; row < count; row++ )
		_fetchAll( mdl, mdl->index( row, 0, parent ) );
}

void OutlineUdbMdl::fetchAll()
{
    _fetchAll( this, QModelIndex() );
}

void OutlineUdbMdl::collapseAll()
{
    const int count = rowCount();
    for( int i = 0; i < count; i++ )
        collapse(getSlot(index( i, 0 )));
    d_outline.commit();
    clearCache(QModelIndex());
}

void OutlineUdbMdl::onDbUpdate( Udb::UpdateInfo info )
{
	if( d_outline.isNull() )
		return;
	switch( info.d_kind )
	{
	case UpdateInfo::DbClosing:
		setOutline( Obj() );
		break;
	case UpdateInfo::ValueChanged:
		{
			const QModelIndex i = getIndex( info.d_id );
			if( i.isValid() && 
				( info.d_name == OutlineItem::AttrText || info.d_name == OutlineItem::AttrIsTitle || info.d_name == OutlineItem::AttrIsReadOnly ) )
			{
				emit dataChanged( i, i );
			}
			// TODO: update, wenn ein Zielobjekt eines Alias-Items geändert wurde?
		}
		break;
	case UpdateInfo::Deaggregated:
		{
			const QModelIndex parentIndex = getIndex( info.d_parent );
			if( !parentIndex.isValid() && info.d_parent != d_outline.getOid() )
				break; // Das Parent-Objekt ist nicht dieses Outline
			const QModelIndex indexToRemove = getIndex( info.d_id );
			if( indexToRemove.isValid() )
			{
                // Wenn Objekt bereits gelöscht wurde oder gar nicht da war, müssen wir nichts tun
				UdbSlot* slotToRemove = getSlot( indexToRemove );
				d_blocked = true; // Nötig, da irgend jemand bei beginRemoveRows fetchMore aufruft
                                    // RISK Seit 15.1.12 nie mehr beobachtet!
                beginRemoveRows( parentIndex, indexToRemove.row(), indexToRemove.row() );
                // TEST
				//qDebug() << "remove" << d_outline.getObject( slotToRemove->getId() ).getValue( OutlineItem::AttrText ).toPrettyString();
				//qDebug() << "from parent" << d_outline.getObject( slotToRemove->getSuper()->getId() ).getValue( OutlineItem::AttrText ).toPrettyString();
				remove( slotToRemove );
				endRemoveRows();
				d_blocked = false;
			}
		}
		break;
	case UpdateInfo::Aggregated:
		{
			const QModelIndex parentIndex = getIndex( info.d_parent );
			if( !parentIndex.isValid() && info.d_parent != d_outline.getOid() )
				break; // Das Parent-Objekt ist noch nicht bekannt. Fetch offensichtlich noch pendent.
			UdbSlot* parentSlot = getSlot( parentIndex );
			Q_ASSERT( parentSlot != 0 );
            if( findSlot( info.d_id ) )
            {
                // TEST
				//qDebug() << "*** already loaded" << d_outline.getObject( info.d_id ).getValue( OutlineItem::AttrText ).toPrettyString();
				//qDebug() << "to parent" << d_outline.getObject( s->getSuper()->getId() ).getValue( OutlineItem::AttrText ).toPrettyString();
                break; // fetch hat das Objekt bereits geladen; wir müssen nicht nochmals
            }
			Udb::Obj objToAdd = d_outline.getObject( info.d_id );
			Q_ASSERT( !objToAdd.isNull() );
			if( objToAdd.getType() != OutlineItem::TID )
				break; // Es können auch andere Objekte aggregiert sein, die nichts ins Outline gehören.
			if( parentSlot->getSubs().isEmpty() )
			{
				// wir vermuten in diesem Fall, dass Fetch noch nicht stattgefunden hat.
				fetchLevel( parentIndex );
				break; 
			}
            // TEST
			//qDebug() << "add" << d_outline.getObject( info.d_id ).getValue( OutlineItem::AttrText ).toPrettyString();
			//qDebug() << "to parent" << d_outline.getObject( parentSlot->getId() ).getValue( OutlineItem::AttrText ).toPrettyString();
			if( info.d_before )
			{
				UdbSlot* beforeSlot = findSlot( info.d_before );
				const int row = parentSlot->getSubs().indexOf( beforeSlot );
				if( row < 0 )
					break; // TODO: kann -1 sein, wenn CRTL+R auf geschlossenem Element
				beginInsertRows( parentIndex, row, row );
				UdbSlot* newSlot = new UdbSlot();
				newSlot->d_item = objToAdd;
				add( newSlot, parentSlot, row );
				endInsertRows();
			}else
			{
				const int size = parentSlot->getSubs().size();
				beginInsertRows( parentIndex, size, size );
				UdbSlot* newSlot = new UdbSlot();
				newSlot->d_item = objToAdd;
				add( newSlot, parentSlot );
				endInsertRows();
			}
		}
		break;
	case UpdateInfo::ObjectErased:
		if( info.d_id == d_outline.getOid() )
			setOutline( Obj() );
		break;
	}
}

bool OutlineUdbMdl::setData ( const QModelIndex & index, const QVariant & value, int role )  
{
	if( d_outline.isNull() )
		return false;
	if( role == Qt::SizeHintRole )
	{
		// Trick um Height-Cache zu invalidieren
		emit dataChanged( index, index );
		return true;
	}else if( role == Qt::EditRole )
	{
		UdbSlot* s = getSlot( index );
		if( !s->d_item.getValue( OutlineItem::AttrAlias ).isOid() )
		{
			// Den Wert von Alias-Items können wir nicht direkt setzen
			Stream::DataCell v;
			if( value.canConvert<Oln::OutlineUdbMdl::Bml>() )
				v.setBml( value.value<Oln::OutlineUdbMdl::Bml>().d_bml );
			else if( value.canConvert<Oln::OutlineUdbMdl::Html>() )
				v.setHtml( value.value<Oln::OutlineUdbMdl::Html>().d_html );
			else
				v.setString( value.toString() );
			OutlineItem::updateBackRefs( s->d_item, v );
			s->d_item.setValue( OutlineItem::AttrText, v );
			s->d_item.setValue( OutlineItem::AttrModifiedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
			d_outline.commit();
			emit dataChanged( index, index );
		}
		return true;
	}else if( role == ExpandedRole )
	{
		UdbSlot* s = getSlot( index );
		const bool exp = value.toBool();
		s->d_item.setValue( OutlineItem::AttrIsExpanded, Stream::DataCell().setBool( exp ) );
		d_outline.commit();
        if( !exp )
			clearCache( index );
		return true;
	}
	return false;
}

Udb::Obj OutlineUdbMdl::getItem( const QModelIndex & index ) const
{
	if( d_outline.isNull() )
		return Udb::Obj();
	UdbSlot* p = getSlot( index );
	if( p == 0 )
		return Udb::Obj();
	else
        return p->d_item;
}

Obj OutlineUdbMdl::getItem(int row, const QModelIndex &parent) const
{
    if( d_outline.isNull() )
		return Udb::Obj();
    UdbSlot* p = getSlot( parent );
    // Diese Funktion ist robust gegenüber row ausserhalb Gültigkeit, so wie sie bei loadFromText/Html erscheinen
    if( row >= 0 && row < p->getSubs().size() )
        return d_outline.getObject( p->getSubs()[row]->getId() );
    else
        return Udb::Obj();
}

QModelIndex OutlineUdbMdl::getIndex( quint64 oid, bool fetch ) const
{
    if( !fetch )
        return OutlineMdl::getIndex( oid );
	if( d_outline.isNull() || oid == 0 || d_outline.getOid() == oid )
		return QModelIndex();

	UdbSlot* s = findSlot( oid ); // Best Guess
	if( s != 0 )
	{
		// Das Objekt wurde bereits geladen.
		const int row = s->getSuper()->getSubs().indexOf( s );
		Q_ASSERT( row >= 0 );
		return createIndex( row, 0, s );
	}
	// Das Objekt wurde noch nicht geladen
	QModelIndex res;
	QList<quint64> path;
	Obj o = d_outline.getObject( oid );
	while( !o.isNull() && d_outline.getOid() != o.getOid() )
	{
		path.prepend( o.getOid() );
		o = o.getParent();
	}
	if( o.isNull() )
		return QModelIndex();
	// path beinhaltet nun Grossvater, Vater, Sohn, ...
	for( int i = 0; i < path.size(); i++ )
	{
		// Suche von oben nach unten und lade jeweils die Levels soweit erforderlich
		QModelIndex sub = findInLevel( res, path[i] );
		if( !sub.isValid() )
			break;
		res = sub;
	}
	return res;
}

QModelIndex OutlineUdbMdl::findInLevel( const QModelIndex & parent, quint64 oid ) const
{
	if( d_outline.isNull() || oid == 0 || d_outline.getOid() == oid )
		return QModelIndex();
	UdbSlot* p = getSlot( parent );
	int i = 0;
	do
	{
		if( i >= p->getSubs().size() )
		{
			ObjList l;
			const int n = fetch( p, s_batch, &l );
			if( n == 0 )
				break; // Nichts gefunden auf Level, trotz vollständigem Fetch
            OutlineUdbMdl* mdl = const_cast<OutlineUdbMdl*>( this );
			mdl->beginInsertRows( parent, p->getSubs().size(), p->getSubs().size() + n - 1 );
			mdl->create( p, l );
			mdl->endInsertRows();
		}
		if( p->getSubs()[i]->getId() == oid )
			return createIndex( i, 0, p->getSubs()[i] );
		i++;
	}while( true ); // NOTE: ansonsten keine Chance für fetch: i < p->getSubs().size() );
	return QModelIndex();
}

QMimeData * OutlineUdbMdl::mimeData ( const QModelIndexList & indexes ) const
{
	// NOTE: alle Elemente von indexes müssen denselben Parent haben, damit sie hier verarbeitet werden.

	QMimeData* mimeData = new QMimeData();
	if( indexes.isEmpty() || d_outline.isNull() )
		return mimeData;

	// Die Liste kommt z.T. komisch sortiert.
	QModelIndexList idx = indexes;
	qSort( idx.begin(), idx.end() );

	const QModelIndex parent = indexes.first().parent();

	// Schreibe ItemRefs und Outline
	Stream::DataWriter out1;
	Stream::DataWriter out2;
	QString out3;
    QList<QUrl> urls;
	out1.writeSlot( Stream::DataCell().setUuid( d_outline.getDb()->getDbUuid() ) );
	for( int i = 0; i < idx.size(); i++ )
	{
		if( idx[i].parent() == parent )
		{
			Udb::Obj o = getItem( idx[i] );
			out1.writeSlot( o );
			OutlineUdbStream::writeItem( out2, o, i == 0 );
			out3 += TextToOutline::toText( o );
			urls.append( objToUrl( o ) );
		}
	}
    mimeData->setData( QLatin1String( Udb::Obj::s_mimeObjectRefs ), out1.getStream() );
	mimeData->setData( QLatin1String( s_mimeOutline ), out2.getStream() );
	mimeData->setText( out3 );
    if( !urls.isEmpty() )
        mimeData->setUrls( urls );

	d_outline.getTxn()->commit(); // RISK: wegen getUuid in OutlineStream
	return mimeData;
}

QStringList OutlineUdbMdl::mimeTypes () const
{
	QStringList l;
	l << QLatin1String( Udb::Obj::s_mimeObjectRefs ) << QLatin1String( s_mimeOutline );
	return l;
}

/*
static void dump( const Udb::Obj& o, int level )
{
	QByteArray indent( level, '\t' );
	qDebug( "%s %d \"%s\"", indent.data(), o.getValue( OutlineUdbMdl::s_attrIsTitle ).getBool(),
		o.getValue( OutlineUdbMdl::s_attrText ).toString().simplified().toLatin1().data() );
	Udb::Obj sub = o.getFirstObj();
	if( !sub.isNull() ) do
	{
		dump( sub, level + 1 );
	}while( sub.next() );
}
*/

QList<Udb::Obj> OutlineUdbMdl::loadFromText( const QString& text, int row, const QModelIndex & parent )
{
	const Udb::Obj p = getItem( parent );
	if( p.isNull() )
		return QList<Udb::Obj>();
	Udb::Obj before;
	if( row != -1 )
		before = getItem( row, parent );

	QList<Udb::Obj> res = TextToOutline::parse( text, d_outline.getTxn(), d_outline.getOid() );
	if( res.isEmpty() )
	{
		d_outline.getTxn()->rollback();
		return QList<Udb::Obj>();
	}
	for( int i = 0; i < res.size(); i++ )
	{
		res[i].aggregateTo( p, before );
	}
	d_outline.commit();
	//dump( o, 0 );
	return res;

}

Udb::Obj OutlineUdbMdl::loadFromHtml( const QString& html, int row, const QModelIndex & parent, bool rooted )
{
	const Udb::Obj p = getItem( parent );
	if( p.isNull() )
		return Udb::Obj();
	Udb::Obj before;
	if( row != -1 )
		before = getItem( row, parent );

	HtmlToOutline hi;
	Udb::Obj o = hi.parse( html, d_outline.getTxn(), d_outline.getOid() );
	if( o.isNull() )
	{
		d_outline.getTxn()->rollback();
		return Udb::Obj();
	}
	if( rooted )
		o.aggregateTo( p, before );
	else
	{
		Udb::Obj sub = o.getFirstObj();
		while( !sub.isNull() )
		{
			Udb::Obj old = sub;
			sub = sub.getNext();
			old.aggregateTo( p, before );
		}
		OutlineItem::erase( o );
		o = p;
	}
	d_outline.commit();
	//dump( o, 0 );
	return o;
}

bool OutlineUdbMdl::moveOrLinkRefs( const QByteArray& bml, const Udb::Obj& parent, const Udb::Obj& before, 
								   bool link, bool docLink )
{
	Stream::DataReader in( bml );
	if( in.nextToken() == Stream::DataReader::Slot && in.readValue().getUuid() == d_outline.getDb()->getDbUuid() )
	{
		while( in.nextToken() == Stream::DataReader::Slot )
		{
			Udb::Obj o = d_outline.getObject( in.readValue().getOid() );
			// Wenn wir aus einem anderen Outline droppen, kann es kein Move sein, sondern nur Link
			const bool linkOverride = d_outline.getOid() != o.getValue( OutlineItem::AttrHome ).getOid();
			if( !o.isNull() )
			{
				if( link || linkOverride ) // link
				{
					Udb::Obj a = d_outline.createObject( OutlineItem::TID );
					a.setValue( OutlineItem::AttrHome, d_outline );
					a.setValue( OutlineItem::AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );

					if( docLink && o.getType() == OutlineItem::TID )
					{
						// Falls nicht das Item, sondern das übergeordnete Outline als Link eingefügt werden soll.
						o = o.getValueAsObj( OutlineItem::AttrHome );
						if( o.isNull() )
							continue; // RISK: ignoriere das fehlerhafte Item
					}
					// Wenn das Linkziel selber ein Alias ist, verwende dessen Referenz (inkl. Text falls vorhanden)
					Stream::DataCell ref = o.getValue( OutlineItem::AttrAlias );
					if( ref.isOid() )
					{
						a.setValue( OutlineItem::AttrAlias, ref );
						Stream::DataCell v = o.getValue( OutlineItem::AttrText );
						OutlineItem::updateBackRefs( a, v ); // TODO: macht das Sinn?
						a.setValue( OutlineItem::AttrText, v );
					}else
						a.setValue( OutlineItem::AttrAlias, o );
					a.aggregateTo( parent, before );
					if( parent.equals( d_outline ) )
						a.setValue( Outline::AttrHasItems, Stream::DataCell().setBool(true) );
				}else // Move
				{
					// TODO: Cycles entdecken
					o.deaggregate();
					o.aggregateTo( parent, before );
					o.setValue( OutlineItem::AttrModifiedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
				}
			}
		}
		d_outline.commit();
		return true;
	}
	return false;
}

bool OutlineUdbMdl::dropBml( const QByteArray& bml, const QModelIndex& parent, int row, Action action )
{
	const Udb::Obj p = getItem( parent );
	if( p.isNull() )
		return false;
	Udb::Obj before;
	if( row != -1 )
		before = getItem( row, parent );
	switch( action )
	{
	case MoveItems:
		return moveOrLinkRefs( bml, p, before, false, false );
	case LinkItems:
		return moveOrLinkRefs( bml, p, before, true, false );
	case LinkDocs:
		return moveOrLinkRefs( bml, p, before, true, true );
	case CopyItems:
		{
			Stream::DataReader in( bml );
			if( !OutlineUdbStream::readItems( in, p, before ).isEmpty() )
			{
				d_outline.getTxn()->rollback();
				return false;
			}
			d_outline.commit();
			return true;
		}
	}
	return false;
}

bool OutlineUdbMdl::dropMimeData ( const QMimeData * data, Qt::DropAction action, 
							   int row, int column, const QModelIndex & parent )
{
    Q_UNUSED(column);
	if( action == Qt::IgnoreAction )
		return true;
	if( ( action == Qt::MoveAction || action == Qt::LinkAction ) && 
		data->hasFormat( QLatin1String( Udb::Obj::s_mimeObjectRefs ) ) )
	{
		if( action == Qt::MoveAction )
            return dropBml( data->data( QLatin1String( Udb::Obj::s_mimeObjectRefs ) ), parent, row, MoveItems );
		else 
            return dropBml( data->data( QLatin1String( Udb::Obj::s_mimeObjectRefs ) ), parent, row, LinkItems );
	}
	if( data->hasFormat( QLatin1String( s_mimeOutline ) ) )
		return dropBml( data->data( QLatin1String( s_mimeOutline ) ), parent, row, CopyItems );
	return false;
}

QPixmap OutlineUdbMdl::getPixmap( quint32 typeId )
{
	return QPixmap( s_pix.value( typeId ).first );
}

QString OutlineUdbMdl::getPixmapPath( quint32 typeId )
{
    return s_pix.value( typeId ).first;
}

QString OutlineUdbMdl::getTypeCode(quint32 typeId)
{
    return s_pix.value( typeId ).second;
}

quint32 OutlineUdbMdl::getTypeFromCode(const QString &typeCode)
{
    return s_typeCodes.value( typeCode, 0 );
}

void OutlineUdbMdl::registerPixmap( quint32 typeId, const QString& pix, const QString& typeCode )
{
	s_pix[ typeId ] = qMakePair( pix, typeCode );
    if( !typeCode.isEmpty() )
        s_typeCodes[typeCode] = typeId;
}

void OutlineUdbMdl::writeObjectUrls(QMimeData *data, const QList<Obj> & objs)
{
    if( objs.isEmpty() )
        return;
    QList<QUrl> urls;
    foreach( Udb::Obj o, objs )
		urls.append( objToUrl( o ) );
    if( !urls.isEmpty() )
        data->setUrls( urls );
}

QUrl OutlineUdbMdl::objToUrl(const Obj & o)
{
	if( o.isNull() )
		return QUrl();
	// URI-Schema gem. http://en.wikipedia.org/wiki/URI_scheme und http://tools.ietf.org/html/rfc3986 bzw. http://tools.ietf.org/html/rfc6068
	// Format: xoid:49203@348c2483-206a-40a6-835a-9b9753b60188?ty=MO;id=MO1425;txt=lkjd aölkj adfölkj adsf
	QUrl url = Udb::Obj::objToUrl(o);
	if( true )
	{
        QUrlQuery q;
        q.setQueryDelimiters( '=', ';' );
		QList<QPair<QString, QString> > items;
		QString ty = getTypeCode( o.getType() );
		if( !ty.isEmpty() )
			items.append( qMakePair( tr("ty"), ty ) );
		if( o.hasValue( OutlineItem::AttrAltIdent ) )
			items.append( qMakePair( tr("id"), o.getString(OutlineItem::AttrAltIdent ) ) );
		items.append( qMakePair( tr("txt"), o.getString( OutlineItem::AttrText ) ) );
        q.setQueryItems( items );
        url.setQuery(q);
	}
	return url;
}
