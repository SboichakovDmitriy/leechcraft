/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2013  Oleg Linkin <MaledictusDeMagog@gmail.com>
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

#include "picasamanager.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardItemModel>
#include <QtDebug>
#include <qjson/parser.h>
#include <qjson/serializer.h>
#include "picasaaccount.h"

namespace LeechCraft
{
namespace Blasq
{
namespace Vangog
{
	PicasaManager::PicasaManager (PicasaAccount *account, QObject *parent)
	: QObject (parent)
	, Account_ (account)
	{
	}

	void PicasaManager::UpdateCollections ()
	{
		ApiCallsQueue_ << [this] (const QString& key) { RequestCollections (key); };
		RequestAccessToken ();
	}

	void PicasaManager::UpdatePhotos (const QByteArray& albumId)
	{
		ApiCallsQueue_ << [this, albumId] (const QString& key) { RequestPhotos (albumId, key); };
	}

	void PicasaManager::RequestAccessToken ()
	{
		QNetworkRequest request (QUrl ("https://accounts.google.com/o/oauth2/token"));
		QString str = QString ("refresh_token=%1&client_id=%2&client_secret=%3&grant_type=%4")
				.arg (Account_->GetRefreshToken ())
				.arg ("844868161425.apps.googleusercontent.com")
				.arg ("l09HkM6nbPMEYcMdcdeGBdaV")
				.arg ("refresh_token");

		request.setHeader (QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

		QNetworkReply *reply = Account_->GetProxy ()->
				GetNetworkAccessManager ()->post (request, str.toUtf8 ());

		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (handleAuthTokenRequestFinished ()));
	}

	void PicasaManager::ParseError (const QVariantMap& map)
	{

	}

	namespace
	{
		QNetworkRequest CreateRequest (const QUrl& url)
		{
			QNetworkRequest request (url);
			request.setRawHeader ("GData-Version", "2");

			return request;
		}
	}

	void PicasaManager::RequestCollections (const QString& key)
	{
		QString urlStr = QString ("https://picasaweb.google.com/data/feed/api/user/%1?access_token=%2&access=all")
				.arg (Account_->GetLogin ())
				.arg (key);
		QNetworkReply *reply = Account_->GetProxy ()->GetNetworkAccessManager ()->
				get (CreateRequest (QUrl (urlStr)));

		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (handleRequestCollectionFinished ()));
	}

	void PicasaManager::RequestPhotos (const QByteArray& albumId, const QString& key)
	{
		QString urlStr = QString ("https://picasaweb.google.com/data/feed/api/user/%1/albumid/%2?access_token=%3&imgmax=d")
				.arg (Account_->GetLogin ())
				.arg (QString::fromUtf8 (albumId))
				.arg (key);
		QNetworkReply *reply = Account_->GetProxy ()->GetNetworkAccessManager ()->
				get (CreateRequest (QUrl (urlStr)));

		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (handleRequestPhotosFinished ()));
	}

	void PicasaManager::handleAuthTokenRequestFinished ()
	{
		QNetworkReply *reply = qobject_cast<QNetworkReply*> (sender ());
		if (!reply)
			return;

		reply->deleteLater ();

		bool ok = false;
		QVariant res = QJson::Parser ().parse (reply->readAll (), &ok);

		if (!ok)
		{
			qWarning () << Q_FUNC_INFO << "parse error";
			return;
		}

		QString accessKey = res.toMap ().value ("access_token").toString ();
		qDebug () << accessKey;
		if (accessKey.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO << "access token is empty";
			return;
		}

		if (ApiCallsQueue_.isEmpty ())
			return;

		ApiCallsQueue_.dequeue () (accessKey);
	}

	namespace
	{
		QByteArray CreateDomDocumentFromReply (QNetworkReply *reply, QDomDocument &document)
		{
			if (!reply)
				return QByteArray ();

			const auto& content = reply->readAll ();
			reply->deleteLater ();
			QString errorMsg;
			int errorLine = -1, errorColumn = -1;
			if (!document.setContent (content, &errorMsg, &errorLine, &errorColumn))
			{
				qWarning () << Q_FUNC_INFO
						<< errorMsg
						<< "in line:"
						<< errorLine
						<< "column:"
						<< errorColumn;
				return QByteArray ();
			}

			return content;
		}
	}

	namespace
	{
		Access PicasaRightsToAccess (const QString& rights)
		{
			if (rights == "protected" || rights == "private")
				return Access::Private;
			else
				return Access::Public;
		}

		Author PicasaAuthorToAuthor (const QDomElement& elem)
		{
			Author author;

			const auto& fields = elem.childNodes ();
			for (int i = 0, size = fields.size (); i < size; ++i)
			{
				const auto& field = fields.at (i).toElement ();
				const QString& name = field.tagName ();
				const QString& value = field.text ();
				if (name == "name")
					author.Name_ = value;
				else if (name == "uri")
					author.Image_ = value;
			}

			return author;
		}

		Thumbnail PicasaMediaGroupToThumbnail (const QDomElement& elem)
		{
			Thumbnail thumbnail;
			const auto& fields = elem.childNodes ();
			for (int i = 0, count = fields.size (); i < count; ++i)
			{
				const auto& field = fields.at (i).toElement ();
				const QString& name = field.tagName ();
				if (name == "media:thumbnail")
				{
					thumbnail.Height_ = field.attribute ("height").toInt ();
					thumbnail.Width_ = field.attribute ("width").toInt ();
					thumbnail.Url_ = QUrl (field.attribute ("url"));
					break;
				}
			}

			return thumbnail;
		}

		Album PicasaEntryToAlbum (const QDomElement& elem)
		{
			Album album;

			const auto& fields = elem.childNodes ();
			for (int i = 0, count = fields.size (); i < count; ++i)
			{
				const auto& field = fields.at (i).toElement ();
				const QString& name = field.tagName ();
				const QString& value = field.text ();
				if (name == "published")
					album.Published_ = QDateTime::fromString (value, Qt::ISODate);
				else if (name == "updated")
					album.Updated_ = QDateTime::fromString (value, Qt::ISODate);
				else if (name == "title")
					album.Title_ = value;
				else if (name == "rights")
					album.Access_ = PicasaRightsToAccess (value);
				else if (name == "author")
					album.Author_ = PicasaAuthorToAuthor (field);
				else if (name == "gphoto:id")
					album.ID_ = value.toUtf8 ();
				else if (name == "gphoto:numphotos")
					album.NumberOfPhoto_ = value.toInt ();
				else if (name == "gphoto:bytesUsed")
					album.BytesUsed_ = value.toULongLong ();
				else if (name == "media:group")
					//TODO check is it possible to more then one thumbnail
					album.Thumbnails_ << PicasaMediaGroupToThumbnail (field);
			}

			return album;
		}
	}

	QList<Album> PicasaManager::ParseAlbums (const QDomDocument& document)
	{
		QList<Album> albums;
		const auto& entryElements = document.elementsByTagName ("entry");
		if (entryElements.isEmpty ())
			return albums;

		for (int i = 0, count = entryElements.count (); i < count; ++i)
		{
			const auto& album = PicasaEntryToAlbum (entryElements.at (i).toElement ());
			UpdatePhotos (album.ID_);
			albums << album;
		}

		return albums;
	}

	void PicasaManager::handleRequestCollectionFinished ()
	{
		QNetworkReply *reply = qobject_cast<QNetworkReply*> (sender ());
		if (!reply)
			return;

		QDomDocument document;
		if (CreateDomDocumentFromReply (reply, document).isEmpty ())
			return;

		auto albums = ParseAlbums (document);

		emit gotAlbums (albums);
		RequestAccessToken ();
	}

	namespace
	{
		Exif PicasaExifTagsToExif (const QDomElement& element)
		{
			Exif exif;
			const auto& fields = element.childNodes ();
			for (int i = 0, count = fields.size (); i < count; ++i)
			{
				const auto& field = fields.at (i).toElement ();
				const QString& name = field.tagName ();
				const QString& value = field.text ();
				if (name == "exif:fstop")
					exif.FNumber_ = value.toInt ();
				else if (name == "exif:make")
					exif.Manufacturer_ = value;
				else if (name == "exif:model")
					exif.Model_ = value;
				else if (name == "exif:exposure")
					exif.Exposure_ = value.toFloat ();
				else if (name == "exif:flash")
					exif.Flash_ = (value == "true");
				else if (name == "exif:focallength")
					exif.FocalLength_ = value.toFloat ();
				else if (name == "exif:iso")
					exif.ISO_ = value.toInt ();
			}
			return exif;
		}

		Photo PicasaEntryToPhoto (const QDomElement& elem)
		{
			Photo photo;
			const auto& fields = elem.childNodes ();
			QDomElement mediaGroupElement;
			for (int i = 0, count = fields.size (); i < count; ++i)
			{
				const auto& field = fields.at (i).toElement ();
				const QString& name = field.tagName ();
				const QString& value = field.text ();
				if (name == "title")
					photo.Title_ = value;
				else if (name == "published")
					photo.Published_ = QDateTime::fromString (value, Qt::ISODate);
				else if (name == "updated")
					photo.Updated_ = QDateTime::fromString (value, Qt::ISODate);
				else if (name == "gphoto:id")
					photo.ID_ = value.toUtf8 ();
				else if (name == "gphoto:albumid")
					photo.AlbumID_ = value.toUtf8 ();
				else if (name == "gphoto:access")
					photo.Access_ = PicasaRightsToAccess (value);
				else if (name == "gphoto:width")
					photo.Width_ = value.toInt ();
				else if (name == "gphoto:height")
					photo.Height_ = value.toInt ();
				else if (name == "gphoto:size")
					photo.Size_ = value.toULongLong ();
				else if (name == "exif:tags")
					photo.Exif_ = PicasaExifTagsToExif (field);
				else if (name == "media:group")
				{
					const auto& mgFields = field.childNodes ();
					for (int i = 0, count = mgFields.size (); i < count; ++i)
					{
						const auto& mgField = mgFields.at (i).toElement ();
						const auto& mgName = mgFields.at (i).toElement ().tagName ();
						const auto& mgValue = mgFields.at (i).toElement ().text ();
						if (mgName == "media:content")
						{
							photo.Height_ = mgField.attribute ("height").toInt ();
							photo.Width_ = mgField.attribute ("width").toInt ();
							photo.Url_ = QUrl (mgField.attribute ("url"));
						}
						else if (mgName == "media:keywords")
							photo.Tags_ = mgValue.split (',');
						else if (mgName == "media:thumbnail")
						{
							Thumbnail thumbnail;
							thumbnail.Height_ = mgField.attribute ("height").toInt ();
							thumbnail.Width_ = mgField.attribute ("width").toInt ();
							thumbnail.Url_ = QUrl (mgField.attribute ("url"));
							photo.Thumbnails_ << thumbnail;
						}
					}
				}
			}

			return photo;
		}

		QList<Photo> ParsePhotos (const QDomDocument& document)
		{
			QList<Photo> photos;
			const auto& EntryElements = document.elementsByTagName ("entry");
			if (EntryElements.isEmpty ())
				return photos;
			for (int i = 0, count = EntryElements.count (); i < count; ++i)
				photos << PicasaEntryToPhoto (EntryElements.at (i).toElement ());;

			return photos;
		}
	}

	void PicasaManager::handleRequestPhotosFinished ()
	{
		QNetworkReply *reply = qobject_cast<QNetworkReply*> (sender ());
		if (!reply)
			return;

		QDomDocument document;
		if (CreateDomDocumentFromReply (reply, document).isEmpty ())
			return;

		emit gotPhotos (ParsePhotos (document));
		RequestAccessToken ();
	}

}
}
}