/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2009  Georg Rudoy
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

#include <limits>
#include <stdexcept>
#include <list>
#include <functional>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <QMainWindow>
#include <QMessageBox>
#include <QtDebug>
#include <QTimer>
#include <QNetworkProxy>
#include <QApplication>
#include <QAction>
#include <QToolBar>
#include <QKeyEvent>
#include <QDir>
#include <QTextCodec>
#include <QDesktopServices>
#include <QNetworkReply>
#include <QAbstractNetworkCache>
#include <QClipboard>
#include <plugininterface/util.h>
#include <plugininterface/customcookiejar.h>
#include <interfaces/iinfo.h>
#include <interfaces/idownload.h>
#include <interfaces/ientityhandler.h>
#include <interfaces/ijobholder.h>
#include <interfaces/iembedtab.h>
#include <interfaces/imultitabs.h>
#include <interfaces/ihavesettings.h>
#include <interfaces/ihaveshortcuts.h>
#include <interfaces/iwindow.h>
#include <interfaces/itoolbarembedder.h>
#include <interfaces/structures.h>
#include "application.h"
#include "mainwindow.h"
#include "pluginmanager.h"
#include "core.h"
#include "xmlsettingsmanager.h"
#include "mergemodel.h"
#include "sqlstoragebackend.h"
#include "requestparser.h"
#include "handlerchoicedialog.h"
#include "tagsmanager.h"
#include "tabcontentsmanager.h"

using namespace LeechCraft;
using namespace LeechCraft::Util;

HookProxy::HookProxy ()
: Cancelled_ (false)
{
}

HookProxy::~HookProxy ()
{
}

void HookProxy::CancelDefault ()
{
	Cancelled_ = true;
}

bool HookProxy::IsCancelled () const
{
	return Cancelled_;
}

LeechCraft::Core::Core ()
: MergeModel_ (new MergeModel (QStringList (tr ("Name"))
			<< tr ("State")
			<< tr ("Progress")))
, NetworkAccessManager_ (new NetworkAccessManager)
, StorageBackend_ (new SQLStorageBackend)
, DirectoryWatcher_ (new DirectoryWatcher)
{
	MergeModel_->setObjectName ("Core MergeModel");
	MergeModel_->setProperty ("__LeechCraft_own_core_model", true);
	connect (NetworkAccessManager_.get (),
			SIGNAL (error (const QString&)),
			this,
			SIGNAL (error (const QString&)));

	connect (DirectoryWatcher_.get (),
			SIGNAL (gotEntity (const LeechCraft::DownloadEntity&)),
			this,
			SLOT (handleGotEntity (LeechCraft::DownloadEntity)));

	StorageBackend_->Prepare ();

	PluginManager_ = new PluginManager (this);

	ClipboardWatchdog_ = new QTimer (this);
	connect (ClipboardWatchdog_,
			SIGNAL (timeout ()),
			this,
			SLOT (handleClipboardTimer ()));
	ClipboardWatchdog_->start (2000);

	QList<QByteArray> proxyProperties;
	proxyProperties << "ProxyEnabled"
		<< "ProxyHost"
		<< "ProxyPort"
		<< "ProxyLogin"
		<< "ProxyPassword"
		<< "ProxyType";
	XmlSettingsManager::Instance ()->RegisterObject (proxyProperties,
			this, "handleProxySettings");

	handleProxySettings ();
}

LeechCraft::Core::~Core ()
{
}

Core& LeechCraft::Core::Instance ()
{
	static Core core;
	return core;
}

void LeechCraft::Core::Release ()
{
	LocalSocketHandler_.reset ();
	XmlSettingsManager::Instance ()->setProperty ("FirstStart", "false");
	DirectoryWatcher_.reset ();
	MergeModel_.reset ();

	PluginManager_->Release ();
	delete PluginManager_;
	ClipboardWatchdog_->stop ();
	delete ClipboardWatchdog_;

	NetworkAccessManager_.reset ();

	StorageBackend_.reset ();
}

void LeechCraft::Core::SetReallyMainWindow (MainWindow *win)
{
	ReallyMainWindow_ = win;
	ReallyMainWindow_->GetTabWidget ()->installEventFilter (this);
	ReallyMainWindow_->installEventFilter (this);
}

MainWindow* LeechCraft::Core::GetReallyMainWindow ()
{
	return ReallyMainWindow_;
}

const IShortcutProxy* LeechCraft::Core::GetShortcutProxy () const
{
	return ReallyMainWindow_->GetShortcutProxy ();
}

QObjectList LeechCraft::Core::GetSettables () const
{
	return PluginManager_->GetAllCastableRoots<IHaveSettings*> ();
}

QObjectList LeechCraft::Core::GetShortcuts () const
{
	return PluginManager_->GetAllCastableRoots<IHaveShortcuts*> ();
}

QList<QList<QAction*> > LeechCraft::Core::GetActions2Embed () const
{
	QList<IToolBarEmbedder*> plugins = PluginManager_->GetAllCastableTo<IToolBarEmbedder*> ();
	QList<QList<QAction*> > actions;
	Q_FOREACH (IToolBarEmbedder *plugin, plugins)
		actions << plugin->GetActions ();
	return actions;
}

QAbstractItemModel* LeechCraft::Core::GetPluginsModel () const
{
	return PluginManager_;
}

QAbstractItemModel* LeechCraft::Core::GetTasksModel (const QString& text) const
{
	RequestNormalizer *rm = new RequestNormalizer (MergeModel_);
	rm->SetRequest (text);
	return rm->GetModel ();
}

PluginManager* LeechCraft::Core::GetPluginManager () const
{
	return PluginManager_;
}

StorageBackend* LeechCraft::Core::GetStorageBackend () const
{
	return StorageBackend_.get ();
}

QToolBar* LeechCraft::Core::GetToolBar (int index) const
{
	return TabContainer_->GetToolBar (index);
}

QToolBar* LeechCraft::Core::GetControls (const QModelIndex& index) const
{
	if (!index.isValid ())
		return 0;

	QVariant data = index.data (RoleControls);
	return data.value<QToolBar*> ();
}

QWidget* LeechCraft::Core::GetAdditionalInfo (const QModelIndex& index) const
{
	if (!index.isValid ())
		return 0;

	QVariant data = index.data (RoleAdditionalInfo);
	return data.value<QWidget*> ();
}

QStringList LeechCraft::Core::GetTagsForIndex (int index,
		QAbstractItemModel *model) const
{
	MergeModel::const_iterator modIter =
		dynamic_cast<MergeModel*> (model)->GetModelForRow (index);

	int starting = dynamic_cast<MergeModel*> (model)->
		GetStartingRow (modIter);

	QStringList ids = (*modIter)->data ((*modIter)->
			index (index - starting, 0), RoleTags).toStringList ();
	QStringList result;
	Q_FOREACH (QString id, ids)
		result << TagsManager::Instance ().GetTag (id);
	return result;
}

void LeechCraft::Core::Setup (QObject *plugin)
{
	IJobHolder *ijh = qobject_cast<IJobHolder*> (plugin);
	IEmbedTab *iet = qobject_cast<IEmbedTab*> (plugin);
	IMultiTabs *imt = qobject_cast<IMultiTabs*> (plugin);

	InitDynamicSignals (plugin);

	if (ijh)
		InitJobHolder (plugin);

	if (iet)
		connect (plugin,
				SIGNAL (bringToFront ()),
				this,
				SLOT (embeddedTabWantsToFront ()));

	if (imt)
		InitMultiTab (plugin);
}

void LeechCraft::Core::DelayedInit ()
{
	connect (this,
			SIGNAL (error (QString)),
			ReallyMainWindow_,
			SLOT (catchError (QString)));

	TabContainer_.reset (new TabContainer (ReallyMainWindow_->GetTabWidget ()));

	PluginManager_->Init ();

	QObjectList plugins = PluginManager_->GetAllPlugins ();
	foreach (QObject *plugin, plugins)
	{
		IEmbedTab *iet = qobject_cast<IEmbedTab*> (plugin);
		if (iet)
			InitEmbedTab (plugin);
	}

	InitMultiTab (&TabContentsManager::Instance ());

	TabContainer_->handleTabNames ();

	LocalSocketHandler_.reset (new LocalSocketHandler (ReallyMainWindow_));
	connect (LocalSocketHandler_.get (),
			SIGNAL (gotEntity (const LeechCraft::DownloadEntity&)),
			this,
			SLOT (handleGotEntity (const LeechCraft::DownloadEntity&)));

	QTimer::singleShot (1000,
			LocalSocketHandler_.get (),
			SLOT (pullCommandLine ()));
}

void LeechCraft::Core::TryToAddJob (QString name, QString where)
{
	HookProxy_ptr proxy (new HookProxy);
	Q_FOREACH (HookSignature<HIDManualJobAddition>::Signature_t f,
			GetHooks<HIDManualJobAddition> ())
	{
		f (proxy, &name, &where);

		if (proxy->IsCancelled ())
			return;
	}

	DownloadEntity e;
	if (QFile::exists (name))
		e.Entity_ = QUrl::fromLocalFile (name);
	else
	{
		QUrl url (name);
		if (url.isValid ())
			e.Entity_ = url;
		else
			e.Entity_ = name;
	}
	e.Location_ = where;
	e.Parameters_ = FromUserInitiated;

	if (!handleGotEntity (e))
		emit error (tr ("No plugins are able to download \"%1\"").arg (name));
}

bool LeechCraft::Core::SameModel (const QModelIndex& i1, const QModelIndex& i2) const
{
	QModelIndex mapped1 = MapToSourceRecursively (i1);
	QModelIndex mapped2 = MapToSourceRecursively (i2);
	return mapped1.model () == mapped2.model ();
}

QPair<qint64, qint64> LeechCraft::Core::GetSpeeds () const
{
	qint64 download = 0;
	qint64 upload = 0;
	QObjectList plugins = PluginManager_->GetAllPlugins ();
	foreach (QObject *plugin, plugins)
	{
		IDownload *di = qobject_cast<IDownload*> (plugin);
		if (di)
		{
			try
			{
				download += di->GetDownloadSpeed ();
				upload += di->GetUploadSpeed ();
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
					<< "unable to get speeds"
					<< e.what ()
					<< plugin;
			}
			catch (...)
			{
				qWarning () << Q_FUNC_INFO
					<< "unable to get speeds"
					<< plugin;
			}
		}
	}

	return QPair<qint64, qint64> (download, upload);
}

int LeechCraft::Core::CountUnremoveableTabs () const
{
	// + 1 because of tabs with downloaders
	return PluginManager_->GetAllCastableTo<IEmbedTab*> ().size () + 1;
}

QNetworkAccessManager* LeechCraft::Core::GetNetworkAccessManager () const
{
	return NetworkAccessManager_.get ();
}

QModelIndex LeechCraft::Core::MapToSource (const QModelIndex& index) const
{
	return MapToSourceRecursively (index);
}

#define LC_APPENDER(a) a##_.Functors_.append (functor)
#define LC_GETTER(a) a##_.Functors_
#define LC_DEFINE_REGISTER(a) \
void LeechCraft::Core::RegisterHook (LeechCraft::HookSignature<a>::Signature_t functor) \
{ \
	LC_APPENDER(a); \
} \
template<> \
	LeechCraft::HooksContainer<LeechCraft::a>::Functors_t LeechCraft::Core::GetHooks<a> () const \
{ \
	return LC_GETTER(a); \
}
#define LC_TRAVERSER(z,i,array) LC_DEFINE_REGISTER (BOOST_PP_SEQ_ELEM(i, array))
#define LC_EXPANDER(Names) BOOST_PP_REPEAT (BOOST_PP_SEQ_SIZE (Names), LC_TRAVERSER, Names)
	LC_EXPANDER (HOOKS_TYPES_LIST);
#undef LC_EXPANDER
#undef LC_TRAVERSER
#undef LC_DEFINE_REGISTER
#undef LC_GETTER
#undef LC_APPENDER

bool LeechCraft::Core::eventFilter (QObject *watched, QEvent *e)
{
	if (ReallyMainWindow_ &&
			watched == ReallyMainWindow_->GetTabWidget ())
	{
		if (e->type () == QEvent::KeyRelease)
		{
			QKeyEvent *key = static_cast<QKeyEvent*> (e);
			bool handled = false;

			if (key->modifiers () & Qt::ControlModifier)
			{
				if (key->key () == Qt::Key_BracketLeft)
				{
					TabContainer_->RotateLeft ();
					handled = true;
				}
				else if (key->key () == Qt::Key_BracketRight)
				{
					TabContainer_->RotateRight ();
					handled = true;
				}
			}
			if (handled)
				return true;
			else
				TabContainer_->ForwardKeyboard (key);
		}
	}
	else if (ReallyMainWindow_ &&
			watched == ReallyMainWindow_)
	{
		if (e->type () == QEvent::DragEnter)
		{
			QDragEnterEvent *event = static_cast<QDragEnterEvent*> (e);

			Q_FOREACH (QString format, event->mimeData ()->formats ())
			{
				DownloadEntity e = Util::MakeEntity (event->
							mimeData ()->data (format),
						QString (),
						LeechCraft::FromUserInitiated,
						format);

				if (CouldHandle (e))
				{
					event->acceptProposedAction ();
					break;
				}
			}

			return true;
		}
		else if (e->type () == QEvent::Drop)
		{
			QDropEvent *event = static_cast<QDropEvent*> (e);

			Q_FOREACH (QString format, event->mimeData ()->formats ())
			{
				DownloadEntity e = Util::MakeEntity (event->
							mimeData ()->data (format),
						QString (),
						LeechCraft::FromUserInitiated,
						format);

				if (handleGotEntity (e))
				{
					event->acceptProposedAction ();
					break;
				}
			}

			return true;
		}
	}
	return QObject::eventFilter (watched, e);
}

void LeechCraft::Core::handleProxySettings () const
{
	bool enabled = XmlSettingsManager::Instance ()->property ("ProxyEnabled").toBool ();
	QNetworkProxy pr;
	if (enabled)
	{
		pr.setHostName (XmlSettingsManager::Instance ()->property ("ProxyHost").toString ());
		pr.setPort (XmlSettingsManager::Instance ()->property ("ProxyPort").toInt ());
		pr.setUser (XmlSettingsManager::Instance ()->property ("ProxyLogin").toString ());
		pr.setPassword (XmlSettingsManager::Instance ()->property ("ProxyPassword").toString ());
		QString type = XmlSettingsManager::Instance ()->property ("ProxyType").toString ();
		QNetworkProxy::ProxyType pt = QNetworkProxy::HttpProxy;
		if (type == "socks5")
			pt = QNetworkProxy::Socks5Proxy;
		else if (type == "tphttp")
			pt = QNetworkProxy::HttpProxy;
		else if (type == "chttp")
			pr = QNetworkProxy::HttpCachingProxy;
		else if (type == "cftp")
			pr = QNetworkProxy::FtpCachingProxy;
		pr.setType (pt);
	}
	else
		pr.setType (QNetworkProxy::NoProxy);
	QNetworkProxy::setApplicationProxy (pr);
	NetworkAccessManager_->setProxy (pr);
}

void LeechCraft::Core::handleSettingClicked (const QString& name)
{
	if (name == "ClearCache")
	{
		if (QMessageBox::question (0,
					tr ("LeechCraft"),
					tr ("Do you really want to clear the network cache?"),
					QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
			return;

		QAbstractNetworkCache *cache = NetworkAccessManager_->cache ();
		if (cache)
			cache->clear ();
	}
	else if (name == "ClearCookies")
	{
		if (QMessageBox::question (0,
					tr ("LeechCraft"),
					tr ("Do you really want to clear cookies?"),
					QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
			return;

		CustomCookieJar *jar = static_cast<CustomCookieJar*> (NetworkAccessManager_->cookieJar ());
		jar->setAllCookies (QList<QNetworkCookie> ());
		jar->Save ();
	}
}

bool LeechCraft::Core::CouldHandle (const LeechCraft::DownloadEntity& e) const
{
	HookProxy_ptr proxy (new HookProxy);
	Q_FOREACH (HookSignature<HIDCouldHandle>::Signature_t f,
			GetHooks<HIDCouldHandle> ())
	{
		bool result = f (proxy, e);

		if (proxy->IsCancelled ())
			return result;
	}

	if (!(e.Parameters_ & LeechCraft::OnlyHandle))
		if (GetObjects (e, OTDownloaders, true).size ())
			return true;

	if (!(e.Parameters_ & LeechCraft::OnlyDownload))
		if (GetObjects (e, OTHandlers, true).size ())
			return true;

	return false;
}

namespace
{
	bool DoDownload (IDownload *sd,
			LeechCraft::DownloadEntity p,
			int *id,
			QObject **pr)
	{
		int l = -1;
		try
		{
			l = sd->AddJob (p);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< "could not add job"
				<< e.what ();
			return false;
		}
		catch (...)
		{
			qWarning () << Q_FUNC_INFO
				<< "could not add job";
			return false;
		}

		if (id)
			*id = l;
		if (pr)
		{
			QObjectList plugins = Core::Instance ().GetPluginManager ()->
				GetAllCastableRoots<IDownload*> ();
			*pr = *std::find_if (plugins.begin (), plugins.end (),
					boost::bind (std::equal_to<IDownload*> (),
						sd,
						boost::bind<IDownload*> (
							static_cast<IDownload* (*) (const QObject*)> (qobject_cast<IDownload*>),
							_1
							)));
		}

		return true;
	}

	bool DoHandle (IEntityHandler *sh,
			LeechCraft::DownloadEntity p)
	{
		try
		{
			sh->Handle (p);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< "could not handle job"
				<< e.what ();
			return false;
		}
		catch (...)
		{
			qWarning () << Q_FUNC_INFO
				<< "could not add job";
			return false;
		}
		return true;
	}
};

QList<QObject*> LeechCraft::Core::GetObjects (const DownloadEntity& p,
		LeechCraft::Core::ObjectType type, bool detectOnly) const
{
	QObjectList plugins;
	switch (type)
	{
		case OTDownloaders:
			plugins = PluginManager_->GetAllCastableRoots<IDownload*> ();
			break;
		case OTHandlers:
			plugins = PluginManager_->GetAllCastableRoots<IEntityHandler*> ();
			break;
	}

	QObjectList result;
	for (int i = 0; i < plugins.size (); ++i)
	{
		if (plugins.at (i) == sender () &&
				!(p.Parameters_ & ShouldQuerySource))
			continue;

		try
		{
			switch (type)
			{
				case OTDownloaders:
					{
						IDownload *id = qobject_cast<IDownload*> (plugins.at (i));
						if (id->CouldDownload (p))
							result << plugins.at (i);
					}
					break;
				case OTHandlers:
					{
						IEntityHandler *ih = qobject_cast<IEntityHandler*> (plugins.at (i));
						if (ih->CouldHandle (p))
							result << plugins.at (i);
					}
					break;
			}

			if (detectOnly && result.size ())
				break;
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< "could not query"
				<< e.what ()
				<< plugins.at (i);
		}
		catch (...)
		{
			qWarning () << Q_FUNC_INFO
				<< "could not query"
				<< plugins.at (i);
		}
	}

	return result;
}

bool LeechCraft::Core::handleGotEntity (DownloadEntity p, int *id, QObject **pr)
{
	HookProxy_ptr proxy (new HookProxy);
	Q_FOREACH (HookSignature<HIDGotEntity>::Signature_t f,
			GetHooks<HIDGotEntity> ())
	{
		bool result = f (proxy, &p, id, pr, sender ());

		if (proxy->IsCancelled ())
			return result;
	}

	QString string = Util::GetUserText (p);

	std::auto_ptr<HandlerChoiceDialog> dia (new HandlerChoiceDialog (string));

	int numDownloaders = 0;
	if (!(p.Parameters_ & LeechCraft::OnlyHandle))
		Q_FOREACH (QObject *plugin, GetObjects (p, OTDownloaders, false))
			numDownloaders +=
				dia->Add (qobject_cast<IInfo*> (plugin), qobject_cast<IDownload*> (plugin));

	int numHandlers = 0;
	// Handlers don't fit when we want to delegate.
	if (!id && !(p.Parameters_ & LeechCraft::OnlyDownload))
		Q_FOREACH (QObject *plugin, GetObjects (p, OTHandlers, false))
			numHandlers +=
				dia->Add (qobject_cast<IInfo*> (plugin), qobject_cast<IEntityHandler*> (plugin));

	if (!(numHandlers + numDownloaders))
		return false;

	if (p.Parameters_ & FromUserInitiated &&
			!(p.Parameters_ & AutoAccept))
	{
		bool ask = true;
		if (XmlSettingsManager::Instance ()->
				property ("DontAskWhenSingle").toBool ())
			ask = numDownloaders;

		IDownload *sd = 0;
		IEntityHandler *sh = 0;
		if (ask)
		{
			dia->SetFilenameSuggestion (p.Location_);
			if (dia->exec () == QDialog::Rejected)
				return false;
			sd = dia->GetDownload ();
			sh = dia->GetEntityHandler ();
		}
		else
		{
			sd = dia->GetFirstDownload ();
			sh = dia->GetFirstEntityHandler ();
		}

		if (sd)
		{
			QString dir = dia->GetFilename ();
			if (dir.isEmpty ())
				return false;

			p.Location_ = dir;

			if (!DoDownload (sd, p, id, pr))
			{
				if (dia->NumChoices () > 1 &&
						QMessageBox::question (0,
						tr ("Error"),
						tr ("Could not add task to the selected downloader, "
							"would you like to try another one?"),
						QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
					return handleGotEntity (p, id, pr);
				else
					return false;
			}
			else
				return true;
		}
		if (sh)
		{
			if (!DoHandle (sh, p))
			{
				if (dia->NumChoices () > 1 &&
						QMessageBox::question (0,
						tr ("Error"),
						tr ("Could not handle task with the selected handler, "
							"would you like to try another one?"),
						QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
					return handleGotEntity (p, id, pr);
				else
					return false;
			}
			else
				return true;
		}
	}
	else if (dia->GetDownload ())
	{
		IDownload *sd = dia->GetDownload ();
		if (p.Location_.isEmpty ())
			p.Location_ = QDir::tempPath ();
		return DoDownload (sd, p, id, pr);
	}
	else if ((p.Parameters_ & LeechCraft::AutoAccept) &&
			dia->GetFirstEntityHandler ())
		return DoHandle (dia->GetFirstEntityHandler (), p);
	else
	{
		emit log (tr ("Could not handle download entity %1.")
				.arg (string));
		return false;
	}
	return true;
}

void LeechCraft::Core::handleCouldHandle (const LeechCraft::DownloadEntity& e, bool *could)
{
	*could = CouldHandle (e);
}

void LeechCraft::Core::handleClipboardTimer ()
{
	QString text = QApplication::clipboard ()->text ();
	if (text.isEmpty () || text == PreviousClipboardContents_)
		return;

	PreviousClipboardContents_ = text;

	DownloadEntity e = Util::MakeEntity (text.toUtf8 (),
			QString (),
			LeechCraft::FromUserInitiated);

	if (XmlSettingsManager::Instance ()->property ("WatchClipboard").toBool ())
		handleGotEntity (e);
}

void LeechCraft::Core::embeddedTabWantsToFront ()
{
	IEmbedTab *iet = qobject_cast<IEmbedTab*> (sender ());
	if (!iet)
		return;

	try
	{
		TabContainer_->bringToFront (iet->GetTabContents ());
		ReallyMainWindow_->show ();
	}
	catch (const std::exception& e)
	{
		qWarning () << Q_FUNC_INFO
			<< e.what ()
			<< sender ();
	}
	catch (...)
	{
		qWarning () << Q_FUNC_INFO
			<< sender ();
	}
}

void LeechCraft::Core::handleStatusBarChanged (QWidget *contents, const QString& origMessage)
{
	QString msg = origMessage;
	HookProxy_ptr proxy (new HookProxy);
	Q_FOREACH (HookSignature<HIDStatusBarChanged>::Signature_t f,
			GetHooks<HIDStatusBarChanged> ())
	{
		f (proxy, contents, &msg);

		if (proxy->IsCancelled ())
			return;
	}

	if (contents->visibleRegion ().isEmpty ())
		return;

	ReallyMainWindow_->statusBar ()->showMessage (msg, 30000);
}

void LeechCraft::Core::handleLog (const QString& msg)
{
	QString message = msg;
	HookProxy_ptr proxy (new HookProxy);
	Q_FOREACH (HookSignature<HIDLogString>::Signature_t f,
			GetHooks<HIDLogString> ())
	{
		f (proxy, &message, sender ());

		if (proxy->IsCancelled ())
			return;
	}

	IInfo *ii = qobject_cast<IInfo*> (sender ());
	try
	{
		emit log (ii->GetName () + ": " + message);
	}
	catch (const std::exception& e)
	{
		qWarning () << Q_FUNC_INFO
			<< e.what ()
			<< sender ();
	}
	catch (...)
	{
		qWarning () << Q_FUNC_INFO
			<< sender ();
	}
}

void LeechCraft::Core::InitDynamicSignals (QObject *plugin)
{
	const QMetaObject *qmo = plugin->metaObject ();

	if (qmo->indexOfSignal (QMetaObject::normalizedSignature (
					"couldHandle (const LeechCraft::DownloadEntity&, bool*)"
					).constData ()) != -1)
		connect (plugin,
				SIGNAL (couldHandle (const LeechCraft::DownloadEntity&, bool*)),
				this,
				SLOT (handleCouldHandle (const LeechCraft::DownloadEntity&, bool*)));

	if (qmo->indexOfSignal (QMetaObject::normalizedSignature (
					"gotEntity (const LeechCraft::DownloadEntity&)"
					).constData ()) != -1)
		connect (plugin,
				SIGNAL (gotEntity (const LeechCraft::DownloadEntity&)),
				this,
				SLOT (handleGotEntity (LeechCraft::DownloadEntity)),
				Qt::QueuedConnection);

	if (qmo->indexOfSignal (QMetaObject::normalizedSignature (
					"delegateEntity (const LeechCraft::DownloadEntity&, int*, QObject**)"
					).constData ()) != -1)
		connect (plugin,
				SIGNAL (delegateEntity (const LeechCraft::DownloadEntity&,
						int*, QObject**)),
				this,
				SLOT (handleGotEntity (LeechCraft::DownloadEntity,
						int*, QObject**)));

	if (qmo->indexOfSignal (QMetaObject::normalizedSignature (
					"downloadFinished (const QString&)"
					).constData ()) != -1)
		connect (plugin,
				SIGNAL (downloadFinished (const QString&)),
				this,
				SIGNAL (downloadFinished (const QString&)));

	if (qmo->indexOfSignal (QMetaObject::normalizedSignature (
					"log (const QString&)"
					).constData ()) != -1)
		connect (plugin,
				SIGNAL (log (const QString&)),
				this,
				SLOT (handleLog (const QString&)));
}

void LeechCraft::Core::InitJobHolder (QObject *plugin)
{
	try
	{
		IJobHolder *ijh = qobject_cast<IJobHolder*> (plugin);
		QAbstractItemModel *model = ijh->GetRepresentation ();
		Representation2Object_ [model] = plugin;
		MergeModel_->AddModel (model);

		if (model)
		{
			QToolBar *controlsWidget = model->
				index (0, 0).data (RoleControls).value<QToolBar*> ();
			if (controlsWidget)
				controlsWidget->setParent (ReallyMainWindow_);

			QWidget *additional = model->
				index (0, 0).data (RoleAdditionalInfo).value<QWidget*> ();
			if (additional)
				additional->setParent (ReallyMainWindow_);
		}
	}
	catch (const std::exception& e)
	{
		qWarning () << Q_FUNC_INFO
			<< e.what ()
			<< plugin;
	}
	catch (...)
	{
		qWarning () << Q_FUNC_INFO
			<< plugin;
	}
}

void LeechCraft::Core::InitEmbedTab (QObject *plugin)
{
	try
	{
		IInfo *ii = qobject_cast<IInfo*> (plugin);
		IEmbedTab *iet = qobject_cast<IEmbedTab*> (plugin);
		TabContainer_->add (ii->GetName (),
				iet->GetTabContents (),
				ii->GetIcon ());
		TabContainer_->SetToolBar (iet->GetToolBar (),
				iet->GetTabContents ());
	}
	catch (const std::exception& e)
	{
		qWarning () << Q_FUNC_INFO
			<< e.what ()
			<< plugin;
	}
	catch (...)
	{
		qWarning () << Q_FUNC_INFO
			<< plugin;
	}
}

void LeechCraft::Core::InitMultiTab (QObject *plugin)
{
	connect (plugin,
			SIGNAL (addNewTab (const QString&, QWidget*)),
			TabContainer_.get (),
			SLOT (add (const QString&, QWidget*)));
	connect (plugin,
			SIGNAL (removeTab (QWidget*)),
			TabContainer_.get (),
			SLOT (remove (QWidget*)));
	connect (plugin,
			SIGNAL (changeTabName (QWidget*, const QString&)),
			TabContainer_.get (),
			SLOT (changeTabName (QWidget*, const QString&)));
	connect (plugin,
			SIGNAL (changeTabIcon (QWidget*, const QIcon&)),
			TabContainer_.get (),
			SLOT (changeTabIcon (QWidget*, const QIcon&)));
	connect (plugin,
			SIGNAL (changeTooltip (QWidget*, QWidget*)),
			TabContainer_.get (),
			SLOT (changeTooltip (QWidget*, QWidget*)));
	connect (plugin,
			SIGNAL (statusBarChanged (QWidget*, const QString&)),
			this,
			SLOT (handleStatusBarChanged (QWidget*, const QString&)));
	connect (plugin,
			SIGNAL (raiseTab (QWidget*)),
			TabContainer_.get (),
			SLOT (bringToFront (QWidget*)));
}

QModelIndex LeechCraft::Core::MapToSourceRecursively (QModelIndex index) const
{
	const QAbstractProxyModel *model = 0;
	while ((model = qobject_cast<const QAbstractProxyModel*> (index.model ())) &&
			model->property ("__LeechCraft_own_core_model").toBool ())
		index = model->mapToSource (index);
	return index;
}

