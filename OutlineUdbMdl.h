#ifndef __Oln_OutlineUdbMdl__
#define __Oln_OutlineUdbMdl__

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

#include <Oln2/OutlineMdl.h>
#include <QPixmap>
#include <Udb/Obj.h>

namespace Oln
{
	class OutlineUdbMdl : public OutlineMdl
	{
		Q_OBJECT
	public:
		static const char* s_mimeOutline;

		OutlineUdbMdl( QObject* );

		void setOutline( const Udb::Obj& );
		const Udb::Obj& getOutline() const { return d_outline; }
		void fetchLevel( const QModelIndex & parent, bool all = false );
		void fetchAll();
        void collapseAll();
		Udb::Obj getItem( const QModelIndex & ) const;
        Udb::Obj getItem( int row, const QModelIndex & parent ) const;
		// QModelIndex findIndex( quint64 oid ); // lädt nach
        QModelIndex getIndex( quint64, bool fetch = false ) const; // override
		QModelIndex findInLevel( const QModelIndex & parent, quint64 oid ) const;
		QList<Udb::Obj> loadFromText( const QString& html, int row, const QModelIndex & parent ); // neue Items oder empty

        Udb::Obj loadFromHtml( const QString& html, // TODO: hier QByteArray übergeben und zuerst Zeichenkodierung feststellen!
                               int row, const QModelIndex & parent, bool rooted = true ); // neues Item oder null
		bool moveOrLinkRefs( const QByteArray& bml, const Udb::Obj& parent, const Udb::Obj& before, bool link, bool docLink = false );
		
		enum Action { MoveItems, LinkItems, LinkDocs, CopyItems };
		bool dropBml( const QByteArray& bml, const QModelIndex& parent, int row, Action );
		
		bool isReadOnly() const; // override

		static QPixmap getPixmap( quint32 typeId );
		static QString getPixmapPath( quint32 typeId );
        static QString getTypeCode( quint32 typeId );
        static quint32 getTypeFromCode( const QString& typeCode );
        static void registerPixmap( quint32 typeId, const QString& path, const QString& typeCode =  QString() );

		static void writeObjectUrls(QMimeData *data, const QList<Udb::Obj>& );
		static QUrl objToUrl(const Udb::Obj & o);

		// Overrides
		bool canFetchMore ( const QModelIndex & parent ) const;
		void fetchMore ( const QModelIndex & parent );
		bool hasChildren( const QModelIndex & parent = QModelIndex() ) const;	
		bool setData ( const QModelIndex & index, const QVariant & value, int role );
		QMimeData * mimeData ( const QModelIndexList & indexes ) const;
		QStringList mimeTypes () const;
		bool dropMimeData ( const QMimeData *, Qt::DropAction, int row, int column, const QModelIndex & );
	protected slots:
		void onDbUpdate( Udb::UpdateInfo );
    protected:
		Udb::Obj d_outline;
		class UdbSlot : public Slot
		{
		public:
			Udb::Obj d_item;

			UdbSlot* getSuper() const { return static_cast<UdbSlot*>( Slot::getSuper() ); }
			virtual quint64 getId() const;
			virtual bool isTitle(const OutlineMdl*) const;
			virtual bool isExpanded(const OutlineMdl*) const;
			virtual bool isReadOnly(const OutlineMdl*) const;
			virtual bool isAlias(const OutlineMdl*) const;
			virtual QVariant getData(const OutlineMdl*,int role) const;
		};
		UdbSlot* getSlot( const QModelIndex& index ) const;
		UdbSlot* findSlot( quint64 id ) const;
	protected:
		typedef QList<Udb::Obj> ObjList;
		int fetch( UdbSlot*, int max = 20, ObjList* = 0 ) const; // max=0..all
		void create( UdbSlot*, const ObjList& );
        void collapse(UdbSlot*);
        bool d_blocked;
	};
}

#endif
