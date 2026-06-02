#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <qboxlayout.h>
#include <qpushbutton.h>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

private slots:

    void on_cityButton_clicked();
    void initTitleButtons();
    void onQueryClicked();//查询按钮


    void onWeatherReply(QNetworkReply*);//网络返回

private:
    Ui::Widget *ui;

    QPushButton *btnMin = nullptr;    // 最小化
    QPushButton *btnClose = nullptr;  // 关闭

    QNetworkAccessManager *m_net;
    QString  jwt;
    QTimer* m_jwtTimer;

    QMap<QString, QString> m_iconMap;

    void get7dayWeather(const QString& cityId);//获取未来天气
    void refreshJwt();//刷新JWT
    void setTodayHighLowTemp(const QJsonObject& todayObj);//高低温
    void set7DayForecast(const QJsonArray& dailyArray);//7天预报
    void setSingleDayWeather(int dayIndex, const QJsonObject& dayObj);//设置图标

};
#endif // WIDGET_H
