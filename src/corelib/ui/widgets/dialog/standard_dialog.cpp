#include "standard_dialog.h"

#include "dialog.h"

#include <ui/design_system/design_system.h>

#include <QApplication>
#include <QRandomGenerator>


QString StandardDialog::generateOkTerm()
{
    const QVector<QString> terms = {
        QApplication::translate("StandardDialog", "Got it"),
        QApplication::translate("StandardDialog", "Ah, perfect"),
        QApplication::translate("StandardDialog", "This is fine"),
        QApplication::translate("StandardDialog", "Not bad"),
        QApplication::translate("StandardDialog", "I see"),
        QApplication::translate("StandardDialog", "Wonderful"),
        QApplication::translate("StandardDialog", "Alright"),
        QApplication::translate("StandardDialog", "So-so"),
        QApplication::translate("StandardDialog", "Amazing"),
        QApplication::translate("StandardDialog", "Cool"),
        QApplication::translate("StandardDialog", "Gorgeous"),
        QApplication::translate("StandardDialog", "Fine"),
    };
    const auto _index = QRandomGenerator::global()->bounded(0, terms.size());
    return terms[_index];
}

void StandardDialog::information(QWidget* _parent, const QString& _title, const QString& _text)
{
    auto dialog = new Dialog(_parent);
    dialog->setContentMaximumWidth(Ui::DesignSystem::dialog().maximumWidth());
    dialog->showDialog(_title, _text, { { 0, generateOkTerm(), Dialog::AcceptButton } });
    QObject::connect(dialog, &Dialog::finished, dialog, &Dialog::hideDialog);
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}
