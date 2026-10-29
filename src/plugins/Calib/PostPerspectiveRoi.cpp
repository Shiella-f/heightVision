#include "PostPerspectiveRoi.h"
#include <QJsonArray>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool finite(cv::Point2d p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool project(const cv::Mat& h,cv::Point2d p,cv::Point2d& q){
    const auto* a=h.ptr<double>();double w=a[6]*p.x+a[7]*p.y+a[8];
    if(!std::isfinite(w)||std::abs(w)<1e-12)return false;
    q={(a[0]*p.x+a[1]*p.y+a[2])/w,(a[3]*p.x+a[4]*p.y+a[5])/w};return finite(q);
}
std::vector<cv::Point2f> points(const QVector<QPointF>& p){std::vector<cv::Point2f> r;for(auto q:p)r.emplace_back(float(q.x()),float(q.y()));return r;}
QVector<QPointF> ordered(std::vector<cv::Point2f> p){
    cv::Point2f center;for(auto q:p)center+=q;center*=.25f;
    std::sort(p.begin(),p.end(),[&](auto a,auto b){return std::atan2(a.y-center.y,a.x-center.x)<std::atan2(b.y-center.y,b.x-center.x);});
    auto first=std::min_element(p.begin(),p.end(),[](auto a,auto b){return a.x+a.y<b.x+b.y;});std::rotate(p.begin(),first,p.end());
    if(cv::contourArea(p,true)<0)std::reverse(p.begin()+1,p.end());
    QVector<QPointF> r;for(auto q:p)r.append({q.x,q.y});return r;
}
}
bool CanExtractPostPerspectiveRoi(double w,double h){return std::isfinite(w)&&std::isfinite(h)&&w>300&&h>300;}
namespace calibration {
cv::Mat halfPixelToMm(cv::Size px,cv::Size2d mm){return (cv::Mat_<double>(3,3)<<mm.width/px.width,0,.5*mm.width/px.width,0,mm.height/px.height,.5*mm.height/px.height,0,0,1);}
bool PostPerspectiveRoi::configure(const PostPerspectiveRoiConfig& requested,cv::Size pixels,cv::Size2d mm,
    double pixelsPerMm,const cv::Mat& pixelToMm,const QString& baseBinding,QString& error){
    *this={};error.clear();auto fail=[&](const QString& s){error=s;return false;};
    if(!CanExtractPostPerspectiveRoi(mm.width,mm.height))return fail("基础透视的宽和高必须分别大于300 mm。");
    if(requested.corners.size()!=4||pixels.width<2||pixels.height<2||!std::isfinite(pixelsPerMm)||pixelsPerMm<=0
        ||requested.targetSizeMm!=QSizeF(300,300))return fail("ROI必须有四个角，目标尺寸必须为300×300 mm。");
    double rw=requested.referenceSizeMm.width(),rh=requested.referenceSizeMm.height();
    if(!std::isfinite(rw)||!std::isfinite(rh)||rw<=0||rh<=0)return fail("请输入外框实际宽高。");
    if(pixelToMm.type()!=CV_64FC1||pixelToMm.size()!=cv::Size(3,3)||!cv::checkRange(pixelToMm)||std::abs(cv::determinant(pixelToMm))<1e-15)
        return fail("基础像素坐标约定无效。");
    auto polygon=points(requested.corners);
    for(auto p:requested.corners)if(!std::isfinite(p.x())||!std::isfinite(p.y())||p.x()<0||p.y()<0||p.x()>pixels.width-1||p.y()>pixels.height-1)
        return fail("参考框角点超出基础透视图片。");
    if(!cv::isContourConvex(polygon)||cv::contourArea(polygon,true)<=1)return fail("四角须按左上、右上、右下、左下排列，且不能退化或自交。");
    std::vector<cv::Point2d> src;for(auto p:requested.corners){cv::Point2d q;if(!project(pixelToMm,{p.x(),p.y()},q))return fail("角点映射失败。");src.push_back(q);}
    const double x=(300-rw)/2,y=(300-rh)/2;
    std::vector<cv::Point2d> dst{{x,y},{x+rw,y},{x+rw,y+rh},{x,y+rh}};
    cv::Mat transform=cv::findHomography(src,dst,0);
    if(transform.empty()||!cv::checkRange(transform)||std::abs(cv::determinant(transform))<1e-15)return fail("ROI变换退化。");
    const cv::Mat inverse=(transform*pixelToMm).inv();
    // Check complete physical boundaries, not just pixel centers; never pad a missing ROI.
    double sign=0;
    for(int iy=0;iy<=20;++iy)for(int ix=0;ix<=20;++ix){
        cv::Point2d p;if(!project(inverse,{ix*15.,iy*15.},p)||p.x<-.5-1e-6||p.y<-.5-1e-6||p.x>pixels.width-.5+1e-6||p.y>pixels.height-.5+1e-6)
            return fail("完整300×300 mm区域超出基础透视图片，请增大基础范围或调整参考框。");
        const auto* a=inverse.ptr<double>();double w=a[6]*ix*15+a[7]*iy*15+a[8];if(sign==0)sign=w;if(w*sign<=0)return fail("ROI映射发生翻转。");
    }
    const double n=std::round(300*pixelsPerMm);
    if(!std::isfinite(n)||n<1||n>=32767||n*n*12>512.*1024*1024)return fail("ROI输出尺寸或内存超限，请降低像素密度。");
    enabled=true;config=requested;baseSize=pixels;baseSizeMm=mm;density=pixelsPerMm;outputSize={int(n),int(n)};
    basePixelToMm=pixelToMm.clone();baseMmToRoiMm=transform;binding=baseBinding;return true;
}
bool PostPerspectiveRoi::apply(const cv::Mat& source,cv::Mat& output)const{
    const cv::Mat input=source;output.release();if(!enabled){output=input;return !output.empty();}
    if(input.empty()||input.size()!=baseSize||input.depth()!=CV_8U||(input.channels()!=1&&input.channels()!=3&&input.channels()!=4))return false;
    try{cv::Mat scale=(cv::Mat_<double>(3,3)<<outputSize.width/300.,0,-.5,0,outputSize.height/300.,-.5,0,0,1);
        cv::warpPerspective(input,output,scale*baseMmToRoiMm*basePixelToMm,outputSize,cv::INTER_LINEAR,cv::BORDER_REPLICATE);return !output.empty();
    }catch(const cv::Exception&){output.release();return false;}
}
bool PostPerspectiveRoi::map(cv::Point2d& p)const{
    if(!enabled)return finite(p);cv::Point2d q;
    if(!project(baseMmToRoiMm,p,q)||q.x<-1e-7||q.y<-1e-7||q.x>300+1e-7||q.y>300+1e-7)return false;
    p={std::clamp(q.x,0.,300.),std::clamp(q.y,0.,300.)};return true;
}
QJsonObject PostPerspectiveRoi::report()const{
    QJsonArray corners;for(auto p:config.corners)corners.append(QJsonArray{p.x(),p.y()});
    return {{"enabled",enabled},{"corners",corners},{"reference_width_mm",config.referenceSizeMm.width()},
        {"reference_height_mm",config.referenceSizeMm.height()},{"target_width_mm",300},{"target_height_mm",300},
        {"output_width_px",outputSize.width},{"output_height_px",outputSize.height},{"physical_origin","roi_top_left_boundary"}};
}
void PostPerspectiveRoi::write(cv::FileStorage& fs)const{
    if(!enabled)return;
    cv::Mat corners(4,2,CV_64F);for(int i=0;i<4;++i){corners.at<double>(i,0)=config.corners[i].x();corners.at<double>(i,1)=config.corners[i].y();}
    fs<<"post_perspective_roi"<<"{"<<"version"<<1<<"binding"<<binding.toStdString()<<"base_width_px"<<baseSize.width<<"base_height_px"<<baseSize.height
      <<"base_width_mm"<<baseSizeMm.width<<"base_height_mm"<<baseSizeMm.height<<"pixels_per_mm"<<density
      <<"reference_width_mm"<<config.referenceSizeMm.width()<<"reference_height_mm"<<config.referenceSizeMm.height()
      <<"target_width_mm"<<300.<<"target_height_mm"<<300.<<"pixel_center_convention"<<"base_matrix_to_half_pixel_output"
      <<"corners_px"<<corners<<"base_pixel_to_mm"<<basePixelToMm<<"base_mm_to_roi_mm"<<baseMmToRoiMm<<"}";
}
bool PostPerspectiveRoi::read(const cv::FileNode& node,cv::Size px,cv::Size2d mm,double ppm,const cv::Mat& b,const QString& key,QString& error){
    *this={};if(node.isNone())return true;
    auto fail=[&]{error="ROI数据损坏、版本不支持或与基础透视参数不匹配。";return false;};
    try{
        if(!node.isMap()||!node["version"].isInt()||int(node["version"])!=1||!node["binding"].isString()
          ||QString::fromStdString(std::string(node["binding"]))!=key||int(node["base_width_px"])!=px.width||int(node["base_height_px"])!=px.height
          ||std::string(node["pixel_center_convention"])!="base_matrix_to_half_pixel_output")return fail();
        for(auto pair:{std::make_pair("base_width_mm",mm.width),{"base_height_mm",mm.height},{"pixels_per_mm",ppm},{"target_width_mm",300.},{"target_height_mm",300.}}){
            auto n=node[pair.first];if((!n.isReal()&&!n.isInt())||!std::isfinite(double(n))||std::abs(double(n)-pair.second)>1e-8*std::max(1.,std::abs(pair.second)))return fail();
        }
        cv::Mat corners,savedB,savedM;node["corners_px"]>>corners;node["base_pixel_to_mm"]>>savedB;node["base_mm_to_roi_mm"]>>savedM;
        if(corners.type()!=CV_64FC1||corners.size()!=cv::Size(2,4)||savedB.type()!=CV_64FC1||savedB.size()!=cv::Size(3,3)||!cv::checkRange(savedB)
          ||savedM.type()!=CV_64FC1||savedM.size()!=cv::Size(3,3)||!cv::checkRange(savedM)||cv::norm(savedB-b)>1e-8)return fail();
        PostPerspectiveRoiConfig spec;spec.referenceSizeMm={double(node["reference_width_mm"]),double(node["reference_height_mm"])};
        for(int i=0;i<4;++i)spec.corners.append({corners.at<double>(i,0),corners.at<double>(i,1)});
        PostPerspectiveRoi candidate;if(!candidate.configure(spec,px,mm,ppm,b,key,error))return false;
        if(std::abs(savedM.at<double>(2,2))<1e-12||cv::norm(savedM/savedM.at<double>(2,2)-candidate.baseMmToRoiMm/candidate.baseMmToRoiMm.at<double>(2,2))>1e-6)return fail();
        *this=std::move(candidate);return true;
    }catch(const std::exception&){return fail();}
}
}

bool DetectPostPerspectiveRectangles(const cv::Mat& base,const QSizeF& mm,const QSizeF& reference,QVector<PostPerspectiveRoiCandidate>& result,QString& error){
    result.clear();error.clear();
    if(!CanExtractPostPerspectiveRoi(mm.width(),mm.height())||base.empty()||base.depth()!=CV_8U||reference.width()<=0||reference.height()<=0){error="需要宽高均大于300 mm的有效基础透视图。";return false;}
    try{
        const double scale=std::min(1.,1600./std::max(base.cols,base.rows));cv::Mat gray,work;
        if(base.channels()==1)gray=base;else if(base.channels()==3)cv::cvtColor(base,gray,cv::COLOR_BGR2GRAY);else if(base.channels()==4)cv::cvtColor(base,gray,cv::COLOR_BGRA2GRAY);else return false;
        cv::resize(gray,work,{},scale,scale,cv::INTER_AREA);cv::GaussianBlur(work,work,{3,3},.6);
        cv::Mat edges;cv::Canny(work,edges,35,100);cv::Mat distance;cv::distanceTransform(255-edges,distance,cv::DIST_L2,3);
        auto add=[&](std::vector<cv::Point2f> p){
            auto corners=ordered(p);p=points(corners);if(!cv::isContourConvex(p)||cv::contourArea(p)<100)return;
            double width=(cv::norm(p[1]-p[0])+cv::norm(p[2]-p[3]))*.5/work.cols*mm.width();
            double height=(cv::norm(p[3]-p[0])+cv::norm(p[2]-p[1]))*.5/work.rows*mm.height();
            // The reference frame is precisely what corrects a wrong base scale. Do not
            // reject it merely because the old calibration is off by more than 15%.
            if(width<reference.width()*.4||height<reference.height()*.4||width>reference.width()*2.5||height>reference.height()*2.5)return;
            double support=0;for(int e=0;e<4;++e){int hits=0;for(int n=0;n<=100;++n){auto q=p[e]+(p[(e+1)%4]-p[e])*(n/100.f);
                if(q.x<1||q.y<1||q.x>=work.cols-1||q.y>=work.rows-1)return;if(distance.at<float>(cvRound(q.y),cvRound(q.x))<=3)++hits;}
                if(hits<60)return;
                for(bool end:{false,true}){int cornerHits=0;for(float t:{.01f,.02f,.03f}){float f=end?1-t:t;auto q=p[e]+(p[(e+1)%4]-p[e])*f;
                    if(distance.at<float>(cvRound(q.y),cvRound(q.x))<=3)++cornerHits;}if(cornerHits<2)return;}
                support+=hits/101.;}
            for(auto& q:corners)q/=scale;
            for(auto existing:result){double delta=0;for(int i=0;i<4;++i)delta+=std::hypot(existing.corners[i].x()-corners[i].x(),existing.corners[i].y()-corners[i].y());if(delta<32/scale)return;}
            PostPerspectiveRoiCandidate candidate;candidate.corners=corners;candidate.areaPixels=cv::contourArea(p)/(scale*scale);candidate.edgeSupport=support/4;result.append(candidate);
        };
        // Closed contours give accurate quadrilaterals when all four strokes are visible.
        for(int threshold:{70,110,150,190}){
            cv::Mat mask;cv::threshold(work,mask,threshold,255,cv::THRESH_BINARY);cv::morphologyEx(mask,mask,cv::MORPH_CLOSE,cv::getStructuringElement(cv::MORPH_RECT,{5,5}));
            std::vector<std::vector<cv::Point>> contours;cv::findContours(mask,contours,cv::RETR_LIST,cv::CHAIN_APPROX_SIMPLE);
            for(const auto& c:contours){if(cv::arcLength(c,true)<200)continue;std::vector<cv::Point> q;cv::approxPolyDP(c,q,.008*cv::arcLength(c,true),true);if(q.size()==4){std::vector<cv::Point2f> p;for(auto v:q)p.emplace_back(float(v.x),float(v.y));add(p);}}
        }
        const auto contourCandidates=result;result.clear();
        // Long-line grouping bridges reflections and merges the two edges of a bright stroke.
        struct Line {std::vector<cv::Point2f> endpoints;cv::Vec4f fit;double length=0;};std::vector<Line> groups;
        std::vector<cv::Vec4f> segments;cv::createLineSegmentDetector(cv::LSD_REFINE_STD)->detect(work,segments);
        std::sort(segments.begin(),segments.end(),[](auto a,auto b){return std::hypot(a[0]-a[2],a[1]-a[3])>std::hypot(b[0]-b[2],b[1]-b[3]);});
        for(auto s:segments){cv::Point2f a(s[0],s[1]),b(s[2],s[3]);double length=cv::norm(a-b);if(length<std::min(work.cols,work.rows)*.055)continue;
            cv::Point2f d=(b-a)*(float(1/length));bool merged=false;
            for(auto& g:groups){cv::Point2f v(g.fit[0],g.fit[1]),origin(g.fit[2],g.fit[3]);
                if(std::abs(d.dot(v))>.998&&std::abs((a-origin).cross(v))<4&&std::abs((b-origin).cross(v))<4){g.endpoints.push_back(a);g.endpoints.push_back(b);g.length+=length;cv::fitLine(g.endpoints,g.fit,cv::DIST_HUBER,0,.01,.01);merged=true;break;}}
            if(!merged){Line l;l.endpoints={a,b};l.fit={d.x,d.y,a.x,a.y};l.length=length;groups.push_back(l);}
        }
        std::sort(groups.begin(),groups.end(),[](const auto&a,const auto&b){return a.length>b.length;});if(groups.size()>32)groups.resize(32);
        std::vector<Line> horizontal,vertical;for(auto g:groups){if(std::abs(g.fit[0])>.9&&horizontal.size()<12)horizontal.push_back(g);else if(std::abs(g.fit[1])>.9&&vertical.size()<12)vertical.push_back(g);}
        auto intersect=[](const Line&a,const Line&b,cv::Point2f& p){cv::Point2f av(a.fit[0],a.fit[1]),bv(b.fit[0],b.fit[1]),o(a.fit[2],a.fit[3]),d(b.fit[2]-o.x,b.fit[3]-o.y);double cross=av.cross(bv);if(std::abs(cross)<.5)return false;p=o+av*float(d.cross(bv)/cross);return true;};
        for(size_t a=0;a<horizontal.size();++a)for(size_t b=a+1;b<horizontal.size();++b)
            for(size_t c=0;c<vertical.size();++c)for(size_t d=c+1;d<vertical.size();++d){std::vector<cv::Point2f> p(4);
                if(intersect(horizontal[a],vertical[c],p[0])&&intersect(horizontal[a],vertical[d],p[1])&&intersect(horizontal[b],vertical[d],p[2])&&intersect(horizontal[b],vertical[c],p[3]))add(p);}
        for(const auto& c:contourCandidates){bool duplicate=false;for(const auto& current:result){double delta=0;for(int i=0;i<4;++i)delta+=std::hypot(current.corners[i].x()-c.corners[i].x(),current.corners[i].y()-c.corners[i].y());if(delta<32/scale){duplicate=true;break;}}if(!duplicate)result.append(c);}
        std::sort(result.begin(),result.end(),[](const auto&a,const auto&b){return a.areaPixels>b.areaPixels;});
        if(result.isEmpty()){error="未找到尺寸和四边支持度可靠的矩形框，请手动选四角。";return false;}return true;
    }catch(const cv::Exception& e){error=QString::fromUtf8(e.what());return false;}
}
