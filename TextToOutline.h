#ifndef __Oln2_TextToOutline__
#define __Oln2_TextToOutline__

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

namespace Oln
{
	class TextToOutline
	{
	public:
		static QList<Udb::Obj> parse( QString text, Udb::Transaction*, Stream::DataCell::OID home = 0 ); // return: empty bei fehler
		static QString toText( const Udb::Obj& item );
	};
}

#endif
