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

#include "OutlineUdbCtrl.h"
#include "OutlineStream.h"
#include "OutlineItem.h"
#include "EditUrlDlg.h"
#include <Udb/Database.h>
#include <GuiTools/UiFunction.h>
#include <GuiTools/AutoShortcut.h>
#include <Txt/TextOutStream.h>
#include <Txt/TextCursor.h>
#include <QApplication>
#include <QMouseEvent>
#include <QClipboard>
#include <QKeySequence>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <cassert>
#include <QtDebug>
#include <QMimeData>
#include <QUrlQuery>
using namespace Oln;
using namespace Stream;
using namespace Txt;

Link OutlineUdbCtrl::s_itemDefault = Link( false, false, true, false, 50, true, true );
Link OutlineUdbCtrl::s_objectDefault = Link( true, true, true, false, 0, false, true );

static void _expand( QTreeView* tv, OutlineUdbMdl* mdl, const QModelIndex& index, bool expand )
{
	// Analog zu QTreeView
	if( expand )
		mdl->fetchLevel( index, true );
	const int count = mdl->rowCount( index );
	if( count == 0 )
		return;

	if( expand )
		// Öffne von oben nach unten
		tv->setExpanded( index, true );
	for( int i = 0; i < count; i++ )
		_expand( tv, mdl, mdl->index( i, 0, index ), expand );
	if( !expand )
		// Gehe zuerst runter, dann von unten nach oben schliessen
		tv->setExpanded( index, false );
}

OutlineUdbCtrl::OutlineUdbCtrl( OutlineTree* p, Udb::Transaction* txn ):
    OutlineCtrl( p, &d_linkRenderer ), d_txn( txn ), d_linkRenderer( txn )
{
	d_mdl = new OutlineUdbMdl( p );
	d_txn->getDb()->addObserver( d_mdl, SLOT(onDbUpdate( Udb::UpdateInfo )), false );
	setModel( d_mdl );
    d_txn->getDb()->addObserver( this, SLOT(onDbUpdate( Udb::UpdateInfo )), false );
	// Nein connect( p, SIGNAL( returnPressed() ), this, SLOT( onAddNextImp() ) );

    connect( d_deleg->getEditCtrl(), SIGNAL(anchorActivated(QByteArray,bool)),
			 this, SLOT( followLink(QByteArray,bool) ) );

}

OutlineUdbCtrl* OutlineUdbCtrl::create( QWidget* p, Udb::Transaction* txn )
{
	OutlineTree* tree = new OutlineTree( p );
	OutlineUdbCtrl* ctrl = new OutlineUdbCtrl( tree, txn );
    connect( tree, SIGNAL( identDoubleClicked() ), ctrl, SLOT( followAlias() ) );
    return ctrl;
}

void OutlineUdbCtrl::addItemCommands(Gui::AutoMenu * sub)
{
    sub->addCommand( tr("Add next"),  this, SLOT(onAddNext()), tr("Return"), true );
	sub->addCommand( tr("Add right"),  this, SLOT(onAddRight()), tr("CTRL+R"), true );
	sub->addCommand( tr("Add left"),  this, SLOT(onAddLeft()), tr("CTRL+L"), true );
	sub->addCommand( tr("Remove..."),  this, SLOT(onRemove()), tr("CTRL+D"), true );
	sub->addSeparator();
	sub->addCommand( tr("Set Title"),  this, SLOT(onSetTitle()), tr("CTRL+T"), true )->setCheckable(true);
	sub->addCommand( tr("Set Readonly"),  this, SLOT(onReadOnly()) )->setCheckable(true);
	sub->addCommand( tr("Properties..."),  this, SLOT(onEditProps()), tr("ALT+Return"), true );
	sub->addCommand( tr("Indent"), this, SLOT(onIndent()), tr("TAB"), true );
    new Gui::AutoShortcut( tr("SHIFT+RIGHT"), getTree(), this, SLOT(onIndent()) );
	sub->addCommand( tr("Unindent"),  this, SLOT(onUnindent()), tr("SHIFT+TAB"), true );
    new Gui::AutoShortcut( tr("SHIFT+LEFT"), getTree(), this, SLOT(onUnindent()) );
	sub->addCommand( tr("Move up"),  this, SLOT(onMoveUp()), tr("SHIFT+UP"), true );
    sub->addCommand( tr("Move down"),  this, SLOT(onMoveDown()), tr("SHIFT+DOWN"), true );
    sub->addSeparator();
    sub->addCommand( tr("Follow Alias"), this, SLOT( onFollowAlias()) );
}

void OutlineUdbCtrl::addOutlineCommands(Gui::AutoMenu * pop)
{
    pop->addCommand( tr("Cut"), this, SLOT(onCut()), tr("CTRL+X"), true );
	pop->addCommand( tr("Copy"),  this, SLOT(onCopy()), tr("CTRL+C"), true );
	pop->addCommand( tr("Paste"),  this, SLOT(onPaste()), tr("CTRL+V"), true );
    pop->addCommand( tr("Paste Special"),  this, SLOT(onPasteSpecial()), tr("CTRL+SHIFT+V"), true );
	// Niemand braucht das: pop->addCommand( tr("Paste Document Link"), this, SLOT(onPasteDocLink()) );
	pop->addSeparator();
	pop->addCommand( tr("Expand all"),  this, SLOT(onExpandAll()), tr("CTRL+SHIFT+A"), true  );
	pop->addCommand( tr("Expand selected"),  this, SLOT(onExpandSubs()) );
	pop->addCommand( tr("Collapse all"),  this, SLOT(onCollapseAll()) );
    new Gui::AutoShortcut( tr("CTRL+S"), getTree(), this, SLOT(onSave()) );
}


Udb::Obj OutlineUdbCtrl::createSub( Udb::Obj& parent, const Udb::Obj& before )
{
	Udb::Obj newObj = parent.createObject( OutlineItem::TID );
	newObj.setValue( OutlineItem::AttrCreatedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
	Udb::Obj home = parent.getValueAsObj( OutlineItem::AttrHome );
	if( home.isNull() )
		home = d_mdl->getOutline();
	newObj.setValue( OutlineItem::AttrHome, home );
	newObj.aggregateTo( parent, before );
	if( parent.equals( home ) )
		parent.setValue( Outline::AttrHasItems, DataCell().setBool(true) );
	return newObj;
}

void OutlineUdbCtrl::setOutline( const Udb::Obj& o, bool setCurrent )
{
	if( d_deleg->isEditing() )
		d_deleg->closeEdit();
	d_mdl->setOutline( o );
	if( setCurrent )
	{
		d_mdl->fetchLevel( QModelIndex() );
		if( d_mdl->rowCount() )
			getTree()->setCurrentIndex( d_mdl->index( 0, 0 ) );
	}
}

bool OutlineUdbCtrl::addItem()
{
	if( getTree()->selectionModel()->selectedRows().size() <= 1 && !d_deleg->isReadOnly() )
	{
		onAddNextImp();
		return true;
	}else
		return false;
}

void OutlineUdbCtrl::onAddNext()
{
	ENABLED_IF( getTree()->selectionModel()->selectedRows().size() <= 1 && !d_deleg->isReadOnly() &&
		!d_mdl->getOutline().isNull() );
	onAddNextImp();
}

void OutlineUdbCtrl::onAddNextImp()
{
	if( d_deleg->isReadOnly() || d_mdl->getOutline().isNull() )
		return;
	QModelIndex parent;
	int newRow = 0;
	nextOrSub( parent, newRow );

	Udb::Obj successor = d_mdl->getItem( newRow, parent );
	Udb::Obj parentObj = d_mdl->getItem( parent );
	Udb::Obj newObj = createSub( parentObj, successor );
	newObj.commit();
	getTree()->goAndEdit( d_mdl->getIndex( newObj.getOid() ) );
}

void OutlineUdbCtrl::onAddRight()
{
	ENABLED_IF( getTree()->selectionModel()->selectedRows().size() == 1 && 
		getTree()->currentIndex().isValid() && !d_deleg->isReadOnly() );

	getTree()->setExpanded( getTree()->currentIndex(), true );
	Udb::Obj item = d_mdl->getItem( getTree()->currentIndex() );
	Udb::Obj sub = createSub( item, item.getFirstObj() );
	sub.commit();
	getTree()->goAndEdit( d_mdl->getIndex( sub.getOid() ) );
}

void OutlineUdbCtrl::onAddLeft()
{
	ENABLED_IF( getTree()->selectionModel()->selectedRows().size() == 1 && 
		getTree()->currentIndex().isValid() && getTree()->currentIndex().parent().isValid() && 
		!d_deleg->isReadOnly() );

	Udb::Obj cur = d_mdl->getItem( getTree()->currentIndex() );
	Udb::Obj par = cur.getParent();
	Udb::Obj parpar = par.getParent();
	assert( !parpar.isNull() );
	Udb::Obj sub = createSub( parpar, par.getNext() );
	sub.commit();
	getTree()->goAndEdit( d_mdl->getIndex( sub.getOid() ) );
}

void OutlineUdbCtrl::onRemove()
{
	ENABLED_IF( getTree()->selectionModel()->hasSelection() && !d_deleg->isReadOnly() );

	d_deleg->closeEdit();
	deleteSelection( getTree()->selectionModel()->selectedRows() );
}

void OutlineUdbCtrl::onSetTitle()
{
	Udb::Obj cur = d_mdl->getItem( getTree()->currentIndex() );
	CHECKED_IF( getTree()->selectionModel()->selectedRows().size() == 1 && !cur.isNull() && !d_deleg->isReadOnly(), 
		cur.getValue( OutlineItem::AttrIsTitle ).getBool() );
	const bool isTitle = cur.getValue( OutlineItem::AttrIsTitle ).getBool();

	d_deleg->writeData();
	cur.setValue( OutlineItem::AttrIsTitle, DataCell().setBool( !isTitle ) );
	cur.commit();
}

void OutlineUdbCtrl::onEditProps()
{
	OutlineItem cur = d_mdl->getItem( getTree()->currentIndex() );
	ENABLED_IF( getTree()->selectionModel()->selectedRows().size() == 1 && !cur.isNull() );

	const bool ro = d_deleg->isReadOnly() || cur.isReadOnly();
	if( !ro )
		d_deleg->writeData();
	EditPropertiesDlg dlg(getTree(), ro );
	if( dlg.edit( cur ) && !ro )
		d_deleg->readData();
}

QList<Udb::Obj> OutlineUdbCtrl::getSelectedItems(bool checkConnected) const
{
    QModelIndexList selected = getTree()->selectionModel()->selectedRows();
    qSort( selected );
    QList<Udb::Obj> res;
    foreach( QModelIndex i, selected )
    {
        res.append( d_mdl->getItem( i ) );
    }
    if( checkConnected )
    {
        for( int i = 1; i < res.size(); i++ )
            if( !res[i-1].getNext().equals( res[i] ) )
                return QList<Udb::Obj>();

    }
    return res;
}

void OutlineUdbCtrl::selectItems(const QList<Udb::Obj> &items)
{
    getTree()->selectionModel()->clearSelection();
    foreach( Udb::Obj item, items )
        getTree()->selectionModel()->select( d_mdl->getIndex( item.getOid() ),
											 QItemSelectionModel::Select | QItemSelectionModel::Rows );
}

void OutlineUdbCtrl::insertTocImp(const Udb::Obj &item, Udb::Obj &toc, int level)
{
	if( level == 0 )
		return;
	OutlineItem sub = item.getFirstObj();
	if( !sub.isNull() ) do
	{
		if( sub.getType() == OutlineItem::TID )
		{
			OutlineItem tocItem;
			if( sub.isTitle() )
			{
				tocItem = createSub( toc );
				Link link;
				link.d_db = toc.getDb()->getDbUuid();
				link.d_oid = sub.getOid();
				link.d_paraNumber = true;
				QTextDocument doc;
				Txt::TextCursor cur(&doc);
				cur.insertLink( link.writeTo(), QString() );
				Stream::DataWriter out;
				TextOutStream::writeTo( out, &doc );
				tocItem.setTextBml( out.getStream() );
			}else
				tocItem = toc;
			insertTocImp( sub, tocItem, level - 1 );
		}
	}while( sub.next() );
}

void OutlineUdbCtrl::onIndent()
{
	QList<Udb::Obj> toIndent = getSelectedItems();
	ENABLED_IF( !toIndent.isEmpty() && toIndent.first().getPrev().getType() == OutlineItem::TID &&
                !d_deleg->isReadOnly() );

    const quint64 cur = d_deleg->getEditOid();
	d_deleg->closeEdit();
    Udb::Obj newParent = toIndent.first().getPrev();
    const QModelIndex newParentIndex = d_mdl->getIndex( newParent.getOid() );
	d_mdl->fetchLevel( newParentIndex, true );
	getTree()->setExpanded( newParentIndex, true ); // Fetch Subs
    foreach( Udb::Obj item, toIndent )
        item.aggregateTo( newParent );
	toIndent.first().commit();
    selectItems( toIndent );
    getTree()->goAndEdit( d_mdl->getIndex( cur ) );
}

void OutlineUdbCtrl::onUnindent()
{
    QList<Udb::Obj> toUnindent = getSelectedItems(true);
	ENABLED_IF( !toUnindent.isEmpty() && toUnindent.first().getParent().getType() == OutlineItem::TID
                && !d_deleg->isReadOnly() );

    const quint64 cur = d_deleg->getEditOid();
	d_deleg->closeEdit();

	const Udb::Obj before = toUnindent.first().getParent().getNext();
	const Udb::Obj newParent = toUnindent.first().getParent().getParent();
	QList<Udb::Obj> nexts;
	Udb::Obj next = toUnindent.last().getNext();
	if( !next.isNull() ) do
	{
		nexts.append( next );
	}while( next.next() );
    if( !nexts.isEmpty() )
		toUnindent.last().setValue( OutlineItem::AttrIsExpanded, DataCell().setBool( true ) );
    foreach( Udb::Obj item, toUnindent )
        item.aggregateTo( newParent, before );
    foreach( Udb::Obj item, nexts )
        item.aggregateTo( toUnindent.last() );
	toUnindent.first().commit();
    selectItems( toUnindent );
    getTree()->goAndEdit( d_mdl->getIndex( cur ) );
}

void OutlineUdbCtrl::onMoveUp()
{
    QList<Udb::Obj> toMove = getSelectedItems(true);
	ENABLED_IF( !toMove.isEmpty() &&
                ( !toMove.first().getPrev().isNull() ||
				  toMove.first().getParent().getType() == OutlineItem::TID )
                && !d_deleg->isReadOnly() );

    const quint64 cur = d_deleg->getEditOid();
	d_deleg->closeEdit();

    Udb::Obj parent = toMove.first().getParent();
	Udb::Obj before = toMove.first().getPrev();
	if( before.isNull() )
	{
		before = parent;
		parent = parent.getParent();
	}
    foreach( Udb::Obj item, toMove )
        item.aggregateTo( parent, before );
	toMove.first().commit();
    selectItems( toMove );
	getTree()->goAndEdit( d_mdl->getIndex( cur ) );
}

void OutlineUdbCtrl::onMoveDown()
{
    QList<Udb::Obj> toMove = getSelectedItems(true);
	ENABLED_IF( !toMove.isEmpty() && toMove.last().getNext().getType() == OutlineItem::TID &&
                !d_deleg->isReadOnly() );

    const quint64 cur = d_deleg->getEditOid();
	d_deleg->closeEdit();
	const Udb::Obj parent = toMove.last().getParent();
	const Udb::Obj before = toMove.last().getNext().getNext();
    foreach( Udb::Obj item, toMove )
        item.aggregateTo( parent, before );
    toMove.first().commit();
    selectItems( toMove );
	getTree()->goAndEdit( d_mdl->getIndex( cur ) );
}

bool OutlineUdbCtrl::copyToClipBoard( bool cut )
{
	if( d_deleg->isEditing() ) 
	{
		// nur Text kopieren
		d_deleg->getEditor()->copy();
		if( cut && !d_deleg->getEditor()->isReadOnly() && d_deleg->getEditor()->getCursor().hasSelection()  )
		{
			d_deleg->getEditor()->getCursor().deleteCharOrSelection();
			d_deleg->getEditor()->invalidate();
		}
		return true;
	}else
	{
		// auch ganze Outline-Items kopieren
		QModelIndexList l = getTree()->selectionModel()->selectedRows();
		if( l.isEmpty() )
			return false;
		QMimeData* mime = getTree()->model()->mimeData( l );
		QApplication::clipboard()->setMimeData( mime );
		if( cut )
		{
			if( deleteSelection( l ) )
			{
                mime->removeFormat( Udb::Obj::s_mimeObjectRefs );
			}
		}
		return true;
	}
	return false;
}

bool OutlineUdbCtrl::deleteSelection( const QModelIndexList& l )
{
	if( d_deleg->isReadOnly() || l.isEmpty() )
		return false;
	const int res = QMessageBox::warning( getTree(), tr("Delete Items"), 
		tr("Do you really want to permanently delete the %1 selected items including subitems? This cannot be undone." ).arg( l.size() ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
	if( res == QMessageBox::No )
		return false;
	const QModelIndex parent = l.first().parent();
	bool topLevel = false;
	for( int i = 0; i < l.size(); i++ )
		if( l[i].parent() == parent )
		{
			Udb::Obj o = d_mdl->getItem( l[i] );
			if( o.getParent().equals( d_mdl->getOutline() ) )
				topLevel = true;
			OutlineItem::erase( o );
		}
	if( topLevel )
	{
		Udb::Obj o = d_mdl->getOutline();
		Outline::markHasItems( o );
	}
	d_txn->commit();
	return true;
}

bool OutlineUdbCtrl::loadFromHtml( const QString& path )
{
	if( d_deleg->isReadOnly() )
		return false;
	QFile f( path );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		// d_error = QString("cannot open '%1' for reading").arg(path);
		return false;
	}

	int newRow;
	QModelIndex parent;
	nextOrSub( parent, newRow );

	Udb::Obj o = d_mdl->loadFromHtml( OutlineCtrl::fetchHtml( f.readAll() ), newRow, parent );
	if( o.getValue( OutlineItem::AttrText ).isNull() )
	{
		QFileInfo info( path );
		o.setValue( OutlineItem::AttrText, DataCell().setString( info.baseName() ) );
		o.commit();
	}
	return true;
}

bool OutlineUdbCtrl::isFormatSupported(const QMimeData * mime)
{
	if( mime->hasFormat( Txt::TextInStream::s_mimeRichText ) ||
            mime->hasText() ||
            mime->hasHtml() ||
            mime->hasImage() ||
            mime->hasUrls() ||
            mime->hasFormat( OutlineUdbMdl::s_mimeOutline ) ||
            mime->hasFormat( Udb::Obj::s_mimeObjectRefs ) )
        return true;
    else
        return false;
}

bool OutlineUdbCtrl::pasteFromClipBoard(bool special)
{
	if( d_deleg->isReadOnly() )
		return false;
	if( d_deleg->isEditing() ) 
	{
		const bool ro = d_deleg->getEditor()->isReadOnly();
		const QMimeData* mime = QApplication::clipboard()->mimeData();
        const QUuid dbid = d_txn->getDb()->getDbUuid();
		if( mime->hasFormat( Txt::TextInStream::s_mimeRichText ) && !ro )
		{
			Stream::DataReader r( mime->data( Txt::TextInStream::s_mimeRichText ) );
            if( special )
            {
                d_deleg->getEditor()->getCursor().insertText( r.extractString() );
            }else
            {
                d_deleg->getEditor()->getCursor().beginEditBlock();
                Txt::TextInStream in( d_deleg->getEditor()->getCursor().getStyles(),
                                      d_deleg->getLinkRenderer() );
                in.readFromTo( r, d_deleg->getEditor()->getCursor(), true );
                d_deleg->getEditor()->getCursor().endEditBlock();
            }
            d_deleg->getEditor()->invalidate();
			return true;
        }if( mime->hasFormat( Udb::Obj::s_mimeObjectRefs ) && !ro &&
                Udb::Obj::isLocalObjectRefs( mime, d_txn ) )
		{
            QList<Udb::Obj> objs = Udb::Obj::readObjectRefs( mime, d_txn );
            d_deleg->getEditor()->getCursor().beginEditBlock();
            foreach( Udb::Obj o, objs )
            {
                Link link;
				if( o.getType() != OutlineItem::TID )
                    link = s_objectDefault;
                    // Es wird ein Objekt referenziert
                else
                    link = s_itemDefault;
                    // Es wird Text referenziert
                link.d_oid = o.getOid();
                link.d_db = dbid;
				const bool res = d_linkRenderer.renderLink( d_deleg->getEditor()->getCursor(), link.writeTo() );
				Q_ASSERT( res );
            }
            d_deleg->getEditor()->getCursor().endEditBlock();
            d_deleg->getEditor()->invalidate();
            return true;
		}else if( mime->hasImage() && !ro )
		{
			// muss vor Urls behandelt werden, weil oft die Bilder von URL begleitet werden, wenn aus Browser kopiert.
			d_deleg->getEditor()->getCursor().insertImg( mime->imageData().value<QImage>() );
			d_deleg->getEditor()->invalidate();
			return true;
		}else if( mime->hasUrls() && !ro )
        {
            // Fix Google Chrome: dieser gibt bei "text/uri-list" die URL in der Form <url>%0A<url> zurück
            QList<QUrl> l = mime->urls();
            if( !l.isEmpty() &&
                    ( l.first().scheme().startsWith( QLatin1String( "http" ) ) ||
                      l.first().scheme().startsWith( QLatin1String( "ftp" ) ) ) )
                // Darum hier in Text verwandeln
                d_deleg->getEditor()->getCursor().insertUrl( QUrl( mime->text(), QUrl::TolerantMode ) );
            else
                for( int i = 0; i < l.size(); i++ )
                {
                    QString text;
					if( l[i].scheme() == Udb::Obj::s_xoidScheme )
                    {
                        QUrlQuery q(l[i]);
                        q.setQueryDelimiters( '=', ';' );
                        text = q.queryItemValue( tr("txt") );
                        QString str = q.queryItemValue( tr("id") );
                        if( !str.isEmpty() )
                        {
                            if( !text.isEmpty() )
                                text = str + QString(" ") + text;
                            else
                                text = str;
                        }
                        q.setQueryItems(QList<QPair<QString, QString> >());
                        l[i].setQuery(q);
                    }
                    d_deleg->getEditor()->getCursor().insertUrl( l[i], text );
                }
			d_deleg->getEditor()->invalidate();
			return true;
		}else if( mime->hasHtml() && !ro )
		{
			const QString html = OutlineCtrl::fetchHtml(mime);
            if( special )
				d_deleg->getEditor()->getCursor().insertText( DataCell::stripMarkup(html).simplified() );
            else
				d_deleg->insertHtml( html );
			d_deleg->getEditor()->invalidate();
			return true;
		}else if( mime->hasText() && !ro )
		{
			const QString text = mime->text();
			if( text.startsWith( QLatin1String( "http" ) ) || text.startsWith( QLatin1String( "ftp" ) ) )
			{
				QUrl url( text, QUrl::TolerantMode );
				if( url.isValid() )
				{
					d_deleg->getEditor()->getCursor().insertUrl( url );
					d_deleg->getEditor()->invalidate();
					return true;
				}
			}
			d_deleg->getEditor()->getCursor().insertText( text );
			d_deleg->getEditor()->invalidate();
			return true;
		}
		// fall through
	}
	QModelIndex parent;
	int newRow;
	nextOrSub( parent, newRow );
	if( special && ( QApplication::clipboard()->mimeData()->hasFormat( Udb::Obj::s_mimeObjectRefs ) ) )
    {
        QApplication::setOverrideCursor( Qt::WaitCursor );
        bool res = d_mdl->dropBml( QApplication::clipboard()->mimeData()->data(
            QLatin1String( Udb::Obj::s_mimeObjectRefs ) ), parent, newRow, OutlineUdbMdl::LinkItems );
        QApplication::restoreOverrideCursor();
        return res;
    }else if( QApplication::clipboard()->mimeData()->hasFormat( OutlineUdbMdl::s_mimeOutline ) )
	{
		QApplication::setOverrideCursor( Qt::WaitCursor );
		bool res = d_mdl->dropMimeData( QApplication::clipboard()->mimeData(),
                                        Qt::CopyAction, newRow, 0, parent );
		QApplication::restoreOverrideCursor();
		return res;
	}else if( QApplication::clipboard()->mimeData()->hasHtml() )
	{
		QApplication::setOverrideCursor( Qt::WaitCursor );
		d_mdl->loadFromHtml( OutlineCtrl::fetchHtml(QApplication::clipboard()->mimeData()), newRow, parent, false );
		QApplication::restoreOverrideCursor();
		return true;
	}else if ( QApplication::clipboard()->mimeData()->hasText() )
	{
		QApplication::setOverrideCursor( Qt::WaitCursor );
		d_mdl->loadFromText( QApplication::clipboard()->mimeData()->text(), newRow, parent );
		QApplication::restoreOverrideCursor();
		return true;
	}
	return false;
}

void OutlineUdbCtrl::onPasteSpecial()
{
    ENABLED_IF( isFormatSupported( QApplication::clipboard()->mimeData() ) && !d_deleg->isReadOnly() );
	pasteFromClipBoard(true);
}

void OutlineUdbCtrl::onInsertToc()
{
	Udb::Obj toc = d_mdl->getItem( getTree()->currentIndex() );
	ENABLED_IF( getTree()->selectionModel()->selectedRows().size() == 1 && !toc.isNull() && !d_deleg->isReadOnly() );

	bool ok;
    int level = QInputDialog::getInt( getTree(), tr("Create TOC"), tr("Select number of levels (0..all):"),
										  0, 0, 999, 1, &ok );
	if( !ok )
		return;
	if( level == 0 )
		level = -1;
	QApplication::setOverrideCursor( Qt::WaitCursor );
	insertTocImp( d_mdl->getOutline(), toc, level );
	toc.commit();
	_expand( getTree(), d_mdl, getTree()->currentIndex(), true );
	QApplication::restoreOverrideCursor();
}

void OutlineUdbCtrl::onEditUrl()
{
    ENABLED_IF( canFormat() && ( d_deleg->getEditor()->getCursor().isAnchor() ||
                d_deleg->getEditor()->getCursor().hasSelection() ) );

    if( d_deleg->getEditor()->getCursor().isUrl() )
        editUrl();
    else if( d_deleg->getEditor()->getCursor().isLink() )
    {
        Link l;
        if( !l.readFrom( d_deleg->getEditor()->getCursor().getLink() ) )
            return; // RISK

        Udb::Obj o = d_mdl->getOutline().getObject( l.d_oid );
		EditLinkDlg dlg( getTree() );
		if( dlg.edit( l, o.getType() == OutlineItem::TID ) )
        {
			const QString text = d_deleg->getEditor()->getCursor().getAnchorText();
            d_deleg->getEditor()->getCursor().beginEditBlock();
            d_deleg->getEditor()->getCursor().deleteCharOrSelection();
			const QByteArray link = l.writeTo();
			if( !d_linkRenderer.renderLink( d_deleg->getEditor()->getCursor(), link ) )
				d_deleg->getEditor()->getCursor().insertUrl( d_linkRenderer.renderHref( link ), true, text );
            d_deleg->getEditor()->getCursor().endEditBlock();
            d_deleg->getEditor()->invalidate();
			if( o.getType() == OutlineItem::TID )
                s_itemDefault = l;
            else
                s_objectDefault = l;
        }
    }else
		insertUrl();
}

void OutlineUdbCtrl::onOpenUrl()
{
	// redundant zu OutlineCtrl::onFollowAnchor
	const QUrl url = d_deleg->getSelUrl();
	const QByteArray link = d_deleg->getSelLink();
	ENABLED_IF( url.isValid() || !link.isEmpty() );

	d_deleg->activateAnchor();
}

void OutlineUdbCtrl::onFollowAlias()
{
    QModelIndex i = getTree()->currentIndex();
    Udb::Obj o = getMdl()->getItem( i );
    if( o.isNull() )
        return;
	o = o.getValueAsObj( OutlineItem::AttrAlias );
    ENABLED_IF( !o.isNull() );

    emit sigLinkActivated( o.getOid() );
}

void OutlineUdbCtrl::onExpandAll()
{
	ENABLED_IF( true );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	Udb::Database::TxnGuard guard( d_txn->getDb() );
	const int count = d_mdl->rowCount();
	for( int i = 0; i < count; i++ )
		_expand( getTree(), d_mdl, d_mdl->index( i, 0 ), true );
	QApplication::restoreOverrideCursor();
}

void OutlineUdbCtrl::onCollapseAll()
{
	ENABLED_IF( true );

	QApplication::setOverrideCursor( Qt::WaitCursor );
	Udb::Database::TxnGuard guard( d_txn->getDb() );
	const int count = d_mdl->rowCount();
	for( int i = 0; i < count; i++ )
		_expand( getTree(), d_mdl, d_mdl->index( i, 0 ), false );
	QApplication::restoreOverrideCursor();
}

void OutlineUdbCtrl::onExpandSubs()
{
	ENABLED_IF( getTree()->currentIndex().isValid() );

	QApplication::setOverrideCursor( Qt::WaitCursor );
	Udb::Database::TxnGuard guard( d_txn->getDb() );
	_expand( getTree(), d_mdl, getTree()->currentIndex(), true );
	QApplication::restoreOverrideCursor();
}

void OutlineUdbCtrl::onReadOnly()
{
	CHECKED_IF( getTree()->currentIndex().isValid()&& !d_deleg->isReadOnly() , 
		getTree()->currentIndex().data( OutlineUdbMdl::ReadOnlyRole ).toBool() );

	Udb::Obj o = d_mdl->getItem( getTree()->currentIndex() );
	const bool ro = !o.getValue( OutlineItem::AttrIsReadOnly ).getBool();
	d_deleg->writeData();
	o.setValue( OutlineItem::AttrIsReadOnly, DataCell().setBool( ro ) );
	o.commit();
	d_deleg->readData();
}

void OutlineUdbCtrl::onDocReadOnly()
{
	CHECKED_IF( !d_txn->getDb()->isReadOnly() && !d_mdl->getOutline().isNull(),
		d_deleg->isReadOnly() );

	Udb::Obj o = d_mdl->getOutline();
	const bool ro = !o.getValue( OutlineItem::AttrIsReadOnly ).getBool();
	if( d_deleg->isEditing() )
		d_deleg->writeData();
	o.setValue( OutlineItem::AttrIsReadOnly, DataCell().setBool( ro ) );
	o.commit();
	if( d_deleg->isEditing() )
		d_deleg->readData();
}

void OutlineUdbCtrl::onDbUpdate( Udb::UpdateInfo info )
{
	if( !d_deleg->getEditIndex().isValid() )
		return;
	switch( info.d_kind )
	{
	case Udb::UpdateInfo::ValueChanged:
		if( d_deleg->getEditIndex().data( OutlineUdbMdl::OidRole ).toULongLong() == info.d_id &&
			info.d_name == OutlineItem::AttrIsTitle || info.d_name == OutlineItem::AttrIsReadOnly )
		{
			d_deleg->writeData();
			d_deleg->readData();
		}
		break;
	case Udb::UpdateInfo::Deaggregated:
		if( d_deleg->getEditIndex().data( OutlineUdbMdl::OidRole ).toULongLong() == info.d_id )
			d_deleg->closeEdit();
		break;
    }
}

void OutlineUdbCtrl::followLink(const QByteArray & data, bool isUrl )
{
    if( !isUrl )
    {
        Link link;
        if( link.readFrom( data ) && d_mdl->getOutline().getDb()->getDbUuid() == link.d_db )
            emit sigLinkActivated( link.d_oid );
    }// else wird in OutlineCtrl gehandhabt
}

void OutlineUdbCtrl::followAlias()
{
    QModelIndex i = getTree()->currentIndex();
    Udb::Obj o = getMdl()->getItem( i );
    if( o.isNull() )
        return;
	o = o.getValueAsObj( OutlineItem::AttrAlias );
    if( !o.isNull() )
        emit sigLinkActivated( o.getOid() );
}

bool OutlineUdbCtrl::gotoItem( quint64 oid )
{
	Udb::Obj o = d_txn->getObject( oid );
    if( o.isNull() )
        return false;
	if( o.getValue( OutlineItem::AttrHome ).getOid() == d_mdl->getOutline().getOid() )
	{
		QModelIndex idx = d_mdl->getIndex( oid, true );
		QModelIndex up = idx.parent();
		while( up.isValid() )
		{
			getTree()->expand( up );
			up = up.parent();
		}
		if( idx.isValid() )
		{
			getTree()->goAndEdit( idx );
		}
		return true;
	}else
		getTree()->selectionModel()->clear();
    return false;
}

bool OutlineUdbCtrl::LinkRenderer::renderLink(TextCursor & cur, const QByteArray &data) const
{
    Q_ASSERT( d_txn != 0 );
    Link link;
    if( !link.readFrom( data ) )
        return false;
    if( d_txn->getDb()->getDbUuid() != link.d_db )
    {
		// cur.insertLink( data, tr("<external reference>") );
		return false; // Neu, damit Caller den bestehenden Text verwenden kann
		// Externe Links werden eh als xoid-URL eingebettet, nicht als Link
    }
	OutlineItem obj = d_txn->getObject( link.d_oid );
	if( obj.isNull(true,true) )
    {
        cur.insertLink( data, tr("<null reference>") );
        return true;
    }
	const Udb::Atom type = obj.getType();
	if( type == OutlineItem::TID )
	{
		const Udb::Obj alias = obj.getAlias();
		if( !alias.isNull() ) // Löse Aliasse auf
			obj = alias;
	}
	QString nr;
	if( link.d_paraNumber && type == OutlineItem::TID )
		nr = OutlineItem::getParagraphNumber( obj );

	QString itemName;
	if( link.d_showContext )
    {
		if( type == OutlineItem::TID )
		{
			const OutlineItem item = obj;
			obj = obj.getHome();
			if( obj.isNull() )
			{
				obj = item;
				link.d_showContext = false;
			}else if( link.d_showSubName )
				itemName = item.getText();
		}else
		{
			const Udb::Obj parent = obj.getParent();
			if( parent.isNull() )
				link.d_showContext = false;
			else
			{
				if( link.d_showSubName )
					itemName = obj.getText();
				if( link.d_paraNumber )
				{
					nr = obj.getAltIdent();
					if( nr.isEmpty() )
						nr = obj.getIdent();
				}
				obj = parent;
			}
		}
	}

    QString icon;
    if( link.d_showIcon )
		icon = OutlineUdbMdl::getPixmapPath( obj.getType() );

	QString id;
	if( link.d_showId )
    {
		id = obj.getAltIdent();
		if( id.isEmpty() )
			id = obj.getIdent();
	}

	QString name;
	if( link.d_showName )
		name = obj.getText();

	// Zur Verfügung: id, name, itemName, nr, icon
	QString text;
	if( false ) // name.isEmpty() && itemName.isEmpty() && id.isEmpty() )
		id = nr;
	else if( link.d_showContext )
	{
		text = name;
		if( !nr.isEmpty() || !itemName.isEmpty() )
		{
			if( !text.isEmpty() && ( !nr.isEmpty() || !itemName.isEmpty() ) )
				text += QLatin1String(": ");
			text += nr;
			if( !nr.isEmpty() && !itemName.isEmpty() )
				text += QChar(' ');
			text += itemName;
		}
	}else
	{
		if( false ) // id.isEmpty() )
		{
			id = nr;
			text = name;
		}else
		{
			text = nr;
			if( !text.isEmpty() && !name.isEmpty() )
				text += QChar(' ');
			text += name;
		}
	}

    if( link.d_elide > 0 )
    {
		if( text.size() > link.d_elide )
        {
			text.truncate( link.d_elide );
			text += QString("...");
        }
    }
	cur.insertLink( data, text, icon, id );
	return true;
}

QString OutlineUdbCtrl::LinkRenderer::renderHref(const QByteArray &link) const
{
	Link l;
	if( !l.readFrom( link ) )
		return Txt::LinkRendererInterface::renderHref( link );
	return Udb::Obj::oidToUrl(l.d_oid, l.d_db ).toEncoded();
}


