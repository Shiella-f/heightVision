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

}