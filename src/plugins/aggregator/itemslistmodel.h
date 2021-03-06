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

#ifndef PLUGINS_AGGREGATOR_ITEMSLISTMODEL_H
#define PLUGINS_AGGREGATOR_ITEMSLISTMODEL_H
#include <QAbstractItemModel>
#include <QStringList>
#include <QSet>
#include <QPair>
#include <QIcon>
#include "item.h"

namespace LeechCraft
{
namespace Aggregator
{
	class ItemsListModel : public QAbstractItemModel
	{
		Q_OBJECT

		QStringList ItemHeaders_;
		items_shorts_t CurrentItems_;
		int CurrentRow_;
		IDType_t CurrentChannel_;

		const QIcon StarredIcon_;
		const QIcon UnreadIcon_;
		const QIcon ReadIcon_;
	public:
		ItemsListModel (QObject* = 0);

		int GetSelectedRow () const;
		const IDType_t& GetCurrentChannel () const;
		void SetCurrentChannel (const IDType_t&);
		void Selected (const QModelIndex&);
		void MarkItemReadStatus (const QModelIndex&, bool);
		const ItemShort& GetItem (const QModelIndex&) const;
		const items_shorts_t& GetAllItems () const;
		bool IsItemRead (int) const;
		QStringList GetCategories (int) const;
		void Reset (const IDType_t&);
		void Reset (const QList<IDType_t>&);
		void RemoveItems (QSet<IDType_t>);
		void ItemDataUpdated (Item_ptr);

		int columnCount (const QModelIndex& = QModelIndex ()) const;
		QVariant data (const QModelIndex&, int = Qt::DisplayRole) const;
		Qt::ItemFlags flags (const QModelIndex&) const;
		QVariant headerData (int, Qt::Orientation, int = Qt::DisplayRole) const;
		QModelIndex index (int, int, const QModelIndex& = QModelIndex()) const;
		QModelIndex parent (const QModelIndex&) const;
		int rowCount (const QModelIndex& = QModelIndex ()) const;
	private slots:
		void handleChannelRemoved (IDType_t);
		void handleItemsRemoved (const QSet<IDType_t>&);
	};
}
}

#endif
