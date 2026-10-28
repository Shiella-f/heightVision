#include "PlaneModel.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace calibration {
namespace {
bool finite(cv::Point2d p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool project(const cv::Mat& h, cv::Point2d p, cv::Point2d& q) {
    const double* a = h.ptr<double>();
    const double w = a[6]*p.x+a[7]*p.y+a[8];
    if (!std::isfinite(w) || std::abs(w)<1e-13) return false;
    q = {(a[0]*p.x+a[1]*p.y+a[2])/w, (a[3]*p.x+a[4]*p.y+a[5])/w};
    return finite(q);
}
bool readMatrix(const cv::FileStorage& fs, const char* key, cv::Mat& m) {
    if (fs[key].empty()) return false;
    fs[key] >> m;
    if (m.empty() || m.channels()!=1) return false;
    m.convertTo(m,CV_64F);
    return cv::checkRange(m);
}
bool number(const cv::FileStorage& fs,const char* name,double& result) {
    auto n=fs[name]; if(!n.isInt()&&!n.isReal())return false;
    result=double(n);return std::isfinite(result);
}
cv::Mat pointMatrix(const std::vector<cv::Point2f>& points) {
    cv::Mat m(int(points.size()),2,CV_64F);
    for(int i=0;i<m.rows;++i){m.at<double>(i,0)=points[i].x;m.at<double>(i,1)=points[i].y;}
    return m;
}
bool readPoints(const cv::FileStorage& fs,const char* key,std::vector<cv::Point2f>& points) {
    cv::Mat m;if(!readMatrix(fs,key,m)||m.cols!=2||m.rows<3||m.rows>1000000)return false;
    for(int i=0;i<m.rows;++i)points.emplace_back(float(m.at<double>(i,0)),float(m.at<double>(i,1)));
    return true;
}
QJsonObject json(const cv::FileStorage& fs,const char* key) {
    QByteArray bytes;auto node=fs[key];
    if(node.isString())bytes=QByteArray::fromStdString(std::string(node));
    else if(node.isSeq()){
        QByteArray encoded;for(auto it=node.begin();it!=node.end();++it){if(!(*it).isString())return {};encoded+=QByteArray::fromStdString(std::string(*it));}
        bytes=QByteArray::fromBase64(encoded);
    }
    return QJsonDocument::fromJson(bytes).object();
}
void writeJson(cv::FileStorage& fs,const char* key,const QJsonDocument& document) {
    const auto encoded=document.toJson(QJsonDocument::Compact).toBase64();
    fs<<key<<"[";
    for(qsizetype at=0;at<encoded.size();at+=512)fs<<encoded.mid(at,512).toStdString();
    fs<<"]";
}
}

bool PlaneModel::valid() const {
    if(imageSize.width<=0||imageSize.height<=0||imageSize.width>=32767||imageSize.height>=32767
       ||!std::isfinite(normalizationScale)||normalizationScale<=0
       ||!finite(distortionCenter)||!std::isfinite(radial[0])||!std::isfinite(radial[1])
       ||worldHomography.type()!=CV_64FC1||worldHomography.size()!=cv::Size(3,3)
       ||!cv::checkRange(worldHomography)||supportHull.size()<3||observations.size()<3)return false;
    if(distortionCenter.x < -.25*imageSize.width || distortionCenter.x>1.25*imageSize.width
       ||distortionCenter.y<-.25*imageSize.height||distortionCenter.y>1.25*imageSize.height)return false;
    cv::Mat inv;const double n=cv::norm(worldHomography,cv::NORM_INF);
    if(!(n>0)||cv::invert(worldHomography/n,inv,cv::DECOMP_SVD)<1e-12)return false;
    if(!cv::isContourConvex(supportHull)||std::abs(cv::contourArea(supportHull))<1)return false;
    for(auto p:observations)if(!finite(p)||p.x<0||p.y<0||p.x>=imageSize.width||p.y>=imageSize.height)return false;
    return true;
}
double PlaneModel::radiusLimit() const {
    double limit=4.;
    // First positive zero of the radial derivative. Stay strictly on its unique branch.
    const double a=5*radial[1],b=3*radial[0];
    if(std::abs(a)<1e-15) { if(b<0)limit=std::min(limit,std::sqrt(-1/b)); }
    else {
        const double discriminant=b*b-4*a;
        if(discriminant>=0)for(double t:{(-b-std::sqrt(discriminant))/(2*a),(-b+std::sqrt(discriminant))/(2*a)})
            if(t>0)limit=std::min(limit,std::sqrt(t));
    }
    return limit*(1-1e-8);
}
bool PlaneModel::radialValid(double radius) const {
    if(!std::isfinite(radius)||radius<0||radius>=radiusLimit())return false;
    const double t=radius*radius;
    const double factor=1+radial[0]*t+radial[1]*t*t;
    const double derivative=1+3*radial[0]*t+5*radial[1]*t*t;
    return factor>1e-6 && derivative>1e-6;
}
bool PlaneModel::rawToWorld(cv::Point2d raw,cv::Point2d& world,bool checkSupport) const {
    if(!finite(raw)||raw.x<0||raw.y<0||raw.x>imageSize.width-1||raw.y>imageSize.height-1)return false;
    if(checkSupport && cv::pointPolygonTest(supportHull,raw,true)<-1)return false;
    const cv::Point2d n=(raw-distortionCenter)/normalizationScale;
    const double rd=cv::norm(n),limit=radiusLimit();
    auto forward=[&](double r){double t=r*r;return r*(1+radial[0]*t+radial[1]*t*t);};
    if(!std::isfinite(rd)||rd>forward(limit))return false;
    double lo=0,hi=limit;
    for(int i=0;i<65;++i){double mid=(lo+hi)*.5;if(forward(mid)<rd)lo=mid;else hi=mid;}
    const double r=(lo+hi)*.5;
    return radialValid(r)&&project(worldHomography,rd>1e-15?n*(r/rd):cv::Point2d(),world);
}
bool PlaneModel::worldToRaw(cv::Point2d world,cv::Point2d& raw,bool checkImage) const {
    cv::Point2d n;
    if(!project(worldHomography.inv(),world,n)||!radialValid(cv::norm(n)))return false;
    double t=n.dot(n); raw=distortionCenter+n*(normalizationScale*(1+radial[0]*t+radial[1]*t*t));
    return finite(raw)&&(!checkImage||(raw.x>=0&&raw.y>=0&&raw.x<=imageSize.width-1&&raw.y<=imageSize.height-1));
}
bool PlaneModel::mapPoints(const std::vector<cv::Point2f>& raw,std::vector<cv::Point2d>& mm) const {
    return mapPoints(std::vector<cv::Point2d>(raw.begin(),raw.end()),mm);
}
bool PlaneModel::mapPoints(const std::vector<cv::Point2d>& raw,std::vector<cv::Point2d>& mm) const {
    mm.clear();if(!regionValid||!valid()||raw.empty())return false;
    const cv::Point2d origin=regionCenter-cv::Point2d(regionSize.width/2,regionSize.height/2);
    std::vector<cv::Point2d> result;result.reserve(raw.size());
    for(auto p:raw) {
        cv::Point2d w;if(!rawToWorld(p,w,true))return false;w-=origin;
        constexpr double eps=1e-7;
        if(w.x < -eps||w.y < -eps||w.x>regionSize.width+eps||w.y>regionSize.height+eps)return false;
        w.x=std::clamp(w.x,0.,regionSize.width);w.y=std::clamp(w.y,0.,regionSize.height);result.push_back(w);
    }
    mm=std::move(result);return true;
}
std::vector<cv::Point2d> PlaneModel::regionBoundary() const {
    std::vector<cv::Point2d> result;
    if(regionSize.width<=0||regionSize.height<=0)return result;
    cv::Point2d origin=regionCenter-cv::Point2d(regionSize.width/2,regionSize.height/2);
    for(int edge=0;edge<4;++edge)for(int i=0;i<25;++i) {
        double t=i/24.;cv::Point2d p;
        if(edge==0)p={t*regionSize.width,0};
        if(edge==1)p={regionSize.width,t*regionSize.height};
        if(edge==2)p={(1-t)*regionSize.width,regionSize.height};
        if(edge==3)p={0,(1-t)*regionSize.height};
        cv::Point2d raw;if(!worldToRaw(origin+p,raw,false))return {};result.push_back(raw);
    }
    return result;
}
bool PlaneModel::setRegion(cv::Point2d center,cv::Size2d size,double density,QString& error) {
    regionValid=false;outputSize={};error.clear();
    report.remove("region_coverage");
    const bool changed=cv::norm(center-regionCenter)>1e-10||size!=regionSize;
    regionCenter=center;regionSize=size;requestedPixelsPerMm=density;
    if(changed){report["physical_validation_status"]="unverified";report.remove("physical_validation");}
    if(!valid()||!finite(center)||!std::isfinite(size.width)||!std::isfinite(size.height)
       ||size.width<=0||size.height<=0||!std::isfinite(density)||density<=0) {
        error="区域中心、宽高和像素密度必须有效，宽高和密度必须大于零。";return false;
    }
    const double nx=std::max(1.,std::round(size.width*density)),ny=std::max(1.,std::round(size.height*density));
    if(nx>=32767||ny>=32767||nx*ny*12>512.*1024*1024){
        error="输出超过 OpenCV 尺寸或 512 MiB 映射/图像预算，请降低渲染密度，物理范围保持不变。";return false;
    }
    const cv::Point2d origin=center-cv::Point2d(size.width/2,size.height/2);
    int missing=0;double minJac=std::numeric_limits<double>::max(),orientation=0;
    for(int y=0;y<=40;++y)for(int x=0;x<=40;++x) {
        cv::Point2d w=origin+cv::Point2d(size.width*x/40,size.height*y/40),p;
        if(!worldToRaw(w,p)||cv::pointPolygonTest(supportHull,p,true)<-1){++missing;continue;}
        cv::Point2d a,b;
        if(!worldToRaw(w+cv::Point2d(.001,0),a,false)||!worldToRaw(w+cv::Point2d(0,.001),b,false)){++missing;continue;}
        const double cross=(a.x-p.x)*(b.y-p.y)-(a.y-p.y)*(b.x-p.x);
        if(orientation==0)orientation=cross;
        if(cross*orientation<=0)++missing;
        minJac=std::min(minJac,std::abs(cross)/1e-6);
        if(std::abs(cross)<1e-12)++missing;
    }
    int cells[9]={};
    for(auto p:observations) {
        cv::Point2d w;if(!rawToWorld(p,w))continue;w-=origin;
        if(w.x<0||w.y<0||w.x>size.width||w.y>size.height)continue;
        int x=std::min(2,int(w.x/size.width*3)),y=std::min(2,int(w.y/size.height*3));++cells[y*3+x];
    }
    QJsonArray coverage;int empty=0;for(int n:cells){coverage.append(n);empty+=n==0;}
    report["region_coverage"]=QJsonObject{{"missing_samples",missing},{"sample_count",1681},{"cells_3x3",coverage},{"min_jacobian_abs",minJac}};
    if(missing||empty){error=QString("区域超出原图/标定支持范围或观测不足：%1 个采样位置无效，%2 个分区缺少角点。请调整范围/中心或补图。").arg(missing).arg(empty);return false;}
    outputSize={int(nx),int(ny)};regionValid=true;return true;
}
bool PlaneModel::buildMaps(cv::Mat& x,cv::Mat& y) const {
    x.release();y.release();if(!regionValid||!valid())return false;
    x.create(outputSize,CV_32F);y.create(outputSize,CV_32F);
    const cv::Mat inverse=worldHomography.inv();
    const cv::Point2d origin=regionCenter-cv::Point2d(regionSize.width/2,regionSize.height/2);
    cv::parallel_for_(cv::Range(0,outputSize.height),[&](const cv::Range& rows){
        for(int row=rows.start;row<rows.end;++row)for(int col=0;col<outputSize.width;++col){
            cv::Point2d world=origin+cv::Point2d((col+.5)*regionSize.width/outputSize.width,(row+.5)*regionSize.height/outputSize.height),n;
            if(!project(inverse,world,n)){x.at<float>(row,col)=-100000;y.at<float>(row,col)=-100000;continue;}
            double t=n.dot(n),factor=normalizationScale*(1+radial[0]*t+radial[1]*t*t);
            x.at<float>(row,col)=float(distortionCenter.x+n.x*factor);
            y.at<float>(row,col)=float(distortionCenter.y+n.y*factor);
        }
    });
    return true;
}
QString PlaneModel::fingerprint(bool includeRegion) const {
    QByteArray data;
    auto add=[&](double value){data.append(QByteArray::number(value,'g',17));data.append(';');};
    add(imageSize.width);add(imageSize.height);add(normalizationScale);add(distortionCenter.x);add(distortionCenter.y);
    add(radial[0]);add(radial[1]);for(int i=0;i<9;++i)add(worldHomography.ptr<double>()[i]);
    if(includeRegion){add(regionCenter.x);add(regionCenter.y);add(regionSize.width);add(regionSize.height);}
    return QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());
}
bool PlaneModel::save(const QString& path,bool measurement) const {
    if(!valid()||(measurement&&!regionValid))return false;
    try {
        cv::FileStorage fs("",cv::FileStorage::WRITE|cv::FileStorage::MEMORY|cv::FileStorage::FORMAT_YAML);
        fs<<"format_version"<<2<<"model_type"<<"fixed_plane_radial"<<"file_kind"<<(measurement?"measurement_profile":"plane_calibration")
          <<"calibration_scope"<<"fixed_height_plane"<<"physical_origin"<<"roi_top_left_boundary"
          <<"physical_validation_status"<<report.value("physical_validation_status").toString("unverified").toStdString()
          <<"input_stage"<<"raw"<<"unit"<<"mm"<<"image_width"<<imageSize.width<<"image_height"<<imageSize.height
          <<"normalization_scale_px"<<normalizationScale<<"distortion_center_x_px"<<distortionCenter.x<<"distortion_center_y_px"<<distortionCenter.y
          <<"radial_k1"<<radial[0]<<"radial_k2"<<radial[1]<<"world_homography_mm"<<worldHomography
          <<"support_hull_raw"<<pointMatrix(supportHull)<<"observations_raw"<<pointMatrix(observations);
        fs<<"json_encoding"<<"base64_chunks";
        writeJson(fs,"board_json",QJsonDocument(board));
        writeJson(fs,"report_json",QJsonDocument(report));
        writeJson(fs,"training_hashes_json",QJsonDocument(QJsonArray::fromStringList(trainingHashes)));
        fs<<"reference_image"<<referenceImage.toStdString()<<"has_region"<<int(regionValid)
          <<"roi_center_x_mm"<<regionCenter.x<<"roi_center_y_mm"<<regionCenter.y
          <<"roi_width_mm"<<regionSize.width<<"roi_height_mm"<<regionSize.height<<"requested_pix_per_mm"<<requestedPixelsPerMm
          <<"actual_pix_per_mm_x"<<(regionValid?outputSize.width/regionSize.width:0.)
          <<"actual_pix_per_mm_y"<<(regionValid?outputSize.height/regionSize.height:0.)
          <<"output_width_px"<<outputSize.width<<"output_height_px"<<outputSize.height
          <<"pixel_center_convention"<<"half_pixel"<<"model_region_hash"<<fingerprint().toStdString();
        auto bytes=fs.releaseAndGetString();QSaveFile file(path);
        return file.open(QIODevice::WriteOnly)&&file.write(bytes.data(),qint64(bytes.size()))==qint64(bytes.size())&&file.commit();
    }catch(const cv::Exception&){return false;}
}
bool PlaneModel::load(const QString& path,PlaneModel& model,bool requireRegion,QString& error) {
    model={};error.clear();
    try {
        QFile file(path);if(!file.open(QIODevice::ReadOnly)){error="无法读取平面标定文件。";return false;}
        cv::FileStorage fs(file.readAll().toStdString(),cv::FileStorage::READ|cv::FileStorage::MEMORY);
        if(!fs.isOpened()||!fs["format_version"].isInt()||int(fs["format_version"])!=2
           ||std::string(fs["model_type"])!="fixed_plane_radial"||std::string(fs["input_stage"])!="raw"
           ||std::string(fs["unit"])!="mm"||std::string(fs["pixel_center_convention"])!="half_pixel") {error="平面参数版本、模型或坐标约定无效。";return false;}
        const std::string kind=fs["file_kind"];
        if(kind!="plane_calibration"&&kind!="measurement_profile"){error="不支持的平面文件类型。";return false;}
        if(requireRegion&&kind!="measurement_profile"){error="需要已经设置有效 ROI 的振镜测量参数包。";return false;}
        PlaneModel candidate;
        if(!fs["image_width"].isInt()||!fs["image_height"].isInt())return false;
        candidate.imageSize={int(fs["image_width"]),int(fs["image_height"])};
        if(!number(fs,"normalization_scale_px",candidate.normalizationScale)
           ||!number(fs,"distortion_center_x_px",candidate.distortionCenter.x)||!number(fs,"distortion_center_y_px",candidate.distortionCenter.y)
           ||!number(fs,"radial_k1",candidate.radial[0])||!number(fs,"radial_k2",candidate.radial[1])
           ||!readMatrix(fs,"world_homography_mm",candidate.worldHomography)
           ||!readPoints(fs,"support_hull_raw",candidate.supportHull)||!readPoints(fs,"observations_raw",candidate.observations))return false;
        candidate.board=json(fs,"board_json");candidate.report=json(fs,"report_json");
        if(fs["reference_image"].isString())candidate.referenceImage=QString::fromStdString(std::string(fs["reference_image"]));
        if(!fs["training_hashes_json"].empty()){
            QByteArray bytes;
            if(fs["training_hashes_json"].isString())bytes=QByteArray::fromStdString(std::string(fs["training_hashes_json"]));
            else {
                QByteArray encoded;auto node=fs["training_hashes_json"];
                if(!node.isSeq())return false;
                for(auto it=node.begin();it!=node.end();++it){if(!(*it).isString())return false;encoded+=QByteArray::fromStdString(std::string(*it));}
                bytes=QByteArray::fromBase64(encoded);
            }
            auto array=QJsonDocument::fromJson(bytes).array();
            for(auto v:array)candidate.trainingHashes.append(v.toString());
        }
        if(!candidate.valid()){error="平面矩阵、归一化或有效域无效。";return false;}
        if(!fs["has_region"].isInt())return false;
        if(int(fs["has_region"])) {
            cv::Point2d center;cv::Size2d size;double density;
            if(!number(fs,"roi_center_x_mm",center.x)||!number(fs,"roi_center_y_mm",center.y)
               ||!number(fs,"roi_width_mm",size.width)||!number(fs,"roi_height_mm",size.height)
               ||!number(fs,"requested_pix_per_mm",density))return false;
            candidate.regionCenter=center;candidate.regionSize=size;
            if(!candidate.setRegion(center,size,density,error))return false;
            if(!fs["output_width_px"].isInt()||!fs["output_height_px"].isInt()
               ||candidate.outputSize!=cv::Size(int(fs["output_width_px"]),int(fs["output_height_px"])))return false;
            double actualX,actualY;
            if(!number(fs,"actual_pix_per_mm_x",actualX)||!number(fs,"actual_pix_per_mm_y",actualY)
               ||std::abs(actualX-candidate.outputSize.width/size.width)>1e-10
               ||std::abs(actualY-candidate.outputSize.height/size.height)>1e-10){error="参数包实际采样密度与范围/像素尺寸不一致。";return false;}
        }
        if(requireRegion&&!candidate.regionValid){error="参数包没有有效测量范围。";return false;}
        if(candidate.report.value("physical_validation").toObject().value("scope_hash").toString()!=candidate.fingerprint())
            candidate.report["physical_validation_status"]="unverified";
        model=std::move(candidate);return true;
    }catch(const std::exception& e){error=QString::fromUtf8(e.what());return false;}
}
}
