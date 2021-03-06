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

#include "infomodelmanager.h"
#include <QStandardItemModel>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/ijobholder.h>

Q_DECLARE_METATYPE (QPersistentModelIndex);

namespace LeechCraft
{
namespace TPI
{
	namespace
	{
		class InfoModel : public QStandardItemModel
		{
		public:
			enum Roles
			{
				Done = Qt::UserRole + 1,
				Total,
				Name
			};

			InfoModel (QObject *parent)
			: QStandardItemModel (parent)
			{
				QHash<int, QByteArray> roleNames;
				roleNames [Roles::Done] = "jobDone";
				roleNames [Roles::Total] = "jobTotal";
				roleNames [Roles::Name] = "jobName";
				setRoleNames (roleNames);
			}
		};
	}

	InfoModelManager::InfoModelManager (ICoreProxy_ptr proxy, QObject *parent)
	: QObject (parent)
	, Proxy_ (proxy)
	, Model_ (new InfoModel (this))
	{
	}

	QAbstractItemModel* InfoModelManager::GetModel () const
	{
		return Model_;
	}

	void InfoModelManager::SecondInit ()
	{
		auto pm = Proxy_->GetPluginsManager ();
		for (auto ijh : pm->GetAllCastableTo<IJobHolder*> ())
			ManageModel (ijh->GetRepresentation ());
	}

	void InfoModelManager::ManageModel (QAbstractItemModel *model)
	{
		connect (model,
				SIGNAL (rowsInserted (QModelIndex, int, int)),
				this,
				SLOT (handleRowsInserted (QModelIndex, int, int)));
		connect (model,
				SIGNAL (rowsAboutToBeRemoved (QModelIndex, int, int)),
				this,
				SLOT (handleRowsRemoved (QModelIndex, int, int)));
		connect (model,
				SIGNAL (dataChanged (QModelIndex, QModelIndex)),
				this,
				SLOT (handleDataChanged (QModelIndex, QModelIndex)));

		if (auto numRows = model->rowCount ())
			HandleRows (model, 0, numRows - 1);
	}

	namespace
	{
		bool IsInternal (const QModelIndex& idx)
		{
			const auto flags = idx.data (ProcessState::TaskFlags).value<TaskParameters> ();
			return flags & TaskParameter::Internal;
		}
	}

	void InfoModelManager::HandleRows (QAbstractItemModel *model, int from, int to)
	{
		for (int i = from; i <= to; ++i)
		{
			const auto& idx = model->index (i, JobHolderColumn::JobProgress);
			if (IsInternal (idx))
				continue;

			const auto row = idx.data (CustomDataRoles::RoleJobHolderRow).value<JobHolderRow> ();
			if (row != JobHolderRow::DownloadProgress &&
					row != JobHolderRow::ProcessProgress)
				continue;

			if (idx.data (ProcessState::Done) == idx.data (ProcessState::Total))
				continue;

			auto ourItem = new QStandardItem;

			const auto& name = model->index (i, JobHolderColumn::JobName).data ().toString ();
			ourItem->setData (name, InfoModel::Name);

			PIdx2Item_ [idx] = ourItem;
			HandleData (model, i, i);

			Model_->appendRow (ourItem);
		}
	}

	void InfoModelManager::HandleData (QAbstractItemModel *model, int from, int to)
	{
		for (int i = from; i <= to; ++i)
		{
			const auto& idx = model->index (i, JobHolderColumn::JobProgress);
			if (IsInternal (idx))
				continue;

			auto done = idx.data (ProcessState::Done).toLongLong ();
			auto total = idx.data (ProcessState::Total).toLongLong ();

			auto item = PIdx2Item_.value (idx);
			if (!item)
			{
				if (done != total)
					HandleRows (model, i, i);
				continue;
			}

			if (done == total)
			{
				PIdx2Item_.remove (idx);
				Model_->removeRow (item->row ());
				continue;
			}

			while (done > 1000 && total > 1000)
			{
				done /= 10;
				total /= 10;
			}

			item->setData (static_cast<double> (done), InfoModel::Roles::Done);
			item->setData (static_cast<double> (total), InfoModel::Roles::Total);
			item->setData (model->index (i, JobHolderColumn::JobName).data ().toString (),
					InfoModel::Roles::Name);
		}
	}

	void InfoModelManager::handleRowsInserted (const QModelIndex& parent, int from, int to)
	{
		if (parent.isValid ())
			return;

		auto model = qobject_cast<QAbstractItemModel*> (sender ());
		HandleRows (model, from, to);
	}

	void InfoModelManager::handleRowsRemoved (const QModelIndex& parent, int from, int to)
	{
		if (parent.isValid ())
			return;

		auto model = qobject_cast<QAbstractItemModel*> (sender ());

		for (int i = from; i <= to; ++i)
		{
			const auto& idx = model->index (i, JobHolderColumn::JobProgress);
			auto item = PIdx2Item_.take (idx);
			if (!item)
				continue;

			Model_->removeRow (item->row ());
		}
	}

	void InfoModelManager::handleDataChanged (const QModelIndex& topLeft, const QModelIndex& bottomRight)
	{
		if (bottomRight.column () < JobHolderColumn::JobProgress)
			return;

		auto model = qobject_cast<QAbstractItemModel*> (sender ());
		HandleData (model, topLeft.row (), bottomRight.row ());
	}
}
}
