#ifndef __Oln_RefByItemMdl__
#define __Oln_RefByItemMdl__

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

#include <QAbstractItemModel>
#include <QWidget>
#include <Udb/Obj.h>
#include <QHash>

class QTreeView;

namespace Oln
{
	class RefByItemMdl : public QAbstractItemModel
	{
		Q_OBJECT
	public:
		static QString (*formatTitle)(const Udb::Obj & o );
		RefByItemMdl( QTreeView* );

		void setObj( const Udb::Obj&, bool focus = false );
		void focusOn( const Udb::Obj& );
		const Udb::Obj& getObj() const { return d_root.d_obj; }
		Udb::Obj getObject( const QModelIndex& ) const;
		QModelIndex getIndex( const Udb::Obj& ) const;
		QTreeView* getTree() const;
		void refill();

		// overrides
		int columnCount ( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent ( const QModelIndex & index ) const;
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
	signals:
		void sigFollowObject(Udb::Obj);
	protected slots:
		void onRefDblClicked( const QModelIndex & );
	private:
		struct Slot
		{
			Udb::Obj d_obj;
			QString d_nr;
			QList<Slot*> d_children;
			Slot* d_parent;
			Slot(Slot* p = 0):d_parent(p){ if( p ) p->d_children.append(this); }
			~Slot() { foreach( Slot* s, d_children ) delete s; }
			void fillSubs();
		};
		void fillSubs( Slot* );
		void recursiveRemove( Slot* s );
		QModelIndex getIndex( Slot* ) const;
		QHash<quint32,Slot*> d_cache;
		Slot d_root;
	};
}

#endif
