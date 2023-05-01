#ifndef _Oln_OutlineTree
#define _Oln_OutlineTree

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

#include <QTreeView>

namespace Oln
{
	class OutlineTree : public QTreeView
	{
		Q_OBJECT
	public:
		OutlineTree( QWidget* );
		~OutlineTree();
		void setShowNumbers( bool on ) { d_showNumbers = on; }
		void setStepSize( quint8 ); // Für vertikalen Scrollbar
		int stepSize() const { return d_stepSize; }
		void setHandleWidth( quint8 ); // 11 default. 9 and 7 work fine
		void ensureVisibleInCurrent( int yOffset, int h ); // yOffset..Position innerhalb currentIndex 
		void edit( const QModelIndex &index ) { if( startEdit( index, 0 ) ) QTreeView::edit( index ); }
		void goAndEdit( const QModelIndex& );
		void closeEdit();
		void setDragEnabled( bool on ) { d_dragEnabled = on; } // Verdeckt Methode der Oberklasse
		// Overrides
		void setModel ( QAbstractItemModel * model );
	signals:
		void returnPressed();
		void identDoubleClicked();
		void identClicked();
	protected:
		int viewIndex(const QModelIndex &index) const;
		void restoreExpanded( const QModelIndex &index, int start = 0, int end = -1 );
		bool startEdit(const QModelIndex &index, QEvent *event);
		// Overrides
		void keyPressEvent(QKeyEvent *event);
		bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event);
		void drawBranches ( QPainter * painter, const QRect & rect, const QModelIndex & index ) const;
        int indexRowSizeHint(const QModelIndex &index) const;
		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mouseDoubleClickEvent(QMouseEvent *event);
		void drawRow ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		void updateGeometries();
		void currentChanged ( const QModelIndex & current, const QModelIndex & previous ) ;
		void rowsInserted ( const QModelIndex & parent, int start, int end );
		void verticalScrollbarValueChanged( int );
	protected slots:
		void onCollapsed ( const QModelIndex & index );
		void onExpanded ( const QModelIndex & index );
	private:
		mutable bool d_block;
		mutable bool d_block2;
		bool d_showNumbers;
		bool d_dragEnabled;
		quint8 d_stepSize;
        quint8 d_indicatorAreaWidth; // the part of the handle where the indicator is displayed
		bool d_clickInEditor;
        bool d_dragging;
        bool d_hitExpander;
		QPoint d_lastPressPoint;
		mutable int lastViewedItem;
		Q_DECLARE_PRIVATE(::QTreeView)
	};
}

#endif // _Oln_OutlineTree
