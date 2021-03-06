/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "historywidget.h"
#include <QDateTime>
#include "core.h"
#include "historymodel.h"

namespace LeechCraft
{
namespace Poshuku
{
	HistoryWidget::HistoryWidget (QWidget *parent)
	: QWidget (parent)
	{
		Ui_.setupUi (this);

		HistoryFilterModel_.reset (new HistoryFilterModel (this));
		HistoryFilterModel_->setSourceModel (Core::Instance ().GetHistoryModel ());
		HistoryFilterModel_->setDynamicSortFilter (true);
		Ui_.HistoryView_->setModel (HistoryFilterModel_.get ());
	
		connect (Ui_.HistoryFilterLine_,
				SIGNAL (textChanged (const QString&)),
				this,
				SLOT (updateHistoryFilter ()));
		connect (Ui_.HistoryFilterType_,
				SIGNAL (currentIndexChanged (int)),
				this,
				SLOT (updateHistoryFilter ()));
		connect (Ui_.HistoryFilterCaseSensitivity_,
				SIGNAL (stateChanged (int)),
				this,
				SLOT (updateHistoryFilter ()));

		QHeaderView *itemsHeader = Ui_.HistoryView_->header ();
		QFontMetrics fm = fontMetrics ();
		itemsHeader->resizeSection (0,
				fm.width ("Average site title can be very big, it's also the "
					"most important part, so it's priority is the biggest."));
		itemsHeader->resizeSection (1,
				fm.width (QDateTime::currentDateTime ().toString () + " space"));
		itemsHeader->resizeSection (2,
				fm.width ("Average URL could be very very long, but we don't account this."));
	}

	void HistoryWidget::on_HistoryView__activated (const QModelIndex& index)
	{
		if (!index.parent ().isValid ())
			return;

		Core::Instance ().NewURL (index.sibling (index.row (),
					HistoryModel::ColumnURL).data ().toString ());
	}
	
	void HistoryWidget::updateHistoryFilter ()
	{
		int section = Ui_.HistoryFilterType_->currentIndex ();
		QString text = Ui_.HistoryFilterLine_->text ();
	
		switch (section)
		{
			case 1:
				HistoryFilterModel_->setFilterWildcard (text);
				break;
			case 2:
				HistoryFilterModel_->setFilterRegExp (text);
				break;
			default:
				HistoryFilterModel_->setFilterFixedString (text);
				break;
		}
	
		HistoryFilterModel_->
			setFilterCaseSensitivity ((Ui_.HistoryFilterCaseSensitivity_->
						checkState () == Qt::Checked) ? Qt::CaseSensitive :
					Qt::CaseInsensitive);
	}
}
}
