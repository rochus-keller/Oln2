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

#include "OutlineTreeDeleg.h"
#include <Gui/VariantValue.h>
#include "OutlineTreeModel.h"
#include <Text2/ParEditorCtrl.h>
#include <Text2/ParagraphMdl.h>
#include <Text2/TextFormat.h>
#include <private/qtexthtmlparser_p.h>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QtDebug>
using namespace Oln;
using namespace Text;
using namespace Gui;

static const int s_leftOff = 2;
static const int s_rightOff = 3;
static const int s_bottomOff = 1;

static TextFormat::ParagraphStyle levelToStyle( int l )
{
	switch( l )
	{
	case 0:
	case 1:
		return TextFormat::H1;
	case 2:
		return TextFormat::H2;
	case 3:
		return TextFormat::H3;
	case 4:
		return TextFormat::H4;
	case 5:
		return TextFormat::H5;
	default:
		return TextFormat::H6;
	}
}

OutlineTreeDeleg::OutlineTreeDeleg( QAbstractItemView* p, Text::ParagraphCtrl* ctrl ):
	QAbstractItemDelegate( p )
{
	if( ctrl == 0 )
		ctrl = new Text::ParEditorCtrl( p, 
					new Text::ParagraphView( new Text::ParagraphMdl() ), false );
	assert( ctrl && ctrl->view() );
	d_ctrl = ctrl;
	d_ctrl->setWnd( p->viewport() );
	d_ctrl->view()->addObserver( this );
	p->installEventFilter( this ); // wegen Focus und Resize während Edit
	// temp hat eigenes Format, damit keine Interferenz mit Editor
	d_temp = new ParagraphView( new ParagraphMdl() );
}

OutlineTreeDeleg::~OutlineTreeDeleg()
{
	d_ctrl->view()->removeObserver( this );
}

void OutlineTreeDeleg::handle( Root::Message& msg )
{
	BEGIN_HANDLER();
	MESSAGE( ParagraphView::Update, upd, msg )
	{
		switch( upd->getAction() )
		{
		case ParagraphView::Update::HeightChanged:
			{
				QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
				mdl->setData( d_edit, 0, Qt::SizeHintRole );
				// Dirty Trick um dataChanged auszulösen und Kosten für doItemsLayout zu sparen.
			}
			break;
		}
		msg.consume();
	}
	END_HANDLER();
}

QWidget * OutlineTreeDeleg::createEditor ( QWidget * parent, 
		const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	// Hierhin kommen wir immer, wenn ein anderes Item editiert werden soll.
	// Im Falle von QAbstractItemView::AllEditTriggers kommt man sofort hierher,
	// d.h. currentChanged() wird ausgelassen.
	closeEdit();

	d_edit = index;
	readData();
	// Bei Indent wird diese Funktion irrtümlich von Qt ohne gültige Breite aufgerufen.
	d_ctrl->view()->setTopLeft( option.rect.topLeft() + QPoint( s_leftOff, 0 ) ); // das muss hier stehen, damit erster Click richtig übersetzt
	if( option.rect.isValid() && option.rect.width() != d_ctrl->view()->getViewRect().width() )
		d_ctrl->view()->setWidth( option.rect.width() - s_leftOff - s_rightOff );
	d_ctrl->focusInEvent( 0, 0 );
	return 0;
}

void OutlineTreeDeleg::readData() const
{
	if( !d_edit.isValid() )
		return;
	QVariant v = d_edit.data(Qt::EditRole);
	QVariant titleRole = d_edit.data( OutlineTreeModel::TitleRole );
	QVariant level = d_edit.data( OutlineTreeModel::LevelRole );

	if( v.canConvert<VariantValue::ValueBox>() )
	{
		VariantValue::ValueBox f = v.value<VariantValue::ValueBox>();
		if( Text::ParagraphMdl::isParagraph( f.d_val, f.d_atoms ) )
		{
			Root::ValueReader reader( f.d_val.getData(), f.d_val.getDataLen() );
			d_ctrl->view()->getText()->read( reader, f.d_atoms );
		}
		d_ctrl->setPlainText( false );
	}else if( v.type() == QVariant::String )
	{
		d_ctrl->view()->getText()->clear();
		d_ctrl->view()->getText()->insertText( 0, v.toString().toLatin1() );
		d_ctrl->setPlainText( true );
	}else
	{
		d_ctrl->view()->getText()->clear();
		d_ctrl->setPlainText( false );
	}
	Text::TextFormat::ParagraphStyle style = Text::TextFormat::Normal;
	if( titleRole.toBool() )
		style = levelToStyle( level.toInt() );
	d_ctrl->view()->getFormat()->setFont( 
		Text::TextFormat::getFont( style, view()->font() ) ); 

	d_ctrl->view()->getText()->resetUndo();
	d_ctrl->view()->setCursor( d_ctrl->view()->getText()->getCount() );
}

void OutlineTreeDeleg::writeData() const
{
	if( !d_edit.isValid() )
		return;
	if( !d_ctrl->view()->getText()->isModified() )
		return;
	QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
	if( !d_ctrl->isPlainText() )
	{
		if( d_atoms.isNull() )
		{
			QVariant tmp = d_edit.data( OutlineTreeModel::AtomRole );
			if( tmp.canConvert<VariantValue::ValueBox>() )
			{
				VariantValue::ValueBox f = tmp.value<VariantValue::ValueBox>();
				d_atoms = f.d_atoms;
				assert( d_atoms );
			}else
				return; // RISK
		}
		Root::ValueWriter writer;
		d_ctrl->view()->getText()->write( writer, d_atoms );
		Root::Value v;
		writer.assignTo( v, false );
		mdl->setData( d_edit, VariantValue::toVariant( v, d_atoms ) );
	}else
	{
		QByteArray ba;
		d_ctrl->view()->getText()->writePlainText( ba );
		mdl->setData( d_edit, QString( ba ) );
	}
}

QSize OutlineTreeDeleg::sizeHint( const QStyleOptionViewItem & option, 
								   const QModelIndex & index ) const
{
	// NOTE: habe QTreeView ein wenig geändert, so dass with() nun immer korrekt
	// übergeben wird und wir hier height berechnen können. WidthRole überflüssig.

	if( d_edit == index )
	{
		return d_ctrl->view()->getViewRect().size().toSize() + 
			QSize( s_leftOff + s_rightOff, s_bottomOff );
	}

	int width = option.rect.width();
	if( width == -1 )
		return QSize(); // Noch nicht bekannt.
		// QSize() oder QSize(0,0) zeichnet gar nichts

	QVariant v = index.data(Qt::DisplayRole);
	QVariant titleRole = index.data( OutlineTreeModel::TitleRole );
	if( v.canConvert<VariantValue::ValueBox>() )
	{
		VariantValue::ValueBox f = v.value<VariantValue::ValueBox>();
		if( Text::ParagraphMdl::isParagraph( f.d_val, f.d_atoms ) )
		{
			Root::ValueReader reader( f.d_val.getData(), f.d_val.getDataLen() );
			d_temp->getText()->read( reader, f.d_atoms );
		}else
			return QSize();
	}else if( v.type() == QVariant::String )
	{
		d_temp->getText()->clear();
		d_temp->getText()->insertText( 0, v.toString().toLatin1() );
	}else
		return QSize(); // TODO

	Text::TextFormat::ParagraphStyle style = Text::TextFormat::Normal;
	if( titleRole.toBool() )
	{
		QVariant level = index.data( OutlineTreeModel::LevelRole );
		style = levelToStyle( level.toInt() );
	}
	d_temp->getFormat()->setFont( 
		Text::TextFormat::getFont( style, view()->font() ) ); 
	d_temp->setWidth( width - s_leftOff - s_rightOff );
	return d_temp->getViewRect().size().toSize() + 
		QSize( s_leftOff + s_rightOff, s_bottomOff );
}

void OutlineTreeDeleg::paint(QPainter *painter,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
	QVariant titleRole = index.data( OutlineTreeModel::TitleRole );
	QVariant level = index.data( OutlineTreeModel::LevelRole );
	if( level.type() == QVariant::Int )
	{
		QColor c;
		if( titleRole.toBool() )
			c = option.palette.color( QPalette::AlternateBase );
		else
			c = option.palette.color( QPalette::Base );
		c = QColor::fromHsv( c.hue(), c.saturation(), c.value() - ( level.toInt() - 1 ) * 2 ); // TEST
		painter->fillRect( option.rect, c );
	}
	if( d_edit == index && view()->hasFocus() )
	{
		painter->setPen( Qt::black );
		painter->drawRect( option.rect.adjusted( 0, 0, -1, -1 ) );
	}else
	{
		painter->setPen( Qt::white );
		painter->drawLine( option.rect.bottomLeft(), option.rect.bottomRight() );
	}

	if( d_edit == index )
	{
		d_ctrl->view()->setTopLeft( option.rect.topLeft() + QPoint( s_leftOff, 0 ) );
		d_ctrl->paint( painter );
		return;
	}
	if( option.showDecorationSelected && (option.state & QStyle::State_Selected)) 
		painter->fillRect(option.rect, Qt::lightGray );

	// Zeichne hier die Zellen, entweder im Edit-Mode oder im View-Mode
    QVariant v = index.data(Qt::DisplayRole);

	if( v.canConvert<VariantValue::ValueBox>() )
	{
		VariantValue::ValueBox f = v.value<VariantValue::ValueBox>();
		if( Text::ParagraphMdl::isParagraph( f.d_val, f.d_atoms ) )
		{
			Root::ValueReader reader( f.d_val.getData(), f.d_val.getDataLen() );
			d_temp->getText()->read( reader, f.d_atoms );
		}
	}else if( v.type() == QVariant::String )
	{
		d_temp->getText()->clear();
		d_temp->getText()->insertText( 0, v.toString().toLatin1() );
	}// else TODO
	Text::TextFormat::ParagraphStyle style = Text::TextFormat::Normal;
	if( titleRole.toBool() )
	{
		QVariant level = index.data( OutlineTreeModel::LevelRole );
		style = levelToStyle( level.toInt() );
	}
	d_temp->getFormat()->setFont( 
		Text::TextFormat::getFont( style, view()->font() ) ); 
	d_temp->setWidth( option.rect.width() - s_leftOff - s_rightOff );
	d_temp->setTopLeft( option.rect.topLeft() + QPoint( s_leftOff, 0 ) );
	d_temp->paint( painter );
}

bool OutlineTreeDeleg::editorEvent(QEvent *event,
                                QAbstractItemModel *model,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index)
{
	// option.rect bezieht sich immer auf das Item unter der Maus. event->pos() ist
	// daher immer innerhalb option.rect.

	// qDebug( "Event type %d row %d", event->type(), index.row() ); 
	// Hier kommt nur 2, 3, 4, 5 und 6 an
	switch( event->type() )
	{
	case QEvent::MouseButtonDblClick:
	case QEvent::MouseButtonPress:
		{
			if( d_edit == index  )
			{
				QMouseEvent* m = (QMouseEvent*) event;
				if( option.rect.contains( m->pos() ) )
					return d_ctrl->eventFilter( view()->viewport(), event );
				else
				{
					closeEdit();
					return true;
				}
			}else
				return false;
		}
		break;
	}
	if( d_edit.isValid() ) // Insb. MouseMove muss an Editor gehen
	{
		return d_ctrl->eventFilter( view()->viewport(), event );
	} // else TODO
	return false;
}

bool OutlineTreeDeleg::eventFilter(QObject *watched, QEvent *event)
{
	bool res = QAbstractItemDelegate::eventFilter( watched, event );
	if( d_edit.isValid() )
	{
		switch( event->type() )
		{
		case QEvent::FocusIn:
		case QEvent::FocusOut:
			d_ctrl->eventFilter( view()->viewport(), event );
			break;
		case QEvent::Resize:
			{
				QRect r = view()->visualRect( d_edit );
				d_ctrl->view()->setWidth( r.width() - s_leftOff - s_rightOff );
				view()->doItemsLayout(); // TODO: geht das auch billiger?
			}
			break;
		}
	}
	return res;
}

void OutlineTreeDeleg::closeEdit() const
{
	if( !d_edit.isValid() )
		return;
	writeData();
	d_ctrl->focusOutEvent( 0, 0 );
	view()->viewport()->update( d_ctrl->view()->getViewRect().
		toRect().adjusted(-s_leftOff, 0, s_rightOff, s_bottomOff ) );
	d_edit = QModelIndex();
}

void OutlineTreeDeleg::setModelData( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const
{
	if( d_edit.isValid() )
	{
		closeEdit();
	}
}
