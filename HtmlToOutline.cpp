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

#include "HtmlToOutline.h"
#include "OutlineUdbMdl.h"
#include "OutlineItem.h"
#include <QFile>
#include <QBuffer>
#include <QtDebug>
#include <bitset>
#include <QStack>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QImageReader>
#include <QStyle>
#include <Stream/DataWriter.h>
#include <Txt/TextInStream.h>
#include <Txt/TextHtmlParser.h>
#include <Txt/ImageGlyph.h>
using namespace Oln;
using namespace Stream;
using namespace Txt;

static QString quoteNewline(const QString &s)
{
	QString n = s;
	n.replace(QLatin1Char('\n'), QLatin1String("\\n"));
	return n;
}

HtmlToOutline::HtmlToOutline()
{
}

struct Context
{
	Context():home(0),d_skipWs(false){}
	bool d_skipWs;
	QStack<int> trace; // Level
	Udb::Obj oln;
	QStack<Udb::Obj> parent;
	DataWriter bml;
	DataCell::OID home;
	Txt::TextHtmlParser parser;
	QDir dir;
	Udb::Obj createObject()
	{
		Udb::Obj o = oln.createObject( OutlineItem::TID );
		o.setValue( OutlineItem::AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		o.setValue( OutlineItem::AttrHome, DataCell().setOid( home ) );
		return o;
	}
};

static void readImg( Context& ctx, const Txt::TextHtmlParserNode& n )
{
	ctx.bml.startFrame( NameTag( "img" ) );

	QImage img;
	bool ok = false;

	if( n.imageName.startsWith( QLatin1String( "file:" ) ) )
	{
		// NOTE n.imageName ist noch kodiert und enthält z.B. %20
		QFileInfo path;
		if( n.imageName.startsWith( QLatin1String( "file:///" ), Qt::CaseInsensitive ) ) // Windows-Spezialität
			// siehe http://en.wikipedia.org/wiki/File_URI_scheme
			path = n.imageName.mid( 8 );
		else
		{
			const QUrl url = QUrl::fromEncoded( n.imageName.toAscii(), QUrl::TolerantMode );
			path = url.toLocalFile();
		}
		if( path.isRelative() )
			path = ctx.dir.absoluteFilePath( path.fileName() );
		if( img.load( path.absoluteFilePath() ) )
			ok = true;
	}else if( n.imageName.startsWith("data:") )
	{
		img = Txt::ImageGlyph::parseDataSrc(n.imageName);
		if( !img.isNull() )
			ok = true;
	}
	if( !ok )
	{
		ctx.bml.writeSlot( Stream::DataCell().setImage( 
			QApplication::style()->standardPixmap( QStyle::SP_FileIcon ).toImage() ) );
	}else
	{
		if( n.imageWidth > 0 && n.imageHeight > 0 )
			img = img.scaled( QSize( n.imageWidth, n.imageHeight ), Qt::KeepAspectRatio, Qt::SmoothTransformation );
		ctx.bml.writeSlot( Stream::DataCell().setImage( img ) );
	}
	ctx.bml.endFrame(); // img
}

static int f2i( Txt::TextHTMLElements f )
{
	switch( f )
	{
	case Html_em:
	case Html_i:
	case Html_cite:
	case Html_var:
	case Html_dfn:
		return Txt::TextInStream::Italic;
	case Html_big:
	case Html_small:
	case Html_nobr: // unbekannt
		// ignorieren
		break;
	case Html_strong:
	case Html_b:
		return Txt::TextInStream::Bold;
	case Html_u:
		return Txt::TextInStream::Underline;
	case Html_s:
		return Txt::TextInStream::Strikeout;
	case Html_code:
	case Html_tt:
	case Html_kbd:
	case Html_samp:
		return Txt::TextInStream::Fixed;
	case Html_sub:
		return Txt::TextInStream::Sub;
	case Html_sup:
		return Txt::TextInStream::Super;
    default:
        break;
	}
	return -1;
}

static void writeFormat( Context& ctx, const std::bitset<8>& format )
{
	if( format.any() )
		ctx.bml.writeSlot( DataCell().setUInt8( format.to_ulong() ) );
}

static QString strip( QString str, bool trimLeft = false )
{
	if( trimLeft )
	{
		int i = 0;
		for( ; i < str.size(); i++ )
		{
			if( !str[i].isSpace() )
				break;
		}
		str = str.mid(i);
	}
	str.replace(QLatin1Char('\n'), QLatin1Char( ' ' ) );
	return str;
}

static void consumeFollowers( Context& ctx, int n, std::bitset<8>& format, int fidx = -1 )
{
	// Nur von readFrag verwendet
	const int startn = n;
	const int parent = ctx.parser.at(n).parent;
	n++;
	while( n < ctx.parser.count() && ctx.parser.at(n).id == Html_unknown && ctx.parser.at(n).tag.isEmpty() )
	{
		// qDebug() << "consumeFollowers" << startn << n << "?" << quoteNewline( ctx.parser.at(n).text );

		// hält nicht Q_ASSERT( parent == ctx.parser.at(n).parent );

		// Setzte Format sofort zurück, sobald Scope des Formats endet
		if( fidx != -1 && parent != ctx.parser.at(n).parent )
			format.set( fidx, false );
		const QString str = strip( ctx.parser.at(n).text, ctx.d_skipWs );
		if( !str.isEmpty() )
		{
			ctx.bml.startFrame( NameTag( "frag" ) );
			writeFormat( ctx, format );
			ctx.bml.writeSlot( DataCell().setString( str ) ); 
			ctx.bml.endFrame();
			ctx.d_skipWs = false;
		}
		n++;
	}
}

static bool readFrag( Context& ctx, int nodeNr, std::bitset<8>& format, int level )
{
	// nur von readFrags verwendet
	const TextHtmlParserNode& n = ctx.parser.at( nodeNr );
	// qDebug() << "readFrag" << level << nodeNr << n.id << n.tag << quoteNewline( n.text );
	if( n.id == Html_img )
	{
		readImg( ctx, n );
		ctx.d_skipWs = false;
	}else if( n.id == Html_br )
	{
		// TODO: setzt <br> das Format zurück?
		ctx.bml.startFrame( NameTag( "frag" ) );
		writeFormat( ctx, format );
		ctx.bml.writeSlot( DataCell().setString( QString( QChar::LineSeparator ) ) ); // Soft Break
		ctx.bml.endFrame();
		ctx.d_skipWs = true;
	}else if( n.id == Html_a && !n.charFormat.anchorHref().isEmpty() )
	{
		ctx.bml.startFrame( NameTag( "anch" ) );
		ctx.bml.writeSlot( DataCell().setUrl( n.charFormat.anchorHref().toAscii() ), NameTag( "url" ) );
		ctx.bml.writeSlot( DataCell().setString( n.text.simplified() ), NameTag( "text" ) );
		ctx.bml.endFrame();
		ctx.d_skipWs = false;
	}else if( n.id == Html_unknown )
	{
		// Ignore
	}else
	{
		const int fidx = f2i( n.id );
		if( fidx != -1 )
			format.set( fidx );

		const QString str = strip(n.text, ctx.d_skipWs );
		if( !str.isEmpty() )
		{
			ctx.bml.startFrame( NameTag( "frag" ) );
			writeFormat( ctx, format );
			ctx.bml.writeSlot( DataCell().setString( str ) );
			ctx.bml.endFrame();
			ctx.d_skipWs = false;
		}

		if( fidx != -1 )
			format.set( fidx, false );
	}
	for( int i = 0; i < n.children.size(); i++ )
	{
		readFrag( ctx, n.children[i], format, level + 1 );
	}
	consumeFollowers( ctx, nodeNr, format );
	return false;
}

static void readFrags( Context& ctx, const Txt::TextHtmlParserNode& p )
{
	// wird nur von readParagraph verwendet

	// qDebug() << "*** readFrags" << p.id << p.tag << quoteNewline( p.text );
	ctx.d_skipWs = true;
	const QString str = strip( p.text, true );
	if( !str.isEmpty() )
	{
		ctx.bml.startFrame( NameTag( "frag" ) );
		ctx.bml.writeSlot( DataCell().setString( str ) );
		ctx.bml.endFrame(); // frag
		ctx.d_skipWs = false;
	}

	std::bitset<8> format;
	for( int i = 0; i < p.children.size(); i++ )
	{
		// Starte Neues frag für je imp und alle übrigen
		// img hat nie Children
		// a hat selten Children. Falls dann sup oder sub.
		readFrag( ctx, p.children[i], format, 0 );
	}
}

static QString collectText( const Txt::TextHtmlParser& parser, const Txt::TextHtmlParserNode& p )
{
	// Hier verwenden wir normales QString::simplified(), da Formatierung ignoriert wird.
	QString text;
	if( p.id == Html_img )
		text += "<img> ";
	text += p.text.simplified().trimmed();
	for( int i = 0; i < p.children.size(); i++ )
	{
		const QString text2 = collectText( parser, parser.at( p.children[i] ) );
		if( !text.isEmpty() && !text2.isEmpty() )
			text += QChar(' ');
		text += text2;
		// Read followers:
		int n = p.children[i] + 1;
		while( n < parser.count() && parser.at(n).id == Html_unknown && parser.at(n).tag.isEmpty() )
		{
			const QString text3 = parser.at(n).text.simplified().trimmed();
			if( !text.isEmpty() && !text3.isEmpty() )
				text += QChar(' ');
			text += text3;
			n++;
		}
	}
	return text;
}

static QString coded( QString str )
{
	str.replace( QChar('&'), "&amp;" );
	str.replace( QChar('>'), "&gt;" );
	str.replace( QChar('<'), "&lt;" );
	return str;
}

static QString generateHtml( Context& ctx, const Txt::TextHtmlParserNode& p )
{
	const QStringList blocked = QStringList() << "width" << "height" << "lang" << "class" << "size" << 
		"style" << "align" << "valign";

	QString html;
	if( p.id != Html_unknown && p.id != Html_font )
	{
		// qDebug() << p.tag << p.attributes; // TEST
		html += QString( "<%1" ).arg( p.tag );
		for( int i = 0; i < p.attributes.size() / 2; i++ )
		{
			const QString name = p.attributes[ 2 * i ].toLower();
			if( !blocked.contains( name ) )
				html += QString( " %1=\"%2\"" ).arg( name ).arg( p.attributes[ 2 * i + 1 ] );
		}
		html += ">";
	}
	html += coded( p.text ); 
	for( int i = 0; i < p.children.size(); i++ )
	{
		html += generateHtml( ctx, ctx.parser.at( p.children[i] ) );
		int n = p.children[i] + 1;
		while( n < ctx.parser.count() && ctx.parser.at(n).id == Html_unknown && ctx.parser.at(n).tag.isEmpty() )
		{
			html += coded( ctx.parser.at(n).text ); 
			n++;
		}
	}
	if( p.id != Html_unknown && p.id != Html_font )
	{
		html += QString( "</%1>" ).arg( p.tag );
	}
	return html;
}

static void readParagraph( Context& ctx, const Txt::TextHtmlParserNode& p )
{
	// wird nur von createParagraphObj verwendet

	ctx.bml.setDevice();

	ctx.bml.startFrame( NameTag( "rtxt" ) );
	ctx.bml.writeSlot( DataCell().setAscii( "0.1" ), NameTag( "ver" ) );
	ctx.bml.startFrame( NameTag( "par" ) );

	readFrags( ctx, p );

	ctx.bml.endFrame(); // par
	ctx.bml.endFrame(); // rtxt
	Stream::DataCell v = ctx.bml.getBml();
	OutlineItem::updateBackRefs( ctx.parent.top(), v );
	ctx.parent.top().setValue( OutlineItem::AttrText, v );
}

static int h2i( Txt::TextHTMLElements f )
{
	switch( f )
	{
	case Html_h1:
		return 1;
	case Html_h2:
		return 2;
	case Html_h3:
		return 3;
	case Html_h4:
		return 4;
	case Html_h5:
		return 5;
	case Html_h6:
		return 6;
	default:
		return 0;
	}
}

static void dump( const Txt::TextHtmlParser& parser, const Txt::TextHtmlParserNode& p, int level )
{
	QByteArray indent( level, '\t' );
	QByteArray str = p.text.toLatin1();
	qDebug( "%s %s %d    \"%s\"", indent.data(), p.tag.toLatin1().data(), p.id, str.data() );
	for( int i = 0; i < p.children.count(); i++ )
		dump( parser, parser.at( p.children[i] ), level + 1 );
}

static void createParagraphObj( Context& ctx, const Txt::TextHtmlParserNode& p )
{
	if( !collectText( ctx.parser, p ).isEmpty() ) // RISK: teuer
	{
		// wie paragraph
		Udb::Obj o = ctx.createObject();
		o.aggregateTo( ctx.parent.top() );
		ctx.parent.push( o );
		readParagraph( ctx, p );
		ctx.parent.pop();
	}
}

static void createHtmlObj( Context& ctx, const Txt::TextHtmlParserNode& p )
{
	Udb::Obj o = ctx.createObject();
	o.aggregateTo( ctx.parent.top() );
	ctx.parent.push( o );
	DataCell v;
	v.setHtml( generateHtml( ctx, p ) );
	OutlineItem::updateBackRefs( ctx.parent.top(), v );
	ctx.parent.top().setValue( OutlineItem::AttrText, v );
	ctx.parent.pop();
}

static bool isStructure( const Txt::TextHtmlParserNode& p )
{
	switch( p.id )
	{
	case Html_div:
	case Html_p:
	case Html_ul:
	case Html_ol:
	case Html_table:
	case Html_dl:
	case Html_pre:
	case Html_h1:
	case Html_h2:
	case Html_h3:
	case Html_h4:
	case Html_h5:
	case Html_h6:
	case Html_dt:
	case Html_dd:
		return true;
	default:
		return false;
	}
}

static QString findAttr( const QStringList& attrs, const QString& attr )
{
	// p.attributes ist eine Liste von namen und Werten, also immer ganzzahlig
	for( int i = 0; i < attrs.size(); i += 2 )
	{
		if( attrs[i].compare( attr, Qt::CaseInsensitive ) == 0 )
			return attrs[i+1];
	}
	return QString();
}

static void handleChild( Context& ctx, const Txt::TextHtmlParserNode& p );

static void traverseChildren( Context& ctx, const Txt::TextHtmlParserNode& p )
{
	for( int i = 0; i < p.children.count(); i++ )
	{
		handleChild( ctx, ctx.parser.at( p.children[i] ) );
		int n = p.children[i] + 1;
		while( n < ctx.parser.count() && ctx.parser.at(n).id == Html_unknown && ctx.parser.at(n).tag.isEmpty() )
		{
			createParagraphObj( ctx, ctx.parser.at(n) );
			n++;
		}
	}
}

static void handleChild( Context& ctx, const Txt::TextHtmlParserNode& p )
{
	switch( p.id )
	{
	case Html_html:
	case Html_body:
	default:
		traverseChildren( ctx, p );
		break;
	case Html_div:
		{
			bool hasChildStructure = false;
			for( int i = 0; i < p.children.count(); i++ )
			{
				if( isStructure( ctx.parser.at( p.children[i] ) ) )
				{
					hasChildStructure = true;
					break;
				}
			}
			if( hasChildStructure )
				// wie body
				traverseChildren( ctx, p );
			else
				createParagraphObj( ctx, p );
		}
		break;
	case Html_a:
	case Html_span:
		// das kann vorkommen
		createParagraphObj( ctx, p );
		break;
	case Html_p:
	case Html_dt:
	case Html_dd:
	case Html_li:
		createParagraphObj( ctx, p );
		break;
	case Html_ul:
	case Html_ol:
		// p.textListNumberPrefix wird nicht befüllt
		if( findAttr( p.attributes, QLatin1String("type") ).isEmpty() )
		{
			// wie body
			Q_ASSERT( !ctx.parent.isEmpty() );
			Udb::Obj newParent = ctx.parent.top().getLastObj();
			if( !newParent.isNull() )
				ctx.parent.push(newParent);
			traverseChildren( ctx, p );
			if( !newParent.isNull() )
				ctx.parent.pop();
		}else
			// wie table
			createHtmlObj( ctx, p );
		break;
	case Html_table:
    case Html_pre: 
		createHtmlObj( ctx, p );
		break;
	case Html_h1:
	case Html_h2:
	case Html_h3:
	case Html_h4:
	case Html_h5:
	case Html_h6:
		{
			const QString str = collectText( ctx.parser, p );
			if( !str.isEmpty() )
			{
				const int level = h2i( p.id );
				while( level <= ctx.trace.top() && ctx.trace.size() > 1 )
				{
					ctx.trace.pop();
					ctx.parent.pop();
				}
				ctx.trace.push( level );
				Udb::Obj o = ctx.createObject();
				o.aggregateTo( ctx.parent.top() );
				ctx.parent.push( o );
				ctx.parent.top().setValue( OutlineItem::AttrIsTitle, DataCell().setBool( true ) );
				// updateBackRefs hier unnötig, da neu erzeugt und nur String
				ctx.parent.top().setValue( OutlineItem::AttrText, DataCell().setString( str ) );
			}
		}
		break;
	case Html_address:
	case Html_hr:
		// ignored
		break;
	case Html_tr:
	case Html_td:
	case Html_th:
		// nie separat, immer als teil von table als HTML übernommen
		break;
	case Html_thead:
	case Html_tbody:
	case Html_tfoot:
	case Html_caption:
		// deprecated
		break;
	case Html_title:
		// ignore
		break;
	}
}

static void findName( Context& ctx )
{
	for( int i = 0; i < ctx.parser.count(); i++ )
	{
		if( ctx.parser.at(i).id == Html_title )
		{
			const QString str = ctx.parser.at(i).text.simplified().trimmed();
			if( !str.isEmpty() )
			{
				// updateBackRefs hier unnötig, da neu erzeugt und nur string
				ctx.oln.setValue( OutlineItem::AttrText, DataCell().setString( str ) );
				return;
			}
		}
	}
}

Udb::Obj HtmlToOutline::parse( const QString &html, Udb::Transaction * txn, DataCell::OID home )
{
	d_error.clear();

	//qDebug() << html; // TEST
	// In Word sind die Bilder teils in deren Vector-ML eingebettet, wodurch hier nicht geparst.

	Context ctx;
	ctx.dir = d_context;
	ctx.parser.parse( html, 0 );
	if( ctx.parser.count() == 0 )
	{
		d_error = "HTML stream has no contents!";
		return Udb::Obj();
	}
	// qDebug() << html;
	// ctx.parser.dumpHtml(); // TEST
	try
	{
		if( home )
		{
			// Erzeuge ein Toplevel-Item
			ctx.oln = txn->createObject( OutlineItem::TID );
			ctx.oln.setValue( OutlineItem::AttrIsTitle, DataCell().setBool(true) );
			ctx.oln.setValue( OutlineItem::AttrHome, DataCell().setOid( home ) );
			ctx.home = home;
		}else
		{
			// Erzeuge ein Outline
			ctx.oln = txn->createObject();
			ctx.home = ctx.oln.getOid();
		}
		ctx.oln.setValue( OutlineItem::AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		findName( ctx );

		const Txt::TextHtmlParserNode& n = ctx.parser.at(0);
		ctx.parent.push( ctx.oln );
		ctx.trace.push( 0 );
		handleChild( ctx, n );

		Udb::Obj h = txn->getObject( ctx.home );
		Outline::markHasItems(h);
	}catch( std::exception& e )
	{
		d_error += e.what();
		return Udb::Obj();
	}
	return ctx.oln;
}
