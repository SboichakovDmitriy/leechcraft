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

#include "storage.h"
#include <stdexcept>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <util/util.h>
#include <util/dblock.h>
#include "oral.h"

namespace LeechCraft
{
namespace Poleemery
{
	struct StorageImpl
	{
		QSqlDatabase DB_;

		oral::ObjectInfo<Account> AccountInfo_;
		oral::ObjectInfo<NakedExpenseEntry> NakedExpenseEntryInfo_;
		oral::ObjectInfo<ReceiptEntry> ReceiptEntryInfo_;
		oral::ObjectInfo<Category> CategoryInfo_;
		oral::ObjectInfo<CategoryLink> CategoryLinkInfo_;
		oral::ObjectInfo<Rate> RateInfo_;

		QHash<QString, Category> CatCache_;
		QHash<int, Category> CatIDCache_;
	};

	Storage::Storage (QObject *parent)
	: QObject (parent)
	, Impl_ (new StorageImpl)
	{
		Impl_->DB_ = QSqlDatabase::addDatabase ("QSQLITE", "Poleemery_Connection");
		const auto& dir = Util::CreateIfNotExists ("poleemeery");
		Impl_->DB_.setDatabaseName (dir.absoluteFilePath ("database.db"));

		if (!Impl_->DB_.open ())
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open database:"
					<< Impl_->DB_.lastError ().text ();
			throw std::runtime_error ("Poleemery database creation failed");
		}

		{
			QSqlQuery query (Impl_->DB_);
			query.exec ("PRAGMA foreign_keys = ON;");
		}

		InitializeTables ();
		LoadCategories ();
	}

	QList<Account> Storage::GetAccounts () const
	{
		try
		{
			return Impl_->AccountInfo_.DoSelectAll_ ();
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	void Storage::AddAccount (Account& acc)
	{
		try
		{
			Impl_->AccountInfo_.DoInsert_ (acc);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	void Storage::UpdateAccount (const Account& acc)
	{
		try
		{
			Impl_->AccountInfo_.DoUpdate_ (acc);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	void Storage::DeleteAccount (const Account& acc)
	{
		try
		{
			Impl_->AccountInfo_.DoDelete_ (acc);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	QList<ExpenseEntry> Storage::GetExpenseEntries ()
	{
		try
		{
			return HandleNaked (Impl_->NakedExpenseEntryInfo_.DoSelectAll_ ());
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	QList<ExpenseEntry> Storage::GetExpenseEntries (const Account& parent)
	{
		try
		{
			return HandleNaked (Impl_->NakedExpenseEntryInfo_.SelectByFKeysActor_ (boost::fusion::make_vector (parent)));
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	void Storage::AddExpenseEntry (ExpenseEntry& entry)
	{
		Util::DBLock lock (Impl_->DB_);
		lock.Init ();

		try
		{
			Impl_->NakedExpenseEntryInfo_.DoInsert_ (entry);
			AddNewCategories (entry, entry.Categories_);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}

		lock.Good ();
	}

	void Storage::UpdateExpenseEntry (const ExpenseEntry& entry)
	{
		Util::DBLock lock (Impl_->DB_);
		lock.Init ();

		Impl_->NakedExpenseEntryInfo_.DoUpdate_ (entry);

		auto nowCats = entry.Categories_;

		for (const auto& cat : boost::fusion::at_c<1> (Impl_->CategoryLinkInfo_.SingleFKeySelectors_) (entry))
		{
			if (!nowCats.removeAll (Impl_->CatIDCache_.value (cat.Category_).Name_))
				Impl_->CategoryLinkInfo_.DoDelete_ (cat);
		}

		if (!nowCats.isEmpty ())
			AddNewCategories (entry, nowCats);

		lock.Good ();
	}

	void Storage::DeleteExpenseEntry (const ExpenseEntry& entry)
	{
		try
		{
			Impl_->NakedExpenseEntryInfo_.DoDelete_ (entry);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	QList<ReceiptEntry> Storage::GetReceiptEntries ()
	{
		try
		{
			return Impl_->ReceiptEntryInfo_.DoSelectAll_ ();
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	QList<ReceiptEntry> Storage::GetReceiptEntries (const Account& account)
	{
		try
		{
			return Impl_->ReceiptEntryInfo_.SelectByFKeysActor_ (boost::fusion::make_vector (account));
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	void Storage::AddReceiptEntry (ReceiptEntry& entry)
	{
		try
		{
			Impl_->ReceiptEntryInfo_.DoInsert_ (entry);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	void Storage::UpdateReceiptEntry (const ReceiptEntry& entry)
	{
		Impl_->ReceiptEntryInfo_.DoUpdate_ (entry);
	}

	void Storage::DeleteReceiptEntry (const ReceiptEntry& entry)
	{
		Impl_->ReceiptEntryInfo_.DoDelete_ (entry);
	}

	QList<Rate> Storage::GetRates ()
	{
		try
		{
			return Impl_->RateInfo_.DoSelectAll_ ();
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	QList<Rate> Storage::GetRates (const QDateTime& start, const QDateTime& end)
	{
		try
		{
			return Impl_->RateInfo_.DoSelectByFields_ (start < oral::ph::_2 && oral::ph::_2 < end);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	QList<Rate> Storage::GetRate (const QString& code)
	{
		try
		{
			return Impl_->RateInfo_.DoSelectByFields_ (oral::ph::_1 == code);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	QList<Rate> Storage::GetRate (const QString& code, const QDateTime& start, const QDateTime& end)
	{
		try
		{
			return Impl_->RateInfo_.DoSelectByFields_ (oral::ph::_1 == code && start < oral::ph::_2 && oral::ph::_2 < end);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	void Storage::AddRate (Rate& rate)
	{
		try
		{
			Impl_->RateInfo_.DoInsert_ (rate);
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}

	Category Storage::AddCategory (const QString& name)
	{
		Category cat { name };
		Impl_->CategoryInfo_.DoInsert_ (cat);
		Impl_->CatCache_ [name] = cat;
		Impl_->CatIDCache_ [cat.ID_] = cat;
		return cat;
	}

	void Storage::AddNewCategories (const ExpenseEntry& entry, const QStringList& cats)
	{
		for (const auto& cat : cats)
		{
			if (!Impl_->CatCache_.contains (cat))
				AddCategory (cat);
			LinkEntry2Cat (entry, Impl_->CatCache_ [cat]);
		}
	}

	void Storage::LinkEntry2Cat (const ExpenseEntry& entry, const Category& category)
	{
		CategoryLink link (category, entry);
		Impl_->CategoryLinkInfo_.DoInsert_ (link);
	}

	void Storage::UnlinkEntry2Cat (const ExpenseEntry& entry, const Category& category)
	{
		const auto& link = Impl_->CategoryLinkInfo_.SelectByFKeysActor_ (boost::fusion::make_vector (category, entry));
		if (!link.isEmpty ())
			Impl_->CategoryLinkInfo_.DoDelete_ (link.first ());
	}

	QList<ExpenseEntry> Storage::HandleNaked (const QList<NakedExpenseEntry>& nakedItems)
	{
		QList<ExpenseEntry> entries;

		try
		{
			for (const auto& naked : nakedItems)
			{
				ExpenseEntry entry { naked };

				const auto& cats = boost::fusion::at_c<1> (Impl_->CategoryLinkInfo_.SingleFKeySelectors_) (naked);
				for (const auto& cat : cats)
					entry.Categories_ << Impl_->CatIDCache_ [cat.Category_].Name_;

				entries << entry;
			}
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}

		return entries;
	}


	namespace oral
	{
		template<>
		struct Type2Name<AccType>
		{
			QString operator() () const { return "TEXT"; }
		};

		template<>
		struct ToVariant<AccType>
		{
			QVariant operator() (const AccType& type) const
			{
				switch (type)
				{
				case AccType::BankAccount:
					return "BankAccount";
				case AccType::Cash:
					return "Cash";
				}

				qWarning () << Q_FUNC_INFO
						<< "unknown type"
						<< static_cast<int> (type);
				return {};
			}
		};

		template<>
		struct FromVariant<AccType>
		{
			AccType operator() (const QVariant& var) const
			{
				const auto& str = var.toString ();
				if (str == "BankAccount")
					return AccType::BankAccount;
				else
					return AccType::Cash;
			}
		};
	}

	void Storage::InitializeTables ()
	{
		Impl_->AccountInfo_ = oral::Adapt<Account> (Impl_->DB_);
		Impl_->NakedExpenseEntryInfo_ = oral::Adapt<NakedExpenseEntry> (Impl_->DB_);
		Impl_->ReceiptEntryInfo_ = oral::Adapt<ReceiptEntry> (Impl_->DB_);
		Impl_->CategoryInfo_ = oral::Adapt<Category> (Impl_->DB_);
		Impl_->CategoryLinkInfo_ = oral::Adapt<CategoryLink> (Impl_->DB_);
		Impl_->RateInfo_ = oral::Adapt<Rate> (Impl_->DB_);

		const auto& tables = Impl_->DB_.tables ();

		QMap<QString, QString> queryCreates;
		queryCreates [Account::ClassName ()] = Impl_->AccountInfo_.CreateTable_;
		queryCreates [NakedExpenseEntry::ClassName ()] = Impl_->NakedExpenseEntryInfo_.CreateTable_;
		queryCreates [ReceiptEntry::ClassName ()] = Impl_->ReceiptEntryInfo_.CreateTable_;
		queryCreates [Category::ClassName ()] = Impl_->CategoryInfo_.CreateTable_;
		queryCreates [CategoryLink::ClassName ()] = Impl_->CategoryLinkInfo_.CreateTable_;
		queryCreates [Rate::ClassName ()] = Impl_->RateInfo_.CreateTable_;

		Util::DBLock lock (Impl_->DB_);
		lock.Init ();

		QSqlQuery query (Impl_->DB_);

		bool tablesCreated = false;
		for (const auto& key : queryCreates.keys ())
			if (!tables.contains (key))
			{
				tablesCreated = true;
				if (!query.exec (queryCreates [key]))
					{
						Util::DBLock::DumpError (query);
						throw std::runtime_error ("cannot create tables");
					}
			}

		lock.Good ();

		// Otherwise queries created by oral::Adapt() don't work.
		if (tablesCreated)
			InitializeTables ();
	}

	void Storage::LoadCategories ()
	{
		try
		{
			for (const auto& cat : Impl_->CategoryInfo_.DoSelectAll_ ())
			{
				Impl_->CatCache_ [cat.Name_] = cat;
				Impl_->CatIDCache_ [cat.ID_] = cat;
			}
		}
		catch (const oral::QueryException& e)
		{
			qWarning () << Q_FUNC_INFO;
			Util::DBLock::DumpError (e.GetQuery ());
			throw;
		}
	}
}
}
