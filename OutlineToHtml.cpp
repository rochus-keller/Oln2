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

#include "OutlineToHtml.h"
#include "OutlineItem.h"
#include <Txt/TextOutHtml.h>
#include <Txt/TextInStream.h>
#include <Txt/TextOutStream.h>
#include <Txt/TextHtmlImporter.h>
#include <Udb/Database.h>
#include "OutlineUdbCtrl.h"
#include "LinkSupport.h"
using namespace Oln;

OutlineToHtml::OutlineToHtml(bool useItemIds, bool noFileDirs):d_useItemIDs(useItemIds),d_noFileDirs(noFileDirs)
{
}

void OutlineToHtml::writeTo( QTextStream& out, const Udb::Obj& oln, QString title, bool fragment )
{
    if( oln.isNull() )
        return;
	d_oln = oln;
	out.setCodec( "UTF-8" );
	if( !fragment )
	{
		out << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">" << endl;
		out << "<html><META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">" << endl;
		if( title.isEmpty() )
			title = Qt::escape( oln.getString( OutlineItem::AttrText ) );
		out << QString( "<head><title>%1</title>" ).arg( title ) << endl;
		writeCss(out);
		out << "</head><body>" << endl;
	}

	Udb::Obj sub = oln.getFirstObj();
	int n = 1;
	if( !sub.isNull() ) do
	{
		if( sub.getType() == OutlineItem::TID )
			descend( out, sub, QString::number( n++ ), 0 );
	}while( sub.next() );

	if( !fragment )
		out << "</body></html>";
}

void OutlineToHtml::writeCss(QTextStream &out)
{
	// See
	// http://alistapart.com/article/building-books-with-css3
	// http://www.smashingmagazine.com/2011/11/how-to-set-up-a-print-style-sheet/
	// http://www.smashingmagazine.com/2015/01/designing-for-print-with-css/

	out << QString("<style type=\"text/css\" media=\"screen, projection, print\">\n"
				   "body{font-family:Arial,sans-serif;font-size:small;orphans:2;widows:2;}\n"
				   "h1,h2,h3,h4{background-color:#F5F5F5;padding:0.3em;page-break-after:avoid;margin:0;"
					   "border-style:solid;border-color:white #F5F5F5 #F5F5F5 #F5F5F5;border-width:thin;}\n"
				   "span.label{position:absolute;top:auto;left:0;margin-top:0.2em;"
					   "font-size:smaller;color:darkblue; }\n"
				   "span.ident{background-color:aliceblue;font-weight:bold;padding:0 0.2em 0 0.2em;}\n"
				   "a[href^=\"#\"]{text-decoration:none;font-weight:normal;" // alle internen Links
					   "color:darkblue;background-color:#F5F5F5;}\n"
				   "div{border-style:none solid solid solid;border-width:thin;"
					   "border-color:#F5F5F5;margin:0;padding:0.3em;}\n"
				   "ol,ul{margin:0;}\n"
				   // "@page{ @bottom-left { content: counter(page); } }\n"
				   "</style>" ) << endl;
}

void OutlineToHtml::writeFragment( QTextStream& out, const Stream::DataCell& txt, QString id, Udb::OID alias )
{
	const QString pfeil = "&#x21B3;"; // 0x21E7
	QTextDocument doc;
	if( txt.isHtml() )
	{
		Txt::TextHtmlImporter imp( &doc, txt.getStr() );
		Oln::OutlineUdbCtrl::LinkRenderer lr( d_oln.getTxn() );
		imp.setLinkRenderer( &lr );
		imp.import();
	}else if( txt.isBml() )
	{
		Txt::TextCursor cur( &doc );
		Stream::DataReader in( txt );
		Oln::OutlineUdbCtrl::LinkRenderer lr( d_oln.getTxn() );
        Txt::TextInStream s( 0, &lr );
		s.readFromTo( in, cur );
	}else
		doc.setPlainText( txt.toString() );
	Txt::TextOutHtml html( &doc, false );
	class LinkRenderer : public Txt::LinkRendererInterface
	{
	public:
		LinkRenderer( const Udb::Obj& o ):d_oln(o) {}
		QString renderHref( const QByteArray& link ) const
		{
			Link l;
			if( l.readFrom( link ) )
			{
				if( l.d_db == d_oln.getDb()->getDbUuid() )
				{
					OutlineItem item = d_oln.getObject( l.d_oid );
					if( item.getType() == OutlineItem::TID && item.getHome() == d_oln )
					{
						return QString("#%1").arg( l.d_oid );
					}
					return Udb::Obj::objToUrl( item ).toEncoded();
				}
				return Udb::Obj::oidToUrl(l.d_oid, l.d_db ).toEncoded();
			}
			return Txt::LinkRendererInterface::renderHref(link);
		}
	private:
		Udb::Obj d_oln;
	};
	LinkRenderer lr( d_oln );
	html.setLinkRenderer( &lr );
	html.setNoFileDirs(d_noFileDirs);
	if( !id.isEmpty() )
	{
		id = QString("<span class=\"ident\">%1</span>").arg( Qt::escape(id) );
	}
	if( alias != 0 )
		out << QString("<a href=\"#%1\">%2</a>").arg( alias ).arg( pfeil ) << id << html.toHtml( true );
	else
		out << id << html.toHtml( true );
}

void OutlineToHtml::writeParagraph( QTextStream& out, const Stream::DataCell& txt, const QString &id,
							bool isTitle, const QString &label, int level, Udb::OID alias, const QString &anchor )
{
	QString tag;
	if( isTitle )
		tag = "h4";
	else
		tag = "div";
	// NOTE: hatte zuerst <p> statt <div> verwendet; sieht genau gleichaus, ausser dass in <p> keine Tabellen
	// oder Listen eingebettet werden können und diese dann ganz links aliniert sind.
	out << QString( "<%1 style=\"margin-left:%2px\">" ).arg(tag).arg( 9 + level * 18 );

	out << "<span class=\"label\">";
	if( !anchor.isEmpty() )
        out << QString( "<a name=\"%1\">%2</a>" ).arg( anchor ).arg( label );
    else
    	out << label;
	out << "</span>";

	writeFragment( out, txt, id, alias );
	if( isTitle )
		out << "</h4>";
	else
		out << "</div>";
	out << endl;
}

QString OutlineToHtml::anchor( const Udb::Obj& o )
{
    return QString::number( o.getOid() );
}

static QString _getId( const Udb::Obj& o )
{
	if( o.isNull() )
		return QString();
	QString id = o.getString( OutlineItem::AttrAltIdent, true );
	if( id.isEmpty() )
		id = o.getString( OutlineItem::AttrIdent, true );
	return id;
}

void OutlineToHtml::descend( QTextStream& out, const Udb::Obj &item, const QString &label, int level )
{
	Stream::DataCell v;
	const bool isTitle = item.getValue( OutlineItem::AttrIsTitle ).getBool() ;
	v = item.getValue( OutlineItem::AttrAlias );
	bool writeText = true;
	// Wenn das Item ein Alias ist, schreiben wir vorsorglich den referenzierten Text auch raus.
	if( v.hasValue() )
	{
		Udb::Obj o = item.getObject( v.getOid() );
		if( !o.isNull() )
		{
			v = o.getValue( OutlineItem::AttrText );
			if( v.hasValue() )
			{
				QString id;
				if( d_useItemIDs )
					id = _getId( o );
				writeParagraph( out, v, id, isTitle, label, level, o.getOid(), anchor( item ) );
				// Der Anchor zeigt auf das Alias und nicht das dereferenzierte Objekt
				writeText = false;
			}
		}
	}
	if( writeText )
	{
		v = item.getValue( OutlineItem::AttrText );
		if( v.hasValue() )
		{
			QString id;
			if( d_useItemIDs )
				id = _getId( item );
			writeParagraph( out, v, id, isTitle, label, level, 0, anchor( item ) );
		}
	}
	Udb::Obj sub = item.getFirstObj();
	int n = 1;
	if( !sub.isNull() ) do
	{
		if( sub.getType() == OutlineItem::TID )
			descend( out, sub, QString( "%1.%2" ).arg( label ).arg( n++ ), level + 1 );
	}while( sub.next() );
}
