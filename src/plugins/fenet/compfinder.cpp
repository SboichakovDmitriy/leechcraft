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

#include "compfinder.h"

namespace LeechCraft
{
namespace Fenet
{
	CompFinder::CompFinder (QObject *parent)
	: FinderBase (parent)
	{
		Find ("compositing");
	}

	CompInfo CompFinder::GetInfo (const QString&, const QStringList& execs, const QVariantMap& map) const
	{
		CompInfo info
		{
			{},
			{},
			map ["name"].toString (),
			map ["comment"].toString (),
			execs
		};

		for (const auto& item : map ["flags"].toList ())
		{
			const auto& flagMap = item.toMap ();
			info.Flags_.append ({ flagMap ["param"].toString (), flagMap ["desc"].toString () });
		}

		for (const auto& item : map ["params"].toList ())
		{
			const auto& pMap = item.toMap ();
			info.Params_.append ({
					pMap ["param"].toString (),
					pMap ["desc"].toString (),

					pMap ["default"].toDouble (),

					pMap.value ("min", 0).toDouble (),
					pMap.value ("max", 0).toDouble ()
				});
		}

		return info;
	}
}
}