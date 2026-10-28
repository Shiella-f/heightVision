#include "PlaneCalibrationPort.h"
#include "PlaneModel.h"
#include "BoardDetection.h"
#include "BoardConfiguration.h"
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QSaveFile>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/objdetect.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <mutex>

namespace {
using calibration::PlaneModel;
struct View {
    QString path,hash;
    calibration::BoardObservation observation;
};
struct Solution {
    PlaneModel model;
    double rms=1e100,score=1e100,validation=1e100;
    int inliers=0,count=0,order=0;
    bool freeCenter=false;
    cv::Mat parameters;
    QJsonArray views;
};
cv::Mat readImage(const QString& path,QByteArray* contents=nullptr) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};
    auto bytes=f.readAll();if(contents)*contents=bytes;
    if(bytes.isEmpty())return {};
    return cv::imdecode(cv::Mat(1,int(bytes.size()),CV_8U,bytes.data()),cv::IMREAD_COLOR);
}
QString hash(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
bool parseBoard(const QJsonObject& config,calibration::BoardSpec& spec,QString& error) {
    if(config.value("board_type").toString("charuco")!="charuco"){error="固定平面模式需要 ChArUco 板。";return false;}
    return calibration::parseBoardConfiguration(config,spec,error);
}

bool project(const cv::Mat& matrix,cv::Point2d point,cv::Point2d& result) {
    const double* a=matrix.ptr<double>();double z=a[6]*point.x+a[7]*point.y+1.;
    if(!std::isfinite(z)||std::abs(z)<1e-10)return false;
    result={(a[0]*point.x+a[1]*point.y+a[2])/z,(a[3]*point.x+a[4]*point.y+a[5])/z};
    return std::isfinite(result.x)&&std::isfinite(result.y);
}
cv::Mat homogeneous(const cv::Mat& p) {
    cv::Mat h=cv::Mat::eye(3,3,CV_64F);for(int i=0;i<8;++i)h.ptr<double>()[i]=p.at<double>(i);return h;
}
struct FitProblem : cv::LMSolver::Callback {
    const std::vector<View>& views;
    cv::Size size;
    double scale,worldScale;
    int order;bool freeCenter;
    FitProblem(const std::vector<View>& v,cv::Size s,double ws,int o,bool c)
        :views(v),size(s),scale(std::max(s.width,s.height)),worldScale(ws),order(o),freeCenter(c){}
    int poseStart() const {return 8+order+(freeCenter?2:0);}
    cv::Point2d center(const cv::Mat& p)const{
        cv::Point2d c((size.width-1)*.5,(size.height-1)*.5);
        if(freeCenter){c.x+=.25*size.width*std::tanh(p.at<double>(8+order));c.y+=.25*size.height*std::tanh(p.at<double>(9+order));}
        return c;
    }
    bool predictions(const cv::Mat& p,std::vector<cv::Point2d>& predictions)const {
        const cv::Mat h=homogeneous(p);const auto c=center(p);
        double k1=order?p.at<double>(8):0,k2=order>1?p.at<double>(9):0;
        predictions.clear();
        for(size_t view=0;view<views.size();++view){
            double theta=0,tx=0,ty=0;
            if(view){int at=poseStart()+3*int(view-1);theta=p.at<double>(at);tx=p.at<double>(at+1);ty=p.at<double>(at+2);}
            double a=std::cos(theta),b=std::sin(theta);
            for(auto object:views[view].observation.objectPoints){
                double x=object.x/worldScale,y=object.y/worldScale;cv::Point2d n;
                if(!project(h,{a*x-b*y+tx,b*x+a*y+ty},n))return false;
                double r=n.dot(n),factor=1+k1*r+k2*r*r;
                if(!std::isfinite(factor)||factor<=.001||r>16)return false;
                predictions.push_back(c+n*(scale*factor));
            }
        }return true;
    }
    bool residual(const cv::Mat& p,cv::Mat& result)const {
        std::vector<cv::Point2d> predicted;if(!predictions(p,predicted))return false;
        result.create(int(predicted.size()*2),1,CV_64F);size_t i=0;
        for(const auto& view:views)for(auto measured:view.observation.pixels){
            cv::Point2d r=predicted[i]-cv::Point2d(measured);double length=cv::norm(r);
            // Squared transformed residual is the Huber loss (delta = one original pixel).
            double w=length>1?std::sqrt(2*length-1)/length:1;
            result.at<double>(2*int(i))=r.x*w;result.at<double>(2*int(i)+1)=r.y*w;++i;
        }return true;
    }
    bool compute(cv::InputArray parameter,cv::OutputArray err,cv::OutputArray jacobian) const override {
        cv::Mat p=parameter.getMat(),base;
        if(!residual(p,base))return false;base.copyTo(err);
        if(jacobian.needed()){
            cv::Mat j(base.rows,p.rows,CV_64F);
            for(int col=0;col<p.rows;++col){
                cv::Mat plus=p.clone(),minus=p.clone(),a,b;double step=1e-6*(1+std::abs(p.at<double>(col)));
                plus.at<double>(col)+=step;minus.at<double>(col)-=step;
                bool pa=residual(plus,a),pb=residual(minus,b);
                if(pa&&pb){cv::Mat derivative=(a-b)/(2*step);derivative.copyTo(j.col(col));}
                else if(pa){cv::Mat derivative=(a-base)/step;derivative.copyTo(j.col(col));}
                else if(pb){cv::Mat derivative=(base-b)/step;derivative.copyTo(j.col(col));}
                else j.col(col).setTo(0);
            }
            j.copyTo(jacobian);
        }return true;
    }
    cv::Mat initial(double k1)const {
        cv::Mat p=cv::Mat::zeros(poseStart()+3*int(views.size()-1),1,CV_64F);
        const cv::Point2d c((size.width-1)*.5,(size.height-1)*.5);
        std::vector<cv::Mat> maps;
        for(const auto& view:views){
            std::vector<cv::Point2d> object,image;
            for(auto o:view.observation.objectPoints)object.emplace_back(o.x/worldScale,o.y/worldScale);
            for(auto q:view.observation.pixels)image.push_back((cv::Point2d(q)-c)/scale);
            auto h=cv::findHomography(object,image);if(h.empty())return {};
            h/=h.at<double>(2,2);maps.push_back(h);
        }
        for(int i=0;i<8;++i)p.at<double>(i)=maps[0].ptr<double>()[i];
        if(order)p.at<double>(8)=k1;
        for(size_t i=1;i<views.size();++i){
            cv::Mat t=maps[0].inv()*maps[i];t/=t.at<double>(2,2);
            int at=poseStart()+3*int(i-1);
            p.at<double>(at)=std::atan2(t.at<double>(1,0)-t.at<double>(0,1),t.at<double>(0,0)+t.at<double>(1,1));
            p.at<double>(at+1)=t.at<double>(0,2);p.at<double>(at+2)=t.at<double>(1,2);
        }return p;
    }
    Solution solve(double initialK1)const {
        Solution result;result.order=order;result.freeCenter=freeCenter;
        cv::Mat p=initial(initialK1);if(p.empty())return result;
        auto callback=cv::makePtr<FitProblem>(*this);
        try{cv::LMSolver::create(callback,200,1e-10)->run(p);}catch(const cv::Exception&){return result;}
        std::vector<cv::Point2d> predicted;if(!predictions(p,predicted)||!cv::checkRange(p))return result;
        size_t index=0;double sum=0;
        for(const auto& view:views){
            double viewSum=0,maximum=0;int inliers=0;QJsonArray points;std::vector<double> errors;size_t localIndex=0;
            for(auto q:view.observation.pixels){
                auto expected=predicted[index++];double e=cv::norm(expected-cv::Point2d(q));sum+=e*e;viewSum+=e*e;maximum=std::max(maximum,e);inliers+=e<=2;errors.push_back(e);
                points.append(QJsonObject{{"id",view.observation.ids[localIndex++]},{"x",q.x},{"y",q.y},
                    {"dx",expected.x-q.x},{"dy",expected.y-q.y},{"error_px",e},{"inlier",e<=2}});
            }
            std::sort(errors.begin(),errors.end());
            result.inliers+=inliers;result.count+=int(view.observation.pixels.size());
            result.views.append(QJsonObject{{"image",view.path},{"point_count",int(view.observation.pixels.size())},
                {"rms_px",std::sqrt(viewSum/view.observation.pixels.size())},{"max_px",maximum},
                {"p95_px",errors[size_t(std::ceil(errors.size()*.95))-1]},{"inlier_count",inliers},{"points",points},
                {"detection_ms",view.observation.milliseconds},{"refinement_ms",view.observation.refinementMs}});
        }
        result.rms=std::sqrt(sum/result.count);result.parameters=p;
        result.model.imageSize=size;result.model.normalizationScale=scale;result.model.distortionCenter=center(p);
        result.model.radial={order?p.at<double>(8):0,order>1?p.at<double>(9):0};
        result.model.worldHomography=(cv::Mat_<double>(3,3)<<worldScale,0,0,0,worldScale,0,0,0,1)*homogeneous(p).inv();
        result.model.worldHomography/=result.model.worldHomography.at<double>(2,2);
        size_t k=0;
        for(const auto& view:views)for(auto q:view.observation.pixels){
            if(cv::norm(predicted[k]-cv::Point2d(q))<=2)result.model.observations.push_back(q);++k;
        }
        cv::convexHull(result.model.observations,result.model.supportHull);
        if(!result.model.valid()){result.rms=1e100;return result;}
        for(auto raw:result.model.observations){cv::Point2d world,back;if(!result.model.rawToWorld(raw,world)
            ||!result.model.worldToRaw(world,back)||cv::norm(back-cv::Point2d(raw))>1e-5){result.rms=1e100;break;}}
        return result;
    }
};
bool rigidFit(const std::vector<cv::Point2d>& object,const std::vector<cv::Point2d>& world,
              const std::vector<int>& indices,double& angle,cv::Point2d& translation) {
    if(indices.size()<2)return false;cv::Point2d a,b;
    for(int i:indices){a+=object[i];b+=world[i];}a*=1./indices.size();b*=1./indices.size();
    double dot=0,cross=0;for(int i:indices){auto p=object[i]-a,q=world[i]-b;dot+=p.dot(q);cross+=p.x*q.y-p.y*q.x;}
    if(std::hypot(dot,cross)<1e-12)return false;angle=std::atan2(cross,dot);
    translation=b-cv::Point2d(std::cos(angle)*a.x-std::sin(angle)*a.y,std::sin(angle)*a.x+std::cos(angle)*a.y);return true;
}
double validationError(const PlaneModel& model,const std::vector<View>& views,QJsonArray* details=nullptr) {
    double ss=0;int count=0;
    for(const auto& view:views){
        std::vector<cv::Point2d> object,world;
        for(size_t i=0;i<view.observation.pixels.size();++i){
            cv::Point2d p;if(!model.rawToWorld(view.observation.pixels[i],p))return 1e100;
            auto o=view.observation.objectPoints[i];object.emplace_back(o.x,o.y);world.push_back(p);
        }
        if(object.size()<6)return 1e100;
        // Three spatially separated anchors only. Remaining points are never used to fit a homography.
        int first=0,second=1,third=2;double distance=0,area=0;
        for(int i=1;i<int(object.size());++i)if(cv::norm(object[i]-object[first])>distance){distance=cv::norm(object[i]-object[first]);second=i;}
        for(int i=0;i<int(object.size());++i){auto a=object[second]-object[first],b=object[i]-object[first];double value=std::abs(a.x*b.y-a.y*b.x);if(value>area){area=value;third=i;}}
        if(area<1e-6)return 1e100;
        double angle;cv::Point2d t;if(!rigidFit(object,world,{first,second,third},angle,t))return 1e100;
        double sum=0,max=0;int n=0;QJsonArray residuals;
        for(int i=0;i<int(object.size());++i){
            if(i==first||i==second||i==third)continue;
            auto p=object[i];auto predicted=cv::Point2d(std::cos(angle)*p.x-std::sin(angle)*p.y,std::sin(angle)*p.x+std::cos(angle)*p.y)+t;
            double e=cv::norm(predicted-world[i]);sum+=e*e;ss+=e*e;++n;++count;max=std::max(max,e);
            residuals.append(QJsonObject{{"corner_id",view.observation.ids[i]},{"error_mm",e}});
        }
        if(details)details->append(QJsonObject{{"image",view.path},{"evaluated_points",n},{"rms_mm",std::sqrt(sum/n)},
            {"max_mm",max},{"basis","nominal_board_dimensions"},{"anchors",QJsonArray{view.observation.ids[first],view.observation.ids[second],view.observation.ids[third]}},{"residuals",residuals}});
    }return count?std::sqrt(ss/count):1e100;
}
double lineError(const View& view,int columns,const PlaneModel* model) {
    double ss=0;int count=0;
    for(int axis=0;axis<2;++axis){
        std::map<int,std::vector<cv::Point2f>> lines;
        for(size_t i=0;i<view.observation.ids.size();++i){
            cv::Point2d point=view.observation.pixels[i];
            if(model&&!model->rawToWorld(point,point))continue;
            int id=view.observation.ids[i];lines[axis?id%columns:id/columns].emplace_back(float(point.x),float(point.y));
        }
        for(const auto& entry:lines)if(entry.second.size()>=3){
            cv::Vec4f line;cv::fitLine(entry.second,line,cv::DIST_L2,0,.001,.001);
            for(auto p:entry.second){double d=(p.x-line[2])*line[1]-(p.y-line[3])*line[0];ss+=d*d;++count;}
        }
    }return count?std::sqrt(ss/count):0;
}

class PlaneCalibration final : public PlaneCalibrationPort {
    PlaneModel m_model;
    QJsonObject m_report;
    QString m_error;
    std::function<void(const QString&)> m_progress;
    mutable cv::Mat m_x,m_y;
    mutable QString m_mapKey;
    mutable std::mutex m_renderMutex;
    void log(const QString& text){if(m_progress)m_progress(text);}
    void clear(){m_model={};m_report={};m_error.clear();std::lock_guard<std::mutex> lock(m_renderMutex);m_x.release();m_y.release();m_mapKey.clear();}
    bool fail(const QString& text){m_error=text;log(text);return false;}
    bool observations(const QVector<QString>& paths,const calibration::BoardSpec& spec,std::vector<View>& views,
                      cv::Size& size,bool rejectTrainingDuplicates) {
        std::set<QString> seen;
        for(const auto& path:paths){
            QElapsedTimer timer;timer.start();QByteArray bytes;auto image=readImage(path,&bytes);
            if(image.empty())return fail("读取图片失败："+path);
            QString digest=hash(bytes);
            if(!seen.insert(digest).second){log("跳过内容重复的图片："+path);continue;}
            if(rejectTrainingDuplicates&&m_model.trainingHashes.contains(digest))return fail("验证图片与标定图片内容重复："+path);
            if(size.area()&&image.size()!=size)return fail("同批图片或参数文件的分辨率不一致："+path);
            size=image.size();View v;v.path=path;v.hash=digest;
            if(!calibration::detectBoard(image,spec,v.observation))return fail("局部 ChArUco 检测失败："+path+"；"+QString::fromStdString(v.observation.error));
            log(QString("%1：%2 个标记，%3 个角点；检测 %4 ms，含读取 %5 ms")
                .arg(path).arg(v.observation.markerCount).arg(v.observation.pixels.size())
                .arg(v.observation.milliseconds,0,'f',1).arg(timer.nsecsElapsed()/1e6,0,'f',1));
            views.push_back(std::move(v));
        }return true;
    }
public:
    void SetProgressCallback(std::function<void(const QString&)> callback)override{m_progress=std::move(callback);}
    bool PreviewBoard(const QJsonObject& config,cv::Mat& image)override {
        QString error;if(calibration::PreviewBoardConfiguration(config,image,error))return true;return fail(error);
    }
    bool InspectBoard(const cv::Mat& image,const QJsonObject& config,QJsonObject& report)const override {
        return calibration::InspectBoardConfiguration(image,config,report);
    }
    bool Calibrate(const QVector<QString>& paths,const QJsonObject& config,const QString& reference)override {
        clear();QElapsedTimer timer;timer.start();calibration::BoardSpec spec;
        if(!parseBoard(config,spec,m_error))return fail(m_error);
        try {
            std::vector<View> views;cv::Size size;if(!observations(paths,spec,views,size,false))return false;
            if(views.size()<3)return fail("固定平面联合标定至少需要三张内容不同的有效图片。");
            auto ref=std::find_if(views.begin(),views.end(),[&](const View& v){return v.path==reference;});
            if(ref==views.end())return fail("基准图必须属于去重后的标定图片。");
            std::iter_swap(views.begin(),ref);
            const double ws=std::max(spec.squares.width,spec.squares.height)*spec.squareMm;
            std::vector<Solution> candidates;QJsonArray summaries;
            std::vector<View> training=views,held;
            if(views.size()>=5){held.push_back(training.back());training.pop_back();held.push_back(training[training.size()/2]);training.erase(training.begin()+training.size()/2);}
            for(auto mode:std::vector<std::pair<int,bool>>{{0,false},{1,false},{2,false},{2,true}}){
                const int order=mode.first;const bool center=mode.second;
                log(QString("联合求解：%1 个径向参数，%2畸变中心。").arg(order).arg(center?"估计":"固定"));
                FitProblem problem(views,size,ws,order,center);std::vector<Solution> starts;
                for(double k:order?std::vector<double>{-.2,0,.2}:std::vector<double>{0}){
                    auto s=problem.solve(k);if(s.rms<1e90&&s.inliers>=.8*s.count)starts.push_back(std::move(s));
                }
                if(starts.empty())continue;
                std::sort(starts.begin(),starts.end(),[](const Solution& a,const Solution& b){return a.rms<b.rms;});
                Solution s=starts.front();double spread=0;int near=0;
                for(const auto& other:starts)if(other.rms<=s.rms*1.05+.01){
                    ++near;
                    for(auto p:s.model.observations){
                        cv::Point2d a,b;if(!s.model.rawToWorld(p,a)||!other.model.rawToWorld(p,b)){spread=1e100;break;}
                        spread=std::max(spread,cv::norm(a-b));
                    }
                }
                if(order&& (near<2||spread>.05)){log("多初值结果不稳定，排除该候选。");continue;}
                if(!held.empty()){
                    FitProblem cvProblem(training,size,ws,order,center);auto cvFit=cvProblem.solve(0);
                    if(cvFit.rms<1e90)s.validation=validationError(cvFit.model,held);
                } else s.validation=validationError(s.model,views);
                s.score=s.validation;
                summaries.append(QJsonObject{{"radial_order",order},{"free_center",center},{"rms_px",s.rms},
                    {"validation_rms_mm",s.validation<1e90?s.validation:-1},{"multistart_spread_mm",spread},{"validation_independent",!held.empty()}});
                if(s.rms<=2&&s.score<1e90)candidates.push_back(std::move(s));
            }
            if(candidates.empty())return fail("无稳定且有效的平面解。请检查板实际尺寸/布局、平整度、平面运动条件及边缘覆盖。");
            std::sort(candidates.begin(),candidates.end(),[](const Solution& a,const Solution& b){return a.score<b.score;});
            const double tolerance=std::max(.002,candidates.front().score*.05);
            size_t chosen=0;
            for(size_t i=1;i<candidates.size();++i)if(candidates[i].score<=candidates.front().score+tolerance
                &&candidates[i].order+2*int(candidates[i].freeCenter)<candidates[chosen].order+2*int(candidates[chosen].freeCenter))chosen=i;
            auto selected=std::move(candidates[chosen]);m_model=std::move(selected.model);
            std::vector<cv::Vec3d> poses{{0,0,0}};
            const int poseStart=8+selected.order+(selected.freeCenter?2:0);
            for(size_t i=1;i<views.size();++i){
                int at=poseStart+3*int(i-1);cv::Vec3d pose(selected.parameters.at<double>(at),
                    selected.parameters.at<double>(at+1)*ws,selected.parameters.at<double>(at+2)*ws);
                bool distinct=true;
                for(auto previous:poses)if(std::abs(std::remainder(pose[0]-previous[0],2*CV_PI))<CV_PI/180
                    &&std::hypot(pose[1]-previous[1],pose[2]-previous[2])<1)distinct=false;
                if(distinct)poses.push_back(pose);
            }
            if(poses.size()<3){clear();return fail("需要至少三个不同的平面位置/角度；亮度变化或重复姿态不增加几何约束。");}
            std::vector<double> millimeterErrors;QJsonArray annotatedViews;
            for(size_t i=0;i<views.size();++i){
                auto item=selected.views[int(i)].toObject();auto points=item["points"].toArray();
                double angle=0,tx=0,ty=0;
                if(i){int at=poseStart+3*int(i-1);angle=selected.parameters.at<double>(at);tx=selected.parameters.at<double>(at+1)*ws;ty=selected.parameters.at<double>(at+2)*ws;}
                for(size_t j=0;j<views[i].observation.pixels.size();++j){
                    auto p=points[int(j)].toObject();auto object=views[i].observation.objectPoints[j];cv::Point2d actual;
                    if(m_model.rawToWorld(views[i].observation.pixels[j],actual)){
                        auto expected=cv::Point2d(std::cos(angle)*object.x-std::sin(angle)*object.y+tx,
                            std::sin(angle)*object.x+std::cos(angle)*object.y+ty);
                        double e=cv::norm(actual-expected);millimeterErrors.push_back(e);
                        p["world_x_mm"]=actual.x;p["world_y_mm"]=actual.y;p["error_nominal_mm"]=e;
                    }points[int(j)]=p;
                }
                item["points"]=points;item["raw_line_rms_px"]=lineError(views[i],spec.innerSize().width,nullptr);
                item["mapped_line_rms_mm"]=lineError(views[i],spec.innerSize().width,&m_model);annotatedViews.append(item);
            }
            double sumMm=0;for(double e:millimeterErrors)sumMm+=e*e;
            std::sort(millimeterErrors.begin(),millimeterErrors.end());QJsonArray hull;
            for(auto p:m_model.supportHull)hull.append(QJsonArray{p.x,p.y});
            m_model.board=config;m_model.referenceImage=reference;
            for(const auto& v:views)m_model.trainingHashes.append(v.hash);
            m_report={{"model_type","fixed_plane_radial"},{"status","geometry_calibrated"},{"physical_validation_status","unverified"},
                {"coordinate_basis","reference_board_pattern_mm"},{"rms_px",selected.rms},{"image_count",int(views.size())},
                {"point_count",selected.count},{"radial_order",selected.order},{"free_distortion_center",selected.freeCenter},
                {"candidate_reports",summaries},{"view_reports",annotatedViews},{"coverage_hull_raw",hull},
                {"rms_nominal_mm",std::sqrt(sumMm/std::max(size_t(1),millimeterErrors.size()))},
                {"p95_nominal_mm",millimeterErrors.empty()?0:millimeterErrors[size_t(std::ceil(millimeterErrors.size()*.95))-1]},
                {"max_nominal_mm",millimeterErrors.empty()?0:millimeterErrors.back()},
                {"solve_and_detection_ms",timer.nsecsElapsed()/1e6},
                {"note","平面标定，不声明真实相机焦距；毫米残差基于棋盘名义/提供的实测尺寸，未进行独立实物验收。"}};
            m_model.report=m_report;log(QString("固定平面标定完成：%1 张图，RMS %2 px；未实测验收。").arg(views.size()).arg(selected.rms,0,'f',4));
            return true;
        }catch(const std::exception& e){clear();return fail(QString::fromUtf8(e.what()));}
    }
    bool LoadCalibrationData(const QString& path)override {
        clear();if(!PlaneModel::load(path,m_model,false,m_error))return fail(m_error);
        m_report=m_model.report;return true;
    }
    bool SaveCalibrationData(const QString& path)const override{return m_model.save(path,false);}
    bool SetRegion(const Region& region)override {
        m_x.release();m_y.release();m_mapKey.clear();
        bool ok=m_model.setRegion({region.centerMm.x(),region.centerMm.y()},{region.widthMm,region.heightMm},region.pixelsPerMm,m_error);
        m_report=m_model.report;if(!ok)log(m_error);return ok;
    }
    bool MapPoints(const QVector<QPointF>& raw,QVector<QPointF>& mm)const override {
        mm.clear();std::vector<cv::Point2d> p;for(auto q:raw)p.emplace_back(q.x(),q.y());
        std::vector<cv::Point2d> result;if(!m_model.mapPoints(p,result))return false;
        for(auto q:result)mm.append({q.x,q.y});return true;
    }
    bool MapRawToReference(const QPointF& raw,QPointF& mm)const override {
        if(!m_model.valid())return false;cv::Point2d p;
        if(!m_model.rawToWorld({raw.x(),raw.y()},p))return false;mm={p.x,p.y};return true;
    }
    QVector<QPointF> GetRegionBoundary()const override {
        QVector<QPointF> result;if(!m_model.valid())return result;
        for(auto p:m_model.regionBoundary())result.append({p.x,p.y});return result;
    }
    bool RectifyImage(const cv::Mat& raw,cv::Mat& output)const override {
        const cv::Mat input=raw;output.release();
        if(!m_model.regionValid||input.empty()||input.dims!=2||input.size()!=m_model.imageSize
           ||input.depth()!=CV_8U||(input.channels()!=1&&input.channels()!=3&&input.channels()!=4))return false;
        try{
            std::lock_guard<std::mutex> lock(m_renderMutex);
            QString key=m_model.fingerprint()+QString::number(m_model.outputSize.width)+":"+QString::number(m_model.outputSize.height);
            if(key!=m_mapKey){if(!m_model.buildMaps(m_x,m_y))return false;m_mapKey=key;}
            cv::remap(input,output,m_x,m_y,cv::INTER_LINEAR,cv::BORDER_CONSTANT);return !output.empty();
        }catch(const cv::Exception&){output.release();return false;}
    }
    bool Validate(const QVector<QString>& paths,const QJsonObject& physical)override {
        m_error.clear();
        m_model.report["physical_validation_status"]="unverified";m_model.report.remove("physical_validation");
        if(!m_model.valid())return fail("尚无有效平面标定。");
        if(paths.isEmpty()&&physical.isEmpty())return fail("请提供未参与拟合的验证图或独立实物验收数据。");
        try{
            if(!paths.isEmpty()){
                calibration::BoardSpec spec;if(!parseBoard(m_model.board,spec,m_error))return false;
                std::vector<View> views;auto size=m_model.imageSize;if(!observations(paths,spec,views,size,true))return false;
                QJsonArray details;double e=validationError(m_model,views,&details);
                if(e>=1e90)return fail("验证图的锚点/观测不足或映射失败。");
                m_model.report["held_out_validation"]=QJsonObject{{"rms_mm",e},{"views",details},{"basis","nominal_board_dimensions"}};
            }
            if(!physical.isEmpty()){
                if(!m_model.regionValid)return fail("独立实物验收前须设置有效测量区域。");
                if(!physical.value("independent").toBool()||physical.value("source").toString().trimmed().isEmpty())
                    return fail("验收数据须声明 independent=true 并记录独立尺寸/坐标来源 source。");
                const auto points=physical.value("points").toArray(),distances=physical.value("distances").toArray();
                double maximum=0,ss=0;int count=0,cells[9]={};QJsonArray details;bool horizontal=false,vertical=false,diagonal=false;
                auto anchorZone=[](double value){
                    if(value<=.1+1e-9)return 0;if(value>=.9-1e-9)return 2;
                    if(value>=.4-1e-9&&value<=.6+1e-9)return 1;return -1;
                };
                std::map<QString,std::vector<cv::Point2d>> repeats;
                std::map<QString,std::set<QString>> repeatFrames;
                std::map<QString,cv::Point2d> repeatExpected;
                std::map<QString,int> repeatCell;
                for(auto v:points){
                    auto p=v.toObject();for(auto key:{"pixel_x","pixel_y","expected_x_mm","expected_y_mm"})if(!p.value(key).isDouble())return fail("实物验收点缺少数值字段。");
                    std::vector<cv::Point2d> raw{{p["pixel_x"].toDouble(),p["pixel_y"].toDouble()}};std::vector<cv::Point2d> mm;
                    if(!m_model.mapPoints(raw,mm))return fail("实物验收点超出有效区域。");
                    cv::Point2d expected(p["expected_x_mm"].toDouble(),p["expected_y_mm"].toDouble());
                    if(expected.x<0||expected.y<0||expected.x>m_model.regionSize.width||expected.y>m_model.regionSize.height)return fail("实物参考坐标超出当前 ROI。");
                    double e=cv::norm(mm[0]-expected);maximum=std::max(maximum,e);ss+=e*e;++count;
                    int x=anchorZone(expected.x/m_model.regionSize.width),y=anchorZone(expected.y/m_model.regionSize.height);
                    if(x>=0&&y>=0)++cells[y*3+x];
                    if(!p.value("target_id").toString().isEmpty()&&!p.value("frame_id").toString().isEmpty()){
                        auto id=p["target_id"].toString(),frame=p["frame_id"].toString();
                        if(!repeatFrames[id].insert(frame).second)return fail("同一验收点的重复测量必须来自不同 frame_id。");
                        if(repeatExpected.count(id)&&cv::norm(repeatExpected[id]-expected)>1e-6)return fail("同一 target_id 的参考坐标不一致。");
                        repeatExpected[id]=expected;repeatCell[id]=x>=0&&y>=0?y*3+x:-1;
                        repeats[id].push_back(mm[0]);
                    }
                    details.append(QJsonObject{{"error_mm",e},{"x_mm",mm[0].x},{"y_mm",mm[0].y}});
                }
                for(auto v:distances){
                    auto p=v.toObject();auto a=p.value("pixel_a").toArray(),b=p.value("pixel_b").toArray();
                    if(a.size()!=2||b.size()!=2||!p.value("expected_mm").isDouble()||p["expected_mm"].toDouble()<=0)return fail("独立长度参考无效。");
                    for(auto coordinate:{a[0],a[1],b[0],b[1]})if(!coordinate.isDouble())return fail("长度参考端点需要数值像素坐标。");
                    std::vector<cv::Point2d> raw{{a[0].toDouble(),a[1].toDouble()},{b[0].toDouble(),b[1].toDouble()}};
                    std::vector<cv::Point2d> mm;if(!m_model.mapPoints(raw,mm))return fail("长度参考端点超出测量区域。");
                    double e=std::abs(cv::norm(mm[0]-mm[1])-p["expected_mm"].toDouble());maximum=std::max(maximum,e);ss+=e*e;++count;
                    auto delta=mm[1]-mm[0];double length=cv::norm(delta);
                    if(std::abs(delta.y)<.05*length&&length+1e-7>=.8*m_model.regionSize.width)horizontal=true;
                    else if(std::abs(delta.x)<.05*length&&length+1e-7>=.8*m_model.regionSize.height)vertical=true;
                    else if(std::abs(delta.x)>.05*length&&std::abs(delta.y)>.05*length
                        &&length+1e-7>=.8*std::hypot(m_model.regionSize.width,m_model.regionSize.height))diagonal=true;
                    details.append(QJsonObject{{"length_error_mm",e}});
                }
                int empty=0;for(int n:cells)empty+=!n;
                double repeatability=0;int repeated=0,repeatedCells[9]={};
                for(const auto& entry:repeats)if(entry.second.size()>=3){
                    cv::Point2d mean;for(auto p:entry.second)mean+=p;mean*=1./entry.second.size();
                    for(auto p:entry.second)repeatability=std::max(repeatability,cv::norm(p-mean));++repeated;
                    if(repeatCell[entry.first]>=0)++repeatedCells[repeatCell[entry.first]];
                }
                int missingRepeats=0;for(int n:repeatedCells)missingRepeats+=n==0;
                const bool referenceQualified=physical["reference_uncertainty_mm"].isDouble()
                    &&physical["reference_uncertainty_mm"].toDouble()>=0&&physical["reference_uncertainty_mm"].toDouble()<=.03;
                bool passed=count>0&&!empty&&horizontal&&vertical&&diagonal&&maximum<=.1&&!missingRepeats&&repeatability<=.1&&referenceQualified;
                m_model.report["physical_validation"]=QJsonObject{{"scope_hash",m_model.fingerprint()},{"source",physical["source"]},
                    {"point_count",points.size()},{"distance_count",distances.size()},{"max_error_mm",maximum},
                    {"rms_mm",count?std::sqrt(ss/count):0},{"missing_regions",empty},{"repeated_targets",repeated},
                    {"repeatability_max_mm",repeatability},{"missing_repeatability_regions",missingRepeats},
                    {"horizontal_checked",horizontal},{"vertical_checked",vertical},{"diagonal_checked",diagonal},
                    {"reference_qualified",referenceQualified},{"details",details},{"passed",passed}};
                m_model.report["physical_validation_status"]=passed?"passed_0_1mm":"unverified";
            }
            m_report=m_model.report;return true;
        }catch(const std::exception& e){return fail(QString::fromUtf8(e.what()));}
    }
    QJsonObject GetReport()const override {
        auto result=m_model.report.isEmpty()?m_report:m_model.report;
        result["error"]=m_error;result["model_ready"]=m_model.valid();result["region_valid"]=m_model.regionValid;
        result["board_configuration"]=m_model.board;
        result["reference_image"]=m_model.referenceImage;result["image_width"]=m_model.imageSize.width;result["image_height"]=m_model.imageSize.height;
        result["roi_center_x_mm"]=m_model.regionCenter.x;result["roi_center_y_mm"]=m_model.regionCenter.y;
        result["roi_width_mm"]=m_model.regionSize.width;result["roi_height_mm"]=m_model.regionSize.height;
        result["requested_pixels_per_mm"]=m_model.requestedPixelsPerMm;result["output_width_px"]=m_model.outputSize.width;result["output_height_px"]=m_model.outputSize.height;
        if(m_model.regionValid){result["actual_pixels_per_mm_x"]=m_model.outputSize.width/m_model.regionSize.width;result["actual_pixels_per_mm_y"]=m_model.outputSize.height/m_model.regionSize.height;}
        return result;
    }
    bool SaveMeasurementCalibration(const QString& path)const override{return m_model.save(path,true);}
};
}
PlaneCalibrationPort* newPlaneCalibrationPort(){return new PlaneCalibration();}
