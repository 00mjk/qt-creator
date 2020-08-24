/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "projectconfigurationaspects.h"

#include "environmentaspect.h"
#include "project.h"
#include "projectexplorer.h"
#include "projectexplorersettings.h"
#include "runconfiguration.h"
#include "target.h"

#include <coreplugin/variablechooser.h>
#include <utils/utilsicons.h>
#include <utils/fancylineedit.h>
#include <utils/pathchooser.h>
#include <utils/qtcprocess.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QSpinBox>
#include <QToolButton>
#include <QTextEdit>
#include <QRadioButton>
#include <QButtonGroup>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class BoolAspectPrivate
{
public:
    BoolAspect::LabelPlacement m_labelPlacement = BoolAspect::LabelPlacement::AtCheckBox;
    bool m_value = false;
    bool m_defaultValue = false;
    bool m_enabled = true;
    QString m_labelText;
    QString m_tooltip;
    QPointer<QCheckBox> m_checkBox; // Owned by configuration widget
    QPointer<QLabel> m_label; // Owned by configuration widget
};

class SelectionAspectPrivate
{
public:
    int m_value = 0;
    int m_defaultValue = 0;
    SelectionAspect::DisplayStyle m_displayStyle
            = SelectionAspect::DisplayStyle::RadioButtons;
    struct Option { QString displayName; QString tooltip; };
    QVector<Option> m_options;

    // These are all owned by the configuration widget.
    QList<QPointer<QRadioButton>> m_buttons;
    QPointer<QComboBox> m_comboBox;
    QPointer<QLabel> m_label;
    QPointer<QButtonGroup> m_buttonGroup;
    QString m_tooltip;
};

class StringAspectPrivate
{
public:
    StringAspect::DisplayStyle m_displayStyle = StringAspect::LabelDisplay;
    StringAspect::CheckBoxPlacement m_checkBoxPlacement
        = StringAspect::CheckBoxPlacement::Right;
    StringAspect::UncheckedSemantics m_uncheckedSemantics
        = StringAspect::UncheckedSemantics::Disabled;
    QString m_labelText;
    std::function<QString(const QString &)> m_displayFilter;
    std::unique_ptr<BoolAspect> m_checker;

    QString m_value;
    QString m_placeHolderText;
    QString m_historyCompleterKey;
    QString m_tooltip;
    PathChooser::Kind m_expectedKind = PathChooser::File;
    Environment m_environment;
    QPointer<QLabel> m_label;
    QPointer<QLabel> m_labelDisplay;
    QPointer<FancyLineEdit> m_lineEditDisplay;
    QPointer<PathChooser> m_pathChooserDisplay;
    QPointer<QTextEdit> m_textEditDisplay;
    Utils::MacroExpanderProvider m_expanderProvider;
    QPixmap m_labelPixmap;
    Utils::FilePath m_baseFileName;
    StringAspect::ValueAcceptor m_valueAcceptor;
    bool m_readOnly = false;
    bool m_showToolTipOnLabel = false;
    bool m_fileDialogOnly = false;

    template<class Widget> void updateWidgetFromCheckStatus(Widget *w)
    {
        const bool enabled = !m_checker || m_checker->value();
        if (m_uncheckedSemantics == StringAspect::UncheckedSemantics::Disabled)
            w->setEnabled(enabled);
        else
            w->setReadOnly(!enabled);
    }
};

class IntegerAspectPrivate
{
public:
    qint64 m_value = 0;
    qint64 m_defaultValue = 0;
    QVariant m_minimumValue;
    QVariant m_maximumValue;
    int m_displayIntegerBase = 10;
    qint64 m_displayScaleFactor = 1;
    QString m_labelText;
    QString m_prefix;
    QString m_suffix;
    QString m_tooltip;
    QPointer<QLabel> m_label;
    QPointer<QSpinBox> m_spinBox; // Owned by configuration widget
    bool m_enabled = true;
};

class StringListAspectPrivate
{
public:
    QStringList m_value;
};

class AspectContainerPrivate
{
public:
    QList<ProjectConfigurationAspect *> m_items;
};

class TextDisplayPrivate
{
public:
    QString m_message;
    QString m_tooltip;
    Utils::InfoLabel::InfoType m_type;
    QPointer<InfoLabel> m_label;
};

} // Internal

/*!
    \class ProjectExplorer::StringAspect
*/

StringAspect::StringAspect()
    : d(new Internal::StringAspectPrivate)
{}

StringAspect::~StringAspect() = default;

void StringAspect::setValueAcceptor(StringAspect::ValueAcceptor &&acceptor)
{
    d->m_valueAcceptor = std::move(acceptor);
}

QString StringAspect::value() const
{
    return d->m_value;
}

void StringAspect::setValue(const QString &value)
{
    const bool isSame = value == d->m_value;
    if (isSame)
        return;

    QString processedValue = value;
    if (d->m_valueAcceptor) {
        const Utils::optional<QString> tmp = d->m_valueAcceptor(d->m_value, value);
        if (!tmp) {
            update(); // Make sure the original value is retained in the UI
            return;
        }
        processedValue = tmp.value();
    }

    d->m_value = processedValue;
    update();
    emit changed();
}

void StringAspect::fromMap(const QVariantMap &map)
{
    if (!settingsKey().isEmpty())
        d->m_value = map.value(settingsKey()).toString();
    if (d->m_checker)
        d->m_checker->fromMap(map);
}

void StringAspect::toMap(QVariantMap &map) const
{
    if (!settingsKey().isEmpty())
        map.insert(settingsKey(), d->m_value);
    if (d->m_checker)
        d->m_checker->toMap(map);
}

FilePath StringAspect::filePath() const
{
    return FilePath::fromUserInput(d->m_value);
}

void StringAspect::setFilePath(const FilePath &val)
{
    setValue(val.toUserOutput());
}

void StringAspect::setLabelText(const QString &labelText)
{
    d->m_labelText = labelText;
    if (d->m_label)
        d->m_label->setText(labelText);
}

void StringAspect::setLabelPixmap(const QPixmap &labelPixmap)
{
    d->m_labelPixmap = labelPixmap;
    if (d->m_label)
        d->m_label->setPixmap(labelPixmap);
}

void StringAspect::setShowToolTipOnLabel(bool show)
{
    d->m_showToolTipOnLabel = show;
    update();
}

QString StringAspect::labelText() const
{
    return d->m_labelText;
}

void StringAspect::setDisplayFilter(const std::function<QString(const QString &)> &displayFilter)
{
    d->m_displayFilter = displayFilter;
}

bool StringAspect::isChecked() const
{
    return !d->m_checker || d->m_checker->value();
}

void StringAspect::setChecked(bool checked)
{
    QTC_ASSERT(d->m_checker, return);
    d->m_checker->setValue(checked);
}

void StringAspect::setDisplayStyle(DisplayStyle displayStyle)
{
    d->m_displayStyle = displayStyle;
}

void StringAspect::setPlaceHolderText(const QString &placeHolderText)
{
    d->m_placeHolderText = placeHolderText;
    if (d->m_lineEditDisplay)
        d->m_lineEditDisplay->setPlaceholderText(placeHolderText);
    if (d->m_textEditDisplay)
        d->m_textEditDisplay->setPlaceholderText(placeHolderText);
}

void StringAspect::setHistoryCompleter(const QString &historyCompleterKey)
{
    d->m_historyCompleterKey = historyCompleterKey;
    if (d->m_lineEditDisplay)
        d->m_lineEditDisplay->setHistoryCompleter(historyCompleterKey);
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->setHistoryCompleter(historyCompleterKey);
}

void StringAspect::setExpectedKind(const PathChooser::Kind expectedKind)
{
    d->m_expectedKind = expectedKind;
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->setExpectedKind(expectedKind);
}

void StringAspect::setFileDialogOnly(bool requireFileDialog)
{
    d->m_fileDialogOnly = requireFileDialog;
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->setFileDialogOnly(requireFileDialog);
}

void StringAspect::setEnvironment(const Environment &env)
{
    d->m_environment = env;
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->setEnvironment(env);
}

void StringAspect::setBaseFileName(const FilePath &baseFileName)
{
    d->m_baseFileName = baseFileName;
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->setBaseDirectory(baseFileName);
}

void StringAspect::setToolTip(const QString &tooltip)
{
    d->m_tooltip = tooltip;
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->setToolTip(tooltip);
    if (d->m_lineEditDisplay)
        d->m_lineEditDisplay->setToolTip(tooltip);
    if (d->m_textEditDisplay)
        d->m_textEditDisplay->setToolTip(tooltip);
}

void StringAspect::setReadOnly(bool readOnly)
{
    d->m_readOnly = readOnly;
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->setReadOnly(readOnly);
    if (d->m_lineEditDisplay)
        d->m_lineEditDisplay->setReadOnly(readOnly);
    if (d->m_textEditDisplay)
        d->m_textEditDisplay->setReadOnly(readOnly);
}

void StringAspect::setMacroExpanderProvider(const MacroExpanderProvider &expanderProvider)
{
    d->m_expanderProvider = expanderProvider;
}

void StringAspect::validateInput()
{
    if (d->m_pathChooserDisplay)
        d->m_pathChooserDisplay->triggerChanged();
    if (d->m_lineEditDisplay)
        d->m_lineEditDisplay->validate();
}

void StringAspect::setUncheckedSemantics(StringAspect::UncheckedSemantics semantics)
{
    d->m_uncheckedSemantics = semantics;
}

void StringAspect::addToLayout(LayoutBuilder &builder)
{
    QTC_CHECK(!d->m_label);

    if (d->m_checker && d->m_checkBoxPlacement == CheckBoxPlacement::Top) {
        d->m_checker->addToLayout(builder);
        builder.startNewRow();
    }

    d->m_label = new QLabel;
    d->m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    d->m_label->setText(d->m_labelText);
    if (!d->m_labelPixmap.isNull())
        d->m_label->setPixmap(d->m_labelPixmap);
    builder.addItem(d->m_label.data());

    const auto useMacroExpander = [this, &builder](QWidget *w) {
        if (!d->m_expanderProvider)
            return;
        const auto chooser = new Core::VariableChooser(builder.layout()->parentWidget());
        chooser->addSupportedWidget(w);
        chooser->addMacroExpanderProvider(d->m_expanderProvider);
    };

    switch (d->m_displayStyle) {
    case PathChooserDisplay:
        d->m_pathChooserDisplay = new PathChooser;
        d->m_pathChooserDisplay->setExpectedKind(d->m_expectedKind);
        if (!d->m_historyCompleterKey.isEmpty())
            d->m_pathChooserDisplay->setHistoryCompleter(d->m_historyCompleterKey);
        d->m_pathChooserDisplay->setEnvironment(d->m_environment);
        d->m_pathChooserDisplay->setBaseDirectory(d->m_baseFileName);
        d->m_pathChooserDisplay->setReadOnly(d->m_readOnly);
        useMacroExpander(d->m_pathChooserDisplay->lineEdit());
        connect(d->m_pathChooserDisplay, &PathChooser::pathChanged,
                this, &StringAspect::setValue);
        builder.addItem(d->m_pathChooserDisplay.data());
        d->m_pathChooserDisplay->setFileDialogOnly(d->m_fileDialogOnly);
        break;
    case LineEditDisplay:
        d->m_lineEditDisplay = new FancyLineEdit;
        d->m_lineEditDisplay->setPlaceholderText(d->m_placeHolderText);
        if (!d->m_historyCompleterKey.isEmpty())
            d->m_lineEditDisplay->setHistoryCompleter(d->m_historyCompleterKey);
        d->m_lineEditDisplay->setReadOnly(d->m_readOnly);
        useMacroExpander(d->m_lineEditDisplay);
        connect(d->m_lineEditDisplay, &FancyLineEdit::textEdited,
                this, &StringAspect::setValue);
        builder.addItem(d->m_lineEditDisplay.data());
        break;
    case TextEditDisplay:
        d->m_textEditDisplay = new QTextEdit;
        d->m_textEditDisplay->setPlaceholderText(d->m_placeHolderText);
        d->m_textEditDisplay->setReadOnly(d->m_readOnly);
        useMacroExpander(d->m_textEditDisplay);
        connect(d->m_textEditDisplay, &QTextEdit::textChanged, this, [this] {
            const QString value = d->m_textEditDisplay->document()->toPlainText();
            if (value != d->m_value) {
                d->m_value = value;
                emit changed();
            }
        });
        builder.addItem(d->m_textEditDisplay.data());
        break;
    case LabelDisplay:
        d->m_labelDisplay = new QLabel;
        d->m_labelDisplay->setTextInteractionFlags(Qt::TextSelectableByMouse);
        builder.addItem(d->m_labelDisplay.data());
        break;
    }

    validateInput();

    if (d->m_checker && d->m_checkBoxPlacement == CheckBoxPlacement::Right)
        d->m_checker->addToLayout(builder);

    update();
}

void StringAspect::update()
{
    const QString displayedString = d->m_displayFilter ? d->m_displayFilter(d->m_value)
                                                       : d->m_value;

    if (d->m_pathChooserDisplay) {
        d->m_pathChooserDisplay->setFilePath(FilePath::fromString(displayedString));
        d->m_pathChooserDisplay->setToolTip(d->m_tooltip);
        d->updateWidgetFromCheckStatus(d->m_pathChooserDisplay.data());
    }

    if (d->m_lineEditDisplay) {
        d->m_lineEditDisplay->setTextKeepingActiveCursor(displayedString);
        d->m_lineEditDisplay->setToolTip(d->m_tooltip);
        d->updateWidgetFromCheckStatus(d->m_lineEditDisplay.data());
    }

    if (d->m_textEditDisplay) {
        d->m_textEditDisplay->setText(displayedString);
        d->m_textEditDisplay->setToolTip(d->m_tooltip);
        d->updateWidgetFromCheckStatus(d->m_textEditDisplay.data());
    }

    if (d->m_labelDisplay) {
        d->m_labelDisplay->setText(displayedString);
        d->m_labelDisplay->setToolTip(d->m_showToolTipOnLabel ? displayedString : d->m_tooltip);
    }

    if (d->m_label) {
        d->m_label->setText(d->m_labelText);
        if (!d->m_labelPixmap.isNull())
            d->m_label->setPixmap(d->m_labelPixmap);
    }

    validateInput();
}

void StringAspect::makeCheckable(CheckBoxPlacement checkBoxPlacement,
                                     const QString &checkerLabel, const QString &checkerKey)
{
    QTC_ASSERT(!d->m_checker, return);
    d->m_checkBoxPlacement = checkBoxPlacement;
    d->m_checker.reset(new BoolAspect);
    d->m_checker->setLabel(checkerLabel, checkBoxPlacement == CheckBoxPlacement::Top
                           ? BoolAspect::LabelPlacement::InExtraLabel
                           : BoolAspect::LabelPlacement::AtCheckBox);
    d->m_checker->setSettingsKey(checkerKey);

    connect(d->m_checker.get(), &BoolAspect::changed, this, &StringAspect::update);
    connect(d->m_checker.get(), &BoolAspect::changed, this, &StringAspect::changed);
    connect(d->m_checker.get(), &BoolAspect::changed, this, &StringAspect::checkedChanged);

    update();
}

/*!
    \class ProjectExplorer::BoolAspect
*/

BoolAspect::BoolAspect(const QString &settingsKey)
    : d(new Internal::BoolAspectPrivate)
{
    setSettingsKey(settingsKey);
}

BoolAspect::~BoolAspect() = default;

void BoolAspect::addToLayout(LayoutBuilder &builder)
{
    QTC_CHECK(!d->m_checkBox);
    d->m_checkBox = new QCheckBox();
    switch (d->m_labelPlacement) {
    case LabelPlacement::AtCheckBoxWithoutDummyLabel:
        d->m_checkBox->setText(d->m_labelText);
        break;
    case LabelPlacement::AtCheckBox:
        d->m_checkBox->setText(d->m_labelText);
        builder.addItem(new QLabel);
        break;
    case LabelPlacement::InExtraLabel:
        d->m_label = new QLabel(d->m_labelText);
        d->m_label->setToolTip(d->m_tooltip);
        builder.addItem(d->m_label.data());
        break;
    }
    d->m_checkBox->setChecked(d->m_value);
    d->m_checkBox->setToolTip(d->m_tooltip);
    d->m_checkBox->setEnabled(d->m_enabled);
    builder.addItem(d->m_checkBox.data());
    connect(d->m_checkBox.data(), &QAbstractButton::clicked, this, [this] {
        d->m_value = d->m_checkBox->isChecked();
        emit changed();
    });
}

void BoolAspect::fromMap(const QVariantMap &map)
{
    d->m_value = map.value(settingsKey(), d->m_defaultValue).toBool();
}

void BoolAspect::toMap(QVariantMap &data) const
{
    data.insert(settingsKey(), d->m_value);
}

bool BoolAspect::defaultValue() const
{
    return d->m_defaultValue;
}

void BoolAspect::setDefaultValue(bool defaultValue)
{
    d->m_defaultValue = defaultValue;
    d->m_value = defaultValue;
}

bool BoolAspect::value() const
{
    return d->m_value;
}

void BoolAspect::setValue(bool value)
{
    d->m_value = value;
    if (d->m_checkBox)
        d->m_checkBox->setChecked(d->m_value);
}

void BoolAspect::setLabel(const QString &labelText, LabelPlacement labelPlacement)
{
    d->m_labelText = labelText;
    d->m_labelPlacement = labelPlacement;
}

void BoolAspect::setToolTip(const QString &tooltip)
{
    d->m_tooltip = tooltip;
}

void BoolAspect::setEnabled(bool enabled)
{
    d->m_enabled = enabled;
    if (d->m_checkBox)
        d->m_checkBox->setEnabled(enabled);
}

/*!
    \class ProjectExplorer::SelectionAspect
*/

SelectionAspect::SelectionAspect()
    : d(new Internal::SelectionAspectPrivate)
{}

SelectionAspect::~SelectionAspect() = default;

void SelectionAspect::addToLayout(LayoutBuilder &builder)
{
    QTC_CHECK(d->m_buttonGroup == nullptr);
    QTC_CHECK(!d->m_comboBox);
    QTC_ASSERT(d->m_buttons.isEmpty(), d->m_buttons.clear());

    switch (d->m_displayStyle) {
    case DisplayStyle::RadioButtons:
        d->m_buttonGroup = new QButtonGroup;
        d->m_buttonGroup->setExclusive(true);
        for (int i = 0, n = d->m_options.size(); i < n; ++i) {
            const Internal::SelectionAspectPrivate::Option &option = d->m_options.at(i);
            auto button = new QRadioButton(option.displayName);
            button->setChecked(i == d->m_value);
            button->setToolTip(option.tooltip);
            builder.addItems(QString(), button);
            d->m_buttons.append(button);
            d->m_buttonGroup->addButton(button);
            connect(button, &QAbstractButton::clicked, this, [this, i] {
                d->m_value = i;
                emit changed();
            });
        }
        break;
    case DisplayStyle::ComboBox:
        d->m_label = new QLabel(displayName());
        d->m_label->setToolTip(d->m_tooltip);
        d->m_comboBox = new QComboBox;
        d->m_comboBox->setToolTip(d->m_tooltip);
        for (int i = 0, n = d->m_options.size(); i < n; ++i)
            d->m_comboBox->addItem(d->m_options.at(i).displayName);
        connect(d->m_comboBox.data(), QOverload<int>::of(&QComboBox::activated), this,
                [this](int index) { d->m_value = index; emit changed(); });
        d->m_comboBox->setCurrentIndex(d->m_value);
        builder.addItems(d->m_label.data(), d->m_comboBox.data());
        break;
    }
}

void SelectionAspect::fromMap(const QVariantMap &map)
{
    d->m_value = map.value(settingsKey(), d->m_defaultValue).toInt();
}

void SelectionAspect::toMap(QVariantMap &data) const
{
    data.insert(settingsKey(), d->m_value);
}

void SelectionAspect::setVisibleDynamic(bool visible)
{
    if (d->m_label)
        d->m_label->setVisible(visible);
    if (d->m_comboBox)
        d->m_comboBox->setVisible(visible);
    for (QRadioButton * const button : qAsConst(d->m_buttons))
        button->setVisible(visible);
}

int SelectionAspect::defaultValue() const
{
    return d->m_defaultValue;
}

void SelectionAspect::setDefaultValue(int defaultValue)
{
    d->m_defaultValue = defaultValue;
}

void SelectionAspect::setDisplayStyle(SelectionAspect::DisplayStyle style)
{
    d->m_displayStyle = style;
}

void SelectionAspect::setToolTip(const QString &tooltip)
{
    d->m_tooltip = tooltip;
}

int SelectionAspect::value() const
{
    return d->m_value;
}

void SelectionAspect::setValue(int value)
{
    d->m_value = value;
    if (d->m_buttonGroup && 0 <= value && value < d->m_buttons.size())
        d->m_buttons.at(value)->setChecked(true);
    else if (d->m_comboBox) {
        d->m_comboBox->setCurrentIndex(value);
    }
}

QString SelectionAspect::stringValue() const
{
    return d->m_options.at(d->m_value).displayName;
}

void SelectionAspect::addOption(const QString &displayName, const QString &toolTip)
{
    d->m_options.append({displayName, toolTip});
}

/*!
    \class ProjectExplorer::IntegerAspect
*/

// IntegerAspect

IntegerAspect::IntegerAspect()
    : d(new Internal::IntegerAspectPrivate)
{}

IntegerAspect::~IntegerAspect() = default;

void IntegerAspect::addToLayout(LayoutBuilder &builder)
{
    QTC_CHECK(!d->m_label);
    d->m_label = new QLabel(d->m_labelText);
    d->m_label->setEnabled(d->m_enabled);

    QTC_CHECK(!d->m_spinBox);
    d->m_spinBox = new QSpinBox;
    d->m_spinBox->setValue(int(d->m_value / d->m_displayScaleFactor));
    d->m_spinBox->setDisplayIntegerBase(d->m_displayIntegerBase);
    d->m_spinBox->setPrefix(d->m_prefix);
    d->m_spinBox->setSuffix(d->m_suffix);
    d->m_spinBox->setEnabled(d->m_enabled);
    d->m_spinBox->setToolTip(d->m_tooltip);
    if (d->m_maximumValue.isValid() && d->m_maximumValue.isValid())
        d->m_spinBox->setRange(int(d->m_minimumValue.toLongLong() / d->m_displayScaleFactor),
                               int(d->m_maximumValue.toLongLong() / d->m_displayScaleFactor));

    builder.addItems(d->m_label.data(), d->m_spinBox.data());
    connect(d->m_spinBox.data(), QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        d->m_value = value * d->m_displayScaleFactor;
        emit changed();
    });
}

void IntegerAspect::fromMap(const QVariantMap &map)
{
    d->m_value = map.value(settingsKey(), d->m_defaultValue).toLongLong();
}

void IntegerAspect::toMap(QVariantMap &data) const
{
    if (d->m_value != d->m_defaultValue)
        data.insert(settingsKey(), d->m_value);
    else
        data.remove(settingsKey());
}

qint64 IntegerAspect::value() const
{
    return d->m_value;
}

void IntegerAspect::setValue(qint64 value)
{
    d->m_value = value;
    if (d->m_spinBox)
        d->m_spinBox->setValue(int(d->m_value / d->m_displayScaleFactor));
}

void IntegerAspect::setRange(qint64 min, qint64 max)
{
    d->m_minimumValue = min;
    d->m_maximumValue = max;
}

void IntegerAspect::setLabel(const QString &label)
{
    d->m_labelText = label;
    if (d->m_label)
        d->m_label->setText(label);
}

void IntegerAspect::setPrefix(const QString &prefix)
{
    d->m_prefix = prefix;
}

void IntegerAspect::setSuffix(const QString &suffix)
{
    d->m_suffix = suffix;
}

void IntegerAspect::setDisplayIntegerBase(int base)
{
    d->m_displayIntegerBase = base;
}

void IntegerAspect::setDisplayScaleFactor(qint64 factor)
{
    d->m_displayScaleFactor = factor;
}

void IntegerAspect::setEnabled(bool enabled)
{
    d->m_enabled = enabled;
    if (d->m_label)
        d->m_label->setEnabled(enabled);
    if (d->m_spinBox)
        d->m_spinBox->setEnabled(enabled);
}

void IntegerAspect::setDefaultValue(qint64 defaultValue)
{
    d->m_defaultValue = defaultValue;
}

void IntegerAspect::setToolTip(const QString &tooltip)
{
    d->m_tooltip = tooltip;
}

/*!
    \class ProjectExplorer::BaseTristateAspect
*/

TriStateAspect::TriStateAspect()
{
    setDisplayStyle(DisplayStyle::ComboBox);
    setDefaultValue(2);
    addOption(tr("Enable"));
    addOption(tr("Disable"));
    addOption(tr("Leave at Default"));
}

TriState TriStateAspect::setting() const
{
    return TriState::fromVariant(value());
}

void TriStateAspect::setSetting(TriState setting)
{
    setValue(setting.toVariant().toInt());
}

const TriState TriState::Enabled{TriState::EnabledValue};
const TriState TriState::Disabled{TriState::DisabledValue};
const TriState TriState::Default{TriState::DefaultValue};

TriState TriState::fromVariant(const QVariant &variant)
{
    int v = variant.toInt();
    QTC_ASSERT(v == EnabledValue || v == DisabledValue || v == DefaultValue, v = DefaultValue);
    return TriState(Value(v));
}


/*!
    \class ProjectExplorer::StringListAspect
*/

StringListAspect::StringListAspect()
    : d(new Internal::StringListAspectPrivate)
{}

StringListAspect::~StringListAspect() = default;

void StringListAspect::addToLayout(LayoutBuilder &builder)
{
    Q_UNUSED(builder)
    // TODO - when needed.
}

void StringListAspect::fromMap(const QVariantMap &map)
{
    d->m_value = map.value(settingsKey()).toStringList();
}

void StringListAspect::toMap(QVariantMap &data) const
{
    data.insert(settingsKey(), d->m_value);
}

QStringList StringListAspect::value() const
{
    return d->m_value;
}

void StringListAspect::setValue(const QStringList &value)
{
    d->m_value = value;
}

/*!
    \class ProjectExplorer::TextDisplay
*/

TextDisplay::TextDisplay(const QString &message, InfoLabel::InfoType type)
    : d(new Internal::TextDisplayPrivate)
{
    d->m_message = message;
    d->m_type = type;
}

TextDisplay::~TextDisplay() = default;

void TextDisplay::addToLayout(LayoutBuilder &builder)
{
    if (!d->m_label) {
        d->m_label = new InfoLabel(d->m_message, d->m_type);
        d->m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        d->m_label->setToolTip(d->m_tooltip);
        d->m_label->setElideMode(Qt::ElideNone);
        d->m_label->setWordWrap(true);
    }
    builder.addItem(d->m_label.data());
    d->m_label->setVisible(isVisible());
}

void TextDisplay::setVisible(bool visible)
{
    ProjectConfigurationAspect::setVisible(visible);
    if (d->m_label)
        d->m_label->setVisible(visible);
}

void TextDisplay::setToolTip(const QString &tooltip)
{
    d->m_tooltip = tooltip;
    if (d->m_label)
        d->m_label->setToolTip(tooltip);
}

void TextDisplay::setIconType(InfoLabel::InfoType t)
{
    d->m_type = t;
    if (d->m_label)
        d->m_label->setType(t);
}

/*!
    \class ProjectExplorer::AspectContainer
*/

AspectContainer::AspectContainer()
    : d(new Internal::AspectContainerPrivate)
{}

AspectContainer::~AspectContainer() = default;

void AspectContainer::addAspectHelper(ProjectConfigurationAspect *aspect)
{
    d->m_items.append(aspect);
}

void AspectContainer::addToLayout(LayoutBuilder &builder)
{
    for (ProjectConfigurationAspect *aspect : d->m_items) {
        if (aspect->isVisible())
            aspect->addToLayout(builder);
    }
}

void AspectContainer::fromMap(const QVariantMap &map)
{
    for (ProjectConfigurationAspect *aspect : d->m_items)
        aspect->fromMap(map);
}

void AspectContainer::toMap(QVariantMap &map) const
{
    for (ProjectConfigurationAspect *aspect : d->m_items)
        aspect->toMap(map);
}

} // namespace ProjectExplorer
