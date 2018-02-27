#ifndef __Oln_OutlineMdl__
#define __Oln_OutlineMdl__

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

#include <QAbstractItemModel>
#include <QList>
#include <QTextDocument>
#include <QMap>
#include <QSet>

namespace Oln
{
	class OutlineMdl : public QAbstractItemModel
	{
	public:
		enum Role { 
			OidRole = Qt::UserRole + 1,
			LevelRole, 
			TitleRole,
			NumberRole,
			ExpandedRole,
			ReadOnlyRole,
			AliasRole,
			IdentRole
		};

		struct Html { Html( const QString& html = QString() ):d_html(html){} QString d_html; };
		struct Bml { Bml( const QByteArray& bml = QByteArray() ):d_bml(bml){} QByteArray d_bml; };

		OutlineMdl(QObject *parent);
		~OutlineMdl();

		// Helper
		void clear();
		void clearCache( const QModelIndex & parent ); // entferne alle Childs aus dem Arbeitsspeicher
		QModelIndex getFirstIndex() const;
		quint64 getId( const QModelIndex & ) const;

		// Interface
		virtual bool isReadOnly() const { return false; } // identisch mit data( QModelIndex(), ReadOnlyRole );
        virtual QModelIndex getIndex( quint64, bool fetch = false ) const;

		// Overrides
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
        int columnCount(const QModelIndex & = QModelIndex() ) const { return 1; }
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
		Qt::DropActions supportedDropActions () const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
	protected:
		class Slot
		{
			QList<Slot*> d_subs;
			Slot* d_super;
			friend class OutlineMdl;
		protected:
			virtual ~Slot();
		public:
			const QList<Slot*>& getSubs() const { return d_subs; }
			Slot* getSuper() const { return d_super; }
			int getLevel() const;
			Slot():d_super(0) {}

			virtual quint64 getId() const { return 0; }
			virtual bool isTitle(const OutlineMdl*) const { return false; }
			virtual bool isExpanded(const OutlineMdl*) const { return false; }
			virtual bool isReadOnly(const OutlineMdl*) const { return true; }
			virtual bool isAlias(const OutlineMdl*) const { return false; }
            virtual QVariant getData(const OutlineMdl*,int) const { return QVariant(); }
		};
		Slot* getSlot( const QModelIndex& index ) const;
		void add( Slot* s, Slot* to, int before = -1 );
		void remove( Slot* );
		Slot* findSlot( quint64 id ) const;
		Slot* getRoot() const { return d_root; }
	private:
		Slot* d_root;
		typedef QMap<quint32,Slot*> Cache;
		Cache d_cache;
		void recursiveRemove( Slot* s );
	};
}
Q_DECLARE_METATYPE( Oln::OutlineMdl::Html ) 
Q_DECLARE_METATYPE( Oln::OutlineMdl::Bml ) 

#endif // __Oln_OutlineMdl__
