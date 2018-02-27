#ifndef __Oln_OutlineUdbCtrl__
#define __Oln_OutlineUdbCtrl__

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

#include <Oln2/OutlineCtrl.h>
#include <Oln2/OutlineUdbMdl.h>
#include <Udb/Transaction.h>
#include <Udb/UpdateInfo.h>
#include <Oln2/LinkSupport.h>

namespace Oln
{
    class OutlineUdbCtrl : public OutlineCtrl
	{
		Q_OBJECT
	public:
        static Link s_objectDefault;
        static Link s_itemDefault;

        class LinkRenderer : public Txt::LinkRendererInterface
        {
        public:
			LinkRenderer( Udb::Transaction* t ):d_txn(t) {}
            bool renderLink( Txt::TextCursor&, const QByteArray& link ) const;
			QString renderHref( const QByteArray& link ) const;
        private:
            Udb::Transaction* d_txn;
        };

		OutlineUdbCtrl( OutlineTree* p, Udb::Transaction* );
		OutlineUdbMdl* getMdl() const { return d_mdl; }
		Udb::Transaction* getTxn() const { return d_txn; }
		void setOutline( const Udb::Obj&, bool setCurrent = false );
		const Udb::Obj& getOutline() const { return d_mdl->getOutline(); }
        //bool pasteAlias(bool docOnly = false);
		bool loadFromHtml( const QString& path );
		bool addItem();
		bool gotoItem( quint64 oid );

		static OutlineUdbCtrl* create( QWidget* p, Udb::Transaction* );

        void addItemCommands( Gui2::AutoMenu* );
        void addOutlineCommands( Gui2::AutoMenu* );

        // overrides
        bool copyToClipBoard( bool cut = false );
        bool pasteFromClipBoard(bool special = false);
        bool isFormatSupported(const QMimeData *mime);
    signals:
        void sigLinkActivated( quint64 ); // OID
	protected:
		virtual Udb::Obj createSub( Udb::Obj& parent, const Udb::Obj& before = Udb::Obj() );
		bool deleteSelection( const QModelIndexList& );
        QList<Udb::Obj> getSelectedItems(bool checkConnected = false) const;
        void selectItems( const QList<Udb::Obj>& items );
		void insertTocImp( const Udb::Obj& item, Udb::Obj& toc, int level );
	public slots:
		void onAddNext();
		void onAddLeft();
		void onAddRight();
		void onRemove();
		void onSetTitle();
		void onEditProps();
		void onIndent();
		void onUnindent();
		void onMoveUp();
		void onMoveDown();
		void onExpandAll();
		void onExpandSubs();
		void onCollapseAll();
		void onReadOnly();
		void onDocReadOnly();
        void onPasteSpecial();
		void onInsertToc();
        //void onPasteDocAlias();
        void onEditUrl();
        void onFollowAlias();
	protected slots:
		void onAddNextImp();
		void onDbUpdate( Udb::UpdateInfo );
        void followLink( const QByteArray&, bool isUrl );
        void followAlias();
	private:
		OutlineUdbMdl* d_mdl;
		Udb::Transaction* d_txn;
        LinkRenderer d_linkRenderer;
	};
}

#endif
