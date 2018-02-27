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

#include "OutlineDeleg.h"
#include "OutlineTree.h"
#include "OutlineMdl.h"
#include "OutlineStream.h"
#include <Txt/TextOutHtml.h>
#include <Txt/Styles.h>
#include <QTextEdit>
#include <QPainter>
#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QAbstractItemView>
#include <QClipboard>
#include <QKeySequence>
#include <QtDebug>
#include <QToolTip>
#include <Txt/TextOutStream.h>
#include <Txt/TextCursor.h>
#include <Txt/TextHtmlImporter.h>
using namespace Oln;
using namespace Txt;

// Dies ist eine Adaption von DoorScope DocDeleg
// Deleg ist frei von Udb und damit mit jedem Modell anwendbar

static const qreal s_horiIdMargin = 2.0;
static const int s_lo = 1; // Left offset
static const int s_ro = 0; // Right offset
static const int s_wb = 1; // Change Bar Width
static const int s_vo = -2; // -2, Vertical Offset of Text (nur für den Text, nicht das Icon)
static const int s_vhr = -1; // -1, Vertical Height Reduction (nur für den Text, nicht das Icon)
static const int s_pb = 1; // Pix Board

OutlineDeleg::OutlineDeleg(OutlineTree *parent, Styles* s, const LinkRendererInterface *lr)
	: QAbstractItemDelegate(parent), d_isTitle(false), d_isReadOnly( false ), d_biggerTitle( true ), 
	  d_block1(false), d_showIcons( false ), d_linkRenderer( lr ), d_showIDs( true )
{
	parent->viewport()->installEventFilter( this ); // wegen Focus und Resize während Edit
	parent->installEventFilter( this ); // wegen Focus und Resize während Edit

	TextView* view = new TextView( this, s );
	d_ctrl = new TextCtrl( this, view );
	onFontStyleChanged();

	// Das muss so sein. Ansonsten werden Cursortasten-Moves nicht aktualisiert
	connect( d_ctrl->view(), SIGNAL( invalidate( const QRectF& ) ), this, SLOT( invalidate( const QRectF& ) ) );
	connect( view->getCursor().getStyles(), SIGNAL( sigFontStyleChanged() ), this, SLOT( onFontStyleChanged() ) );
}

OutlineDeleg::~OutlineDeleg()
{

}

bool OutlineDeleg::isReadOnly() const
{
	return d_isReadOnly || view()->model()->data( QModelIndex(), OutlineMdl::ReadOnlyRole ).toBool();
}

void OutlineDeleg::setReadOnly( bool on )
{
	d_isReadOnly = on;
	closeEdit();
}

void OutlineDeleg::setBiggerTitle( bool on )
{
	d_biggerTitle = on;
	onFontStyleChanged();
	closeEdit();
}

void OutlineDeleg::onFontStyleChanged()
{
	d_titleFont = Styles::inst()->getFont();
	d_titleFont.setBold( true );
	d_titleFont.setPointSizeF( d_titleFont.pointSizeF() * ( (d_biggerTitle)? 1.08 : 1.0 ) );
	QFontMetrics m( d_ctrl->view()->getCursor().getStyles()->getFont() );
	view()->setStepSize( m.height() );
	view()->doItemsLayout();
}

OutlineTree* OutlineDeleg::view() const 
{ 
	return static_cast<OutlineTree*>( parent() ); 
}

static QString collectText( const TextHtmlParser& parser )
{
	QString text;
	for( int i = 0; i < parser.count(); i++ )
		text += parser.at( i ).text.simplified();
	return text;
}

OutlineDeleg::Format OutlineDeleg::renderToDocument( const QModelIndex & index, QTextDocument& doc ) const
{
	const bool isTitle = index.data( OutlineMdl::TitleRole ).toBool();
	const QVariant v = index.data( Qt::DisplayRole );
	if( isTitle )
	{
        //const int level = qMin( index.data( OutlineMdl::LevelRole ).toInt(), 6 );
		QTextCursor cur( &doc );
		QTextCharFormat f;
		f.setFont( d_titleFont ); // d_ctrl->view()->getCursor().getStyles()->getFont( level ) );
		doc.setDefaultFont( d_titleFont );
		QTextBlockFormat format = d_ctrl->view()->getCursor().getStyles()->getBlockFormat( Styles::PAR );
		if( d_showIDs )
		{
			const QString id = index.data( OutlineMdl::IdentRole ).toString();
			if( !id.isEmpty() )
			{
				QFontMetricsF fm(d_titleFont);
				format.setTextIndent( fm.width( id ) + 2.0 * s_horiIdMargin );
			}
		}
		cur.setBlockFormat( format );
		if( v.canConvert<Oln::OutlineMdl::Html>() )
		{
			TextHtmlParser parser;
			parser.parse( v.value<Oln::OutlineMdl::Html>().d_html, 0 );
			cur.insertText( collectText( parser ), f );
		}else if( v.canConvert<Oln::OutlineMdl::Bml>() )
		{
			Stream::DataReader r( v.value<Oln::OutlineMdl::Bml>().d_bml );
			QTextDocument tempDoc;
			Txt::TextCursor tempCur( &tempDoc );
            Txt::TextInStream in( 0, d_linkRenderer );
            in.readFromTo( r, tempCur );
			cur.insertText( tempDoc.toPlainText(), f );
			// NOTE: auch hier wird Link-Text aufgeloest; ev. etwas aufwaendig
			// vorher einfach nur: cur.insertText( r.extractString(), f );
		}else
		{
			cur.insertText( v.toString(), f );
		}
		return Plain;
	}else
	{
		doc.setDefaultFont( d_ctrl->view()->getCursor().getStyles()->getFont( 0 ) );
		QTextBlockFormat format = d_ctrl->view()->getCursor().getStyles()->getBlockFormat( Styles::PAR );
		if( d_showIDs )
		{
			const QString id = index.data( OutlineMdl::IdentRole ).toString();
			if( !id.isEmpty() )
			{
				QFont f = doc.defaultFont();
				f.setBold(true);
				QFontMetricsF fm(f);
				format.setTextIndent( fm.width( id ) + 2.0 * s_horiIdMargin );
			}
		}
		if( v.canConvert<Oln::OutlineMdl::Html>() )
		{
			// NOTE: hier werden in doc alle Styles::s_linkSchema in ordentliche Links umgewandelt
			TextHtmlImporter imp( &doc, v.value<Oln::OutlineMdl::Html>().d_html );
			imp.setLinkRenderer( getLinkRenderer() );
			imp.setBlockFormat( format );
			imp.import();
			return Html;
		}else if( v.canConvert<Oln::OutlineMdl::Bml>() )
		{
            Txt::TextInStream in( 0, d_linkRenderer );
			Stream::DataReader r( v.value<Oln::OutlineMdl::Bml>().d_bml );
			Txt::TextCursor cur( &doc );
			cur.setParagraphStyle( Styles::PAR );
			cur.setFirstLineIndent( format.textIndent() );
			in.readFromTo( r, cur );
			return Bml;
		}else
		{
			QTextCursor cur( &doc );
			QTextCharFormat f;
			f.setFont( d_ctrl->view()->getCursor().getStyles()->getFont( 0 ) );
			cur.setBlockFormat( format );
			cur.insertText( v.toString(), f );
			return Plain;
		}
	}
}

void OutlineDeleg::paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	painter->save();

	const bool isAlternatingColor = view()->alternatingRowColors();
	QColor bgClr;
	if( !isAlternatingColor )
	{
		if( index.data( OutlineMdl::AliasRole ).toBool() )
		{
			bgClr = QColor::fromRgb( 245, 245, 245 ); // Gräulich
		}else if( index.data( OutlineMdl::TitleRole ).toBool() )
		{
			bgClr = QColor::fromRgb( 224, 249, 206 ); // Grünlich
		}else
		{
			bgClr = QColor::fromRgb( 255, 254, 225 ); // Gelblich
		}
		const int level = index.data( OutlineMdl::LevelRole ).toInt();
		bgClr = QColor::fromHsv( bgClr.hue(), bgClr.saturation(), bgClr.value() - ( level - 1 ) * 3 ); // TEST
		painter->fillRect( option.rect, bgClr );
	}

	const QSize ds = decoSize( index );
	QFont font;
	if( d_edit == index )
	{
		d_ctrl->setPos( option.rect.left() + ds.width(), option.rect.top() + s_vo );
		d_ctrl->paint( painter, option.palette );
		font = d_ctrl->view()->getDocument()->defaultFont();
	}else
	{
		const QPoint off = QPoint( option.rect.left() + ds.width(), option.rect.top() + s_vo );
		painter->translate( off );
		QTextDocument doc;
		renderToDocument( index, doc );
		doc.setTextWidth( option.rect.width() - ds.width() - s_ro );
		doc.drawContents( painter );
		painter->translate( -off.x(), -off.y() );
		font = doc.defaultFont();
	}
	if( d_showIDs )
	{
		const QString id = index.data( OutlineMdl::IdentRole ).toString();
		if( !id.isEmpty() )
		{
			font.setBold(true);
			QFontMetricsF fm(font);
			QRectF rect( option.rect.left() + ds.width() + s_horiIdMargin, option.rect.top(), fm.width( id ), fm.height() );
			painter->setPen( Qt::black );
			painter->setFont( font );
			painter->fillRect( rect.adjusted( -2.0, 0.0, 2.0, 0.0 ), bgClr.darker(107) );
			//painter->drawRect( rect.adjusted( -2.0, 0.0, 1.0, -2.0 ) );
			painter->drawText( rect, Qt::AlignCenter, id );
		}
	}
	painter->setPen( Qt::lightGray );
	if( d_showIcons && index.data( OutlineMdl::AliasRole ).toBool() )
	{
		QPixmap pix = index.data( Qt::DecorationRole ).value<QPixmap>();
		if( !pix.isNull() )
			painter->drawPixmap( option.rect.left() + s_lo + s_pb, option.rect.top() + s_pb, pix );
	}
	if( !isAlternatingColor )
	{
        // Zeichne grauen Rahmen um den Textbereich
        painter->setPen( Qt::lightGray );
		painter->drawLine( option.rect.left(), option.rect.bottom(), option.rect.right(), option.rect.bottom() ); // unten
		painter->drawLine( option.rect.left(), option.rect.top(), option.rect.left(), option.rect.bottom() ); // links vertikal
		painter->drawLine( option.rect.right(), option.rect.top(), option.rect.right(), option.rect.bottom() ); // rechts vertikal
	}
    if( d_edit == index && view()->hasFocus() )
	{
        if( view()->allColumnsShowFocus() )
        {
            painter->setPen( QPen( option.palette.color( QPalette::Highlight), 1 ) );
            // Hier zeichnet QTreeView noch ein Focus-Rect, darum um einen Point nach innen versetzt
            painter->drawLine( option.rect.left() + 1, option.rect.bottom()-1,
                               option.rect.right(), option.rect.bottom()-1 ); // unten
            painter->drawLine( option.rect.left() + 1, option.rect.top(),
                               option.rect.left() + 1, option.rect.bottom() - 1 ); // links
        }else
        {
            const int penWidth = 1; // 2
            painter->setPen( QPen( option.palette.color( QPalette::Highlight), penWidth ) );
            painter->drawLine( option.rect.left() + 1, option.rect.bottom(),
                               option.rect.right(), option.rect.bottom()); // unten
            painter->drawLine( option.rect.left() + penWidth / 2, option.rect.top() + penWidth / 2,
                               option.rect.left() + penWidth / 2, option.rect.bottom() ); // links
        }
	}
	painter->restore();
}

QSize OutlineDeleg::sizeHint ( const QStyleOptionViewItem & option, 
								 const QModelIndex & index ) const
{
	QSize s;
	const QSize ds = decoSize( index );
	if( d_edit == index )
	{
		s = d_ctrl->view()->getExtent().toSize();
		s.setHeight( qMax( ds.height(), s.height() + s_vo + s_vhr ) );
		return s;
	}

	const int width = option.rect.width();
	if( width == -1 )
		return QSize(0,0); // Noch nicht bekannt.
		// QSize() oder QSize(0,0) zeichnet gar nichts

	QTextDocument doc;
	renderToDocument( index, doc );
	doc.setTextWidth( width - ds.width() - s_ro );
	s = doc.size().toSize();
	s.setHeight( qMax( ds.height(), s.height() + s_vo + s_vhr ) ); 
	return s;
}

QSize OutlineDeleg::decoSize( const QModelIndex & index ) const
{
	if( d_showIcons && index.data( OutlineMdl::AliasRole ).toBool() )
	{
		QPixmap pix = index.data( Qt::DecorationRole ).value<QPixmap>();
		if( !pix.isNull() )
			return QSize( s_lo + s_pb + pix.width() + s_pb, pix.height() + 2 * s_pb + 1 ); // + 1 wegen unterer Trennlinie
	}
	return QSize( s_lo, 0 );
}

QWidget * OutlineDeleg::createEditor ( QWidget * parent, 
		const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    Q_UNUSED(parent);
	// Hierhin kommen wir immer, wenn ein anderes Item editiert werden soll.
	// Im Falle von QAbstractItemView::AllEditTriggers kommt man sofort hierher,
	// d.h. currentChanged() wird ausgelassen.
	closeEdit();

	disconnect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	d_edit = index;
	readData();
	// Bei Indent wird diese Funktion irrtümlich von Qt ohne gültige Breite aufgerufen.
	const QSize ds = decoSize( index );
	d_ctrl->setPos( option.rect.left() + ds.width(), option.rect.top() + s_vo ); // das muss hier stehen, damit erster Click richtig übersetzt
	if( ( option.rect.width() - ds.width() - s_ro ) != d_ctrl->view()->getExtent().width() )
		d_ctrl->view()->setWidth( option.rect.width() - ds.width() - s_ro, option.rect.height() );
	
	if( option.rect.height() != qMax( ds.height(), int( d_ctrl->view()->getExtent().height() + s_vo + s_vhr ) ) )
	{
		QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
		mdl->setData( d_edit, 0, Qt::SizeHintRole );
		// Dirty Trick um dataChanged auszulösen und Kosten für doItemsLayout zu sparen.
	}

	view()->viewport()->update( option.rect );

	connect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	return 0;
}

bool OutlineDeleg::editorEvent(QEvent *event,
                                QAbstractItemModel *model,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index)
{
    Q_UNUSED(option);
    Q_UNUSED(model);
	// option.rect bezieht sich immer auf das Item unter der Maus. event->pos() ist
	// daher immer innerhalb option.rect.

	if( d_edit == index  )
	{
		switch( event->type() )
		{
		case QEvent::MouseButtonDblClick:
			d_ctrl->mouseDoubleClickEvent( (QMouseEvent*) event );
			return true; // Event wird in jedem Fall konsumiert, damit Selektion nicht verändert wird durch Tree::edit
		case QEvent::MouseButtonPress:
			d_ctrl->mousePressEvent( (QMouseEvent*) event );
			return true; // Dito
		case QEvent::MouseButtonRelease:
			d_ctrl->mouseReleaseEvent( (QMouseEvent*) event );
			return true; // Dito
		case QEvent::MouseMove:
			d_ctrl->mouseMoveEvent( (QMouseEvent*) event );
			return true; // Dito
		case QEvent::KeyPress:
			{
				// Lasse PageUp/Down durch an Tree
				QKeyEvent* e = (QKeyEvent*) event;
				switch( e->key() ) 
				{
				case Qt::Key_Home:
				case Qt::Key_End:
				case Qt::Key_PageDown:
				case Qt::Key_PageUp:
					if( e->modifiers() == Qt::NoModifier )
						return false;
				case Qt::Key_Return:
				case Qt::Key_Enter:
					if( e->modifiers() != Qt::ControlModifier )
						return false; // nur Softbreaks werden an den Editor weitergegeben
				}
				if( d_ctrl->keyPressEvent( e ) )
				{
					// Sorge dafür, dass Cursor immer sichtbar bleibt während Eingabe, bzw. dass er nicht
					// unter dem Rand verschwindet
					QRectF r = d_ctrl->getView()->cursorRect();
					view()->ensureVisibleInCurrent( r.top(), r.height() );
					return true;
				}
			}
			break;
		// Focus, Paint und Resize kommen hier nicht an.
        case QEvent::InputMethod:
            {
                QInputMethodEvent* e = (QInputMethodEvent*) event;
                if( d_ctrl->inputMethodEvent( e ) )
                {
                    // Sorge dafür, dass Cursor immer sichtbar bleibt während Eingabe, bzw. dass er nicht
                    // unter dem Rand verschwindet
                    QRectF r = d_ctrl->getView()->cursorRect();
                    view()->ensureVisibleInCurrent( r.top(), r.height() );
                    return true;
                }

            }
            break;
		default:
            qDebug( "OutlineDeleg::editorEvent: other editor event %d", event->type() );
		}
	}else
	{
		//closeEdit();
		return false;
	}
	return false;
}

void OutlineDeleg::readData() const
{
	if( !d_edit.isValid() )
		return;
	d_ctrl->view()->clear();
	const bool editable = !d_isReadOnly && !d_edit.data( OutlineMdl::ReadOnlyRole ).toBool() && 
		!d_edit.data( OutlineMdl::AliasRole ).toBool();
	d_ctrl->view()->getDocument()->setUndoRedoEnabled( false ); // Das ist nötig, damit fillDoc nicht Undo unterliegt.
	d_editFormat = renderToDocument( d_edit, *d_ctrl->view()->getDocument() );
	d_isTitle = d_edit.data( OutlineMdl::TitleRole ).toBool();
	d_ctrl->view()->getDocument()->setUndoRedoEnabled( true );
	d_ctrl->view()->getDocument()->setModified( false );
	d_ctrl->view()->setCursorVisible( editable );
	d_ctrl->view()->setBlinkingCursorEnabled( editable );
	d_ctrl->view()->setReadOnly( !editable );
}

void OutlineDeleg::writeData( const QModelIndex & index ) const
{
	if( !index.isValid() )
		return;
	if( d_ctrl->view()->isReadOnly() )
		return;
	if( !d_ctrl->view()->getDocument()->isModified() )
		return;
	d_ctrl->view()->getDocument()->setModified( false );
	QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( index.model() );
	if( d_editFormat == Plain )
	{
		if( Txt::TextOutStream::hasStyle( d_ctrl->view()->getDocument() ) )
			d_editFormat = Bml;
		else
		{
			mdl->setData( index, d_ctrl->view()->getDocument()->toPlainText() );
			return;
		}
	}
	if( d_editFormat == Bml )
	{
		Stream::DataWriter w;
		Txt::TextOutStream::writeTo( w, d_ctrl->view()->getDocument() );
		Oln::OutlineMdl::Bml bml;
		bml.d_bml = w.getStream();
		mdl->setData( index, QVariant::fromValue( bml ) );
		return;
	}
	if( d_editFormat == Html )
	{
		Oln::OutlineMdl::Html html;
		Txt::TextOutHtml exp( d_ctrl->view()->getDocument() );
		html.d_html = exp.toHtml();
		// qDebug() << html.d_html; // TEST
		mdl->setData( index, QVariant::fromValue( html ) );
		return;
	}
}

void OutlineDeleg::writeData() const
{
	if( !d_edit.isValid() )
		return;
	writeData( d_edit );
}

void OutlineDeleg::invalidate( const QRectF& r )
{
	QRect rr = r.toRect();
	rr.translate( d_ctrl->getDx(), d_ctrl->getDy() );
	view()->viewport()->update( rr );
}

void OutlineDeleg::extentChanged()
{
	if( d_block1 )
		return;
	d_block1 = true;
	QAbstractItemModel* mdl = const_cast<QAbstractItemModel*>( d_edit.model() );
	// Dirty Trick um dataChanged auszulösen und Kosten für doItemsLayout zu sparen.
	mdl->setData( d_edit, 0, Qt::SizeHintRole );
	// folgendes wird bei jedem keyPressEvent erledigt, nicht nur bei extentChanged
	//QRectF r = d_ctrl->getView()->cursorRect();
	//view()->ensureVisibleInCurrent( r.top(), r.height() );
	d_block1 = false;
}

TextView* OutlineDeleg::getEditor() const
{
	if( !d_edit.isValid() )
		return 0;
	else
        return d_ctrl->view();
}

quint64 OutlineDeleg::getEditOid() const
{
    if( !d_edit.isValid() )
        return 0;
    else
        return d_edit.data( OutlineMdl::OidRole ).toULongLong();
}

void OutlineDeleg::closeEdit() const
{
	if( !d_edit.isValid() )
		return;
	QModelIndex index = d_edit; // Um loops via closeEdit in setEditorData zu vermeiden
	d_edit = QModelIndex();
	d_ctrl->view()->setCursorVisible( false );
	d_ctrl->view()->setBlinkingCursorEnabled(false);
	disconnect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
	writeData( index );
	d_ctrl->view()->setDocument( 0 );
	view()->viewport()->update( view()->visualRect( index ) );
	connect( d_ctrl->view(), SIGNAL( extentChanged() ), this, SLOT( extentChanged() ) );
}

void OutlineDeleg::relayout()
{
	view()->doItemsLayout();
	if( d_edit.isValid() )
	{
		QRect r = view()->visualRect( d_edit );
		const QSize ds = decoSize( d_edit );
		d_ctrl->view()->setWidth( r.width() - ds.width() - s_ro, r.height() );
		view()->scrollTo( d_edit );
	}
}

bool OutlineDeleg::eventFilter(QObject *watched, QEvent *event)
{
	if( watched == view()->viewport() && event->type() == QEvent::Resize )
	{
		QResizeEvent* e = static_cast<QResizeEvent*>( event );
		if( e->size().width() != e->oldSize().width() )
			// Bei unmittelbarer Ausführung scheint die Breite noch nicht zu stimmen.
			QMetaObject::invokeMethod( this, "relayout", Qt::QueuedConnection );
		return false; // auch Tree soll etwas davon haben
	}
	if( watched == view() && d_edit.isValid() )
	{
		switch( event->type() )
		{
		case QEvent::FocusIn:
			d_ctrl->handleFocusIn();
			return false;
		case QEvent::FocusOut:
			d_ctrl->handleFocusOut();
			return false;
        default:
            break;
		}
	}
	return false;
}

void OutlineDeleg::setModelData( QWidget *, QAbstractItemModel *, const QModelIndex & ) const
{
	// Mit diesem Trick kann man Editor schliessen, ohne Deleg zu spezialisieren.
	closeEdit();
}

void OutlineDeleg::insertHtml( QString html )
{
	if( !isEditing() || d_ctrl->view()->isReadOnly() )
		return;
    // In Linux kommt nur ein Fragment, das mit <meta http-equiv="content-type" content="text/html; charset=utf-8"> beginnt
    // siehe auch Herald MailObj::removeAllMetaTags und MailObj::correctWordMailHtml
//    if( html.startsWith( QLatin1String( "<meta" ), Qt::CaseInsensitive ) )
//    {
//        const int pos = html.indexOf( QChar('>' ) );
//        if( pos != -1 )
//            html = html.mid( pos + 1 );
//    }
	// 2015-12-10: dem TextHtmlParser ist "<meta charset>" egal
	QTextDocument doc;
	Txt::TextHtmlImporter imp( &doc, html );
	imp.import();
	if( d_isTitle )
	{
		getEditor()->getCursor().insertText( doc.toPlainText() );
	}else
	{
		getEditor()->getCursor().insertFragment( QTextDocumentFragment( &doc ) );
		d_editFormat = Html;
	}
}

QUrl OutlineDeleg::getSelUrl() const
{
	if( !isEditing() )
		return QUrl();
	if( !d_ctrl->view()->getCursor().hasSelection() || !d_ctrl->view()->getCursor().isUrl() )
		return QUrl();
    return d_ctrl->view()->getCursor().getUrl();
}

QByteArray OutlineDeleg::getSelLink() const
{
    if( !isEditing() )
		return QByteArray();
	if( !d_ctrl->view()->getCursor().hasSelection() || !d_ctrl->view()->getCursor().isLink() )
		return QByteArray();
    return d_ctrl->view()->getCursor().getLink();
}

void OutlineDeleg::activateAnchor()
{
    if( !isEditing() )
        return;
    d_ctrl->activateAnchor();
}
