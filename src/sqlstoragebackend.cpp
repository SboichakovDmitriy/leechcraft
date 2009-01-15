#include "sqlstoragebackend.h"
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <plugininterface/dblock.h>

using namespace LeechCraft;

SQLStorageBackend::SQLStorageBackend ()
: DB_ (QSqlDatabase::addDatabase ("QSQLITE", "CoreConnection"))
{
	QDir dir = QDir::home ();
	dir.cd (".leechcraft");
	dir.cd ("core");
	DB_.setDatabaseName (dir.filePath ("core.db"));
	if (!DB_.open ())
		LeechCraft::Util::DBLock::DumpError (DB_.lastError ());

	if (!DB_.tables ().contains ("sitesAuth"))
		InitializeTables ();
}

SQLStorageBackend::~SQLStorageBackend ()
{
}

void SQLStorageBackend::Prepare ()
{
	QSqlQuery pragma (DB_);
	if (!pragma.exec ("PRAGMA journal_mode = TRUNCATE;"))
		LeechCraft::Util::DBLock::DumpError (pragma);
	if (!pragma.exec ("PRAGMA synchronous = OFF;"))
		LeechCraft::Util::DBLock::DumpError (pragma);
	if (!pragma.exec ("PRAGMA temp_store = MEMORY;"))
		LeechCraft::Util::DBLock::DumpError (pragma);

	AuthGetter_ = QSqlQuery (DB_);
	AuthGetter_.prepare ("SELECT "
			"login, "
			"password "
			"FROM sitesAuth "
			"WHERE realm = :realm");

	AuthInserter_ = QSqlQuery (DB_);
	AuthInserter_.prepare ("INSERT INTO sitesAuth ("
			"realm, "
			"login, "
			"password"
			") VALUES ("
			":realm, "
			":login, "
			":password"
			")");

	AuthUpdater_ = QSqlQuery (DB_);
	AuthUpdater_.prepare ("UPDATE sitesAuth SET "
			"login = :login, "
			"password = :password "
			"WHERE realm = :realm");
}

void SQLStorageBackend::GetAuth (const QString& realm,
		QString& login, QString& password) const
{
	AuthGetter_.bindValue (":realm", realm);

	if (!AuthGetter_.exec () || !AuthGetter_.next ())
	{
		LeechCraft::Util::DBLock::DumpError (AuthGetter_);
		return;
	}

	login = AuthGetter_.value (0).toString ();
	password = AuthGetter_.value (1).toString ();
}

void SQLStorageBackend::SetAuth (const QString& realm,
		const QString& login, const QString& password)
{
	AuthGetter_.bindValue (":realm", realm);

	if (!AuthGetter_.exec ())
	{
		LeechCraft::Util::DBLock::DumpError (AuthGetter_);
		return;
	}

	if (!AuthGetter_.size ())
	{
		AuthInserter_.bindValue (":realm", realm);
		AuthInserter_.bindValue (":login", login);
		AuthInserter_.bindValue (":password", password);
		if (!AuthInserter_.exec ())
		{
			LeechCraft::Util::DBLock::DumpError (AuthInserter_);
			return;
		}
	}
	else
	{
		AuthUpdater_.bindValue (":realm", realm);
		AuthUpdater_.bindValue (":login", login);
		AuthUpdater_.bindValue (":password", password);
		if (!AuthUpdater_.exec ())
		{
			LeechCraft::Util::DBLock::DumpError (AuthUpdater_);
			return;
		}
	}
}

void SQLStorageBackend::InitializeTables ()
{
	QSqlQuery query (DB_);

	if (!query.exec ("CREATE TABLE sitesAuth ("
				"realm TEXT PRIMARY KEY, "
				"login TEXT, "
				"password TEXT"
				");"))
	{
		LeechCraft::Util::DBLock::DumpError (query);
		return;
	}

	if (!query.exec ("CREATE UNIQUE INDEX sitesAuth_realm "
				"ON sitesAuth (realm);"))
		LeechCraft::Util::DBLock::DumpError (query);
}


