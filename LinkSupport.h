#ifndef LINKSUPPORT_H
#define LINKSUPPORT_H

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

#include <Udb/Obj.h>
#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace Oln
{
    struct Link // Eingebetteter Link in Text
    {
        Udb::OID d_oid;
        QUuid d_db;
        bool d_showIcon;
        bool d_showId;
		bool d_showName; // Text
		bool d_showContext; // für Home Outline Items, Parent für alle anderen
        bool d_paraNumber; // Paragraphennummerierung für Outline Items
		bool d_showSubName; // Text von Subitem if showContext
        quint8 d_elide; // max. Länge, 0..alles

        bool readFrom( const QByteArray& );
        QByteArray writeTo();

        Link():d_oid(0),d_showIcon(false),d_showId(false),d_elide(0),
			d_showName(true),d_showContext(false),d_paraNumber(false),d_showSubName(true) {}
        Link( const Udb::Obj& );
		Link( bool showIcon, bool showId, bool showName, bool showContext, quint8 elide, bool para, bool subName );
    };

    class EditLinkDlg : public QDialog
    {
    public:
        EditLinkDlg( QWidget* );
        bool edit( Link& url, bool isOutlineItem = false );
    private:
        QCheckBox* d_showIcon;
        QCheckBox* d_showId;
        QCheckBox* d_showName;
        QSpinBox* d_elide;
		QCheckBox* d_showContext;
        QCheckBox* d_paraNumber;
		QCheckBox* d_showSubName;
    };
}

#endif // LINKSUPPORT_H
