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

#ifndef PLUGINS_AZOTH_ANIMATEDICONMANAGER_H
#define PLUGINS_AZOTH_ANIMATEDICONMANAGER_H
#include <boost/function.hpp>
#include <QObject>
#include <QImage>
#include <QHash>
#include <QFile>
#include <QTimerEvent>
#include <QIcon>
#include <QImageReader>
#include <QCache>

namespace LeechCraft
{
namespace Azoth
{
	template<typename T>
	class AnimatedIconManager : public QObject
	{
		struct IconInfo
		{
			int CurrentFrame_;
			QList<QImage> Images_;
			int TimerID_;
		};
		QHash<T, IconInfo> Object2Icon_;
		QHash<int, T> Timer2Object_;

		QCache<QByteArray, QIcon> IconCache_;
	public:
		typedef boost::function<void (T, const QIcon&)> IconSetter_t;
	private:
		IconSetter_t Setter_;
	public:
		AnimatedIconManager (IconSetter_t setter, QObject* parent = 0)
		: QObject (parent)
		, IconCache_ (2 * 1024 * 1024)
		, Setter_ (setter)
		{
		}

		void SetIcon (const T& t, const QString& name)
		{
			QFile file (name);
			if (!file.open (QIODevice::ReadOnly))
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to open file"
						<< name;
				return;
			}

			SetIcon (t, &file);
		}

		void SetIcon (const T& t, QIODevice *dev)
		{
			Cancel (t);

			if (dev && dev->atEnd ())
				dev->seek (0);

			const auto& data = dev->readAll ();
			if (auto icon = IconCache_.object (data))
			{
				Setter_ (t, *icon);
				return;
			}

			if (dev && dev->atEnd ())
				dev->seek (0);

			QImageReader reader (dev);
			QImage image = reader.read ();
			const int w = image.size ().width ();
			const int h = image.size ().height ();
			if (w == h &&
					reader.imageCount () <= 1)
			{
				const QIcon icon (QPixmap::fromImage (image));
				Setter_ (t, icon);
				IconCache_.insert (data, new QIcon (icon), data.size ());
				return;
			}

			int delay = 0;
			QList<QImage> images;
			if (reader.supportsAnimation ())
			{
				while (!image.isNull ())
				{
					images << image;
					image = reader.read ();
				}

				delay = reader.nextImageDelay ();
			}
			else if (!(w % h))
			{
				int frame = 0;
				while (frame * h < w)
				{
					images << image.copy (frame * h, 0, h, h);
					++frame;
				}

				delay = 200;
			}

			IconInfo info =
			{
				0,
				images,
				startTimer (delay)
			};

			Setter_ (t, QIcon (QPixmap::fromImage (images.first ())));

			Object2Icon_ [t] = info;
			Timer2Object_ [info.TimerID_] = t;
		}

		void Cancel (const T& t)
		{
			if (!Object2Icon_.contains (t))
				return;

			const int timerId = Object2Icon_.take (t).TimerID_;
			killTimer (timerId);
			Timer2Object_.remove (timerId);
		}
	protected:
		void timerEvent (QTimerEvent *e)
		{
			QObject::timerEvent (e);

			const int id = e->timerId ();

			const T& t = Timer2Object_ [id];
			IconInfo info = Object2Icon_ [t];
			if (++info.CurrentFrame_ == info.Images_.size ())
				info.CurrentFrame_ = 0;
			Setter_ (t, QIcon (QPixmap::fromImage (info.Images_ [info.CurrentFrame_])));

			Object2Icon_ [t] = info;
		}
	};
}
}

#endif
