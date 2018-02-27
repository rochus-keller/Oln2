#ifndef _DbGui_OutlineViewCtrl
#define _DbGui_OutlineViewCtrl

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

#include <Gui/TreeCtrl.h>
#include <QTreeView>
#include <Text2/ValueReader.h>
#include <Stream/DataWriter.h>

namespace Oln
{
	class OutlineTreeModel;
	class OutlineTreeDeleg;

	class OutlineTreeCtrl : public Gui::EventListener // Gui::TreeCtrl
	{
		Q_OBJECT
	public:
		OutlineTreeCtrl(QTreeView*, OutlineTreeDeleg* = 0, bool = true);

		bool loadFrom( Root::ValueReader& );
		bool saveTo( Root::ValueWriter& );
		bool saveTo( Stream::DataWriter& );
		void createEmpty();

		QTreeView* wnd() const { return static_cast<QTreeView*>( parent() ); }
		bool isDirty() const { return d_dirty; }
		Root::AtomManager* getAtoms() const;

		static const char* LocalAddRight;
		static const char* LocalAddNext;
		static const char* LocalAddLeft;
		static const char* Indent;
		static const char* Outdent;
		static const char* MoveUp;
		static const char* MoveDown;
		static const char* Dump;
		static const char* TitleItem;

		IMPLEMENT_REACTOR( OutlineTreeCtrl );
		// IMPLEMENT_REACTOR2( OutlineTreeCtrl, Gui::TreeCtrl );
	public slots:
		void editTop();
		void closeEdit();
		void saveEdit();
	signals:
		void modelChanged();
	protected:
        virtual bool keyPressEvent(QWidget*,QKeyEvent*);
		void checkExpanded( QModelIndex );
		void connectModel();
	protected slots:
		void collapsed( const QModelIndex & index );
		void expanded( const QModelIndex & index );
		void inserted( const QModelIndex & index, int start, int end );
		void onChange();
	private:
		void handleExpand( Root::Action& );
		void handleCollapseAll( Root::Action& );
		void handleExpandSubs( Root::Action & );
		void handleExpandAll( Root::Action& );

		void handleTitleItem( Root::Action& );
		void handleLocalAddLeft( Root::Action& );
		void handleMoveUp( Root::Action& );
		void handleMoveDown( Root::Action& );
		void handlePaste( Root::Action& );
		void handleCopy( Root::Action& );
		void handleCut( Root::Action& );
		void handleRedo( Root::Action& );
		void handleUndo( Root::Action& );
		void handleOutdent( Root::Action& );
		void handleIndent( Root::Action& );
		void handleDump( Root::Action& );
		void handleRecCreate( Root::Action& );
		void handleRecCreateSub( Root::Action& );
		void handleRecDelete( Root::Action& );
	private:
		OutlineTreeModel* d_mdl;
		OutlineTreeDeleg* d_deleg;
		bool d_expandLock;
		bool d_dirty;
	};
}

#endif // _DbGui_OutlineViewCtrl
