#ifndef __Oln_EditUrlDlg__
#define __Oln_EditUrlDlg__

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

#include <QDialog>
#include <Oln2/OutlineItem.h>

class QLineEdit;
class QPushButton;
class QCheckBox;
class QLabel;

namespace Oln
{
	class EditUrlDlg : public QDialog
	{
		Q_OBJECT 
	public:
		EditUrlDlg( QWidget* );

		bool edit( QString& url, QString& text );
	private slots:
		void copyAll();
		void copyHost();
		void copyBase();
		void copyFull();
	private:
		QLineEdit* d_url;
		QLineEdit* d_text;
	};

	class EditPropertiesDlg : public QDialog
	{
	public:
		EditPropertiesDlg( QWidget*, bool readOnly );
		bool edit( OutlineItem& item );
	private:
		QLabel* d_ident;
		QLineEdit* d_altIdent;
		QLabel* d_alias;
		QLabel* d_createdOn;
		QLabel* d_modifiedOn;
	};

}

#endif
