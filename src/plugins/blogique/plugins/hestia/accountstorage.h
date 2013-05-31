/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <interfaces/blogique/iaccount.h>

namespace LeechCraft
{
namespace Blogique
{
namespace Hestia
{
	class LocalBlogAccount;

	class AccountStorage : public QObject
	{
		Q_OBJECT

		LocalBlogAccount *Account_;
		bool Ready_;

		QSqlDatabase AccountDB_;

		QSqlQuery AddEntry_;
		QSqlQuery RemoveEntry_;
		QSqlQuery UpdateEntry_;

		QSqlQuery GetEntries_;
		QSqlQuery GetLastEntries_;
		QSqlQuery GetShortEntries_;
		QSqlQuery GetFullEntry_;
		QSqlQuery GetEntriesByDate_;
		QSqlQuery GetEntriesCountByDate_;

		QSqlQuery AddEntryTag_;
		QSqlQuery RemoveEntryTags_;
		QSqlQuery GetEntryTags_;
		QSqlQuery GetTags_;

	public:
		enum class Mode
		{
			ShortMode,
			FullMode
		};

		explicit AccountStorage (LocalBlogAccount *acc, QObject *parent = 0);

		void Init (const QString& dbPath);
		bool IsReady () const;
		bool CheckDatabase (const QString& dbPath);

		qint64 SaveNewEntry (const Entry& e);
		qint64 UpdateEntry (const Entry& e, qint64 entryId);
		void RemoveEntry(qint64 entryId);
		QList<Entry> GetEntries (Mode mode);
		QList<Entry> GetLastEntries (Mode mode, int count);
		QList<Entry> GetEntriesByDate (const QDate& date);
		QMap<QDate, int> GetEntriesCountByDate ();
		Entry GetFullEntry (qint64 entryId);
		QHash<QString, int> GetAllTags ();
	private:
		void CreateTables ();
		void PrepareQueries ();
	};
}
}
}
