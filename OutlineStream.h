#ifndef __Oln_OutlineStream__
#define __Oln_OutlineStream__

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

#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <Udb/Obj.h>
#include <Udb/Transaction.h>

namespace Oln
{
	class OutlineStream
	{
	public:
		/* External Format
			
				Slot <ascii> = "CrossLineStream"
				Slot <ascii> = "0.1"
				Slot <DateTime> = now
				[ Internal Format ]
		*/
		/* Internal Format (names as Tags)

			Frame 'oln'
				Slot 'text' = <Bml> | <String> | <Html> // rtxt
				Slot 'titl' = bool // isTitle
				Slot 'ro'	= bool // isReadOnly
				Slot 'exp'	= bool // isExpanded
				Slot 'ali'	= OID | UUID  // Alias

				[ Frame 'oln' ]* // sequenziell und rekursiv

		*/
		static void writeTo( Stream::DataWriter&, const Udb::Obj& oln ); // kann Uuid erzeugen
		Udb::Obj readFrom( Stream::DataReader&, Udb::Transaction*, Stream::DataCell::OID home = 0, bool nextToken = true );
		const QString& getError() const { return d_error; }
        static const char* s_olnFrameTag;
	private:
		Udb::Obj readObj( Stream::DataReader&, Udb::Transaction*, Stream::DataCell::OID home );
		QString d_error;
	};
}

#endif // __Oln_OutlineStream__
