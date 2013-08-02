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

#include "syncops.h"
#include <QDataStream>
#include <QByteArray>
#include <QtDebug>
#include "util/exceptions.h"

namespace LeechCraft
{
	namespace Sync
	{
		bool operator== (const Payload& p1, const Payload& p2)
		{
			return p1.Data_ == p2.Data_;
		}

		QDataStream& operator<< (QDataStream& out, const Payload& payload)
		{
			quint16 version = 1;
			out << version
					<< payload.Data_;
			return out;
		}

		QDataStream& operator>> (QDataStream& in, Payload& payload)
		{
			quint16 version;
			in >> version;
			switch (version)
			{
			case 1:
				in >> payload.Data_;
				break;
			default:
				throw UnknownVersionException (version,
						"unknown version while deserializing payload");
			}
			return in;
		}

		QByteArray Serialize (const Payload& payload)
		{
			QByteArray result;

			{
				QDataStream str (&result, QIODevice::WriteOnly);
				str << payload;
			}
			return result;
		}

		Payload Deserialize (const QByteArray& data)
		{
			Payload result;

			QDataStream in (data);
			in >> result;
			return result;
		}

		Payload CreatePayload (const QByteArray& from)
		{
			Payload p = { from };
			return p;
		}

		QDataStream& operator<< (QDataStream& out, const Delta& delta)
		{
			quint16 version = 1;
			out << version
					<< delta.ID_
					<< delta.Payload_;
			return out;
		}

		QDataStream& operator>> (QDataStream& in, Delta& delta)
		{
			quint16 version = 0;
			in >> version;
			if (version == 1)
				in >> delta.ID_
					>> delta.Payload_;
			else
				throw UnknownVersionException (version,
						"unknown version while deserializing delta");
			return in;
		}
	}
}