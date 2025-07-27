#include "onboarding_navigator.h"

#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/starcloud_api.h>
#include <ui/design_system/design_system.h>
#include <ui/settings/widgets/theme_preview.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/combo_box/combo_box.h>
#include <ui/widgets/icon_button/icon_button.h>
#include <ui/widgets/image/image_cropping_dialog.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/slider/slider.h>
#include <ui/widgets/toggle/toggle.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/image_helper.h>
#include <utils/tools/debouncer.h>
#include <utils/validators/email_validator.h>

#include <QBoxLayout>
#include <QDesktopServices>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QTimer>


namespace Ui {

namespace {

enum LanguageRoles {
    LanguageRole = Qt::UserRole + 1,
    LanguageTranslatedRole,
};

void addToggleWithTitle(QVBoxLayout* pageLayout, Toggle* _toggle, AbstractLabel* _label)
{
    auto layout = new QHBoxLayout;
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(_toggle);
    layout->addWidget(_label, 1, Qt::AlignVCenter);
    pageLayout->addLayout(layout);

    QObject::connect(_label, &AbstractLabel::clicked, _toggle, &Toggle::toggle);
};

struct CompetitorInfo {
    QString name;
    struct {
        QString light;
        QString dark;
    } themes;

    QString theme(ApplicationTheme _theme) const
    {
        return _theme == ApplicationTheme::Dark ? themes.dark : themes.light;
    }
};
const QVector<CompetitorInfo> kCompetitors = {
    { "KIT Scenarist",
      { "ffffff38393a4285f4f8f8f2e4e4e438393af6f6f6000000ec3740f8f8f2000000f8f8f2fefefe000000",
        "404040ebebeb4285f4f8f8f2414244ebebeb26282affffffec3740f8f8f2000000f8f8f23d3d3df8f8f2" } },
    { "Final Draft",
      { "f4eef02626264285f4f8f8f2f6eff3494748ececec000000ec3740f8f8f2000000f8f8f2fefefe000000",
        "312529ebebeb4285f4f8f8f23f383ce3e3e3222222f8f8f2ec3740f8f8f2000000f8f8f2444444ffffff" } },
    { "Arc Studio Pro",
      { "ffffff3333334c26b5f8f8f2ffffff333333f9fafc000000ec3740f8f8f2000000f8f8f2ffffff333333",
        "1d1f21d9dbde7971fcf8f8f21d1f21ebebeb101113f8f8f2ec3740f8f8f2000000f8f8f2161719ffffff" } },
    { "Fade In",
      { "f0f0f00000004196e5f8f8f2e3e3e3141414efefef141414ec3740f8f8f2000000f8f8f2ffffff000000",
        "202020f6f6f54196e5f8f8f2414141fefefe202020f8f8f2ec3740f8f8f2000000f8f8f2373737f3f3f3" } },
    { "Writer Duet",
      { "3a4b59ffffff049effffffffffffff0000008aa5beffffffec3740f8f8f2000000f8f8f2ffffff000000",
        "3a4b59ffffff049effffffff3a4b59ffffff17232edde5ebec3740f8f8f2000000f8f8f22d3b48ffffff" } },
    { "Highland2",
      { "3f4752c4cace37aacfffffffffffff2d333bffffff2d333bec3740f8f8f2000000f8f8f2ffffff2d333b",
        "1c262968848a00b6bdffffff28353aaaaaaa28353aaaaaaae44d66f8f8f2000000f8f8f228353aaaaaaa" } },
    { "Trelby",
      { "ebe7e537383fda924bfffffff6f6f7000000ededed37383fec3740f8f8f2000000f8f8f2ffffff000000",
        "182430d3dce9da924bffffff182430d3dce91f3142b5c1cdec3740f8f8f2000000f8f8f2182430d3dce9" } },
};

} // namespace

class OnboardingNavigator::Implementation
{
public:
    explicit Implementation(OnboardingNavigator* _q);

    void initUiPage();
    void initSignUpPage();
    void initAccountPage();
    void initModulesPage();
    void initStyleChoosePage();
    void initBackupsPage();
    void initSocialPage();

    /**
     * @brief Обновить аватарку как реакцию на изменение размера виджета в котором она отображается
     */
    void updateAccountAvatar();


    OnboardingNavigator* q = nullptr;

    ApplicationTheme selectedTheme = ApplicationTheme::Light;

    Widget* uiPage = nullptr;
    ImageLabel* uiLogo = nullptr;
    H6Label* uiTitle = nullptr;
    Body2Label* uiSubtitle = nullptr;
    ComboBox* uiLanguage = nullptr;
    QStandardItemModel* uiLanguageModel = nullptr;
    Subtitle2Label* uiThemeTitle = nullptr;
    QHBoxLayout* uiThemeLayout = nullptr;
    ThemePreview* uiLightTheme = nullptr;
    ThemePreview* uiDarkAndLightTheme = nullptr;
    ThemePreview* uiDarkTheme = nullptr;
    Subtitle2Label* uiScaleTitle = nullptr;
    Slider* uiScaleSlider = nullptr;
    Body2Label* uiScaleSmallInfoLabel = nullptr;
    Body2Label* uiScaleBigInfoLabel = nullptr;
    Button* uiContinueButton = nullptr;

    Widget* signInPage = nullptr;
    H6Label* signInTitle = nullptr;
    Body2Label* signInSubtitle = nullptr;
    TextField* signInEmail = nullptr;
    Body1Label* signInAuthorizationError = nullptr;
    Body1Label* signInConfirmationCodeInfo = nullptr;
    TextField* signInConfirmationCode = nullptr;
    QTimer signInConfirmationCodeExpirationTimer;
    Button* signInSignInButton = nullptr;
    Button* signInResendCodeButton = nullptr;
    Button* signInContinueButton = nullptr;

    Widget* accountPage = nullptr;
    H6Label* accountTitle = nullptr;
    Body2Label* accountSubtitle = nullptr;
    ImageLabel* accountAvatar = nullptr;
    Button* accountChangeAvatarButton = nullptr;
    TextField* accountName = nullptr;
    Debouncer accountChangeNameDebouncer{ 500 };
    TextField* accountDescription = nullptr;
    Debouncer accountChangeDescriptionDebouncer{ 500 };
    CheckBox* accountSubscription = nullptr;
    Button* accountContinueButton = nullptr;
    Domain::AccountInfo accountInfo;

    Widget* modulesPage = nullptr;
    ImageLabel* modulesLogo = nullptr;
    H6Label* modulesTitle = nullptr;
    Body2Label* modulesSubtitle = nullptr;
    Subtitle1Label* modulesDescription = nullptr;
    Toggle* modulesScreenplayToggle = nullptr;
    Body1Label* modulesScreenplayTitle = nullptr;
    Toggle* modulesComicBookToggle = nullptr;
    Body1Label* modulesComicBookTitle = nullptr;
    Toggle* modulesAudioplayToggle = nullptr;
    Body1Label* modulesAudioplayTitle = nullptr;
    Toggle* modulesStageplayToggle = nullptr;
    Body1Label* modulesStageplayTitle = nullptr;
    Toggle* modulesNovelToggle = nullptr;
    Body1Label* modulesNovelTitle = nullptr;
    Button* modulesContinueButton = nullptr;

    Widget* styleChoosePage = nullptr;
    ImageLabel* styleChooseLogo = nullptr;
    H6Label* styleChooseTitle = nullptr;
    Body2Label* styleChooseSubtitle = nullptr;
    Subtitle1Label* styleChooseDescription = nullptr;
    Toggle* styleChooseActivateToggle = nullptr;
    Body1Label* styleChooseActivateTitle = nullptr;
    ComboBox* styleChooseComboBox = nullptr;
    QStandardItemModel* styleChooseModel = nullptr;
    Button* styleChooseContinueButton = nullptr;

    Widget* backupsPage = nullptr;
    ImageLabel* backupsLogo = nullptr;
    H6Label* backupsTitle = nullptr;
    Body2Label* backupsSubtitle = nullptr;
    Subtitle1Label* backupsDescription = nullptr;
    Button* backupsContinueButton = nullptr;

    Widget* socialPage = nullptr;
    ImageLabel* socialLogo = nullptr;
    H6Label* socialTitle = nullptr;
    Body2Label* socialSubtitle = nullptr;
    Subtitle1Label* socialDescription = nullptr;
    IconButton* socialTwitterButton = nullptr;
    IconButton* socialDiscordButton = nullptr;
    IconButton* socialTelegramButton = nullptr;
    IconButton* socialVkButton = nullptr;
    IconButton* socialFacebookButton = nullptr;
    Button* socialContinueButton = nullptr;
};

OnboardingNavigator::Implementation::Implementation(OnboardingNavigator* _q)
    : q(_q)
    , uiPage(new Widget(q))
    , uiLogo(new ImageLabel(uiPage))
    , uiTitle(new H6Label(uiPage))
    , uiSubtitle(new Body2Label(uiPage))
    , uiLanguage(new ComboBox(uiPage))
    , uiLanguageModel(new QStandardItemModel(uiLanguage))
    , uiThemeTitle(new Subtitle2Label(uiPage))
    , uiThemeLayout(new QHBoxLayout)
    , uiLightTheme(new ThemePreview(uiPage))
    , uiDarkAndLightTheme(new ThemePreview(uiPage))
    , uiDarkTheme(new ThemePreview(uiPage))
    , uiScaleTitle(new Subtitle2Label(uiPage))
    , uiScaleSlider(new Slider(uiPage))
    , uiScaleSmallInfoLabel(new Body2Label(uiPage))
    , uiScaleBigInfoLabel(new Body2Label(uiPage))
    , uiContinueButton(new Button(uiPage))
    //
    , signInPage(new Widget(q))
    , signInTitle(new H6Label(signInPage))
    , signInSubtitle(new Body2Label(signInPage))
    , signInEmail(new TextField(signInPage))
    , signInAuthorizationError(new Body1Label(signInPage))
    , signInConfirmationCodeInfo(new Body1Label(signInPage))
    , signInConfirmationCode(new TextField(signInPage))
    , signInSignInButton(new Button(signInPage))
    , signInResendCodeButton(new Button(signInPage))
    , signInContinueButton(new Button(signInPage))
    //
    , accountPage(new Widget(q))
    , accountTitle(new H6Label(accountPage))
    , accountSubtitle(new Body2Label(uiPage))
    , accountAvatar(new ImageLabel(accountPage))
    , accountChangeAvatarButton(new Button(accountPage))
    , accountName(new TextField(accountPage))
    , accountDescription(new TextField(accountPage))
    , accountSubscription(new CheckBox(accountPage))
    , accountContinueButton(new Button(accountPage))
    //
    , modulesPage(new Widget(q))
    , modulesLogo(new ImageLabel(modulesPage))
    , modulesTitle(new H6Label(modulesPage))
    , modulesSubtitle(new Body2Label(modulesPage))
    , modulesDescription(new Subtitle1Label(modulesPage))
    , modulesScreenplayToggle(new Toggle(modulesPage))
    , modulesScreenplayTitle(new Body1Label(modulesPage))
    , modulesComicBookToggle(new Toggle(modulesPage))
    , modulesComicBookTitle(new Body1Label(modulesPage))
    , modulesAudioplayToggle(new Toggle(modulesPage))
    , modulesAudioplayTitle(new Body1Label(modulesPage))
    , modulesStageplayToggle(new Toggle(modulesPage))
    , modulesStageplayTitle(new Body1Label(modulesPage))
    , modulesNovelToggle(new Toggle(modulesPage))
    , modulesNovelTitle(new Body1Label(modulesPage))
    , modulesContinueButton(new Button(modulesPage))
    //
    , styleChoosePage(new Widget(q))
    , styleChooseLogo(new ImageLabel(styleChoosePage))
    , styleChooseTitle(new H6Label(styleChoosePage))
    , styleChooseSubtitle(new Body2Label(styleChoosePage))
    , styleChooseDescription(new Subtitle1Label(styleChoosePage))
    , styleChooseActivateToggle(new Toggle(styleChoosePage))
    , styleChooseActivateTitle(new Body1Label(styleChoosePage))
    , styleChooseComboBox(new ComboBox(styleChoosePage))
    , styleChooseModel(new QStandardItemModel(styleChooseComboBox))
    , styleChooseContinueButton(new Button(styleChoosePage))
    //
    , backupsPage(new Widget(q))
    , backupsLogo(new ImageLabel(backupsPage))
    , backupsTitle(new H6Label(backupsPage))
    , backupsSubtitle(new Body2Label(backupsPage))
    , backupsDescription(new Subtitle1Label(backupsPage))
    , backupsContinueButton(new Button(backupsPage))
    //
    , socialPage(new Widget(q))
    , socialLogo(new ImageLabel(socialPage))
    , socialTitle(new H6Label(socialPage))
    , socialSubtitle(new Body2Label(socialPage))
    , socialDescription(new Subtitle1Label(socialPage))
    , socialTwitterButton(new IconButton(socialPage))
    , socialDiscordButton(new IconButton(socialPage))
    , socialTelegramButton(new IconButton(socialPage))
    , socialVkButton(new IconButton(socialPage))
    , socialFacebookButton(new IconButton(socialPage))
    , socialContinueButton(new Button(socialPage))
{
    initUiPage();
    initSignUpPage();
    initAccountPage();
    initModulesPage();
    initStyleChoosePage();
    initBackupsPage();
    initSocialPage();
}

void OnboardingNavigator::Implementation::initUiPage()
{
    uiLogo->setImage(QPixmap(":/images/logo"));
    uiTitle->setAlignment(Qt::AlignCenter);
    uiSubtitle->setAlignment(Qt::AlignCenter);
    QStandardItem* defaultLanguage = nullptr;
    auto addLanguage
        = [this, &defaultLanguage](const QString& _languageName, QLocale::Language _language,
                                   qreal _trnaslated) -> QStandardItem* {
        if (_trnaslated <= 20) {
            return nullptr;
        }

        auto languageItem = new QStandardItem(_languageName);
        languageItem->setData(_language, LanguageRole);
        languageItem->setData(_trnaslated, LanguageTranslatedRole);
        uiLanguageModel->appendRow(languageItem);

        //
        // Если умеем в системный язык, запомним его
        //
        if (QLocale().system().language() == _language) {
            defaultLanguage = languageItem;
        }

        return languageItem;
    };
    addLanguage("Azərbaycan", QLocale::Azerbaijani, 77);
    addLanguage("Беларуский", QLocale::Belarusian, 41);
    addLanguage("Català", QLocale::Catalan, 68);
    addLanguage("Dansk", QLocale::Danish, 75);
    addLanguage("Deutsch", QLocale::German, 92);
    auto englishItem = addLanguage("English", QLocale::English, 100);
    addLanguage("Español", QLocale::Spanish, 95);
    addLanguage("Esperanto", QLocale::Esperanto, 7);
    addLanguage("Français", QLocale::French, 72);
    addLanguage("Galego", QLocale::Galician, 46);
    addLanguage("Hrvatski", QLocale::Croatian, 44);
    addLanguage("Indonesian", QLocale::Indonesian, 100);
    addLanguage("Italiano", QLocale::Italian, 53);
    addLanguage("Magyar", QLocale::Hungarian, 28);
    addLanguage("Nederlands", QLocale::Dutch, 93);
    addLanguage("Polski", QLocale::Polish, 95);
    addLanguage("Português", static_cast<QLocale::Language>(QLocale::LastLanguage + 1), 9);
    addLanguage("Português Brasileiro", QLocale::Portuguese, 97);
    addLanguage("Română", QLocale::Romanian, 40);
    addLanguage("Русский", QLocale::Russian, 100);
    addLanguage("Slovenski", QLocale::Slovenian, 100);
    addLanguage("Svenska", QLocale::Swedish, 29);
    addLanguage("Tagalog", QLocale::Filipino, 13);
    addLanguage("Türkçe", QLocale::Turkish, 89);
    addLanguage("Українська", QLocale::Ukrainian, 97);
    addLanguage("اَلْعَرَبِيَّةُ", QLocale::Arabic, 79);
    addLanguage("فارسی", QLocale::Persian, 52);
    addLanguage("עִבְרִית", QLocale::Hebrew, 75);
    addLanguage("हिन्दी", QLocale::Hindi, 25);
    addLanguage("தமிழ்", QLocale::Tamil, 31);
    addLanguage("తెలుగు", QLocale::Telugu, 97);
    addLanguage("Հայերեն", QLocale::Armenian, 13);
    addLanguage("汉语", QLocale::Chinese, 84);
    addLanguage("한국어", QLocale::Korean, 48);
    uiLanguage->setModel(uiLanguageModel);
    //
    // Выбираем язык в зависимости от того, умеем ли мы в системный
    //
    if (defaultLanguage != nullptr) {
        uiLanguage->setCurrentText(defaultLanguage->text());
    } else {
        uiLanguage->setCurrentText(englishItem->text());
    }
    uiLightTheme->setTheme(ApplicationTheme::Light);
    uiDarkAndLightTheme->setTheme(ApplicationTheme::DarkAndLight);
    uiDarkTheme->setTheme(ApplicationTheme::Dark);
    uiScaleSlider->setMaximumValue(3500);
    uiScaleSlider->setValue(500);
    uiScaleSlider->setDefaultValue(500);
    uiContinueButton->setContained(true);

    auto pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins({});
    pageLayout->setSpacing(0);
    pageLayout->addWidget(uiLogo, 0, Qt::AlignHCenter);
    pageLayout->addWidget(uiTitle);
    pageLayout->addWidget(uiSubtitle);
    pageLayout->addWidget(uiLanguage);
    pageLayout->addWidget(uiThemeTitle);
    {
        uiThemeLayout->setContentsMargins({});
        uiThemeLayout->setSpacing(0);
        uiThemeLayout->addWidget(uiLightTheme);
        uiThemeLayout->addWidget(uiDarkAndLightTheme);
        uiThemeLayout->addWidget(uiDarkTheme);
        pageLayout->addLayout(uiThemeLayout);
    }
    pageLayout->addWidget(uiScaleTitle);
    pageLayout->addWidget(uiScaleSlider);
    {
        auto layout = new QHBoxLayout;
        layout->setContentsMargins({});
        layout->setSpacing(0);
        layout->addWidget(uiScaleSmallInfoLabel);
        layout->addStretch();
        layout->addWidget(uiScaleBigInfoLabel);
        pageLayout->addLayout(layout);
    }
    pageLayout->addStretch();
    pageLayout->addWidget(uiContinueButton);
    uiPage->setLayout(pageLayout);
}

void OnboardingNavigator::Implementation::initSignUpPage()
{
    signInTitle->setAlignment(Qt::AlignCenter);
    signInSubtitle->setAlignment(Qt::AlignCenter);
    signInAuthorizationError->hide();
    signInConfirmationCodeInfo->hide();
    signInEmail->setCapitalizeWords(false);
    signInEmail->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    signInConfirmationCode->setCapitalizeWords(false);
    signInConfirmationCode->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    signInConfirmationCode->hide();
    signInConfirmationCodeExpirationTimer.setSingleShot(true);
    signInConfirmationCodeExpirationTimer.setInterval(std::chrono::minutes{ 10 });
    signInSignInButton->setContained(true);
    signInSignInButton->hide();
    signInResendCodeButton->setContained(true);
    signInResendCodeButton->hide();

    auto pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins({});
    pageLayout->setSpacing(0);
    pageLayout->addWidget(signInTitle);
    pageLayout->addWidget(signInSubtitle);
    pageLayout->addWidget(signInEmail);
    pageLayout->addWidget(signInAuthorizationError);
    pageLayout->addWidget(signInConfirmationCodeInfo);
    pageLayout->addWidget(signInConfirmationCode);
    pageLayout->addWidget(signInSignInButton);
    pageLayout->addWidget(signInResendCodeButton);
    pageLayout->addStretch();
    pageLayout->addWidget(signInContinueButton, 0, Qt::AlignHCenter);
    signInPage->setLayout(pageLayout);
}

void OnboardingNavigator::Implementation::initAccountPage()
{
    accountTitle->setAlignment(Qt::AlignCenter);
    accountSubtitle->setAlignment(Qt::AlignCenter);
    accountName->setCapitalizeWords(false);
    accountName->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    accountDescription->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    accountContinueButton->setContained(true);

    auto pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins({});
    pageLayout->setSpacing(0);
    pageLayout->addWidget(accountTitle);
    pageLayout->addWidget(accountSubtitle);
    pageLayout->addWidget(accountAvatar, 0, Qt::AlignHCenter);
    pageLayout->addWidget(accountChangeAvatarButton, 0, Qt::AlignHCenter);
    pageLayout->addWidget(accountName);
    pageLayout->addWidget(accountDescription);
    pageLayout->addWidget(accountSubscription);
    pageLayout->addStretch();
    pageLayout->addWidget(accountContinueButton);
    accountPage->setLayout(pageLayout);
}

void OnboardingNavigator::Implementation::initModulesPage()
{
    modulesLogo->setImage(QPixmap(":/images/logo"));
    modulesTitle->setAlignment(Qt::AlignCenter);
    modulesSubtitle->setAlignment(Qt::AlignCenter);
    modulesContinueButton->setContained(true);

    auto pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins({});
    pageLayout->setSpacing(0);
    pageLayout->addWidget(modulesLogo, 0, Qt::AlignHCenter);
    pageLayout->addWidget(modulesTitle);
    pageLayout->addWidget(modulesSubtitle);
    pageLayout->addWidget(modulesDescription);

    addToggleWithTitle(pageLayout, modulesScreenplayToggle, modulesScreenplayTitle);
    addToggleWithTitle(pageLayout, modulesComicBookToggle, modulesComicBookTitle);
    addToggleWithTitle(pageLayout, modulesAudioplayToggle, modulesAudioplayTitle);
    addToggleWithTitle(pageLayout, modulesStageplayToggle, modulesStageplayTitle);
    addToggleWithTitle(pageLayout, modulesNovelToggle, modulesNovelTitle);
    pageLayout->addStretch();
    pageLayout->addWidget(modulesContinueButton);
    modulesPage->setLayout(pageLayout);
}

void OnboardingNavigator::Implementation::initStyleChoosePage()
{
    styleChooseLogo->setImage(QPixmap(":/images/logo"));
    styleChooseTitle->setAlignment(Qt::AlignCenter);
    styleChooseSubtitle->setAlignment(Qt::AlignCenter);
    styleChooseContinueButton->setContained(true);

    for (const auto& competitor : kCompetitors) {
        styleChooseModel->appendRow(new QStandardItem(competitor.name));
    }
    styleChooseComboBox->setModel(styleChooseModel);
    styleChooseComboBox->setEnabled(false);

    auto pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins({});
    pageLayout->setSpacing(0);
    pageLayout->addWidget(styleChooseLogo, 0, Qt::AlignHCenter);
    pageLayout->addWidget(styleChooseTitle);
    pageLayout->addWidget(styleChooseSubtitle);
    pageLayout->addWidget(styleChooseDescription);
    addToggleWithTitle(pageLayout, styleChooseActivateToggle, styleChooseActivateTitle);
    pageLayout->addWidget(styleChooseComboBox);
    pageLayout->addStretch();
    pageLayout->addWidget(styleChooseContinueButton);
    styleChooseActivateToggle->setChecked(false);
    styleChoosePage->setLayout(pageLayout);
}

void OnboardingNavigator::Implementation::initBackupsPage()
{
    backupsLogo->setImage(QPixmap(":/images/logo"));
    backupsTitle->setAlignment(Qt::AlignCenter);
    backupsSubtitle->setAlignment(Qt::AlignCenter);
    backupsContinueButton->setContained(true);

    auto pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins({});
    pageLayout->setSpacing(0);
    pageLayout->addWidget(backupsLogo, 0, Qt::AlignHCenter);
    pageLayout->addWidget(backupsTitle);
    pageLayout->addWidget(backupsSubtitle);
    pageLayout->addWidget(backupsDescription);
    pageLayout->addStretch();
    pageLayout->addWidget(backupsContinueButton);
    backupsPage->setLayout(pageLayout);
}

void OnboardingNavigator::Implementation::initSocialPage()
{
    socialLogo->setImage(QPixmap(":/images/logo"));
    socialTitle->setAlignment(Qt::AlignCenter);
    socialSubtitle->setAlignment(Qt::AlignCenter);
    socialTwitterButton->setIcon(u8"\U0000f099");
    socialDiscordButton->setIcon(u8"\U0000f392");
    socialTelegramButton->setIcon(u8"\U0000f3fe");
    socialVkButton->setIcon(u8"\U0000f189");
    socialFacebookButton->setIcon(u8"\U0000f09a");
    socialContinueButton->setContained(true);

    auto pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins({});
    pageLayout->setSpacing(0);
    pageLayout->addWidget(socialLogo, 0, Qt::AlignHCenter);
    pageLayout->addWidget(socialTitle);
    pageLayout->addWidget(socialSubtitle);
    pageLayout->addWidget(socialDescription);
    {
        auto layout = new QHBoxLayout;
        layout->setContentsMargins({});
        layout->setSpacing(0);
        layout->addStretch();
        layout->addWidget(socialTwitterButton);
        layout->addWidget(socialDiscordButton);
        layout->addWidget(socialTelegramButton);
        layout->addWidget(socialVkButton);
        layout->addWidget(socialFacebookButton);
        layout->addStretch();
        pageLayout->addLayout(layout);
    }
    pageLayout->addStretch();
    pageLayout->addWidget(socialContinueButton);
    socialPage->setLayout(pageLayout);
}

void OnboardingNavigator::Implementation::updateAccountAvatar()
{
    const auto avatarSize = accountAvatar->contentsRect().size();
    if (accountInfo.avatar.isEmpty()) {
        accountAvatar->setImage(ImageHelper::makeAvatar(
            accountInfo.name, Ui::DesignSystem::font().h6(), avatarSize, Qt::white));
    } else {
        accountAvatar->setImage(
            ImageHelper::makeAvatar(ImageHelper::imageFromBytes(accountInfo.avatar), avatarSize));
    }
}


// ****


OnboardingNavigator::OnboardingNavigator(QWidget* _parent)
    : StackWidget(_parent)
    , d(new Implementation(this))
{
    setAnimationType(AnimationType::Slide);

    setCurrentWidget(d->uiPage);
    addWidget(d->signInPage);
    addWidget(d->accountPage);
    addWidget(d->styleChoosePage);
    addWidget(d->modulesPage);
    addWidget(d->backupsPage);
    addWidget(d->socialPage);


    connect(d->uiLanguage, &ComboBox::currentIndexChanged, this, [this](const QModelIndex& _index) {
        if (const auto translationProgress = _index.data(LanguageTranslatedRole).toInt();
            translationProgress < 100) {
            d->uiLanguage->setHelper(tr("Translation is ready for %1%").arg(translationProgress));
        } else {
            d->uiLanguage->setHelper({});
        }

        emit languageChanged(static_cast<QLocale::Language>(_index.data(LanguageRole).toInt()));
    });
    for (auto themePreview : {
             d->uiLightTheme,
             d->uiDarkAndLightTheme,
             d->uiDarkTheme,
         }) {
        connect(themePreview, &ThemePreview::themePressed, this, [this](ApplicationTheme _theme) {
            d->selectedTheme = _theme;
            emit themeChanged(_theme);
        });
    }
    connect(d->uiScaleSlider, &Slider::valueChanged, this, [this](int _value) {
        emit scaleFactorChanged(0.5 + static_cast<qreal>(_value) / 1000.0);
    });
    connect(d->uiContinueButton, &Button::clicked, this, [this] {
        if (!d->accountInfo.isValid()) {
            setCurrentWidget(d->signInPage);
            QTimer::singleShot(animationDuration(), this, [this] { d->signInEmail->setFocus(); });
        } else {
            setCurrentWidget(d->accountPage);
            QTimer::singleShot(animationDuration(), this, [this] {
                QTimer::singleShot(animationDuration(), this,
                                   [this] { d->accountName->setFocus(); });
            });
        }
    });
    //
    connect(d->signInEmail, &TextField::textChanged, this, [this] {
        d->signInEmail->clearError();
        d->signInAuthorizationError->hide();
        d->signInConfirmationCodeInfo->hide();
        d->signInConfirmationCode->hide();
        d->signInConfirmationCode->clear();
        d->signInResendCodeButton->hide();
        d->signInSignInButton->setVisible(EmailValidator::isValid(d->signInEmail->text()));
    });
    connect(d->signInEmail, &TextField::enterPressed, d->signInSignInButton, &Button::click);
    connect(d->signInConfirmationCode, &TextField::textChanged, this, [this] {
        const auto code = d->signInConfirmationCode->text();
        if (code.isEmpty()) {
            return;
        }

        emit confirmationCodeChanged(code);
    });
    connect(&d->signInConfirmationCodeExpirationTimer, &QTimer::timeout, this, [this] {
        d->signInAuthorizationError->hide();
        d->signInConfirmationCodeInfo->setText(tr("The confirmation code we've sent expired."));
        d->signInResendCodeButton->show();
        d->signInResendCodeButton->setFocus();
        d->signInConfirmationCode->hide();
        d->signInSignInButton->hide();
    });
    connect(d->signInSignInButton, &Button::clicked, this, [this] {
        if (!EmailValidator::isValid(d->signInEmail->text())) {
            d->signInEmail->setError(tr("Email invalid"));
            return;
        }

        emit signInPressed();
    });
    connect(d->signInResendCodeButton, &Button::clicked, d->signInSignInButton, &Button::click);
    connect(d->signInContinueButton, &Button::clicked, this,
            [this] { setCurrentWidget(d->styleChoosePage); });
    //
    auto notifyAccountChanged = [this] {
        emit accountInfoChanged(d->accountInfo.email, d->accountInfo.name,
                                d->accountInfo.description, d->accountInfo.newsletterLanguage,
                                d->accountInfo.newsletterSubscribed, d->accountInfo.avatar);
    };
    connect(d->accountChangeAvatarButton, &Button::clicked, this, [this, notifyAccountChanged] {
        const auto imagesFolders
            = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
        const auto imagesFolder = !imagesFolders.isEmpty() ? imagesFolders.constFirst() : "";
        const QString imagePath = QFileDialog::getOpenFileName(
            this, tr("Choose image"), imagesFolder,
            QString("%1 (*.png *.jpeg *.jpg *.bmp *.tiff *.tif *.gif)").arg(tr("Images")));
        if (imagePath.isEmpty()) {
            return;
        }

        auto dlg = new ImageCroppingDialog(topLevelWidget());
        dlg->setImage(QPixmap(imagePath));
        dlg->setImageProportion({ 1, 1 });
        dlg->setImageProportionFixed(true);
        dlg->setImageCroppingText(tr("Select an area for the avatar"));
        connect(dlg, &ImageCroppingDialog::disappeared, dlg, &ImageCroppingDialog::deleteLater);
        connect(dlg, &ImageCroppingDialog::imageSelected, this,
                [this, notifyAccountChanged](const QPixmap& _image) {
                    d->accountInfo.avatar = ImageHelper::bytesFromImage(_image);
                    d->updateAccountAvatar();
                    notifyAccountChanged();
                });

        dlg->showDialog();
    });
    connect(d->accountName, &TextField::textChanged, &d->accountChangeNameDebouncer,
            &Debouncer::orderWork);
    connect(&d->accountChangeNameDebouncer, &Debouncer::gotWork, this,
            [this, notifyAccountChanged] {
                if (d->accountName->text().isEmpty()) {
                    d->accountName->setError(tr("Username can't be empty, please fill it"));
                    return;
                }

                d->accountName->setError({});
                d->accountInfo.name = d->accountName->text();
                notifyAccountChanged();
            });
    connect(d->accountDescription, &TextField::textChanged, &d->accountChangeDescriptionDebouncer,
            &Debouncer::orderWork);
    connect(&d->accountChangeDescriptionDebouncer, &Debouncer::gotWork, this,
            [this, notifyAccountChanged] {
                d->accountInfo.description = d->accountDescription->text();
                notifyAccountChanged();
            });
    connect(d->accountSubscription, &CheckBox::checkedChanged, this, [this, notifyAccountChanged] {
        d->accountInfo.newsletterLanguage = QLocale().language() == QLocale::Russian ? "ru" : "en";
        d->accountInfo.newsletterSubscribed = d->accountSubscription->isChecked();
        notifyAccountChanged();
    });
    connect(d->accountContinueButton, &Button::clicked, this,
            [this] { setCurrentWidget(d->styleChoosePage); });
    //
    connect(d->modulesScreenplayToggle, &Toggle::checkedChanged, this, [](bool _checked) {
        setSettingsValue(DataStorageLayer::kComponentsScreenplayAvailableKey, _checked);
    });
    connect(d->modulesComicBookToggle, &Toggle::checkedChanged, this, [](bool _checked) {
        setSettingsValue(DataStorageLayer::kComponentsComicBookAvailableKey, _checked);
    });
    connect(d->modulesAudioplayToggle, &Toggle::checkedChanged, this, [](bool _checked) {
        setSettingsValue(DataStorageLayer::kComponentsAudioplayAvailableKey, _checked);
    });
    connect(d->modulesStageplayToggle, &Toggle::checkedChanged, this, [](bool _checked) {
        setSettingsValue(DataStorageLayer::kComponentsStageplayAvailableKey, _checked);
    });
    connect(d->modulesNovelToggle, &Toggle::checkedChanged, this, [](bool _checked) {
        setSettingsValue(DataStorageLayer::kComponentsNovelAvailableKey, _checked);
    });
    connect(d->modulesContinueButton, &Button::clicked, this,
            [this] { setCurrentWidget(d->backupsPage); });
    //
    connect(d->styleChooseActivateToggle, &Toggle::checkedChanged, this, [this](bool _checked) {
        d->styleChooseComboBox->setEnabled(_checked);

        if (_checked) {
            const auto& themeHash = kCompetitors.at(d->styleChooseComboBox->currentIndex().row())
                                        .theme(d->selectedTheme);
            emit useCustomThemeRequested(themeHash);
        } else {
            emit themeChanged(d->selectedTheme);
        }
    });
    connect(d->styleChooseContinueButton, &Button::clicked, this,
            [this] { setCurrentWidget(d->modulesPage); });
    connect(d->styleChooseComboBox, &ComboBox::currentIndexChanged, this,
            [this](const QModelIndex& _index) {
                const auto& themeHash = kCompetitors.at(_index.row()).theme(d->selectedTheme);
                emit useCustomThemeRequested(themeHash);
            });

    //
    connect(d->backupsContinueButton, &Button::clicked, this,
            [this] { setCurrentWidget(d->socialPage); });
    //
    connect(d->socialTwitterButton, &IconButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://twitter.com/starcapp_")); });
    connect(d->socialDiscordButton, &IconButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://discord.gg/8Hjze3UYgQ")); });
    connect(d->socialTelegramButton, &IconButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QLocale().language() == QLocale::Russian
                                               || QLocale().language() == QLocale::Belarusian
                                           ? "https://t.me/starcapp_ru"
                                           : "https://t.me/starcapp"));
    });
    connect(d->socialVkButton, &IconButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl("https://vk.com/starc_app")); });
    connect(d->socialFacebookButton, &IconButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QLocale().language() == QLocale::Russian
                                               || QLocale().language() == QLocale::Belarusian
                                           ? "https://www.facebook.com/starc.application.ru/"
                                           : "https://www.facebook.com/starc.application/"));
    });
    //
    connect(d->socialContinueButton, &Button::clicked, this,
            &OnboardingNavigator::finishOnboardingRequested);
}

OnboardingNavigator::~OnboardingNavigator() = default;

void OnboardingNavigator::showWelcomePage()
{
    d->uiPage->setFocus();
    d->uiScaleSlider->setValue(d->uiScaleSlider->defaultValue());

    setCurrentWidget(d->uiPage);
}

QString OnboardingNavigator::email() const
{
    return d->signInEmail->text();
}

void OnboardingNavigator::showConfirmationCodeStep()
{
    d->signInAuthorizationError->hide();
    d->signInConfirmationCodeInfo->setText(
        tr("We've sent a confirmation code to the e-mail above, please enter it here to verify"));
    d->signInConfirmationCodeInfo->show();
    d->signInConfirmationCode->show();
    d->signInConfirmationCode->setFocus();
    d->signInSignInButton->hide();
    d->signInResendCodeButton->hide();
}

QString OnboardingNavigator::confirmationCode() const
{
    return d->signInConfirmationCode->text();
}

void OnboardingNavigator::setAuthorizationError(const QString& _error)
{
    d->signInAuthorizationError->setText(_error);
    d->signInAuthorizationError->show();
}

void OnboardingNavigator::showAccountPage()
{
    //
    // Не показываем страницу аккаунта, пока не получим данные пользователя, иначе во время анимации
    // будет некрасиво наполянться контент и провисать количество кадров
    //
}

void OnboardingNavigator::setAccountInfo(const Domain::AccountInfo& _accountInfo)
{
    d->accountInfo = _accountInfo;

    d->updateAccountAvatar();
    if (!d->accountChangeNameDebouncer.hasPendingWork()) {
        const auto signalBlocker = QSignalBlocker(d->accountName);
        d->accountName->setText(d->accountInfo.name);
    }
    if (!d->accountChangeDescriptionDebouncer.hasPendingWork()) {
        const auto signalBlocker = QSignalBlocker(d->accountDescription);
        d->accountDescription->setText(d->accountInfo.description);
    }
    const auto signalBlocker = QSignalBlocker(d->accountSubscription);
    d->accountSubscription->setChecked(d->accountInfo.newsletterSubscribed);

    if (currentWidget() == d->signInPage) {
        QTimer::singleShot(animationDuration(), this, [this] {
            setCurrentWidget(d->accountPage);
            QTimer::singleShot(animationDuration(), this, [this] { d->accountName->setFocus(); });
        });
    }
}

void OnboardingNavigator::updateTranslations()
{
    d->uiTitle->setText(tr("Welcome to the Story Architect"));
    d->uiSubtitle->setText(tr("Let's configure something before start"));
    d->uiLanguage->setLabel(tr("Language"));
    if (const auto translationProgress
        = d->uiLanguage->currentIndex().data(LanguageTranslatedRole).toInt();
        translationProgress < 100) {
        d->uiLanguage->setHelper(tr("Translation is ready for %1%").arg(translationProgress));
    }
    d->uiThemeTitle->setText(tr("Choose application theme"));
    d->uiScaleTitle->setText(tr("Setup size of the user interface elements"));
    d->uiScaleSmallInfoLabel->setText(tr("small"));
    d->uiScaleBigInfoLabel->setText(tr("big"));
    d->uiContinueButton->setText(tr("Continue"));

    d->signInTitle->setText(
        tr("Sign in to get access to the extended\nFREE, PRO and CLOUD features"));
    d->signInSubtitle->setText(tr("Such as writing sprints, writing statistics, worldbuilding "
                                  "tools, story development tools, collaboration and more"));
    d->signInEmail->setLabel(tr("Email"));
    d->signInConfirmationCode->setLabel(tr("Confirmation code"));
    d->signInSignInButton->setText(tr("Sign up"));
    d->signInResendCodeButton->setText(tr("Send code again"));
    d->signInContinueButton->setText(tr("Continue without sign in"));

    d->accountTitle->setText(tr("Set up your account"));
    d->accountSubtitle->setText(tr("Personalize your workspace"));
    d->accountChangeAvatarButton->setText(tr("Change avatar"));
    d->accountName->setLabel(tr("Your name"));
    d->accountDescription->setLabel(tr("Your bio"));
    d->accountSubscription->setText(tr("I want to receive STARC news"));
    d->accountContinueButton->setText(tr("Continue"));

    d->modulesTitle->setText(tr("Customize your workspace"));
    d->modulesSubtitle->setText(tr("Feel at home"));
    d->modulesDescription->setText(
        tr("Story Architect provides tools for working with any form of writing. Choose which ones "
           "you plan to use:"));
    d->modulesScreenplayTitle->setText(tr("Screenplay"));
    d->modulesComicBookTitle->setText(tr("Comic book"));
    d->modulesAudioplayTitle->setText(tr("Audioplay"));
    d->modulesStageplayTitle->setText(tr("Stageplay"));
    d->modulesNovelTitle->setText(tr("Novel"));
    d->modulesContinueButton->setText(tr("Continue"));

    d->styleChooseTitle->setText(tr("Customize your work environment"));
    d->styleChooseSubtitle->setText(tr("Feel at home"));
    d->styleChooseDescription->setText(
        tr("We'll help you get used to STARC by leveraging your experience with other "
           "applications."));
    d->styleChooseContinueButton->setText(tr("Continue"));
    d->styleChooseComboBox->setLabel(tr("Application"));
    d->styleChooseActivateTitle->setText(tr("Adapt the interface and workflow"));

    d->backupsTitle->setText(tr("Before you get started"));
    d->backupsSubtitle->setText(tr("Feel our care"));
    d->backupsDescription->setText(
        tr("You should know that Story Architect cares about the safety of your work, so the "
           "app:\n\n"
           "• automatically saves changes every three seconds when there is no activity\n\n"
           "• automatically saves changes every three minutes when you are actively working\n\n"
           "• automatically creates backup copies of local and cloud projects in the folder "
           "\"%1\"\n\n"
           "This will help to protect your creativity in any unforeseen situation.")
            .arg(settingsValue(DataStorageLayer::kApplicationBackupsFolderKey).toString()));
    d->backupsContinueButton->setText(tr("Thanks I'll know"));

    d->socialTitle->setText(tr("Follow us on social media"));
    d->socialSubtitle->setText(tr("Let's unite to make the best app for writers"));
    d->socialDescription->setText(tr(
        "Be the first to know our news, fresh updates and special offers.\n\n"
        "Get in touch with our technical support, share your feedback and suggest improvements.\n\n"
        "Discuss everything with fellow community of writers, share your work in progress and chat "
        "about life."));
    const auto isRussianSpeaking
        = QLocale().language() == QLocale::Russian || QLocale().language() == QLocale::Belarusian;
    d->socialTwitterButton->setVisible(!isRussianSpeaking);
    d->socialDiscordButton->setVisible(!isRussianSpeaking);
    d->socialVkButton->setVisible(isRussianSpeaking);
    d->socialContinueButton->setText(tr("Start writing"));
}

void OnboardingNavigator::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    StackWidget::designSystemChangeEvent(_event);

    setBackgroundColor(DesignSystem::color().primary());
    setTextColor(DesignSystem::color().onPrimary());

    for (auto widget : std::vector<Widget*>{
             d->uiPage,
             d->uiLogo,
             d->uiTitle,
             d->uiSubtitle,
             d->uiThemeTitle,
             d->uiLightTheme,
             d->uiDarkAndLightTheme,
             d->uiDarkTheme,
             d->uiScaleTitle,
             d->uiScaleSlider,
             d->uiScaleSmallInfoLabel,
             d->uiScaleBigInfoLabel,
             //
             d->signInPage,
             d->signInTitle,
             d->signInSubtitle,
             d->signInAuthorizationError,
             d->signInConfirmationCodeInfo,
             //
             d->accountPage,
             d->accountTitle,
             d->accountSubtitle,
             d->accountAvatar,
             d->accountSubscription,
             //
             d->modulesPage,
             d->modulesLogo,
             d->modulesTitle,
             d->modulesSubtitle,
             d->modulesDescription,
             d->modulesScreenplayTitle,
             d->modulesComicBookTitle,
             d->modulesAudioplayTitle,
             d->modulesStageplayTitle,
             d->modulesNovelTitle,
             //
             d->backupsPage,
             d->backupsLogo,
             d->backupsTitle,
             d->backupsSubtitle,
             d->backupsDescription,
             //
             d->styleChoosePage,
             d->styleChooseLogo,
             d->styleChooseTitle,
             d->styleChooseSubtitle,
             d->styleChooseDescription,
             d->styleChooseActivateTitle,
             //
             d->socialPage,
             d->socialLogo,
             d->socialTitle,
             d->socialSubtitle,
             d->socialDescription,
         }) {
        widget->setBackgroundColor(DesignSystem::color().primary());
        widget->setTextColor(DesignSystem::color().onPrimary());
    }
    d->signInAuthorizationError->setTextColor(DesignSystem::color().error());
    const auto margin = DesignSystem::layout().px(48);
    d->uiTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(), margin / 2.0,
                                   DesignSystem::layout().px4());
    d->uiSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->uiThemeTitle->setContentsMargins(margin, DesignSystem::layout().px24(), margin,
                                        DesignSystem::layout().px8());
    d->uiScaleTitle->setContentsMargins(margin, DesignSystem::layout().px24(), margin, 0);
    d->uiScaleSmallInfoLabel->setContentsMargins(isLeftToRight() ? margin : 0,
                                                 isLeftToRight() ? 0 : margin, 0, 0);
    d->uiScaleBigInfoLabel->setContentsMargins(isLeftToRight() ? 0 : margin, 0,
                                               isLeftToRight() ? margin : 0, 0);
    d->signInTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(), margin / 2.0,
                                       DesignSystem::layout().px4());
    d->signInSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->signInAuthorizationError->setContentsMargins(margin, DesignSystem::layout().px24(), margin,
                                                    0);
    d->signInConfirmationCodeInfo->setContentsMargins(margin, DesignSystem::layout().px24(), margin,
                                                      0);
    d->accountTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(), margin / 2.0,
                                        DesignSystem::layout().px4());
    d->accountSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->modulesTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(), margin / 2.0,
                                        DesignSystem::layout().px4());
    d->modulesSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->modulesDescription->setContentsMargins(margin, DesignSystem::layout().px24(), margin,
                                              DesignSystem::layout().px12());
    d->styleChooseTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(),
                                            margin / 2.0, DesignSystem::layout().px4());
    d->styleChooseSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->styleChooseDescription->setContentsMargins(margin, DesignSystem::layout().px24(), margin,
                                                  DesignSystem::layout().px12());
    d->styleChooseActivateTitle->setContentsMargins(isLeftToRight() ? 0 : margin, 0,
                                                    isLeftToRight() ? margin : 0, 0);
    d->backupsTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(), margin / 2.0,
                                        DesignSystem::layout().px4());
    d->backupsSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->backupsDescription->setContentsMargins(margin, DesignSystem::layout().px24(), margin,
                                              DesignSystem::layout().px48());
    d->socialTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(), margin / 2.0,
                                       DesignSystem::layout().px4());
    d->socialSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->socialDescription->setContentsMargins(margin, DesignSystem::layout().px24(), margin,
                                             DesignSystem::layout().px48());

    for (auto logo : {
             d->uiLogo,
             d->modulesLogo,
             d->styleChooseLogo,
             d->backupsLogo,
             d->socialLogo,
         }) {
        logo->setFixedSize(DesignSystem::layout().px62(), DesignSystem::layout().px62());
    }
    d->accountAvatar->setContentsMargins(0, Ui::DesignSystem::layout().px24(), 0,
                                         Ui::DesignSystem::layout().px8());
    d->accountAvatar->setFixedSize(
        QRectF(0, 0, DesignSystem::layout().px(94), DesignSystem::layout().px(94))
            .marginsAdded(d->accountAvatar->contentsMargins())
            .size()
            .toSize());
    d->updateAccountAvatar();

    for (auto textField : std::vector<TextField*>{
             d->uiLanguage,
             d->signInEmail,
             d->signInConfirmationCode,
             d->accountName,
             d->accountDescription,
             d->styleChooseComboBox,
         }) {
        textField->setBackgroundColor(DesignSystem::color().onPrimary());
        textField->setTextColor(DesignSystem::color().onPrimary());
        textField->setCustomMargins({ margin, DesignSystem::layout().px24(), margin, 0 });
    }
    d->uiLanguage->setPopupBackgroundColor(DesignSystem::color().primary());
    d->styleChooseComboBox->setPopupBackgroundColor(DesignSystem::color().primary());

    for (auto themePreview : {
             d->uiLightTheme,
             d->uiDarkAndLightTheme,
             d->uiDarkTheme,
         }) {
        themePreview->setFixedSize(themePreview->sizeHint());
    }

    for (auto containedButton : {
             d->uiContinueButton,
             d->signInSignInButton,
             d->signInResendCodeButton,
             d->signInContinueButton,
             d->accountContinueButton,
             d->modulesContinueButton,
             d->styleChooseContinueButton,
             d->backupsContinueButton,
             d->socialContinueButton,
         }) {
        containedButton->setBackgroundColor(DesignSystem::color().accent());
        containedButton->setTextColor(DesignSystem::color().onAccent());
        containedButton->setContentsMargins(margin - DesignSystem::card().shadowMargins().left(), 0,
                                            margin - DesignSystem::card().shadowMargins().right(),
                                            DesignSystem::layout().px16());
    }
    for (auto button : {
             d->signInContinueButton,
             d->accountChangeAvatarButton,
         }) {
        button->setBackgroundColor(DesignSystem::color().accent());
        button->setTextColor(DesignSystem::color().accent());
    }

    for (auto toggle : {
             d->modulesScreenplayToggle,
             d->modulesComicBookToggle,
             d->modulesAudioplayToggle,
             d->modulesStageplayToggle,
             d->modulesNovelToggle,
             d->styleChooseActivateToggle,
         }) {
        toggle->setBackgroundColor(DesignSystem::color().primary());
        toggle->setTextColor(DesignSystem::color().onPrimary());
        toggle->setContentsMargins(DesignSystem::layout().px(32), 0, 0, 0);
    }

    for (auto iconButton : {
             d->socialTwitterButton,
             d->socialDiscordButton,
             d->socialTelegramButton,
             d->socialVkButton,
             d->socialFacebookButton,
         }) {
        iconButton->setBackgroundColor(DesignSystem::color().primary());
        iconButton->setTextColor(DesignSystem::color().onPrimary());
        iconButton->setCustomFont(DesignSystem::font().brandsBig());
        iconButton->setFixedSize(DesignSystem::layout().px(64), DesignSystem::layout().px(64));
    }

    d->uiThemeLayout->setContentsMargins(margin, 0, margin, 0);
    d->uiThemeLayout->setSpacing(DesignSystem::layout().px12());

    d->uiScaleSlider->setContentsMargins(margin, 0, margin, 0);

    d->accountSubscription->setContentsMargins(
        margin - Ui::DesignSystem::radioButton().margins().left(), 0,
        margin - Ui::DesignSystem::radioButton().margins().right(), 0);
}

} // namespace Ui
