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

#include "vcarddialog.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Vader
{
	VCardDialog::VCardDialog (QWidget *parent)
	: QDialog (parent)
	{
		Ui_.setupUi (this);
	}

	void VCardDialog::SetAvatar (const QImage& avatar)
	{
		if (!avatar.isNull ())
			Ui_.AvatarLabel_->setPixmap (QPixmap::fromImage (avatar));
	}

	void VCardDialog::SetInfo (QMap<QString, QString> headers)
	{
		Ui_.InfoFields_->clear ();

		QStringList strings;

		auto append = [&strings, &headers] (const QString& key, const QString& trPattern)
		{
			const auto& val = headers [key];
			if (!val.isEmpty ())
				strings << trPattern.arg (val);
		};
		append ("Nickname", tr ("Nickname: %1."));
		append ("FirstName", tr ("First name: %1."));
		append ("LastName", tr ("Last name: %1."));

		if (headers.contains ("Sex"))
			headers ["Sex"] = headers ["Sex"] == "1" ? tr ("male") : tr ("female");
		append ("Sex", tr ("Gender: %1."));

		append ("Birthday", tr ("Birthday: %1."));
		append ("Zodiac", tr ("Zodiac sign: %1."));
		append ("Location", tr ("Location: %1."));
		append ("Phone", tr ("Phone number: %1."));

		Ui_.InfoFields_->setPlainText (strings.join ("\n"));
	}
}
}
}
