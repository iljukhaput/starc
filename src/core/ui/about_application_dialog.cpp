#include "about_application_dialog.h"

#include <ui/design_system/design_system.h>
#include <ui/layouts/flow_layout/flow_layout.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/icon_button/icon_button.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/label/link_label.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/ui_helper.h>

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDesktopServices>


namespace Ui {

class AboutApplicationDialog::Implementation
{
public:
    explicit Implementation(QWidget* _parent);


    bool isRussianSpeaking = false;

    Body2LinkLabel* versionLabel = nullptr;
    Body1Label* authorsLabel1 = nullptr;
    Body1LinkLabel* authorsLink = nullptr;
    Body1Label* authorsLabel2 = nullptr;
    Body1Label* partnerLabel1 = nullptr;
    Body1LinkLabel* partnerLink = nullptr;
    Body1Label* partnerLabel2 = nullptr;
    Body1Label* descriptionLabel1 = nullptr;
    Body1Label* descriptionLabel2 = nullptr;

    IconButton* twitterButton = nullptr;
    IconButton* discordButton = nullptr;
    IconButton* telegramButton = nullptr;
    IconButton* vkButton = nullptr;
    IconButton* mailButton = nullptr;
    Button* closeButton = nullptr;
    QHBoxLayout* buttonsLayout = nullptr;
};

AboutApplicationDialog::Implementation::Implementation(QWidget* _parent)
    : versionLabel(new Body2LinkLabel(_parent))
    , authorsLabel1(new Body1Label(_parent))
    , authorsLink(new Body1LinkLabel(_parent))
    , authorsLabel2(new Body1Label(_parent))
    , partnerLabel1(new Body1Label(_parent))
    , partnerLink(new Body1LinkLabel(_parent))
    , partnerLabel2(new Body1Label(_parent))
    , descriptionLabel1(new Body1Label(_parent))
    , descriptionLabel2(new Body1Label(_parent))
    , twitterButton(new IconButton(_parent))
    , discordButton(new IconButton(_parent))
    , telegramButton(new IconButton(_parent))
    , vkButton(new IconButton(_parent))
    , mailButton(new IconButton(_parent))
    , closeButton(new Button(_parent))
    , buttonsLayout(new QHBoxLayout)
{
    partnerLink->setLink(
        QUrl("https://scriptwriting.courses/?utm_source=starc-desktop-app&utm_medium=referral"));

    twitterButton->setIcon(u8"\U0000f099");
    discordButton->setIcon(u8"\U0000f392");
    telegramButton->setIcon(u8"\U0000f3fe");
    vkButton->setIcon(u8"\U0000f189");
    mailButton->setIcon(u8"\U000F01EE");

    buttonsLayout->setContentsMargins({});
    buttonsLayout->setSpacing(0);
    buttonsLayout->addWidget(twitterButton);
    buttonsLayout->addWidget(discordButton);
    buttonsLayout->addWidget(telegramButton);
    buttonsLayout->addWidget(vkButton);
    buttonsLayout->addWidget(mailButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(closeButton, 0, Qt::AlignBottom);
}


// ****


AboutApplicationDialog::AboutApplicationDialog(QWidget* _parent)
    : AbstractDialog(_parent)
    , d(new Implementation(this))
{
    setTitle("Story Architect");
    setRejectButton(d->closeButton);

    int row = 0;
    contentsLayout()->addWidget(d->versionLabel, row++, 0, Qt::AlignLeft);
    {
        auto layout = new QHBoxLayout;
        layout->setContentsMargins({});
        layout->setSpacing(0);
        layout->addWidget(d->authorsLabel1);
        layout->addWidget(d->authorsLink);
        layout->addWidget(d->authorsLabel2);
        layout->addStretch();
        contentsLayout()->addLayout(layout, row++, 0);
    }
    {
        auto layout = new QHBoxLayout;
        layout->setContentsMargins({});
        layout->setSpacing(0);
        layout->addWidget(d->partnerLabel1);
        layout->addWidget(d->partnerLink);
        layout->addWidget(d->partnerLabel2);
        layout->addStretch();
        contentsLayout()->addLayout(layout, row++, 0);
    }
    contentsLayout()->addWidget(d->descriptionLabel1, row++, 0);
    contentsLayout()->addWidget(d->descriptionLabel2, row++, 0);
    contentsLayout()->addLayout(d->buttonsLayout, row++, 0);


    connect(d->twitterButton, &IconButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://twitter.com/starcapp_")); });
    connect(d->discordButton, &IconButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://discord.gg/8Hjze3UYgQ")); });
    connect(d->vkButton, &IconButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://vk.com/starc_app")); });
    connect(d->telegramButton, &IconButton::clicked, this, [this] {
        QDesktopServices::openUrl(
            QUrl(d->isRussianSpeaking ? "https://t.me/starcapp_ru" : "https://t.me/starcapp"));
    });
    connect(d->mailButton, &IconButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://starc.app/contacts")); });
    connect(d->closeButton, &Button::clicked, this, &AboutApplicationDialog::hideDialog);
}

AboutApplicationDialog::~AboutApplicationDialog() = default;

QWidget* AboutApplicationDialog::focusedWidgetAfterShow() const
{
    return d->closeButton;
}

QWidget* AboutApplicationDialog::lastFocusableWidget() const
{
    return d->closeButton;
}

void AboutApplicationDialog::updateTranslations()
{
    d->isRussianSpeaking = QLocale().language() == QLocale::Russian
        || QLocale().language() == QLocale::Belarusian
        || QLocale().language() == QLocale::Ukrainian;
    d->twitterButton->setVisible(!d->isRussianSpeaking);
    d->discordButton->setVisible(!d->isRussianSpeaking);
    d->vkButton->setVisible(d->isRussianSpeaking);

    d->versionLabel->setText(
        QString("%1 %2").arg(tr("version"), QCoreApplication::applicationVersion()));
    d->versionLabel->setLink(
        QUrl(d->isRussianSpeaking ? "https://starc.app/ru/blog/" : "https://starc.app/blog/"));
    d->authorsLabel1->setText(
        QString("%1 ").arg(tr("Designed, coded and crafted with love at the")));
    d->authorsLink->setLink(
        QUrl(d->isRussianSpeaking ? "https://storyapps.dev/ru.html" : "https://storyapps.dev/"));
    d->authorsLink->setText("Story Apps");
    d->authorsLabel2->setText(QString(" %1.").arg(tr("company")));
    d->partnerLabel1->setText(QString("%1 ").arg(tr("The Logline Generator is powered by")));
    d->partnerLink->setText("Immersion Screenwriting");
    d->partnerLabel2->setText(".");
    d->descriptionLabel1->setText(
        tr("Starc is being developed with strong authors’ involvement and support so we won’t "
           "waste the app performance on the features you don’t need."));
    d->descriptionLabel2->setText(
        tr("Feel free to contact us at any point of your creative journey to share your thoughts "
           "about the app, suggest ideas and report issues you met."));

    d->closeButton->setText("Close");
}

void AboutApplicationDialog::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    AbstractDialog::designSystemChangeEvent(_event);

    auto labelMargins = DesignSystem::label().margins().toMargins();
    labelMargins.setTop(0);
    for (auto label : std::vector<Widget*>{
             d->versionLabel,
             d->authorsLabel1,
             d->authorsLink,
             d->authorsLabel2,
             d->partnerLabel1,
             d->partnerLink,
             d->partnerLabel2,
             d->descriptionLabel1,
             d->descriptionLabel2,
         }) {
        label->setBackgroundColor(DesignSystem::color().background());
        label->setTextColor(DesignSystem::color().onBackground());
        label->setContentsMargins(labelMargins);
    }
    d->versionLabel->setTextColor(ColorHelper::transparent(d->versionLabel->textColor(),
                                                           DesignSystem::inactiveTextOpacity()));
    auto leftLabelMargins = labelMargins;
    leftLabelMargins.setRight(0);
    d->authorsLabel1->setContentsMargins(leftLabelMargins);
    d->partnerLabel1->setContentsMargins(leftLabelMargins);
    auto middleLabelMargins = leftLabelMargins;
    middleLabelMargins.setLeft(0);
    d->authorsLink->setContentsMargins(middleLabelMargins);
    d->authorsLink->setTextColor(DesignSystem::color().accent());
    d->partnerLink->setContentsMargins(middleLabelMargins);
    d->partnerLink->setTextColor(DesignSystem::color().accent());
    auto rightLabelMargins = labelMargins;
    rightLabelMargins.setLeft(0);
    d->authorsLabel2->setContentsMargins(rightLabelMargins);
    d->partnerLabel2->setContentsMargins(rightLabelMargins);

    for (auto iconButton : {
             d->twitterButton,
             d->discordButton,
             d->telegramButton,
             d->vkButton,
         }) {
        iconButton->setBackgroundColor(DesignSystem::color().background());
        iconButton->setTextColor(DesignSystem::color().onBackground());
        iconButton->setCustomFont(DesignSystem::font().brandsMid());
    }
    d->mailButton->setBackgroundColor(DesignSystem::color().background());
    d->mailButton->setTextColor(DesignSystem::color().onBackground());

    UiHelper::initColorsFor(d->closeButton, UiHelper::DialogAccept);

    d->buttonsLayout->setContentsMargins(
        QMarginsF(DesignSystem::layout().px12(), DesignSystem::layout().px12(),
                  DesignSystem::layout().px16(), DesignSystem::layout().px12())
            .toMargins());
}

} // namespace Ui
