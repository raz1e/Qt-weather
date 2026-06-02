#include "widget.h"
#include <QFile>
#include "ui_widget.h"
#include <QInputDialog>
#include <QToolTip>
#include <QPixmap>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTimer>
#include <QProcess>
#include <QString>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    m_iconMap = {
        {"100", "100.png"}, // 晴
        {"101", "101.png"}, // 多云
        {"102", "101.png"}, // 少云 → 多云
        {"103", "101.png"}, // 晴间多云 → 多云
        {"104", "101.png"}, // 阴 → 多云
        {"300", "305.png"}, // 阵雨 → 小雨
        {"301", "305.png"}, // 强阵雨 → 小雨
        {"302", "302.png"}, // 雷阵雨
        {"303", "302.png"}, // 强雷阵雨
        {"304", "302.png"}, // 雷阵雨伴冰雹
        {"305", "305.png"}, // 小雨
        {"306", "305.png"}, // 中雨 → 小雨
        {"307", "305.png"}, // 大雨 → 小雨
        {"308", "305.png"}, // 暴雨 → 小雨
        {"400", "401.png"}, // 小雪 → 中雪
        {"401", "401.png"}, // 中雪
        {"402", "401.png"}, // 大雪 → 中雪
        {"403", "401.png"}, // 暴雪 → 中雪
        {"404", "401.png"}, // 雨夹雪 → 中雪
        // 其他未列的，后面用默认
    };

    //定时刷新JWT
    m_jwtTimer = new QTimer(this);

    m_net = new QNetworkAccessManager(this);
    connect(m_jwtTimer, &QTimer::timeout, this, &Widget::refreshJwt);
    m_jwtTimer->start(14*60*1000); // 14分钟刷新一次
    refreshJwt();



    // 绑定网络返回
    connect(m_net, &QNetworkAccessManager::finished,
            this, &Widget::onWeatherReply);

    //全局字体＋主窗口渐变背景
    this->setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei";
            font-size: 14px;
            color: #333333;
        }
        #MainWindow {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #87CEEB, stop:1 #B0E0E6);
        }
    )");

    // ============ 顶部卡片（城市+时间） ============
    ui->topCard->setStyleSheet(R"(
        QWidget#topCard {
            background-color: rgba(255,255,255,0.9);
            border-radius: 12px;
            padding: 10px;
        }
    )");

    // ============ 中部卡片（温度+图标） ============
    ui->middleCard->setStyleSheet(R"(
        QWidget#centerCard {
            background-color: rgba(255,255,255,0.95);
            border-radius: 16px;
            padding: 20px;
        }
    )");

    // ============ 底部卡片（7天预报） ============
    ui->bottomCard->setStyleSheet(R"(
        QWidget#bottomCard {
            background-color: rgba(255,255,255,0.9);
            border-radius: 12px;
            padding: 10px;
        }
    )");

    // ============ 七天小卡片统一样式 ============
    QString dayCardStyle = R"(
        QWidget {
            background-color: rgba(255,255,255,0.8);
            border-radius: 8px;
            padding: 8px;
        }
    )";
    for (int i = 1; i <= 7; ++i) {
        QString name = QString("dayCard%1").arg(i);
        QWidget *card = this->findChild<QWidget*>(name);
        if (card) {
            card->setStyleSheet(dayCardStyle);
        }
    }

    // ============ 城市按钮 ============
    ui->cityButton->setStyleSheet(R"(
        QPushButton#cityButton {
            background-color: rgba(255,255,255,0.8);
            border: none;
            border-radius: 8px;
            padding: 6px 12px;
            color: #333;
        }
        QPushButton#cityButton:hover {
            background-color: rgba(200,230,255,0.9);
        }
    )");

    QFile file(":/styles.qss");
    if (file.open(QFile::ReadOnly)) {
        QString sheet = QString::fromUtf8(file.readAll());
        setStyleSheet(sheet);
        file.close();
    } else {
        qDebug() << "Failed to open styles.qss";
    }

    this->setBackgroundRole(QPalette::Window);

    //退出与缩小
    initTitleButtons();

    // 初始化默认城市为南京
    ui->cityButton->setText("南京");

    // 延迟500毫秒后自动查询南京天气（等待JWT刷新完成）
    QTimer::singleShot(500, this, &Widget::onQueryClicked);

}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_cityButton_clicked()
{
    bool ok;
    QString currentCity = ui->cityButton->text();

    // 弹出输入对话框
    QString newCity = QInputDialog::getText(this,
                                            "切换城市",
                                            "请输入城市名：",
                                            QLineEdit::Normal,
                                            currentCity,
                                            &ok);

    // 如果用户点击了确定并且输入不为空
    if (ok && !newCity.isEmpty()) {
        // 更新按钮上显示的城市名
        ui->cityButton->setText(newCity);

        QToolTip::showText(QCursor::pos(),"正在切换到：" + newCity,this,QRect(),2000);

        onQueryClicked();
    }
}

void Widget::initTitleButtons()
{
    // 给主窗口加上无边框，这样标题栏就不显示了
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

    // 创建按钮，父对象设为this，Qt会自动管理内存
    btnMin = new QPushButton("—", this);
    btnClose = new QPushButton("✕", this);

    // 设置按钮大小和位置（直接定位，不使用布局）
    int btnSize = 30;
    int spacing = 6;
    int topMargin = 12;

    btnMin->setFixedSize(btnSize, btnSize);
    btnClose->setFixedSize(btnSize, btnSize);

    // 计算位置：从右上角往左排
    btnClose -> move ( this -> width () - btnSize - 10 ,topMargin);
    btnMin -> move ( this -> width () - 2 * btnSize - spacing- 10 ,topMargin);

    // 设置样式（半透明背景，hover高亮）
    btnMin->setStyleSheet(R"(
        QPushButton {
            background: rgba(255,100,100,160);
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 16px;
        }
        QPushButton:hover {
            background: #ff5555;
        }
    )");

    btnClose->setStyleSheet(R"(
        QPushButton {
            background: rgba(255,100,100,160);
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 16px;
        }
        QPushButton:hover {
            background: #ff5555;
        }
    )");

    // 连接信号槽
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
    connect(btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
}

void Widget::onQueryClicked()
{
    QString city = ui->cityButton->text();
    if (city.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入城市名");
        return;
    }

    //  显示温度
    QUrl url("https://j87p4468c6.re.qweatherapi.com/geo/v2/city/lookup");
    QUrlQuery query;
    query.addQueryItem("location", city);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + jwt.toUtf8());

    m_net->get(req);
}

//api获取天气
void Widget::onWeatherReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "网络错误:" << reply->errorString();
        QMessageBox::warning(this,"警告","网络错误: " + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    // 城市查询返回
    if (obj.contains("location")) {
        QJsonArray arr = obj["location"].toArray();
        if (arr.isEmpty()) {
            QMessageBox::warning(this,"警告","无此城市");
            return;
        }
        QString locId = arr[0].toObject()["id"].toString();

        //  再查天气
        QUrl url("https://j87p4468c6.re.qweatherapi.com/v7/weather/now");
        QUrlQuery query;
        query.addQueryItem("location", locId);
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", "Bearer " + jwt.toUtf8());
        m_net->get(request);

        // 同时请求7天预报
        get7dayWeather(locId);

        return;
    }

    // 天气查询返回
    if (obj.contains("now")) {
        QJsonObject now = obj["now"].toObject();
        QString temp = now["temp"].toString();
        QString text = now["text"].toString();
        QString icon = now["icon"].toString();

        ui->templable->setText(temp + " °C");
        ui->weatherDescribelable->setText(text);

        QString iconFile = m_iconMap.value(icon,"101.png");
        QString iconPath = ":/res/" + iconFile;
        QPixmap pix(iconPath);
        
        if (pix.isNull()) {
            qDebug() << "图标加载失败: " << iconPath << " (原始图标代码: " << icon << ")";
        } else {
            qDebug() << "图标加载成功: " << iconPath << " (原始图标代码: " << icon << ")";
            // 使用高质量缩放，避免像素丢失
            QSize labelSize = ui->weatherIconlabel->size();
            pix = pix.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        ui->weatherIconlabel->setPixmap(pix);
    }

    //7天预报
    if(obj.contains("daily"))
    {
        QJsonArray dailyList = obj["daily"].toArray();
        if(dailyList.isEmpty())return;

        setTodayHighLowTemp(dailyList[0].toObject());  // 今日高低温
        set7DayForecast(dailyList);                    // 7天预报
    }
}

//获取jwt
void Widget::refreshJwt()
{
    QProcess process;
    QString scriptPath = QCoreApplication::applicationDirPath() + "/dist/generate_jwt.exe";
    process.start(scriptPath);
    bool success = process.waitForFinished(5000);
    if (!success) {
        qDebug() << "刷新JWT失败:" << process.readAllStandardError();
        return;
    }
    jwt = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    qDebug() << "刷新JWT:" << jwt;
}

//获取7天天气预报
void Widget::get7dayWeather(const QString& cityId)
{
    QUrl url("https://j87p4468c6.re.qweatherapi.com/v7/weather/7d");
    QUrlQuery query;
    query.addQueryItem("location", cityId);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + jwt.toUtf8());
    m_net->get(req);
}

//7day预报
void Widget::set7DayForecast(const QJsonArray& dailyArray)
{
    for (int i = 0; i < 7 && i < dailyArray.size(); i++)
    {
        setSingleDayWeather(i + 1, dailyArray[i].toObject());
    }
}

//高低温
void Widget::setTodayHighLowTemp(const QJsonObject& todayObj)
{
    QString minTemp = todayObj["tempMin"].toString();
    QString maxTemp = todayObj["tempMax"].toString();
    ui->lowlable->setText("低" + minTemp + "°");
    ui->highlable->setText("高" + maxTemp + "°");
}

// 设置单天天气
void Widget::setSingleDayWeather(int dayIndex, const QJsonObject& dayObj)
{
    QString date = dayObj["fxDate"].toString();
    QString iconCode = dayObj["iconDay"].toString();
    QString tempMin = dayObj["tempMin"].toString();
    QString tempMax = dayObj["tempMax"].toString();

    // 拼接你的控件名
    QString dateLabelName = QString("datelabel_%1").arg(dayIndex);
    QString tempLabelName = QString("daytemplabel_%1").arg(dayIndex);
    QString iconLabelName = QString("reslabel_%1").arg(dayIndex);

    // 找到控件
    QLabel* dateLabel = findChild<QLabel*>(dateLabelName);
    QLabel* tempLabel = findChild<QLabel*>(tempLabelName);
    QLabel* iconLabel = findChild<QLabel*>(iconLabelName);

    if (!dateLabel || !iconLabel || !tempLabel) return;

    dateLabel->setText(date);
    tempLabel->setText(tempMin + "~" + tempMax + "°");

    QString iconFile = m_iconMap.value(iconCode, "101.png");
    QPixmap pix(":/res/" + iconFile);
    iconLabel->setPixmap(pix.scaled(40, 40, Qt::KeepAspectRatio));
}