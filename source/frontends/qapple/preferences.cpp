#include "preferences.h"
#include <QFileDialog>
#include <iostream>

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

}

Preferences::Preferences(QWidget *parent, const Data & data) :
    QDialog(parent)
{
    setupUi(this);

    // easier to handle in a vector
    myDisks.push_back(disk1);
    myDisks.push_back(disk2);
    myHDs.push_back(hd1);
    myHDs.push_back(hd2);

    initialiseDisks(myDisks, data.disks);
    initialiseDisks(myHDs, data.hds);
}

Preferences::Data Preferences::getData() const
{
    Data data;

    fillData(myDisks, data.disks);
    fillData(myHDs, data.hds);

    return data;
}

void Preferences::browseDisk(const std::vector<QComboBox *> & disks, const size_t id)
{
    QFileDialog diskFileDialog(this);
    diskFileDialog.setFileMode(QFileDialog::AnyFile);

    if (disks[id]->currentIndex() >= 1)
    {
        std::cout << "Set path = " << disks[id]->currentText().toStdString() << std::endl;
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

void Preferences::on_pushButton_clicked()
{
    browseDisk(myDisks, 0);
}

void Preferences::on_pushButton_2_clicked()
{
    browseDisk(myDisks, 1);
}

void Preferences::on_pushButton_3_clicked()
{
    browseDisk(myHDs, 0);
}

void Preferences::on_pushButton_4_clicked()
{
    browseDisk(myHDs, 1);
}
