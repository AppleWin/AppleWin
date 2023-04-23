#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QDialog>

#include <vector>

class QComboBox;
class QSettings;
struct PreferenceData;

namespace Ui {
class Preferences;
}

class Preferences : public QDialog
{
    Q_OBJECT

public:

    explicit Preferences(QWidget *parent);
    ~Preferences();

    void setup(const PreferenceData & data, QSettings & settings);
    PreferenceData getData() const;

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

    void on_browse_ss_clicked();

    void on_colorButton_clicked();

    void on_browse_pf_clicked();

private:
    std::vector<QComboBox *> myDisks;
    std::vector<QComboBox *> myHDs;

    QColor myMonochromeColor;

    void setSettings(QSettings & settings);
    void setData(const PreferenceData & data);
    void populateJoysticks();
    void browseDisk(const std::vector<QComboBox *> & vdisks, const size_t id);

private:
    Ui::Preferences *ui;
};

#endif // PREFERENCES_H
