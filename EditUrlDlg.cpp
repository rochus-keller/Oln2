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

#include "EditUrlDlg.h"
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QUrl>
#include <QFileInfo>
#include <QtDebug>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolBar>
#include <QAction>
#include <QFormLayout>
using namespace Oln;

EditUrlDlg::EditUrlDlg( QWidget* p ):QDialog(p)
{
	setWindowTitle( tr("Edit URL") );

	QGridLayout* grid = new QGridLayout( this );

	grid->addWidget( new QLabel( tr("URL:"), this ), 0, 0 );
	d_url = new QLineEdit( this );
	grid->addWidget( d_url, 0, 1 );

	grid->addWidget( new QLabel( tr("Copy:"), this ), 1, 0 );
	QToolBar* tb = new QToolBar( this );
	tb->setToolButtonStyle( Qt::ToolButtonTextOnly );
	grid->addWidget( tb, 1, 1 );

	tb->addAction( tr("All"), this, SLOT( copyAll() ) )->setShortcut( tr("CTRL+A") );
	tb->addAction( tr("Host"), this, SLOT( copyHost() ) )->setShortcut( tr("CTRL+H") );
	tb->addAction( tr("Base Name"), this, SLOT( copyBase() ) )->setShortcut( tr("CTRL+B") );
	tb->addAction( tr("Full Name"), this, SLOT( copyFull() ) )->setShortcut( tr("CTRL+F") );

	grid->addWidget( new QLabel( tr("Text:"), this ), 2, 0 );
	d_text = new QLineEdit( this );
	grid->addWidget( d_text, 2, 1 );

	QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
	bb->button( QDialogButtonBox::Ok )->setDefault(true);
	grid->addWidget( bb, 3, 0, 1, 2 );
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));
	setMinimumWidth( 400 );
}

bool EditUrlDlg::edit( QString& url, QString& text )
{
	d_url->setText( url );
	d_text->setText( text );
	if( exec() == QDialog::Accepted )
	{
		url = d_url->text();
		text = d_text->text();
		return true;
	}else
		return false;
}

void EditUrlDlg::copyBase()
{
	QUrl url( d_url->text() );
	QFileInfo info( url.toString() );
	d_text->setText( info.completeBaseName() );
}

void EditUrlDlg::copyFull()
{
	QUrl url( d_url->text() );
	QFileInfo info( url.toString() );
	d_text->setText( info.fileName() );
}

void EditUrlDlg::copyAll()
{
	QUrl url( d_url->text() );
	d_text->setText( url.toString() );
}

void EditUrlDlg::copyHost()
{
	QUrl url( d_url->text() );
	d_text->setText( url.host() );
}

EditPropertiesDlg::EditPropertiesDlg(QWidget *, bool readOnly)
{
	setWindowTitle( tr("Item Properties") );

	QVBoxLayout* vbox = new QVBoxLayout( this );
	QFormLayout* form = new QFormLayout();
	vbox->addLayout( form );

	d_altIdent = new QLineEdit( this );
	d_altIdent->setReadOnly( readOnly );
	form->addRow( tr("Custom ID:"), d_altIdent );

	d_ident = new QLabel( this );
	form->addRow( tr("Internal ID:"), d_ident );

	d_alias = new QLabel( this );
	form->addRow( tr("Pointing to:"), d_alias );

	d_createdOn = new QLabel( this );
	form->addRow( tr("Created on:"), d_createdOn );

	d_modifiedOn = new QLabel( this );
	form->addRow( tr("Modified on:"), d_modifiedOn );

	vbox->addStretch();

	QDialogButtonBox* bb = new QDialogButtonBox( (readOnly)? QDialogButtonBox::Close :
															 ( QDialogButtonBox::Ok | QDialogButtonBox::Cancel ),
												 Qt::Horizontal, this );
	vbox->addWidget( bb );
	connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
	connect(bb, SIGNAL(rejected()), this, SLOT(reject()));
}

bool EditPropertiesDlg::edit(OutlineItem &item)
{
	if( item.isNull() )
		return false;
	d_altIdent->setText( item.getAltIdent() );
	d_ident->setText( item.getIdent() );
	OutlineItem alias = item.getAlias();
	if( !alias.isNull() )
	{
		QString id = item.getAltIdent();
		if( id.isEmpty() )
			id = item.getIdent();
		if( !id.isEmpty() )
			id += QChar(' ');
		d_alias->setText( id + alias.getText() );
	}
	d_createdOn->setText( OutlineItem::formatTimeStamp( item.getCreatedOn() ) );
	d_modifiedOn->setText( OutlineItem::formatTimeStamp( item.getModifiedOn() ) );
	if( exec() == QDialog::Accepted && !d_altIdent->isReadOnly() )
	{
		item.setAltIdent( d_altIdent->text().trimmed() );
		item.setModifiedOn();
		item.commit();
		return true;
	}else
		return false;
}

