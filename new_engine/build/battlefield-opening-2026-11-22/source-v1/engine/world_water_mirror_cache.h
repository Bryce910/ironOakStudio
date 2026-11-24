#ifndef VALWOOD_WORLD_WATER_MIRROR_CACHE_H
#define VALWOOD_WORLD_WATER_MIRROR_CACHE_H

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

// Metadata and projection belong to the same successfully captured mirror
// image. In particular, the main camera VP must never replace this projection
// on a frame that samples the previous image.
class WorldWaterMirrorCache {
public:
    struct Pose {
        double x=0.,y=0.,z=0.,yaw=0.,pitch=0.,fov=0.,aspect=0.,level=0.;
        int width=0,height=0;
    };
    using Projection=std::array<float,16>;

    bool valid() const { return valid_; }
    void invalidate() { valid_=false; }

    // Default callers retain the original alternate-frame capture policy.
    // An explicit period may hold a stationary image for at most three reuse
    // frames (capture every fourth frame). The captured projection/plane stay
    // paired with it. Motion still needs a new image: rebasing alone cannot
    // fill newly exposed/scissored reflection pixels.
    bool can_reuse(std::uint64_t frame,const Pose &pose,
            bool forceEveryFrame=false,bool temporalCut=false,
            unsigned capturePeriod=2) const {
        if(!valid_||forceEveryFrame||temporalCut||!valid_pose(pose)||
                capturePeriod<1||capturePeriod>4||
                frame_==std::numeric_limits<std::uint64_t>::max()||
                frame<=frame_||frame-frame_>=capturePeriod)
            return false;
        if(pose.width!=pose_.width||pose.height!=pose_.height||
                pose.fov!=pose_.fov||pose.aspect!=pose_.aspect||pose.level!=pose_.level)
            return false;
        if(std::hypot(pose.x-pose_.x,pose.y-pose_.y,pose.z-pose_.z)>1.e-5)
            return false;
        constexpr double tau=6.283185307179586476925286766559;
        const double yaw=std::remainder(pose.yaw-pose_.yaw,tau);
        const double pitch=std::remainder(pose.pitch-pose_.pitch,tau);
        return std::isfinite(yaw)&&std::isfinite(pitch)&&
            std::abs(yaw)<=1.e-6&&std::abs(pitch)<=1.e-6;
    }

    // Call only after drawing the mirror successfully. Invalid capture data
    // cannot certify an image (which may already have been partly replaced).
    bool commit(std::uint64_t frame,const Pose &pose,const Projection &mirroredVP) {
        if(!valid_pose(pose)){invalidate();return false;}
        for(float value:mirroredVP)
            if(!std::isfinite(value)){invalidate();return false;}
        pose_=pose;frame_=frame;projection_=mirroredVP;valid_=true;
        return true;
    }

    const Projection &projection() const { return projection_; }

    // The renderer expresses reflected geometry about a mirrored camera
    // origin. Y therefore has the opposite origin shift from X/Z. Only use
    // with a pose for which can_reuse is true (same reflection plane).
    std::array<double,3> origin_delta(const Pose &pose) const {
        return {pose.x-pose_.x,pose_.y-pose.y,pose.z-pose_.z};
    }
    Projection rebased_projection(const Pose &pose) const {
        Projection result=projection_;
        const auto delta=origin_delta(pose);
        for(unsigned row=0;row<4;++row)
            result[12+row]=float(double(projection_[12+row])+
                double(projection_[row])*delta[0]+
                double(projection_[4+row])*delta[1]+
                double(projection_[8+row])*delta[2]);
        return result;
    }

private:
    static bool valid_pose(const Pose &pose) {
        constexpr double pi=3.1415926535897932384626433832795;
        return std::isfinite(pose.x)&&std::isfinite(pose.y)&&std::isfinite(pose.z)&&
            std::isfinite(pose.yaw)&&std::isfinite(pose.pitch)&&
            std::isfinite(pose.fov)&&pose.fov>0.&&pose.fov<pi&&
            std::isfinite(pose.aspect)&&pose.aspect>0.&&
            std::isfinite(pose.level)&&pose.width>0&&pose.height>0;
    }
    bool valid_=false;
    std::uint64_t frame_=0;
    Pose pose_{};
    Projection projection_{};
};

#endif
