/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2011  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef PLUGINS_AGGREGATOR_DBUPDATETHREADWORKER_H
#define PLUGINS_AGGREGATOR_DBUPDATETHREADWORKER_H
#include <boost/shared_ptr.hpp>
#include <QObject>
#include "common.h"
#include "channel.h"

namespace LeechCraft
{
namespace Aggregator
{
	class StorageBackend;

	class DBUpdateThreadWorker : public QObject
	{
		Q_OBJECT

		boost::shared_ptr<StorageBackend> SB_;
	public:
		DBUpdateThreadWorker (QObject* = 0);
	public slots:
		void toggleChannelUnread (IDType_t channel, bool state);
	private slots:
		void handleChannelDataUpdated (Channel_ptr);
	signals:
		void channelDataUpdated (IDType_t channelId, IDType_t feedId);
	};
}
}

#endif
