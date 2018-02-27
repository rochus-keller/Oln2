#ifndef _DbGui_OutlineTreeDeleg
#define _DbGui_OutlineTreeDeleg

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

#include <Text2/AtomManager.h>
#include <Text2/ParagraphCtrl.h>
#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QPersistentModelIndex>

namespace Oln
{
	class OutlineTreeDeleg : public QAbstractItemDelegate
	{
	public:
		OutlineTreeDeleg( QAbstractItemView* parent, Text::ParagraphCtrl* = 0 );
		~OutlineTreeDeleg();
		QAbstractItemView* view() const { return static_cast<QAbstractItemView*>( parent() ); }

		Text::ParagraphCtrl* getCtrl() const { return d_ctrl; }
		void writeData() const;
		void closeEdit() const;
		void readData() const;
		bool isEditing() const { return d_edit.isValid(); }

		//* Overrides Object
		bool eventFilter( QObject * watched, QEvent * event );
		GENERIC_MESSENGER( OutlineTreeDeleg );
		//* Overrides von QAbstractItemDelegate
		QWidget * createEditor ( QWidget * parent, const QStyleOptionViewItem & option, 
			const QModelIndex & index ) const;
		bool editorEvent(QEvent *event, QAbstractItemModel *model,
			const QStyleOptionViewItem &option, const QModelIndex &index);
		void paint(QPainter *painter, const QStyleOptionViewItem &option, 
			const QModelIndex &index) const;
		QSize sizeHint( const QStyleOptionViewItem & option, 
			const QModelIndex & index ) const;
		void setModelData( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const;
	protected:
		void handle( Root::Message& );
	private:
		Text::ParagraphCtrl* d_ctrl;
		Root::Ref<Text::ParagraphView> d_temp;
		mutable Root::Ref<Root::AtomManager> d_atoms;
		mutable QPersistentModelIndex d_edit;
	};
}

#endif // _DbGui_OutlineTreeDeleg
