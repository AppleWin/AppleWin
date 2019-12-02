#include "preferences.h"
#include "ui_preferences.h"

#include "StdAfx.h"
#include "Common.h"
#include "Memory.h"

#include "options.h"

#include <QFileDialog>
#include <QGamepadManager>
#include <QSettings>

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

    QGamepadManager * manager = QGamepadManager::instance();
    const QList<int> gamepads = manager->connectedGamepads();

    for (int id : gamepads)
    {
        const QString name = manager->gamepadName(id);
        ui->joystick->addItem(name);
    }
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
}

void Preferences::setData(const PreferenceData & data)
{
    ui->lc_0->setCurrentIndex(data.options.slot0Card);
    ui->timer_gap->setValue(data.options.msGap);
    ui->full_ms->setValue(data.options.msFullSpeed);
    ui->rw_size->setMaximum(kMaxExMemoryBanks);
    ui->rw_size->setValue(data.options.ramWorksMemorySize);
    ui->joystick->setCurrentText(data.options.gamepadName);
    ui->screenshot->setText(data.options.screenshotTemplate);
    ui->audio_latency->setValue(data.options.audioLatency);
    ui->silence_delay->setValue(data.options.silenceDelay);
    ui->volume->setValue(data.options.volume);

    initialiseDisks(myDisks, data.disks);
    initialiseDisks(myHDs, data.hds);

    ui->enhanced_speed->setChecked(data.enhancedSpeed);
    ui->apple2Type->setCurrentIndex(data.apple2Type);
    ui->mouse_4->setChecked(data.mouseInSlot4);
    ui->cpm_5->setChecked(data.cpmInSlot5);
    ui->hd_7->setChecked(data.hdInSlot7);

    // synchronise
    on_hd_7_clicked(data.hdInSlot7);

    ui->save_state->setText(data.saveState);
    ui->video_type->setCurrentIndex(data.videoType);
    ui->scan_lines->setChecked(data.scanLines);
    ui->vertical_blend->setChecked(data.verticalBlend);

    ui->hz_50->setChecked(data.hz50);
}

PreferenceData Preferences::getData() const
{
    PreferenceData data;

    data.options.slot0Card = ui->lc_0->currentIndex();
    data.options.ramWorksMemorySize = ui->rw_size->value();
    data.options.msGap = ui->timer_gap->value();
    data.options.msFullSpeed = ui->full_ms->value();
    data.options.screenshotTemplate = ui->screenshot->text();
    data.options.audioLatency = ui->audio_latency->value();
    data.options.silenceDelay = ui->silence_delay->value();
    data.options.volume = ui->volume->value();

    // because index = 0 is None
    if (ui->joystick->currentIndex() >= 1)
    {
        data.options.gamepadName = ui->joystick->currentText();
    }

    fillData(myDisks, data.disks);
    fillData(myHDs, data.hds);

    data.enhancedSpeed = ui->enhanced_speed->isChecked();
    data.apple2Type = ui->apple2Type->currentIndex();
    data.mouseInSlot4 = ui->mouse_4->isChecked();
    data.cpmInSlot5 = ui->cpm_5->isChecked();
    data.hdInSlot7 = ui->hd_7->isChecked();

    data.saveState = ui->save_state->text();

    data.videoType = ui->video_type->currentIndex();
    data.scanLines = ui->scan_lines->isChecked();
    data.verticalBlend = ui->vertical_blend->isChecked();
    data.hz50 = ui->hz_50->isChecked();

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
