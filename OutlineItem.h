#ifndef OUTLINEITEM_H
#define OUTLINEITEM_H

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

#include <Udb/ContentObject.h>
#include <QVariant>

namespace Oln
{
    class OutlineItem : public Udb::ContentObject
    {
    public:
		static quint32 TID;
		static quint32 AttrIsExpanded; // bool: offen oder geschlossen
		static quint32 AttrIsTitle;    // bool: stellt das Item einen Title dar oder einen Text
		static quint32 AttrIsReadOnly; // bool: ist Item ein Fixtext oder kann es bearbeitet werden
		static quint32 AttrHome;       // OID: Referenz auf Root des Outlines von beliebigem Typ.
		static quint32 AttrAlias;       // OID: optionale Referenz auf irgend ein Object, dessen Body statt des eigenen angezeigt wird.
		static const char* AliasIndex;

        OutlineItem(const Udb::Obj& o ):ContentObject(o){}
		OutlineItem() {}

		static OutlineItem createItem(Obj &parent, const Obj& before = Obj() );
		OutlineItem createSubItem(const Obj& before = Obj() );
        void setExpanded( bool on = true );
        bool isExpanded() const;
        void setTitle( bool on = true );
        bool isTitle() const;
		QString toTextHtml() const;
		QString getAltOrIdent() const;
		void setReadOnly(bool on = true);
        bool isReadOnly() const;
        void setHome( const Udb::Obj& );
        Udb::Obj getHome() const;
        void setAlias( const Udb::Obj& );
        Udb::Obj getAlias() const;
		Udb::Obj getPrevItem() const;
		Udb::Obj getNextItem() const;
		void erase() { ContentObject::erase();}

		static QString getParagraphNumber( const Udb::Obj& );
		static void updateBackRefs( const OutlineItem& item, const Stream::DataCell& newText );
		static void updateBackRefs( const OutlineItem& item );
		static void updateAllRefs(Udb::Transaction *txn);
		static QList<OutlineItem> getReferences( const Udb::Obj& obj ); // returns list of all outline items referencing obj
		static void erase( Obj );
		static void itemErasedCallback( Udb::Transaction*, const Udb::UpdateInfo& );
		static void doBackRef( bool = true );
	};

    class Outline : public Udb::ContentObject
    {
    public:
		static quint32 TID;
		static quint32 AttrHasItems; // bool: true, solange mindestens ein Top-level-item vorhanden ist
        Outline(const Udb::Obj& o ):ContentObject(o){}
		Outline() {}
		static void markHasItems( Udb::Obj& );
		static bool hasItems( const Udb::Obj& );
    };
}

#endif // OUTLINEITEM_H
