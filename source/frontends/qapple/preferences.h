#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "ui_preferences.h"

#include <vector>

class Preferences : public QDialog, private Ui::Preferences
{
    Q_OBJECT

public:

    struct Data
    {
        std::vector<QString> disks;
        std::vector<QString> hds;
    };

    explicit Preferences(QWidget *parent, const Data & data);

    Data getData() const;

private slots:
    void on_disk1_activated(int index);
    void on_disk2_activated(int index);
    void on_hd1_activated(int index);
    void on_hd2_activated(int index);

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

private:
    std::vector<QComboBox *> myDisks;
    std::vector<QComboBox *> myHDs;

    void browseDisk(const std::vector<QComboBox *> & disks, const size_t id);

};

#endif // PREFERENCES_H
