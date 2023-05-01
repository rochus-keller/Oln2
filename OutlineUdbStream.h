#ifndef __Oln_OutlineUdbStream__
#define __Oln_OutlineUdbStream__

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

#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <Udb/Obj.h>
#include <Udb/Transaction.h>

namespace Oln
{
	class OutlineUdbStream
	{
	public:
		static const quint32 s_magic;

		/*  Format Specification (names as Tags, named Slots optional)

			Header ::=
				Slot <tag> = "olns"
				Slot <uint32> = magic
				Slot <uint16> = 2
				Slot <DateTime> = stream created on
				Slot <any> = name
				Slot <String> = ident
				Slot <String> = alt ident
				Slot <DateTime> = outline created on
				Slot <DateTime> = last modified on

			Body ::=
				[ Item ]*

			Item ::=
				Frame 'oln'
					Slot 'dbid' = uuid    // only the first OlnFrame in Body has this Slot!
					Slot 'oid'  = OID
					Slot 'text' = <Bml> | <String> | <Html> // rtxt
					Slot 'titl' = bool    // isTitle
					Slot 'ro'	= bool    // isReadOnly
					Slot 'id'   = String  // Ident
					Slot 'aid'  = String  // AltIdent
					Slot 'exp'	= bool    // isExpanded
					Slot 'ali'	= OID     // Alias; zeigt immer in dieselbe DB wie die anderen Objekte
					[ Item ]*

			Stream ::=
				[ Header ] Body

		*/
		static void writeOutline( Stream::DataWriter&, const Udb::Obj& outline, bool writeHeader = true );
		static void writeItem( Stream::DataWriter&, Udb::Obj item, bool writeDbId = false, bool recursive = true );

		class Exception : public std::exception
		{
		public:
			Exception( const QByteArray& msg ):d_msg(msg){}
			~Exception() throw() {}
			const char* what() const throw() { return d_msg; }
			QByteArray d_msg;
		};
		typedef QMap<Udb::OID,Udb::OID> OidMap; // stream OID -> new OID

		static QByteArray readOutline( Stream::DataReader&, Udb::Obj outline, bool readHeader = true ) throw();
		static QByteArray readItems( Stream::DataReader&, Udb::Obj parent, const Udb::Obj& before ) throw();
	private:
		static QUuid readOlnFrame( Stream::DataReader & in, Udb::Obj parent, const Udb::Obj& home,
								   const Udb::Obj& before, OidMap& );
		static void writeHeader( Stream::DataWriter&, const Udb::Obj& outline );
		static bool remapRefs( const Udb::Obj& home, const QUuid &db, const OidMap & );
	};
}

#endif // __Oln_OutlineUdbStream__
