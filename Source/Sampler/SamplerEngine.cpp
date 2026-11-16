#include "SamplerEngine.h"
#include <cmath>

namespace mpc
{
SamplerEngine::SamplerEngine()
{
    for (int i = 0; i < numPads; ++i)
        params[(size_t) i].midiNote = juce::jlimit (0, 127, 36 + i);
}

Sample::Ptr SamplerEngine::readSample (const juce::File& f) const
{
    if (! f.existsAsFile()) return nullptr;
    juce::AudioFormatManager fm;                     // local: thread-safe
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->lengthInSamples > 44100LL * 60 * 10) return nullptr;

    const int n = (int) r->lengthInSamples, ch = juce::jmin (2, (int) r->numChannels);
    Sample::Ptr s = new Sample();
    s->data.setSize (ch, n);
    if (! r->read (&s->data, 0, n, 0, true, ch > 1)) return nullptr;
    s->sampleRate = r->sampleRate;
    s->path = f.getFullPathName();

    for (size_t b = 0; b < s->peaks.size(); ++b)
    {
        const int a0 = (int) ((int64_t) n * (int64_t) b / 256), a1 = juce::jmax (a0 + 1, (int) ((int64_t) n * (int64_t) (b + 1) / 256));
        float m = 0;
        for (int c = 0; c < ch; ++c) m = juce::jmax (m, s->data.findMinMax (c, a0, juce::jmin (n, a1) - a0).getEnd(),
                                                       -s->data.findMinMax (c, a0, juce::jmin (n, a1) - a0).getStart());
        s->peaks[b] = m;
    }
    return s;
}

void SamplerEngine::assignSample (int pad, Sample::Ptr s)
{
    if (pad < 0 || pad >= numPads) return;
    const juce::SpinLock::ScopedLockType sl (lock);
    samples[(size_t) pad] = s;
}

Sample::Ptr SamplerEngine::getSample (int pad)
{
    if (pad < 0 || pad >= numPads) return nullptr;
    const juce::SpinLock::ScopedLockType sl (lock);
    return samples[(size_t) pad];
}

void SamplerEngine::setPreview (Sample::Ptr s)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    previewSample = s;
}

int SamplerEngine::padForNote (int note) const
{
    for (int i = 0; i < numPads; ++i)
        if (params[(size_t) i].midiNote == note) return i;
    return -1;
}

void SamplerEngine::post (Event e)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { events[(size_t) s1] = e; fifo.finishedWrite (1); }
}

void SamplerEngine::handle (const Event& e)
{
    if (e.type == 0) noteOn (e.pad, e.vel);
    else if (e.type == 1) noteOff (e.pad);
    else if (e.type == 3) { for (auto& v : voices) { v.active = false; v.sample = nullptr; } }
    else if (e.type == 2)
    {
        juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || previewSample == nullptr) return;
        PadParams p;
        startVoice (previewSample, -2, p, previewLevel);
    }
}

void SamplerEngine::noteOn (int pad, float vel)
{
    if (pad < 0 || pad >= numPads) return;
    const auto& p = params[(size_t) pad];
    bool anySolo = false;
    for (auto& q : params) anySolo = anySolo || q.solo;
    if (p.mute || (anySolo && ! p.solo)) return;

    juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked()) return;
    auto s = samples[(size_t) pad];
    if (s == nullptr) return;
    hitMask.fetch_or (1ull << pad);
    startVoice (s, pad, p, vel * p.level * masterVolume);
}

void SamplerEngine::startVoice (Sample::Ptr s, int pad, const PadParams& p, float gain)
{
    Voice* v = nullptr;
    for (auto& c : voices) if (! c.active) { v = &c; break; }
    if (v == nullptr) { v = &voices[0]; for (auto& c : voices) if (c.age < v->age) v = &c; }

    const int len = s->data.getNumSamples();
    const double a = juce::jlimit (0.0, 1.0, (double) p.start) * len;
    const double b = juce::jmax (a + 1.0, juce::jlimit (0.0, 1.0, (double) p.end) * len);
    v->sample = s; v->pad = pad; v->active = true; v->releasing = false;
    v->env = 1.0f; v->fade = 0.0f; v->reverse = p.reverse; v->oneShot = p.oneShot || pad < 0;
    v->age = ++ageCounter;
    v->inc = (s->sampleRate / hostRate) * std::pow (2.0, (double) p.tuneSemis / 12.0);
    v->pos = p.reverse ? b - 1.0 : a;
    v->endPos = p.reverse ? a : b;
    v->gain = gain;
    const float ang = (juce::jlimit (-1.0f, 1.0f, p.pan) * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    v->panL = std::cos (ang) * 1.41421356f; v->panR = std::sin (ang) * 1.41421356f;
}

void SamplerEngine::noteOff (int pad)
{
    for (auto& v : voices)
        if (v.active && v.pad == pad && ! v.oneShot) v.releasing = true;
}

void SamplerEngine::renderVoice (Voice& v, juce::AudioBuffer<float>& out, int start, int n)
{
    auto& d = v.sample->data;
    const int len = d.getNumSamples();
    const float* l = d.getReadPointer (0);
    const float* r = d.getNumChannels() > 1 ? d.getReadPointer (1) : l;
    float* oL = out.getWritePointer (0);
    float* oR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const float rel = 1.0f / (0.005f * (float) hostRate);

    for (int i = 0; i < n; ++i)
    {
        const bool done = v.reverse ? v.pos <= v.endPos : v.pos >= v.endPos;
        if (done || v.env <= 0.0f) { v.active = false; v.sample = nullptr; return; }
        const int i0 = juce::jlimit (0, len - 1, (int) v.pos);
        const int i1 = juce::jmin (len - 1, i0 + 1);
        const float f = (float) (v.pos - std::floor (v.pos));
        const float sl = l[i0] + (l[i1] - l[i0]) * f;
        const float sr = r[i0] + (r[i1] - r[i0]) * f;
        const float g = v.gain * v.env * v.fade;
        oL[start + i] += sl * g * v.panL * 0.7071f;
        if (oR != nullptr) oR[start + i] += sr * g * v.panR * 0.7071f;
        v.fade = juce::jmin (1.0f, v.fade + 1.0f / 64.0f);
        if (v.releasing) v.env -= rel;
        v.pos += v.reverse ? -v.inc : v.inc;
    }
}

void SamplerEngine::process (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    buf.clear();

    int s1, n1, s2, n2;
    fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) handle (events[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) handle (events[(size_t) (s2 + i)]);
    fifo.finishedRead (n1 + n2);

    int pos = 0;
    const int total = buf.getNumSamples();
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, total, meta.samplePosition);
        for (auto& v : voices) if (v.active) renderVoice (v, buf, pos, t - pos);
        pos = t;
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int lp = learnPad.load();
            if (lp >= 0 && lp < numPads) { params[(size_t) lp].midiNote = m.getNoteNumber(); learnPad.store (-1); }
            noteOn (padForNote (m.getNoteNumber()), m.getFloatVelocity());
        }
        else if (m.isNoteOff())  noteOff (padForNote (m.getNoteNumber()));
        else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) { v.active = false; v.sample = nullptr; }
    }
    for (auto& v : voices) if (v.active) renderVoice (v, buf, pos, total - pos);
}
}
