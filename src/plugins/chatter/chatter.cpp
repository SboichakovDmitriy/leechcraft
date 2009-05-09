/***************************************************************************
 *   Copyright (C) 2008 by Voker57   *
 *   voker57@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include <QApplication>
#include <plugininterface/proxy.h>

#include "chatter.h"
#include "fsirc.h"

Chatter::~Chatter ()
{
}

void Chatter::Init ()
{
    setWindowTitle ("Chatter");
    IsShown_ = false;
	ircClient = new fsirc();
	connect(ircClient, SIGNAL(gotLink(QString)), this, SIGNAL(gotEntity(QString)));
	connect(ircClient, SIGNAL(bringToFront()), this, SIGNAL(bringToFront()));
    setCentralWidget (ircClient);
}

QString Chatter::GetName () const
{
    return "Chatter";
}

QString Chatter::GetInfo () const
{
    return tr("A f^W very simple irc client");
}

QStringList Chatter::Provides () const
{
    return QStringList ("irc");
}

QStringList Chatter::Uses () const
{
    return QStringList ();
}

QStringList Chatter::Needs () const
{
    return QStringList ();
}

void Chatter::SetProvider (QObject *obj, const QString& feature)
{
	Q_UNUSED (obj);
	Q_UNUSED (feature);
}

void Chatter::Release ()
{
    QSettings settings (LeechCraft::Util::Proxy::Instance ()->GetOrganizationName (),
			LeechCraft::Util::Proxy::Instance ()->GetApplicationName ());
    settings.beginGroup (GetName ());
    settings.beginGroup ("geometry");
    settings.setValue ("pos", pos ());
    settings.endGroup ();
    settings.endGroup ();

	fsirc::finalizeIrcList ();
	delete ircClient;

	IrcLayer::finalizeServers ();
}

QIcon Chatter::GetIcon () const
{
    return QIcon (":/fsirc/data/icon.svg");
}

QWidget* Chatter::GetTabContents ()
{
	return ircClient;
}

QToolBar* Chatter::GetToolBar () const
{
	return 0;
}

Q_EXPORT_PLUGIN2 (leechcraft_chatter, Chatter);

