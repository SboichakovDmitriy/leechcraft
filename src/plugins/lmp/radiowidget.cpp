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

#include "radiowidget.h"
#include <QStandardItemModel>
#include <QInputDialog>
#include <QSortFilterProxyModel>
#include <QtDebug>
#include <util/gui/clearlineeditaddon.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/media/iradiostationprovider.h>
#include "core.h"
#include "player.h"

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		class StationsFilterModel : public QSortFilterProxyModel
		{
		public:
			StationsFilterModel (QObject *parent)
			: QSortFilterProxyModel (parent)
			{
			}
		protected:
			bool filterAcceptsRow (int row, const QModelIndex& parent) const
			{
				const auto& pat = this->filterRegExp ().pattern ();
				if (pat.isEmpty ())
					return true;

				const auto& idx = sourceModel ()->index (row, 0, parent);
				if (idx.data (Media::RadioItemRole::ItemType).toInt () == Media::RadioType::None)
					return true;

				return idx.data ().toString ().contains (pat, Qt::CaseInsensitive);
			}
		};
	}

	RadioWidget::RadioWidget (QWidget *parent)
	: QWidget (parent)
	, Player_ (0)
	, StationsModel_ (new QStandardItemModel (this))
	, StationsProxy_ (new StationsFilterModel (this))
	{
		Ui_.setupUi (this);

		StationsProxy_->setDynamicSortFilter (true);
		StationsProxy_->setSourceModel (StationsModel_);
		Ui_.StationsView_->setModel (StationsProxy_);

		connect (Ui_.StationsSearch_,
				SIGNAL (textChanged (QString)),
				StationsProxy_,
				SLOT (setFilterFixedString (QString)));

		new Util::ClearLineEditAddon (Core::Instance ().GetProxy (), Ui_.StationsSearch_);
	}

	void RadioWidget::InitializeProviders ()
	{
		auto providerObjs = Core::Instance ().GetProxy ()->GetPluginsManager ()->
				GetAllCastableRoots<Media::IRadioStationProvider*> ();
		Q_FOREACH (auto provObj, providerObjs)
		{
			auto prov = qobject_cast<Media::IRadioStationProvider*> (provObj);
			Q_FOREACH (auto item, prov->GetRadioListItems ())
			{
				StationsModel_->appendRow (item);
				Root2Prov_ [item] = prov;
			}
		}
	}

	void RadioWidget::SetPlayer (Player *player)
	{
		Player_ = player;
	}

	void RadioWidget::on_StationsView__doubleClicked (const QModelIndex& unmapped)
	{
		const auto& index = StationsProxy_->mapToSource (unmapped);
		const auto item = StationsModel_->itemFromIndex (index);
		auto root = item;
		while (auto parent = root->parent ())
			root = parent;
		if (!Root2Prov_.contains (root))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown provider for index"
					<< index;
			return;
		}

		QString param;
		switch (item->data (Media::RadioItemRole::ItemType).toInt ())
		{
		case Media::RadioType::None:
			return;
		case Media::RadioType::Predefined:
			break;
		case Media::RadioType::SimilarArtists:
			param = QInputDialog::getText (this,
					tr ("Similar artists radio"),
					tr ("Enter artist name for which to tune the similar artists radio station:"));
			if (param.isEmpty ())
				return;
			break;
		case Media::RadioType::GlobalTag:
			param = QInputDialog::getText (this,
					tr ("Global tag radio"),
					tr ("Enter global tag name:"));
			if (param.isEmpty ())
				return;
			break;
		}

		auto station = Root2Prov_ [root]->GetRadioStation (item, param);
		if (station)
			Player_->SetRadioStation (station);
	}
}
}
