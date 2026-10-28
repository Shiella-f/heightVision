#include "BoardConfiguration.h"
#include <QJsonArray>
#include <QStringList>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
namespace calibration {
namespace {
const QStringList dictionaries={"DICT_4X4_50","DICT_4X4_100","DICT_4X4_250","DICT_4X4_1000",
    "DICT_5X5_50","DICT_5X5_100","DICT_5X5_250","DICT_5X5_1000","DICT_6X6_50","DICT_6X6_100","DICT_6X6_250","DICT_6X6_1000",
    "DICT_7X7_50","DICT_7X7_100","DICT_7X7_250","DICT_7X7_1000","DICT_ARUCO_ORIGINAL",
    "DICT_APRILTAG_16h5","DICT_APRILTAG_25h9","DICT_APRILTAG_36h10","DICT_APRILTAG_36h11"};
bool integer(const QJsonObject& c,const char* key,int fallback,int& result) {
    auto v=c.value(key);if(v.isUndefined()){result=fallback;return true;}
    if(!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()!=v.toInt(-2147483647))return false;
    result=v.toInt();return true;
}
}
QJsonObject DefaultBoardConfiguration(){
    return {{"board_type","charuco"},{"dictionary","DICT_5X5_100"},{"squares_x",12},{"squares_y",9},
        {"square_length",30},{"marker_length",22.5},{"min_marker_id",0},{"min_valid_marker_num",7},
        {"min_valid_corner_num",8},{"interpolate_min_markers",2},{"board_thickness_mm",3.2},
        {"board_margin_x_mm",20},{"board_margin_y_mm",15},{"board_size_mm",QJsonArray{400,300}},
        {"note",QString::fromUtf8("Step06 新标定板：12x9 squares（400x300mm），checker 30mm/marker 22.5mm，X/Y 物理边缘留白 20/15mm，板厚 3.2mm，DICT_5X5_100，marker ID 从 0 连续编号")}};
}
bool parseBoardConfiguration(const QJsonObject& c,BoardSpec& spec,QString& error,QJsonObject* normalized){
    spec={};error.clear();auto reject=[&](const QString& text){error=text;return false;};
    const auto type=c.value("board_type").toString("charuco");
    if(type!="charuco"&&type!="chessboard")return reject("board_type 须为 charuco 或 chessboard。");
    spec.type=type=="charuco"?BoardType::Charuco:BoardType::Chessboard;
    if(!integer(c,"squares_x",0,spec.squares.width)||!integer(c,"squares_y",0,spec.squares.height))return reject("整板行列必须为整数。");
    spec.squareMm=c.value("square_length").toDouble();
    QJsonObject result=c;result["board_type"]=type;
    if(spec.type==BoardType::Charuco){
        spec.markerMm=c.value("marker_length").toDouble();
        spec.dictionaryId=c.value("dictionary").isString()?dictionaries.indexOf(c.value("dictionary").toString()):c.value("dictionary_id").toInt(-1);
        if(!integer(c,"min_valid_marker_num",7,spec.minimumMarkers)||!integer(c,"min_valid_corner_num",8,spec.minimumCharucoCorners)
            ||!integer(c,"interpolate_min_markers",2,spec.interpolateMinMarkers))return reject("检测门槛必须为整数。");
        int first=0;if(!integer(c,"min_marker_id",0,first)||first<0)return reject("min_marker_id 须为非负整数。");
        if(c.contains("marker_ids")){
            if(!c["marker_ids"].isArray())return reject("marker_ids 必须为整数数组。");
            for(auto id:c["marker_ids"].toArray()){
                if(!id.isDouble()||id.toDouble()!=id.toInt(-1))return reject("标记 ID 必须为整数。");
                spec.markerIds.push_back(id.toInt());
            }
            if(c.contains("min_marker_id")&&!spec.markerIds.empty()&&*std::min_element(spec.markerIds.begin(),spec.markerIds.end())!=first)
                return reject("min_marker_id 与显式 marker_ids 的最小 ID 不一致。");
        }else{
            const int64_t count=int64_t(spec.squares.width)*spec.squares.height/2;
            if(count<0||count>100000||int64_t(first)+count>2147483647)return reject("板尺寸或 ID 范围超限。");
            for(int i=0;i<count;++i)spec.markerIds.push_back(first+i);
        }
        if(c.contains("corner_points_mm")){
            if(!c["corner_points_mm"].isArray())return reject("实测角点必须为完整数组。");
            for(auto v:c["corner_points_mm"].toArray()){
                const auto p=v.toArray();if(p.size()!=2||!p[0].isDouble()||!p[1].isDouble())return reject("实测角点须为 [X,Y]。");
                spec.measuredCorners.emplace_back(float(p[0].toDouble()),float(p[1].toDouble()),0);
            }
        }
        if(c.contains("layout")&&c["layout"].toString()!="opencv_4_7")return reject("不支持此布局，请核对整板 ID 预览。");
        result["dictionary"]=dictionaries.value(spec.dictionaryId);
        result["min_valid_marker_num"]=spec.minimumMarkers;result["min_valid_corner_num"]=spec.minimumCharucoCorners;
        result["interpolate_min_markers"]=spec.interpolateMinMarkers;
    }
    try{if(!spec.valid())return reject("板规格、字典容量、标记 ID、实际尺寸或检测门槛无效。");}
    catch(const cv::Exception& e){return reject(QString::fromUtf8(e.what()));}
    for(const char* key:{"board_margin_x_mm","board_margin_y_mm","board_thickness_mm"})
        if(c.contains(key)&&(!c[key].isDouble()||!std::isfinite(c[key].toDouble())||c[key].toDouble()<0))return reject(QString(key)+" 须为非负有限数。");
    const double w=spec.squares.width*spec.squareMm,h=spec.squares.height*spec.squareMm;
    if(c.contains("board_size_mm")){
        auto s=c["board_size_mm"].toArray();
        if(s.size()!=2||!s[0].isDouble()||!s[1].isDouble()||!std::isfinite(s[0].toDouble())||!std::isfinite(s[1].toDouble())
            ||s[0].toDouble()<w||s[1].toDouble()<h)return reject("板材尺寸不能小于图案。");
        if((c.contains("board_margin_x_mm")&&std::abs(s[0].toDouble()-w-2*c["board_margin_x_mm"].toDouble())>1e-6)
            ||(c.contains("board_margin_y_mm")&&std::abs(s[1].toDouble()-h-2*c["board_margin_y_mm"].toDouble())>1e-6))
            return reject("图案尺寸＋双侧留白与板材外形不一致。");
    }
    if(normalized)*normalized=result;return true;
}
bool NormalizeBoardConfiguration(const QJsonObject& c,QJsonObject& result,QString& error){
    result={};BoardSpec spec;return parseBoardConfiguration(c,spec,error,&result);
}
QJsonObject observationReport(const BoardObservation& o){
    QJsonArray points,attempts;
    for(size_t i=0;i<o.pixels.size();++i)points.append(QJsonObject{{"id",o.ids[i]},{"x",o.pixels[i].x},{"y",o.pixels[i].y},
        {"object_x_mm",o.objectPoints[i].x},{"object_y_mm",o.objectPoints[i].y},{"inlier",true}});
    for(auto a:o.attempts)attempts.append(QJsonObject{{"method",QString::fromStdString(a.method)},{"milliseconds",a.milliseconds}});
    return {{"points",points},{"point_count",int(o.pixels.size())},{"marker_count",o.markerCount},{"detection_ms",o.milliseconds},
        {"refinement_ms",o.refinementMs},{"attempts",attempts},{"method",QString::fromStdString(o.method)},{"error",QString::fromStdString(o.error)}};
}
bool InspectBoardConfiguration(const cv::Mat& image,const QJsonObject& config,QJsonObject& report){
    BoardSpec spec;QString error;
    if(!parseBoardConfiguration(config,spec,error)){report={{"error",error}};return false;}
    BoardObservation observation;const bool ok=detectBoard(image,spec,observation);report=observationReport(observation);
    report["detected"]=ok;return ok;
}
bool PreviewBoardConfiguration(const QJsonObject& c,cv::Mat& image,QString& error){
    image.release();BoardSpec spec;if(!parseBoardConfiguration(c,spec,error))return false;
    try{
        const double pw=spec.squares.width*spec.squareMm,ph=spec.squares.height*spec.squareMm;
        const auto outer=c["board_size_mm"].toArray();
        const double mx=c["board_margin_x_mm"].toDouble(outer.size()==2?(outer[0].toDouble()-pw)/2:0);
        const double my=c["board_margin_y_mm"].toDouble(outer.size()==2?(outer[1].toDouble()-ph)/2:0);
        const double bw=pw+2*mx,bh=ph+2*my,scale=std::min(1400./bw,1400./bh);
        image=cv::Mat(int(std::ceil(bh*scale))+100,int(std::ceil(bw*scale))+100,CV_8UC3,cv::Scalar(205,205,205));
        const cv::Point origin(50+cvRound(mx*scale),50+cvRound(my*scale));
        const cv::Size patternSize(std::max(10,cvRound(pw*scale)),std::max(10,cvRound(ph*scale)));
        if(spec.type==BoardType::Charuco){
            cv::aruco::CharucoBoard board(spec.squares,float(spec.squareMm),float(spec.markerMm),cv::aruco::getPredefinedDictionary(spec.dictionaryId),spec.markerIds);
            cv::Mat gray,color;board.generateImage(patternSize,gray,0);cv::cvtColor(gray,color,cv::COLOR_GRAY2BGR);color.copyTo(image(cv::Rect(origin,patternSize)));
            const auto ids=board.getIds();const auto corners=board.getObjPoints();
            for(size_t i=0;i<ids.size();++i){cv::Point2d center;for(auto p:corners[i])center+=cv::Point2d(p.x,p.y);center*=.25;
                cv::putText(image,std::to_string(ids[i]),cv::Point2d(origin)+center*scale,cv::FONT_HERSHEY_SIMPLEX,.45,{0,0,255},1,cv::LINE_AA);}
        }else for(int y=0;y<spec.squares.height;++y)for(int x=0;x<spec.squares.width;++x)
            cv::rectangle(image,{origin.x+cvRound(x*spec.squareMm*scale),origin.y+cvRound(y*spec.squareMm*scale),
                cvRound(spec.squareMm*scale),cvRound(spec.squareMm*scale)},(x+y)%2?cv::Scalar::all(255):cv::Scalar::all(0),cv::FILLED);
        cv::rectangle(image,{50,50,cvRound(bw*scale),cvRound(bh*scale)},{255,100,0},2);
        cv::arrowedLine(image,origin,origin+cv::Point(80,0),{0,0,255},2);
        cv::arrowedLine(image,origin,origin+cv::Point(0,80),{255,0,0},2);
        cv::putText(image,"X",origin+cv::Point(85,5),0,.6,{0,0,255},2);cv::putText(image,"Y",origin+cv::Point(0,95),0,.6,{255,0,0},2);
        cv::putText(image,QString("Board %1 x %2 mm; pattern %3 x %4 mm").arg(bw).arg(bh).arg(pw).arg(ph).toStdString(),{20,30},0,.6,{0,0,0},1,cv::LINE_AA);
        return true;
    }catch(const cv::Exception& e){error=QString::fromUtf8(e.what());image.release();return false;}
}
}
