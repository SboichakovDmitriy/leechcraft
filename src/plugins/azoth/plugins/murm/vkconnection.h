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

#pragma once

#include <functional>
#include <QObject>
#include <QHash>
#include <QUrl>
#include <QVariantList>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/azoth/iclentry.h>
#include "structures.h"

namespace LeechCraft
{
namespace Util
{
	class QueueManager;

	namespace SvcAuth
	{
		class VkAuthManager;
	}
}

namespace Azoth
{
namespace Murm
{
	class VkConnection : public QObject
	{
		Q_OBJECT

		Util::SvcAuth::VkAuthManager * const AuthMgr_;
		const ICoreProxy_ptr Proxy_;

		QByteArray LastCookies_;

		QList<std::function<void (QString)>> PreparedCalls_;
		Util::QueueManager *CallQueue_;

		EntryStatus Status_;

		QString LPKey_;
		QString LPServer_;
		qulonglong LPTS_;

		QUrl LPURLTemplate_;

		QHash<int, std::function<void (QVariantList)>> Dispatcher_;
		QHash<QNetworkReply*, std::function<void (qulonglong)>> MsgReply2Setter_;
	public:
		VkConnection (const QByteArray&, ICoreProxy_ptr);

		const QByteArray& GetCookies () const;

		void RerequestFriends ();

		void SendMessage (qulonglong to, const QString& body,
				std::function<void (qulonglong)> idSetter);

		void SetStatus (const EntryStatus&);
		const EntryStatus& GetStatus () const;
	private:
		void PushFriendsRequest ();
		void PushLPFetchCall ();
		void Poll ();
	private slots:
		void handlePollFinished ();

		void callWithKey (const QString&);

		void handleGotFriendLists ();
		void handleGotFriends ();
		void handleGotLPServer ();
		void handleMessageSent ();

		void saveCookies (const QByteArray&);
	signals:
		void cookiesChanged ();

		void gotLists (const QList<ListInfo>&);
		void gotUsers (const QList<UserInfo>&);

		void userStateChanged (qulonglong uid, bool online);
	};
}
}
}
