/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2012  Georg Rudoy
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

#include "queuemanager.h"
#include <QTimer>

namespace LeechCraft
{
namespace Util
{
	QueueManager::QueueManager (int timeout, QObject *parent)
	: QObject (parent)
	, Timeout_ (timeout)
	{
	}

	void QueueManager::Schedule (std::function<void ()> f, QObject *dep)
	{
		const auto& now = QDateTime::currentDateTime ();
		Queue_.push_back ({ f, dep });

		const auto diff = LastRequest_.msecsTo (now);
		if (diff > 1.1 * Timeout_)
			exec ();
		else if (Queue_.size () == 1)
			QTimer::singleShot (Timeout_ - diff,
					this,
					SLOT (exec ()));
	}

	void QueueManager::exec ()
	{
		if (Queue_.isEmpty ())
			return;

		const auto& pair = Queue_.takeFirst ();
		if (!pair.second)
		{
			exec ();
			return;
		}

		pair.first ();
		LastRequest_ = QDateTime::currentDateTime ();

		if (!Queue_.isEmpty ())
			QTimer::singleShot (Timeout_,
					this,
					SLOT (exec ()));
	}
}
}