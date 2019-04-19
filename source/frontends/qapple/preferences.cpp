#include "preferences.h"
#include <QFileDialog>
#include <QtGamepad/QGamepad>

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
    QDialog(parent)
{
    setupUi(this);

    // easier to handle in a vector
    myDisks.push_back(disk1);
    myDisks.push_back(disk2);
    myHDs.push_back(hd1);
    myHDs.push_back(hd2);
}

void Preferences::setup(const Data & data, QSettings & settings)
{
    populateJoysticks();
    setData(data);
    setSettings(settings);
}

void Preferences::populateJoysticks()
{
    joystick->clear();
    joystick->addItem("None"); // index = 0

    const QList<int> gamepads = QGamepadManager::instance()->connectedGamepads();

    for (int id : gamepads)
    {
        QGamepad gp(id);
        QString name = gp.name();
        if (name.isEmpty())
        {
            name = QString::number(id);
        }
        joystick->addItem(name, QVariant::fromValue(id));
    }
}

void Preferences::setSettings(QSettings & settings)
{
    registryTree->clear();

    QStringList columns;
    columns.append(QString::fromUtf8("Registry")); // the name
    columns.append(settings.fileName()); // the value

    QTreeWidgetItem * newItem = new QTreeWidgetItem(registryTree, columns);
    processSettingsGroup(settings, newItem);

    registryTree->addTopLevelItem(newItem);
    registryTree->expandAll();
}

void Preferences::setData(const Data & data)
{
    initialiseDisks(myDisks, data.disks);
    initialiseDisks(myHDs, data.hds);

    apple2Type->setCurrentIndex(data.apple2Type);
    lc_0->setChecked(data.languageCardInSlot0);
    mouse_4->setChecked(data.mouseInSlot4);
    cpm_5->setChecked(data.cpmInSlot5);
    hd_7->setChecked(data.hdInSlot7);

    // synchronise
    on_hd_7_clicked(data.hdInSlot7);

    joystick->setCurrentText(data.joystick);

    save_state->setText(data.saveState);
    screenshot->setText(data.screenshotTemplate);
}

Preferences::Data Preferences::getData() const
{
    Data data;

    fillData(myDisks, data.disks);
    fillData(myHDs, data.hds);

    data.apple2Type = apple2Type->currentIndex();
    data.languageCardInSlot0 = lc_0->isChecked();
    data.mouseInSlot4 = mouse_4->isChecked();
    data.cpmInSlot5 = cpm_5->isChecked();
    data.hdInSlot7 = hd_7->isChecked();

    // because index = 0 is None
    if (joystick->currentIndex() >= 1)
    {
        const QVariant & device = joystick->itemData(joystick->currentIndex());
        data.joystick = joystick->currentText();
        data.joystickId = device.toInt();
    }
    else
    {
        data.joystickId = 0;
    }

    data.saveState = save_state->text();
    data.screenshotTemplate = screenshot->text();

    return data;
}

void Preferences::browseDisk(const std::vector<QComboBox *> & disks, const size_t id)
{
    QFileDialog diskFileDialog(this);
    diskFileDialog.setFileMode(QFileDialog::AnyFile);

    // because index = 0 is None
    if (disks[id]->currentIndex() >= 1)
    {
        diskFileDialog.selectFile(disks[id]->currentText());
    }

    if (diskFileDialog.exec())
    {
        QStringList files = diskFileDialog.selectedFiles();
        if (files.size() == 1)
        {
            const QString & filename = files[0];
            const int selection = addDiskItem(disks[id], filename);
            // and now make sure there are no duplicates
            checkDuplicates(disks, id, selection);
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
    hd1->setEnabled(checked);
    hd2->setEnabled(checked);
    browse_hd1->setEnabled(checked);
    browse_hd2->setEnabled(checked);
}

void Preferences::on_browse_ss_clicked()
{
    const QString name = QFileDialog::getSaveFileName(this, QString(), save_state->text());
    if (!name.isEmpty())
    {
        save_state->setText(name);
    }
}
