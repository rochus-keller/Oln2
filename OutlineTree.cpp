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

#include "OutlineTree.h"
#include <QStack>
#include "private/qtreeview_p.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QtDebug>
#include <QItemSelectionModel>
#include <cassert>
#include "OutlineMdl.h"
#include "OutlineDeleg.h"
using namespace Oln;

// Diese Datei ist eine Adaption von DoorScope DocTree
// Ganz frei von Udb. Weiss nichts über die Art der Speicherung des Outlines.

class OutlineTreeSelectionModel : public QItemSelectionModel
{
public:
	OutlineTreeSelectionModel( QAbstractItemModel * model ):QItemSelectionModel ( model ) {}
	// select(const QModelIndex &index ruft intern auch dieses select auf, und QTreeView ruft beide varianten auf.
	static int findTop( const QItemSelection & sel )
	{
		int index = -1;
		quint32 level = 0xffffffff;
		for( int i = 0; i < sel.size(); i++ )
		{
			const quint32 l = sel[i].parent().data( OutlineMdl::LevelRole ).toInt();
			if( l < level )
			{
				level = l;
				index = i;
			}
		}
		return index;
	}
	void select ( const QItemSelection & newSel, QItemSelectionModel::SelectionFlags command )
	{
		//qDebug() << QString("%1").arg( command, 0, 2 );
		if( hasSelection() && ( command & Select || command & Toggle ) && !( command & Clear ) )
		{
			const QItemSelection curSel = selection();
			if( curSel.isEmpty() )
				return; // Das kommt tatsächlich vor und wird auch von Qt nicht erwartet
			const QModelIndex parent = curSel.first().topLeft().parent();
			QItemSelection sel = newSel;
			int i = 0;
			//qDebug( "newSel count=%d", newSel.size() );
			while( i < sel.size() )
			{
				if( sel[i].parent() != parent )
				{
					sel.removeAt( i );
				}else
					i++;
			}
			if( !sel.isEmpty() )
				QItemSelectionModel::select( sel, command );
		}else if( ( command & Select ) && ( command & Clear ) )
		{
			const int row = findTop( newSel );
			if( row == -1 )
				return;
			const QModelIndex parent = newSel[row].topLeft().parent();
			QItemSelection sel = newSel;
			int i = 0;
			while( i < sel.size() )
			{
				if( sel[i].parent() != parent )
				{
					sel.removeAt( i );
				}else
					i++;
			}
			if( !sel.isEmpty() )
				QItemSelectionModel::select( sel, command );
		}else
			QItemSelectionModel::select( newSel, command );
	}
};

OutlineTree::OutlineTree( QWidget* parent ):
	QTreeView( parent), d_block( false ), d_stepSize(1), d_block2(false), d_showNumbers( true ),
    d_handleWidth( 11 ), d_dragEnabled( true ), d_clickInEditor( false ), d_dragging(false)
{
    setAttribute(Qt::WA_KeyCompression); // hat keinen erkennbaren Effekt, weder auf Linux noch Windows
    // setAttribute(Qt::WA_InputMethodEnabled); // wird bereits in QAbstractItemView gesetzt

    QPalette pal = palette();
	pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 245, 245, 245 ) );
	setPalette( pal );

	setDropIndicatorShown( true );
	setAcceptDrops( true );
	setDragDropMode( QAbstractItemView::DropOnly );

	setEditTriggers( SelectedClicked ); // AllEditTriggers
	setUniformRowHeights( false );
	setAllColumnsShowFocus(false); // Wenn das true ist, wird Focus-Rect um die Zeile gemalt
	setIndentation( 25 );
	setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
	setHorizontalScrollMode( QAbstractItemView::ScrollPerPixel );
	setWordWrap( true );
	setSelectionBehavior( QAbstractItemView::SelectRows );
	setSelectionMode( QAbstractItemView::ContiguousSelection ); // Das ist der beste mode. Multi funktioniert
                                                                // nicht und extended lässt unzusammenhängende zu
	header()->hide();

	connect( this, SIGNAL( expanded ( const QModelIndex & ) ), this, SLOT( onExpanded ( const QModelIndex & ) ) );
	connect( this, SIGNAL( collapsed ( const QModelIndex & ) ), this, SLOT( onCollapsed ( const QModelIndex & ) ) );
}

OutlineTree::~OutlineTree()
{
	closeEdit();
}

void OutlineTree::drawBranches( QPainter * painter, 
							   const QRect & rect, const QModelIndex & index ) const
{
    Q_D(const QTreeView);
    // const int indent = indentation();
	const bool expanded = isExpanded( index );
	const bool hasSubs = model()->hasChildren( index );
	const bool selected = selectionModel()->isSelected( index );

	// TODO if( selected )
		// painter->fillRect( rect.adjusted( 0, 0, -indent / 2, 0 ), palette().brush( QPalette::Highlight ) );

	if( alternatingRowColors() )
	{
		painter->fillRect( rect, palette().brush( 
			(d->current & 1)?QPalette::AlternateBase:QPalette::Base ) );
		if( selected )
			painter->fillRect( rect.adjusted( 0, 0, -d_handleWidth, -1 ), palette().brush( QPalette::Highlight ) );
	}else
	{
		painter->fillRect( rect, palette().brush( QPalette::Base ) );
		painter->setPen( Qt::lightGray );
		painter->drawLine( rect.left(), rect.bottom(), rect.right(), rect.bottom() );
		const QRect r = rect.adjusted( 0, 0, -d_handleWidth, -1 );
		if( selected )
		{
			painter->fillRect( r,
				//(hasSubs)?r:rect, 
				palette().brush( QPalette::Highlight ) );
			painter->setPen( palette().color( QPalette::HighlightedText ) );
		}else
		{
			painter->setPen( Qt::black );
		}
		if( d_showNumbers )
			painter->drawText( r.adjusted( 1, 0, 0, 0 ), Qt::TextSingleLine | Qt::AlignTop | Qt::AlignLeft, 
				index.data( OutlineMdl::NumberRole ).toString() );
	}
	if( hasSubs )
	{
		QStyleOption opt;
		opt.initFrom(this);
		QStyle::State extraFlags = QStyle::State_None;
		if (isEnabled())
			extraFlags |= QStyle::State_Enabled;
		if (window()->isActiveWindow())
			extraFlags |= QStyle::State_Active;
		// +1: ansonsten gerät das Dreieck in den Selektionsrahmen
		const int off = d_handleWidth / 5.0 + 0.5; // 2 bei hw=11
        opt.rect = QRect( rect.right() - off - ( d_handleWidth / 2.0 + 0.5 ), 
			rect.top() + off, d_handleWidth - off, d_handleWidth - off );
        opt.state = QStyle::State_Item | extraFlags;
		// bei w=9 ist die von IndicatorArrow schwarz gefärbte Area w=7
		if( expanded )
			style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &opt, painter, this);
		else
			style()->drawPrimitive(QStyle::PE_IndicatorArrowRight, &opt, painter, this);
	}
}

void OutlineTree::drawRow( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const 
{
    QStyleOptionViewItem opt = option;
    // Focus-Flag deaktivieren, da sonst bei TAB ein FokusRect gemalt wird. Wirkung unklar.
    //opt.state &= ~QStyle::State_HasFocus;
	//opt.state &= ~QStyle::State_KeyboardFocusChange;
    // Dieser Aufruf ist nötig, damit überhaupt etwas gemalt wird. Die Oberklasse malt jedoch beharrlich
    // ein PE_FrameFocusRect um die Zeile, sobald currentRowHasFocus, egal welche Optionen noch gesetzt sind.
    // State_HasFocus oder State_KeyboardFocusChange haben keinen Einfluss, auch sonst keines der Flags
	QTreeView::drawRow( painter, opt, index );

	if( alternatingRowColors() && index == currentIndex() )
	{
        // Im "normalen" Outline-Modus kommen wir hier nicht an.
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;

		painter->setPen( option.palette.color( cg, QPalette::Highlight) );
		//painter->drawRect( option.rect.adjusted( 0, 0, -1, -1 ) );
		//painter->drawLine( option.rect.left(), option.rect.bottom(), option.rect.right(), option.rect.bottom() );
		//painter->drawLine( option.rect.right(), option.rect.top(), option.rect.right(), option.rect.bottom() );

		painter->drawRect( option.rect.adjusted( 0, 0, -1, -1 ) );
	}
}

bool OutlineTree::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event)
{
    Q_UNUSED(trigger);

    Q_D(QTreeView);

    if (!d->isIndexValid(index))
        return false;

 #if QT_VERSION >= 0x040600
    if (QWidget *w = (d->persistent.isEmpty() ? 0 : d->editorForIndex(index).editor)) {
#else
    if (QWidget *w = (d->persistent.isEmpty() ? 0 : d->editorForIndex(index))) {
#endif
        w->setFocus();
        return true;
    }
	
	if (d->sendDelegateEvent(index, event)) {
        d->viewport->update(visualRect(index));
        return true;
    }

	return false;
}

bool OutlineTree::startEdit(const QModelIndex &index, QEvent *event)
{
	// Analog zu AbstractItemView, ausser dass wir hier ohne
	// Delay arbeiten und im Falle von SelectedClicked sofort editieren.
	// Zudem wird in jedem Fall der Event an den Editor weitergeleitet.

    Q_D(QTreeView);

    if (!d->isIndexValid(index))
        return false;
    if (d->state == QAbstractItemView::EditingState)
        return false;
    if (d->hasEditor(index))
        return false;

    Qt::ItemFlags flags = d->model->flags(index);
    if (((flags & Qt::ItemIsEditable) == 0) || ((flags & Qt::ItemIsEnabled) == 0))
        return false;

    d->openEditor(index, event);
	d->state = QAbstractItemView::EditingState; // RISK: ansonsten nicht gesetzt
    return true;
}

void OutlineTree::goAndEdit( const QModelIndex& index )
{
	if( index.isValid() )
	{
		selectionModel()->setCurrentIndex( index, QItemSelectionModel::Clear | QItemSelectionModel::Select );
		scrollTo( index, QAbstractItemView::EnsureVisible );
		startEdit( index, 0 );
	}else
		closeEdit();
}


void OutlineTree::keyPressEvent(QKeyEvent *event)
{
    //qDebug() << "OutlineTree::keyPressEvent" << event->key() << "'"<< event->text() << "'";
    Q_D(QTreeView);
	bool directSend = false;
    switch (event->key()) 
	{
    case Qt::Key_Down:
    case Qt::Key_Up:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
	case Qt::Key_Return:
	case Qt::Key_Enter:
	case Qt::Key_Plus: // da sonst als Shortcut zum öffnen/schliessen des Items interpretiert
	case Qt::Key_Minus:
		directSend = true;
        break;
    }
	if( event == QKeySequence::Copy && dynamic_cast<OutlineDeleg*>( itemDelegate() ) )
		directSend = true; // Copy soll durch Delegate durchgeführt werden

	if( directSend )
	{
		if( !d->sendDelegateEvent( currentIndex(), event) ) 
		{
			QTreeView::keyPressEvent( event );
			startEdit( currentIndex(), event );
			if( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter )
				emit returnPressed();
		}
	}else
		QTreeView::keyPressEvent( event );
}
// Requires Qt >4.4: QT_VERSION >= 0x040400
int OutlineTree::indexRowSizeHint(const QModelIndex &index) const
{
    Q_D(const QTreeView);
    if (!d->isIndexValid(index) || !d->itemDelegate)
        return 0;

    int start = -1;
    int end = -1;
    int count = d->header->count();
    bool emptyHeader = (count == 0);
    QModelIndex parent = index.parent();

    if (count && isVisible()) {
        // If the sections have moved, we end up checking too many or too few
        start = d->header->visualIndexAt(0);
    } else {
        // If the header has not been laid out yet, we use the model directly
        count = d->model->columnCount(parent);
    }

    if (isRightToLeft()) {
        start = (start == -1 ? count - 1 : start);
        end = 0;
    } else {
        start = (start == -1 ? 0 : start);
        end = count - 1;
    }

    if (end < start)
        qSwap(end, start);

    int height = -1;
    QStyleOptionViewItemV4 option = d->viewOptionsV4();
    // ### If we want word wrapping in the items,
    // ### we need to go through all the columns
    // ### and set the width of the column

    // Hack to speed up the function
    option.rect.setWidth(-1);

    for (int column = start; column <= end; ++column) {
        int logicalColumn = emptyHeader ? column : d->header->logicalIndex(column);
        if (d->header->isSectionHidden(logicalColumn))
            continue;
        QModelIndex idx = d->model->index(index.row(), logicalColumn, parent);
        if (idx.isValid()) {
			//* Start ROCHUS
			int width = header()->sectionSize(column);
			if( !d_block && column == 0 )
			{
				// Block braucht es, da d->viewIndex intern seinerseits indexRowSizeHint aufruft
				//d_block = true;
				width -= d->indentationForItem( viewIndex(idx) );
				//d_block = false;
			}
			option.rect.setWidth( width );
			//* End ROCHUS
#if QT_VERSION >= 0x040600
            QWidget *editor = d->editorForIndex(idx).editor;
#else
            QWidget *editor = d->editorForIndex(idx);
#endif
            if (editor && d->persistent.contains(editor)) {
                height = qMax(height, editor->sizeHint().height());
                int min = editor->minimumSize().height();
                int max = editor->maximumSize().height();
                height = qBound(min, height, max);
            }
            int hint = d->delegateForIndex(idx)->sizeHint(option, idx).height();
            height = qMax(height, hint);
        }
    }

    return height;
}

int OutlineTree::viewIndex(const QModelIndex &index) const
{
	// TEST: analog zu QTreeViewPrivate::viewIndex, jedoch ohne firstVisibleItem
	// Ergebnis: noch immer falsch gelayoutete Teile. Etwas langsamer als oben.
    Q_D(const QTreeView);
    if (!index.isValid() || d->viewItems.isEmpty())
        return -1;

    const int totalCount = d->viewItems.count();
    const QModelIndex parent = index.parent();

    // A quick check near the last item to see if we are just incrementing
    const int start = lastViewedItem > 2 ? lastViewedItem - 2 : 0;
    const int end = lastViewedItem < totalCount - 2 ? lastViewedItem + 2 : totalCount;
    for (int i = start; i < end; ++i) {
        const QModelIndex idx = d->viewItems.at(i).index;
        if (idx.row() == index.row()) {
            if (idx.internalId() == index.internalId() || idx.parent() == parent) {// ignore column
                lastViewedItem = i;
                return i;
            }
        }
    }

    // NOTE: this function is slow 
    for (int i = 0; i < totalCount; ++i) {
        const QModelIndex idx = d->viewItems.at(i).index;
        if (idx.row() == index.row()) {
            if (idx.internalId() == index.internalId() || idx.parent() == parent) {// ignore column
                lastViewedItem = i;
                return i;
            }
        }
    }
    // nothing found
    return -1;
}

void OutlineTree::mousePressEvent(QMouseEvent *event)
{
    Q_D(QTreeView);
    d_dragging = false;
    d_lastPressPoint = event->pos();
    // we want to handle mousePress in EditingState (persistent editors)
    if ((state() != NoState && state() != EditingState) || !d->viewport->rect().contains(event->pos())) {
        return;
    }
    const int vi = d->itemAtCoordinate( event->pos().y() );
	if( vi < 0 )
		return;
    const int col = d->columnAt( event->pos().x() );
	if( col < 0 )
		return;
	const int itemIndent = d->indentationForItem( vi );
    QRect r( columnViewportPosition( col ) + itemIndent, d->coordinateForItem(vi),
		columnWidth( col ) - itemIndent, d->itemHeight(vi) );
	QModelIndex i = d->modelIndex(vi);
	d_clickInEditor = false;
	if( r.contains( event->pos() ) )
	{
		// Klick im Edit-Bereich
		setCurrentIndex( i );
		startEdit( i, event );
		QAbstractItemView::mousePressEvent(event); // ruft intern edit auf
		d_clickInEditor = true;
	}else
	{
		// Klick im Handle-Bereich
		if( event->buttons() == Qt::RightButton )
			return; // Ansonsten wird Kontext immer verändert vor Menü
		closeEdit();
		setCurrentIndex( i );
        //const int indent = indentation();

		emit identClicked(); // Klick auf Handler-Bereich

		QRect r2( columnViewportPosition( col )	+ itemIndent - d_handleWidth, 
			d->coordinateForItem(vi), d_handleWidth, d->itemHeight(vi) );
		if( r2.contains( event->pos() ) && event->modifiers() == Qt::NoModifier )
        {
			// Klick im Expander-Bereich wird von QTreeView gehandhabt
            d_hitExpander = true;
			QTreeView::mousePressEvent( event );
		}else
        {
            // Klick ausserhalb Editor und Expanderbereich wird von QAbstractItemView gehandhabt
            d_hitExpander = false;
			QAbstractItemView::mousePressEvent(event);
        }
		if( event->modifiers() == Qt::ShiftModifier ) // wir wollen nur Drag/Drop wenn SHIFT gedrückt
            // Beginne Drag-Operation
            d_dragging = true;
	}
}

void OutlineTree::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QTreeView);
    const int vi = d->itemAtCoordinate( event->pos().y() );
	if( vi < 0 )
		return;
    const int col = d->columnAt( event->pos().x() );
	if( col < 0 )
		return;
	const int itemIndent = d->indentationForItem( vi );
	const int cvp = columnViewportPosition( col );
	const int cfi = d->coordinateForItem(vi);
	const int ih = d->itemHeight(vi);
    QRect r( cvp + itemIndent, cfi, columnWidth( col ) - itemIndent, ih );
	QRect r2( cvp, cfi, itemIndent, ih );
	QModelIndex i = d->modelIndex(vi);
	if( r.contains( event->pos() ) )
		edit( i, NoEditTriggers, event ); // Doppelklick auf Editor
	else if( r2.contains( event->pos() ) ) 
		emit identDoubleClicked(); // Doppelklick auf Handler-Bereich
	else
		QTreeView::mouseDoubleClickEvent( event );
}

void OutlineTree::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QTreeView);
	if( d_dragging )
	{
		// Es fand zuvor ein Click im Handle-Bereich statt. Wir selektieren oder starten Drag&Drop 
		if( ( d_lastPressPoint - event->pos() ).manhattanLength() > QApplication::startDragDistance() && d_dragEnabled )
		{
			QModelIndexList indexes = selectedIndexes();
			if (indexes.count() > 0) 
			{
				QMimeData *data = model()->mimeData(indexes);
				if (!data)
					return;
				QDrag *drag = new QDrag(this);
				drag->setMimeData(data);
				drag->exec(Qt::MoveAction);
			}
		}else
			QTreeView::mouseMoveEvent( event );
	}else if( d_clickInEditor )
	{
		// Move im Editor wird direkt an diesen geliefert. Ansonsten führt Move zu Änderung des
		// Current Index und damit Aufhebung der Selektion
		if( !d->sendDelegateEvent( currentIndex(), event) ) 
			QTreeView::mouseMoveEvent( event );
	}else
    {
        // Wir wollen kein MouseMove in Expander-Bereich
        if( !d_hitExpander )
            QAbstractItemView::mouseMoveEvent( event );
    }
}

void OutlineTree::updateGeometries()
{
	QTreeView::updateGeometries(); // setzt die Scrollbars
	QScrollBar* sb = verticalScrollBar();
	if( sb )
		sb->setSingleStep( d_stepSize );
}

void OutlineTree::setStepSize( quint8 s )
{
	d_stepSize = s;
	updateGeometries();
}

void OutlineTree::setHandleWidth( quint8 w )
{
	d_handleWidth = w;
	updateGeometries();
}

void OutlineTree::ensureVisibleInCurrent( int yOffset, int h )
{
    Q_D(const QTreeView);
	QScrollBar* sb = verticalScrollBar();
	if( sb == 0 )
		return;
    QRect area = d->viewport->rect();
	const int item = d->viewIndex( currentIndex() );
	const int y = d->coordinateForItem(item) + yOffset;
    const bool above = ( y < area.top() || area.height() < h );
    const bool below = ( y + h ) > area.bottom() && h < area.height();
    
	int verticalValue = sb->value();
    if ( above )
        verticalValue += y;
    else if ( below )
        verticalValue += ( y + h ) - area.height();
    verticalScrollBar()->setValue(verticalValue);
}

void OutlineTree::closeEdit()
{
	if( dynamic_cast<OutlineDeleg*>( itemDelegate() ) ) // Ansonsten Problem, falls nicht OutlineDeleg
	{
		itemDelegate()->setModelData( 0, model(), QModelIndex() );
		setState( QAbstractItemView::NoState );
	}
}

void OutlineTree::currentChanged ( const QModelIndex & current, const QModelIndex & previous ) 
{
	closeEdit();
	QTreeView::currentChanged( current, previous );
}

void OutlineTree::setModel( QAbstractItemModel * model )
{
	QTreeView::setModel( model );
	setSelectionModel( new OutlineTreeSelectionModel( model ) );
	restoreExpanded( QModelIndex() );
}

void OutlineTree::onCollapsed ( const QModelIndex & index )
{
	if( !d_block2 )
		model()->setData( index, false, OutlineMdl::ExpandedRole );
}

void OutlineTree::onExpanded ( const QModelIndex & index )
{
	if( !d_block2 )
		model()->setData( index, true, OutlineMdl::ExpandedRole );
	restoreExpanded( index );
}

void OutlineTree::restoreExpanded( const QModelIndex &index, int start, int end )
{
	const bool block = d_block2;
	d_block2 = true;
	const int max = qMax( model()->rowCount( index ) - 1, end );
	for( int row = start; row <= max; row++ )
	{
		const QModelIndex i = model()->index( row, 0, index );
		const bool exp = i.data( OutlineMdl::ExpandedRole ).toBool();
		if( exp )
			expand( i );
	}
	d_block2 = block;
}

void OutlineTree::rowsInserted ( const QModelIndex & parent, int start, int end )
{
	QTreeView::rowsInserted( parent, start, end );
	restoreExpanded( parent, start, end );
}

void OutlineTree::verticalScrollbarValueChanged(int value)
{
	QAbstractItemView::verticalScrollbarValueChanged( value );
	return;
	// NOTE: QAbstractItemView ruft canFetchMore nur mit parent=root auf. QTreeView nur in expand.
	//QModelIndex i = indexAt( 
    if( verticalScrollBar()->maximum() == value )
	{
		if( model()->canFetchMore( rootIndex() ) )
			model()->fetchMore( rootIndex() );
	}
}
