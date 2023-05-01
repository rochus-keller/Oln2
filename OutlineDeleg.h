#ifndef DOCDELEG_H
#define DOCDELEG_H

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

#include <QAbstractItemDelegate>
#include <Txt/TextCtrl.h>
#include <Txt/LinkRendererInterface.h>
#include <QPersistentModelIndex>

class QTextDocument;

namespace Oln
{
	class OutlineTree;

	class OutlineDeleg : public QAbstractItemDelegate
	{
		Q_OBJECT
	public:
		OutlineDeleg(OutlineTree *parent, Txt::Styles* = 0, const Txt::LinkRendererInterface* lr = 0);
		~OutlineDeleg();

		void writeData() const;
		void closeEdit() const;
		void readData() const;
		bool isEditing() const { return d_edit.isValid(); }
		bool isTitle() const { return d_isTitle; }
		bool isHtml() const { return d_editFormat == Html; }
		bool isReadOnly() const;
		void setReadOnly( bool );
		void setBiggerTitle( bool );
		void setShowIcons( bool on ) { d_showIcons = on; }
		QUrl getSelUrl() const; // returned URL is empty if not selected
        QByteArray getSelLink() const; // returned Link is empty if not selected
        void activateAnchor(); // Simuliert Click auf Anchor

		OutlineTree* view() const;
		Txt::TextView* getEditor() const;
		Txt::TextCtrl* getEditCtrl() const { return d_ctrl; }
		const QPersistentModelIndex& getEditIndex() const { return d_edit; }
        quint64 getEditOid() const;
        void insertHtml( QString );

		enum Format { Plain, Bml, Html };
		Format renderToDocument( const QModelIndex & index, QTextDocument& doc ) const;
        const Txt::LinkRendererInterface* getLinkRenderer() const { return d_linkRenderer; }

		//* Overrides von QAbstractItemDelegate
		void paint(QPainter *painter, const QStyleOptionViewItem &option, 
			const QModelIndex &index) const;
		QSize sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		QWidget * createEditor ( QWidget * parent, const QStyleOptionViewItem & option, 
			const QModelIndex & index ) const;
		bool editorEvent(QEvent *event, QAbstractItemModel *model,
			const QStyleOptionViewItem &option, const QModelIndex &index);
		bool eventFilter(QObject *watched, QEvent *event);
		void setModelData( QWidget * editor, QAbstractItemModel * model, const QModelIndex & index ) const;
	protected slots:
		void extentChanged();
		void invalidate( const QRectF& );
		void relayout();
		void onFontStyleChanged();
	protected:
		void writeData( const QModelIndex & index ) const;
		QSize decoSize( const QModelIndex & index ) const;
	private:
		QFont d_titleFont;
		Txt::TextCtrl* d_ctrl;
		bool d_isReadOnly;
		bool d_biggerTitle;
		bool d_showIcons;
		bool d_showIDs;
		mutable bool d_isTitle;
		mutable bool d_block1;
		mutable quint8 d_editFormat;
		mutable QPersistentModelIndex d_edit;
        const Txt::LinkRendererInterface* d_linkRenderer;
	};
}

#endif // DOCDELEG_H
