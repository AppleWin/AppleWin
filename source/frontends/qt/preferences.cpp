#include "preferences.h"
#include "ui_preferences.h"

#include "StdAfx.h"
#include "Common.h"
#include "Memory.h"

#include "options.h"

#include <QFileDialog>
#include <QColorDialog>
#include <QSettings>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QGamepadManager>
#endif

namespace
{

    int addDiskItem(QComboBox *disk, const QString & item)
    {
        if (!item.isEmpty())
        {
            // check if we already have a item with same name
            const int position = disk->findText(item);
            if (position != -1)
            {
                // and reuse it
                disk->setCurrentIndex(position);
                return position;
            }
            else
            {
                // or add a new one
                const int size = disk->count();
                disk->insertItem(size, item);
                disk->setCurrentIndex(size);
                return size;
            }
        }
        return -1;
    }

    void checkDuplicates(const std::vector<QComboBox *> & disks, const size_t id, const int new_selection)
    {
        if (new_selection >= 1)
        {
            for (size_t i = 0; i < disks.size(); ++i)
            {
                if (i != id)
                {
                    const int disk_selection = disks[i]->currentIndex();
                    if (disk_selection == new_selection)
                    {
                        // eject disk
                        disks[i]->setCurrentIndex(0);
                    }
                }
            }
        }
    }

    void initialiseDisks(const std::vector<QComboBox *> & disks, const std::vector<QString> & data)
    {
        // share the same model for all disks in a group
        for (size_t i = 1; i < disks.size(); ++i)
        {
            disks[i]->setModel(disks[0]->model());
        }
        for (size_t i = 0; i < disks.size(); ++i)
        {
            addDiskItem(disks[i], data[i]);
        }
    }

    void fillData(const std::vector<QComboBox *> & disks, std::vector<QString> & data)
    {
        data.resize(disks.size());
        for (size_t i = 0; i < disks.size(); ++i)
        {
            if (disks[i]->currentIndex() >= 1)
            {
                data[i] = disks[i]->currentText();
            }
        }
    }

    void processSettingsGroup(QSettings & settings, QTreeWidgetItem * item)
    {
        QList<QTreeWidgetItem *> items;

        // first the leaves
        const QStringList children = settings.childKeys();
        for (const auto & child : children)
        {
            QStringList columns;
            columns.append(child); // the name
            columns.append(settings.value(child).toString()); // the value

            QTreeWidgetItem * newItem = new QTreeWidgetItem(item, columns);
            items.append(newItem);
        }

        // then the subtrees
        const QStringList groups = settings.childGroups();
        for (const auto & group : groups)
        {
            settings.beginGroup(group);

            QStringList columns;
            columns.append(group); // the name

            QTreeWidgetItem * newItem = new QTreeWidgetItem(item, columns);
            processSettingsGroup(settings, newItem); // process subtree
            items.append(newItem);

            settings.endGroup();
        }

        item->addChildren(items);
    }

    const std::vector<eApple2Type> computerTypes = {A2TYPE_APPLE2, A2TYPE_APPLE2PLUS, A2TYPE_APPLE2JPLUS, A2TYPE_APPLE2E,
                                                    A2TYPE_APPLE2EENHANCED, A2TYPE_PRAVETS82, A2TYPE_PRAVETS8M, A2TYPE_PRAVETS8A,
                                                    A2TYPE_BASE64A, A2TYPE_TK30002E};
    const std::vector<SS_CARDTYPE> cardsInSlot3 = {CT_Empty, CT_Uthernet, CT_Uthernet2, CT_VidHD};
    const std::vector<SS_CARDTYPE> cardsInSlot4 = {CT_Empty, CT_MouseInterface, CT_MockingboardC, CT_Phasor};
    const std::vector<SS_CARDTYPE> cardsInSlot5 = {CT_Empty, CT_Z80, CT_MockingboardC, CT_SAM};

    template<class T>
    int getIndexInList(const std::vector<T> & values, const T value, const int defaultValue)
    {
        const auto it = std::find(values.begin(), values.end(), value);
        if (it != values.end())
        {
            return std::distance(values.begin(), it);
        }
        else
        {
            return defaultValue;
        }
    }

}

Preferences::Preferences(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Preferences)
{
    ui->setupUi(this);

    // easier to handle in a vector
    myDisks.push_back(ui->disk1);
    myDisks.push_back(ui->disk2);
    myHDs.push_back(ui->hd1);
    myHDs.push_back(ui->hd2);
}

Preferences::~Preferences()
{
    delete ui;
}

void Preferences::setup(const PreferenceData & data, QSettings & settings)
{
    populateJoysticks();
    setData(data);
    setSettings(settings);
}

void Preferences::populateJoysticks()
{
    ui->joystick->clear();
    ui->joystick->addItem("None"); // index = 0

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QGamepadManager * manager = QGamepadManager::instance();
    const QList<int> gamepads = manager->connectedGamepads();

    for (int id : gamepads)
    {
        const QString name = manager->gamepadName(id);
        ui->joystick->addItem(name);
    }
#endif
}

void Preferences::setSettings(QSettings & settings)
{
    ui->registryTree->clear();

    QStringList columns;
    columns.append(QString::fromUtf8("Registry")); // the name
    columns.append(settings.fileName()); // the value

    QTreeWidgetItem * newItem = new QTreeWidgetItem(ui->registryTree, columns);
    processSettingsGroup(settings, newItem);

    ui->registryTree->addTopLevelItem(newItem);
    ui->registryTree->expandAll();
    ui->registryTree->resizeColumnToContents(0);
}

void Preferences::setData(const PreferenceData & data)
{
    ui->lc_0->setCurrentIndex(data.options.slot0Card);
    ui->timer_gap->setValue(data.options.msGap);
    ui->full_ms->setValue(data.options.msFullSpeed);
    ui->rw_size->setMaximum(kMaxExMemoryBanks);
    ui->rw_size->setValue(data.options.ramWorksMemorySize);
    ui->joystick->setCurrentText(data.options.gamepadName);
    ui->squareCircle->setChecked(data.options.gamepadSquaring);
    ui->screenshot->setText(data.options.screenshotTemplate);
    ui->audio_buffer->setValue(data.options.msAudioBuffer);

    ui->speaker_volume->setValue(ui->speaker_volume->maximum() - data.speakerVolume);
    ui->mb_volume->setValue(ui->mb_volume->maximum() - data.mockingboardVolume);

    initialiseDisks(myDisks, data.disks);
    initialiseDisks(myHDs, data.hds);

    ui->enhanced_speed->setChecked(data.enhancedSpeed);

    const int apple2Index = getIndexInList(computerTypes, data.apple2Type, 2);
    ui->apple2Type->setCurrentIndex(apple2Index);

    const int slot3Index = getIndexInList(cardsInSlot3, data.cardInSlot3, 0);
    ui->slot3_combo->setCurrentIndex(slot3Index);

    const int slot4Index = getIndexInList(cardsInSlot4, data.cardInSlot4, 0);
    ui->slot4_combo->setCurrentIndex(slot4Index);

    const int slot5Index = getIndexInList(cardsInSlot5, data.cardInSlot5, 0);
    ui->slot5_combo->setCurrentIndex(slot5Index);

    ui->hd_7->setChecked(data.hdInSlot7);

    // synchronise
    on_hd_7_clicked(data.hdInSlot7);

    ui->save_state->setText(data.saveState);
    ui->printer_filename->setText(data.printerFilename);
    ui->video_type->setCurrentIndex(data.videoType);
    ui->scan_lines->setChecked(data.scanLines);
    ui->vertical_blend->setChecked(data.verticalBlend);

    ui->hz_50->setChecked(data.hz50);

    myMonochromeColor = data.monochromeColor;
}

PreferenceData Preferences::getData() const
{
    PreferenceData data;

    data.options.slot0Card = ui->lc_0->currentIndex();
    data.options.ramWorksMemorySize = ui->rw_size->value();
    data.options.msGap = ui->timer_gap->value();
    data.options.msFullSpeed = ui->full_ms->value();
    data.options.screenshotTemplate = ui->screenshot->text();
    data.options.msAudioBuffer = ui->audio_buffer->value();

    // because index = 0 is None
    if (ui->joystick->currentIndex() >= 1)
    {
        data.options.gamepadName = ui->joystick->currentText();
    }
    data.options.gamepadSquaring = ui->squareCircle->isChecked();

    fillData(myDisks, data.disks);
    fillData(myHDs, data.hds);

    data.speakerVolume = ui->speaker_volume->maximum() - ui->speaker_volume->value();
    data.mockingboardVolume = ui->mb_volume->maximum() - ui->mb_volume->value();
    data.enhancedSpeed = ui->enhanced_speed->isChecked();
    data.apple2Type = computerTypes[ui->apple2Type->currentIndex()];
    data.cardInSlot3 = cardsInSlot3[ui->slot3_combo->currentIndex()];
    data.cardInSlot4 = cardsInSlot4[ui->slot4_combo->currentIndex()];
    data.cardInSlot5 = cardsInSlot5[ui->slot5_combo->currentIndex()];
    data.hdInSlot7 = ui->hd_7->isChecked();

    data.saveState = ui->save_state->text();
    data.printerFilename = ui->printer_filename->text();

    data.videoType = ui->video_type->currentIndex();
    data.scanLines = ui->scan_lines->isChecked();
    data.verticalBlend = ui->vertical_blend->isChecked();
    data.hz50 = ui->hz_50->isChecked();
    data.monochromeColor = myMonochromeColor;

    return data;
}

void Preferences::browseDisk(const std::vector<QComboBox *> & vdisks, const size_t id)
{
    QFileDialog diskFileDialog(this);
    diskFileDialog.setFileMode(QFileDialog::AnyFile);

    // because index = 0 is None
    if (vdisks[id]->currentIndex() >= 1)
    {
        diskFileDialog.selectFile(vdisks[id]->currentText());
    }

    if (diskFileDialog.exec())
    {
        QStringList files = diskFileDialog.selectedFiles();
        if (files.size() == 1)
        {
            const QString & filename = files[0];
            const int selection = addDiskItem(vdisks[id], filename);
            // and now make sure there are no duplicates
            checkDuplicates(vdisks, id, selection);
        }
    }
}

void Preferences::on_disk1_activated(int index)
{
    checkDuplicates(myDisks, 0, index);
}

void Preferences::on_disk2_activated(int index)
{
    checkDuplicates(myDisks, 1, index);
}

void Preferences::on_hd1_activated(int index)
{
    checkDuplicates(myHDs, 0, index);
}

void Preferences::on_hd2_activated(int index)
{
    checkDuplicates(myHDs, 1, index);
}

void Preferences::on_browse_disk1_clicked()
{
    browseDisk(myDisks, 0);
}

void Preferences::on_browse_disk2_clicked()
{
    browseDisk(myDisks, 1);
}

void Preferences::on_browse_hd1_clicked()
{
    browseDisk(myHDs, 0);
}

void Preferences::on_browse_hd2_clicked()
{
    browseDisk(myHDs, 1);
}

void Preferences::on_hd_7_clicked(bool checked)
{
    ui->hd1->setEnabled(checked);
    ui->hd2->setEnabled(checked);
    ui->browse_hd1->setEnabled(checked);
    ui->browse_hd2->setEnabled(checked);
}

void Preferences::on_browse_ss_clicked()
{
    const QString name = QFileDialog::getSaveFileName(this, QString(), ui->save_state->text());
    if (!name.isEmpty())
    {
        ui->save_state->setText(name);
    }
}

void Preferences::on_colorButton_clicked()
{
    QColorDialog dialog(myMonochromeColor);
    if (dialog.exec())
    {
        myMonochromeColor = dialog.currentColor();
    }
}

void Preferences::on_browse_pf_clicked()
{
    const QString name = QFileDialog::getSaveFileName(this, QString(), ui->printer_filename->text());
    if (!name.isEmpty())
    {
        ui->printer_filename->setText(name);
    }

}
