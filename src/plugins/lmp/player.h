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

#pragma once

#include <QObject>
#include <phonon/mediasource.h>
#include <phonon/path.h>
#include <interfaces/media/iradiostation.h>
#include "mediainfo.h"

class QModelIndex;
class QStandardItem;
class QAbstractItemModel;
class QStandardItemModel;

namespace Phonon
{
	class MediaObject;
	class AudioOutput;
}

namespace LeechCraft
{
namespace LMP
{
	struct MediaInfo;

	class Player : public QObject
	{
		Q_OBJECT

		QStandardItemModel *PlaylistModel_;
		Phonon::MediaObject *Source_;
		Phonon::AudioOutput *Output_;
		Phonon::Path Path_;

		QList<Phonon::MediaSource> CurrentQueue_;
		QHash<Phonon::MediaSource, QStandardItem*> Items_;
		QHash<QPair<QString, QString>, QStandardItem*> AlbumRoots_;

		Phonon::MediaSource CurrentStopSource_;

		Media::IRadioStation_ptr CurrentStation_;
		QStandardItem *RadioItem_;
		QHash<QUrl, MediaInfo> Url2Info_;
	public:
		enum class PlayMode
		{
			Sequential,
			Shuffle,
			RepeatTrack,
			RepeatAlbum,
			RepeatWhole
		};
	private:
		PlayMode PlayMode_;
	public:
		enum Role
		{
			IsCurrent = Qt::UserRole + 1,
			IsStop,
			IsAlbum,
			Source,
			Info,
			AlbumArt,
			AlbumLength
		};

		Player (QObject* = 0);

		QAbstractItemModel* GetPlaylistModel () const;
		Phonon::MediaObject* GetSourceObject () const;
		Phonon::AudioOutput* GetAudioOutput () const;

		void SetPlayMode (PlayMode);

		void Enqueue (const QStringList&, bool = true);
		void Enqueue (const QList<Phonon::MediaSource>&, bool = true);
		QList<Phonon::MediaSource> GetQueue () const;

		void Dequeue (const QModelIndex&);

		void SetStopAfter (const QModelIndex&);

		void SetRadioStation (Media::IRadioStation_ptr);
	private:
		MediaInfo GetMediaInfo (const Phonon::MediaSource&) const;
		void AddToPlaylistModel (QList<Phonon::MediaSource>, bool);
		void ApplyOrdering (QList<Phonon::MediaSource>&);

		bool HandleCurrentStop (const Phonon::MediaSource&);

		void UnsetRadio ();
	public slots:
		void play (const QModelIndex&);
		void previousTrack ();
		void nextTrack ();
		void togglePause ();
		void stop ();
		void clear ();
	private slots:
		void restorePlaylist ();
		void handleStationError (const QString&);
		void handleRadioStream (const QUrl&, const Media::AudioInfo&);
		void handleUpdateSourceQueue ();
		void handlePlaybackFinished ();
		void handleStateChanged (Phonon::State);
		void handleCurrentSourceChanged (const Phonon::MediaSource&);
	signals:
		void songChanged (const MediaInfo&);
		void insertedAlbum (const QModelIndex&);
	};
}
}
