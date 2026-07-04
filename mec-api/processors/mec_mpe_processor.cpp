
#include "mec_mpe_processor.h"

#include "mec_log.h"

namespace mec {

static constexpr unsigned TIMBRE_CC=74;

MPE_Processor::MPE_Processor(float pbr) : Midi_Processor(1, pbr) {
    for(unsigned i=0;i<MAX_VOICE;i++) {
        VoiceData& voice = voices_[i];
        voice.startNote_ = 0;
        voice.note_ = 0;
        voice.pitchbend_ = 0;
        voice.timbre_ = 0;
        voice.pressure_ = 0;
        voice.active_ = false;
    }

}

MPE_Processor::~MPE_Processor() {
    ;
}

void MPE_Processor::sendMPEConfig() {
    // Tell the synth this is an MPE lower zone. Without this many synths (incl.
    // Aalto) never route per-note channel pressure / CC74 to their expression
    // inputs, even though the messages are being sent.

    // baseChannel_ is the first member channel (index 1 == MIDI ch 2), so the
    // MPE master channel is baseChannel_ - 1 (index 0 == MIDI ch 1).
    unsigned masterCh = baseChannel_ - 1;

    // MPE Configuration Message: CC127 on the master channel = number of member
    // channels in the zone. This is the CC127 convention Madrona's instruments use.
    cc(masterCh, 127, MAX_VOICE - 1);

    // Pitch-bend sensitivity via RPN 0, on a member channel (applies to the zone).
    unsigned range = static_cast<unsigned>(pitchbendRange_);
    cc(baseChannel_, 100, 0);       // RPN LSB
    cc(baseChannel_, 101, 0);       // RPN MSB (RPN 0 = pitch bend sensitivity)
    cc(baseChannel_, 6, range);     // data entry MSB = range in semitones
    cc(baseChannel_, 38, 0);        // data entry LSB
}

/////////////////////////
// ICallback interface
void MPE_Processor::touchOn(int id, float note, float x, float y, float z) {

    VoiceData& voice = voices_[id];

    unsigned ch = id + baseChannel_; // MPE starts on 2

    unsigned startNote = static_cast<unsigned>(note + 0.4999999 );
    float semis = note - float(startNote);
    unsigned pb = bipolar14bit(semis / pitchbendRange_);

    unsigned mx = bipolar14bit(x);
    unsigned my = bipolar7bit(y);
    unsigned mz = unipolar7bit(z);

    if(voice.active_) {
        LOG_1("WARN: duplicated note, starting new note on active channel " << ch << " new note: " << startNote  << " existing note " << voice.startNote_);
        noteOff(ch, voice.startNote_ , 0);
    }

    // LOG_1("MPE_Processor::touchOn");
    // LOG_1("   note : " << note  << " startNote " << startNote << " semi :" << semis << " pb: " << pb);
    // LOG_1("   x :" << x << " mx: " << mx);
    // LOG_1("   y :" << y << " my: " << my);
    // LOG_1("   z :" << z << " mz: " << mz);

    // Soundplane touches start at ~zero pressure, so mz (initial z) is often 0.
    // A Note On with velocity 0 is a Note Off per the MIDI spec, silencing the note,
    // so floor it: dynamics still come through via Channel Pressure below.
    unsigned vel = mz > 0 ? mz : 63;

    pitchbend(ch, pb);
    cc(ch, TIMBRE_CC,  my);
    noteOn(ch, startNote, vel);
    // LOG_1("note on : " << startNote);

    voice.active_ = true;
    voice.startNote_ = startNote;
    voice.note_ = startNote;
    voice.pitchbend_ = pb;
    voice.timbre_ = my;

    // start with zero z, as we use intial z of velocity
    pressure(ch, 0);
    voice.pressure_ = 0;
}

void MPE_Processor::touchContinue(int id, float note, float x, float y, float z) {

    VoiceData& voice = voices_[id];
    unsigned ch = id + baseChannel_; // MPE starts on 2
    // unsigned mx = bipolar14bit(x);
    unsigned my = bipolar7bit(y);
    unsigned mz = unipolar7bit(z);

    float semis = note - float(voice.startNote_);
    unsigned pb = bipolar14bit(semis / pitchbendRange_);

    // LOG_1(std::cout  << "midi output c")
    // LOG_1(           << " note :" << note << " pb: " << pb << " semis: " << semis)
    // LOG_1(           << " y :" << y << " my: " << my)
    // LOG_1(           << " z :" << z << " mz: " << mz)
    // LOG_1(           << " startnote :" << voice.startNote_ << " pbr: " << pitchbendRange_)
    // LOG_1(           )

    voice.note_ = note;

    if (voice.pitchbend_ != pb) {
        voice.pitchbend_ = pb;
        pitchbend(ch, pb) ;
    }
    if (voice.timbre_ != my) {
        voice.timbre_ = my;
        cc(ch, TIMBRE_CC, my);
    }
    if (voice.pressure_ != mz) {
        voice.pressure_ = mz;
        pressure(ch, mz);
    }
}

void MPE_Processor::touchOff(int id, float note, float x, float y, float z) {

    VoiceData& voice = voices_[id];

    unsigned ch = id + baseChannel_; // MPE starts on 2

    if(!voice.active_) {
        LOG_1("WARN: touchOff for inactive note , channel: " << ch << " note: " << voice.startNote_ << " n:" << note << " x:" << x << " y:" << y << " z: "<< z);
        return;
    }

    unsigned vel = 0; // last vel = release velocity
    pressure(ch, 0);
    noteOff(ch, voice.startNote_ , vel);
    // LOG_1("note off : " << voice.startNote_);

    voice.active_ = false;
    // voice.startNote_ = 0;
    // voice.note_ = 0;
    // voice.pitchbend_ = 0;
    // voice.timbre_ = 0;
    // voice.pressure_ = 0;//
}

void MPE_Processor::control(int attr, float v) {

    if (global_[attr] != v ) {
        global_[attr] = v;
        cc(0, attr, unipolar7bit(v));
        // cc(ch, attr, isBipolar ? bipolar7bit(v) : unipolar7bit(v));
    }
}

void MPE_Processor::mec_control(int cmd, void* other) {
    // ignored
    ;
}


}
