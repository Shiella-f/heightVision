#include "matchWidget.h"
#include <QWidget> 
#include <QToolButton> 
#include <QSizePolicy> 
#include <QLabel> 
#include <QSpinBox>
#include <QDoubleSpinBox> 
#include <QCheckBox> 
#include <QVBoxLayout> 
#include <QHBoxLayout> 
#include <QGridLayout> 
#include <QComboBox> 
#include <QGraphicsView> 
#include <QMessageBox> 
#include <QDebug> 
#include <QTimer>
#include <QGroupBox>
#include <QFileDialog>
#include <QSlider>
#include <cmath>
#include <QScrollArea>
#include <QSpacerItem>
#include <QTransform>
#include <QCoreApplication>
#include <QDir>
#include <QStringList> // 使用 QStringList 维护候选路径列表

#include "Scene/ImageDisplayScene.h"
#include "../Scene/MatchScene.h"

matchWidget::matchWidget(QWidget* parent)
    : baseWidget(parent),m_templateManager(),m_hasLearnedTemplate(false),m_lastMatchResults(),
    m_paraWidget(nullptr),showParaWidget(nullptr),m_ImageProcess(),m_initParam()
{
    init();
    Q_INIT_RESOURCE(resources);

    QStringList qssCandidates;
    qssCandidates << ":/res/dark_style.qss";
    qssCandidates << QCoreApplication::applicationDirPath() + "/res/dark_style.qss";
    qssCandidates << QCoreApplication::applicationDirPath() + "/dark_style.qss";
    qssCandidates << QDir::currentPath() + "/res/dark_style.qss";
    for(const QString& path : qssCandidates) {
        qDebug() << "尝试加载样式表路径:" << path;
    }
    bool qssLoaded = false;
    for (const QString& path : qssCandidates) {
        QFile qssFile(path);
        if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
            m_loadedStyleSheet = QString::fromUtf8(qssFile.readAll());
            this->setStyleSheet(m_loadedStyleSheet);
            qssFile.close();
            qDebug() << "加载样式表成功:" << path;
            qssLoaded = true;
            break;
        }
    }
    if (!qssLoaded) {
        qWarning() << "样式表加载失败：请确认已编译 resources.qrc，或将 dark_style.qss 放到宿主程序目录的 res 目录下";
    }
    // m_initParam.canvasSize = QSize(459.866, 459.866);
    // m_initParam.unitedRect = QRect(100.06, 168.38, 51.096, 25.548);
    // QRectF rect = QRectF(m_initParam.unitedRect);
    // m_initParam.combinedPath.addRect(rect);
    // m_initParam.img = QImage("C:\\Users\\m1760\\Desktop\\bcadhicv141\\res\\image\\camera_capture_20260107_101657.png");
    m_ImageProcess = new ImageProcess();
    this->setWindowTitle(QStringLiteral("")); 
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    this->sizePolicy().setHorizontalStretch(0);
    this->sizePolicy().setVerticalStretch(0);
    this->resize(1200, 800);
    loadHeightPlugin(); // 加载测高插件
    loadCalibPlugin(); // 加载相机标定插件
    //loadMirrorCalibPlugin(); // 加载振镜标定插件
}

void matchWidget::applyLoadedStyleSheet(QWidget* widget)
{
    if (!widget) {
        return;
    }
    if (m_loadedStyleSheet.isEmpty()) {
        return;
    }
    widget->setStyleSheet(m_loadedStyleSheet);
}

matchWidget::~matchWidget()
{
    if (m_ImageProcess) {
        delete m_ImageProcess;
        m_ImageProcess = nullptr;
    }
    if(m_heightMainWindow) {
        delete m_heightMainWindow;
        m_heightMainWindow = nullptr;
    }
    if(m_mirrorCalibMainWindow) {
        delete m_mirrorCalibMainWindow;
        m_mirrorCalibMainWindow = nullptr;
    }
    if(m_CameraCalibMainWindow) {
        delete m_CameraCalibMainWindow;
        m_CameraCalibMainWindow = nullptr;
    }
}

bool matchWidget::hasLearnedTemplate() const
{
    return m_hasLearnedTemplate;
}

void matchWidget::init()
{

    QGraphicsView* roiImageView = new QGraphicsView(this);
    roiImageView->setMouseTracking(true);
    roiImageView->setInteractive(true);
    m_RoiDisplayScene = new ImageSceneBase(roiImageView);
    m_RoiDisplayScene->attachView(roiImageView);
    roiImageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    roiImageView->setFixedSize(180, 180);
    roiImageView->setStyleSheet("background-color: rgb(40, 40, 40);border:1px solid gray;");
    m_roiDisplayWidget = roiImageView;

    auto newButton = [](QToolButton* btn, const QString& text) {
        btn->setText(text);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        btn->setFixedSize(80, 20);
        return btn;
    };
    auto newLabel = [](QLabel* label, const QString& text) {
        label->setText(text);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return label;
    };

    auto newHorizontalLayout = [](QHBoxLayout* layout) {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        return layout;
    };

    m_openImageBtn = newButton(new QToolButton(this), QStringLiteral("打开图像"));
    connect(m_openImageBtn, &QToolButton::clicked, this, &matchWidget::OpenImage);
    m_confirmMatchBtn = newButton(new QToolButton, QStringLiteral("全图匹配"));
    connect(m_confirmMatchBtn, &QToolButton::clicked, this, &matchWidget::confirmMatch);

    m_clearBtn = newButton(new QToolButton, QStringLiteral("清除结果"));
    connect(m_clearBtn, &QToolButton::clicked, this, &matchWidget::clearToggled);

    m_drawpathBtn = newButton(new QToolButton(this), QStringLiteral("显示路径"));
    connect(m_drawpathBtn, &QToolButton::clicked, this, &matchWidget::DrawPathBtnClicked);

    m_confirmDrawBtn = newButton(new QToolButton(this), QStringLiteral("一键排版"));
    connect(m_confirmDrawBtn, &QToolButton::clicked, [this]() { if(confirmDrawBtnClicked()) emit sendDrawPath(); });

    m_shapeModeCombo = new QComboBox(this);
    m_shapeModeCombo->addItem(QStringLiteral("矩形"));
    m_shapeModeCombo->addItem(QStringLiteral("椭圆"));
    m_shapeModeCombo->addItem(QStringLiteral("自由绘制"));

    connect(m_shapeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx){
        if (!m_ImageDisplayScene) return;
        auto ms = static_cast<MatchScene*>(m_ImageDisplayScene);
        if (!ms) return;
        ms->setShapeMode(static_cast<MatchScene::ShapeMode>(idx));
    });

    m_compModeCombo = new QComboBox(this);
    m_compModeCombo->addItem(QStringLiteral("交集"));
    m_compModeCombo->addItem(QStringLiteral("并集"));
    m_compModeCombo->addItem(QStringLiteral("差集"));
    connect(m_compModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx){
        if (!m_ImageDisplayScene) return;
        m_ImageDisplayScene->setCompositionMode(static_cast<MatchScene::CompositionMode>(idx));
    });

    m_penWidthSpin = new QSpinBox(this);
    m_penWidthSpin->setRange(1, 50);
    m_penWidthSpin->setValue(3);
    connect(m_penWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v){ if (m_ImageDisplayScene) m_ImageDisplayScene->setPenWidth(v); });

    QGridLayout* btnLayout = new QGridLayout;
    btnLayout->addWidget(m_confirmMatchBtn, 0, 0);
    btnLayout->addWidget(m_clearBtn, 0, 1);
    btnLayout->addWidget(m_shapeModeCombo, 1, 0);
    btnLayout->addWidget(m_compModeCombo, 1, 1);
    btnLayout->addWidget(m_penWidthSpin, 2, 0);
    btnLayout->addWidget(m_openImageBtn, 2, 1);
    btnLayout->addWidget(m_drawpathBtn, 3, 0);
    btnLayout->addWidget(m_confirmDrawBtn, 3, 1);

    QVBoxLayout* rightLayout = new QVBoxLayout; // 右侧整体纵向布局
    m_paraWidgetshow = new QCheckBox(QStringLiteral("设置参数"), this);
    
    showParaWidget = new QWidget(this);


    m_paraWidget = new ParaWidget(this);
    paraScrollArea = new QScrollArea(this);
    paraScrollArea->setWidget(m_paraWidget);
    paraScrollArea->setWidgetResizable(true);
    QVBoxLayout* paraLayout = new QVBoxLayout;
    paraLayout->setContentsMargins(0, 0, 0, 0);
    paraLayout->addWidget(paraScrollArea);
    showParaWidget->setLayout(paraLayout);
    paraScrollArea->hide();
    m_paraWidgetshow->setChecked(false);
    rightLayout->addWidget(m_roiDisplayWidget, 0, Qt::AlignHCenter);
    connect(m_paraWidgetshow, &QCheckBox::toggled, this, [this](bool checked){
        if (paraScrollArea!= nullptr) {
            if (checked) {
                paraScrollArea->show();
            } else {
                paraScrollArea->hide();
            }
        }
    });

    m_matchselectCheckBox = new QCheckBox(QStringLiteral("模板选择"), this);
    m_matchselectCheckBox->setChecked(true);
    connect(m_matchselectCheckBox, &QCheckBox::toggled, this, [this](bool checked){
        if (m_ImageDisplayScene) {
            auto ms = static_cast<MatchScene*>(m_ImageDisplayScene);
            if (!ms) return;
            ms->setSelectionTemplateEnabled(checked);
        }
    });
    QHBoxLayout* checkBoxLayout = new QHBoxLayout; // 右侧整体纵向布局
    checkBoxLayout->addWidget(m_paraWidgetshow);
    checkBoxLayout->addWidget(m_matchselectCheckBox);
    rightLayout->addLayout(checkBoxLayout);
    rightLayout->addWidget(showParaWidget, 3, Qt::AlignHCenter);
    rightLayout->addLayout(btnLayout);
    rightLayout->addStretch();

    QWidget *rightWidget = new QWidget(this);
    QVBoxLayout* rightWidgetLayout = new QVBoxLayout(rightWidget);
    rightWidgetLayout->setContentsMargins(0, 0, 0, 0);
    rightWidgetLayout->addLayout(rightLayout);
    rightWidget->setFixedWidth(180); // 建议固定右侧宽度，防止调整大小时布局混乱


    QGraphicsView* imageView = new QGraphicsView(this); // 主图像视图
    imageView->setMouseTracking(true); // 启用鼠标跟踪
    imageView->setInteractive(true);
    m_imageDisplayWidget = imageView; // 保存主视图指针
    m_ImageDisplayScene = new MatchScene(imageView);
    m_ImageDisplayScene->attachView(imageView); // 让场景附加到视图，启用缩放等行为
    m_ImageDisplayScene->setSelectionTemplateEnabled(true); // 允许选择/绘制模板区域
    imageView->setMouseTracking(true);
    imageView->setStyleSheet("background-color: rgb(40, 40, 40);border:1px solid gray;");
    imageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 连接异步匹配完成信号
    connect(&m_matchWatcher, &QFutureWatcher<std::vector<MatchResult>>::finished, this, &matchWidget::onMatchFinished);

    QImage qimg; // 临时 QImage 用于显示
    try {
        if (!m_currentImage.empty() && TIGER_BSVISION::cvImage2qImage(qimg, m_currentImage)) { 
            m_ImageDisplayScene->setOriginalPixmap(QPixmap::fromImage(qimg));
            m_ImageDisplayScene->setSourceImageSize(qimg.size()); 
            // 确保图片显示在视图中心
            imageView->fitInView(m_ImageDisplayScene->itemsBoundingRect(), Qt::KeepAspectRatio);
            imageView->centerOn(m_ImageDisplayScene->itemsBoundingRect().center());
        } else {
            m_ImageDisplayScene->showPlaceholder(QStringLiteral("No Image"));
        }
        QTimer::singleShot(0, this, [this](){ if (m_ImageDisplayScene) m_ImageDisplayScene->resetImage(); });
    } catch (const cv::Exception &e) {
        qWarning().noquote() << "cv::Exception in cvImage2qImage:" << e.what();
        m_ImageDisplayScene->showPlaceholder(QStringLiteral("Image Error"));
    }

        // 创建自定义标题栏
    CustomTitleBar* titleBar = new CustomTitleBar(this);
    titleBar->setTitle("Orion Vision 窗口");
    connect(titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(titleBar, &CustomTitleBar::closeClicked, this, &QWidget::close);

    m_CameraCalibBtn = newButton(new QToolButton(this), QStringLiteral("标定"));
    m_testHeightBtn = newButton(new QToolButton(this), QStringLiteral("高度测量"));

    connect(m_CameraCalibBtn, &QToolButton::clicked, this, &matchWidget::CameraCalibBtnClicked);
    connect(m_testHeightBtn, &QToolButton::clicked, this, &matchWidget::testHeightBtnClicked);
    
    QHBoxLayout* functionLayout = new QHBoxLayout();
    functionLayout->setAlignment(Qt::AlignLeft);
    functionLayout->addWidget(m_CameraCalibBtn, Qt::AlignLeft);
    functionLayout->addWidget(m_testHeightBtn, Qt::AlignLeft);
    // 内容区域布局：左侧图像，右侧控制面板
    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(10, 10, 10, 10);
    contentLayout->addWidget(imageView, 1); // 自动伸缩
    contentLayout->addWidget(rightWidget, 0); // 固定宽度，紧贴右侧

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(titleBar);
    mainLayout->addLayout(functionLayout);
    mainLayout->addLayout(contentLayout);
    connect(m_ImageDisplayScene, &MatchScene::totalPathPathChanged,this, &matchWidget::learnMatch);

}
// 全图匹配槽实现
void matchWidget::confirmMatch()
{
    executeMatch(false);
}
// 学习模板槽实现
bool matchWidget::learnMatch()
{
    if (!m_ImageDisplayScene) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("主显示场景不可用"));
        return false;
    }

    cv::Mat templateMat = m_ImageDisplayScene->extractTemplate();
    if (templateMat.empty()) {
        QMessageBox::information(this, QStringLiteral("模板"), QStringLiteral("未选择 ROI 或模板为空")); 
        return false;
    }

    buildMatchParams();
    if (!m_templateManager.learnTemplate(m_currentImage, templateMat, m_MatchParams)) {
        QMessageBox::warning(this, QStringLiteral("模板提取"), QStringLiteral("模板提取失败，请检查图像质量。"));
        return false;
    }
    m_learnedFeaturePoints.clear();
    m_learnedKeypoints.clear();
    m_learnedFeaturePoints = m_templateManager.currentFeaturePoints();
    m_learnedKeypoints.reserve(m_learnedFeaturePoints.size());
    for(const auto& pt : m_learnedFeaturePoints){
        m_learnedKeypoints.emplace_back(pt, 1.0f);
    }

    m_hasLearnedTemplate = true;

    if (m_RoiDisplayScene) {
        QImage templmg;
        if (TIGER_BSVISION::cvImage2qImage(templmg, templateMat)) {
            m_RoiDisplayScene->setOriginalPixmap(QPixmap::fromImage(templmg));
            m_RoiDisplayScene->setSourceImageSize(templmg.size()); 
            m_RoiDisplayScene->clearOverlays();

            QPen ptPen(Qt::green);
            ptPen.setWidth(1);
            for (const auto &kp : m_learnedKeypoints) {
                m_RoiDisplayScene->addOverlayPoint(QPointF(kp.pt.x, kp.pt.y), ptPen, 2); // 在预览上绘制关键点
            }
        }
        m_RoiDisplayScene->resetImage();
    }

    if (m_ImageDisplayScene) {
        QRect roi = static_cast<MatchScene*>(m_ImageDisplayScene)->getTemplateBoundingRect(); // 获取 ROI
        if (!roi.isEmpty()) {
            m_ImageDisplayScene->clearOverlays();

            QPen ptPen(Qt::green);
            ptPen.setWidth(1);
            for (const auto &kp : m_learnedKeypoints) {
                m_ImageDisplayScene->addOverlayPoint(QPointF(roi.x() + kp.pt.x, roi.y() + kp.pt.y), ptPen, 2);
            }
        }
    }
    qInfo().noquote() << "Learned" << m_learnedFeaturePoints.size() << "feature points.";
    return true;
}

void matchWidget::clearToggled()
{
    if (m_ImageDisplayScene) {
        auto ms = static_cast<MatchScene*>(m_ImageDisplayScene); // 转换为 MatchScene
        if (ms) {
            ms->clear(); // 清除场景中的 ROI
        }
        m_ImageDisplayScene->clearOverlays(); // 清除覆盖图形
    }
}

void matchWidget::buildMatchParams()
{
    m_MatchParams = m_paraWidget->buildMatchParams();
}

void matchWidget::buildFindMatchParams(bool useRoi)
{
    m_FindMatchParams = m_paraWidget->buildFindMatchParams(useRoi);
}

void matchWidget::executeMatch(bool useRoi = false)
{
    if (m_currentImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("操作无效"), QStringLiteral("当前没有可用图像，请先加载图像。"));
        return;
    }
    
    if (!m_templateManager.hasTemplate()) {
        QMessageBox::warning(this, QStringLiteral("匹配失败"), QStringLiteral("请框选正确的模板区域。"));
        return;
    }
    // 禁用按钮，防止重复点击
    m_confirmMatchBtn->setEnabled(false);
    
    // 准备匹配参数
    buildFindMatchParams(useRoi);
    FindMatchParams findParams = m_FindMatchParams;
    if (!useRoi) {
        findParams.Mask.release(); // 全图匹配直接清空掩膜
    } 
    QFuture<std::vector<MatchResult>> future = QtConcurrent::run([this, findParams]() {
        return m_templateManager.matchTemplate(m_currentImage, findParams);
    });

    m_matchWatcher.setFuture(future);
}

void matchWidget::onMatchFinished()
{
    // 恢复按钮状态
    if (m_hasLearnedTemplate) {
        m_confirmMatchBtn->setEnabled(true);
    }

    // 获取结果并渲染
    std::vector<MatchResult> results = m_matchWatcher.result();
    m_lastMatchResults = results; // 保存结果以备后续使用
    renderMatchResults(results);
}

void matchWidget::renderMatchResults(const std::vector<MatchResult> &results)
{
    if(results.empty()) {
        QMessageBox::information(this, QStringLiteral("匹配完成"), QStringLiteral("未找到匹配结果"));
        return;
    }
    if (m_ImageDisplayScene) {
        m_ImageDisplayScene->clearOverlays(); // 清除之前的覆盖物
        const std::array<QColor, 3> palette{QColor(0, 220, 0), QColor(255, 215, 0), QColor(255, 128, 0)};
        QPen rectPen(Qt::red);
        rectPen.setWidth(2);
        QPen ptPen(Qt::green);
        ptPen.setWidth(1);
        QPen CertenPen(Qt::red);
        CertenPen.setWidth(2);
        QPen resultsPenRect(Qt::green);
        resultsPenRect.setWidth(2);

        for (const auto &res : results) {
            // 如果有变换后的特征点，计算其最小外接旋转矩形并绘制为多边形
            if (!res.transformedFeaturePoints.empty()) {
                std::vector<cv::Point2f> pts = res.transformedFeaturePoints;
                cv::RotatedRect rr = res.rotatedRect;
                cv::Point2f boxPts[4];
                rr.points(boxPts);
                QPolygonF poly;
                poly.reserve(4);
                for (int i = 0; i < 4; ++i) {
                    QPointF imgPt(boxPts[i].x, boxPts[i].y);
                    poly << imgPt;
                }
                QPolygonF minpoly;
                    minpoly << QPointF(res.Roiarea.x, res.Roiarea.y)
                            << QPointF(res.Roiarea.x + res.Roiarea.width, res.Roiarea.y)
                            << QPointF(res.Roiarea.x + res.Roiarea.width, res.Roiarea.y +  res.Roiarea.height)
                            << QPointF(res.Roiarea.x, res.Roiarea.y + res.Roiarea.height);
                m_ImageDisplayScene->addOverlayPolygon(poly, rectPen);
                m_ImageDisplayScene->addOverlayPolygon(minpoly, resultsPenRect);
                
                for (const auto &p : res.transformedFeaturePoints) {
                    m_ImageDisplayScene->addOverlayPoint(QPointF(p.x, p.y), ptPen, 2.0);
                }
                m_ImageDisplayScene->addOverlayPoint(QPointF(res.center.x, res.center.y), CertenPen, 4.0);
            } else {
                QRect qr(res.Roiarea.x, res.Roiarea.y, res.Roiarea.width, res.Roiarea.height);
                m_ImageDisplayScene->addOverlayRect(qr, rectPen);
                m_ImageDisplayScene->addOverlayPoint(QPointF(res.center.x, res.center.y), CertenPen, 4.0);
            }
        }
    }

    const MatchResult &best = results.front();
    for(const auto& res : results){
        qDebug().noquote() << QStringLiteral("匹配结果: 位置=(%1, %2), 角度=%3°,缩放=%4, 分数=%5")
                                .arg(res.center.x, 0, 'f', 2)
                                .arg(res.center.y, 0, 'f', 2)
                                .arg(res.angle, 0, 'f', 2)
                                .arg(res.scale, 0, 'f', 2)
                                .arg(res.score * 100.0, 0, 'f', 1);
    }
     const QString summary = QStringLiteral("共找到 %1 个结果，最高分 %2, 角度 %3°, 最低分 %4")
                                .arg(results.size())
                                .arg(best.score * 100.0)
                                .arg(best.angle)
                                .arg(results.back().score * 100.0);
    // QMessageBox::information(this, QStringLiteral("匹配完成"), summary);
    qInfo() << summary;
}

bool matchWidget::setcurrentImage(const cv::Mat &image)
{
    if (image.empty()) {
        return false;
    }
    m_currentImage = image.clone();
    QImage qimg; // 临时 QImage 用于显示
     if (!m_currentImage.empty() && TIGER_BSVISION::cvImage2qImage(qimg, m_currentImage)) { 
            m_ImageDisplayScene->setOriginalPixmap(QPixmap::fromImage(qimg));
            m_ImageDisplayScene->setSourceImageSize(qimg.size()); 
            // 确保图片显示在视图中心
            if (m_imageDisplayWidget) {
                QGraphicsView* view = qobject_cast<QGraphicsView*>(m_imageDisplayWidget);
                if (view) {
                    view->fitInView(m_ImageDisplayScene->itemsBoundingRect(), Qt::KeepAspectRatio); // 调整视图以适应图片
                    view->centerOn(m_ImageDisplayScene->itemsBoundingRect().center()); // 将视图中心对准图片中心
                }
            }
        } else {
            m_ImageDisplayScene->showPlaceholder(QStringLiteral("No Image"));
        }
    return true;
}

void matchWidget::OpenImage()
{
    const QString m_testImagePath = QFileDialog::getOpenFileName(nullptr, ("打开图片"), "", ("Images (*.png *.xpm *.jpg *.bmp);;All Files (*)"));
    if (m_testImagePath.isEmpty())
        return ;
    cv::Mat img = cv::imread(m_testImagePath.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法加载所选图像文件"));
        return;
    }
    setcurrentImage(img);
}

bool matchWidget::setMoveRotateData()
{
    double x = m_cale_initParam.unitedRect.x() + m_cale_initParam.unitedRect.width() / 2.0;
    double y = m_cale_initParam.unitedRect.y() + m_cale_initParam.unitedRect.height() / 2.0;
    initPoint = QPointF(x, y);
    if(m_lastMatchResults.empty()) {
        return false;
    }
    m_MoveRotateData.clear();
    QVector<double> distances;
    distances.reserve(m_lastMatchResults.size());
    for (int i = 0; i < static_cast<int>(m_lastMatchResults.size()); ++i) {
        double dx = m_lastMatchResults[i].center.x - initPoint.x();
        double dy = m_lastMatchResults[i].center.y - initPoint.y();
        double distance = std::sqrt(dx * dx + dy * dy);
        distances.append(distance);
    }
    int disMinIndex = 0;
    for(int i = 1; i < distances.size(); ++i){
        if(distances[i] < distances[disMinIndex]){
            disMinIndex = i;
        }
    }
    for(int i = 0; i < static_cast<int>(m_lastMatchResults.size()); ++i){
        if(i == disMinIndex){
            continue;
        }
        double dx = m_lastMatchResults[i].center.x -m_lastMatchResults[disMinIndex].center.x;
        double dy = m_lastMatchResults[i].center.y - m_lastMatchResults[disMinIndex].center.y;
        double angle = m_lastMatchResults[i].angle - m_lastMatchResults[disMinIndex].angle;
        m_MoveRotateData.addData(QPointF(dx, dy), -angle);
    }


    if(m_MoveRotateData.moveOffsets.size() != m_MoveRotateData.angles.size()){
        return false;
    }
    m_MoveRotateData.totalCounts = m_MoveRotateData.moveOffsets.size();
    return true;
}

std::vector<cv::Point2f> matchWidget::getPhysicalPoint(const std::vector<cv::Point2f> imagePoint)
{
    if(!m_isCalibLoaded_Mirror || HomographyMatrix.empty() || imagePoint.empty()){
        return {};
    }
    std::vector<cv::Point2f> physicalPoints;
    cv::perspectiveTransform(imagePoint, physicalPoints, HomographyMatrix);
    return physicalPoints;
}

void matchWidget::connectUDPReceiver()
{
    auto* m_UDPMatReceiver = new UDPMatReceiver(this);

    connect(m_UDPMatReceiver, &UDPMatReceiver::statusText, this, [](const QString& s){
    qDebug() << s;
    });

    // 注意：cv::Mat 是引用计数对象，跨线程传递一般可用；
    // 但如果你会在接收线程继续复用/修改 frame，建议在槽里 frame.clone()。
    auto udpDataReceived = std::make_shared<bool>(false); // 使用共享指针记录是否收到数据，避免局部变量悬空
    auto udpAttemptCount = std::make_shared<int>(0); // 使用共享指针记录已尝试次数

    connect(m_UDPMatReceiver, &UDPMatReceiver::frameReady, this, [&, udpDataReceived](const cv::Mat& frame){
        *udpDataReceived = true; // 收到首帧标记成功
        cv::Mat local = frame.clone(); // 拷贝一份图像避免线程安全问题
        setcurrentImage(m_ImageProcess->CalibImage(local)); // 校正后更新到界面
    });
    // 注意：0.0.0.0 是服务器监听地址（表示监听所有网卡），客户端发送必须指定具体 IP。
    // 如果服务器在本地，用 "127.0.0.1"；如果在树莓派/其他机器，请输入它的局域网 IP（如 "192.168.1.x"）。
    QTimer* udpRetryTimer = new QTimer(this); // 创建重试定时器
    udpRetryTimer->setInterval(500); // 设置 500ms 间隔
    connect(udpRetryTimer, &QTimer::timeout, this, [this, m_UDPMatReceiver, udpRetryTimer, udpDataReceived, udpAttemptCount]() {
        if (*udpDataReceived) { // 如果已收到数据则认为成功
            qDebug() << "UDP 已收到首帧，停止重试"; // 打印成功日志
            udpRetryTimer->stop(); // 停止定时器
            udpRetryTimer->deleteLater(); // 释放定时器
            return; // 结束本次回调
        }
        if (*udpAttemptCount >= 10) { // 超过 5 次未收到数据则放弃
            qWarning() << "UDP 未收到数据，停止重试，共尝试" << *udpAttemptCount << "次"; // 打印失败日志
            udpRetryTimer->stop(); // 停止定时器
            udpRetryTimer->deleteLater(); // 释放定时器
            m_UDPMatReceiver->stop(); // 停止内部重连定时器，避免继续发送握手日志
            return; // 结束本次回调
        }
        ++(*udpAttemptCount); // 增加尝试计数
        m_UDPMatReceiver->start("192.168.137.131", 9000); // 发起一次握手尝试等待数据
    });
    udpRetryTimer->start(); // 启动定时器开始尝试接收数据
}

 QVariantMap matchWidget::getMoveRotateDataMap() const{
    QVariantMap map;
    CopyItems_Move_Rotate_Data data = getMoveRotateData(); // 获取原有数据
    // 根据缩放比例调整偏移量
    if(m_scaleX != 0 || m_scaleY != 0){
        for(int i = 0; i < data.moveOffsets.size(); ++i){
            QPointF scaledPt(data.moveOffsets[i].x() * (1.0/m_scaleX), data.moveOffsets[i].y() * (1.0/m_scaleY));
            data.moveOffsets[i] = scaledPt;
        }
    }
    // 将 QVector 转换为 QVariantList
    QVariantList offsetsList;
    for(const auto& pt : data.moveOffsets) {
        offsetsList << QVariant::fromValue(pt);
    }

    QVariantList anglesList;
    for(const auto& angle : data.angles) {
        anglesList << angle;
    }

    map["moveOffsets"] = offsetsList;
    map["angles"] = anglesList;
    map["totalCounts"] = data.totalCounts;

    return map;
}

void matchWidget::DrawPathBtnClicked()
{
    scaleinitData();
    setMoveRotateData();

    if (m_ImageDisplayScene) {
        auto ms = static_cast<MatchScene*>(m_ImageDisplayScene); // 转换为 MatchScene
        if (!ms) {
            return;
        }
    }
        if(m_cale_initParam.combinedPath.isEmpty()){
        return ;
    }

    QPointF pathCenter(m_cale_initParam.unitedRect.x() + m_cale_initParam.unitedRect.width() / 2.0,
                       m_cale_initParam.unitedRect.y() + m_cale_initParam.unitedRect.height() / 2.0);
    QPointF pivotPoint = pathCenter; // 默认回退到路径中心
    
    if (!m_lastMatchResults.empty()) {
        int bestIdx = 0;
        double minDistance = std::numeric_limits<double>::max();
        for (int i = 0; i < static_cast<int>(m_lastMatchResults.size()); ++i) {
            double dx = m_lastMatchResults[i].center.x - pathCenter.x();
            double dy = m_lastMatchResults[i].center.y - pathCenter.y();
            double dist = dx*dx + dy*dy;
            if (dist < minDistance) {
                minDistance = dist;
                bestIdx = i;
            }
        }
        pivotPoint.setX(m_lastMatchResults[bestIdx].center.x); // 使用基准匹配的中心作为旋转轴心
        pivotPoint.setY(m_lastMatchResults[bestIdx].center.y); // 使用基准匹配的中心作为旋转轴心
    }

    m_drawPaths = copyPainterPathWithTransform(m_cale_initParam.combinedPath,
                                                    m_MoveRotateData.moveOffsets,
                                                    m_MoveRotateData.angles,
                                                    pivotPoint);                                          
    m_ImageDisplayScene->clear();
    for(int i = 1; i < m_drawPaths.size(); ++i){
        m_MoveRotateData.moveOffsets[i-1] = QPointF(m_drawPaths[i].boundingRect().center().x(),
                                                  m_drawPaths[i].boundingRect().center().y());
        qDebug().noquote() << QStringLiteral("路径数据: 位置=(%1, %2), 角度=%3°")
                                .arg(m_MoveRotateData.moveOffsets[i-1].x(), 0, 'f', 2)
                                .arg(m_MoveRotateData.moveOffsets[i-1].y(), 0, 'f', 2)
                                .arg(m_MoveRotateData.angles[i-1], 0, 'f', 2);
    }
    m_ImageDisplayScene->DrawTotalPath(m_drawPaths);
}

QVector<QPainterPath> matchWidget::copyPainterPathWithTransform(const QPainterPath& originalPath, const QVector<QPointF>& moveOffsets,
                                                   const QVector<double>& angles, QPointF pivotPoint)
{
    auto ms = qobject_cast<MatchScene*>(m_ImageDisplayScene);
    QVector<QPainterPath> newPaths;
    newPaths.append(originalPath); // 包含原始路径
    if (moveOffsets.size() != angles.size()) {
        qWarning() << "copyPainterPathWithTransform：偏移量数量(" << moveOffsets.size() 
                   << ")与角度数量(" << angles.size() << ")不一致！";
        return newPaths;
    }
    QTransform imgToScene;
    if (ms) {
        QPointF p0 = ms->imageToScenePoint(QPointF(0, 0));
        QPointF px = ms->imageToScenePoint(QPointF(1, 0));
        QPointF py = ms->imageToScenePoint(QPointF(0, 1));
        imgToScene = QTransform(px.x() - p0.x(), px.y() - p0.y(),
                                py.x() - p0.x(), py.y() - p0.y(),
                                p0.x(), p0.y());
    }

    QPainterPath scenePath = ms ? imgToScene.map(originalPath) : originalPath;

    // 计算场景坐标系下的旋转轴心
    QPointF pivotScene = ms ? ms->imageToScenePoint(pivotPoint) : pivotPoint;
    QPointF vecRef = ms ? ms->imageToScenePoint(QPointF(0,0)) : QPointF(0,0);

    for (int i = 0; i < moveOffsets.size(); ++i) {
        //const QPointF& offsetImg = moveOffsets[i]; // 这是 Image 坐标下的位移 vector
        QPointF offsetImg = QPointF();
        offsetImg.setX(moveOffsets[i].x());
        offsetImg.setY(moveOffsets[i].y());
        QPointF offsetScene = offsetImg;
        if (ms) {
            QPointF pOffset = ms->imageToScenePoint(offsetImg);
            offsetScene = pOffset - vecRef;
        }
        double angle = angles[i];
        QTransform transform;
        transform.translate(pivotScene.x() + offsetScene.x(), pivotScene.y() + offsetScene.y()); 
        transform.rotate(angle);
        transform.translate(-pivotScene.x(), -pivotScene.y()); 
        // 使用已经转换到场景坐标的 path
        QPainterPath newPath = transform.map(scenePath);
        newPaths.append(newPath);
    }
    return newPaths;
}

void matchWidget::scaleinitData()
{
    if(m_initParam.combinedPath.isEmpty()){
        return;
    }
    m_scaleX = m_initParam.img.width()/m_initParam.canvasSize.width();
    m_scaleY = m_initParam.img.height()/m_initParam.canvasSize.height();
    QTransform coordTransform;
    // 按坐标系的X、Y比例缩放
    coordTransform.scale(m_scaleX, m_scaleY);
    // 2. 将变换应用到原始路径，得到适配新坐标系的路径
    m_cale_initParam.combinedPath = coordTransform.map(m_initParam.combinedPath);
    m_cale_initParam.img = m_initParam.img;
    m_cale_initParam.canvasSize = m_initParam.canvasSize;
    m_cale_initParam.unitedRect.setX(m_initParam.unitedRect.x() *  m_scaleX);
    m_cale_initParam.unitedRect.setY(m_initParam.unitedRect.y() *  m_scaleY);
    m_cale_initParam.unitedRect.setWidth(m_initParam.unitedRect.width() *  m_scaleX);
    m_cale_initParam.unitedRect.setHeight(m_initParam.unitedRect.height() *  m_scaleY);
}

bool matchWidget::confirmDrawBtnClicked()
{
    if (m_drawPaths.isEmpty()) return false;
    if (!m_ImageDisplayScene) return false;
    // 获取场景中所有的图元
    QList<QGraphicsItem*> items = m_ImageDisplayScene->items();
    QVector<InteractivePathItem*> pathItems;
    
    // 筛选出 InteractivePathItem 类型的图元
    for (QGraphicsItem* item : items) {
        if (item->type() == InteractivePathItem::Type) {
            pathItems.append(static_cast<InteractivePathItem*>(item));
        }
    }

    // items() 通常按 Z 轴顺序（最上层在前）返回，添加顺序通常是最早的在最下层
    // 因此需要反转以匹配原始 m_drawPaths 的顺序
    std::reverse(pathItems.begin(), pathItems.end());

    // 检查数量是否一致（可选）
    if (pathItems.size() != m_drawPaths.size()) {
        qWarning() << "Scene items count mismatch with m_drawPaths";
    }

    bool anyChanged = false;
    QVector<QPainterPath> updatedPaths = m_drawPaths; // 创建副本用于存储更新后的路径
    
    // 记录基准项（pathItems[0]）的移动和旋转，用于调整后续项的相对坐标
    QPointF baseOffset(0, 0);
    double baseAngle = 0.0;

    for (int i = 0; i < pathItems.size() && i < m_drawPaths.size(); ++i) {
        InteractivePathItem* item = pathItems[i];
        QPointF offset = item->pos(); // 获取用户拖拽产生的偏移
        double angle = item->rotation(); // 获取旋转
        
        // 1. 更新路径：将变换应用到路径本身，以便重置 item 位置
        // 即使没有偏移，执行 map 也是安全的（相当于原样复制）
        updatedPaths[i] = item->sceneTransform().map(item->path());
        
        if (offset.isNull() && qFuzzyCompare(angle, 0.0)) {
            continue;
        }
        anyChanged = true;

        if (i == 0) {
            baseOffset = offset;
            baseAngle = angle;
        } else {
            int dataIndex = i - 1;
            if (dataIndex >= 0 && dataIndex < m_MoveRotateData.moveOffsets.size()) {
                m_MoveRotateData.moveOffsets[dataIndex] += (offset - baseOffset);
                m_MoveRotateData.angles[dataIndex] += (angle - baseAngle);
            }
        }
    }
    if (anyChanged) {
        m_drawPaths = updatedPaths;
        m_ImageDisplayScene->DrawTotalPath(m_drawPaths);
        qDebug() << "Paths adjusted. Count:" << m_MoveRotateData.totalCounts;
    }
    return true;
}

void matchWidget::CameraCalibBtnClicked()
{
    if (m_CameraCalibMainWindow != nullptr) 
    {
        applyLoadedStyleSheet(m_CameraCalibMainWindow);
        m_CameraCalibMainWindow->setWindowFlags(Qt::Window |Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
        m_CameraCalibMainWindow->show();
        m_CameraCalibMainWindow->raise();
        m_CameraCalibMainWindow->activateWindow();
    }else if(m_cameraCalibPluginFactory)
    {
        m_CameraCalibMainWindow = m_cameraCalibPluginFactory->createWidget(nullptr);
        applyLoadedStyleSheet(m_CameraCalibMainWindow);
        //m_CameraCalibMainWindow->setWindowFlags(Qt::Window |Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
        m_CameraCalibMainWindow->show();
    }else{
        loadCalibPlugin();
        if(m_cameraCalibPluginFactory)
        {
            m_CameraCalibMainWindow = m_cameraCalibPluginFactory->createWidget(nullptr);
            applyLoadedStyleSheet(m_CameraCalibMainWindow);
            //m_CameraCalibMainWindow->setWindowFlags(Qt::Window |Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
            m_CameraCalibMainWindow->show();
            m_CameraCalibMainWindow->raise();
            m_CameraCalibMainWindow->activateWindow();
        }else {
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("标定插件加载失败！"));
        }
    }
}

void matchWidget::testHeightBtnClicked()
{
    if (m_heightMainWindow) {
        applyLoadedStyleSheet(m_heightMainWindow);
        //m_heightMainWindow->setWindowFlags(Qt::Window |Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
        m_heightMainWindow->show();
        m_heightMainWindow->raise();
        m_heightMainWindow->activateWindow();
    }else if(m_heightPluginFactory){
        m_heightMainWindow = m_heightPluginFactory->createWidget(nullptr);
        applyLoadedStyleSheet(m_heightMainWindow);
        //m_heightMainWindow->setWindowFlags(Qt::Window |Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
        m_heightMainWindow->show();
    }else{
        loadHeightPlugin();
        if(m_heightPluginFactory){
            m_heightMainWindow = m_heightPluginFactory->createWidget(nullptr);
            applyLoadedStyleSheet(m_heightMainWindow);
            //m_heightMainWindow->setWindowFlags(Qt::Window |Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
            m_heightMainWindow->show();
            m_heightMainWindow->raise();
            m_heightMainWindow->activateWindow();
    }else{
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("测高插件加载失败！"));
    }
    }
}

void matchWidget::loadHeightPlugin()
{
     // 获取应用程序目录
    QDir pluginsDir(QCoreApplication::applicationDirPath());
    
    // 尝试加载 TemplateMatchPlugin
    QString pluginName = "HeightMeaturePlugin";
    QStringList paths = {
        pluginsDir.absoluteFilePath(pluginName),
        pluginsDir.absoluteFilePath("plugins/" + pluginName),
        pluginsDir.absoluteFilePath("src/plugins/heightMeature/" + pluginName),
        pluginsDir.absoluteFilePath("../src/plugins/heightMeature/" + pluginName)
    };
    for(const QString& path : paths) {
        // 使用 QPluginLoader 加载
        QPluginLoader loader(path, this);
        QObject *plugin = loader.instance();
        
        if (plugin) {
            // 尝试转换为工厂接口
            m_heightPluginFactory = qobject_cast<HeightPluginInterface *>(plugin);
            if (m_heightPluginFactory) {
                qDebug() << "HeightMeasurePlugin loaded successfully from: " + path;
                return;
            } else {
                qDebug() << QString("Failed to cast plugin from %1 to HeightPluginInterface: ").arg(path) + loader.errorString();
            }
        } else {
            qDebug() << "Failed to load HeightMeasurePlugin from " + path + ": " + loader.errorString();
        }
    }
    qDebug() << "HeightMeasurePlugin could not be loaded from any known path.";
}

void matchWidget::loadCalibPlugin()
{
         // 获取应用程序目录
    QDir pluginsDir(QCoreApplication::applicationDirPath());
    
    // 尝试加载 TemplateMatchPlugin
    QString pluginName = "CalibPlugin";
    QStringList paths = {
        pluginsDir.absoluteFilePath(pluginName),
        pluginsDir.absoluteFilePath("plugins/" + pluginName),
        pluginsDir.absoluteFilePath("src/Calib/plugins/Calib/" + pluginName),
        pluginsDir.absoluteFilePath("../src/Calib/plugins/Calib/" + pluginName)
    };
    for(const QString& path : paths) {
        // 使用 QPluginLoader 加载
        QPluginLoader loader(path, this);
        QObject *plugin = loader.instance();
        
        if (plugin) {
            // 尝试转换为工厂接口
            m_cameraCalibPluginFactory = qobject_cast<CalibInterface *>(plugin);
            if (m_cameraCalibPluginFactory) {
                qDebug() << "CalibPlugin loaded successfully from: " + path;
                return;
            } else {
                qDebug() << QString("Failed to cast plugin from %1 to CalibInterface: ").arg(path) + loader.errorString();
            }
        } else {
            qDebug() << "Failed to load CalibPlugin from " + path + ": " + loader.errorString();
        }
    }
    qDebug() << "CalibPlugin could not be loaded from any known path.";
}



