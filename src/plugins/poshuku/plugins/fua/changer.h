#ifndef CHANGER_H
#define CHANGER_H
#include <QDialog>
#include <QMap>
#include "ui_changer.h"

class Changer : public QDialog
{
	Q_OBJECT

	Ui::Changer Ui_;
	QMap<QString, QString> IDs_;
public:
	Changer (const QMap<QString, QString>&,
			const QString& = QString (),
			const QString& = QString (),
			QWidget* = 0);
	QString GetDomain () const;
	QString GetID () const;
private slots:
	void on_Domain__textChanged ();
	void on_IDString__textChanged ();
	void on_Agent__currentIndexChanged (const QString&);
private:
	void SetEnabled ();
};

#endif

