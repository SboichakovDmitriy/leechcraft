#include "searchhandler.h"
#include <QUrl>
#include <QFile>
#include <QtDebug>
#include <interfaces/structures.h>
#include <plugininterface/util.h>

SearchHandler::SearchHandler (const Description& d)
: D_ (d)
{
}

int SearchHandler::columnCount (const QModelIndex&) const
{
	return 3;
}

QVariant SearchHandler::data (const QModelIndex& index, int role) const
{
	if (!index.isValid () || role != Qt::DisplayRole)
		return QVariant ();

	int r = index.row ();
	switch (index.column ())
	{
		case 0:
			return SearchString_;
		case 1:
			if (Results_.at (r).TotalResults_ >= 0)
				return tr ("%1 total results")
					.arg (Results_.at (r).TotalResults_);
			else
				return tr ("Unknown number of results");
		case 2:
			{
				QString result = D_.ShortName_;
				switch (Results_.at (r).Type_)
				{
					case Result::TypeRSS:
						result += tr (" (RSS)");
						break;
					case Result::TypeAtom:
						result += tr (" (Atom)");
						break;
					case Result::TypeHTML:
						result += tr (" (HTML)");
						break;
				}
				return result;
			}
		default:
			return QString ("");
	}
}

Qt::ItemFlags SearchHandler::flags (const QModelIndex& index) const
{
	if (!index.isValid ())
		return 0;
	else
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant SearchHandler::headerData (int header, Qt::Orientation orient, int role) const
{
	if (orient == Qt::Horizontal && role == Qt::DisplayRole)
		return QString ("");
	else
		return QVariant ();
}

QModelIndex SearchHandler::index (int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex (row, column, parent))
		return QModelIndex ();

	return createIndex (row, column);
}

QModelIndex SearchHandler::parent (const QModelIndex&) const
{
	return QModelIndex ();
}

int SearchHandler::rowCount (const QModelIndex& parent) const
{
	return parent.isValid () ? 0 : Results_.size ();
}

void SearchHandler::Start (const LeechCraft::Request& r)
{
	SearchString_ = r.String_;
	Q_FOREACH (UrlDescription u, D_.URLs_)
	{
		QUrl url (u.Template_);
		QList<QPair<QString, QString> > items = url.queryItems (),
			newItems;
		QPair<QString, QString> item;
		Q_FOREACH (item, items)
		{
			if (item.second.size () >= 3 &&
					item.second.at (0) == '{' &&
					item.second.at (item.second.size () - 1) == '}' &&
					item.second.at (item.second.size () - 2) == '?')
				continue;
			if (item.second == "{searchTerms}")
				item.second = SearchString_;
			else
				item.second = "";
			newItems << item;
		}
		url.setQueryItems (newItems);
		qDebug () << url.toString () << u.Template_;

		QString fname = LeechCraft::Util::GetTemporaryName ();
		LeechCraft::DownloadEntity e =
		{
			url.toString ().toUtf8 (),
			fname,
			u.Type_,
			LeechCraft::Internal |
				LeechCraft::DoNotNotifyUser |
				LeechCraft::DoNotSaveInHistory |
				LeechCraft::NotPersistent |
				LeechCraft::DoNotAnnounceEntity
		};

		int id = -1;
		QObject *pr;
		emit delegateEntity (e, &id, &pr);
		if (id == -1)
		{
			emit error (tr ("Job for request<br />%1<br />wasn't delegated.")
					.arg (url.toString ()));
			continue;
		}

		HandleProvider (pr);

		Result job;
		job.Filename_ = fname;
		if (u.Type_ == "application/rss+xml")
			job.Type_ = Result::TypeRSS;
		else if (u.Type_ == "application/atom+xml")
			job.Type_ = Result::TypeAtom;
		else
			job.Type_ = Result::TypeHTML;
		Jobs_ [id] = job;
	}
}

void SearchHandler::handleJobFinished (int id)
{
	if (!Jobs_.contains (id))
		return;

	Result result = Jobs_ [id];
	Jobs_.remove (id);

	QFile file (result.Filename_);
	if (!file.open (QIODevice::ReadOnly))
	{
		emit error (tr ("Could not open file %1.")
				.arg (result.Filename_));
		return;
	}

	result.Response_ = file.readAll ();

	file.close ();
	if (!file.remove ())
		emit warning (tr ("Could not remove temporary file %1.")
				.arg (result.Filename_));

	beginInsertRows (QModelIndex (), Results_.size (), Results_.size ());
	Results_ << result;
	endInsertRows ();
}

void SearchHandler::handleJobError (int id)
{
	if (!Jobs_.contains (id))
		return;

	emit error (tr ("A job was delegated, but it failed."));
	Jobs_.remove (id);
}

void SearchHandler::HandleProvider (QObject *provider)
{
	if (Downloaders_.contains (provider))
		return;
	
	Downloaders_ << provider;
	connect (provider,
			SIGNAL (jobFinished (int)),
			this,
			SLOT (handleJobFinished (int)));
	connect (provider,
			SIGNAL (jobError (int, IDownload::Error)),
			this,
			SLOT (handleJobError (int)));
}

