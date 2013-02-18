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

#include "account.h"
#include <QtDebug>
#include <util/passutils.h>
#include "protocol.h"

namespace LeechCraft
{
namespace Azoth
{
namespace VelvetBird
{
	Account::Account (const QString& name, PurpleAccount *acc, Protocol *proto)
	: QObject (proto)
	, Name_ (name)
	, Account_ (acc)
	, Proto_ (proto)
	{
		HandleStatus (purple_account_get_active_status (acc));
	}

	PurpleAccount* Account::GetPurpleAcc () const
	{
		return Account_;
	}

	QObject* Account::GetObject ()
	{
		return this;
	}

	QObject* Account::GetParentProtocol () const
	{
		return Proto_;
	}

	IAccount::AccountFeatures Account::GetAccountFeatures () const
	{
		return FRenamable;
	}

	QList<QObject*> Account::GetCLEntries ()
	{
		return {};
	}

	QString Account::GetAccountName () const
	{
		return Name_;
	}

	QString Account::GetOurNick () const
	{
		return QString ();
	}

	void Account::RenameAccount (const QString& name)
	{
		Name_ = name;
		emit accountRenamed (name);
	}

	QByteArray Account::GetAccountID () const
	{
		return Proto_->GetProtocolID () + "_" + purple_account_get_username (Account_);
	}

	QList<QAction*> Account::GetActions () const
	{
		return {};
	}

	void Account::QueryInfo (const QString&)
	{
	}

	void Account::OpenConfigurationDialog ()
	{
	}

	EntryStatus Account::GetState () const
	{
		return EntryStatus ();
	}

	void Account::ChangeState (const EntryStatus& status)
	{
		if (!purple_account_get_password (Account_))
		{
			const auto& str = Util::GetPassword ("Azoth." + GetAccountID (),
					tr ("Enter password for account %1:").arg (GetAccountName ()), Proto_, true);
			if (str.isEmpty ())
				return;

			purple_account_set_password (Account_, str.toUtf8 ().constData ());
		}

		if (status.State_ == SOffline)
		{
			if (!purple_account_is_disconnected (Account_))
				purple_account_disconnect (Account_);
			purple_account_set_enabled (Account_, "leechcraft.azoth", false);
			return;
		}

		if (!purple_account_get_enabled (Account_, "leechcraft.azoth"))
			purple_account_set_enabled (Account_, "leechcraft.azoth", true);
		purple_account_set_status (Account_, purple_primitive_get_id_from_type (PURPLE_STATUS_AVAILABLE), true, NULL);
	}

	void Account::Authorize (QObject*)
	{
	}

	void Account::DenyAuth (QObject*)
	{
	}

	void Account::RequestAuth (const QString&, const QString&, const QString&, const QStringList&)
	{
	}

	void Account::RemoveEntry (QObject*)
	{
	}

	QObject* Account::GetTransferManager() const
	{
		return 0;
	}

	void Account::HandleStatus (PurpleStatus *status)
	{
		qDebug () << Q_FUNC_INFO << status;
		qDebug () << purple_status_get_id (status) << purple_status_get_name (status);
	}
}
}
}