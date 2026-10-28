#include "Warpective.h"
#include <QDebug>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <map>
#include <QSaveFile>
#include "../../BoardConfiguration.h"

namespace {
double medianValue(std::vector<double> values) {
    if(values.empty())return 0;
    auto mid=values.begin()+values.size()/2;
    std::nth_element(values.begin(),mid,values.end());return *mid;
}
calibration::ResidualStatistics statistics(std::vector<double> values) {
    calibration::ResidualStatistics result;
    if(values.empty()) return result;
    for(double value:values) result.rms+=value*value;
    result.rms=std::sqrt(result.rms/values.size());
    std::sort(values.begin(),values.end());
    result.p95=values[size_t(std::ceil(values.size()*0.95))-1];result.maximum=values.back();
    return result;
}
calibration::BoardSpec boardSpec(const CameraWarptivePort::Config& cfg) {
    calibration::BoardSpec spec;
    if(!cfg.boardConfiguration.isEmpty()){
        QString error;if(!calibration::parseBoardConfiguration(cfg.boardConfiguration,spec,error))throw std::invalid_argument(error.toStdString());
        return spec;
    }
    spec.type=cfg.boardType==CameraWarptivePort::BoardType::Charuco?calibration::BoardType::Charuco:calibration::BoardType::Chessboard;
    spec.squares={cfg.gridCols,cfg.gridRows};spec.squareMm=cfg.squareMm;spec.markerMm=cfg.markerMm;spec.dictionaryId=cfg.dictionaryId;
    return spec;
}
}
CameraCorrector::CameraCorrector(const Config& cfg):cfg_(cfg) {
    if((cfg.inputStage!=CameraWarptivePort::InputStage::Raw && cfg.inputStage!=CameraWarptivePort::InputStage::Undistorted)
        || !std::isfinite(cfg.fitToleranceCells) || cfg.fitToleranceCells<0.001 || cfg.fitToleranceCells>0.10)
        throw std::invalid_argument("invalid input stage or fit tolerance (0.001..0.10 cells)");
    if((cfg.boardType!=CameraWarptivePort::BoardType::Chessboard && cfg.boardType!=CameraWarptivePort::BoardType::Charuco)
        || !boardSpec(cfg).valid())throw std::invalid_argument("invalid board parameters");
    if(!std::isfinite(cfg.outWidthMm)||!std::isfinite(cfg.outHeightMm)
       ||cfg.outWidthMm<=0||cfg.outHeightMm<=0
       ||(!cfg.useOutputRegion&&(cfg.outWidthMm<cfg.gridCols*cfg.squareMm||cfg.outHeightMm<cfg.gridRows*cfg.squareMm)))
        throw std::invalid_argument("output smaller than board dimensions");
}
double CameraCorrector::estimateAdaptivePixPerMm(const calibration::BoardObservation& obs,const cv::Size& size) const {
    std::map<int,size_t> byId;
    for(size_t i=0;i<obs.ids.size();++i)byId.emplace(obs.ids[i],i);
    std::vector<double> horizontal,vertical;
    for(const auto& entry:byId) {
        int id=entry.first;size_t i=entry.second;
        for(int axis=0;axis<2;++axis) {
            if(axis==0 && id%cfg_.innerCols()==cfg_.innerCols()-1)continue;
            auto it=byId.find(id+(axis==0?1:cfg_.innerCols()));
            if(it==byId.end())continue;
            auto j=it->second;
            double mm=cv::norm(obs.objectPoints[i]-obs.objectPoints[j]);
            if(mm>0)(axis==0?horizontal:vertical).push_back(cv::norm(obs.pixels[i]-obs.pixels[j])/mm);
        }
    }
    double x=medianValue(horizontal),y=medianValue(vertical);
    double scale=x>0&&y>0?std::min(x,y):std::max(x,y);
    return std::min(scale,std::min(size.width/cfg_.outWidthMm,size.height/cfg_.outHeightMm));
}
bool CameraCorrector::computeMap(const cv::Mat& undistImg) {
    hasMap_=false;H_.release();
    calibration::BoardObservation observation;
    return calibration::detectBoard(undistImg,boardSpec(cfg_),observation)
        &&computeMap(observation,undistImg.size());
}
bool CameraCorrector::computeMap(const calibration::BoardObservation& obs,cv::Size size) {
    hasMap_=false;H_.release();error_.clear();report_={};QElapsedTimer timer;timer.start();
    if(obs.pixels.size()<4 || obs.pixels.size()!=obs.objectPoints.size() || obs.ids.size()!=obs.pixels.size()) {
        error_="对应点数量不足或像素/物理点不匹配";return false;
    }
    if(!cfg_.useOutputRegion)cfg_.pixPerMm=estimateAdaptivePixPerMm(obs,size);
    if(!std::isfinite(cfg_.pixPerMm)||cfg_.pixPerMm<=0){error_="无法估计有效的像素/毫米比例";return false;}
    std::vector<cv::Point2d> source(obs.pixels.begin(),obs.pixels.end()),target;
    for(auto p:obs.objectPoints)target.emplace_back(p.x/cfg_.squareMm,p.y/cfg_.squareMm);
    cv::Mat mask;
    const cv::Mat cellsH=cv::findHomography(source,target,cv::RANSAC,cfg_.fitToleranceCells,mask,2000,0.9999);
    if(cellsH.empty()||!cv::checkRange(cellsH)){error_="无法计算有效的透视矩阵";return false;}
    std::vector<cv::Point2d> projected;cv::perspectiveTransform(source,projected,cellsH);
    std::vector<double> allErrors,inlierErrors;
    for(size_t i=0;i<projected.size();++i){
        const double denominator=cellsH.at<double>(2,0)*source[i].x+cellsH.at<double>(2,1)*source[i].y+cellsH.at<double>(2,2);
        double error=cv::norm(projected[i]-target[i]);
        if(!std::isfinite(error)||!std::isfinite(denominator)||std::abs(denominator)<1e-12){error_="透视点转换无效";return false;}
        allErrors.push_back(error);if(error<=cfg_.fitToleranceCells)inlierErrors.push_back(error);
    }
    report_.inputStage=cfg_.inputStage==CameraWarptivePort::InputStage::Raw?"raw":"undistorted";
    report_.pointCount=int(source.size());report_.inlierCount=int(inlierErrors.size());
    report_.toleranceCells=cfg_.fitToleranceCells;report_.squareMm=cfg_.squareMm;report_.pixPerMm=cfg_.pixPerMm;
    report_.all=statistics(allErrors);report_.inliers=statistics(inlierErrors);
    if(inlierErrors.size()<4 || double(inlierErrors.size())/source.size()<0.8){
        error_="透视拟合质量不足：最终内点 "+std::to_string(inlierErrors.size())+"/"+std::to_string(source.size())
            +"，要求至少 80%，容差 "+std::to_string(cfg_.fitToleranceCells)+" 格";return false;
    }
    const cv::Mat output=(cv::Mat_<double>(3,3)<<cfg_.squareMm*cfg_.pixPerMm,0,cfg_.marginXmm()*cfg_.pixPerMm,
        0,cfg_.squareMm*cfg_.pixPerMm,cfg_.marginYmm()*cfg_.pixPerMm,0,0,1);
    H_=output*cellsH;
    boardH_=(cv::Mat_<double>(3,3)<<cfg_.squareMm,0,0,0,cfg_.squareMm,0,0,0,1)*cellsH;
    cv::Mat inverse;
    if(!cv::checkRange(H_) || cv::invert(H_/cv::norm(H_,cv::NORM_INF),inverse,cv::DECOMP_SVD)<=1e-12){
        H_.release();error_="透视矩阵不可逆";return false;
    }
    hasMap_=true;
    qInfo()<<"[Perspective] fit ms:"<<timer.nsecsElapsed()/1e6<<report_.description();
    return true;
}
bool CameraCorrector::warp(const cv::Mat& undistImg, cv::Mat& dst) const
{
    if (!hasMap_)
    {
        qDebug() << "[CameraCorrector] 请先调用 computeMap()。\n";
        return false;
    }
    if (undistImg.empty())
        return false;

    cv::warpPerspective(
        undistImg, dst,
        H_,
        cfg_.outSize(),
        cv::INTER_LANCZOS4,
        cv::BORDER_CONSTANT,
        cv::Scalar::all(0));

    return true;
}

bool CameraCorrector::save(const std::string& path) const
{
    if(!hasMap_) return false;
    try {
    const bool xml=QString::fromUtf8(path.c_str()).endsWith(".xml",Qt::CaseInsensitive);
    cv::FileStorage fs("", cv::FileStorage::WRITE|cv::FileStorage::MEMORY
        |(xml?cv::FileStorage::FORMAT_XML:cv::FileStorage::FORMAT_YAML));
    fs << "gridCols"     << cfg_.gridCols;
    fs << "gridRows"     << cfg_.gridRows;
    fs << "squareMm"     << cfg_.squareMm;
    fs << "outWidthMm"   << cfg_.outWidthMm;
    fs << "outHeightMm"  << cfg_.outHeightMm;
    fs << "pixPerMm"     << cfg_.pixPerMm;
    fs << "boardType" << static_cast<int>(cfg_.boardType) << "markerMm" << cfg_.markerMm << "dictionaryId" << cfg_.dictionaryId;
    fs << "homography_input" << "undistorted_pixels" << "calibration_input_stage" << static_cast<int>(cfg_.inputStage)
       << "fitToleranceCells" << cfg_.fitToleranceCells;
    report_.write(fs);
    fs << "physicalOffsetXmm" << physicalOffsetMm_.x;
    fs << "physicalOffsetYmm" << physicalOffsetMm_.y;
    if (!H_.empty())
        fs << "homography" << H_;
    const auto contents=fs.releaseAndGetString();QSaveFile file(QString::fromUtf8(path.c_str()));
    return file.open(QIODevice::WriteOnly) && file.write(contents.data(),qint64(contents.size()))==qint64(contents.size()) && file.commit();
    } catch(const cv::Exception&){return false;}
}

bool CameraCorrector::load(const std::string& path)
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) return false;
    fs["gridCols"]    >> cfg_.gridCols;
    fs["gridRows"]    >> cfg_.gridRows;
    fs["squareMm"]    >> cfg_.squareMm;
    fs["outWidthMm"]  >> cfg_.outWidthMm;
    fs["outHeightMm"] >> cfg_.outHeightMm;
    fs["pixPerMm"]    >> cfg_.pixPerMm;
    cfg_.boardType = CameraWarptivePort::BoardType::Chessboard; cfg_.markerMm=0; cfg_.dictionaryId=0;
    if(!fs["boardType"].empty()){int type=0;fs["boardType"]>>type;cfg_.boardType=static_cast<CameraWarptivePort::BoardType>(type);}
    if(!fs["markerMm"].empty())fs["markerMm"]>>cfg_.markerMm;
    if(!fs["dictionaryId"].empty())fs["dictionaryId"]>>cfg_.dictionaryId;
    if (!fs["physicalOffsetXmm"].empty())
        fs["physicalOffsetXmm"] >> physicalOffsetMm_.x;
    else
        physicalOffsetMm_.x = 0.0;

    if (!fs["physicalOffsetYmm"].empty())
        fs["physicalOffsetYmm"] >> physicalOffsetMm_.y;
    else
        physicalOffsetMm_.y = 0.0;
    if (!fs["homography"].empty())
    {
        fs["homography"] >> H_;
        hasMap_ = !H_.empty();
    }
    fs.release();
    return hasMap_;
}

bool CameraCorrector::setCoordinateOriginByPair(cv::Point2d pixelPt,
                                                cv::Point2d physicalPtMm)
{
    if (cfg_.pixPerMm <= 0.0)
    {
        std::cerr << "[BoardHomography] pixPerMm 非法，无法设置二次初始化偏移。\n";
        return false;
    }

    const cv::Point2d baseMm(pixelPt.x / cfg_.pixPerMm,
                             pixelPt.y / cfg_.pixPerMm);
    physicalOffsetMm_ = physicalPtMm - baseMm;

    std::cout << "[BoardHomography] 二次初始化偏移(mm): ("
              << physicalOffsetMm_.x << ", "
              << physicalOffsetMm_.y << ")\n";
    return true;
}
