#include "BoardDetection.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <set>
#include <map>

namespace calibration {
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
double median(std::vector<double> values) {
    if (values.empty()) return 0;
    auto mid = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), mid, values.end());
    return *mid;
}
std::vector<cv::Point2f> grid(cv::Size size, int x = 0, int y = 0) {
    std::vector<cv::Point2f> points;
    for (int r = 0; r < size.height; ++r)
        for (int c = 0; c < size.width; ++c) points.emplace_back(float(c + x), float(r + y));
    return points;
}
void rawCoordinates(std::vector<cv::Point2f>& points, cv::Size raw, cv::Size work) {
    // resize uses pixel centres, and the two rounded dimensions can have different scales.
    for (auto& p : points) {
        p.x = float((p.x + 0.5) * raw.width / work.width - 0.5);
        p.y = float((p.y + 0.5) * raw.height / work.height - 0.5);
    }
}
bool inside(const std::vector<cv::Point2f>& points, cv::Size size) {
    return std::all_of(points.begin(), points.end(), [&](auto p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && p.x >= 1 && p.y >= 1
            && p.x < size.width - 1 && p.y < size.height - 1;
    });
}
double spacing(const std::vector<cv::Point2f>& points, cv::Size size) {
    std::vector<double> distances;
    for (int r = 0; r < size.height; ++r) for (int c = 0; c < size.width; ++c) {
        int i = r * size.width + c;
        if (c + 1 < size.width) distances.push_back(cv::norm(points[i+1]-points[i]));
        if (r + 1 < size.height) distances.push_back(cv::norm(points[i+size.width]-points[i]));
    }
    return median(distances);
}
void refine(const cv::Mat& gray, std::vector<cv::Point2f>& points, double pitch,
            int limit, BoardObservation& result) {
    auto start = Clock::now();
    int window = std::min(limit, std::max(2, int(pitch * 0.2)));
    cv::cornerSubPix(gray, points, {window, window}, {-1,-1},
        {cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 80, 0.001});
    result.refinementMs += elapsed(start);
}
double checkerSupport(const cv::Mat& gray, cv::Point2f p, cv::Point2f u, cv::Point2f v) {
    double values[4];
    const int signs[4][2] = {{-1,-1},{1,-1},{-1,1},{1,1}};
    int patchSize = std::clamp(int(std::min(cv::norm(u), cv::norm(v)) * 0.05) | 1, 3, 9);
    for (int i = 0; i < 4; ++i) {
        auto centre = p + (u * float(signs[i][0]) + v * float(signs[i][1])) * 0.18f;
        if (centre.x < 0 || centre.y < 0 || centre.x >= gray.cols || centre.y >= gray.rows) return -255;
        cv::Mat patch;
        cv::getRectSubPix(gray, {patchSize, patchSize}, centre, patch);
        values[i] = cv::mean(patch)[0];
    }
    double sign = values[0]+values[3] > values[1]+values[2] ? 1 : -1;
    return std::min({sign*(values[0]-values[1]), sign*(values[0]-values[2]),
                     sign*(values[3]-values[1]), sign*(values[3]-values[2])});
}
bool validateGrid(const cv::Mat& gray, const std::vector<cv::Point2f>& points, cv::Size size) {
    if (int(points.size()) != size.area() || !inside(points, gray.size())) return false;
    double pitch = spacing(points, size);
    if (!(pitch >= 5)) return false;
    double orientation = 0;
    for (int r=0;r<size.height;++r) for(int c=0;c<size.width;++c) {
        int i=r*size.width+c;
        auto u = c+1<size.width ? points[i+1]-points[i] : points[i]-points[i-1];
        auto v = r+1<size.height ? points[i+size.width]-points[i] : points[i]-points[i-size.width];
        double cross = u.x*v.y-u.y*v.x;
        if (std::abs(cross) < pitch*pitch*0.1) return false;
        if (orientation == 0) orientation = cross;
        if (cross*orientation <= 0 || checkerSupport(gray,points[i],u,v)<20) return false;
        for (int j=0;j<i;++j) if(cv::norm(points[i]-points[j]) < pitch*0.25) return false;
    }
    return true;
}
void canonicalize(std::vector<cv::Point2f>& points, cv::Size size) {
    auto original=points;
    double best=-1e9;
    for(int transpose=0;transpose<=(size.width==size.height?1:0);++transpose)
        for(int fx=0;fx<2;++fx) for(int fy=0;fy<2;++fy) {
            auto candidate=original;
            for(int r=0;r<size.height;++r) for(int c=0;c<size.width;++c) {
                int x=fx?size.width-1-c:c, y=fy?size.height-1-r:r;
                candidate[r*size.width+c]=original[transpose?x*size.width+y:y*size.width+x];
            }
            auto u=candidate[size.width-1]-candidate[0];
            auto v=candidate[(size.height-1)*size.width]-candidate[0];
            double score=u.x/cv::norm(u)+v.y/cv::norm(v);
            if(score>best){best=score;points=std::move(candidate);}
        }
}
bool largerGrid(const cv::Mat& gray, const std::vector<cv::Point2f>& points, cv::Size size) {
    // Reject an inner sub-array of a larger board. Search just outside each boundary.
    cv::Mat H=cv::findHomography(grid(size),points,0);
    if(H.empty()) return true;
    for(int edge=0;edge<4;++edge) {
        int count=edge<2?size.width:size.height, supported=0;
        for(int i=0;i<count;++i) {
            cv::Point2f q = edge==0?cv::Point2f(float(i),-1):edge==1?cv::Point2f(float(i),float(size.height)):
                edge==2?cv::Point2f(-1,float(i)):cv::Point2f(float(size.width),float(i));
            std::vector<cv::Point2f> src{q,q+cv::Point2f(1,0),q+cv::Point2f(0,1)}, dst;
            cv::perspectiveTransform(src,dst,H);
            if(checkerSupport(gray,dst[0],dst[1]-dst[0],dst[2]-dst[0])>=20) ++supported;
        }
        if(supported*2>count) return true;
    }
    return false;
}
bool accept(const cv::Mat& gray, std::vector<cv::Point2f>& points, cv::Size size,
            BoardObservation& result, bool sbSeed = false) {
    if(!inside(points,gray.size())) return false;
    double pitch=spacing(points,size);
    const auto initial=points;
    if(sbSeed) refine(gray,points,pitch,9,result);
    // Search real image features before subpixel refinement. A homography seed
    // can be several pixels off under lens distortion; cornerSubPix alone may
    // then converge onto a scratch or a single edge instead of the saddle.
    if(pitch>=16){
        int radius=std::min(32,std::max(4,int(pitch*0.2)));
        for(int r=0;r<size.height;++r)for(int c=0;c<size.width;++c){
            int i=r*size.width+c;
            auto u=c+1<size.width?initial[i+1]-initial[i]:initial[i]-initial[i-1];
            auto v=r+1<size.height?initial[i+size.width]-initial[i]:initial[i]-initial[i-size.width];
            if(sbSeed && inside({points[i]},gray.size()) && checkerSupport(gray,points[i],u,v)>=20)continue;
            cv::Rect roi(int(initial[i].x)-radius,int(initial[i].y)-radius,radius*2+1,radius*2+1);
            roi&=cv::Rect(0,0,gray.cols,gray.rows);
            std::vector<cv::Point2f> features;
            cv::goodFeaturesToTrack(gray(roi),features,12,0.01,3,cv::noArray(),5);
            for(auto& feature:features)feature+=cv::Point2f(float(roi.x),float(roi.y));
            features.erase(std::remove_if(features.begin(),features.end(),[&](auto p){return checkerSupport(gray,p,u,v)<20;}),features.end());
            if(!features.empty())refine(gray,features,pitch,9,result);
            std::stable_sort(features.begin(),features.end(),[&](auto a,auto b){return cv::norm(a-initial[i])<cv::norm(b-initial[i]);});
            bool found=false;
            for(auto feature:features){
                if(inside({feature},gray.size())&&checkerSupport(gray,feature,u,v)>=20
                    &&checkerSupport(gray,feature,u/3,v/3)>=20){points[i]=feature;found=true;break;}
            }
            if(!found)return false;
        }
    }
    if(!sbSeed)refine(gray,points,pitch,9,result);
    if(!validateGrid(gray,points,size) || largerGrid(gray,points,size)) return false;
    canonicalize(points,size);
    return true;
}
bool sameGrid(const std::vector<cv::Point2f>& a,const std::vector<cv::Point2f>& b,cv::Size size) {
    if(a.size()!=b.size())return false;
    // Transposed seeds can converge differently within the same corner patch.
    // Distinct grid extensions differ by an entire cell, not a few pixels.
    double tolerance=std::min(3.0,spacing(a,size)*0.05);
    for(size_t i=0;i<a.size();++i)if(cv::norm(a[i]-b[i])>tolerance)return false;
    return true;
}
bool validSparseGrid(const std::vector<cv::Point2f>& points,const std::vector<int>& ids,int columns){
    if(points.size()<4||points.size()!=ids.size())return false;
    std::vector<cv::Point2f> hull;cv::convexHull(points,hull);
    if(hull.size()<3||cv::contourArea(hull)<1)return false;
    std::map<int,size_t> byId;for(size_t i=0;i<ids.size();++i)byId[ids[i]]=i;
    std::vector<double> distances[2];double orientation=0;
    for(auto entry:byId){
        auto right=entry.first%columns==columns-1?byId.end():byId.find(entry.first+1);
        auto down=byId.find(entry.first+columns);
        if(right!=byId.end())distances[0].push_back(cv::norm(points[right->second]-points[entry.second]));
        if(down!=byId.end())distances[1].push_back(cv::norm(points[down->second]-points[entry.second]));
        if(right!=byId.end()&&down!=byId.end()){
            auto u=points[right->second]-points[entry.second],v=points[down->second]-points[entry.second];
            double cross=u.x*v.y-u.y*v.x;
            if(std::abs(cross)<1)return false;
            if(orientation==0)orientation=cross;
            if(orientation*cross<=0)return false;
        }
    }
    for(auto& values:distances){double middle=median(values);for(double d:values)if(d<5||d<middle*0.2||d>middle*3)return false;}
    return true;
}
bool guided(const cv::Mat& gray, std::vector<cv::Point2f> seed, cv::Size seedSize,
            cv::Size target, BoardObservation& result, std::vector<cv::Point2f>& output, bool& ambiguous) {
    std::vector<std::vector<cv::Point2f>> valid;
    if(seedSize==target){
        // SB can join an exterior black-square boundary and omit the opposite
        // inner line. Remove each suspect boundary and revalidate both sides.
        for(int edge=0;edge<4;++edge){
            cv::Size shape(target.width-(edge<2),target.height-(edge>=2));
            std::vector<cv::Point2f> trimmed;
            for(int r=0;r<shape.height;++r)for(int c=0;c<shape.width;++c)
                trimmed.push_back(seed[(r+(edge==2))*target.width+c+(edge==0)]);
            std::vector<cv::Point2f> candidate;bool multiple=false;
            if(guided(gray,trimmed,shape,target,result,candidate,multiple)
                &&std::none_of(valid.begin(),valid.end(),[&](const auto& p){return sameGrid(p,candidate,target);}))
                valid.push_back(std::move(candidate));
            if(multiple){ambiguous=true;return false;}
        }
        ambiguous=valid.size()>1;
        if(valid.size()!=1)return false;
        output=std::move(valid[0]);return true;
    }
    for(int transpose=0;transpose<2;++transpose) {
        cv::Size shape=transpose?cv::Size(seedSize.height,seedSize.width):seedSize;
        if(!((shape.width==target.width-1 && shape.height==target.height) ||
             (shape.height==target.height-1 && shape.width==target.width)))continue;
        std::vector<cv::Point2f> ordered(seed.size());
        for(int r=0;r<shape.height;++r)for(int c=0;c<shape.width;++c)
            ordered[r*shape.width+c]=seed[transpose?c*seedSize.width+r:r*seedSize.width+c];
        if(!inside(ordered,gray.size()))continue;
        refine(gray,ordered,spacing(ordered,shape),9,result);
        cv::Mat H=cv::findHomography(grid(shape),ordered,cv::RANSAC,3);
        if(H.empty())continue;
        for(int side:{-1,0}) {
            auto ideal=grid(target,shape.width<target.width?side:0,shape.height<target.height?side:0);
            std::vector<cv::Point2f> candidate;
            cv::perspectiveTransform(ideal,candidate,H);
            if(!accept(gray,candidate,target,result)){++result.rejectedGuides;continue;}
            if(std::none_of(valid.begin(),valid.end(),[&](const auto& p){return sameGrid(p,candidate,target);}))
                valid.push_back(std::move(candidate));
        }
    }
    ambiguous=valid.size()>1;
    if(valid.size()!=1)return false;
    output=std::move(valid[0]);return true;
}
}
bool BoardSpec::valid() const {
    int minimum=type==BoardType::Charuco?3:4;
    if(squares.width<minimum || squares.height<minimum || int64_t(squares.width)*squares.height>100000
        || !std::isfinite(squareMm) || squareMm<=0)return false;
    if(type==BoardType::Chessboard)return true;
    if(dictionaryId<0||dictionaryId>20||!std::isfinite(markerMm)||markerMm<=0||markerMm>=squareMm
       ||minimumMarkers<0||minimumCharucoCorners<4||interpolateMinMarkers<1||interpolateMinMarkers>2)return false;
    const auto dictionary=cv::aruco::getPredefinedDictionary(dictionaryId);
    int count=squares.area()/2;
    if(count>dictionary.bytesList.rows||minimumMarkers>count
       ||minimumCharucoCorners>innerSize().area())return false;
    if(!markerIds.empty()){
        if(int(markerIds.size())!=count)return false;
        std::set<int> ids;
        for(int id:markerIds)if(id<0||id>=dictionary.bytesList.rows||!ids.insert(id).second)return false;
    }
    if(!measuredCorners.empty()){
        if(int(measuredCorners.size())!=innerSize().area())return false;
        for(auto p:measuredCorners)if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.z!=0)return false;
    }
    return true;
}
bool detectBoard(const cv::Mat& image,const BoardSpec& spec,BoardObservation& result) {
    result={};auto start=Clock::now();
    auto finish=[&](bool ok){result.milliseconds=elapsed(start);if(!ok){result.pixels.clear();result.ids.clear();result.objectPoints.clear();}return ok;};
    if(!spec.valid() || image.empty() || image.dims!=2 || image.depth()!=CV_8U
       || (image.channels()!=1 && image.channels()!=3 && image.channels()!=4)){
        result.error="invalid board/image";return finish(false);
    }
    try {
        cv::Mat gray;
        if(image.channels()==1)gray=image;
        else cv::cvtColor(image,gray,image.channels()==4?cv::COLOR_BGRA2GRAY:cv::COLOR_BGR2GRAY);
        cv::Size previous;
        std::vector<int> limits{1400,2100};
        if(spec.type==BoardType::Charuco)limits.push_back(std::max(image.cols,image.rows));
        for(int limit:limits) {
            double scale=std::min(1.0,double(limit)/std::max(image.cols,image.rows));
            cv::Size size(std::max(1,int(std::round(image.cols*scale))),std::max(1,int(std::round(image.rows*scale))));
            if(size==previous)continue;previous=size;
            cv::Mat work;
            if(size==gray.size())work=gray;else cv::resize(gray,work,size,0,0,cv::INTER_AREA);
            if(spec.type==BoardType::Charuco) {
                auto t=Clock::now();
                auto dictionary=cv::aruco::getPredefinedDictionary(spec.dictionaryId);
                cv::aruco::CharucoBoard board(spec.squares,float(spec.squareMm),float(spec.markerMm),dictionary,spec.markerIds);
                cv::aruco::CharucoParameters parameters;parameters.minMarkers=spec.interpolateMinMarkers;
                cv::aruco::CharucoDetector detector(board,parameters);
                std::vector<std::vector<cv::Point2f>> markers;std::vector<int> markerIds,ids;
                std::vector<cv::Point2f> corners;
                detector.detectBoard(work,corners,ids,markers,markerIds);
                const auto boardIds=board.getIds();
                const std::set<int> allowed(boardIds.begin(),boardIds.end());
                std::set<int> present;bool conflict=false;
                for(int id:markerIds)if(allowed.count(id)&&!present.insert(id).second)conflict=true;
                result.markerCount=int(present.size());
                if(conflict){result.error="conflicting duplicate board marker ID";return finish(false);}
                bool sufficient=int(ids.size())>=spec.minimumCharucoCorners&&result.markerCount>=spec.minimumMarkers;
                result.attempts.push_back({"charuco",size,elapsed(t),sufficient});
                if(!sufficient)continue;
                rawCoordinates(corners,gray.size(),size);
                if(!inside(corners,gray.size()))continue;
                // Coding IDs supply correspondence; local interpolation/refinement supplies pixels.
                std::map<int,size_t> positions;for(size_t i=0;i<ids.size();++i)positions[ids[i]]=i;
                std::vector<double> pitches;
                for(const auto& entry:positions)for(int offset:{1,spec.innerSize().width}){
                    if(offset==1&&entry.first%spec.innerSize().width==spec.innerSize().width-1)continue;
                    auto next=positions.find(entry.first+offset);
                    if(next!=positions.end())pitches.push_back(cv::norm(corners[entry.second]-corners[next->second]));
                }
                const double pitch=pitches.empty()?std::min(double(image.cols)/spec.squares.width,double(image.rows)/spec.squares.height):median(pitches);
                // Keep the subpixel window inside the checker region, away from marker bits.
                const int checkerWindow=int(std::floor(pitch*.5*(1-spec.markerMm/spec.squareMm)*.7));
                if(checkerWindow>=2)refine(gray,corners,pitch,std::min(9,checkerWindow),result);
                const auto model=spec.measuredCorners.empty()?board.getChessboardCorners():spec.measuredCorners;std::set<int> unique;
                bool good=true;
                for(int id:ids)if(id<0 || id>=int(model.size()) || !unique.insert(id).second)good=false;
                if(!good||!inside(corners,gray.size())||!validSparseGrid(corners,ids,spec.innerSize().width))continue;
                result.pixels=std::move(corners);result.ids=std::move(ids);
                for(int id:result.ids)result.objectPoints.push_back(model[id]);
                result.method="charuco";return finish(true);
            }
            auto attempt=[&](const cv::Mat& input,const std::string& name,bool partial,std::vector<cv::Point2f>& points,cv::Mat& meta){
                auto t=Clock::now();
                auto pattern=spec.innerSize();
                if(partial)pattern={std::min(5,pattern.width),std::min(5,pattern.height)};
                int flags=cv::CALIB_CB_NORMALIZE_IMAGE|cv::CALIB_CB_EXHAUSTIVE|(partial?cv::CALIB_CB_LARGER:0);
                bool found=cv::findChessboardCornersSB(input,pattern,points,flags,meta);
                result.attempts.push_back({name,size,elapsed(t),found});
                return found;
            };
            for(int pass=0;pass<3;++pass) {
                if(pass==1 && image.channels()==1)continue;
                cv::Mat input,blue,meta;
                if(pass==1){cv::extractChannel(image,blue,0);if(size!=gray.size())cv::resize(blue,input,size,0,0,cv::INTER_AREA);else input=blue;}
                else input=work;
                std::vector<cv::Point2f> points;
                std::string name=pass==0?"gray-SB":pass==1?"blue-SB":"guided-SB";
                if(!attempt(input,name,pass==2,points,meta))continue;
                rawCoordinates(points,gray.size(),size);
                bool ok=false,ambiguous=false;
                if(pass<2)ok=accept(gray,points,spec.innerSize(),result,true);
                else {
                    auto initial=points;
                    if(meta.size()==spec.innerSize())ok=accept(gray,points,spec.innerSize(),result,true);
                    if(!ok)ok=guided(gray,initial,meta.size(),spec.innerSize(),result,points,ambiguous);
                }
                if(ambiguous){result.error="ambiguous grid extension";return finish(false);}
                if(!ok)continue;
                result.pixels=std::move(points);result.method=name;
                for(int r=0;r<spec.innerSize().height;++r)for(int c=0;c<spec.innerSize().width;++c){
                    result.ids.push_back(r*spec.innerSize().width+c);
                    result.objectPoints.emplace_back(float((c+1)*spec.squareMm),float((r+1)*spec.squareMm),0.f);
                }
                return finish(true);
            }
        }
        result.error="no unambiguous board with image-supported corners";
    }catch(const cv::Exception& e){result.error=e.what();}
    return finish(false);
}
}
