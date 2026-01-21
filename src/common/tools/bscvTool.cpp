#include "bscvTool.h"
#include <QDebug>
#include <QPainter>
#include <QPoint>
#include <QSet>
#include <QtMath>
#include <QObject>
#include <QtGlobal>

inline uint qHash(const QPoint &pt, uint seed = 0) noexcept
{
    const quint64 packed = (static_cast<quint64>(static_cast<quint32>(pt.x())) << 32) | static_cast<quint32>(pt.y());
    return ::qHash(packed, seed);
}
namespace TIGER_BSVISION
{
    using namespace cv;
    using namespace std;
    // 3通道图
    bool qImage2cvImage(const QImage &p_qimg, Mat &p_mat, int channel)
    {
        try
        {
            if (p_qimg.isNull())
            {
                qWarning() << QObject::tr("图像为空无法转换");
                return false;
            }
            switch (channel)
            {
            case 1:
            {
                QImage temp = (p_qimg.format() != QImage::Format_Grayscale8 ? p_qimg.convertToFormat(QImage::Format_Grayscale8) : p_qimg);
                p_mat = Mat(temp.height(),
                            temp.width(),
                            CV_8UC1)
                            .clone();
                uchar *pDest = p_mat.data;
                for (int row = 0; row < p_mat.rows; row++, pDest += p_mat.step)
                {
                    uchar *pSrc = temp.scanLine(row);
                    memcpy(pDest, pSrc, p_mat.cols);
                }
            }
            break;
            case 3:
            default:
            {
                QImage temp = p_qimg.format() != QImage::Format_RGB888 ? p_qimg.convertToFormat(QImage::Format_RGB888) : p_qimg;
                p_mat = Mat(temp.height(),
                            temp.width(),
                            CV_8UC3,
                            (uchar *)temp.bits(),
                            temp.bytesPerLine())
                            .clone();
            }
            break;
            }
            return true;
        }
        catch (cv::Exception &e)
        {
            qWarning().noquote() << QStringLiteral("qImageToCVImage error:") << e.what();
        }
        return false;
    }

    bool cvImage2qImage(QImage &p_qimg, const cv::Mat &p_mat)
    {
        switch (p_mat.type())
        {
        case CV_8UC1:
            return cvImage2qImageGray(p_qimg, p_mat);
        case CV_8UC3:
            return cvImage2qImageRGB(p_qimg, p_mat);
        case CV_8UC4:
            return cvImage2qImageRGBA(p_qimg, p_mat);
        default:
            break;
        }
        return false;
    }

    bool cvImage2qImageGray(QImage &p_qimg, const cv::Mat &p_matGray)
    {
        assert(p_matGray.type() == CV_8UC1);
        p_qimg = QImage(p_matGray.cols, p_matGray.rows, QImage::Format_Grayscale8);
        uchar *pSrc = p_matGray.data;
        for (int row = 0; row < p_matGray.rows; row++, pSrc += p_matGray.step)
        {
            uchar *pDest = p_qimg.scanLine(row);
            memcpy(pDest, pSrc, p_matGray.cols);
        }
        return !p_qimg.isNull();
    }

    bool cvImage2qImageRGB(QImage &p_qimg, const cv::Mat &p_matBGR)
    {
        assert(p_matBGR.type() == CV_8UC3);
        const uchar *pSrc = (const uchar *)p_matBGR.data;
        p_qimg = QImage(pSrc, p_matBGR.cols, p_matBGR.rows, p_matBGR.step, QImage::Format_RGB888).rgbSwapped();
        return !p_qimg.isNull();
    }

    bool cvImage2qImageRGBA(QImage &p_qimg, const cv::Mat &p_matBGRA)
    {
        assert(p_matBGRA.type() == CV_8UC4);
        const uchar *pSrc = (const uchar *)p_matBGRA.data;
        // Format_ARGB32 实际是bgra顺序
        p_qimg = QImage(pSrc, p_matBGRA.cols, p_matBGRA.rows, p_matBGRA.step, QImage::Format_ARGB32);
        p_qimg = p_qimg.convertToFormat(QImage::Format_RGBA8888);
        return !p_qimg.isNull();
    }

    bool qRegion2cvRegion(const QSize &p_regionSize, const QPainterPath &p_path, cv::Mat &p_hRegion)
    {
        if (p_regionSize.isEmpty())
        {
            qWarning() << QObject::tr("无有效区域,无法转换");
            return false;
        }
        QImage qimage = QImage(p_regionSize.width(), p_regionSize.height(), QImage::Format_Grayscale8);
        qimage.fill(Qt::black);
        QPainter painter(&qimage);
        painter.setPen(Qt::white);
        painter.setBrush(Qt::white);
        painter.translate(qimage.width() * 0.5, qimage.height() * 0.5);
        painter.drawPath(p_path);
        painter.end();
        if (!qImage2cvImage(qimage, p_hRegion, 1))
        {
            qWarning() << QObject::tr("图像转换失败，无法识别处理");
            return false;
        }
        return true;
    }

    bool qRegion2MinRegion(const QSize &p_regionSize, const QPainterPath &p_path, cv::Mat &p_hRegion, cv::Rect &p_boundingRect)
    {
        auto qRect = p_path.boundingRect();
        auto regionRect = QRectF(QPoint(0, 0), p_regionSize);
        qRect = qRect.translated(regionRect.center());
        auto intersect = regionRect.intersected(qRect);
        if (!intersect.isValid())
        {
            qWarning() << QObject::tr("无有效区域,无法转换");
            return false;
        }

        int x = int(intersect.left());
        int y = int(intersect.top());
        double offsetX = intersect.left() - x;
        double offsetY = intersect.top() - y;
        int w = qCeil(intersect.width() + offsetX);
        int h = qCeil(intersect.height() + offsetY);
        QImage qimage = QImage(w, h, QImage::Format_Grayscale8);
        qimage.fill(Qt::black);
        QPainter painter(&qimage);
        painter.setPen(Qt::white);
        painter.setBrush(Qt::white);
        painter.translate(regionRect.center() - intersect.topLeft());
        painter.drawPath(p_path);
        painter.end();
        if (!qImage2cvImage(qimage, p_hRegion, 1))
        {
            qWarning() << QObject::tr("图像转换失败，无法识别处理");
            return false;
        }
        p_boundingRect = Rect(x, y, w, h);
        return true;
    }

    cv::RotatedRect getSmallestRectangle(const cv::Mat &p_matRegion)
    {
        cv::RotatedRect minRect;
        try
        {
            vector<Point> contourslist;
            // 联合所有轮廓
            {
                vector<Vec4i> hierarchy;
                vector<vector<Point>> contours;
                findContours(p_matRegion, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
                for (int i = 0; i < contours.size(); i++)
                {
                    vector<Point> singContours;
                    singContours = contours[i];
                    contourslist.insert(contourslist.end(), singContours.begin(), singContours.end());
                }
            }
            if (!contourslist.empty())
            {
                minRect = minAreaRect(contourslist);
            }
        }
        catch (const std::exception &e)
        {
            qWarning().noquote() << QStringLiteral("getSmallestRectangle error:") << e.what();
        }
        return minRect;
    }

    cv::Mat getSmallestRectangleRegion(const cv::Mat &p_matRegion, bool p_fill)
    {
        try
        {
            Mat region = Mat::zeros(p_matRegion.rows, p_matRegion.cols, CV_8UC1);
            RotatedRect minRect = getSmallestRectangle(p_matRegion);
            vector<Point2f> boxPts(4);
            minRect.points(boxPts.data());

            if (p_fill)
            {
                vector<vector<Point2f>> poly;
                poly.emplace_back(boxPts);
                fillPoly(region, poly, Scalar(255));
            }
            else
            {
                vector<Point> poly;
                poly.reserve(4);
                for (const auto &p : boxPts)
                {
                    poly.emplace_back(Point(p.x, p.y));
                }
                polylines(region, poly, true, Scalar(255));
            }
            return region;
        }
        catch (cv::Exception &e)
        {
            qWarning().noquote() << QStringLiteral("getSmallestRectangleRegion error:") << e.what();
        }
        return Mat();
    }

    cv::Mat getSmallestEllipseRegion(const cv::Mat &p_matRegion, bool p_fill)
    {
        try
        {
            Mat region = Mat::zeros(p_matRegion.rows, p_matRegion.cols, CV_8UC1);
            RotatedRect minRect = getSmallestRectangle(p_matRegion);
            const float cMinR = 5;
            auto &width = minRect.size.width;
            auto &height = minRect.size.height;
            width = qMax(width, cMinR) + cMinR;
            height = qMax(height, cMinR) + cMinR;
            ellipse(region, minRect, Scalar(255), p_fill ? -1 : 1);
            return region;
        }
        catch (cv::Exception &e)
        {
            qWarning().noquote() << QStringLiteral("getSmallestEllipseRegion error:") << e.what();
        }
        return Mat();
    }

    Mat getCrossContours(const QSize &p_size, const double &p_rows, const double &p_cols)
    {
        try
        {
            // 十字叉
            Mat cross = Mat::zeros(p_size.height(), p_size.width(), CV_8UC1);
            int halfLong = 5;
            line(cross, Point(p_cols - halfLong, p_rows), Point(p_cols + halfLong, p_rows), Scalar(0xff));
            line(cross, Point(p_cols, p_rows - halfLong), Point(p_cols, p_rows + halfLong), Scalar(0xff));
            cross.at<uchar>(p_rows, p_cols) = 0xff;
            // 旋转
            Mat rotateMatrix;
            const double cAngle = 60;
            Size dstSize(cross.cols, cross.rows);
            Point2f center(p_cols, p_rows);
            rotateMatrix = getRotationMatrix2D(center, cAngle, 1);
            warpAffine(cross, cross, rotateMatrix, dstSize);
            return cross;
        }
        catch (cv::Exception &e)
        {
            qWarning().noquote() << QStringLiteral("getCrossContours error:") << e.what();
        }
        return Mat();
    }

    // Bresenham圆算法
    QVector<QPointF> getCirclePts(int p_xc, int p_yc, int radius)
    {
        QSet<QPoint> pts;
        int x = 0;
        int y = radius;
        int d = 3 - 2 * radius; // 初始决策变量
        while (x <= y)
        {
            // 利用对称性添加 8 个点
            pts << QPoint(p_xc + x, p_yc + y);
            pts << QPoint(p_xc - x, p_yc + y);
            pts << QPoint(p_xc + x, p_yc - y);
            pts << QPoint(p_xc - x, p_yc - y);
            pts << QPoint(p_xc + y, p_yc + x);
            pts << QPoint(p_xc - y, p_yc + x);
            pts << QPoint(p_xc + y, p_yc - x);
            pts << QPoint(p_xc - y, p_yc - x);

            // 更新决策变量
            if (d < 0)
            {
                d = d + 4 * x + 6;
            }
            else
            {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
        QVector<QPointF> outPts;
        for (auto pt : pts)
        {
            outPts << pt;
        }
        return outPts;
    }
}