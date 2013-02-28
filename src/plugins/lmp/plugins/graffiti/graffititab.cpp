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

#include "graffititab.h"
#include <functional>
#include <QFileSystemModel>
#include <QToolBar>
#include <QMessageBox>
#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QtDebug>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <util/tags/tagscompletionmodel.h>
#include <util/tags/tagscompleter.h>
#include <interfaces/lmp/ilmpproxy.h>
#include <interfaces/lmp/itagresolver.h>
#include <interfaces/lmp/mediainfo.h>
#include "filesmodel.h"
#include "renamedialog.h"
#include "genres.h"
#include "fileswatcher.h"
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/media/itagsfetcher.h>

namespace LeechCraft
{
namespace LMP
{
namespace Graffiti
{
	GraffitiTab::GraffitiTab (ICoreProxy_ptr coreProxy, ILMPProxy_ptr proxy, const TabClassInfo& tc, QObject *plugin)
	: CoreProxy_ (coreProxy)
	, LMPProxy_ (proxy)
	, TC_ (tc)
	, Plugin_ (plugin)
	, FSModel_ (new QFileSystemModel (this))
	, FilesModel_ (new FilesModel (this))
	, FilesWatcher_ (new FilesWatcher (this))
	, Toolbar_ (new QToolBar ("Graffiti"))
	, IsChangingCurrent_ (false)
	{
		Ui_.setupUi (this);

		FSModel_->setRootPath (QDir::homePath ());
		FSModel_->setFilter (QDir::Dirs | QDir::NoDotAndDotDot);
		FSModel_->setReadOnly (true);
		Ui_.DirectoryTree_->setModel (FSModel_);

		auto idx = FSModel_->index (QDir::homePath ());
		while (idx.isValid ())
		{
			Ui_.DirectoryTree_->expand (idx);
			idx = idx.parent ();
		}

		Ui_.FilesList_->setModel (FilesModel_);

		connect (Ui_.FilesList_->selectionModel (),
				SIGNAL (currentRowChanged (QModelIndex, QModelIndex)),
				this,
				SLOT (currentFileChanged (QModelIndex)));

		Save_ = Toolbar_->addAction (tr ("Save"),
				this, SLOT (save ()));
		Save_->setProperty ("ActionIcon", "document-save");
		Save_->setShortcut (QString ("Ctrl+S"));

		Revert_ = Toolbar_->addAction (tr ("Revert"),
				this, SLOT (revert ()));
		Revert_->setProperty ("ActionIcon", "document-revert");

		Toolbar_->addSeparator ();

		RenameFiles_ = Toolbar_->addAction (tr ("Rename files"),
				this, SLOT (renameFiles ()));
		RenameFiles_->setProperty ("ActionIcon", "edit-rename");

		Toolbar_->addSeparator ();

		GetTags_ = Toolbar_->addAction (tr ("Fetch tags"),
				this, SLOT (fetchTags ()));
		GetTags_->setProperty ("ActionIcon", "download");

		Ui_.Genre_->SetSeparator (" / ");

		auto model = new Util::TagsCompletionModel (this);
		model->UpdateTags (Genres);
		auto completer = new Util::TagsCompleter (Ui_.Genre_, this);
		completer->OverrideModel (model);

		Ui_.Genre_->AddSelector ();

		connect (FilesWatcher_,
				SIGNAL (rereadFiles ()),
				this,
				SLOT (handleRereadFiles ()));
	}

	TabClassInfo GraffitiTab::GetTabClassInfo () const
	{
		return TC_;
	}

	QObject* GraffitiTab::ParentMultiTabs ()
	{
		return Plugin_;
	}

	void GraffitiTab::Remove ()
	{
		emit removeTab (this);
	}

	QToolBar* GraffitiTab::GetToolBar () const
	{
		return Toolbar_.get ();
	}

	template<typename T, typename F>
	void GraffitiTab::UpdateData (const T& newData, F getter)
	{
		if (IsChangingCurrent_)
			return;

		static_assert (std::is_lvalue_reference<typename std::result_of<F (MediaInfo&)>::type>::value,
				"functor doesn't return an lvalue reference");

		const auto& selected = Ui_.FilesList_->selectionModel ()->selectedRows ();
		for (const auto& index : selected)
		{
			const auto& infoData = index.data (FilesModel::Roles::MediaInfoRole);
			auto info = infoData.template value<MediaInfo> ();
			getter (info) = newData;
			FilesModel_->UpdateInfo (index, info);
		}

		if (!selected.isEmpty ())
		{
			Save_->setEnabled (true);
			Revert_->setEnabled (true);
		}
	}

	void GraffitiTab::on_Artist__textEdited (const QString& artist)
	{
		UpdateData (artist, [] (MediaInfo& info) -> QString& { return info.Artist_; });
	}

	void GraffitiTab::on_Album__textEdited (const QString& album)
	{
		UpdateData (album, [] (MediaInfo& info) -> QString& { return info.Album_; });
	}

	void GraffitiTab::on_Title__textEdited (const QString& title)
	{
		UpdateData (title, [] (MediaInfo& info) -> QString& { return info.Title_; });
	}

	void GraffitiTab::on_Genre__textChanged (const QString& genreString)
	{
		auto genres = genreString.split ('/', QString::SkipEmptyParts);
		for (auto& genre : genres)
			genre = genre.trimmed ();

		UpdateData (genres, [] (MediaInfo& info) -> QStringList& { return info.Genres_; });
	}

	void GraffitiTab::on_Year__valueChanged (int year)
	{
		UpdateData (year, [] (MediaInfo& info) -> int& { return info.Year_; });
	}

	void GraffitiTab::save ()
	{
		const auto& modified = FilesModel_->GetModified ();
		if (modified.isEmpty ())
			return;

		if (QMessageBox::question (this,
				"LMP Graffiti",
				tr ("Do you really want to accept changes to %n file(s)?", 0, modified.size ()),
				QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		ITagResolver *resolver = LMPProxy_->GetTagResolver ();

		auto toTLStr = [] (const QString& str)
		{
			return TagLib::String (str.toUtf8 ().constData (), TagLib::String::UTF8);
		};

		for (const auto& pair : modified)
		{
			const auto& newInfo = pair.first;

			QMutexLocker locker (&resolver->GetMutex ());
			auto file = resolver->GetFileRef (newInfo.LocalPath_);
			auto tag = file.tag ();

			tag->setArtist (toTLStr (newInfo.Artist_));
			tag->setAlbum (toTLStr (newInfo.Album_));
			tag->setTitle (toTLStr (newInfo.Title_));
			tag->setYear (newInfo.Year_);
			tag->setGenre (toTLStr (newInfo.Genres_.join (" / ")));

			if (!file.save ())
				qWarning () << Q_FUNC_INFO
						<< "unable to save file"
						<< newInfo.LocalPath_;
		}

		handleRereadFiles ();
	}

	void GraffitiTab::revert ()
	{
		const auto& modified = FilesModel_->GetModified ();
		if (modified.isEmpty ())
			return;

		if (QMessageBox::question (this,
				"LMP Graffiti",
				tr ("Do you really want to revert changes to %n file(s)?", 0, modified.size ()),
				QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		QList<MediaInfo> origs;
		for (const auto& pair : modified)
			origs << pair.second;
		FilesModel_->SetInfos (origs);

		Save_->setEnabled (false);
		Revert_->setEnabled (false);

		currentFileChanged (Ui_.FilesList_->currentIndex ());
	}

	void GraffitiTab::renameFiles ()
	{
		if (!FilesModel_->GetModified ().isEmpty ())
		{
			auto res = QMessageBox::question (this,
					"LMP Graffiti",
					tr ("You have unsaved files with changed tags. Do you want to save or discard those changes?"),
					QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
			if (res == QMessageBox::Save)
				save ();
			else if (res == QMessageBox::Discard)
				revert ();
			else
				return;
		}

		QList<MediaInfo> infos;
		for (const auto& index : Ui_.FilesList_->selectionModel ()->selectedRows ())
			infos << index.data (FilesModel::Roles::MediaInfoRole).value<MediaInfo> ();
		if (infos.isEmpty ())
			return;

		auto dia = new RenameDialog (LMPProxy_, this);
		dia->SetInfos (infos);

		dia->setAttribute (Qt::WA_DeleteOnClose);
		dia->show ();
	}

	void GraffitiTab::fetchTags ()
	{
		auto provs = CoreProxy_->GetPluginsManager ()->GetAllCastableTo<Media::ITagsFetcher*> ();
		if (provs.isEmpty ())
			return;

		auto prov = provs.first ();

		for (const auto& index : Ui_.FilesList_->selectionModel ()->selectedRows ())
		{
			const auto& info = index.data (FilesModel::Roles::MediaInfoRole).value<MediaInfo> ();
			auto pending = prov->FetchTags (info.LocalPath_);
			connect (pending->GetObject (),
					SIGNAL (ready (QString, Media::AudioInfo)),
					this,
					SLOT (handleTagsFetched (QString, Media::AudioInfo)));
		}
	}

	void GraffitiTab::handleTagsFetched (const QString& filename, const Media::AudioInfo& info)
	{
		qDebug () << Q_FUNC_INFO;
	}

	void GraffitiTab::on_DirectoryTree__activated (const QModelIndex& index)
	{
		setEnabled (false);
		FilesModel_->Clear ();
		FilesWatcher_->Clear ();

		const auto& path = FSModel_->filePath (index);

		auto watcher = new QFutureWatcher<QList<QFileInfo>> ();
		connect (watcher,
				SIGNAL (finished ()),
				this,
				SLOT (handleIterateFinished ()));

		auto worker = [this, path] () { return LMPProxy_->RecIterateInfo (path, true); };
		watcher->setFuture (QtConcurrent::run (std::function<QList<QFileInfo> ()> (worker)));
	}

	void GraffitiTab::currentFileChanged (const QModelIndex& index)
	{
		const auto& infoData = FilesModel_->data (index, FilesModel::Roles::MediaInfoRole);
		const auto& info = infoData.value<MediaInfo> ();

		IsChangingCurrent_ = true;

		Ui_.Album_->setText (info.Album_);
		Ui_.Artist_->setText (info.Artist_);
		Ui_.Title_->setText (info.Title_);
		Ui_.Genre_->setText (info.Genres_.join (" / "));

		Ui_.Year_->setValue (info.Year_);

		IsChangingCurrent_ = false;
	}

	void GraffitiTab::handleRereadFiles ()
	{
		const auto& current = Ui_.DirectoryTree_->currentIndex ();
		on_DirectoryTree__activated (current);
	}

	void GraffitiTab::handleIterateFinished ()
	{
		auto watcher = dynamic_cast<QFutureWatcher<QList<QFileInfo>>*> (sender ());
		watcher->deleteLater ();

		const auto& files = watcher->result ();

		FilesWatcher_->AddFiles (files);
		FilesModel_->AddFiles (files);

		auto resolver = LMPProxy_->GetTagResolver ();
		auto worker = [resolver, files] () -> QList<MediaInfo>
		{
			QList<MediaInfo> infos;
			for (const auto& file : files)
				try
				{
					infos << resolver->ResolveInfo (file.absoluteFilePath ());
				}
				catch (const std::exception& e)
				{
					qWarning () << Q_FUNC_INFO
							<< e.what ();
				}
			return infos;
		};

		auto scanWatcher = new QFutureWatcher<QList<MediaInfo>> ();
		connect (scanWatcher,
				SIGNAL (finished ()),
				this,
				SLOT (handleScanFinished ()));
		scanWatcher->setFuture (QtConcurrent::run (std::function<QList<MediaInfo> ()> (worker)));
	}

	void GraffitiTab::handleScanFinished ()
	{
		auto watcher = dynamic_cast<QFutureWatcher<QList<MediaInfo>>*> (sender ());
		watcher->deleteLater ();

		FilesModel_->SetInfos (watcher->result ());
		setEnabled (true);
	}
}
}
}
