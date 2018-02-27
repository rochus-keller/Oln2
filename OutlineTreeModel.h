#ifndef _DBGUI_OutlineTreeModel
#define _DBGUI_OutlineTreeModel

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

#include <Root/Resource.h>
#include <Oln/OutlineMdl.h>
#include <QAbstractItemModel>

namespace Oln
{
	class OutlineTreeModel : public QAbstractItemModel
	{
		Q_OBJECT
	public:
		enum { ExpandedRole = Qt::UserRole + 1, TitleRole, LevelRole, AtomRole };

		OutlineTreeModel( QObject* );
		~OutlineTreeModel();

		OutlineItem* getRoot() const { return d_mdl.getRoot(); }
		OutlineItem* getItem( const QModelIndex & index ) const;
		void clear();
		bool loadFrom( Root::ValueReader& );
		bool saveTo( Root::ValueWriter& ) const;
		bool saveTo( Stream::DataWriter& ) const;
		Root::AtomManager* getAtoms() const { return d_mdl.getAtoms(); }

		QModelIndex createChild( QModelIndex parent, int row = -1, bool isBody = true );
		bool removeChild( QModelIndex parent, int row );
		bool move( QModelIndex source, QModelIndex toParent, int row = -1 );
		QMimeData* createMimeData(const QModelIndexList &indexes, bool pointer ) const;

		// Overrides
		// GENERIC_MESSENGER( OutlineTreeModel );
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent ( const QModelIndex & index ) const;
		QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
		bool setData ( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole );
		QStringList mimeTypes() const;
		QMimeData* mimeData(const QModelIndexList &indexes) const;
		bool dropMimeData(const QMimeData *data, Qt::DropAction action, 
			int row, int column, const QModelIndex &parent);
		Qt::DropActions supportedDropActions() const;
		Qt::DropActions supportedDragActions() const { return Qt::MoveAction; }
	signals:
		void onDirty();
	protected:
		bool dropOutlineItem(const QMimeData *data, Qt::DropAction action, 
			int row, int column, QModelIndex parent);
		bool dropOutlinePtr(const QMimeData *data, Qt::DropAction action, 
			int row, int column, QModelIndex parent);
		bool dropText(const QMimeData *data, Qt::DropAction action, 
			int row, int column, const QModelIndex &parent);
	private:
		OutlineMdl d_mdl;
	};
}

#endif // _DBGUI_OutlineTreeModel2
