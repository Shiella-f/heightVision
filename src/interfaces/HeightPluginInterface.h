#pragma once
#include <QtPlugin>
#include <QWidget>
#include <QString>
#include <opencv2/opencv.hpp>

class HeightPluginInterface {
public:
    virtual ~HeightPluginInterface() = default;
    virtual QString name() const = 0;
    virtual QWidget* createWidget(QWidget* parent = nullptr) = 0;

    // 新增接口
    virtual void loadFile() = 0;  //加载文件夹
    virtual void setPreference() = 0;//设置参考图片
    virtual void selectImage() = 0; //选择待测图片
    virtual void startTest() = 0; //开始测高
    virtual void saveCalibrationData() = 0; //保存高度标定数据
    virtual void loadCalibrationData() = 0; //加载高度标定数据
    virtual double getMeasurementResult() const = 0;//获取测量结果
    virtual void setCameraImage(const cv::Mat& image) = 0;//设置相机图像
};

#define HeightPluginInterface_iid "com.yourcompany.HeightPluginInterface"
Q_DECLARE_INTERFACE(HeightPluginInterface, HeightPluginInterface_iid)