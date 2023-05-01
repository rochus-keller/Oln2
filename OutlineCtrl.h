#ifndef __Oln_OutlineCtrl__
#define __Oln_OutlineCtrl__

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

#include <GuiTools/Controller.h>
#include <GuiTools/AutoMenu.h>
#include <Oln2/OutlineTree.h>
#include <Oln2/OutlineDeleg.h>
#include <Txt/LinkRendererInterface.h>

class QMimeData;

namespace Oln
{
    class OutlineMdl;

    class OutlineCtrl : public Gui::Controller
	{
		Q_OBJECT
	public:
		OutlineCtrl( OutlineTree*, Txt::LinkRendererInterface* = 0 );
		OutlineTree* getTree() const;
		OutlineDeleg* getDeleg() const { return d_deleg; }
		virtual bool copyToClipBoard( bool cut = false );
        virtual bool pasteFromClipBoard(bool special = false);
        virtual bool isFormatSupported( const QMimeData *mime);
        void addTextCommands( Gui::AutoMenu* );
		static QString fetchHtml( const QMimeData * );
		static QString fetchHtml( const QByteArray& );
	signals:
		void sigCurrentChanged( quint64 ); // OID
        void sigUrlActivated( QUrl );
	protected:
		void nextOrSub( QModelIndex& parent, int& newRow );
        void setModel( OutlineMdl* );
        void editUrl();
        void insertUrl();
        bool canFormat();
		// overrides
		bool keyPressEvent(QKeyEvent*);
	public slots:
		void onBold();
		void onUnderline();
		void onItalic();
		void onStrike();
		void onSuper();
		void onSub();
		void onFixed();
		void onCopy();
		void onCut();
		void onPaste();
		void onSave();
		void onTimestamp();
		void onInsertDate();
		void onInsertUrl();
		void onEditUrl();
		void onOpenUrl();
		void onCopyUrl();
        void onStripWhitespace();
        void onFollowAnchor();
	protected slots:
		void onCurrentChanged( const QModelIndex &,const QModelIndex & );
        void followUrl(QByteArray,bool);
	protected:
		OutlineDeleg* d_deleg;
	};
}

#endif
