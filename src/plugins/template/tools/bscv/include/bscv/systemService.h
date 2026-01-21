#ifndef BSCV_SYSTEM_SYSYEMSERVICE_H
#define BSCV_SYSTEM_SYSYEMSERVICE_H

#include "opencv2/opencv.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

namespace TIGER_BSVISION
{
	class ISystemService
	{
		//friend void closeSystemService();
	public:
		virtual void msgOut(cv::String p_msg) = 0;
		virtual void error(cv::String p_msg) = 0;
		virtual void sysAssert(bool p_condition) = 0;
		virtual void sysAssert(bool p_condition, cv::String p_msg) = 0;

		//virtual void load() = 0;
		//virtual void save() = 0;

		//virtual void showSystemInfo() = 0;
		virtual void startTimer() = 0;
		virtual void ouputPasstime(int p_time = 0) = 0;
		virtual void ouputSumtime() = 0;

		virtual bool imgShowState() = 0;
		virtual void setImgShow(bool p_bShow = true)= 0;

	protected:
		virtual ~ISystemService(){};
	};

	ISystemService* sys();
};

#endif  // BSCV_SYSTEM_SYSYEMSERVICE_H