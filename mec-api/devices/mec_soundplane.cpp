#include "mec_soundplane.h"


#include "mec_log.h"
#include "../mec_voice.h"
#include "mec_velocity.h"
#include <set>

namespace mec {

////////////////////////////////////////////////
// TODO
// 1. voices not needed? as soundplane already does touch alloction, just need to detemine on and off
////////////////////////////////////////////////
class SoundplaneHandler : public ::SPLiteCallback {
public:
    // these are used to adjust velocity, they are highly dependent on hardware, to get the right feel, experiment!
    static constexpr float V_COUNT = 4; // samples to use for velocity , lets try 2-N,  (was 4)
    static constexpr float V_SCALE_AMT = 4.0f; // scale, to help V_COUNT pressures, quickly = max vel.  (was 4.0)
    static constexpr float V_CURVE_AMT = 4.0f; // a pow scaling, 1.0 = linear, < 1.0 = more sensitive,  > 1.0 = less sensitive (more firm pressure)  (was 4.0)
    static constexpr float DZ_SCALE_AMT = 50.0f; // gain for dz velocity mode; tracker dz is ~0.005-0.05 for typical strikes

    SoundplaneHandler(Preferences &p,
		    ICallback& cb)
            : prefs_(p),
              callback_(cb),
              valid_(true),
              voices_(
                static_cast<unsigned>(p.getInt("voices", 15)),
                static_cast<unsigned>(p.getInt("vel_count", V_COUNT)),
                static_cast<float>(p.getDouble("vel_curve", V_CURVE_AMT)),
                static_cast<float>(p.getDouble("vel_scale", V_SCALE_AMT))
              ),
              stealVoices_(p.getBool("steal voices", true)),
              velFromDz_(p.getString("velocity mode", "dz") != "regression"),
              pressureScale_(static_cast<float>(p.getDouble("pressure scale", 1.0))),
              dzScale_(static_cast<float>(p.getDouble("dz scale", DZ_SCALE_AMT))),
              velCurve_(static_cast<float>(p.getDouble("vel_curve", V_CURVE_AMT))) {
        if (valid_) {
            LOG_0("SoundplaneHandler enabling for mecapi, velocity mode : " << (velFromDz_ ? "dz" : "regression"));
        }
    }


    void onInit() override  {
        LOG_0("Soundplane initialised");
    }
    void onFrame() override {;}
    void onDeinit()override {;}
    void onError(unsigned err, const char *errStr) override {;}

    void touchOn(unsigned voice, float x, float y, float z) override {
        touchOn(voice, x, y, z, 0.0f);
    }
    void touchContinue(unsigned voice, float x, float y, float z) override {
        touchContinue(voice, x, y, z, 0.0f);
    }
    void touchOff(unsigned voice, float x, float y, float z) override {
        touchOff(voice, x, y, z, 0.0f);
    }

    void touchOn(unsigned voice, float x, float y, float z, float dz) override {
        unsigned ix = unsigned(x);
        unsigned iy = unsigned(y);
        float fn = (ix + (iy * 4)) + (x -ix - 0.5f);
        float fx = (x-float(ix)-0.5f) * 2.0f;
        float fy = (y-float(iy)-0.5f) * 2.0f;
        float fz = z;
        //fprintf(stderr,"on %d %f %f %f, %f - %f %f %f \n", voice, x, y, z, fn, fx,fy,fz);

        touch(true, voice, fn, fx,fy,fz, dz);
    }
    void touchContinue(unsigned voice, float x,float y, float z, float dz) override {
        unsigned ix = unsigned(x);
        unsigned iy = unsigned(y);
        float fn = (ix + (iy * 4)) + (x -ix + 0.5f);
        float fx = (x-float(ix)-0.5f) * 2.0f;
        float fy = (y-float(iy)-0.5f) * 2.0f;
        float fz = z;
        //fprintf(stderr,"cont %d %f %f %f, %f - %f %f %f \n", voice, x, y, z, fn, fx,fy,fz);

        touch(true, voice, fn, fx,fy,fz, dz);
    }

    void touchOff(unsigned voice, float x,float y, float z, float dz) override {
        unsigned ix = unsigned(x);
        unsigned iy = unsigned(y);
        float fn = (ix + (iy * 4)) + (x -ix + 0.5f);
        float fx = (x-float(ix)-0.5f) * 2.0f;
        float fy = (y-float(iy)-0.5f) * 2.0f;
        float fz = 0.0f; //z;
        //fprintf(stderr,"off %d %f %f %f, %f - %f %f %f \n", voice, x, y, z, fn, fx,fy,fz);
        touch(false, voice, fn, fx,fy,fz, dz);
    }

    bool isValid() { return valid_; }
//    virtual void device(const char *dev, int rows, int cols) {
//        LOG_1("SoundplaneHandler  device d: " << dev);
//        LOG_1(" r: " << rows << " c: " << cols);
//    }

    void touch(bool a, int itouch, float n, float x, float y, float z, float dz) {
        static const unsigned int NOTE_CH_OFFSET = 1;

        unsigned touch = (unsigned) itouch;
        Voices::Voice *voice = voices_.voiceId(touch);
        float fn = n;
        float mn = note(fn);
        float mx = clamp(x, -1.0f, 1.0f);
        float my = clamp(y, -1.0f, 1.0f);
        // the touch tracker outputs z in 0..8; scale before clamping so firm
        // presses don't saturate (pressure scale pref, e.g. 0.5 for headroom)
        float mz = clamp(z * pressureScale_,  0.0f, 1.0f);
        unsigned long long t = 0;

        if (a) {
            // LOG_1("SoundplaneHandler  touch device d: "   << dev      << " a: "   << a)
            // LOG_1(" touch: " <<  touch);
            // LOG_1(" note: " <<  n  << " mn: "   << mn << " fn: " << fn);
            // LOG_1(" x: " << x      << " y: "   << y    << " z: "   << z);
            // LOG_1(" mx: " << mx    << " my: "  << my   << " mz: "  << mz);
            if (!voice) {
                if (stolenTouches_.find(touch) != stolenTouches_.end()) {
                    // this key has been stolen, must be released to reactivate it
                    return;
                }

                voice = voices_.startVoice(touch);
                //LOG_1("start voice for " << touch << " ch " << voice->i_);

                if (!voice && stealVoices_) {
                    // no available voices, steal?
                    Voices::Voice *stolen = voices_.oldestActiveVoice();
                    // only send touchOff if the stolen voice actually sounded (touchOn was sent)
                    if (stolen->state_ == Voices::Voice::ACTIVE) {
                        callback_.touchOff(stolen->i_, stolen->note_, stolen->x_, stolen->y_, 0.0f);
                    }
                    voices_.stopVoice(stolen);

                    voice = voices_.startVoice(touch);
                }
            }

            if (voice) {
                if (voice->state_ == Voices::Voice::PENDING) {
                    if (velFromDz_) {
                        // the touch tracker's dz already measures the onset
                        // pressure rise (including pre-threshold), so the note
                        // can start immediately with no detection latency.
                        voice->v_ = velocityFromDz(dz, dzScale_, velCurve_);
                        voice->state_ = Voices::Voice::ACTIVE;
                        callback_.touchOn(voice->i_, mn, mx, my, voice->v_);
                    } else {
                        // feed the velocity detector; defer touchOn until it has
                        // enough pressure frames to measure the onset velocity.
                        voices_.addPressure(voice, mz);
                        if (voice->state_ == Voices::Voice::ACTIVE) {
                            // LOG_1("calculated velocity" << touch << " ch " << voice->i_ << " vel " << voice->v_);
                            callback_.touchOn(voice->i_, mn, mx, my, voice->v_); //v_ = calculated velocity
                        }
                        // don't send to callbacks until we have the minimum pressures for velocity
                    }
                } else {
                    callback_.touchContinue(voice->i_, mn, mx, my, mz);
                }
                voice->note_ = mn;
                voice->x_ = mx;
                voice->y_ = my;
                voice->z_ = mz;
                voice->t_ = t;
            }

        } else {
            if (voice) {
                //LOG_1("stop voice for " << touch << " ch " << voice->i_ );
                // only send touchOff if touchOn was sent (voice reached ACTIVE);
                // a PENDING voice never sounded, so just release it.
                if (voice->state_ == Voices::Voice::ACTIVE) {
                    callback_.touchOff(voice->i_, mn, mx, my, mz);
                }
                voices_.stopVoice(voice);
            }
            stolenTouches_.erase(touch);
        }
    }
private:
    inline float clamp(float v, float mn, float mx) { return (std::max(std::min(v, mx), mn)); }

    float note(float n) { return n; }

    Preferences prefs_;
    ICallback &callback_;
    Voices voices_;
    bool valid_;
    bool stealVoices_;
    bool velFromDz_;
    float pressureScale_;
    float dzScale_;
    float velCurve_;
    std::set<unsigned> stolenTouches_;
};


////////////////////////////////////////////////
Soundplane::Soundplane(ICallback &cb) :
        active_(false), callback_(cb) {
}

Soundplane::~Soundplane() {
    deinit();
}

bool Soundplane::init(void *arg) {
    Preferences prefs(arg);
    //prefs.print();
    unsigned maxtouch = static_cast<unsigned>(prefs.getInt("voices", 15));
    LOG_1("max voices : " << maxtouch);

    if (active_) {
        deinit();
    }
    active_ = false;

    device_ = std::unique_ptr<SPLiteDevice>(new SPLiteDevice());


    std::shared_ptr<::SPLiteCallback> callback
        = std::shared_ptr<::SPLiteCallback>(new SoundplaneHandler(prefs, callback_));
    device_->addCallback(callback);

    // touch tracker tuning, applied when the device starts
    device_->touchThreshold(static_cast<float>(prefs.getDouble("touch threshold", 0.05)));
    device_->lopassZ(static_cast<float>(prefs.getDouble("lopass z", 100.0)));

    device_->start();
    device_->maxTouches(maxtouch);
    active_ = true;

    return active_;
}

bool Soundplane::process() {
    return device_->process();
}

void Soundplane::deinit() {
    LOG_0("Soundplane::deinit");
    if (!device_) return;
    LOG_0("Soundplane::reset model");
    device_->stop();
    device_.reset();
    active_ = false;
}

bool Soundplane::isActive() {
    return active_;
}


}

