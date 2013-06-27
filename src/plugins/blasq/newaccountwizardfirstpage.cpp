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

#include "newaccountwizardfirstpage.h"
#include <interfaces/blasq/iservice.h>
#include "servicesmanager.h"
#include <QVBoxLayout>

namespace LeechCraft
{
namespace Blasq
{
	NewAccountWizardFirstPage::NewAccountWizardFirstPage (const ServicesManager *svcMgr,
			QWidget *parent)
	: QWizardPage (parent)
	, ServicesMgr_ (svcMgr)
	{
		Ui_.setupUi (this);
		connect (Ui_.Service_,
				SIGNAL (currentIndexChanged (int)),
				this,
				SLOT (updatePages ()));
	}

	void NewAccountWizardFirstPage::initializePage ()
	{
		connect (wizard (),
				SIGNAL (accepted ()),
				this,
				SLOT (handleAccepted ()));

		for (auto service : ServicesMgr_->GetServices ())
			Ui_.Service_->addItem (service->GetServiceIcon (), service->GetServiceName (),
					QVariant::fromValue (service->GetQObject ()));

		updatePages ();
	}

	void NewAccountWizardFirstPage::updatePages ()
	{
		const int currentId = wizard ()->currentId ();
		for (const int id : wizard ()->pageIds ())
			if (id > currentId)
				wizard ()->removePage (id);

		Widgets_.clear ();
		Service_ = 0;

		const auto idx = Ui_.Service_->currentIndex ();
		if (idx < 0)
			return;

		const auto serviceObj = Ui_.Service_->itemData (idx).value<QObject*> ();
		Service_ = qobject_cast<IService*> (serviceObj);

		Widgets_ = Service_->GetAccountRegistrationWidgets ();
		for (auto w : Widgets_)
		{
			auto page = qobject_cast<QWizardPage*> (w);
			if (!page)
			{
				page = new QWizardPage ();
				page->setTitle (tr ("%1 options")
						.arg (Service_->GetServiceName ()));
				page->setLayout (new QVBoxLayout);
				page->layout ()->addWidget (w);
			}
			wizard ()->addPage (page);
		}
	}

	void NewAccountWizardFirstPage::handleAccepted ()
	{
		if (!Service_)
			return;

		Service_->RegisterAccount (Ui_.AccName_->text (), Widgets_);
	}
}
}