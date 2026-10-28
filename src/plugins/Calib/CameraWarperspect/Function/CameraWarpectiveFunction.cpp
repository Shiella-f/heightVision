#include "CameraWarpectiveFunction.h"
#include <QDebug>
#include "tools/bscvTool.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include "../../BoardConfiguration.h"
#include <cmath>

CameraWarpectiveFunction::CameraWarpectiveFunction(QWidget* parent)
    : QObject(parent), m_scene(new WarpectiveScene(this)), m_CameraCorrector(nullptr)
{
}

CameraWarpectiveFunction::~CameraWarpectiveFunction()
{
    delete m_CameraCorrector;
}

bool CameraWarpectiveFunction::LoadCameraCalibrationData(const QString& path)
{
    m_boardHomography.release();m_regionValid=false;m_report={};m_renderMeasurement={};m_detectionCalls=0;
    m_remapper.reset();
    isCalibrated = false;
    m_rectifiedImage.release();
    delete m_CameraCorrector;
    m_CameraCorrector = nullptr;
    return m_measurement.loadCamera(path);
}

bool CameraWarpectiveFunction::CalibWarptive(const Config& requested, const cv::Mat& rawImage)
{
    Config config=requested;
    m_boardHomography.release();m_regionValid=false;m_report={};m_supportHull.clear();m_renderMeasurement={};
    m_remapper.reset();
    isCalibrated = false;
    m_rectifiedImage.release();
    m_measurement.homography.release();
    m_measurement.pixPerMm = 0;
    delete m_CameraCorrector;
    m_CameraCorrector = nullptr;
    try {
        QElapsedTimer timer; timer.start();
        if (!m_measurement.hasCamera()) { emit calibrationLog("尚未加载有效相机参数，未执行角点检测。"); return false; }
        if(!config.boardConfiguration.isEmpty()){
            calibration::BoardSpec board;QString error;QJsonObject normalized;
            if(!calibration::parseBoardConfiguration(config.boardConfiguration,board,error,&normalized)){
                m_report["error"]=error;emit calibrationLog(error);return false;
            }
            config.boardConfiguration=normalized;config.gridCols=board.squares.width;config.gridRows=board.squares.height;
            config.squareMm=board.squareMm;config.markerMm=board.markerMm;config.dictionaryId=board.dictionaryId;
            config.boardType=board.type==calibration::BoardType::Charuco?BoardType::Charuco:BoardType::Chessboard;
        }
        m_config=config;
        m_CameraCorrector = new CameraCorrector(config); // Validate config before running detection.
        if (rawImage.size() != m_measurement.imageSize) {
            emit calibrationLog(QString("原图尺寸 %1×%2 与相机参数 %3×%4 不一致。")
                .arg(rawImage.cols).arg(rawImage.rows).arg(m_measurement.imageSize.width).arg(m_measurement.imageSize.height));
            return false;
        }
        calibration::BoardSpec spec;
        spec.type = config.boardType == BoardType::Charuco ? calibration::BoardType::Charuco : calibration::BoardType::Chessboard;
        if (config.boardType != BoardType::Charuco && config.boardType != BoardType::Chessboard) return false;
        spec.squares = {config.gridCols, config.gridRows}; spec.squareMm = config.squareMm;
        spec.markerMm = config.markerMm; spec.dictionaryId = config.dictionaryId;
        if(!config.boardConfiguration.isEmpty()){
            QString error;if(!calibration::parseBoardConfiguration(config.boardConfiguration,spec,error))return false;
        }
        calibration::BoardObservation observation;
        const bool detected = calibration::detectBoard(rawImage, spec, observation);
        m_report=calibration::observationReport(observation);m_report["board_configuration"]=config.boardConfiguration;
        m_report["detection_call_count"]=++m_detectionCalls;
        for (const auto& attempt : observation.attempts) {
            emit calibrationLog(QString("搜索 %1，工作图 %2×%3：%4，%5 ms")
                .arg(QString::fromStdString(attempt.method)).arg(attempt.workSize.width).arg(attempt.workSize.height)
                .arg(attempt.found ? "发现候选网格" : "未找到").arg(attempt.milliseconds,0,'f',1));
        }
        if (!detected) {
            emit calibrationLog("检测失败："+QString::fromStdString(observation.error));
            qWarning() << "[Perspective] detection failed:" << observation.error.c_str(); return false;
        }
        qInfo() << "[Perspective] detection:" << observation.method.c_str() << "points:" << observation.pixels.size()
                << "ms:" << observation.milliseconds;
        emit calibrationLog(QString("检测 %1：%2 点，%3 次搜索，%4 ms（精化 %5 ms）")
            .arg(QString::fromStdString(observation.method)).arg(observation.pixels.size()).arg(observation.attempts.size())
            .arg(observation.milliseconds,0,'f',1).arg(observation.refinementMs,0,'f',1));
        QElapsedTimer stage; stage.start();
        if(config.inputStage==InputStage::Raw) {
        cv::undistortPoints(observation.pixels, observation.pixels, m_measurement.cameraMatrix,
            m_measurement.distCoeffs, cv::noArray(), m_measurement.newCameraMatrix,
            cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,100,1e-13));
            emit calibrationLog("输入阶段：相机原图；按 K_new 去畸变角点一次。");
        } else {
            emit calibrationLog("输入阶段：已去畸变；跳过重复校正，使用所加载 K_new 的同分辨率坐标。");
        }
        const double pointMs=stage.nsecsElapsed()/1e6;
        stage.restart();
        if (!m_CameraCorrector->computeMap(observation, rawImage.size())) {
            if(m_CameraCorrector->fitReport().pointCount)emit calibrationLog(m_CameraCorrector->fitReport().description());
            emit calibrationLog("检测已成功；"+QString::fromStdString(m_CameraCorrector->lastError())); return false;
        }
        emit calibrationLog(m_CameraCorrector->fitReport().description());
        emit calibrationLog(QString("点坐标去畸变 %1 ms，透视拟合 %2 ms")
            .arg(pointMs,0,'f',1).arg(stage.nsecsElapsed()/1e6,0,'f',1));
        m_measurement.homography = m_CameraCorrector->homography();
        m_measurement.pixPerMm = m_CameraCorrector->config().pixPerMm;
        m_measurement.offsetMm = m_CameraCorrector->physicalOffsetMm();
        m_boardHomography=m_CameraCorrector->boardHomography();
        std::vector<cv::Point2f> support;for(auto p:observation.objectPoints)support.emplace_back(p.x,p.y);
        cv::convexHull(support,m_supportHull);
        m_report["fit_description"]=m_CameraCorrector->fitReport().description();
        m_report["physical_validation_status"]="unverified";
        isCalibrated = m_measurement.ready();
        qInfo() << "[Perspective] calibration total ms:" << timer.nsecsElapsed()/1e6;
        return isCalibrated;
    } catch (const std::exception& error) {
        m_boardHomography.release();m_report["error"]=QString::fromUtf8(error.what());
        emit calibrationLog("透视标定失败："+QString::fromUtf8(error.what()));
        qWarning() << "Perspective calibration:" << error.what();
        return false;
    }
}

bool CameraWarpectiveFunction::SaveMeasurementCalibration(const QString& path) const
{
    return isCalibrated && (!m_config.useOutputRegion||m_regionValid) && m_CameraCorrector && m_measurement.saveProfile(path,&m_CameraCorrector->fitReport());
}

bool CameraWarpectiveFunction::SaveResult(const QString savePath)
{
    if(m_config.useOutputRegion)return SaveMeasurementCalibration(savePath);
    return isCalibrated && m_CameraCorrector && m_CameraCorrector->save(savePath.toStdString());
}

bool CameraWarpectiveFunction::UseWarptiveImage(const cv::Mat& rawImage, cv::Mat& dstImage) const
{
    if (!isCalibrated || !m_CameraCorrector || (m_config.useOutputRegion&&!m_regionValid)) { dstImage.release(); return false; }
    const auto& render=m_config.useOutputRegion?m_renderMeasurement:m_measurement;
    const auto size=m_config.useOutputRegion?m_outputSize:m_CameraCorrector->config().outSize();
    try {
        QElapsedTimer timer; timer.start();
        if(m_CameraCorrector->config().inputStage==InputStage::Undistorted) {
            const cv::Mat source=rawImage;dstImage.release();
            if(source.empty()||source.dims!=2||source.size()!=m_measurement.imageSize||source.depth()!=CV_8U
                ||(source.channels()!=1&&source.channels()!=3&&source.channels()!=4))return false;
            cv::Mat output;cv::warpPerspective(source,output,render.homography,size,
                cv::INTER_LINEAR,cv::BORDER_CONSTANT,cv::Scalar::all(0));dstImage=std::move(output);
            emit calibrationLog(QString("已去畸变图仅应用 H：%1 ms").arg(timer.nsecsElapsed()/1e6,0,'f',1));
            return !dstImage.empty();
        }
        calibration::RemapTiming timing;
        const bool ok = m_remapper.render(rawImage, render, size, dstImage, timing);
        qInfo() << "[Perspective] full image preview ms:" << timer.nsecsElapsed()/1e6;
        emit calibrationLog(QString("单次映射：%1，建表 %2 ms，采样 %3 ms，总计 %4 ms%5")
            .arg(timing.reused ? "复用映射表" : "新建映射表").arg(timing.buildMs,0,'f',1)
            .arg(timing.sampleMs,0,'f',1).arg(timer.nsecsElapsed()/1e6,0,'f',1)
            .arg(ok ? QString() : "；"+QString::fromStdString(timing.error)));
        return ok;
    } catch (const cv::Exception& error) {
        dstImage.release();
        qWarning() << "Perspective image:" << error.what();
        return false;
    }
}

bool CameraWarpectiveFunction::MapInputToBoard(const QPointF& input,QPointF& result)const {
    if(m_boardHomography.empty()||!std::isfinite(input.x())||!std::isfinite(input.y())
       ||input.x()<0||input.y()<0||input.x()>m_measurement.imageSize.width-1||input.y()>m_measurement.imageSize.height-1)return false;
    try{
        std::vector<cv::Point2d> source{{input.x(),input.y()}},undistorted;
        if(m_config.inputStage==InputStage::Raw)cv::undistortPoints(source,undistorted,m_measurement.cameraMatrix,m_measurement.distCoeffs,
            cv::noArray(),m_measurement.newCameraMatrix,{cv::TermCriteria::COUNT|cv::TermCriteria::EPS,100,1e-13});
        else undistorted=source;
        const auto p=undistorted[0];const auto& h=m_boardHomography;
        double w=h.at<double>(2,0)*p.x+h.at<double>(2,1)*p.y+h.at<double>(2,2);
        if(!std::isfinite(w)||std::abs(w)<1e-12)return false;
        std::vector<cv::Point2d> world;cv::perspectiveTransform(undistorted,world,h);
        if(!std::isfinite(world[0].x)||!std::isfinite(world[0].y))return false;
        result={world[0].x,world[0].y};return true;
    }catch(const cv::Exception&){return false;}
}
bool CameraWarpectiveFunction::SetOutputRegion(const QPointF& center,double width,double height,double density){
    m_regionValid=false;m_remapper.reset();m_report["region_valid"]=false;m_report.remove("error");
    m_report.remove("roi_boundary_input_pixels");m_measurement.regionMetadata={};
    auto fail=[&](const QString& error){m_report["error"]=error;return false;};
    if(!m_config.useOutputRegion||m_boardHomography.empty())return fail("请先完成区域模式的棋盘透视求解。");
    if(!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(density)||!std::isfinite(center.x())||!std::isfinite(center.y())
       ||width<=0||height<=0||density<=0)return fail("范围、中心或渲染密度无效。");
    const double nx=std::max(1.,std::round(width*density)),ny=std::max(1.,std::round(height*density));
    if(!std::isfinite(nx)||!std::isfinite(ny)||nx>=32767||ny>=32767||nx*ny*12>512.*1024*1024)
        return fail("输出尺寸或内存超限，请降低渲染密度，保持物理范围不变。");
    try{
        const double ox=center.x()-width/2,oy=center.y()-height/2;
        std::vector<cv::Point2d> world;
        for(int y=0;y<=40;++y)for(int x=0;x<=40;++x){
            cv::Point2d p(ox+width*x/40,oy+height*y/40);
            world.push_back(p);world.push_back(p+cv::Point2d(.001,0));world.push_back(p+cv::Point2d(0,.001));
        }
        const cv::Mat inverse=m_boardHomography.inv();
        double sign=0;
        for(auto p:world){double w=inverse.at<double>(2,0)*p.x+inverse.at<double>(2,1)*p.y+inverse.at<double>(2,2);
            if(!std::isfinite(w)||std::abs(w)<1e-12||(sign&&sign*w<0))return fail("区域跨越不可逆映射。");sign=w;}
        std::vector<cv::Point2d> undistorted,input;
        cv::perspectiveTransform(world,undistorted,inverse);
        if(m_config.inputStage==InputStage::Raw){
            std::vector<cv::Point2d> normalized;cv::perspectiveTransform(undistorted,normalized,m_measurement.newCameraMatrix.inv());
            std::vector<cv::Point3d> rays;for(auto p:normalized)rays.emplace_back(p.x,p.y,1);
            cv::projectPoints(rays,cv::Vec3d(),cv::Vec3d(),m_measurement.cameraMatrix,m_measurement.distCoeffs,input);
        }else input=undistorted;
        bool extrapolated=false;double orientation=0;QJsonArray boundary;
        for(int y=0;y<=40;++y)for(int x=0;x<=40;++x){
            const size_t i=(y*41+x)*3;auto p=input[i],u=input[i+1]-p,v=input[i+2]-p;
            double cross=u.x*v.y-u.y*v.x;
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<0||p.y<0||p.x>m_measurement.imageSize.width-1||p.y>m_measurement.imageSize.height-1)
                return fail("区域超出输入图片，请调整中心或范围。");
            if(!std::isfinite(cross)||std::abs(cross)<1e-12||(orientation&&orientation*cross<=0))return fail("区域映射存在折叠或退化。");
            orientation=cross;
            if(cv::pointPolygonTest(m_supportHull,world[i],true)<-1e-6)extrapolated=true;
        }
        // Ordered edge points, in the coordinate system of the displayed input image.
        for(int edge=0;edge<4;++edge)for(int t=0;t<=40;++t){
            int x=edge==0?t:edge==1?40:edge==2?40-t:0,y=edge==0?0:edge==1?t:edge==2?40:40-t;
            auto p=input[(y*41+x)*3];boundary.append(QJsonArray{p.x,p.y});
        }
        const cv::Mat measurement=(cv::Mat_<double>(3,3)<<density,0,-density*ox,0,density,-density*oy,0,0,1);
        m_measurement.homography=measurement*m_boardHomography;m_measurement.pixPerMm=density;m_measurement.offsetMm={0,0};
        m_renderMeasurement=m_measurement;
        const double sx=nx/width,sy=ny/height;
        // MatExpr assignment can reuse a shallow-copied Mat's storage. Allocate separately.
        cv::Mat renderH=(cv::Mat_<double>(3,3)<<sx,0,-sx*ox-.5,0,sy,-sy*oy-.5,0,0,1)*m_boardHomography;
        m_renderMeasurement.homography=std::move(renderH);
        m_outputSize={int(nx),int(ny)};m_regionValid=true;
        m_report["roi_center_x_mm"]=center.x();m_report["roi_center_y_mm"]=center.y();
        m_report["roi_width_mm"]=width;m_report["roi_height_mm"]=height;m_report["requested_pixels_per_mm"]=density;
        m_report["output_width_px"]=int(nx);m_report["output_height_px"]=int(ny);
        m_report["actual_pix_per_mm_x"]=sx;m_report["actual_pix_per_mm_y"]=sy;
        m_report["roi_boundary_input_pixels"]=boundary;m_report["region_valid"]=true;
        m_report["coverage_extrapolated"]=extrapolated;m_report["physical_origin"]="roi_top_left_boundary";
        m_report["pixel_center_convention"]="half_pixel";m_report["physical_validation_status"]="unverified";
        QJsonArray h;for(int i=0;i<9;++i)h.append(m_renderMeasurement.homography.ptr<double>()[i]);
        m_report["render_homography"]=h;m_measurement.regionMetadata=m_report;
        return true;
    }catch(const cv::Exception& e){return fail(QString::fromUtf8(e.what()));}
}
QJsonObject CameraWarpectiveFunction::GetReport()const{
    auto r=m_report;r["base_model_ready"]=!m_boardHomography.empty();r["region_valid"]=m_regionValid;
    r["image_width"]=m_measurement.imageSize.width;r["image_height"]=m_measurement.imageSize.height;return r;
}
void CameraWarpectiveFunction::ChangeShow()
{
    const cv::Mat& image = isCurrentImageShow ? m_rectifiedImage : m_currentImage;
    if (image.empty()) return;
    QImage display;
    if (TIGER_BSVISION::cvImage2qImage(display, image)) {
        m_scene->setOriginalPixmap(QPixmap::fromImage(display));
        m_scene->resetImage();
        isCurrentImageShow = !isCurrentImageShow;
    }
}

CameraWarptivePort* newCameraWarptivePort()
{
    return new CameraWarpectiveFunction();
}

ICameraWarptiveSeene* CameraWarpectiveFunction::getScene() const
{
    return m_scene;
}
