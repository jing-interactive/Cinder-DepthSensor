#include "DepthSensor.h"

#ifdef Kinect1_Enabled
#include "v1/src/KinectCommonBridgeLib.h"

#include "cinder/Log.h"
#include "cinder/app/app.h"
#include "cinder/msw/CinderMsw.h"

using namespace ci;
using namespace ci::app;
using namespace std;

namespace ds
{
#define SCOPED_COM_OBJECT(objName) auto scoped##__LINE__## = msw::makeComUnique(objName);

    static std::pair<int, int> mappingPairs[] = {
        {Body::HIP_CENTER, NUI_SKELETON_POSITION_HIP_CENTER},
        {Body::SPINE, NUI_SKELETON_POSITION_SPINE},
        {Body::SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_CENTER},
        {Body::HEAD, NUI_SKELETON_POSITION_HEAD},
        {Body::SHOULDER_LEFT, NUI_SKELETON_POSITION_SHOULDER_LEFT},
        {Body::ELBOW_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT},
        {Body::WRIST_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT},
        {Body::HAND_LEFT, NUI_SKELETON_POSITION_HAND_LEFT},
        {Body::SHOULDER_RIGHT, NUI_SKELETON_POSITION_SHOULDER_RIGHT},
        {Body::ELBOW_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT},
        {Body::WRIST_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT},
        {Body::HAND_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT},
        {Body::HIP_LEFT, NUI_SKELETON_POSITION_HIP_LEFT},
        {Body::KNEE_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT},
        {Body::ANKLE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT},
        {Body::FOOT_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT},
        {Body::HIP_RIGHT, NUI_SKELETON_POSITION_HIP_RIGHT},
        {Body::KNEE_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT},
        {Body::ANKLE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT},
        {Body::FOOT_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT},
    };

    struct DeviceKinect1 : public Device
    {
        unique_ptr<uint8_t[]> depthBuffer;
        KINECT_IMAGE_FRAME_FORMAT depthDesc;

        unique_ptr<uint8_t[]> colorBuffer;
        KINECT_IMAGE_FRAME_FORMAT colorDesc;

        NUI_SKELETON_FRAME skeletonFrame;
        int sensor = KCB_INVALID_HANDLE;

        vector<NUI_COLOR_IMAGE_POINT> depthToColorArray;

        static uint32_t getDeviceCount()
        {
            int count = 0;
            NuiGetSensorCount(&count);

            return count;
        }

        ~DeviceKinect1()
        {
            if (sensor != KCB_INVALID_HANDLE)
            {
                KinectCloseSensor(sensor);
            }
        }

        ivec2 getDepthSize() const { return {depthDesc.dwWidth, depthDesc.dwHeight}; }

        ivec2 getColorSize() const { return {colorDesc.dwWidth, colorDesc.dwHeight}; }

        bool isValid() const { return sensor != KCB_INVALID_HANDLE; }

        DeviceKinect1(Option option)
        {
            this->option = option;

            if (getDeviceCount() == 0)
            {
                CI_LOG_E("There is no Kinect V1 devices.");
                return;
            }

            HRESULT hr = S_OK;

            WCHAR portName[100];
            if (KinectGetPortIDByIndex(option.deviceId, 100, portName))
            {
                sensor = KinectOpenSensor(portName);
            }
            else
            {
                sensor = KinectOpenDefaultSensor();
            }

            if (KCB_INVALID_HANDLE == sensor)
            {
                hr = E_UNEXPECTED;
            }

            if (FAILED(hr))
            {
                CI_LOG_E("Failed to connect to Kinect V1");
                return;
            }

            if (option.enableDepth)
            {
                depthDesc = {sizeof(KINECT_IMAGE_FRAME_FORMAT)};
                KinectGetDepthFrameFormat(sensor, &depthDesc);
                depthBuffer.reset(new uint8_t[depthDesc.cbBufferSize]);
                depthChannel = Channel16u(depthDesc.dwWidth, depthDesc.dwHeight,
                                          depthDesc.cbBytesPerPixel * depthDesc.dwWidth, 1,
                                          (uint16_t*)depthBuffer.get());
            }
            else
            {
                KinectStopDepthStream(sensor);
            }

            if (option.enableColor)
            {
                colorDesc = {sizeof(KINECT_IMAGE_FRAME_FORMAT)};
                KinectGetColorFrameFormat(sensor, &colorDesc);
                colorBuffer.reset(new uint8_t[colorDesc.cbBufferSize]);
                colorSurface = Surface8u(colorBuffer.get(), colorDesc.dwWidth, colorDesc.dwHeight,
                                         colorDesc.cbBytesPerPixel * colorDesc.dwWidth,
                                         SurfaceChannelOrder::BGRX);
            }
            else
            {
                KinectStopColorStream(sensor);
            }

            if (option.enablePointCloud && option.enableColor)
            {
                depthToColorArray.resize(depthDesc.dwWidth * depthDesc.dwHeight);
                depthToColorTable = Surface32f(depthDesc.dwWidth, depthDesc.dwHeight, false,
                                               SurfaceChannelOrder::RGB);
            }

            if (option.enableBody && option.enableBodyIndex)
            {
                KinectStartSkeletonStream(sensor);
            }

            App::get()->getSignalUpdate().connect(std::bind(&DeviceKinect1::update, this));
        }

        const vec3 toCi(const Vector4& pos) { return vec3(pos.x, pos.y, pos.z); }

        void update()
        {
            if (option.enableDepth && KinectIsDepthFrameReady(sensor))
            {
                if (SUCCEEDED(KinectGetDepthFrame(sensor, depthDesc.cbBufferSize, depthBuffer.get(),
                                                  nullptr)))
                {
                    auto depthPointCount = depthDesc.dwWidth * depthDesc.dwHeight;
                    //
                    // signalDepthToColorTable
                    //
                    if (option.enablePointCloud && option.enableColor)
                    {
                        vector<NUI_DEPTH_IMAGE_POINT> depthPoints(depthPointCount);
                        {
                            auto* dst = depthPoints.data();
                            auto* src = (uint16_t*)depthBuffer.get();
                            for (int y = 0; y < depthDesc.dwHeight; y++)
                                for (int x = 0; x < depthDesc.dwWidth; x++)
                                {
                                    dst->x = x;
                                    dst->y = y;
                                    // dst->depth = NuiDepthPixelToDepth(*src);
                                    dst->depth = *src;
                                    dst++;
                                    src++;
                                }
                        }

#if 1
                        static auto nuiSensor = KinectGetNuiSensor(sensor);

                        HRESULT hr =
                            nuiSensor
                                ->NuiImageGetColorPixelCoordinateFrameFromDepthPixelFrameAtResolution(
                                    NUI_IMAGE_RESOLUTION_640x480, NUI_IMAGE_RESOLUTION_640x480,
                                    depthPointCount, (uint16_t*)depthBuffer.get(),
                                    depthPointCount * 2, (LONG*)depthToColorArray.data());
#else
                        HRESULT hr = KinectMapDepthPointToColorPoint(
                            sensor, NUI_IMAGE_RESOLUTION_640x480, depthPoints.data(),
                            NUI_IMAGE_TYPE_COLOR_INFRARED, NUI_IMAGE_RESOLUTION_640x480,
                            depthToColorArray.data());
#endif
                        if (SUCCEEDED(hr))
                        {
                            auto* src = depthToColorArray.data();
                            auto* dst = (vec3*)depthToColorTable.getData();
                            for (int i = 0; i < depthPointCount; i++)
                            {
                                dst[i].x = src[i].x / (float)colorDesc.dwWidth;
                                dst[i].y = src[i].y / (float)colorDesc.dwHeight;
                            }
                            signalDepthToColorTableDirty.emit();
                        }
                    }

                    //
                    // signalDepthDirty
                    //
                    uint16_t* src = (uint16_t*)depthBuffer.get();
                    for (int i = 0; i < depthPointCount; i++)
                    {
                        *src = NuiDepthPixelToDepth(*src);
                        src++;
                    }

                    signalDepthDirty.emit();
                }

                // signalDepthToCameraTableDirty
                if (depthToCameraTable.getWidth() == 0)
                {
                    depthToCameraTable = Surface32f(depthDesc.dwWidth, depthDesc.dwHeight, false,
                                                    SurfaceChannelOrder::RGB);
                    //
                    // Center of depth sensor is at (0,0,0) in skeleton space, and
                    // and (width/2,height/2) in depth image coordinates.  Note that positive Y
                    // is up in skeleton space and down in image coordinates.
                    //
                    for (int y = 0; y < depthDesc.dwHeight; y++)
                        for (int x = 0; x < depthDesc.dwWidth; x++)
                        {
                            float fSkeletonX =
                                (x - depthDesc.dwWidth / 2.0f) * (320.0f / depthDesc.dwWidth) *
                                NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240;
                            float fSkeletonY =
                                -(y - depthDesc.dwHeight / 2.0f) * (240.0f / depthDesc.dwHeight) *
                                NUI_CAMERA_DEPTH_IMAGE_TO_SKELETON_MULTIPLIER_320x240;
                            vec3* dst = (vec3*)depthToCameraTable.getData({x, y});
                            dst->x = fSkeletonX;
                            dst->y = fSkeletonY;
                        }
                    signalDepthToCameraTableDirty.emit();
                }
            }

            if (option.enableColor && KinectIsColorFrameReady(sensor))
            {
                if (SUCCEEDED(KinectGetColorFrame(sensor, colorDesc.cbBufferSize, colorBuffer.get(),
                                                  nullptr)))
                {
                    signalColorDirty.emit();
                }
            }

            if (option.enableBody && KinectIsSkeletonFrameReady(sensor))
            {
                if (SUCCEEDED(KinectGetSkeletonFrame(sensor, &skeletonFrame)))
                {
                    for (auto& data : skeletonFrame.SkeletonData)
                    {
                        if (data.eTrackingState != NUI_SKELETON_TRACKED)
                            continue;
                        Body body;
                        body.id = data.dwTrackingID;

                        for (auto& mapping : mappingPairs)
                        {
                            vec3 pos3d = toCi(data.SkeletonPositions[mapping.second]);
                            vec2 pos2d;

                            if (pos3d.z > FLT_EPSILON)
                            {
                                // Center of depth sensor is at (0,0,0) in skeleton space, and
                                // and (width/2,height/2) in depth image coordinates.  Note that
                                // positive Y is up in skeleton space and down in image coordinates.
                                pos2d.x =
                                    0.5f +
                                    pos3d.x * (depthDesc.dwWidth / 320.f) *
                                        NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 /
                                        pos3d.z;
                                pos2d.y =
                                    0.5f -
                                    pos3d.y * (depthDesc.dwHeight / 240.f) *
                                        NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 /
                                        pos3d.z;
                            }
                            body.joints[mapping.first].pos3d = pos3d;
                            body.joints[mapping.first].pos2d = pos2d;
                        }
                        bodies.push_back(body);
                    }
                    signalBodyDirty.emit();
                }
            }
        }
    };

    uint32_t getKinect1Count() { return DeviceKinect1::getDeviceCount(); }

    DeviceRef createKinect1(Option option) { return DeviceRef(new DeviceKinect1(option)); }
} // namespace ds

#endif
