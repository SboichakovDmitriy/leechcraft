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

#include "vkconnection.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include <QTimer>
#include <qjson/parser.h>
#include <util/svcauth/vkauthmanager.h>
#include <util/queuemanager.h>
#include "longpollmanager.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Murm
{
	VkConnection::VkConnection (const QByteArray& cookies, ICoreProxy_ptr proxy)
	: AuthMgr_ (new Util::SvcAuth::VkAuthManager ("3778319",
			{ "messages", "notifications", "friends", "status", "photos" },
			cookies, proxy, this))
	, Proxy_ (proxy)
	, CallQueue_ (new Util::QueueManager (400))
	, LPManager_ (new LongPollManager (this, proxy))
	{
		connect (AuthMgr_,
				SIGNAL (cookiesChanged (QByteArray)),
				this,
				SLOT (saveCookies (QByteArray)));
		connect (AuthMgr_,
				SIGNAL (gotAuthKey (QString)),
				this,
				SLOT (callWithKey (QString)));

		connect (LPManager_,
				SIGNAL (listening ()),
				this,
				SLOT (handleListening ()));
		connect (LPManager_,
				SIGNAL (stopped ()),
				this,
				SLOT (handlePollStopped ()));
		connect (LPManager_,
				SIGNAL (pollError ()),
				this,
				SLOT (handlePollError ()));
		connect (LPManager_,
				SIGNAL (gotPollData (QVariantMap)),
				this,
				SLOT (handlePollData (QVariantMap)));

		Dispatcher_ [1] = [this] (const QVariantList&) {};
		Dispatcher_ [2] = [this] (const QVariantList&) {};
		Dispatcher_ [3] = [this] (const QVariantList&) {};

		Dispatcher_ [4] = [this] (const QVariantList& items)
		{
			emit gotMessage ({
					items.value (1).toULongLong (),
					items.value (3).toULongLong (),
					items.value (6).toString (),
					MessageFlags (items.value (2).toInt ()),
					QDateTime::fromTime_t (items.value (4).toULongLong ()),
					items.value (7).toMap ()
				});
		};
		Dispatcher_ [8] = [this] (const QVariantList& items)
			{ emit userStateChanged (items.value (1).toLongLong () * -1, true); };
		Dispatcher_ [9] = [this] (const QVariantList& items)
			{ emit userStateChanged (items.value (1).toLongLong () * -1, false); };
		Dispatcher_ [61] = [this] (const QVariantList& items)
			{ emit gotTypingNotification (items.value (1).toULongLong ()); };

		Dispatcher_ [101] = [this] (const QVariantList&) {};	// unknown stuff
	}

	const QByteArray& VkConnection::GetCookies () const
	{
		return LastCookies_;
	}

	void VkConnection::RerequestFriends ()
	{
		PushFriendsRequest ();
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::SendMessage (qulonglong to,
			const QString& body, std::function<void (qulonglong)> idSetter)
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([=] (const QString& key) -> QNetworkReply*
			{
				QUrl url ("https://api.vk.com/method/messages.send");
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("uid", QString::number (to));
				url.addQueryItem ("message", body);
				url.addQueryItem ("type", "1");

				auto reply = nam->get (QNetworkRequest (url));
				MsgReply2Setter_ [reply] = idSetter;
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleMessageSent ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::SendTyping (qulonglong to)
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([this, nam, to] (const QString& key) -> QNetworkReply*
			{
				QUrl url ("https://api.vk.com/method/messages.setActivity");
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("uid", QString::number (to));
				url.addQueryItem ("type", "typing");

				auto reply = nam->get (QNetworkRequest (url));
				connect (reply,
						SIGNAL (finished ()),
						reply,
						SLOT (deleteLater ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	namespace
	{
		template<typename T>
		QString CommaJoin (const QList<T>& ids)
		{
			QStringList converted;
			for (auto id : ids)
				converted << QString::number (id);
			return converted.join (",");
		}
	}

	void VkConnection::MarkAsRead (const QList<qulonglong>& ids)
	{
		const auto& joined = CommaJoin (ids);

		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([this, nam, joined] (const QString& key) -> QNetworkReply*
			{
				QUrl url ("https://api.vk.com/method/messages.markAsRead");
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("mids", joined);

				auto reply = nam->get (QNetworkRequest (url));
				connect (reply,
						SIGNAL (finished ()),
						reply,
						SLOT (deleteLater ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::RequestGeoIds (const QList<int>& codes, GeoSetter_f setter, GeoIdType type)
	{
		const auto& joined = CommaJoin (codes);

		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([=] (const QString& key) -> QNetworkReply*
			{
				QString method;
				switch (type)
				{
				case GeoIdType::Country:
					method = "getCountries";
					break;
				case GeoIdType::City:
					method = "getCities";
					break;
				}

				QUrl url ("https://api.vk.com/method/" + method);
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("cids", joined);

				auto reply = nam->get (QNetworkRequest (url));
				CountryReply2Setter_ [reply] = setter;
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleCountriesFetched ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::GetMessageInfo (qulonglong id, MessageInfoSetter_f setter)
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([=] (const QString& key) -> QNetworkReply*
			{
				QUrl url ("https://api.vk.com/method/messages.getById");
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("mid", QString::number (id));

				auto reply = nam->get (QNetworkRequest (url));
				Reply2MessageSetter_ [reply] = setter;
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleMessageInfoFetched ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::GetPhotoInfos (const QStringList& ids, PhotoInfoSetter_f setter)
	{
		const auto& joined = ids.join (",");
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([=] (const QString& key) -> QNetworkReply*
			{
				QUrl url ("https://api.vk.com/method/photos.getById");
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("photos", joined);

				auto reply = nam->get (QNetworkRequest (url));
				Reply2PhotoSetter_ [reply] = setter;
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handlePhotoInfosFetched ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::SetStatus (const QString& status)
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([=] (const QString& key) -> QNetworkReply*
			{
				QUrl url ("https://api.vk.com/method/status.set");
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("text", status);

				auto reply = nam->get (QNetworkRequest (url));
				connect (reply,
						SIGNAL (finished ()),
						reply,
						SLOT (deleteLater ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::SetStatus (const EntryStatus& status)
	{
		LPManager_->ForceServerRequery ();

		Status_ = status;
		if (Status_.State_ == SOffline)
		{
			LPManager_->Stop ();
			return;
		}

		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([this, nam] (const QString& key) -> QNetworkReply*
			{
				QUrl lpUrl ("https://api.vk.com/method/friends.getLists");
				lpUrl.addQueryItem ("access_token", key);
				auto reply = nam->get (QNetworkRequest (lpUrl));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleGotFriendLists ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	EntryStatus VkConnection::GetStatus () const
	{
		return CurrentStatus_;
	}

	void VkConnection::QueueRequest (VkConnection::PreparedCall_f call)
	{
		PreparedCalls_ << call;
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::PushFriendsRequest ()
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([this, nam] (const QString& key) -> QNetworkReply*
			{
				QUrl friendsUrl ("https://api.vk.com/method/friends.get");
				friendsUrl.addQueryItem ("access_token", key);
				friendsUrl.addQueryItem ("fields",
						"first_name,last_name,nickname,photo,photo_big,sex,"
						"bdate,city,country,timezone,contacts,education");
				auto reply = nam->get (QNetworkRequest (friendsUrl));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleGotFriends ()));
				return reply;
			});
	}

	bool VkConnection::CheckFinishedReply (QNetworkReply *reply)
	{
		reply->deleteLater ();

		const auto pos = std::find_if (RunningCalls_.begin (), RunningCalls_.end (),
				[reply] (decltype (RunningCalls_.at (0)) call) { return call.first == reply; });
		std::shared_ptr<void> eraseGuard (nullptr,
				[this, pos] (void*)
				{
					if (pos != RunningCalls_.end ())
						RunningCalls_.erase (pos);
				});

		if (reply->error () == QNetworkReply::NoError)
		{
			APIErrorCount_ = 0;
			return true;
		}

		if (pos != RunningCalls_.end ())
			PreparedCalls_.push_front (pos->second);
		else
			qWarning () << Q_FUNC_INFO
					<< "no running call found for the reply";

		++APIErrorCount_;

		if (!ShouldRerunPrepared_)
		{
			QTimer::singleShot (30000,
					this,
					SLOT (rerunPrepared ()));
			ShouldRerunPrepared_ = true;
		}

		return false;
	}

	void VkConnection::rerunPrepared ()
	{
		ShouldRerunPrepared_ = false;

		if (!PreparedCalls_.isEmpty ())
			AuthMgr_->GetAuthKey ();
	}

	void VkConnection::callWithKey (const QString& key)
	{
		while (!PreparedCalls_.isEmpty ())
		{
			auto f = PreparedCalls_.takeFirst ();
			CallQueue_->Schedule ([this, f, key] { RunningCalls_.append ({ f (key), f }); });
		}
	}

	void VkConnection::handleListening ()
	{
		CurrentStatus_ = Status_;
		emit statusChanged (GetStatus ());
	}

	void VkConnection::handlePollError ()
	{
		CurrentStatus_ = EntryStatus ();
		emit statusChanged (GetStatus ());
	}

	void VkConnection::handlePollStopped ()
	{
		CurrentStatus_ = Status_;
		emit statusChanged (GetStatus ());

		emit stoppedPolling ();
	}

	void VkConnection::handlePollData (const QVariantMap& rootMap)
	{
		for (const auto& update : rootMap ["updates"].toList ())
		{
			const auto& parmList = update.toList ();
			const auto code = parmList.value (0).toInt ();

			if (!Dispatcher_.contains (code))
				qWarning () << Q_FUNC_INFO
						<< "unknown code"
						<< code
						<< parmList;
			else
				Dispatcher_ [code] (parmList);
		}
	}

	void VkConnection::handleGotFriendLists ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		if (!CheckFinishedReply (reply))
			return;

		QList<ListInfo> lists;

		const auto& data = QJson::Parser ().parse (reply);
		for (const auto& item : data.toMap () ["response"].toList ())
		{
			const auto& map = item.toMap ();
			lists.append ({ map ["lid"].toULongLong (), map ["name"].toString () });
		}

		emit gotLists (lists);

		PushFriendsRequest ();
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::handleGotFriends ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		if (!CheckFinishedReply (reply))
			return;

		QList<UserInfo> users;

		const auto& data = QJson::Parser ().parse (reply);
		for (const auto& item : data.toMap () ["response"].toList ())
		{
			const auto& userMap = item.toMap ();
			if (userMap.contains ("deactivated"))
				continue;

			QList<qulonglong> lists;
			for (const auto& item : userMap ["lists"].toList ())
				lists << item.toULongLong ();

			auto dateString = userMap ["bdate"].toString ();
			if (dateString.count ('.') == 1)
				dateString += ".1800";

			const UserInfo ui
			{
				userMap ["uid"].toULongLong (),

				userMap ["first_name"].toString (),
				userMap ["last_name"].toString (),
				userMap ["nickname"].toString (),

				QUrl (userMap ["photo"].toString ()),
				QUrl (userMap ["photo_big"].toString ()),

				userMap ["sex"].toInt (),

				QDate::fromString (dateString, "d.M.yyyy"),

				userMap ["home_phone"].toString (),
				userMap ["mobile_phone"].toString (),

				userMap ["timezone"].toInt (),
				userMap ["country"].toInt (),
				userMap ["city"].toInt (),

				static_cast<bool> (userMap ["online"].toULongLong ()),

				lists
			};
			users << ui;
		}

		emit gotUsers (users);

		LPManager_->start ();
	}

	void VkConnection::handleMessageSent ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		if (!CheckFinishedReply (reply))
			return;

		const auto& setter = MsgReply2Setter_.take (reply);
		if (!setter)
			return;

		const auto& data = QJson::Parser ().parse (reply);
		const auto code = data.toMap ().value ("response", -1).toULongLong ();
		setter (code);
	}

	void VkConnection::handleCountriesFetched ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		if (!CheckFinishedReply (reply))
			return;

		const auto& setter = CountryReply2Setter_.take (reply);
		if (!setter)
			return;

		QHash<int, QString> result;

		const auto& data = QJson::Parser ().parse (reply);
		for (const auto& item : data.toMap () ["response"].toList ())
		{
			const auto& map = item.toMap ();
			result [map ["cid"].toInt ()] = map ["name"].toString ();
		}

		setter (result);
	}

	namespace
	{
		PhotoInfo PhotoMap2Info (const QVariantMap& map)
		{
			QString srcBig;
			for (auto str : { "src_xxxbig", "src_xxbig", "src_xbig", "src_big" })
				if (map.contains (str))
				{
					srcBig = map [str].toString ();
					break;
				}

			return
			{
				map ["owner_id"].toLongLong (),
				map ["pid"].toULongLong (),
				map ["aid"].toLongLong (),

				map ["src"].toString (),
				srcBig,

				map ["access_key"].toString ()
			};
		}

		AudioInfo AudioMap2Info (const QVariantMap& map)
		{
			return
			{
				map ["owner_id"].toLongLong (),
				map ["aid"].toULongLong (),
				map ["artist"].toString (),
				map ["title"].toString (),
				map ["duration"].toInt (),
				map ["url"].toString ()
			};
		}

		void HandleAttachments (FullMessageInfo& info, const QVariant& attachments)
		{
			const auto& attList = attachments.toList ();
			for (const auto& attVar : attList)
			{
				const auto& attMap = attVar.toMap ();
				if (attMap.contains ("photo"))
					info.Photos_ << PhotoMap2Info (attMap ["photo"].toMap ());
				else if (attMap.contains ("audio"))
					info.Audios_ << AudioMap2Info (attMap ["audio"].toMap ());
				else if (attMap.contains ("wall"))
				{
					const auto& wallMap = attMap ["wall"].toMap ();

					FullMessageInfo repost;
					repost.OwnerID_ = wallMap ["from_id"].toLongLong ();
					repost.ID_ = wallMap ["id"].toULongLong ();
					repost.Text_ = wallMap ["text"].toString ();
					repost.Likes_ = wallMap ["likes"].toMap () ["count"].toInt ();
					repost.Reposts_ = wallMap ["reposts"].toMap () ["count"].toInt ();
					repost.PostDate_ = QDateTime::fromTime_t (wallMap ["date"].toLongLong ());

					HandleAttachments (repost, wallMap ["attachments"]);

					info.ContainedReposts_.append (repost);
				}
			}
		}
	}

	void VkConnection::handleMessageInfoFetched ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		if (!CheckFinishedReply (reply))
			return;

		const auto& setter = Reply2MessageSetter_.take (reply);
		if (!setter)
			return;

		FullMessageInfo info;

		const auto& data = QJson::Parser ().parse (reply);
		const auto& infoList = data.toMap () ["response"].toList ();
		for (const auto& item : infoList)
		{
			if (item.type () != QVariant::Map)
				continue;

			const auto& map = item.toMap ();
			HandleAttachments (info, map ["attachments"]);
		}

		setter (info);
	}

	void VkConnection::handlePhotoInfosFetched ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		if (!CheckFinishedReply (reply))
			return;

		const auto& setter = Reply2PhotoSetter_.take (reply);
		if (!setter)
		{
			qWarning () << Q_FUNC_INFO
					<< "no setter";
			return;
		}

		QList<PhotoInfo> result;

		const auto& data = QJson::Parser ().parse (reply);
		for (const auto& item : data.toMap () ["response"].toList ())
		{
			const auto& map = item.toMap ();

			const auto& thumb = map ["src_small"].toString ();
			QString big;
			for (auto key : { "src_xxbig", "src_xbig", "src_big", "src" })
				if (map.contains (key))
				{
					big = map [key].toString ();
					break;
				}

			result.append ({
					map ["owner_id"].toLongLong (),
					map ["pid"].toULongLong (),
					map ["aid"].toLongLong (),

					thumb,
					big,

					{}
				});
		}

		setter (result);
	}

	void VkConnection::saveCookies (const QByteArray& cookies)
	{
		LastCookies_ = cookies;
		emit cookiesChanged ();
	}
}
}
}
