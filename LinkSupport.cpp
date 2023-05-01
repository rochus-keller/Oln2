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

#include "LinkSupport.h"
#include <Udb/Database.h>
#include <Stream/DataReader.h>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QCheckBox>
using namespace Oln;
using namespace Stream;


Link::Link(const Udb::Obj & obj)
{
    d_oid = obj.getOid();
    if( !obj.isNull() )
        d_db = obj.getDb()->getDbUuid();
    d_showIcon = true;
    d_showId = true;
    d_showName = true;
    d_elide = 0;
	d_showContext = false;
    d_paraNumber = false;
	d_showSubName = true;
}

Link::Link(bool showIcon, bool showId, bool showName, bool showContext, quint8 elide, bool para, bool subName)
{
    d_showIcon = showIcon;
    d_showId = showId;
    d_showName = showName;
    d_elide = elide;
	d_showContext = showContext;
    d_oid = 0;
    d_paraNumber = para;
	d_showSubName = subName;
}

bool Link::readFrom(const QByteArray & data )
{
    *this = Link();
    Stream::DataReader in( data );
    DataReader::Token t = in.nextToken();
    if( t == DataReader::BeginFrame && in.getName().getTag().equals( "link" ) )
    {
        t = in.nextToken();
        while( t == DataReader::Slot )
        {
            const Stream::NameTag name = in.getName().getTag();
            if( name.equals( "oid" ) )
                d_oid = in.getValue().getOid();
            else if( name.equals( "db" ) )
                d_db = in.getValue().getUuid();
            else if( name.equals( "icon" ) )
                d_showIcon = in.getValue().getBool();
            else if( name.equals( "id" ) )
                d_showId = in.getValue().getBool();
            else if( name.equals( "name" ) )
                d_showName = in.getValue().getBool();
            else if( name.equals( "elid" ) )
                d_elide = in.getValue().getUInt8();
            else if( name.equals( "home" ) )
				d_showContext = in.getValue().getBool();
            else if( name.equals( "panr" ) )
                d_paraNumber = in.getValue().getBool();
			else if( name.equals( "subn" ) )
				d_showSubName = in.getValue().getBool();
			t = in.nextToken();
        }
        return true;
    }else
        return false;
}

QByteArray Link::writeTo()
{
    Stream::DataWriter out;
    out.startFrame( Stream::NameTag("link") );
    out.writeSlot( Stream::DataCell().setOid( d_oid ), Stream::NameTag("oid") );
    out.writeSlot( Stream::DataCell().setUuid( d_db ), Stream::NameTag( "db" ) );
    out.writeSlot( Stream::DataCell().setBool( d_showIcon ), Stream::NameTag( "icon" ) );
    out.writeSlot( Stream::DataCell().setBool( d_showId ), Stream::NameTag( "id" ) );
    out.writeSlot( Stream::DataCell().setBool( d_showName ), Stream::NameTag( "name" ) );
    out.writeSlot( Stream::DataCell().setUInt8( d_elide ), Stream::NameTag("elid") );
	out.writeSlot( Stream::DataCell().setBool( d_showContext ), Stream::NameTag( "home" ) );
    out.writeSlot( Stream::DataCell().setBool( d_paraNumber ), Stream::NameTag( "panr" ) );
	out.writeSlot( Stream::DataCell().setBool( d_showSubName ), Stream::NameTag( "subn" ) );
	out.endFrame();
    return out.getStream();
}

EditLinkDlg::EditLinkDlg(QWidget * w):QDialog(w)
{
    setWindowTitle( tr("Edit Link Attributes") );
    QVBoxLayout* vbox = new QVBoxLayout( this );

    d_showIcon = new QCheckBox( tr("Show Icon" ), this );
    vbox->addWidget( d_showIcon );
    d_showId = new QCheckBox( tr("Show ID"), this );
    vbox->addWidget( d_showId );
    d_showName = new QCheckBox( tr("Show Title/Text"), this );
    vbox->addWidget( d_showName );
	d_paraNumber = new QCheckBox( tr("Show Paragraph/ID"), this );
	vbox->addWidget( d_paraNumber );
	d_showContext = new QCheckBox( tr("Show Context"), this );
	vbox->addWidget( d_showContext );
	d_showSubName = new QCheckBox( tr("Show Item Text"), this );
	vbox->addWidget( d_showSubName );
	connect( d_showContext, SIGNAL(toggled(bool)), d_showSubName, SLOT(setEnabled(bool) ) );
	d_elide = new QSpinBox( this );
    d_elide->setRange( 0, 255 );
    vbox->addWidget( d_elide );

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
	bb->button( QDialogButtonBox::Ok )->setDefault(true);
	vbox->addWidget( bb );
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));
}

bool EditLinkDlg::edit(Link &l, bool isOutlineItem)
{
	Q_UNUSED(isOutlineItem);
	// NOTE: neu sind alle Optionen auch für Objekte sinnvoll, die keine Outlines/Items sind
    d_showIcon->setChecked( l.d_showIcon );
    d_showId->setChecked( l.d_showId );
    d_showName->setChecked( l.d_showName );
    d_elide->setValue( l.d_elide );
    d_elide->setToolTip( tr("Elided length (0..show all)") );
	d_showContext->setChecked( l.d_showContext );
	//d_resolveHome->setEnabled( isOutlineItem );
    d_paraNumber->setChecked( l.d_paraNumber );
	//d_paraNumber->setEnabled( isOutlineItem );
	d_showSubName->setChecked( l.d_showSubName );
	d_showSubName->setEnabled( l.d_showContext );
	//d_showSubName->setEnabled( isOutlineItem );

    if( exec() == QDialog::Accepted )
	{
        l.d_showIcon = d_showIcon->isChecked();
        l.d_showId = d_showId->isChecked();
        l.d_showName = d_showName->isChecked();
		l.d_showContext = d_showContext->isChecked();
        l.d_paraNumber = d_paraNumber->isChecked();
		l.d_showSubName = d_showSubName->isChecked();
        l.d_elide = d_elide->value();
		return true;
	}else
		return false;
}
