#ifndef Oln2_OutlineToHtml__
#define Oln2_OutlineToHtml__

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

#include <Udb/Transaction.h>
#include <QTextStream>

namespace Oln
{
	class OutlineToHtml
	{
	public:
		OutlineToHtml(bool useItemIds = false, bool noFileDirs = false);
		void writeTo( QTextStream &out, const Udb::Obj &oln, QString title, bool fragment = false );
		static void writeCss(QTextStream &out);
	private:
		void descend( QTextStream &out, const Udb::Obj &oln, const QString &label, int level );
        QString anchor( const Udb::Obj& o );
		void writeFragment( QTextStream& out, const Stream::DataCell& txt, QString id, Udb::OID alias );
		void writeParagraph( QTextStream& out, const Stream::DataCell& txt, const QString& id, bool isTitle,
                             const QString& label, int level, Udb::OID alias, const QString& anchor );
		Udb::Obj d_oln;
		bool d_useItemIDs;
		bool d_noFileDirs;
    };
}

#endif // Oln2_OutlineToHtml__
