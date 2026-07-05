#ifndef MEC_VOICES_H_
#define MEC_VOICES_H_

#include <math.h>

#include <list>
#include <vector>

#include "mec_log.h"

namespace mec {

class Voices {
public:
    static constexpr float NUM_VOICES = 15;
    static constexpr float V_SCALE_AMT = 4.0f;
    static constexpr float V_CURVE_AMT = 4.0f;
    static constexpr float V_COUNT = 4;

    Voices(unsigned voiceCount = NUM_VOICES, unsigned velCount = V_COUNT, float velCurve = V_CURVE_AMT,
           float velScale = V_SCALE_AMT)
        : maxVoices_(voiceCount), velCount_(velCount), velCurve_(velCurve), velScale_(velScale) {
        voices_.resize(maxVoices_);
        for (int i = 0; i < maxVoices_; i++) {
            voices_[i].i_ = i;
            voices_[i].state_ = Voice::INACTIVE;
            voices_[i].id_ = -1;
            freeVoices_.push_back(&voices_[i]);
        }
    };

    virtual ~Voices() {};


    struct Voice {
        int i_;
        int id_;
        float note_;
        float x_;
        float y_;
        float z_;
        float v_;
        unsigned long long t_;
        enum {
            INACTIVE,
            PENDING,  // velocity
            ACTIVE,
        } state_;

        // velocity, taken from velocity detector
        struct {
            unsigned vcount_;
            float sumx_, sumy_, sumxy_, sumxsq_, x_;
            float scale_, curve_;  // comes from config
            float raw_;
        } vel_;
    };

    Voice* voiceId(unsigned id) {
        for (int i = 0; i < maxVoices_; i++) {
            if (voices_[i].id_ == id) return &voices_[i];
        }
        return NULL;
    }

    Voice* startVoice(unsigned id) {
        Voice* voice;
        if (freeVoices_.size() > 0) {
            voice = freeVoices_.front();
            freeVoices_.pop_front();
        } else {
            // all voices used, use oldestActiveVoice
            // if you wish to steal it
            return NULL;
        }

        voice->id_ = id;
        voice->state_ = Voice::PENDING;
        voice->v_ = 0;

        voice->vel_.scale_ = velScale_;
        voice->vel_.curve_ = velCurve_;
        voice->vel_.vcount_ = 0;
        voice->vel_.sumx_ = 0.0;
        voice->vel_.sumy_ = 0.0;
        voice->vel_.sumxy_ = 0.0;
        voice->vel_.sumxsq_ = 0.0;

        // First actual pressure sample will use x = 1
        voice->vel_.x_ = 1.0;

        usedVoices_.push_back(voice);
        return voice;
    }
    void addPressure(Voice* voice, float p) {
        if (voice->state_ != Voice::PENDING) { return; }

        if (voice->vel_.vcount_ < velCount_) {
            voice->vel_.sumx_ += voice->vel_.x_;
            voice->vel_.sumy_ += p;
            voice->vel_.sumxy_ += (voice->vel_.x_ * p);
            voice->vel_.sumxsq_ += (voice->vel_.x_ * voice->vel_.x_);

            voice->vel_.vcount_++;
            voice->vel_.x_++;

            // Compute velocity immediately when enough samples have been collected.
            if (voice->vel_.vcount_ < velCount_) { return; }
        }

        voice->state_ = Voice::ACTIVE;

        const double n = static_cast<double>(voice->vel_.vcount_);
        const double numerator = n * voice->vel_.sumxy_ - voice->vel_.sumx_ * voice->vel_.sumy_;
        const double denominator = n * voice->vel_.sumxsq_ - voice->vel_.sumx_ * voice->vel_.sumx_;

        if (fabs(denominator) < 1e-12) {
            voice->vel_.raw_ = 0.0;
        } else {
            voice->vel_.raw_ = voice->vel_.scale_ * numerator / denominator;
        }

        // Protect the curve calculation from invalid values.
        if (voice->vel_.raw_ < 0.0)
            voice->vel_.raw_ = 0.0;
        else if (voice->vel_.raw_ > 1.0)
            voice->vel_.raw_ = 1.0;

        voice->v_ = static_cast<float>(1.0 - pow(1.0 - voice->vel_.raw_, voice->vel_.curve_));
        if (voice->v_ > 1.0f) voice->v_ = 1.0f;
        if (voice->v_ < 0.01f) voice->v_ = 0.01f;
    }

    void stopVoice(Voice* voice) {
        if (!voice) return;
        usedVoices_.remove(voice);
        voice->id_ = -1;
        voice->note_ = 0;
        voice->x_ = 0;
        voice->y_ = 0;
        voice->z_ = 0;
        voice->t_ = 0;
        voice->state_ = Voice::INACTIVE;
        freeVoices_.push_back(voice);
    }

    Voice* oldestActiveVoice() { return usedVoices_.front(); }


private:
    std::vector<Voice> voices_;
    std::list<Voice*> freeVoices_;
    std::list<Voice*> usedVoices_;
    unsigned maxVoices_;
    unsigned velCount_;
    float velScale_;
    float velCurve_;
};
}  // namespace mec

#endif  // MEC_VOICES_H_
