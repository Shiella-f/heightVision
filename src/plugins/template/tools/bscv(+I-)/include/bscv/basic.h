#ifndef BSCV_SYSTEM_BASIC_H
#define BSCV_SYSTEM_BASIC_H
/*
    基础定义，包括编译类型、一些架构方面的基础类等
*/
#include <iostream>

#define BS_MsgOut(msg) TIGER_BSVISION::sys()->msgOut(msg)
#define BS_Error(msg) TIGER_BSVISION::sys()->error(msg)

#pragma region TimeOutMethods

#include <chrono>

extern std::chrono::steady_clock::time_point g_begin;
extern int g_timeout;

// 设置超时时间单位ms
#define OpenTimeout

#ifdef OpenTimeout
    #define BS_SetTimeOut(time) g_timeout = time;g_begin = std::chrono::steady_clock::now()
    #define BS_bTimeOut() std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()- g_begin).count() > g_timeout
    #define BS_TimeOut(msg) if(BS_bTimeOut()){ BS_Error(msg); }
#else
    #define BS_SetTimeOut(time)
    #define BS_bTimeOut() false
    #define BS_TimeOut(msg)
#endif

#pragma endregion TimeOutMethods



#ifdef BSDEBUG
	#define BS_StartTimer() TIGER_BSVISION::sys()->startTimer()
	#define BS_OutputPasstime(id) TIGER_BSVISION::sys()->ouputPasstime(id)
	#define BS_OuputSumtime() TIGER_BSVISION::sys()->ouputSumtime()

	#define BSImgShow(name, image) if(TIGER_BSVISION::sys()->imgShowState()) { cv::imshow(name, image); }
    #define BS_Trace(msg) std::cout << "TRACE: " << msg

    #define BS_Assert_1(condition) TIGER_BSVISION::sys()->sysAssert(condition)
	#define BS_Assert_2(condition, msg) TIGER_BSVISION::sys()->sysAssert(condition, msg)
    #define BS_Assert_GET_MACRO(_1,_2,NAME,...) NAME
    #define BS_Assert(...) BS_Assert_GET_MACRO(__VA_ARGS__, BS_Assert_2, BS_Assert_1)(__VA_ARGS__)
#else
	#define BS_StartTimer()
	#define BS_OutputPasstime(id)
	#define BS_OuputSumtime()

	#define BSImgShow(name, image)
    #define BS_Trace(img)

    #define BS_Assert_1(condition)
	#define BS_Assert_2(condition, msg)
    #define BS_Assert_GET_MACRO(_1,_2,NAME,...) NAME
    #define BS_Assert(...) BS_Assert_GET_MACRO(__VA_ARGS__, BS_Assert_2, BS_Assert_1)(__VA_ARGS__)
#endif

#pragma region  "定义 SINGLETON mode"
#define SINGLETON(className) \
    friend class CGarbo##className; \
    public:\
        static className* className::instance()\
        {\
            if (_instance == NULL)\
            {\
                _instance = new className;\
            }\
            return _instance;\
        }; \
    protected: \
        className(); \
        ~className(); \
    private: \
        static className* _instance;

#define SINGLETON_GARBO(className) \
    className* className::_instance = nullptr; \
    class CGarbo##className\
    {\
    public:\
                ~CGarbo##className()\
                {\
                    if (className::_instance != nullptr)\
                    {\
                        delete className::_instance;\
                        className::_instance = nullptr;\
                    }\
                };\
    };\
    static CGarbo##className _garbo;
#pragma endregion

#endif  // BSCV_SYSTEM_BASIC_H