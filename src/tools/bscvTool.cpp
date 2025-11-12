#include "bscvTool.h"
#include <QPainter>
#include <QtMath>
#include <QMessageBox>
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace bscvTool
{
    using namespace cv;

    bool qImage2cvImg(const QImage& inImg, cv::Mat& outImg, int channels)
    {
        try
        {
            if(inImg.isNull())
        {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("输入图像为空！"));
            return false;
        }
        switch (channels)
        {
        case 1:
        {
            QImage temp = (inImg.format() != QImage::Format_Grayscale8) ? inImg.convertToFormat(QImage::Format_Grayscale8) : inImg;
            outImg = Mat(temp.height(), 
                         temp.width(), 
                         CV_8UC1)
                         .clone();
            uchar *pDest = outImg.data;
            for (int row = 0; row < temp.height(); row++) 
            {
                uchar *pSrc = temp.scanLine(row);
                memcpy(pDest, pSrc, outImg.cols);
                pDest += outImg.step;
            }
            break;
        }
        case 3:
        {
            QImage temp = (inImg.format() != QImage::Format_RGB888) ? inImg.convertToFormat(QImage::Format_RGB888) : inImg;
            Mat wrapped(temp.height(),
                        temp.width(),
                        CV_8UC3,
                        const_cast<uchar*>(temp.constBits()),
                        temp.bytesPerLine());
            cv::cvtColor(wrapped, outImg, cv::COLOR_RGB2BGR); // OpenCV 默认使用 BGR
            break;
        }
        default:
            break;
        }
        return true;
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(nullptr, QObject::tr("异常"), QObject::tr("转换QImage到cv::Mat时发生异常：%1").arg(e.what()));
            return false;
        }
    }

    bool cvImg2QImage(const cv::Mat& inImg, QImage& outImg)
    {
         switch (inImg.type())  // 根据Mat的类型选择对应转换函数
        {
        case CV_8UC1:         // 单通道灰度图
            return cvImg2qImg_Gray(inImg, outImg);
        case CV_8UC3:         // 3通道BGR图（OpenCV默认格式）
            return cvImg2qImg_RGB(inImg, outImg);
            break;
        case CV_8UC4:         // 4通道BGRA图
            return cvImg2qImg_RGBA(inImg, outImg);
        default:              // 不支持的类型
            break;
        }
        return false;
    }

    bool cvImg2qImg_Gray(const cv::Mat& inImg, QImage& outImg)
    {
        if (inImg.type() != CV_8UC1) {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("灰度图转换失败：图像类型为 %1").arg(inImg.type()));
            return false;
        }

        outImg = QImage(inImg.cols, inImg.rows, QImage::Format_Grayscale8);
        uchar *pSrc = inImg.data;
        for (int row = 0; row < inImg.rows; row++, pSrc += inImg.step)
        {
            uchar *pDest = outImg.scanLine(row);
            memcpy(pDest, pSrc, inImg.cols);
        }
        return !outImg.isNull();
    }

    bool cvImg2qImg_RGB(const cv::Mat& inImg, QImage& outImg)
    {
        if (inImg.type() != CV_8UC3) {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("RGB 图转换失败：图像类型为 %1").arg(inImg.type()));
            return false;
        }

        cv::Mat rgb;
        cv::cvtColor(inImg, rgb, cv::COLOR_BGR2RGB);
        QImage temp(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        outImg = temp.copy();
        return !outImg.isNull();
    }

    bool cvImg2qImg_RGBA(const cv::Mat& inImg, QImage& outImg)
    {
        if (inImg.type() != CV_8UC4) {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("RGBA 图转换失败：图像类型为 %1").arg(inImg.type()));
            return false;
        }

        cv::Mat rgba;
        cv::cvtColor(inImg, rgba, cv::COLOR_BGRA2RGBA);
        QImage temp(rgba.data, rgba.cols, rgba.rows, rgba.step, QImage::Format_RGBA8888);
        outImg = temp.copy();
        return !outImg.isNull();
    }

    bool qRegion2cvRegion(const QSize &p_regionsize, const QPainterPath& p_path, cv::Mat& outRegion)
    {
        if(p_regionsize.isEmpty())
        {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("无有效区域,无法转换！"));
            return false;
        }
        // 创建与指定大小相同的单通道掩码图像，初始化为全黑
        QImage qimage = QImage(p_regionsize, QImage::Format_Grayscale8);
        qimage.fill(Qt::black);
        QPainter painter(&qimage);
        painter.setPen(Qt::white);
        painter.setBrush(Qt::white);
        painter.translate(qimage.width() / 2.0, qimage.height() / 2.0); // 将坐标系原点移动到图像中心
        painter.drawPath(p_path); // 使用QPainterPath绘制区域
        painter.end();
        if(!qImage2cvImg(qimage, outRegion, 1))
        {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("转换区域图像时出错！"));
            return false;
        }
        return true;
    }

    bool qRegion2cvMinRegion(const QSize &p_regionsize, const QPainterPath& p_path, cv::Mat& outRegion, cv::Rect& outRect)
    {
        auto qRect = p_path.boundingRect();
        auto regionRect = QRectF(QPoint(0,0), p_regionsize);
        qRect = qRect.translated(regionRect.center());
        auto intersect = regionRect.intersected(qRect);
        if(intersect.isEmpty())
        {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("无有效区域,无法转换！"));
            return false;
        }

        int x = int(intersect.left());
        int y = int(intersect.top());
        double offsetx = intersect.left() - x;
        double offsety = intersect.top() - y;
        int w = int(qCeil(intersect.width() + offsetx));
        int h = int(qCeil(intersect.height() + offsety));

        QImage qimage = QImage(QSize(w, h), QImage::Format_Grayscale8);
        qimage.fill(Qt::black);
        QPainter painter(&qimage);
        painter.setPen(Qt::white);
        painter.setBrush(Qt::white);
        painter.translate(regionRect.center() - intersect.topLeft()); // 将坐标系原点移动到裁剪区域左上角
        painter.drawPath(p_path); // 使用QPainterPath绘制区域
        painter.end();

        if(!qImage2cvImg(qimage, outRegion, 1))
        {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("转换区域图像时出错！"));
            return false;
        }
        outRect = Rect(x, y, w, h);
        return true;
    }
} // namespace bscvTool