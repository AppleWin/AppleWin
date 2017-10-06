#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "ui_preferences.h"

#include <vector>
#include <boost/property_tree/ptree.hpp>

class Preferences : public QDialog, private Ui::Preferences
{
    Q_OBJECT

public:

    struct Data
    {
        int apple2Type;
        bool mouseInSlot4;
        bool cpmInSlot5;
        bool hdInSlot7;

        QString joystick;
        int joystickId;   // only putput

        std::vector<QString> disks;
        std::vector<QString> hds;
    };

    explicit Preferences(QWidget *parent);

    void setup(const Data & data, const boost::property_tree::ptree & registry);
    Data getData() const;

private slots:
    void on_disk1_activated(int index);
    void on_disk2_activated(int index);
    void on_hd1_activated(int index);
    void on_hd2_activated(int index);

    void on_browse_disk1_clicked();
    void on_browse_disk2_clicked();
    void on_browse_hd1_clicked();
    void on_browse_hd2_clicked();

    void on_hd_7_clicked(bool checked);

private:
    std::vector<QComboBox *> myDisks;
    std::vector<QComboBox *> myHDs;

    void setRegistry(const boost::property_tree::ptree & registry);
    void setData(const Data & data);
    void populateJoysticks();
    void browseDisk(const std::vector<QComboBox *> & disks, const size_t id);

};

#endif // PREFERENCES_H
