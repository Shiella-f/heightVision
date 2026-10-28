#include "Calib.h"
#include "../../BoardDetection.h"
#include "../../BoardConfiguration.h"
#include <QFile>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QSet>
#include <QDebug>
#include <cmath>
#include <limits>

namespace {
double run(const std::vector<std::string>& paths, const calibration::BoardSpec& spec,
           cv::Mat& K, cv::Mat& D, std::vector<cv::Mat>& rvecs, std::vector<cv::Mat>& tvecs,
           std::vector<std::vector<cv::Point2f>>& imagePoints,
           std::vector<std::vector<cv::Point3f>>& objectPoints, cv::Size& outputSize,const Calib::LogCallback& log)
{
    outputSize={}; imagePoints.clear(); objectPoints.clear();
    if(!spec.valid())return -1;
    cv::Size size;
    QSet<QByteArray> seen;
    try {
        for(const auto& path:paths) {
            QElapsedTimer reading;reading.start();
            QFile file(QString::fromUtf8(path.c_str()));
            if(!file.open(QIODevice::ReadOnly)){qWarning()<<"[Camera] unreadable:"<<file.fileName();if(log)log("无法读取："+file.fileName());continue;}
            auto bytes=file.readAll();
            auto hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
            if(seen.contains(hash)){qInfo()<<"[Camera] duplicate skipped:"<<file.fileName();if(log)log("跳过内容重复的图片："+file.fileName());continue;}
            seen.insert(hash);
            if(bytes.isEmpty())continue;
            cv::Mat encoded(1,int(bytes.size()),CV_8UC1,bytes.data());
            auto image=cv::imdecode(encoded,cv::IMREAD_COLOR);
            if(image.empty())continue;
            qInfo()<<"[Camera] read/decode ms:"<<reading.nsecsElapsed()/1e6<<file.fileName();
            if(log)log(QString("读取 %1：%2 ms").arg(file.fileName()).arg(reading.nsecsElapsed()/1e6,0,'f',1));
            if(size.area()!=0 && image.size()!=size){imagePoints.clear();objectPoints.clear();return -1;}
            size=image.size();
            calibration::BoardObservation observation;
            bool found=calibration::detectBoard(image,spec,observation);
            for(const auto& a:observation.attempts)
                qInfo()<<"[Board]"<<a.method.c_str()<<a.workSize.width<<a.workSize.height<<"found:"<<a.found<<"ms:"<<a.milliseconds;
            qInfo()<<"[Camera] detection:"<<observation.method.c_str()<<"points:"<<observation.pixels.size()
                   <<"total ms:"<<observation.milliseconds<<"refine ms:"<<observation.refinementMs
                   <<"error:"<<observation.error.c_str();
            if(log)log(QString("检测 %1：%2，%3 点，%4 次搜索，%5 ms（精化 %6 ms）%7")
                .arg(file.fileName(),QString::fromStdString(observation.method)).arg(observation.pixels.size())
                .arg(observation.attempts.size()).arg(observation.milliseconds,0,'f',1).arg(observation.refinementMs,0,'f',1)
                .arg(found?QString():"；"+QString::fromStdString(observation.error)));
            if(!found || observation.pixels.size()<5)continue;
            // Preserve the historical camera extrinsic origin at the first inner corner.
            if(spec.type==calibration::BoardType::Chessboard)
                for(auto& p:observation.objectPoints){p.x-=float(spec.squareMm);p.y-=float(spec.squareMm);}
            imagePoints.push_back(std::move(observation.pixels));
            objectPoints.push_back(std::move(observation.objectPoints));
        }
        if(imagePoints.empty())return -1;
        QElapsedTimer solving;solving.start();
        double rms=std::numeric_limits<double>::infinity();
        for(double multiplier:{.75,1.5,3.0}){
            double focal=std::max(size.width,size.height)*multiplier;
            cv::Mat candidateK=(cv::Mat_<double>(3,3)<<focal,0,(size.width-1)*.5,0,focal,(size.height-1)*.5,0,0,1),candidateD;
            std::vector<cv::Mat> candidateR,candidateT;
            double value=cv::calibrateCamera(objectPoints,imagePoints,size,candidateK,candidateD,candidateR,candidateT,
                cv::CALIB_USE_INTRINSIC_GUESS,cv::TermCriteria(cv::TermCriteria::COUNT|cv::TermCriteria::EPS,200,1e-10));
            if(std::isfinite(value)&&value<rms&&cv::checkRange(candidateK)&&cv::checkRange(candidateD)
                &&candidateK.at<double>(0,0)>0&&candidateK.at<double>(1,1)>0){
                rms=value;K=candidateK;D=candidateD;rvecs=std::move(candidateR);tvecs=std::move(candidateT);
            }
        }
        qInfo()<<"[Camera] solve ms:"<<solving.nsecsElapsed()/1e6<<"unique views:"<<imagePoints.size()<<"RMS:"<<rms;
        if(log)log(QString("内参求解：%1 张有效独立图片，%2 ms，RMS=%3").arg(imagePoints.size()).arg(solving.nsecsElapsed()/1e6,0,'f',1).arg(rms));
        if(!std::isfinite(rms)||rms<0||rms>2||!cv::checkRange(K)||!cv::checkRange(D)){
            if(log)log("相机内参求解未通过质量检查：无有限有效解或 RMS 超过 2 px。");return -1;
        }
        if(log)log("相机数值解已求得；仅平面运动的数据不能证明完整内参可靠。固定高度测量请使用固定平面模式，并进行独立实物验收。");
        outputSize=size;
        return rms;
    }catch(const cv::Exception& error){qWarning()<<"[Camera]"<<error.what();imagePoints.clear();objectPoints.clear();return -1;}
}
}
Calib::Calib()=default;
Calib::~Calib()=default;
double Calib::runConfigured(const std::vector<std::string>& paths,const QJsonObject& config,cv::Mat& K,cv::Mat& D,
    std::vector<cv::Mat>& rvecs,std::vector<cv::Mat>& tvecs,std::vector<std::vector<cv::Point2f>>& images,
    std::vector<std::vector<cv::Point3f>>& objects){
    calibration::BoardSpec spec;QString error;
    if(!calibration::parseBoardConfiguration(config,spec,error)){m_imageSize={};images.clear();objects.clear();if(m_log)m_log(error);return -1;}
    return run(paths,spec,K,D,rvecs,tvecs,images,objects,m_imageSize,m_log);
}
double Calib::runCalibration(const std::vector<std::string>& paths,const cv::Size boardSize,const float squareSize,
    cv::Mat& K,cv::Mat& D,std::vector<cv::Mat>& rvecs,std::vector<cv::Mat>& tvecs,
    std::vector<std::vector<cv::Point2f>>& images,std::vector<std::vector<cv::Point3f>>& objects)
{
    calibration::BoardSpec spec;
    if(boardSize.width<3||boardSize.height<3||boardSize.width>10000||boardSize.height>10000){m_imageSize={};images.clear();objects.clear();return -1;}
    spec.squares={boardSize.width+1,boardSize.height+1};spec.squareMm=squareSize;
    return run(paths,spec,K,D,rvecs,tvecs,images,objects,m_imageSize,m_log);
}
double Calib::runCalibrationChArUco(const std::vector<std::string>& paths,const cv::Size squares,
    const float squareSize,const float markerSize,const int dictionaryId,cv::Mat& K,cv::Mat& D,
    std::vector<cv::Mat>& rvecs,std::vector<cv::Mat>& tvecs,
    std::vector<std::vector<cv::Point2f>>& images,std::vector<std::vector<cv::Point3f>>& objects)
{
    calibration::BoardSpec spec;
    spec.type=calibration::BoardType::Charuco;spec.squares=squares;spec.squareMm=squareSize;
    spec.markerMm=markerSize;spec.dictionaryId=dictionaryId<0?cv::aruco::DICT_4X4_50:dictionaryId;
    return run(paths,spec,K,D,rvecs,tvecs,images,objects,m_imageSize,m_log);
}
