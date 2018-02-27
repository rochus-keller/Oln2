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

#include "OutlineCtrl.h"
#include "OutlineMdl.h"
#include "OutlineItem.h"
#include "EditUrlDlg.h"
#include <Gui2/UiFunction.h>
#include <Txt/TextOutStream.h>
#include <Txt/TextCursor.h>
#include <Txt/TextOutHtml.h>
#include <QApplication>
#include <QMouseEvent>
#include <QClipboard>
#include <QKeySequence>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <cassert>
using namespace Oln;
using namespace Stream;
using namespace Txt;

// TODO: Funktionen aus OutlineUdbCtrl übernehmen und ohne Udb-Abhängigkeit via AbstractItemModel realisieren

OutlineCtrl::OutlineCtrl( OutlineTree* p, LinkRendererInterface * lr ):Controller( p )
{
    d_deleg = new OutlineDeleg( p, 0, lr );
	p->setItemDelegate( d_deleg );
    connect( d_deleg->getEditCtrl(), SIGNAL( anchorActivated( QByteArray,bool ) ),
		this, SLOT( followUrl(QByteArray,bool) ) );
}

OutlineTree* OutlineCtrl::getTree() const
{
	return static_cast<OutlineTree*>( parent() );
}

void OutlineCtrl::onBold()
{
	CHECKED_IF( canFormat(), d_deleg->getEditor() && d_deleg->getEditor()->getCursor().isStrong() );

	d_deleg->getEditor()->getCursor().setStrong( !d_deleg->getEditor()->getCursor().isStrong() );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onUnderline()
{
	CHECKED_IF( canFormat(), d_deleg->getEditor() && d_deleg->getEditor()->getCursor().isUnder() );
	d_deleg->getEditor()->getCursor().setUnder( !d_deleg->getEditor()->getCursor().isUnder() );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onItalic()
{
	CHECKED_IF( canFormat(), d_deleg->getEditor() && d_deleg->getEditor()->getCursor().isEm() );
	d_deleg->getEditor()->getCursor().setEm( !d_deleg->getEditor()->getCursor().isEm() );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onStrike()
{
	CHECKED_IF( canFormat(), d_deleg->getEditor() && d_deleg->getEditor()->getCursor().isStrike() );
	d_deleg->getEditor()->getCursor().setStrike( !d_deleg->getEditor()->getCursor().isStrike() );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onSuper()
{
	CHECKED_IF( canFormat(), d_deleg->getEditor()->getCursor().isSuper() );
	d_deleg->getEditor()->getCursor().setSuper( !d_deleg->getEditor()->getCursor().isSuper() );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onSub()
{
	CHECKED_IF( canFormat(), d_deleg->getEditor()->getCursor().isSub() );
	d_deleg->getEditor()->getCursor().setSub( !d_deleg->getEditor()->getCursor().isSub() );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onFixed()
{
	CHECKED_IF( canFormat(), d_deleg->getEditor()->getCursor().isFixed() );
	d_deleg->getEditor()->getCursor().setFixed( !d_deleg->getEditor()->getCursor().isFixed() );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onSave()
{
	ENABLED_IF( canFormat() );

	d_deleg->writeData();
}

void OutlineCtrl::onTimestamp()
{
	ENABLED_IF( canFormat() );

	d_deleg->getEditor()->getCursor().insertText( OutlineItem::formatTimeStamp( QDateTime::currentDateTime() ) );
	d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onInsertDate()
{
	ENABLED_IF( canFormat() );

	d_deleg->getEditor()->getCursor().insertText( QDate::currentDate().toString( "yyyy-MM-dd" ) );
	d_deleg->getEditor()->invalidate();
}

static inline bool _LessThan(const QModelIndex& l, const QModelIndex& r) { return l.row() < r.row(); }


bool OutlineCtrl::copyToClipBoard( bool cut )
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
		QString html = "<html>";
		QString text;
		qSort( l.begin(), l.end(), _LessThan );
		for( int i = 0; i < l.size(); i++ )
		{
			QTextDocument doc;
			d_deleg->renderToDocument( l[i], doc );
			Txt::TextOutHtml out( &doc );
			html += out.toHtml( true );
			text += doc.toPlainText() + QChar('\n');
		}
		html += "</html>";
		QMimeData* mime = new QMimeData();
		mime->setText( text );
		mime->setHtml( html );
		QApplication::clipboard()->setMimeData( mime );
		if( cut )
		{
			// TODO
		}
		return true;
	}
	return false;
}

void OutlineCtrl::nextOrSub( QModelIndex& parent, int& newRow )
{
    // Setze parent und row so, dass abhängig vom gerade editierten oder currentIndex() das neue
    // Item entweder als nächstes oder als erstes untergeordnetes eingefügt wird.
	QModelIndex pred = d_deleg->getEditIndex(); // Vorgänger oder Parent des neu eingefügten
	if( !pred.isValid() )
        // Es wird nicht editiert; also verwende Current
		pred = getTree()->currentIndex();
	if( pred.isValid() )
	{
		if( getTree()->isExpanded( pred ) && getTree()->model()->hasChildren( pred ) )
		{
            // Der Kandidat ist geöffnet und hat Kinder; insert ist also an erster Stelle unter dem Kandidaten
			parent = pred;
			newRow = 0;
		}else
		{
            // Der Kandidat ist ein Element in der Liste
			parent = pred.parent();
			if( d_deleg->isEditing() && d_deleg->getEditCtrl()->getView()->getCursor().getPosition() == 0 )
				newRow = pred.row(); // Wenn Cursor ganz am Anfang, füge Item davor ein.
			else
				newRow = pred.row() + 1; // Wir sind bereits der Vorgänger
		}
	}else
	{
        // Es wird nicht editiert und es gibt keinen Current Index; füge zuunterst ein im Root
		parent = QModelIndex();
		newRow = getTree()->model()->rowCount();
    }
}

void OutlineCtrl::setModel(OutlineMdl * mdl)
{
    OutlineTree* p = getTree();
    p->setModel( mdl ); // erst mit setModel wird ein selectionModel angelegt. Vorher kommt null zurück.
    connect( p->selectionModel(),
		SIGNAL(currentChanged( const QModelIndex &,const QModelIndex & ) ),
		this, SLOT( onCurrentChanged ( const QModelIndex &,const QModelIndex & ) ) );
}

void OutlineCtrl::editUrl()
{
    QString url = d_deleg->getEditor()->getCursor().getUrl().toString();
	QString text = d_deleg->getEditor()->getCursor().getAnchorText();
	EditUrlDlg dlg( getTree() );
	if( dlg.edit( url, text ) )
	{
		d_deleg->getEditor()->getCursor().beginEditBlock();
		d_deleg->getEditor()->getCursor().deleteCharOrSelection();
		d_deleg->getEditor()->getCursor().insertUrl( url, false, text );
		d_deleg->getEditor()->getCursor().endEditBlock();
		d_deleg->getEditor()->invalidate();
    }
}

void OutlineCtrl::insertUrl()
{
    EditUrlDlg dlg( getTree() );
    dlg.setWindowTitle( tr("Insert URL") );
    QString url, text = d_deleg->getEditor()->getCursor().getSelectedText();
    if( dlg.edit( url, text ) )
    {
        d_deleg->getEditor()->getCursor().insertUrl( url, false, text );
        d_deleg->getEditor()->invalidate();
    }
}

bool OutlineCtrl::canFormat()
{
    return d_deleg->isEditing() && !d_deleg->isTitle() &&
            !d_deleg->getEditor()->isReadOnly() && !d_deleg->getEditor()->isPlainText();
}

bool OutlineCtrl::pasteFromClipBoard(bool special)
{
	if( d_deleg->isReadOnly() )
		return false;
	if( d_deleg->isEditing() ) 
	{
		const bool ro = d_deleg->getEditor()->isReadOnly();
		const QMimeData* mime = QApplication::clipboard()->mimeData();
		if( mime->hasFormat( Txt::TextInStream::s_mimeRichText ) && !ro )
		{
			Stream::DataReader r( mime->data( Txt::TextInStream::s_mimeRichText ) );
            if( special )
            {
                d_deleg->getEditor()->getCursor().insertText( r.extractString() );
            }else
            {
                Txt::TextInStream in( d_deleg->getEditor()->getCursor().getStyles(),
                                      d_deleg->getLinkRenderer() );
                d_deleg->getEditor()->getCursor().beginEditBlock();
                in.readFromTo( r, d_deleg->getEditor()->getCursor(), true );
                d_deleg->getEditor()->getCursor().endEditBlock();
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
		}else if( mime->hasImage() && !ro )
		{
			d_deleg->getEditor()->getCursor().insertImg( mime->imageData().value<QImage>() );
			d_deleg->getEditor()->invalidate();
			return true;
		}else if( mime->hasUrls() && !ro )
		{
			QList<QUrl> l = mime->urls();
			for( int i = 0; i < l.size(); i++ )
				d_deleg->getEditor()->getCursor().insertUrl( l[i] );
			d_deleg->getEditor()->invalidate();
			return true;
		}else if( mime->hasText() && !ro )
		{
			const QString text = mime->text();
			if( text.startsWith( QLatin1String( "http" ) ) )
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
	int row;
	nextOrSub( parent, row );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	// TODO: weitere Spezialisierungen
	bool res = getTree()->model()->dropMimeData( QApplication::clipboard()->mimeData(), Qt::CopyAction, row, 0, parent );
	QApplication::restoreOverrideCursor();
    return res;
}

bool OutlineCtrl::isFormatSupported(const QMimeData * mime)
{
	if( mime->hasFormat( Txt::TextInStream::s_mimeRichText ) ||
            mime->hasText() ||
            mime->hasHtml() ||
            mime->hasImage() ||
            mime->hasUrls() )
        return true;
    else
        return false;
}

void OutlineCtrl::addTextCommands(Gui2::AutoMenu * sub)
{
    sub->addCommand( tr("Bold"), this, SLOT(onBold()), tr("CTRL+B"), true )->setCheckable(true);
	sub->addCommand( tr("Underline"), this, SLOT(onUnderline()), tr("CTRL+U"), true )->setCheckable(true);
	sub->addCommand( tr("Italic"),  this, SLOT(onItalic()), tr("CTRL+I"), true )->setCheckable(true);
	sub->addCommand( tr("Strikeout"),  this, SLOT(onStrike()), tr("SHIFT+CTRL+U"), true )->setCheckable(true);
	sub->addCommand( tr("Fixed"), this, SLOT(onFixed()), tr("SHIFT+CTRL+I"), true )->setCheckable(true);
	sub->addCommand( tr("Superscript"), this, SLOT(onSuper()), tr("SHIFT+CTRL+P"), true )->setCheckable(true);
	sub->addCommand( tr("Subscript"),  this, SLOT(onSub()), tr("SHIFT+CTRL+B"), true )->setCheckable(true);
	sub->addSeparator();
	sub->addCommand( tr("Insert Timestamp"),  this, SLOT(onTimestamp()), tr("CTRL+M"), true );
    sub->addCommand( tr("Insert Date"), this, SLOT(onInsertDate()), tr("CTRL+SHIFT+M"), true );
    sub->addCommand( tr("Edit Link..."),  this, SLOT(onEditUrl()), tr("CTRL+SHIFT+L"), true );
	sub->addCommand( tr("Strip Whitespace"), this, SLOT(onStripWhitespace()),tr("CTRL+SHIFT+W"), true );
    sub->addSeparator();
	sub->addCommand( tr("Follow Link"), this, SLOT(onFollowAnchor()) );
}

QString OutlineCtrl::fetchHtml(const QMimeData * mime)
{
	if( mime == 0 || !mime->hasHtml() )
		return QString();
	const QVariant data = mime->data( QLatin1String("text/html") );
	if( data.type() == QVariant::String )
		return data.toString();
	else
		// in QMimeDataPrivate::retrieveTypedData wird leider latin-1 als Default Codec genommen
		return fetchHtml( data.toByteArray() );
}

QString OutlineCtrl::fetchHtml(const QByteArray & ba)
{
	QTextCodec *codec = QTextCodec::codecForName("utf-8");
	codec = QTextCodec::codecForHtml(ba, codec);
	return codec->toUnicode(ba);
}

bool OutlineCtrl::keyPressEvent(QKeyEvent* e)
{
    if( e == QKeySequence::Copy )
		return copyToClipBoard();
	else if( e == QKeySequence::Cut )
		return copyToClipBoard( true );
	else if( e == QKeySequence::Paste )
		return pasteFromClipBoard();
	else if( e == QKeySequence::Undo && d_deleg->isEditing() )
	{
		d_deleg->getEditor()->undo();
		return true;
	}
	else if( e == QKeySequence::Redo && d_deleg->isEditing() )
	{
		d_deleg->getEditor()->redo();
		return true;
	}
	else if( e == QKeySequence::SelectAll && d_deleg->isEditing() )
	{
		d_deleg->getEditor()->selectAll();
		return true;
	}else
		return false;
}

void OutlineCtrl::onCut()
{
    ENABLED_IF( ( d_deleg->isEditing() && d_deleg->getEditor()->getCursor().hasSelection() &&
                  !d_deleg->getEditor()->isReadOnly() ) ||
				( !d_deleg->isReadOnly() && !getTree()->selectionModel()->selectedRows().isEmpty() ) );
	copyToClipBoard( true );
}

void OutlineCtrl::onCopy()
{
    ENABLED_IF( ( d_deleg->isEditing() && d_deleg->getEditor()->getCursor().hasSelection() ) ||
		!getTree()->selectionModel()->selectedRows().isEmpty() );
	copyToClipBoard();
}

void OutlineCtrl::onPaste()
{
    ENABLED_IF( isFormatSupported( QApplication::clipboard()->mimeData() ) && !d_deleg->isReadOnly() );
	pasteFromClipBoard();
}


void OutlineCtrl::onCurrentChanged( const QModelIndex & current,const QModelIndex & previous ) 
{
    Q_UNUSED(previous);
	if( current.isValid() )
	{
		const quint64 id = current.data( OutlineMdl::OidRole ).toULongLong();
		if( id )
			emit sigCurrentChanged( id );
	}
}

void OutlineCtrl::onInsertUrl()
{
    // obsolet
    onEditUrl();
}

void OutlineCtrl::onEditUrl()
{
    ENABLED_IF( canFormat() &&
                ( d_deleg->getEditor()->getCursor().isUrl() ||
                  d_deleg->getEditor()->getCursor().hasSelection() ) );

    if( d_deleg->getEditor()->getCursor().isUrl() )
        editUrl();
    else
        insertUrl();
}

void OutlineCtrl::onOpenUrl()
{
    QUrl url = d_deleg->getSelUrl();
	ENABLED_IF( url.isValid() );

	QDesktopServices::openUrl( url );
}

void OutlineCtrl::onCopyUrl()
{
    QUrl url = d_deleg->getSelUrl();
	ENABLED_IF( url.isValid() );
	QMimeData* mime = new QMimeData();
	mime->setUrls( QList<QUrl>() << url );
	mime->setText( url.toEncoded() );
    QApplication::clipboard()->setMimeData( mime );
}

void OutlineCtrl::onStripWhitespace()
{
    ENABLED_IF( canFormat() );
    d_deleg->getEditor()->getCursor().stripWhiteSpace();
    d_deleg->getEditor()->invalidate();
}

void OutlineCtrl::onFollowAnchor()
{
    ENABLED_IF( d_deleg->isEditing() && d_deleg->getEditor()->getCursor().isAnchor() );

    d_deleg->activateAnchor();
}

void OutlineCtrl::followUrl(QByteArray data, bool isUrl)
{
    if( isUrl )
    {
        QUrl url = QUrl::fromEncoded( data );
        if( url.isValid() )
            emit sigUrlActivated( url );
    }
}
