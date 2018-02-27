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

#include "OutlineTreeCtrl.h"
#include "OutlineTreeModel.h"
#include "OutlineTreeDeleg.h"
#include <Gui/Menu.h>
#include <Gui/ShortcutAction.h>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
using namespace Oln;

const char* OutlineTreeCtrl::LocalAddRight = "outline.add.local.right";
const char* OutlineTreeCtrl::LocalAddNext = "outline.add.local.next";
const char* OutlineTreeCtrl::LocalAddLeft = "outline.add.local.left";
const char* OutlineTreeCtrl::Dump = "outline.dump";
const char* OutlineTreeCtrl::Indent = "outline.indent";
const char* OutlineTreeCtrl::Outdent = "outline.outdent";
const char* OutlineTreeCtrl::MoveUp = "outline.move.up";
const char* OutlineTreeCtrl::MoveDown = "outline.move.down";
const char* OutlineTreeCtrl::TitleItem = "outline.title";

ACTION_SLOTS_BEGIN( OutlineTreeCtrl )
	{ OutlineTreeCtrl::TitleItem, &OutlineTreeCtrl::handleTitleItem }, 
	{ Gui::TreeCtrl::Expand, &OutlineTreeCtrl::handleExpand }, 
	{ Gui::TreeCtrl::ExpandAll, &OutlineTreeCtrl::handleExpandAll }, 
	{ Gui::TreeCtrl::ExpandSubs, &OutlineTreeCtrl::handleExpandSubs }, 
	{ Gui::TreeCtrl::CollapseAll, &OutlineTreeCtrl::handleCollapseAll }, 
	{ OutlineTreeCtrl::LocalAddLeft, &OutlineTreeCtrl::handleLocalAddLeft }, 
	{ OutlineTreeCtrl::MoveUp, &OutlineTreeCtrl::handleMoveUp }, 
	{ OutlineTreeCtrl::MoveDown, &OutlineTreeCtrl::handleMoveDown }, 
	{ OutlineTreeCtrl::LocalAddNext, &OutlineTreeCtrl::handleRecCreate }, 
	{ OutlineTreeCtrl::LocalAddRight, &OutlineTreeCtrl::handleRecCreateSub }, 
	{ OutlineTreeCtrl::Outdent, &OutlineTreeCtrl::handleOutdent }, 
	{ OutlineTreeCtrl::Indent, &OutlineTreeCtrl::handleIndent }, 
	{ OutlineTreeCtrl::Dump, &OutlineTreeCtrl::handleDump }, 
	{ Action::RecordDelete, &OutlineTreeCtrl::handleRecDelete }, 
	{ Action::RecordCreate, &OutlineTreeCtrl::handleRecCreate }, 
	{ Action::RecordCreateSub, &OutlineTreeCtrl::handleRecCreateSub }, 
	{ Action::EditUndo, &OutlineTreeCtrl::handleUndo }, 
	{ Action::EditRedo, &OutlineTreeCtrl::handleRedo }, 
	{ Action::EditCut, &OutlineTreeCtrl::handleCut }, 
	{ Action::EditCopy, &OutlineTreeCtrl::handleCopy }, 
	{ Action::EditPaste, &OutlineTreeCtrl::handlePaste }, 
ACTION_SLOTS_END( OutlineTreeCtrl );

OutlineTreeCtrl::OutlineTreeCtrl( QTreeView* w, OutlineTreeDeleg* d, bool b ):
	EventListener( w, b ), d_deleg( d ), d_expandLock( false ), d_mdl(0), d_dirty(false)
{ // OutlineTreeCtrl( w, b )
	assert( w );
	observe( w->viewport() ); // beobachte zusätzlich den Viewport für Mouse-Events
	d_mdl = 0;

    Gui::Menu* rec = new Gui::Menu( wnd(), true );
	rec->addCommand( tr("Add Item"), Root::Action::RecordCreate, false, tr("CTRL+N") );
	rec->addCommand( tr("Add Item Right"), Root::Action::RecordCreateSub, false, tr("CTRL+R") );
	rec->addCommand( tr("Add Item Left"), OutlineTreeCtrl::LocalAddLeft, false, tr("CTRL+L") );
	rec->addCommand( tr("Title Item"), TitleItem, true );
	rec->addCommand( tr("Delete"), Root::Action::RecordDelete, false );
	rec->addSeparator();
	rec->addCommand( tr("Indent"), Indent );
	rec->addCommand( tr("Outdent"), Outdent );
	rec->addSeparator();
	rec->addCommand( tr("Dump"), Dump, false );
	rec->addCommand( tr("Expand"), Gui::TreeCtrl::Expand, false );
	rec->addCommand( tr("ExpandAll"), Gui::TreeCtrl::ExpandAll, false );
	rec->addCommand( tr("ExpandSubs"), Gui::TreeCtrl::ExpandSubs, false );
	rec->addCommand( tr("CollapseAll"), Gui::TreeCtrl::CollapseAll, false );

	( connect( w, SIGNAL( collapsed( const QModelIndex& ) ),
		this, SLOT( collapsed( const QModelIndex& ) ) ) );
	( connect( w, SIGNAL( expanded( const QModelIndex& ) ),
		this, SLOT( expanded( const QModelIndex& ) ) ) );

	createEmpty();
}

bool OutlineTreeCtrl::loadFrom( Root::ValueReader& vr )
{
	if( d_mdl )
	{
		wnd()->setModel( 0 );
		delete d_mdl;
	}
	d_mdl = new OutlineTreeModel( this );
	if( !d_mdl->loadFrom( vr ) )
		return false;
	wnd()->setModel( d_mdl );
	checkExpanded( QModelIndex() );
	connectModel();
	d_dirty = false;
	return true;
}

void OutlineTreeCtrl::createEmpty()
{
	if( d_mdl )
	{
		wnd()->setModel( 0 );
		delete d_mdl;
	}
	d_mdl = new OutlineTreeModel( this );
	wnd()->setModel( d_mdl );
	connectModel();
	QModelIndex n = d_mdl->createChild( d_mdl->parent( QModelIndex() ), 0, true );
	//TODO wnd()->edit( n );  
	// Wenn hier gesetzt ist Editor zu schmal. Anscheinend gibt visualRect das alte Rect.
	// Mit QueuedSignal zu editTop funktioniert's.
	d_dirty = false;
}

void OutlineTreeCtrl::connectModel()
{
	connect( d_mdl, SIGNAL( rowsInserted( const QModelIndex &, int, int ) ),
		this, SLOT( inserted( const QModelIndex &, int, int ) ) );
	// dataChanged eignet sich nicht, da es auch wegen Grössenänderung getriggert wird.
	connect( d_mdl, SIGNAL( onDirty() ), this, SLOT(onChange() ) );
	connect( d_mdl, SIGNAL( rowsRemoved(const QModelIndex &, int, int) ), this, SLOT(onChange() ) );
	// Qt::QueuedConnection funktioniert nicht wegen QModelIndex, das als Variant nicht bekannt ist.
}

void OutlineTreeCtrl::editTop()
{
	QModelIndex i = d_mdl->index( 0, 0 );
	wnd()->edit( i );
}

void OutlineTreeCtrl::closeEdit()
{
	d_deleg->closeEdit();
}

void OutlineTreeCtrl::saveEdit()
{
	d_deleg->writeData();
}

bool OutlineTreeCtrl::saveTo( Root::ValueWriter& vw )
{
	assert( d_mdl );
	if( !d_mdl->saveTo( vw ) )
		return false;
	d_dirty = false;
	return true;
}

bool OutlineTreeCtrl::saveTo( Stream::DataWriter& vw )
{
	assert( d_mdl );
	if( !d_mdl->saveTo( vw ) )
		return false;
	return true;
}

void OutlineTreeCtrl::handleRecCreate( Root::Action& t)
{
	ENABLED_IF( t, true );

	QModelIndex i = wnd()->currentIndex();
	QModelIndex p = d_mdl->parent( i );
	QModelIndex n = d_mdl->createChild( p, i.row() + 1, true );
	// wnd()->setCurrentIndex( n ); passiert in signalhandler
	wnd()->edit( n );  
}

void OutlineTreeCtrl::handleRecCreateSub( Root::Action& t)
{
	ENABLED_IF( t, true );

	QModelIndex i = wnd()->currentIndex();
	wnd()->setExpanded( i, true );
	QModelIndex n = d_mdl->createChild( i, 0, true );
	// wnd()->setCurrentIndex( n ); passiert in signalhandler
	wnd()->edit( n ); 
}

void OutlineTreeCtrl::handleLocalAddLeft( Root::Action& t)
{
	QModelIndex i = wnd()->currentIndex();
	QModelIndex p = d_mdl->parent( i );
	ENABLED_IF( t, p.isValid() );

	QModelIndex pp = d_mdl->parent( p );
	wnd()->setExpanded( pp, true );
	QModelIndex n = d_mdl->createChild( pp, p.row() + 1, true );
	// wnd()->setCurrentIndex( n ); passiert in signalhandler
	wnd()->edit( n ); 
}

void OutlineTreeCtrl::handleRecDelete( Root::Action& t)
{
	ENABLED_IF( t, wnd()->currentIndex().isValid() );
	QModelIndex i = wnd()->currentIndex();
	QModelIndex p = d_mdl->parent( i );
	d_mdl->removeChild( p, i.row() );
	i = d_mdl->index( i.row(), 0, p );
	if( !i.isValid() )
	{
		i = d_mdl->index( i.row() - 1, 0, p );
		if( !i.isValid() )
			i = p;
	}
	wnd()->setCurrentIndex( i );
}

void OutlineTreeCtrl::handleDump( Root::Action& t)
{
	ENABLED_IF( t, d_mdl->getRoot() );
	QModelIndex i = wnd()->currentIndex();
	OutlineItem* e = d_mdl->getItem( i );
	// qDebug( "*** Selected %d/%d %x", i.row(), i.column(), e );
	d_mdl->getRoot()->dump();
}

void OutlineTreeCtrl::handleIndent( Root::Action& t)
{
	QModelIndex i = wnd()->currentIndex();
	OutlineItem* e = d_mdl->getItem( i );
	ENABLED_IF( t, e && e->getOwner() && e->getOwner()->getFirstChild() != e ); // TODO: hier canMove berücksichtigen
	QModelIndex p = d_mdl->sibling( i.row() - 1, i.column(), i );
	const int toRow = d_mdl->rowCount( p );
	d_deleg->closeEdit();
	d_mdl->move( i, p, toRow );
	wnd()->setExpanded( p, true );
	i = d_mdl->index( toRow, 0, p ); // p.column() kann -1 werden
	wnd()->setCurrentIndex( i );
	wnd()->edit( i );
	// TODO: dasselbe mit Multiselection
}

void OutlineTreeCtrl::handleOutdent( Root::Action& t)
{
	QModelIndex i = wnd()->currentIndex();
	OutlineItem* e = d_mdl->getItem( i );
	ENABLED_IF( t, e && e->getLevel() > 1 ); // TODO: hier canMove berücksichtigen

	// TODO: dasselbe mit Multiselection
	QModelIndex p = d_mdl->parent( i );
	QModelIndex pp = d_mdl->parent( p );
	d_deleg->closeEdit();
	d_mdl->move( i, pp, p.row() + 1 );
	i = d_mdl->index( p.row() + 1 , 0, pp );
	wnd()->setCurrentIndex( i );
	wnd()->edit( i );
}

void OutlineTreeCtrl::handleMoveUp(Root::Action & t)
{
	QModelIndex i = wnd()->currentIndex();
	QModelIndex p = d_mdl->parent( i );
	ENABLED_IF( t, i.row() > 0 || i != d_mdl->index( 0, 0 ) ); 
	// TODO: hier canMove berücksichtigen

	if( i.row() > 0 )
	{
		if( d_mdl->move( i, p, i.row() - 1 ) )
			wnd()->setCurrentIndex( d_mdl->index( i.row() - 1 , 0, p ) );
	}else
	{
		const int to = p.row(); // wenn !p.isValid wird to == -1
		p = d_mdl->parent( p );
		if( to >= 0 && d_mdl->move( i, p, to ) )
			wnd()->setCurrentIndex( d_mdl->index( to , 0, p ) );
	}
}

void OutlineTreeCtrl::handleMoveDown(Root::Action & t)
{
	QModelIndex i = wnd()->currentIndex();
	QModelIndex p = d_mdl->parent( i );
	ENABLED_IF( t, i != d_mdl->index( -1, 0 ) ); // TODO: hier canMove berücksichtigen

	if( i.row() < d_mdl->rowCount( p ) - 1 )
	{
		if( d_mdl->move( i, p, i.row() + 1 ) )
			wnd()->setCurrentIndex( d_mdl->index( i.row() + 1 , 0, p ) );
	}else
	{
		QModelIndex pp;
		while( p.isValid() )
		{
			pp = d_mdl->parent(p);
			if( p.row() < d_mdl->rowCount( pp ) - 1 )
			{
				p = d_mdl->index( p.row() + 1, 0, pp );
				if( d_mdl->move( i, p, 0 ) )
				{
					i = d_mdl->index( 0 , 0, p );
					wnd()->setCurrentIndex( i );
					wnd()->scrollTo( i );
				}
				return;
			}else
			{
				p = pp;
			}
		}
	}
}

bool OutlineTreeCtrl::keyPressEvent(QWidget* w,QKeyEvent* e)
{
    switch( e->key() ) 
	{
    case Qt::Key_Up:
		if( e->modifiers() == Qt::ControlModifier )
		{
			Gui::ShortcutAction a( MoveUp, wnd() );
			if( a.checkAndRun() )
				return true;
		}
		if( d_deleg && d_deleg->getCtrl()->eventFilter( w, e ) )
			return true;
		break;
    case Qt::Key_Down:
		if( e->modifiers() == Qt::ControlModifier )
		{
			Gui::ShortcutAction a( MoveDown, wnd() );
			if( a.checkAndRun() )
				return true;
		}
		if( d_deleg && d_deleg->getCtrl()->eventFilter( w, e ) )
			return true;
		break;
    case Qt::Key_Left:
		if( e->modifiers() == Qt::ControlModifier )
		{
			Gui::ShortcutAction a( Outdent, wnd() );
			if( a.checkAndRun() )
				return true;
		}
		if( d_deleg && d_deleg->getCtrl()->eventFilter( w, e ) )
			return true;
		break;
    case Qt::Key_Right:
		if( e->modifiers() == Qt::ControlModifier )
		{
			Gui::ShortcutAction a( Indent, wnd() );
			if( a.checkAndRun() )
				return true;
		}
		if( d_deleg && d_deleg->getCtrl()->eventFilter( w, e ) )
			return true;
		break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
		{
			Gui::ShortcutAction a( Root::Action::RecordCreate, wnd() );
			return a.checkAndRun();
		}
    case Qt::Key_Tab: 
		{
			Gui::ShortcutAction a( Indent, wnd() );
			return a.checkAndRun();
		}
	case Qt::Key_Backtab:
		{
			Gui::ShortcutAction a( Outdent, wnd() );
			return a.checkAndRun();
		}
	}
	return false;
}

void OutlineTreeCtrl::collapsed( const QModelIndex & index )
{
	if( !d_expandLock )
		d_mdl->setData( index, false, OutlineTreeModel::ExpandedRole );
}

void OutlineTreeCtrl::expanded( const QModelIndex & index )
{
	if( !d_expandLock )
		d_mdl->setData( index, true, OutlineTreeModel::ExpandedRole );
	// doItemsLayout() nützt hier nichts, es wird trotzdem nicht sauber gezeichnet
}

void OutlineTreeCtrl::inserted( const QModelIndex & p, int start, int end )
{
	( disconnect( wnd(), SIGNAL( collapsed( const QModelIndex& ) ),
		this, SLOT( collapsed( const QModelIndex& ) ) ) );
	( disconnect( wnd(), SIGNAL( expanded( const QModelIndex& ) ),
		this, SLOT( expanded( const QModelIndex& ) ) ) );
	checkExpanded( p );
	wnd()->setCurrentIndex( d_mdl->index( start, 0, p ) ); // RISK: funktioniert eigentlich

	( connect( wnd(), SIGNAL( collapsed( const QModelIndex& ) ),
		this, SLOT( collapsed( const QModelIndex& ) ) ) );
	( connect( wnd(), SIGNAL( expanded( const QModelIndex& ) ),
		this, SLOT( expanded( const QModelIndex& ) ) ) );
	onChange();
}

void OutlineTreeCtrl::handleTitleItem(Root::Action & t)
{
	QModelIndex i = wnd()->currentIndex();
	QVariant v = d_mdl->data( i, OutlineTreeModel::TitleRole );
	ENABLED_CHECKED_IF( t, i.isValid(), v.toBool() );
	d_deleg->writeData(); // RISK: es muss ja ein anderes Feld gelesen werden
	d_mdl->setData( i, !v.toBool(), OutlineTreeModel::TitleRole );
	d_deleg->readData();
}

void OutlineTreeCtrl::checkExpanded( QModelIndex index )
{
	bool expanded = d_mdl->data( index, OutlineTreeModel::ExpandedRole ).toBool();
	wnd()->setExpanded( index, expanded );
	const int rows = d_mdl->rowCount( index );
	for( int i = 0; i < rows; i++ )
		checkExpanded( d_mdl->index( i, 0, index ) );
}

void OutlineTreeCtrl::handleUndo(Root::Action & t)
{
	if( d_deleg->isEditing() )
	{
		d_deleg->getCtrl()->dispatch( t );
		return;
	}
}

void OutlineTreeCtrl::handleRedo(Root::Action & t)
{
	if( d_deleg->isEditing() )
	{
		d_deleg->getCtrl()->dispatch( t );
		return;
	}
}

void OutlineTreeCtrl::handleCut(Root::Action & t)
{
	if( d_deleg->isEditing() )
	{
		d_deleg->getCtrl()->dispatch( t );
		return;
	}
	QModelIndex i = wnd()->currentIndex();
	ENABLED_IF( t, i.isValid() );
	QModelIndexList l;
	l << i;
	QApplication::clipboard()->setMimeData( d_mdl->createMimeData( l, false ) );
	d_mdl->removeChild( d_mdl->parent( i ), i.row() );
}

void OutlineTreeCtrl::handleCopy(Root::Action & t)
{
	if( d_deleg->isEditing() )
	{
		d_deleg->getCtrl()->dispatch( t );
		return;
	}
	QModelIndex i = wnd()->currentIndex();
	ENABLED_IF( t, i.isValid() );
	QModelIndexList l;
	l << i;
	QApplication::clipboard()->setMimeData( d_mdl->createMimeData( l, false ) );
}

void OutlineTreeCtrl::handlePaste(Root::Action & t)
{
	if( d_deleg->isEditing() )
	{
		d_deleg->getCtrl()->dispatch( t );
		return;
	}
    const QMimeData *md = QApplication::clipboard()->mimeData();
	// qDebug() << md->formats(); // TEST
	ENABLED_IF( t, md && 
		( md->hasText() || md->hasFormat( "application/outlineitems" ) ) );
	d_mdl->dropMimeData( md, Qt::CopyAction, -1, -1, wnd()->currentIndex() );
}

void OutlineTreeCtrl::handleExpand(Root::Action & t)
{
	QTreeView* tv = wnd();
	QModelIndex i = tv->currentIndex();
	ENABLED_CHECKED_IF( t, i.isValid(), tv->isExpanded( i ) );

	tv->setExpanded( i, !tv->isExpanded( i ) );
}

static void expand( QTreeView* tv, const QModelIndex& index, bool f )
{
	const int count = tv->model()->rowCount( index );
	if( count == 0 )
		return;
	tv->setExpanded( index, f );
	for( int i = 0; i < count; i++ )
		expand( tv, tv->model()->index( i, 0, index ), f );
}

void OutlineTreeCtrl::handleExpandAll(Root::Action & t)
{
	QTreeView* tv = wnd();
	ENABLED_IF( t, true );

	QApplication::setOverrideCursor( Qt::WaitCursor );
	const int count = tv->model()->rowCount();
	for( int i = 0; i < count; i++ )
		expand( tv, tv->model()->index( i, 0 ), true );
	QApplication::restoreOverrideCursor();
}

void OutlineTreeCtrl::handleExpandSubs(Root::Action & t)
{
	QTreeView* tv = wnd();
	QModelIndex i = tv->currentIndex();
	ENABLED_IF( t, i.isValid() );

	QApplication::setOverrideCursor( Qt::WaitCursor );
	expand( tv, i, true );
	QApplication::restoreOverrideCursor();
}

void OutlineTreeCtrl::handleCollapseAll(Root::Action & t)
{
	QTreeView* tv = wnd();
	ENABLED_IF( t, true );

	const int count = tv->model()->rowCount();
	for( int i = 0; i < count; i++ )
		expand( tv, tv->model()->index( i, 0 ), false );
}

void OutlineTreeCtrl::onChange()
{
	d_dirty = true;
	emit modelChanged();
}

Root::AtomManager* OutlineTreeCtrl::getAtoms() const
{
	return d_mdl->getAtoms();
}
